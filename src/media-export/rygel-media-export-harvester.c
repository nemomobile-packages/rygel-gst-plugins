/*
 * Copyright (C) 2010 Jens Georg <mail@jensge.org>.
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
#include <rygel-core.h>

#include "rygel-media-export-errors.h"
#include "rygel-media-export-harvester.h"
#include "rygel-media-export-harvesting-task.h"
#include "rygel-media-export-metadata-extractor.h"
#include "rygel-media-export-media-cache.h"
#include "rygel-media-export-recursive-file-monitor.h"
#include "rygel-media-export-root-container.h"


/**
 * This class takes care of the book-keeping of running and finished
 * extraction tasks running within the media-export plugin
 */

G_DEFINE_TYPE (RygelMediaExportHarvester, rygel_media_export_harvester, G_TYPE_OBJECT)

typedef struct _Block3Data Block3Data;


struct _RygelMediaExportHarvesterPrivate {
  GeeHashMap *tasks;
  GeeHashMap *extraction_grace_timers;
  RygelMediaExportMetadataExtractor *extractor;
  RygelMediaExportRecursiveFileMonitor *monitor;
  GCancellable *cancellable;
  GeeArrayList *locations;
};


struct _Block3Data {
  RygelMediaExportHarvester *self;
  GFile *file;
};

#define RYGEL_MEDIA_EXPORT_HARVESTER_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                RYGEL_MEDIA_EXPORT_TYPE_HARVESTER, \
                                RygelMediaExportHarvesterPrivate))

enum  {
  RYGEL_MEDIA_EXPORT_HARVESTER_DUMMY_PROPERTY,
  RYGEL_MEDIA_EXPORT_HARVESTER_LOCATIONS,
  RYGEL_MEDIA_EXPORT_HARVESTER_CANCELLABLE
};

enum {
  RYGEL_MEDIA_EXPORT_HARVESTER_SIGNAL_DONE,

  RYGEL_MEDIA_EXPORT_HARVESTER_SIGNALS_COUNT
};

static guint signals [RYGEL_MEDIA_EXPORT_HARVESTER_SIGNALS_COUNT];

#define RYGEL_MEDIA_EXPORT_HARVESTER_FILE_CHANGE_DEFAULT_GRACE_PERIOD ((guint) 5)

gboolean
rygel_media_export_harvester_is_eligible (GFileInfo *info) {
  return (g_str_has_prefix (g_file_info_get_content_type (info), "image/") ||
          g_str_has_prefix (g_file_info_get_content_type (info), "video/") ||
          g_str_has_prefix (g_file_info_get_content_type (info), "audio/") ||
          g_strcmp0 (g_file_info_get_content_type (info), "application/ogg"));
}

