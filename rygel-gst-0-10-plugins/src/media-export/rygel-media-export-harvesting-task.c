/*
 * Copyright (C) 2009 Jens Georg <mail@jensge.org>.
 * Copyright (C) 2012, 2013 Intel Corporation.
 *
 * This file is part of Rygel.
 *
 * Rygel is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Rygel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <glib/gi18n-lib.h>
#include <libgupnp-dlna/gupnp-dlna.h>

#include "rygel-media-export-dummy-container.h"
#include "rygel-media-export-errors.h"
#include "rygel-media-export-harvester.h"
#include "rygel-media-export-harvesting-task.h"
#include "rygel-media-export-item-factory.h"
#include "rygel-media-export-media-cache.h"

static void
rygel_media_export_harvesting_task_rygel_state_machine_interface_init (RygelStateMachineIface *iface);

G_DEFINE_TYPE_WITH_CODE (RygelMediaExportHarvestingTask,
                         rygel_media_export_harvesting_task,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (RYGEL_TYPE_STATE_MACHINE,
                                                rygel_media_export_harvesting_task_rygel_state_machine_interface_init))

typedef struct _RygelMediaExportHarvestingTaskRunData RygelMediaExportHarvestingTaskRunData;
typedef struct _RygelMediaExportHarvestingTaskEnumerateDirectoryData RygelMediaExportHarvestingTaskEnumerateDirectoryData;
typedef struct _FileQueueEntry FileQueueEntry;

struct _RygelMediaExportHarvestingTaskPrivate {
  RygelMediaExportMetadataExtractor *extractor;
  RygelMediaExportMediaCache *cache;
  GQueue *containers;
  GeeQueue *files;
  RygelMediaExportRecursiveFileMonitor *monitor;
  gchar *flag;
  RygelMediaContainer *parent;
  GCancellable *cancellable;
  GFile *origin;
};

struct _RygelMediaExportHarvestingTaskRunData {
  GSimpleAsyncResult *simple;
  RygelMediaExportHarvestingTask *self;
};

struct _RygelMediaExportHarvestingTaskEnumerateDirectoryData {
  GSimpleAsyncResult *simple;
  RygelMediaExportHarvestingTask *self;
  GFile *directory;
  GFileEnumerator *enumerator;
  RygelMediaExportDummyContainer *container;
};

struct _FileQueueEntry {
  volatile gint ref_count;
  GFile *file;
  gboolean known;
};

FileQueueEntry *
file_queue_entry_new (GFile *file,
                      gboolean known) {
  FileQueueEntry *entry;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  entry = g_new (FileQueueEntry, 1);
  entry->ref_count = 1;
  entry->file = g_object_ref (file);
  entry->known = known;

  return entry;
}

FileQueueEntry *
file_queue_entry_ref (FileQueueEntry *self) {
  g_return_val_if_fail (self != NULL, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
file_queue_entry_unref (FileQueueEntry *self) {
  g_return_if_fail (self != NULL);

  if (g_atomic_int_dec_and_test (&self->ref_count)) {
    g_object_unref (self->file);
    g_free (self);
  }
}

#define RYGEL_MEDIA_EXPORT_TYPE_FILE_QUEUE_ENTRY (file_queue_entry_get_type ())

G_DEFINE_BOXED_TYPE (FileQueueEntry,
                     file_queue_entry,
                     file_queue_entry_ref,
                     file_queue_entry_unref)

#define RYGEL_MEDIA_EXPORT_HARVESTING_TASK_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), RYGEL_MEDIA_EXPORT_TYPE_HARVESTING_TASK, RygelMediaExportHarvestingTaskPrivate))
enum  {
  RYGEL_MEDIA_EXPORT_HARVESTING_TASK_DUMMY_PROPERTY,
  RYGEL_MEDIA_EXPORT_HARVESTING_TASK_CANCELLABLE,
  RYGEL_MEDIA_EXPORT_HARVESTING_TASK_EXTRACTOR,
  RYGEL_MEDIA_EXPORT_HARVESTING_TASK_MONITOR,
  RYGEL_MEDIA_EXPORT_HARVESTING_TASK_ORIGIN,
  RYGEL_MEDIA_EXPORT_HARVESTING_TASK_PARENT,
  RYGEL_MEDIA_EXPORT_HARVESTING_TASK_FLAG
};

#define RYGEL_MEDIA_EXPORT_HARVESTING_TASK_BATCH_SIZE 256
#define RYGEL_MEDIA_EXPORT_HARVESTING_TASK_HARVESTER_ATTRIBUTES G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_TIME_MODIFIED "," G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," G_FILE_ATTRIBUTE_STANDARD_SIZE

static void
rygel_media_export_harvesting_task_enumerate_directory_data_free (RygelMediaExportHarvestingTaskEnumerateDirectoryData *data) {
  if (data->self) {
    g_object_unref (data->self);
  }
  if (data->container) {
    g_object_unref (data->container);
  }
  if (data->directory) {
    g_object_unref (data->directory);
  }
  if (data->enumerator) {
    g_object_unref (data->enumerator);
  }
  g_slice_free (RygelMediaExportHarvestingTaskEnumerateDirectoryData, data);
}

static void
rygel_media_export_harvesting_task_cleanup_database (RygelMediaExportHarvestingTask *self) {
  RygelMediaExportDummyContainer *container;
  GError *error;
  RygelMediaExportHarvestingTaskPrivate *priv;
  GeeList *children_list;
  gint iter;
  gint size;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_HARVESTING_TASK (self));

  priv = self->priv;
  container = RYGEL_MEDIA_EXPORT_DUMMY_CONTAINER (g_queue_peek_head (priv->containers));
  children_list = rygel_media_export_dummy_container_get_children_list (container);
  size = gee_abstract_collection_get_size (GEE_ABSTRACT_COLLECTION (children_list));
  error = NULL;

  for (iter = 0; iter < size; ++iter) {
    gchar *child = gee_list_get (children_list, iter);

    rygel_media_export_media_cache_remove_by_id (priv->cache, child, &error);
    g_free (child);
    if (error) {
      g_warning (_("Failed to get children of container %s: %s"),
                 rygel_media_object_get_id (RYGEL_MEDIA_OBJECT (container)),
                 error->message);
      g_error_free (error);
    }
  }
}

/* Needed because of the cycle: do_update calls on_idle, which calls
 * enumerate_directory, which calls enumerate_children_ready, which
 * calls failed_to_enumerate_folder, which calls do_update. */