static void
rygel_media_export_harvester_on_file_added (RygelMediaExportHarvester *self,
                                            GFile                     *file) {
  gchar *uri;
  GError *error;
  RygelMediaExportMediaCache *cache;
  GFileInfo *info;
  RygelMediaExportHarvesterPrivate *priv;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_HARVESTER (self));
  g_return_if_fail (G_IS_FILE (file));

  uri = g_file_get_uri (file);
  g_debug ("Filesystem events settled for %s, scheduling extraction…", uri);
  g_free (uri);
  cache = rygel_media_export_media_cache_get_default ();
  priv = self->priv;
  error = NULL;
  info = g_file_query_info (file,
                            G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                            G_FILE_QUERY_INFO_NONE,
                            priv->cancellable,
                            &error);
  if (error) {
    g_warning ("Failed to query file: %s", error->message);
    g_error_free (error);
    error = NULL;

    goto out;
  }
  if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY ||
      rygel_media_export_harvester_is_eligible (info)) {
    RygelMediaContainer *parent_container = NULL;
    GFile *current = g_object_ref (file);
    GeeAbstractCollection *abstract_locations = GEE_ABSTRACT_COLLECTION (priv->locations);

    do {
      GFile *parent = g_file_get_parent (current);
      gchar *id = rygel_media_export_media_cache_get_id (parent);
      RygelMediaObject *tmp_object = rygel_media_export_media_cache_get_object (cache, id, &error);

      if (error) {
        g_warning (_("Error fetching object '%s' from database: %s"),
                   id,
                   error->message);
        g_free (id);
        g_object_unref (parent);

        g_error_free (error);
        error = NULL;

        goto inner_out;
      }
      g_free (id);
      if (parent_container) {
        g_object_unref (parent_container);
        parent_container = NULL;
      }
      if (RYGEL_IS_MEDIA_CONTAINER (tmp_object)) {
        parent_container = RYGEL_MEDIA_CONTAINER (tmp_object);
      } else {
        g_object_unref (tmp_object);
      }
      if (!parent_container) {
        g_object_ref (parent);
        g_object_unref (current);
        current = parent;
      }
      g_object_unref (parent);
      if (!parent_container &&
          gee_abstract_collection_contains (abstract_locations, current)) {
        RygelMediaObject* another_object = rygel_media_export_media_cache_get_object (cache,
                                                                                      RYGEL_MEDIA_EXPORT_ROOT_CONTAINER_FILESYSTEM_FOLDER_ID,
                                                                                      &error);

        if (error) {
          goto inner_out;
        }
        if (parent_container) {
          g_object_unref (parent_container);
          parent_container = NULL;
        }
        if (RYGEL_IS_MEDIA_CONTAINER (another_object)) {
          parent_container = RYGEL_MEDIA_CONTAINER (another_object);
        }

        break;
      }
    } while (!parent_container);
    rygel_media_export_harvester_schedule (self, current, parent_container, NULL);
  inner_out:

    g_object_unref (current);
    if (parent_container) {
      g_object_unref (parent_container);
    }
  } else {
    gchar* file_uri = g_file_get_uri (file);

    g_debug ("%s is not eligible for extraction", file_uri);
    g_free (file_uri);
  }
 out:
  g_object_unref (cache);
}

static gboolean
grace_period_gsource_func (gpointer user_data) {
  Block3Data *data3 = (Block3Data *) user_data;

  rygel_media_export_harvester_on_file_added (data3->self, data3->file);
  return FALSE;
}

static void
block3_data_free (gpointer user_data) {
  Block3Data* data3 = (Block3Data*) user_data;

  if (data3) {
    g_object_unref (data3->file);
    g_object_unref (data3->self);
    g_slice_free (Block3Data, data3);
  }
}

static void
rygel_media_export_harvester_on_changes_done (RygelMediaExportHarvester  *self,
                                              GFile                      *file,
                                              GError                    **error G_GNUC_UNUSED) {
  Block3Data *data3;
  GeeAbstractMap *abstract_grace_timers;
  guint timeout;
  RygelMediaExportHarvesterPrivate *priv;
  gchar *basename;
  gboolean hidden;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_HARVESTER (self));
  g_return_if_fail (G_IS_FILE (file));

  basename = g_file_get_basename (file);
  hidden = (basename ? (basename[0] == '.') : FALSE);
  g_free (basename);

  if (hidden) {
    return;
  }

  data3 = g_slice_new0 (Block3Data);
  data3->self = g_object_ref (self);
  data3->file = g_object_ref (file);
  priv = self->priv;
  abstract_grace_timers = GEE_ABSTRACT_MAP (priv->extraction_grace_timers);
  if (gee_abstract_map_has_key (abstract_grace_timers, file)) {
    g_source_remove ((guint) ((guintptr) gee_abstract_map_get (abstract_grace_timers, file)));
  } else {
    gchar *uri = g_file_get_uri (file);
    g_debug ("Starting grace timer for harvesting %s…", uri);
    g_free (uri);
  }

  timeout = g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
                                        RYGEL_MEDIA_EXPORT_HARVESTER_FILE_CHANGE_DEFAULT_GRACE_PERIOD,
                                        grace_period_gsource_func,
                                        data3,
                                        block3_data_free);
  gee_abstract_map_set (abstract_grace_timers, file, (gpointer) ((guintptr) timeout));
}

static void
rygel_media_export_harvester_on_file_removed (RygelMediaExportHarvester  *self,
                                              GFile                      *file,
                                              GError                    **error G_GNUC_UNUSED) {
  RygelMediaExportMediaCache *cache;
  GError *inner_error;
  GeeAbstractMap *abstract_grace_timers;
  gchar *id;
  RygelMediaObject *object;
  RygelMediaContainer *parent;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_HARVESTER (self));
  g_return_if_fail (G_IS_FILE (file));

  cache = rygel_media_export_media_cache_get_default ();
  abstract_grace_timers = GEE_ABSTRACT_MAP (self->priv->extraction_grace_timers);
  if (gee_abstract_map_has_key (abstract_grace_timers, file)) {
    g_source_remove ((guint) ((guintptr) gee_abstract_map_get (abstract_grace_timers, file)));
    gee_abstract_map_unset (abstract_grace_timers, file, NULL);
  }
  rygel_media_export_harvester_cancel (self, file);
  id = rygel_media_export_media_cache_get_id (file);
  inner_error = NULL;
  object = rygel_media_export_media_cache_get_object (cache, id, &inner_error);

  if (inner_error) {
    g_warning ("Failed to get an object with id %s from database: %s",
               id,
               inner_error->message);
    g_error_free (inner_error);
    goto out;
  }

  parent = NULL;
  while (object) {
    RygelMediaContainer *tmp_parent = rygel_media_object_get_parent (object);

    if (tmp_parent) {
      g_object_ref (tmp_parent);
    }
    if (parent) {
      g_object_unref (parent);
    }
    parent = tmp_parent;
    if (RYGEL_IS_TRACKABLE_CONTAINER (parent)) {
      rygel_trackable_container_remove_child_tracked (RYGEL_TRACKABLE_CONTAINER (parent), object, NULL, NULL);
    }
    if (!parent) {
      break;
    }
    rygel_media_container_set_child_count (parent, rygel_media_container_get_child_count (parent) - 1);
    if (rygel_media_container_get_child_count (parent)) {
      break;
    }
    g_object_ref (parent);
    g_object_unref (object);
    object = RYGEL_MEDIA_OBJECT (parent);
  }
  if (parent) {
    rygel_media_container_updated (parent, NULL, RYGEL_OBJECT_EVENT_TYPE_MODIFIED, FALSE);
    rygel_media_export_media_cache_save_container (cache, parent, NULL);
  }
 out:
  if (parent) {
    g_object_unref (parent);
  }
  if (object) {
    g_object_unref (object);
  }
  g_object_unref (cache);
  g_free (id);
}

static void
rygel_media_export_harvester_on_file_changed (RygelMediaExportHarvester *self,
                                              GFile                     *file,
                                              GFile                     *other G_GNUC_UNUSED,
                                              GFileMonitorEvent          event) {
  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_HARVESTER (self));
  g_return_if_fail (G_IS_FILE (file));

  switch (event) {
  case G_FILE_MONITOR_EVENT_CREATED:
  case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    rygel_media_export_harvester_on_changes_done (self, file, NULL);
    break;

  case G_FILE_MONITOR_EVENT_DELETED:
    rygel_media_export_harvester_on_file_removed (self, file, NULL);
    break;

  default:
      break;
  }
}

static void
rygel_media_export_harvester_on_file_changed_rygel_media_export_recursive_file_monitor_changed (RygelMediaExportRecursiveFileMonitor *sender G_GNUC_UNUSED,
                                                                                                GFile                                *file,
                                                                                                GFile                                *other_file,
                                                                                                GFileMonitorEvent                     event_type,
                                                                                                gpointer                              user_data) {
  rygel_media_export_harvester_on_file_changed (RYGEL_MEDIA_EXPORT_HARVESTER (user_data), file, other_file, event_type);
}