static void
rygel_media_export_harvesting_task_do_update (RygelMediaExportHarvestingTask *self);

static void
cleanup_database_and_do_update (RygelMediaExportHarvestingTaskEnumerateDirectoryData *data)
{
  rygel_media_export_harvesting_task_cleanup_database (data->self);
  rygel_media_export_harvesting_task_do_update (data->self);
  g_simple_async_result_complete (data->simple);
  g_object_unref (data->simple);
  rygel_media_export_harvesting_task_enumerate_directory_data_free (data);
}

static void
failed_to_enumerate_folder (RygelMediaExportHarvestingTaskEnumerateDirectoryData *data,
                            GError                                               *error)
{
  g_warning (_("Failed to enumerate folder: %s"),
             error->message);
  g_error_free (error);
  cleanup_database_and_do_update (data);
}

/**
 * Add a file to the meta-data extraction queue.
 *
 * The file will only be added to the queue if one of the following
 * conditions is met:
 *   - The file is not in the cache
 *   - The current mtime of the file is larger than the cached
 *   - The size has changed
 * @param file to check
 * @param info FileInfo of the file to check
 * @return true, if the file has been queued, false otherwise.
 */
static gboolean
rygel_media_export_harvesting_task_push_if_changed_or_unknown (RygelMediaExportHarvestingTask *self,
                                                               GFile                          *file,
                                                               GFileInfo                      *info) {
  GError *inner_error;
  RygelMediaExportHarvestingTaskPrivate *priv;
  gboolean exists;
  gint64 timestamp;
  gint64 size;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_HARVESTING_TASK (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (G_IS_FILE_INFO (info), FALSE);

  priv = self->priv;
  inner_error = NULL;
  timestamp = 0;
  size = 0;
  exists = rygel_media_export_media_cache_exists (priv->cache, file, &timestamp, &size, &inner_error);
  if (inner_error) {
    g_warning (_("Failed to query database: %s"), inner_error->message);
    return FALSE;
  }
  if (exists) {
    gint64 mtime = (gint64) g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

    if (mtime > timestamp ||
        g_file_info_get_size (info) != size) {
      gee_queue_offer (priv->files, file_queue_entry_new (file, TRUE));
      return TRUE;
    }
  } else {
    gee_queue_offer (priv->files, file_queue_entry_new (file, FALSE));
    return TRUE;
  }

  return FALSE;
}

static gboolean
rygel_media_export_harvesting_task_process_file (RygelMediaExportHarvestingTask *self,
                                                 GFile                          *file,
                                                 GFileInfo                      *info,
                                                 RygelMediaContainer            *parent) {
  GError *inner_error;
  RygelMediaExportHarvestingTaskPrivate *priv;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_HARVESTING_TASK (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (G_IS_FILE_INFO (info), FALSE);
  g_return_val_if_fail (RYGEL_IS_MEDIA_CONTAINER (parent), FALSE);

  if (g_file_info_get_name (info)[0] == '.') {
    return FALSE;
  }

  priv = self->priv;
  inner_error = NULL;
  if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
    RygelMediaExportDummyContainer* container;

    rygel_media_export_recursive_file_monitor_add (priv->monitor, file, NULL, NULL);
    container = rygel_media_export_dummy_container_new (file, parent);
    g_queue_push_tail (priv->containers, container); /* takes ownership */
    rygel_media_export_media_cache_save_container (priv->cache,
                                                   RYGEL_MEDIA_CONTAINER (container),
                                                   &inner_error);
    if (inner_error) {
      g_warning (_("Failed to update database: %s"),
                 inner_error->message);
      g_error_free (inner_error);
      return FALSE;
    }

    return TRUE;
  } else {
    if (rygel_media_export_harvester_is_eligible (info)) {
      return rygel_media_export_harvesting_task_push_if_changed_or_unknown (self,
                                                                            file,
                                                                            info);
    }
  }

  return FALSE;
}

static gboolean
rygel_media_export_harvesting_task_process_children (RygelMediaExportHarvestingTask *self,
                                                     GList                          *list) {
  RygelMediaExportDummyContainer* container;
  GList* iter;
  GFile *file;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_HARVESTING_TASK (self), FALSE);

  if (!list || g_cancellable_is_cancelled (self->priv->cancellable)) {
    return FALSE;
  }
  container = RYGEL_MEDIA_EXPORT_DUMMY_CONTAINER (g_queue_peek_head (self->priv->containers));
  file = rygel_media_export_dummy_container_get_g_file (container);

  for (iter = list; iter; iter = iter->next) {
    GFileInfo *info = G_FILE_INFO (iter->data);
    GFile *child = g_file_get_child (file, g_file_info_get_name (info));

    rygel_media_export_dummy_container_seen (container, child);
    rygel_media_export_harvesting_task_process_file (self, child, info, RYGEL_MEDIA_CONTAINER (container));
    g_object_unref (child);
  }

  return TRUE;
}