RygelMediaExportHarvester *
rygel_media_export_harvester_new (GCancellable *cancellable,
                                  GeeArrayList *locations) {
  g_return_val_if_fail (G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (GEE_IS_ARRAY_LIST (locations), NULL);

  return RYGEL_MEDIA_EXPORT_HARVESTER (g_object_new (RYGEL_MEDIA_EXPORT_TYPE_HARVESTER,
                                                     "cancellable", cancellable,
                                                     "locations", locations,
                                                     NULL));
}

static void
rygel_media_export_harvester_constructed (GObject *object)
{
  RygelMediaExportHarvester *self = RYGEL_MEDIA_EXPORT_HARVESTER (object);
  RygelMediaExportHarvesterPrivate *priv = self->priv;

  G_OBJECT_CLASS (rygel_media_export_harvester_parent_class)->constructed (object);

  priv->extractor = rygel_media_export_metadata_extractor_new ();
  priv->monitor = rygel_media_export_recursive_file_monitor_new (priv->cancellable);
  g_signal_connect_object (priv->monitor,
                           "changed",
                           G_CALLBACK (rygel_media_export_harvester_on_file_changed_rygel_media_export_recursive_file_monitor_changed),
                           self,
                           0);
  self->priv->tasks = gee_hash_map_new (G_TYPE_FILE,
                                        g_object_ref,
                                        g_object_unref,
                                        RYGEL_MEDIA_EXPORT_TYPE_HARVESTING_TASK,
                                        g_object_ref,
                                        g_object_unref,
                                        (GeeHashDataFunc) g_file_hash,
                                        NULL,
                                        NULL,
                                        (GeeEqualDataFunc) g_file_equal,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL);
  self->priv->extraction_grace_timers = gee_hash_map_new (G_TYPE_FILE,
                                                          g_object_ref,
                                                          g_object_unref,
                                                          G_TYPE_UINT,
                                                          NULL,
                                                          NULL,
                                                          (GeeHashDataFunc) g_file_hash,
                                                          NULL,
                                                          NULL,
                                                          (GeeEqualDataFunc) g_file_equal,
                                                          NULL,
                                                          NULL,
                                                          NULL,
                                                          NULL,
                                                          NULL);
}

/**
 * Callback for finished harvester.
 *
 * Updates book-keeping hash.
 * @param state_machine HarvestingTask sending the event
 */
static void
rygel_media_export_harvester_on_file_harvested (RygelMediaExportHarvester *self,
                                                RygelStateMachine         *state_machine) {
  RygelMediaExportHarvestingTask *task;
  gchar *uri;
  GFile *file;
  RygelMediaExportHarvesterPrivate *priv;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_HARVESTER (self));
  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_HARVESTING_TASK (state_machine));

  task = RYGEL_MEDIA_EXPORT_HARVESTING_TASK (state_machine);
  file = rygel_media_export_harvesting_task_get_origin (task);
  uri = g_file_get_uri (file);
  g_message (_("'%s' harvested"), uri);
  g_free (uri);

  priv = self->priv;
  gee_abstract_map_unset (GEE_ABSTRACT_MAP (priv->tasks), file, NULL);
  if (gee_map_get_is_empty (GEE_MAP (priv->tasks))) {
    g_signal_emit (self, signals[RYGEL_MEDIA_EXPORT_HARVESTER_SIGNAL_DONE], 0);
  }
}

static void
rygel_media_export_harvester_on_file_harvested_rygel_state_machine_completed (RygelStateMachine *sender,
                                                                              gpointer           user_data) {
  rygel_media_export_harvester_on_file_harvested (RYGEL_MEDIA_EXPORT_HARVESTER (user_data), sender);
}

/**
 * Put a file on queue for meta-data extraction
 *
 * @param file the file to investigate
 * @param parent container of the filer to be harvested
 * @param flag optional flag for the container to set in the database
 */