/* Needed because of cycle: get_next_files calls next_files_ready,
 * which calls get_next_files */
static void
get_next_files (RygelMediaExportHarvestingTaskEnumerateDirectoryData *data);

static void
rygel_media_export_harvesting_task_enumerator_close_ready (GObject      *source_object G_GNUC_UNUSED,
                                                           GAsyncResult *res,
                                                           gpointer      user_data)
{
  GError *error = NULL;
  RygelMediaExportHarvestingTaskEnumerateDirectoryData *data = (RygelMediaExportHarvestingTaskEnumerateDirectoryData *) user_data;

  g_file_enumerator_close_finish (data->enumerator, res, &error);
  if (error) {
    failed_to_enumerate_folder (data, error);
  } else {
    cleanup_database_and_do_update (data);
  }
}

static void
rygel_media_export_harvesting_task_next_files_ready (GObject      *source_object G_GNUC_UNUSED,
                                                     GAsyncResult *res,
                                                     gpointer      user_data)
{
  RygelMediaExportHarvestingTaskEnumerateDirectoryData *data = (RygelMediaExportHarvestingTaskEnumerateDirectoryData *) user_data;
  GError *error = NULL;
  GList *list = g_file_enumerator_next_files_finish (data->enumerator,
                                                     res,
                                                     &error);

  if (error) {
    failed_to_enumerate_folder (data, error);
  } else {
    gboolean make_another_round = rygel_media_export_harvesting_task_process_children (data->self, list);

    if (list) {
      g_list_free_full (list, g_object_unref);
    }
    if (make_another_round) {
      get_next_files (data);
    } else {
      g_file_enumerator_close_async (data->enumerator,
                                     G_PRIORITY_DEFAULT,
                                     data->self->priv->cancellable,
                                     rygel_media_export_harvesting_task_enumerator_close_ready,
                                     data);
    }
  }
}

static void
get_next_files (RygelMediaExportHarvestingTaskEnumerateDirectoryData *data)
{
  g_file_enumerator_next_files_async (data->enumerator,
                                      RYGEL_MEDIA_EXPORT_HARVESTING_TASK_BATCH_SIZE,
                                      G_PRIORITY_DEFAULT,
                                      data->self->priv->cancellable,
                                      rygel_media_export_harvesting_task_next_files_ready,
                                      data);
}

static void
rygel_media_export_harvesting_task_enumerate_children_ready (GObject      *source_object,
                                                             GAsyncResult *res,
                                                             gpointer      user_data) {
  RygelMediaExportHarvestingTaskEnumerateDirectoryData *data = (RygelMediaExportHarvestingTaskEnumerateDirectoryData *) user_data;
  GError *error = NULL;

  data->enumerator = g_file_enumerate_children_finish (G_FILE (source_object), res, &error);

  if (error) {
    failed_to_enumerate_folder (data, error);
  } else {
    get_next_files (data);
  }
}

static void
rygel_media_export_harvesting_task_enumerate_directory (RygelMediaExportHarvestingTask *self,
                                                        GAsyncReadyCallback             callback,
                                                        gpointer                        user_data) {
  RygelMediaExportHarvestingTaskPrivate *priv;
  RygelMediaExportHarvestingTaskEnumerateDirectoryData *data;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_HARVESTING_TASK (self));

  priv = self->priv;
  data = g_slice_new0 (RygelMediaExportHarvestingTaskEnumerateDirectoryData);
  data->container = RYGEL_MEDIA_EXPORT_DUMMY_CONTAINER (g_object_ref (g_queue_peek_head (priv->containers)));
  data->directory = g_object_ref (rygel_media_export_dummy_container_get_g_file (data->container));
  data->self = g_object_ref (self);
  data->simple = g_simple_async_result_new (G_OBJECT (self),
                                            callback,
                                            user_data,
                                            rygel_media_export_harvesting_task_enumerate_directory);

  g_file_enumerate_children_async (data->directory,
                                   RYGEL_MEDIA_EXPORT_HARVESTING_TASK_HARVESTER_ATTRIBUTES,
                                   G_FILE_QUERY_INFO_NONE,
                                   G_PRIORITY_DEFAULT,
                                   priv->cancellable,
                                   rygel_media_export_harvesting_task_enumerate_children_ready,
                                   data);
}

static gboolean
rygel_media_export_harvesting_task_on_idle (RygelMediaExportHarvestingTask *self) {
  RygelMediaExportHarvestingTaskPrivate *priv;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_HARVESTING_TASK (self), FALSE);

  priv = self->priv;
  if (g_cancellable_is_cancelled (priv->cancellable)) {
    g_signal_emit_by_name (RYGEL_STATE_MACHINE (self), "completed");

    return FALSE;
  }

  if (!gee_collection_get_is_empty (GEE_COLLECTION (priv->files))) {
    FileQueueEntry *entry;
    gchar *uri;

    entry = gee_queue_peek (self->priv->files);
    uri = g_file_get_uri (entry->file);
    g_debug ("Scheduling file %s for meta-data extraction…", uri);
    g_free (uri);
    rygel_media_export_metadata_extractor_extract (priv->extractor, entry->file);
    file_queue_entry_unref (entry);
  } else if (!g_queue_is_empty (priv->containers)) {
      rygel_media_export_harvesting_task_enumerate_directory (self, NULL, NULL);
  } else {
    if (priv->flag) {
      rygel_media_export_media_cache_flag_object (priv->cache, priv->origin, priv->flag, NULL);
    }
    rygel_media_container_updated (priv->parent, RYGEL_MEDIA_OBJECT (priv->parent), RYGEL_OBJECT_EVENT_TYPE_MODIFIED, FALSE);
    rygel_media_export_media_cache_save_container (priv->cache, priv->parent, NULL);
    g_signal_emit_by_name (RYGEL_STATE_MACHINE (self), "completed");
  }
  return FALSE;
}

/**
 * If all files of a container were processed, notify the container
 * about this and set the updating signal.
 * Reschedule the iteration and extraction
 */
static void
rygel_media_export_harvesting_task_do_update (RygelMediaExportHarvestingTask *self) {
  RygelMediaExportHarvestingTaskPrivate *priv;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_HARVESTING_TASK (self));

  priv = self->priv;
  if (gee_collection_get_is_empty (GEE_COLLECTION (priv->files)) &&
      !g_queue_is_empty (priv->containers)) {
    RygelMediaContainer* container = RYGEL_MEDIA_CONTAINER (g_queue_peek_head (priv->containers));
    GError *error = NULL;
    gint size = rygel_media_export_media_cache_get_child_count (priv->cache,
                                                                rygel_media_object_get_id (RYGEL_MEDIA_OBJECT (container)),
                                                                &error);

    if (error) {
      goto out;
    }
    if (size == 0) {
      RygelTrackableContainer *trackable = RYGEL_TRACKABLE_CONTAINER (rygel_media_object_get_parent (RYGEL_MEDIA_OBJECT (container)));

      rygel_trackable_container_remove_child_tracked (trackable,
                                                      RYGEL_MEDIA_OBJECT (container),
                                                      NULL,
                                                      NULL);
    }
  out:
    if (error) {
      g_error_free (error);
    }

    g_queue_pop_head (priv->containers);
    g_object_unref (container);
  }
  rygel_media_export_harvesting_task_on_idle (self);
}