void
rygel_media_export_harvester_schedule (RygelMediaExportHarvester *self,
                                       GFile                     *file,
                                       RygelMediaContainer       *parent,
                                       const gchar               *flag) {
  RygelMediaExportHarvesterPrivate *priv;
  RygelMediaExportMetadataExtractor *extractor;
  RygelMediaExportHarvestingTask *task;
  RygelStateMachine *state_task;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_HARVESTER (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (RYGEL_IS_MEDIA_CONTAINER (parent));

  priv = self->priv;
  gee_abstract_map_unset (GEE_ABSTRACT_MAP (priv->extraction_grace_timers), file, NULL);
  if (!priv->extractor) {
    g_warning (_("No metadata extractor available. Will not crawl."));
    return;
  }

  rygel_media_export_harvester_cancel (self, file);
  extractor = rygel_media_export_metadata_extractor_new ();
  task = rygel_media_export_harvesting_task_new (extractor,
                                                 priv->monitor,
                                                 file,
                                                 parent,
                                                 flag);
  g_object_unref (extractor);
  state_task = RYGEL_STATE_MACHINE (task);
  rygel_state_machine_set_cancellable (state_task, priv->cancellable);
  g_signal_connect_object (state_task,
                           "completed",
                           G_CALLBACK (rygel_media_export_harvester_on_file_harvested_rygel_state_machine_completed),
                           self,
                           0);
  gee_abstract_map_set (GEE_ABSTRACT_MAP (priv->tasks), file, task);
  rygel_state_machine_run (state_task, NULL, NULL);
  g_object_unref (task);
}


/**
 * Cancel a running meta-data extraction run
 *
 * @param file file cancel the current run for
 */
void
rygel_media_export_harvester_cancel (RygelMediaExportHarvester *self,
                                     GFile                     *file) {
  RygelMediaExportHarvesterPrivate *priv;
  GeeAbstractMap *abstract_tasks;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_HARVESTER (self));
  g_return_if_fail (G_IS_FILE (file));

  priv = self->priv;
  abstract_tasks = GEE_ABSTRACT_MAP (priv->tasks);
  if (gee_abstract_map_has_key (abstract_tasks, file)) {
    guint signal_id = 0U;
    RygelMediaExportHarvestingTask *task = RYGEL_MEDIA_EXPORT_HARVESTING_TASK (gee_abstract_map_get (abstract_tasks, file));

    g_signal_parse_name ("completed", RYGEL_TYPE_STATE_MACHINE, &signal_id, NULL, FALSE);
    g_signal_handlers_disconnect_matched (RYGEL_STATE_MACHINE (task),
                                          G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                                          signal_id,
                                          0,
                                          NULL, G_CALLBACK (rygel_media_export_harvester_on_file_harvested_rygel_state_machine_completed),
                                          self);
    gee_abstract_map_unset (abstract_tasks, file, NULL);
    rygel_media_export_harvesting_task_cancel (task);
    g_object_unref (task);
  }
}

GeeArrayList *
rygel_media_export_harvester_get_locations (RygelMediaExportHarvester *self) {
  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_HARVESTER (self), NULL);

  return self->priv->locations;
}

static void
rygel_media_export_harvester_set_locations (RygelMediaExportHarvester *self,
                                            GeeArrayList              *value) {
  RygelMediaExportHarvesterPrivate *priv;
  GeeArrayList *new_locations;
  GeeAbstractCollection *abstract_new_locations;
  GeeAbstractList *abstract_value;
  GeeArrayList *old;
  gint iter;
  gint size;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_HARVESTER (self));
  g_return_if_fail (GEE_IS_ARRAY_LIST (value));

  priv = self->priv;
  new_locations = gee_array_list_new (G_TYPE_FILE, g_object_ref, g_object_unref, (GeeEqualDataFunc) g_file_equal, NULL, NULL);
  abstract_new_locations = GEE_ABSTRACT_COLLECTION (new_locations);
  abstract_value = GEE_ABSTRACT_LIST (value);
  size = gee_abstract_collection_get_size (GEE_ABSTRACT_COLLECTION (value));

  for (iter = 0; iter < size; ++iter) {
    GFile *file = gee_abstract_list_get (abstract_value, iter);

    if (g_file_query_exists (file, NULL)) {
      gee_abstract_collection_add (abstract_new_locations, file);
    }
    g_object_unref (file);
  }

  old = priv->locations;
  priv->locations = new_locations;
  if (old) {
    g_object_unref (old);
  }
  g_object_notify (G_OBJECT (self), "locations");
}