static void
rygel_media_export_harvesting_task_on_extracted_cb (RygelMediaExportHarvestingTask *self,
                                                    GFile                          *file,
                                                    GstDiscovererInfo              *info,
                                                    GUPnPDLNAProfile               *profile,
                                                    GFileInfo                      *file_info) {
  RygelMediaExportHarvestingTaskPrivate *priv;
  gboolean ignore;
  FileQueueEntry *entry;
  RygelMediaItem *item;
  RygelMediaContainer *container;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_HARVESTING_TASK (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (info == NULL || GST_IS_DISCOVERER_INFO (info));
  g_return_if_fail (profile == NULL || GUPNP_IS_DLNA_PROFILE (profile));
  g_return_if_fail (G_IS_FILE_INFO (file_info));

  priv = self->priv;

  if (g_cancellable_is_cancelled (priv->cancellable)) {
    g_signal_emit_by_name (RYGEL_STATE_MACHINE (self), "completed");
  }

  entry = gee_queue_peek (priv->files);
  ignore = (!entry->file || !g_file_equal (file, entry->file));
  if (ignore) {
    file_queue_entry_unref (entry);
    return;
  }
  container = RYGEL_MEDIA_CONTAINER (g_queue_peek_head (priv->containers));

  if (!info) {
    item = rygel_media_export_item_factory_create_simple (container, file, file_info);
  } else {
    item = rygel_media_export_item_factory_create_from_info (container,
                                                             file,
                                                             info,
                                                             profile,
                                                             file_info);
  }
  if (item) {
    RygelTrackableContainer *trackable;
    RygelMediaObject *object = RYGEL_MEDIA_OBJECT (item);

    rygel_media_object_set_parent_ref (object, container);
    if (entry->known) {
      rygel_updatable_object_commit (RYGEL_UPDATABLE_OBJECT (item), NULL, NULL);
    } else {
      trackable = RYGEL_TRACKABLE_CONTAINER (rygel_media_object_get_parent (object));
      rygel_trackable_container_add_child_tracked (trackable, object, NULL, NULL);
    }
    g_object_unref (item);
  }
  file_queue_entry_unref (entry);
  file_queue_entry_unref (gee_queue_poll (priv->files));
  rygel_media_export_harvesting_task_do_update (self);
}

static void
rygel_media_export_harvesting_task_on_extracted_cb_rygel_media_export_metadata_extractor_extraction_done (RygelMediaExportMetadataExtractor *sender G_GNUC_UNUSED,
                                                                                                          GFile                             *file,
                                                                                                          GstDiscovererInfo                 *info,
                                                                                                          GUPnPDLNAProfile                  *profile,
                                                                                                          GFileInfo                         *file_info,
                                                                                                          gpointer                           self) {
  rygel_media_export_harvesting_task_on_extracted_cb (self,
                                                      file,
                                                      info,
                                                      profile,
                                                      file_info);
}

static void
rygel_media_export_harvesting_task_on_extractor_error_cb (RygelMediaExportHarvestingTask *self,
                                                          GFile                          *file,
                                                          GError                         *error) {
  RygelMediaExportHarvestingTaskPrivate *priv;
  gboolean ignore;
  gchar *uri;
  FileQueueEntry *entry;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_HARVESTING_TASK (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (error != NULL);

  priv = self->priv;
  entry = gee_queue_peek (priv->files);
  ignore = (!entry->file || !g_file_equal (file, entry->file));
  file_queue_entry_unref (entry);

  if (ignore) {
    return;
  }

  uri = g_file_get_uri (file);
  g_debug ("Skipping %s; extraction completely failed: %s", uri, error->message);
  g_free (uri);
  file_queue_entry_unref (gee_queue_poll (priv->files));
  rygel_media_export_harvesting_task_do_update (self);
}

static void
rygel_media_export_harvesting_task_on_extractor_error_cb_rygel_media_export_metadata_extractor_error (RygelMediaExportMetadataExtractor *sender G_GNUC_UNUSED,
                                                                                                      GFile                             *file,
                                                                                                      GError                            *err,
                                                                                                      gpointer                           self) {
  rygel_media_export_harvesting_task_on_extractor_error_cb (self, file, err);
}

RygelMediaExportHarvestingTask *
rygel_media_export_harvesting_task_new (RygelMediaExportMetadataExtractor    *extractor,
                                        RygelMediaExportRecursiveFileMonitor *monitor,
                                        GFile                                *file,
                                        RygelMediaContainer                  *parent,
                                        const gchar                          *flag) {
  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_METADATA_EXTRACTOR (extractor), NULL);
  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_RECURSIVE_FILE_MONITOR (monitor), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (RYGEL_IS_MEDIA_CONTAINER (parent), NULL);
  /* flag can be NULL */

  return RYGEL_MEDIA_EXPORT_HARVESTING_TASK (g_object_new (RYGEL_MEDIA_EXPORT_TYPE_HARVESTING_TASK,
                                                           "extractor", extractor,
                                                           "monitor", monitor,
                                                           "origin", file,
                                                           "parent", parent,
                                                           "flag", flag,
                                                           NULL));
}

void
rygel_media_export_harvesting_task_cancel (RygelMediaExportHarvestingTask *self) {
  GCancellable *cancellable;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_HARVESTING_TASK (self));

  cancellable = g_cancellable_new ();
  rygel_state_machine_set_cancellable (RYGEL_STATE_MACHINE (self), cancellable);
  g_cancellable_cancel (cancellable);
}

static void
rygel_media_export_harvesting_task_real_run_data_free (RygelMediaExportHarvestingTaskRunData *data) {
  g_object_unref (data->self);
  g_slice_free (RygelMediaExportHarvestingTaskRunData, data);
}

static void
rygel_media_export_harvesting_task_run_ready (GObject      *source_object,
                                              GAsyncResult *res,
                                              gpointer      user_data) {
  RygelMediaExportHarvestingTaskRunData *data = (RygelMediaExportHarvestingTaskRunData *) user_data;
  RygelMediaExportHarvestingTask *self = data->self;
  RygelMediaExportHarvestingTaskPrivate *priv = self->priv;
  GFile *origin = G_FILE (source_object);
  GError *inner_error = NULL;
  GFileInfo *info = g_file_query_info_finish (origin, res, &inner_error);

  if (!inner_error) {
    if (rygel_media_export_harvesting_task_process_file (self,
                                                         origin,
                                                         info,
                                                         priv->parent)) {
      if (g_file_info_get_file_type (info) != G_FILE_TYPE_DIRECTORY) {
        g_queue_push_tail (priv->containers,
                           g_object_ref (priv->parent));
      }
      rygel_media_export_harvesting_task_on_idle (self);
    } else {
      g_signal_emit_by_name (RYGEL_STATE_MACHINE (self), "completed");
    }
    g_object_unref (info);
  } else {
    gchar *uri = g_file_get_uri (origin);

    if (g_error_matches (inner_error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      g_debug ("Harvesting of uri %s was cancelled", uri);
    } else {
      g_warning (_("Failed to harvest file %s: %s"),
                 uri,
                 inner_error->message);
    }

    g_free (uri);
    g_signal_emit_by_name (RYGEL_STATE_MACHINE (self), "completed");
    g_error_free (inner_error);
  }
  g_simple_async_result_complete (data->simple);
  g_object_unref (data->simple);
  rygel_media_export_harvesting_task_real_run_data_free (data);
}

/**
 * Extract all metainformation from a given file.
 *
 * What action will be taken depends on the arguments
 * * file is a simple file. Then only information of this
 *   file will be extracted
 * * file is a directory and recursive is false. The children
 *   of the directory (if not directories themselves) will be
 *   enqueued for extraction
 * * file is a directory and recursive is true. ++ All ++ children
 *   of the directory will be enqueued for extraction, even directories
 *
 * No matter how many children are contained within file's hierarchy,
 * only one event is sent when all the children are done.
 */
static void
rygel_media_export_harvesting_task_real_run (RygelStateMachine   *base,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data) {
  RygelMediaExportHarvestingTask *self = RYGEL_MEDIA_EXPORT_HARVESTING_TASK (base);
  RygelMediaExportHarvestingTaskPrivate *priv = self->priv;
  RygelMediaExportHarvestingTaskRunData *data = g_slice_new0 (RygelMediaExportHarvestingTaskRunData);

  data->simple = g_simple_async_result_new (G_OBJECT (self),
                                            callback,
                                            user_data,
                                            rygel_media_export_harvesting_task_real_run);
  data->self = g_object_ref (self);

  g_file_query_info_async (priv->origin,
                           RYGEL_MEDIA_EXPORT_HARVESTING_TASK_HARVESTER_ATTRIBUTES,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           priv->cancellable,
                           rygel_media_export_harvesting_task_run_ready,
                           data);
}

static void
rygel_media_export_harvesting_task_real_run_finish (RygelStateMachine *base G_GNUC_UNUSED,
                                                    GAsyncResult      *res G_GNUC_UNUSED) {
}

static GCancellable *
rygel_media_export_harvesting_task_real_get_cancellable (RygelStateMachine *base) {
  RygelMediaExportHarvestingTask *self = RYGEL_MEDIA_EXPORT_HARVESTING_TASK (base);

  return self->priv->cancellable;
}

static void
rygel_media_export_harvesting_task_real_set_cancellable (RygelStateMachine *base,
                                                         GCancellable      *value) {
  RygelMediaExportHarvestingTask *self = RYGEL_MEDIA_EXPORT_HARVESTING_TASK (base);
  RygelMediaExportHarvestingTaskPrivate *priv = self->priv;

  if (value) {
    g_object_ref (value);
  }
  if (priv->cancellable) {
    g_object_unref (priv->cancellable);
  }
  priv->cancellable = value;

  g_object_notify (G_OBJECT (self), "cancellable");
}

static void
rygel_media_export_harvesting_task_get_property (GObject    *object,
                                                 guint       property_id,
                                                 GValue     *value,
                                                 GParamSpec *pspec) {
  RygelMediaExportHarvestingTask *self = RYGEL_MEDIA_EXPORT_HARVESTING_TASK (object);
  RygelMediaExportHarvestingTaskPrivate *priv = self->priv;

  switch (property_id) {
  case RYGEL_MEDIA_EXPORT_HARVESTING_TASK_CANCELLABLE:
    g_value_set_object (value, rygel_state_machine_get_cancellable (RYGEL_STATE_MACHINE (self)));
    break;

  case RYGEL_MEDIA_EXPORT_HARVESTING_TASK_EXTRACTOR:
    g_value_set_object (value, priv->extractor);
    break;

  case RYGEL_MEDIA_EXPORT_HARVESTING_TASK_MONITOR:
    g_value_set_object (value, priv->monitor);
    break;

  case RYGEL_MEDIA_EXPORT_HARVESTING_TASK_ORIGIN:
    g_value_set_object (value, priv->origin);
    break;

  case RYGEL_MEDIA_EXPORT_HARVESTING_TASK_PARENT:
    g_value_set_object (value, priv->parent);
    break;

  case RYGEL_MEDIA_EXPORT_HARVESTING_TASK_FLAG:
    g_value_set_string (value, priv->flag);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
rygel_media_export_harvesting_task_set_property (GObject      *object,
                                                 guint         property_id,
                                                 const GValue *value,
                                                 GParamSpec   *pspec) {
  RygelMediaExportHarvestingTask *self = RYGEL_MEDIA_EXPORT_HARVESTING_TASK (object);
  RygelMediaExportHarvestingTaskPrivate *priv = self->priv;

  switch (property_id) {
  case RYGEL_MEDIA_EXPORT_HARVESTING_TASK_CANCELLABLE:
    rygel_state_machine_set_cancellable (RYGEL_STATE_MACHINE (self), g_value_get_object (value));
    break;

  case RYGEL_MEDIA_EXPORT_HARVESTING_TASK_EXTRACTOR:
    /* construct only property */
    priv->extractor = g_value_dup_object (value);
    break;

  case RYGEL_MEDIA_EXPORT_HARVESTING_TASK_MONITOR:
    /* construct only property */
    priv->monitor = g_value_dup_object (value);
    break;

  case RYGEL_MEDIA_EXPORT_HARVESTING_TASK_ORIGIN:
    /* construct only property */
    priv->origin = g_value_dup_object (value);
    break;

  case RYGEL_MEDIA_EXPORT_HARVESTING_TASK_PARENT:
    /* construct only property */
    priv->parent = g_value_dup_object (value);
    break;

  case RYGEL_MEDIA_EXPORT_HARVESTING_TASK_FLAG:
    /* construct only property */
    priv->flag = g_value_dup_string (value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
rygel_media_export_harvesting_task_dispose (GObject *object) {
  RygelMediaExportHarvestingTask *self = RYGEL_MEDIA_EXPORT_HARVESTING_TASK (object);
  RygelMediaExportHarvestingTaskPrivate *priv = self->priv;

  if (priv->origin) {
    GFile *file = priv->origin;

    priv->origin = NULL;
    g_object_unref (file);
  }
  if (priv->extractor) {
    RygelMediaExportMetadataExtractor *extractor = priv->extractor;

    priv->extractor = NULL;
    g_object_unref (extractor);
  }
  if (priv->cache) {
    RygelMediaExportMediaCache *cache = priv->cache;

    priv->cache = NULL;
    g_object_unref (cache);
  }
  if (priv->containers) {
    GQueue *containers = priv->containers;

    priv->containers = NULL;
    g_queue_free_full (containers, g_object_unref);
  }
  if (priv->files) {
    GeeQueue *files = priv->files;

    priv->files = NULL;
    g_object_unref (files);
  }
  if (priv->monitor) {
    RygelMediaExportRecursiveFileMonitor *monitor = priv->monitor;

    priv->monitor = NULL;
    g_object_unref (monitor);
  }
  if (priv->parent) {
    RygelMediaContainer *parent = priv->parent;

    priv->parent = NULL;
    g_object_unref (parent);
  }
  if (priv->cancellable) {
    GCancellable *cancellable = priv->cancellable;

    priv->cancellable = NULL;
    g_object_unref (cancellable);
  }
  G_OBJECT_CLASS (rygel_media_export_harvesting_task_parent_class)->dispose (object);
}

static void
rygel_media_export_harvesting_task_finalize (GObject *object) {
  RygelMediaExportHarvestingTask *self = RYGEL_MEDIA_EXPORT_HARVESTING_TASK (object);
  RygelMediaExportHarvestingTaskPrivate *priv = self->priv;

  g_free (priv->flag);
  G_OBJECT_CLASS (rygel_media_export_harvesting_task_parent_class)->finalize (object);
}

static void
rygel_media_export_harvesting_task_constructed (GObject *object) {
  RygelMediaExportHarvestingTask *self = RYGEL_MEDIA_EXPORT_HARVESTING_TASK (object);
  RygelMediaExportHarvestingTaskPrivate *priv = self->priv;

  G_OBJECT_CLASS (rygel_media_export_harvesting_task_parent_class)->constructed (object);

  priv->cache = rygel_media_export_media_cache_get_default ();

  g_signal_connect_object (priv->extractor,
                           "extraction-done",
                           G_CALLBACK (rygel_media_export_harvesting_task_on_extracted_cb_rygel_media_export_metadata_extractor_extraction_done),
                           self,
                           0);
  g_signal_connect_object (priv->extractor,
                           "error",
                           G_CALLBACK (rygel_media_export_harvesting_task_on_extractor_error_cb_rygel_media_export_metadata_extractor_error),
                           self,
                           0);
  priv->files = GEE_QUEUE (gee_linked_list_new (RYGEL_MEDIA_EXPORT_TYPE_FILE_QUEUE_ENTRY,
                                                (GBoxedCopyFunc) file_queue_entry_ref,
                                                (GBoxedFreeFunc) file_queue_entry_unref,
                                                NULL, NULL, NULL));
  priv->containers = g_queue_new ();
}

static void
rygel_media_export_harvesting_task_class_init (RygelMediaExportHarvestingTaskClass *task_class) {
  GObjectClass *object_class = G_OBJECT_CLASS (task_class);

  object_class->get_property = rygel_media_export_harvesting_task_get_property;
  object_class->set_property = rygel_media_export_harvesting_task_set_property;
  object_class->dispose = rygel_media_export_harvesting_task_dispose;
  object_class->finalize = rygel_media_export_harvesting_task_finalize;
  object_class->constructed = rygel_media_export_harvesting_task_constructed;

  g_object_class_install_property (object_class,
                                   RYGEL_MEDIA_EXPORT_HARVESTING_TASK_CANCELLABLE,
                                   g_param_spec_object ("cancellable",
                                                        "cancellable",
                                                        "cancellable",
                                                        G_TYPE_CANCELLABLE,
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class,
                                   RYGEL_MEDIA_EXPORT_HARVESTING_TASK_EXTRACTOR,
                                   g_param_spec_object ("extractor",
                                                        "extractor",
                                                        "extractor",
                                                        RYGEL_MEDIA_EXPORT_TYPE_METADATA_EXTRACTOR,
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   RYGEL_MEDIA_EXPORT_HARVESTING_TASK_MONITOR,
                                   g_param_spec_object ("monitor",
                                                        "monitor",
                                                        "monitor",
                                                        RYGEL_MEDIA_EXPORT_TYPE_RECURSIVE_FILE_MONITOR,
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   RYGEL_MEDIA_EXPORT_HARVESTING_TASK_ORIGIN,
                                   g_param_spec_object ("origin",
                                                        "origin",
                                                        "origin",
                                                        G_TYPE_FILE,
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   RYGEL_MEDIA_EXPORT_HARVESTING_TASK_PARENT,
                                   g_param_spec_object ("parent",
                                                        "parent",
                                                        "parent",
                                                        RYGEL_TYPE_MEDIA_CONTAINER,
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   RYGEL_MEDIA_EXPORT_HARVESTING_TASK_FLAG,
                                   g_param_spec_string ("flag",
                                                        "flag",
                                                        "flag",
                                                        NULL,
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (task_class, sizeof (RygelMediaExportHarvestingTaskPrivate));
}

static void
rygel_media_export_harvesting_task_rygel_state_machine_interface_init (RygelStateMachineIface *iface) {
  iface->run = rygel_media_export_harvesting_task_real_run;
  iface->run_finish = rygel_media_export_harvesting_task_real_run_finish;
  iface->get_cancellable = rygel_media_export_harvesting_task_real_get_cancellable;
  iface->set_cancellable = rygel_media_export_harvesting_task_real_set_cancellable;
}

static void
rygel_media_export_harvesting_task_init (RygelMediaExportHarvestingTask * self) {
  self->priv = RYGEL_MEDIA_EXPORT_HARVESTING_TASK_GET_PRIVATE (self);
}

GFile *
rygel_media_export_harvesting_task_get_origin (RygelMediaExportHarvestingTask *self) {
  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_HARVESTING_TASK (self), NULL);

  return self->priv->origin;
}