static void
rygel_media_export_harvester_dispose (GObject *object) {
  RygelMediaExportHarvester *self = RYGEL_MEDIA_EXPORT_HARVESTER (object);
  RygelMediaExportHarvesterPrivate *priv = self->priv;

  if (priv->tasks) {
    GeeHashMap *map = priv->tasks;

    priv->tasks = NULL;
    g_object_unref (map);
  }
  if (priv->extraction_grace_timers) {
    GeeHashMap *map = priv->extraction_grace_timers;

    priv->extraction_grace_timers = NULL;
    g_object_unref (map);
  }
  if (priv->extractor) {
    RygelMediaExportMetadataExtractor *extractor = priv->extractor;

    priv->extractor = NULL;
    g_object_unref (extractor);
  }
  if (priv->monitor) {
    RygelMediaExportRecursiveFileMonitor *monitor = priv->monitor;

    priv->monitor = NULL;
    g_object_unref (monitor);
  }
  if (priv->cancellable) {
    GCancellable *cancellable = priv->cancellable;

    priv->cancellable = NULL;
    g_object_unref (cancellable);
  }
  if (priv->locations) {
    GeeArrayList *locations = priv->locations;

    priv->locations = NULL;
    g_object_unref (locations);
  }

  G_OBJECT_CLASS (rygel_media_export_harvester_parent_class)->dispose (object);
}

static void
rygel_media_export_harvester_get_property (GObject    *object,
                                           guint       property_id,
                                           GValue     *value,
                                           GParamSpec *pspec) {
  RygelMediaExportHarvester *self = RYGEL_MEDIA_EXPORT_HARVESTER (object);

  switch (property_id) {
  case RYGEL_MEDIA_EXPORT_HARVESTER_LOCATIONS:
    g_value_set_object (value, rygel_media_export_harvester_get_locations (self));
    break;

  case RYGEL_MEDIA_EXPORT_HARVESTER_CANCELLABLE:
    g_value_set_object (value, self->priv->cancellable);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
rygel_media_export_harvester_set_property (GObject      *object,
                                           guint         property_id,
                                           const GValue *value,
                                           GParamSpec   *pspec) {
  RygelMediaExportHarvester *self = RYGEL_MEDIA_EXPORT_HARVESTER (object);

  switch (property_id) {
  case RYGEL_MEDIA_EXPORT_HARVESTER_LOCATIONS:
    /* construct-only property */
    rygel_media_export_harvester_set_locations (self, g_value_get_object (value));
    break;

  case RYGEL_MEDIA_EXPORT_HARVESTER_CANCELLABLE:
    /* construct-only property */
    self->priv->cancellable = g_value_dup_object (value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
rygel_media_export_harvester_class_init (RygelMediaExportHarvesterClass *harvester_class) {
  GObjectClass *object_class = G_OBJECT_CLASS (harvester_class);

  object_class->get_property = rygel_media_export_harvester_get_property;
  object_class->set_property = rygel_media_export_harvester_set_property;
  object_class->dispose = rygel_media_export_harvester_dispose;
  object_class->constructed = rygel_media_export_harvester_constructed;
  g_object_class_install_property (object_class,
                                   RYGEL_MEDIA_EXPORT_HARVESTER_LOCATIONS,
                                   g_param_spec_object ("locations",
                                                        "locations",
                                                        "locations",
                                                        GEE_TYPE_ARRAY_LIST,
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   RYGEL_MEDIA_EXPORT_HARVESTER_CANCELLABLE,
                                   g_param_spec_object ("cancellable",
                                                        "cancellable",
                                                        "cancellable",
                                                        G_TYPE_CANCELLABLE,
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY));

  signals[RYGEL_MEDIA_EXPORT_HARVESTER_SIGNAL_DONE] = g_signal_new ("done",
                                                                    RYGEL_MEDIA_EXPORT_TYPE_HARVESTER,
                                                                    G_SIGNAL_RUN_LAST,
                                                                    0,
                                                                    NULL,
                                                                    NULL,
                                                                    NULL, /* generic marshaller */
                                                                    G_TYPE_NONE,
                                                                    0);

  g_type_class_add_private (harvester_class,
                            sizeof (RygelMediaExportHarvesterPrivate));
}

static void
rygel_media_export_harvester_init (RygelMediaExportHarvester *self) {
  self->priv = RYGEL_MEDIA_EXPORT_HARVESTER_GET_PRIVATE (self);
}
