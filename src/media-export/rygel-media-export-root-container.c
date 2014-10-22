/*
 * Copyright (C) 2009, 2010 Jens Georg <mail@jensge.org>.
 * Copyright (C) 2013 Intel Corporation.
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

#include "rygel-media-export-errors.h"
#include "rygel-media-export-harvester.h"
#include "rygel-media-export-null-container.h"
#include "rygel-media-export-query-container-factory.h"
#include "rygel-media-export-query-container.h"
#include "rygel-media-export-root-container.h"
#include "rygel-media-export-string-utils.h"

/**
 * Represents the root container.
 */

static void
rygel_media_export_root_container_rygel_searchable_container_interface_init (RygelSearchableContainerIface *iface);

static void
rygel_media_export_root_container_rygel_trackable_container_interface_init (RygelTrackableContainerIface *iface);

G_DEFINE_TYPE_WITH_CODE (RygelMediaExportRootContainer,
                         rygel_media_export_root_container,
                         RYGEL_MEDIA_EXPORT_TYPE_TRACKABLE_DB_CONTAINER,
                         G_IMPLEMENT_INTERFACE (RYGEL_TYPE_SEARCHABLE_CONTAINER,
                                                rygel_media_export_root_container_rygel_searchable_container_interface_init)
                         G_IMPLEMENT_INTERFACE (RYGEL_TYPE_TRACKABLE_CONTAINER,
                                                rygel_media_export_root_container_rygel_trackable_container_interface_init))

typedef struct _RygelMediaExportFolderDefinition RygelMediaExportFolderDefinition;
typedef struct _RygelMediaExportRootContainerFindObjectData RygelMediaExportRootContainerFindObjectData;
typedef struct _RygelMediaExportRootContainerSearchData RygelMediaExportRootContainerSearchData;

struct _RygelMediaExportFolderDefinition {
  gchar *title;
  gchar *definition;
};

struct _RygelMediaExportRootContainerPrivate {
  RygelMediaExportHarvester *harvester;
  GCancellable *cancellable;
  RygelMediaContainer *filesystem_container;
  gulong harvester_signal_id;
  gboolean initialized;
};

struct _RygelMediaExportRootContainerFindObjectData {
  GSimpleAsyncResult *simple;
  gchar *id;
};

struct _RygelMediaExportRootContainerSearchData {
  GSimpleAsyncResult *simple;
  guint total_matches;
  RygelMediaObjects *result;
  RygelMediaContainer *query_container;
  gchar *upnp_class;
};


static RygelMediaContainer *rygel_media_export_root_container_instance = NULL;
static RygelSearchableContainerIface *parent_searchable_container_iface = NULL;
static RygelTrackableContainerIface *parent_trackable_container_iface = NULL;

#define RYGEL_MEDIA_EXPORT_ROOT_CONTAINER_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                RYGEL_MEDIA_EXPORT_TYPE_ROOT_CONTAINER, \
                                RygelMediaExportRootContainerPrivate))

#define RYGEL_MEDIA_EXPORT_ROOT_CONTAINER_SEARCH_CONTAINER_PREFIX RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_PREFIX "upnp:class," RYGEL_MUSIC_ITEM_UPNP_CLASS ","

const RygelMediaExportFolderDefinition VIRTUAL_FOLDERS_DEFAULT[] = {{"Year", "dc:date,?"}, {"All", ""}};
const RygelMediaExportFolderDefinition VIRTUAL_FOLDERS_MUSIC[] = {{"Artist", "upnp:artist,?,upnp:album,?"}, {"Album", "upnp:album,?"}, {"Genre", "dc:genre,?"}};

static RygelMediaExportRootContainer *
rygel_media_export_root_container_new (void) {
  return RYGEL_MEDIA_EXPORT_ROOT_CONTAINER (g_object_new (RYGEL_MEDIA_EXPORT_TYPE_ROOT_CONTAINER,
                                                          "id", "0",
                                                          "parent", NULL,
                                                          "title", _("@REALNAME@'s media"),
                                                          "child-count", 0,
                                                          NULL));
}

gboolean
rygel_media_export_root_container_ensure_exists (GError **error) {
  if (!rygel_media_export_root_container_instance) {
    GError *inner_error = NULL;

    rygel_media_export_media_cache_ensure_exists (&inner_error);
    if (inner_error) {
      g_propagate_error (error, inner_error);
      return FALSE;
    }
    rygel_media_export_root_container_instance = RYGEL_MEDIA_CONTAINER (rygel_media_export_root_container_new ());
  }

  return TRUE;
}

RygelMediaContainer *
rygel_media_export_root_container_get_instance (void) {
  if (rygel_media_export_root_container_instance) {
    return g_object_ref (rygel_media_export_root_container_instance);
  }

  return NULL;
}

RygelMediaContainer *
rygel_media_export_root_container_get_filesystem_container (RygelMediaExportRootContainer *self) {
  RygelMediaContainer *container;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_ROOT_CONTAINER (self), NULL);

  container = self->priv->filesystem_container;
  if (container) {
    g_object_ref (container);
  }

  return container;
}

void
rygel_media_export_root_container_shutdown (RygelMediaExportRootContainer* self) {
  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_ROOT_CONTAINER (self));

  g_cancellable_cancel (self->priv->cancellable);
}

static void
rygel_media_export_root_container_real_find_object_data_free (RygelMediaExportRootContainerFindObjectData *data) {
  g_free (data->id);
  g_slice_free (RygelMediaExportRootContainerFindObjectData, data);
}

static void
rygel_media_export_root_container_find_object_ready (GObject      *source_object,
                                                     GAsyncResult *res,
                                                     gpointer      user_data) {
  GError *error = NULL;
  RygelMediaExportRootContainerFindObjectData *data = (RygelMediaExportRootContainerFindObjectData *) user_data;
  RygelMediaObject *object = RYGEL_MEDIA_CONTAINER_CLASS (rygel_media_export_root_container_parent_class)->find_object_finish (RYGEL_MEDIA_CONTAINER (source_object),
                                                                                                                               res,
                                                                                                                               &error);

  if (error) {
    g_simple_async_result_take_error (data->simple, error);
  } else {
    if (!object &&
        g_str_has_prefix (data->id, RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_PREFIX)) {
      RygelMediaExportQueryContainerFactory *factory = rygel_media_export_query_container_factory_get_default ();
      RygelMediaExportQueryContainer *container = rygel_media_export_query_container_factory_create_from_id (factory,
                                                                                                             data->id,
                                                                                                             "");

      if (container) {
        object = RYGEL_MEDIA_OBJECT (container);
        rygel_media_object_set_parent (object, RYGEL_MEDIA_CONTAINER (source_object));
      }
      g_object_unref (factory);
    }
    if (object) {
      g_simple_async_result_set_op_res_gpointer (data->simple,
                                                 object,
                                                 g_object_unref);
    }
  }
  g_simple_async_result_complete (data->simple);
  g_object_unref (data->simple);
  rygel_media_export_root_container_real_find_object_data_free (data);
}

static void
rygel_media_export_root_container_real_find_object (RygelMediaContainer *base,
                                                    const gchar         *id,
                                                    GCancellable        *cancellable,
                                                    GAsyncReadyCallback  callback,
                                                    gpointer             user_data) {
  RygelMediaExportRootContainerFindObjectData *data = g_slice_new0 (RygelMediaExportRootContainerFindObjectData);

  data->simple = g_simple_async_result_new (G_OBJECT (base),
                                            callback,
                                            user_data,
                                            rygel_media_export_root_container_real_find_object);
  data->id = g_strdup (id);
  RYGEL_MEDIA_CONTAINER_CLASS (rygel_media_export_root_container_parent_class)->find_object (base,
                                                                                             id,
                                                                                             cancellable,
                                                                                             rygel_media_export_root_container_find_object_ready,
                                                                                             data);
}

static RygelMediaObject *
rygel_media_export_root_container_real_find_object_finish (RygelMediaContainer  *base G_GNUC_UNUSED,
                                                           GAsyncResult         *res,
                                                           GError              **error) {
  RygelMediaObject* result;
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);

  if (g_simple_async_result_propagate_error (simple, error)) {
    return NULL;
  }

  result = RYGEL_MEDIA_OBJECT (g_simple_async_result_get_op_res_gpointer (simple));
  if (result) {
    g_object_ref (result);
  }

  return result;
}

static void
rygel_media_export_root_container_real_search_data_free (gpointer user_data) {
  RygelMediaExportRootContainerSearchData *data = (RygelMediaExportRootContainerSearchData *) user_data;
  if (data->result) {
    g_object_unref (data->result);
  }
  if (data->query_container) {
    g_object_unref (data->query_container);
  }
  g_free (data->upnp_class);
  g_slice_free (RygelMediaExportRootContainerSearchData, data);
}

static void
rygel_media_export_root_container_base_class_search_ready  (GObject      *source_object,
                                                            GAsyncResult *res,
                                                            gpointer      user_data) {
  GError *error = NULL;
  RygelMediaExportRootContainerSearchData *data = (RygelMediaExportRootContainerSearchData *) user_data;
  guint total_matches = 0;
  RygelMediaObjects *objects = parent_searchable_container_iface->search_finish (RYGEL_SEARCHABLE_CONTAINER (source_object),
                                                                                 res,
                                                                                 &total_matches,
                                                                                 &error);

  if (error) {
    g_simple_async_result_take_error (data->simple, error);
  } else {
    data->result = objects;
    data->total_matches = total_matches;
  }
  g_simple_async_result_complete (data->simple);
  g_object_unref (data->simple);
}

static void
rygel_media_export_root_container_get_children_from_container_ready (GObject      *source_object G_GNUC_UNUSED,
                                                                     GAsyncResult *res,
                                                                     gpointer      user_data) {
  GError *error = NULL;
  RygelMediaExportRootContainerSearchData *data = (RygelMediaExportRootContainerSearchData *) user_data;
  guint total_matches = 0;
  RygelMediaObjects *objects = rygel_media_container_get_children_finish (data->query_container,
                                                                          res,
                                                                          &error);

  if (error) {
    g_simple_async_result_take_error (data->simple, error);
  } else {
    total_matches = rygel_media_container_get_child_count (data->query_container);
    if (data->upnp_class) {
      gint size = gee_abstract_collection_get_size (GEE_ABSTRACT_COLLECTION (objects));
      GeeAbstractList *list = GEE_ABSTRACT_LIST (objects);
      gint iter;

      for (iter = 0; iter < size; ++iter) {
        RygelMediaObject *object = RYGEL_MEDIA_OBJECT (gee_abstract_list_get (list, iter));

        if (object) {
          rygel_media_object_set_upnp_class (object, data->upnp_class);
          g_object_unref (object);
        }
      }
    }

    data->result = objects;
    data->total_matches = total_matches;
  }
  g_simple_async_result_complete (data->simple);
  g_object_unref (data->simple);
  g_free (data->upnp_class);
  data->upnp_class = NULL;
  g_object_unref (data->query_container);
  data->query_container = NULL;
}

static RygelMediaExportQueryContainer *
rygel_media_export_root_container_search_to_virtual_container (RygelMediaExportRootContainer *self,
                                                               RygelRelationalExpression     *expression) {
  RygelSearchExpression *search_expression;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_ROOT_CONTAINER (self), NULL);
  g_return_val_if_fail (RYGEL_IS_RELATIONAL_EXPRESSION (expression), NULL);

  search_expression = RYGEL_SEARCH_EXPRESSION (expression);

  if (!g_strcmp0 (search_expression->operand1, "upnp:class") &&
      ((GUPnPSearchCriteriaOp) ((gintptr) search_expression->op)) == GUPNP_SEARCH_CRITERIA_OP_EQ) {
    gchar *id;
    RygelMediaExportQueryContainerFactory *factory;
    RygelMediaExportQueryContainer *container;

    if (!g_strcmp0 (search_expression->operand2, "object.container.album.musicAlbum")) {
      id = g_strdup_printf ("%supnp:album,?", RYGEL_MEDIA_EXPORT_ROOT_CONTAINER_SEARCH_CONTAINER_PREFIX);
    } else if (!g_strcmp0 (search_expression->operand2, "object.container.person.musicArtist")) {
      id = g_strdup_printf ("%sdc:creator,?,upnp:album,?", RYGEL_MEDIA_EXPORT_ROOT_CONTAINER_SEARCH_CONTAINER_PREFIX);
    } else if (!g_strcmp0 (search_expression->operand2, "object.container.genre.musicGenre")) {
      id = g_strdup_printf ("%sdc:genre,?", RYGEL_MEDIA_EXPORT_ROOT_CONTAINER_SEARCH_CONTAINER_PREFIX);
    } else {
      return NULL;
    }

    factory = rygel_media_export_query_container_factory_get_default ();
    container = rygel_media_export_query_container_factory_create_from_description (factory, id, "");
    g_object_unref (factory);
    g_free (id);

    return container;
  }

  return NULL;
}

/**
 * Check if a passed search expression is a simple search in a virtual
 * container.
 *
 * @param expression the expression to check
 * @param new_id contains the id of the virtual container constructed from
 *               the search
 * @param upnp_class contains the class of the container the search was
 *                   looking in
 * @return true if it was a search in virtual container, false otherwise.
 * @note This works single level only. Enough to satisfy Xbox music
 *       browsing, but may need refinement
 */
static gboolean
rygel_media_export_root_container_is_search_in_virtual_container (RygelMediaExportRootContainer  *self,
                                                                  RygelSearchExpression          *expression,
                                                                  RygelMediaContainer           **container) {
  RygelMediaExportQueryContainer *query_container;
  RygelSearchExpression *virtual_expression;
  RygelMediaExportQueryContainerFactory *factory;
  RygelMediaContainer *temp_container;
  gchar *plaintext_id;
  const gchar *id;
  gchar *last_argument;
  gchar *escaped_detail;
  gchar *new_id;
  GError *error;
  gboolean result;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_ROOT_CONTAINER (self), FALSE);
  g_return_val_if_fail (RYGEL_IS_SEARCH_EXPRESSION (expression), FALSE);

  if (!RYGEL_IS_LOGICAL_EXPRESSION (expression)) {
    return FALSE;
  }

  if (!(RYGEL_IS_RELATIONAL_EXPRESSION (expression->operand1) &&
        RYGEL_IS_RELATIONAL_EXPRESSION (expression->operand2) &&
        (expression->op == RYGEL_LOGICAL_OPERATOR_AND))) {
    return FALSE;
  }

  query_container = rygel_media_export_root_container_search_to_virtual_container (self,
                                                                                   RYGEL_RELATIONAL_EXPRESSION (expression->operand1));
  if (!query_container) {
    query_container = rygel_media_export_root_container_search_to_virtual_container (self,
                                                                                     RYGEL_RELATIONAL_EXPRESSION (expression->operand2));
    if (query_container) {
      virtual_expression = expression->operand1;
    } else {
      return FALSE;
    }
  } else {
    virtual_expression = expression->operand2;
  }

  error = NULL;
  factory = rygel_media_export_query_container_factory_get_default ();
  id = rygel_media_object_get_id (RYGEL_MEDIA_OBJECT (query_container));
  plaintext_id = rygel_media_export_query_container_factory_get_virtual_container_definition (factory, id);
  g_object_unref (query_container);
  last_argument = rygel_media_export_string_replace (plaintext_id,
                                                     RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_PREFIX,
                                                     "",
                                                     &error);
  if (error) {
    g_warning ("Could not remove %s from %s: %s",
               RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_PREFIX,
               plaintext_id,
               error->message);
    g_error_free (error);
    result = FALSE;
    goto error_out;
  }
  escaped_detail = g_uri_escape_string (virtual_expression->operand2, "", TRUE);
  new_id = g_strdup_printf ("%s%s,%s,%s",
                            RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_PREFIX,
                            (const gchar *) virtual_expression->operand1,
                            escaped_detail,
                            last_argument);
  temp_container = RYGEL_MEDIA_CONTAINER (rygel_media_export_query_container_factory_create_from_description (factory,
                                                                                                              new_id,
                                                                                                              ""));
  if (container) {
    *container = temp_container;
  } else if (temp_container) {
    g_object_unref (temp_container);
  }
  g_free (new_id);
  g_free (escaped_detail);
  g_free (last_argument);

  result = TRUE;
 error_out:
  g_free (plaintext_id);
  g_object_unref (factory);

  return result;
}

static void
rygel_media_export_root_container_real_search (RygelSearchableContainer *base,
                                               RygelSearchExpression    *expression,
                                               guint                     offset,
                                               guint                     max_count,
                                               const gchar              *sort_criteria,
                                               GCancellable             *cancellable,
                                               GAsyncReadyCallback       callback,
                                               gpointer                  user_data) {
  RygelMediaExportRootContainer *self = RYGEL_MEDIA_EXPORT_ROOT_CONTAINER (base);
  RygelMediaExportRootContainerSearchData *data = g_slice_new0 (RygelMediaExportRootContainerSearchData);

  data->simple = g_simple_async_result_new (G_OBJECT (self),
                                            callback,
                                            user_data,
                                            rygel_media_export_root_container_real_search);
  g_simple_async_result_set_op_res_gpointer (data->simple,
                                             data,
                                             rygel_media_export_root_container_real_search_data_free);
  if (!expression) {
    parent_searchable_container_iface->search (base,
                                               expression,
                                               offset,
                                               max_count,
                                               sort_criteria,
                                               cancellable,
                                               rygel_media_export_root_container_base_class_search_ready,
                                               data);
  } else {
    data->query_container = NULL;
    data->upnp_class = NULL;
    if (RYGEL_IS_RELATIONAL_EXPRESSION (expression)) {
      data->query_container = RYGEL_MEDIA_CONTAINER (rygel_media_export_root_container_search_to_virtual_container (self,
                                                                                                                    RYGEL_RELATIONAL_EXPRESSION (expression)));
      data->upnp_class = g_strdup (expression->operand2);
    } else {
      rygel_media_export_root_container_is_search_in_virtual_container (self,
                                                                        expression,
                                                                        &data->query_container);
    }
    if (data->query_container) {
      rygel_media_container_get_children (data->query_container,
                                          offset,
                                          max_count,
                                          sort_criteria,
                                          cancellable,
                                          rygel_media_export_root_container_get_children_from_container_ready,
                                          data);
    } else {
      parent_searchable_container_iface->search (base,
                                                 expression,
                                                 offset,
                                                 max_count,
                                                 sort_criteria,
                                                 cancellable,
                                                 rygel_media_export_root_container_base_class_search_ready,
                                                 data);
    }
  }
}

static RygelMediaObjects *
rygel_media_export_root_container_real_search_finish (RygelSearchableContainer  *base G_GNUC_UNUSED,
                                                      GAsyncResult              *res,
                                                      guint                     *total_matches,
                                                      GError                   **error) {
  RygelMediaObjects *result;
  RygelMediaExportRootContainerSearchData *data;
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);

  if (g_simple_async_result_propagate_error (simple, error)) {
    return NULL;
  }
  data = g_simple_async_result_get_op_res_gpointer (simple);
  if (total_matches) {
    *total_matches = data->total_matches;
  }
  result = data->result;
  data->result = NULL;
  return result;
}

static void
rygel_media_export_root_container_updated (RygelMediaExportRootContainer *self) {
  RygelMediaContainer *media_container = RYGEL_MEDIA_CONTAINER (self);
  RygelMediaExportMediaCache *media_db = rygel_media_export_db_container_get_media_db (RYGEL_MEDIA_EXPORT_DB_CONTAINER (self));

  rygel_media_container_updated (media_container,
                                 NULL,
                                 RYGEL_OBJECT_EVENT_TYPE_MODIFIED,
                                 FALSE);
  rygel_media_export_media_cache_save_container (media_db, media_container, NULL);
}

static GeeArrayList *
rygel_media_export_root_container_get_shared_uris (RygelMediaExportRootContainer *self);

static void
rygel_media_export_root_container_on_initial_harvesting_done_rygel_media_export_harvester_done (RygelMediaExportHarvester *sender G_GNUC_UNUSED,
                                                                                                gpointer                   self);

static gboolean
rygel_media_export_root_container_late_init (gpointer user_data) {
  RygelMediaExportRootContainer *self = RYGEL_MEDIA_EXPORT_ROOT_CONTAINER (user_data);
  RygelMediaExportRootContainerPrivate *priv = self->priv;
  RygelMediaExportMediaCache* db;
  GError *error = NULL;
  GeeArrayList* ids;
  GeeAbstractCollection *abstract_ids;
  GeeAbstractList *abstract_list_ids;
  RygelMediaContainer *media_container = RYGEL_MEDIA_CONTAINER (self);
  GeeArrayList *shared_uris;
  GeeArrayList *locations;
  GeeAbstractList *abstract_locations;
  gint iter;
  gint size;

  priv->cancellable = g_cancellable_new ();
  db = rygel_media_export_db_container_get_media_db (RYGEL_MEDIA_EXPORT_DB_CONTAINER (self));
  rygel_media_export_media_cache_save_container (db, media_container, &error);
  if (error) {
    g_warning ("Failed to save root container into db: %s",
               error->message);
    g_error_free (error);
    error = NULL;
  }

  priv->filesystem_container = RYGEL_MEDIA_CONTAINER (rygel_media_export_trackable_db_container_new (RYGEL_MEDIA_EXPORT_ROOT_CONTAINER_FILESYSTEM_FOLDER_ID,
                                                                                           _(RYGEL_MEDIA_EXPORT_ROOT_CONTAINER_FILESYSTEM_FOLDER_NAME)));
  rygel_media_object_set_parent (RYGEL_MEDIA_OBJECT (priv->filesystem_container),
                                 media_container);
  rygel_media_export_media_cache_save_container (db, RYGEL_MEDIA_CONTAINER (priv->filesystem_container), &error);
  if (error) {
    g_warning ("Failed to save filesystem container into db: %s",
               error->message);
    g_error_free (error);
    error = NULL;
  }

  ids = rygel_media_export_media_cache_get_child_ids (db, RYGEL_MEDIA_EXPORT_ROOT_CONTAINER_FILESYSTEM_FOLDER_ID, &error);
  if (error) {
    g_warning ("Failed to get child ids of %s from db: %s",
               RYGEL_MEDIA_EXPORT_ROOT_CONTAINER_FILESYSTEM_FOLDER_ID,
               error->message);
    g_error_free (error);
    error = NULL;
    ids = gee_array_list_new (G_TYPE_STRING, (GBoxedCopyFunc) g_strdup, g_free, NULL, NULL, NULL);
  }
  abstract_ids = GEE_ABSTRACT_COLLECTION (ids);

  shared_uris = rygel_media_export_root_container_get_shared_uris (self);
  priv->harvester = rygel_media_export_harvester_new (priv->cancellable, shared_uris);
  if (shared_uris) {
    g_object_unref (shared_uris);
  }
  priv->harvester_signal_id = g_signal_connect_object (priv->harvester,
                                                       "done",
                                                       G_CALLBACK (rygel_media_export_root_container_on_initial_harvesting_done_rygel_media_export_harvester_done),
                                                       self,
                                                       0);

  locations = rygel_media_export_harvester_get_locations (priv->harvester);
  abstract_locations = GEE_ABSTRACT_LIST (locations);
  size = gee_abstract_collection_get_size (GEE_ABSTRACT_COLLECTION (locations));

  for (iter = 0; iter < size; ++iter) {
    GFile *file = gee_abstract_list_get (abstract_locations, iter);
    gchar *file_id = rygel_media_export_media_cache_get_id (file);

    gee_abstract_collection_remove (abstract_ids, file_id);
    rygel_media_export_harvester_schedule (priv->harvester,
                                           file,
                                           priv->filesystem_container,
                                           NULL);
    g_free (file_id);
    g_object_unref (file);
  }

  size = gee_abstract_collection_get_size (abstract_ids);
  abstract_list_ids = GEE_ABSTRACT_LIST (ids);
  for (iter = 0; iter < size; ++iter) {
    gchar *id = gee_abstract_list_get (abstract_list_ids, iter);

    g_debug ("ID %s no longer in config; deleting...", id);
    rygel_media_export_media_cache_remove_by_id (db, id, &error);
    if (error) {
      g_warning ("Failed to remove entry %s: %s",
                 id,
                 error->message);
      g_error_free (error);
      error = NULL;
    }
    g_free (id);
  }
  if (!gee_collection_get_is_empty (GEE_COLLECTION (ids))) {
    rygel_media_export_root_container_updated (self);
  }
  g_object_unref (ids);

  return FALSE;
}

static guint32
rygel_media_export_root_container_real_get_system_update_id (RygelTrackableContainer *base) {
  guint32 id = parent_trackable_container_iface->get_system_update_id (base);
  RygelMediaExportRootContainer *self = RYGEL_MEDIA_EXPORT_ROOT_CONTAINER (base);

  if (!self->priv->initialized) {
    self->priv->initialized = TRUE;
    g_idle_add (rygel_media_export_root_container_late_init, self);
  }

  return id;
}

static void
replace_in_string (gchar       **self,
                   const gchar  *old_part,
                   const gchar  *new_part)
{
  g_return_if_fail (self != NULL && *self != NULL);
  g_return_if_fail (old_part != NULL);

  if (G_LIKELY (new_part)) {
    gchar *temp_uri = *self;
    GError *error = NULL;

    *self = rygel_media_export_string_replace (temp_uri, old_part, new_part, &error);
    if (error) {
      *self = temp_uri;
      g_warning ("Could not replace %s in %s with %s: %s",
                 old_part,
                 *self,
                 new_part,
                 error->message);
      g_error_free (error);
    } else {
      g_free (temp_uri);
    }
  }
}

static GeeArrayList *
rygel_media_export_root_container_get_shared_uris (RygelMediaExportRootContainer *self) {
  GeeArrayList* uris;
  GeeArrayList* actual_uris;
  RygelConfiguration* config;
  GFile* home_dir;
  const gchar* pictures_dir;
  const gchar* videos_dir;
  const gchar* music_dir;
  GError *error;
  gint iter;
  gint size;
  GeeAbstractList *abstract_uris;
  GeeAbstractCollection *abstract_actual_uris;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_ROOT_CONTAINER (self), NULL);

  error = NULL;
  config = RYGEL_CONFIGURATION (rygel_meta_config_get_default ());
  uris = rygel_configuration_get_string_list (config,
                                              "MediaExport",
                                              "uris",
                                              &error);
  if (error) {
    g_error_free (error);
    error = NULL;
    uris = gee_array_list_new (G_TYPE_STRING, (GBoxedCopyFunc) g_strdup, g_free, NULL, NULL, NULL);
  }
  actual_uris = gee_array_list_new (G_TYPE_FILE, g_object_ref, g_object_unref, NULL, NULL, NULL);
  home_dir = g_file_new_for_path (g_get_home_dir ());
  pictures_dir = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
  videos_dir = g_get_user_special_dir (G_USER_DIRECTORY_VIDEOS);
  music_dir = g_get_user_special_dir (G_USER_DIRECTORY_MUSIC);
  size = gee_abstract_collection_get_size (GEE_ABSTRACT_COLLECTION (uris));
  abstract_uris = GEE_ABSTRACT_LIST (uris);
  abstract_actual_uris = GEE_ABSTRACT_COLLECTION (actual_uris);

  for (iter = 0; iter < size; ++iter) {
    gchar *uri = gee_abstract_list_get (abstract_uris, iter);
    GFile *file = g_file_new_for_commandline_arg (uri);
    gboolean add = TRUE;

    if (G_LIKELY (!g_file_equal (file, home_dir))) {
      gchar *actual_uri = g_strdup (uri);

      replace_in_string (&actual_uri, "@PICTURES@", pictures_dir);
      replace_in_string (&actual_uri, "@VIDEOS@", videos_dir);
      replace_in_string (&actual_uri, "@MUSIC@", music_dir);

      g_object_unref (file);
      file = g_file_new_for_commandline_arg (actual_uri);
      if (g_file_equal (file, home_dir)) {
        add = FALSE;
      }
      g_free (actual_uri);
    }

    if (add) {
      gee_abstract_collection_add (abstract_actual_uris, file);
    }

    g_object_unref (file);
    g_free (uri);
  }
  g_object_unref (home_dir);
  g_object_unref (config);
  g_object_unref (uris);

  return actual_uris;
}

static void
rygel_media_export_root_container_add_folder_definition (RygelMediaExportRootContainer           *self,
                                                         RygelMediaContainer                     *container,
                                                         const gchar                             *item_class,
                                                         const RygelMediaExportFolderDefinition  *definition,
                                                         GError                                 **error) {
  gchar *id;
  RygelMediaExportQueryContainerFactory *factory;
  RygelMediaContainer *query_container;
  RygelMediaExportMediaCache *db;
  GError *inner_error;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_ROOT_CONTAINER (self));
  g_return_if_fail (RYGEL_IS_MEDIA_CONTAINER (container));
  g_return_if_fail (item_class != NULL);
  g_return_if_fail (definition != NULL);

  id = g_strdup_printf ("%supnp:class,%s,%s", RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_PREFIX, item_class, definition->definition);
  if (g_str_has_suffix (id, ",")) {
    gchar *tmp = id;

    id = rygel_media_export_string_slice (tmp, 0, -1);
    if (!id) {
      g_warning ("Failed to remove the last character from '%s'.",
                 tmp);
    }
    g_free (tmp);
    if (!id) {
      return;
    }
  }
  factory = rygel_media_export_query_container_factory_get_default ();
  query_container = RYGEL_MEDIA_CONTAINER (rygel_media_export_query_container_factory_create_from_description (factory,
                                                                                                               id,
                                                                                                               _(definition->title)));
  db = rygel_media_export_db_container_get_media_db (RYGEL_MEDIA_EXPORT_DB_CONTAINER (self));
  inner_error = NULL;
  if (rygel_media_container_get_child_count (query_container) > 0) {
    rygel_media_object_set_parent (RYGEL_MEDIA_OBJECT (query_container), container);
    rygel_media_export_media_cache_save_container (db, query_container, &inner_error);
  } else {
    rygel_media_export_media_cache_remove_by_id (db, id, &inner_error);
  }
  if (inner_error) {
    g_propagate_error (error, inner_error);
  }
  g_object_unref (query_container);
  g_object_unref (factory);
  g_free (id);
}

static void
rygel_media_export_root_container_add_virtual_containers_for_class (RygelMediaExportRootContainer           *self,
                                                                    const gchar                             *parent,
                                                                    const gchar                             *item_class,
                                                                    const RygelMediaExportFolderDefinition  *definitions,
                                                                    guint                                    definitions_length,
                                                                    GError                                 **error) {
  RygelMediaContainer *container;
  RygelMediaObject *object;
  gchar *new_id;
  const gchar *id;
  RygelMediaExportMediaCache *db;
  GError *inner_error;
  gint child_count;
  guint iter;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_ROOT_CONTAINER (self));
  g_return_if_fail (parent != NULL);
  g_return_if_fail (item_class != NULL);

  container = RYGEL_MEDIA_CONTAINER (rygel_media_export_null_container_new ());
  object = RYGEL_MEDIA_OBJECT (container);
  rygel_media_object_set_parent (object, RYGEL_MEDIA_CONTAINER (self));
  rygel_media_object_set_title (object, parent);
  new_id = g_strconcat ("virtual-parent:", item_class, NULL);
  rygel_media_object_set_id (object, new_id);
  g_free (new_id);
  inner_error = NULL;
  db = rygel_media_export_db_container_get_media_db (RYGEL_MEDIA_EXPORT_DB_CONTAINER (self));
  rygel_media_export_media_cache_save_container (db, container, &inner_error);
  if (inner_error) {
    goto out;
  }

  for (iter = 0; iter < G_N_ELEMENTS (VIRTUAL_FOLDERS_DEFAULT); ++iter) {
    rygel_media_export_root_container_add_folder_definition (self, container, item_class, &(VIRTUAL_FOLDERS_DEFAULT[iter]), &inner_error);
    if (inner_error) {
      goto out;
    }
  }

  if (definitions) {
    for (iter = 0; iter < definitions_length; ++iter) {
      rygel_media_export_root_container_add_folder_definition (self, container, item_class, &(definitions[iter]), &inner_error);
      if (inner_error) {
        goto out;
      }
    }
  }

  id = rygel_media_object_get_id (object);
  child_count = rygel_media_export_media_cache_get_child_count (db,
                                                                id,
                                                                &inner_error);
  if (inner_error) {
    goto out;
  }

  if (!child_count) {
    rygel_media_export_media_cache_remove_by_id (db, id, &inner_error);
    if (inner_error) {
      goto out;
    }
  } else {
    rygel_media_container_updated (container, NULL, RYGEL_OBJECT_EVENT_TYPE_MODIFIED, FALSE);
  }

 out:
  if (inner_error) {
    g_propagate_error (error, inner_error);
  }
  g_object_unref (container);
}

static void
rygel_media_export_root_container_add_default_virtual_folders (RygelMediaExportRootContainer *self) {
  GError * error = NULL;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_ROOT_CONTAINER (self));

  rygel_media_export_root_container_add_virtual_containers_for_class (self,
                                                                      _("Music"),
                                                                      RYGEL_MUSIC_ITEM_UPNP_CLASS,
                                                                      VIRTUAL_FOLDERS_MUSIC,
                                                                      G_N_ELEMENTS (VIRTUAL_FOLDERS_MUSIC),
                                                                      &error);
  if (error) {
    goto out;
  }
  rygel_media_export_root_container_add_virtual_containers_for_class (self,
                                                                      _("Pictures"),
                                                                      RYGEL_PHOTO_ITEM_UPNP_CLASS,
                                                                      NULL,
                                                                      0,
                                                                      &error);
  if (error) {
    goto out;
  }
  rygel_media_export_root_container_add_virtual_containers_for_class (self,
                                                                      _("Videos"),
                                                                      RYGEL_VIDEO_ITEM_UPNP_CLASS,
                                                                      NULL,
                                                                      0,
                                                                      &error);
 out:
  if (error) {
    g_error_free (error);
  }
}

static void
on_filesystem_container_updated (RygelMediaContainer *sender G_GNUC_UNUSED,
                                 RygelMediaContainer *container G_GNUC_UNUSED,
                                 RygelMediaObject *object G_GNUC_UNUSED,
                                 RygelObjectEventType event_type G_GNUC_UNUSED,
                                 gboolean sub_tree_update G_GNUC_UNUSED,
                                 gpointer user_data) {
  RygelMediaExportRootContainer *self = RYGEL_MEDIA_EXPORT_ROOT_CONTAINER (user_data);

  rygel_media_export_root_container_add_default_virtual_folders (self);
  rygel_media_export_root_container_updated (self);
}

static void
rygel_media_export_root_container_on_initial_harvesting_done (RygelMediaExportRootContainer *self) {
  g_return_if_fail (self != NULL);

  g_signal_handler_disconnect (G_OBJECT (self->priv->harvester), self->priv->harvester_signal_id);
  self->priv->harvester_signal_id = 0;
  rygel_media_export_media_cache_debug_statistics (rygel_media_export_db_container_get_media_db (RYGEL_MEDIA_EXPORT_DB_CONTAINER (self)));
  rygel_media_export_root_container_add_default_virtual_folders (self);
  rygel_media_export_root_container_updated (self);
  g_signal_connect_object (self->priv->filesystem_container,
                           "container-updated",
                           G_CALLBACK (on_filesystem_container_updated),
                           self,
                           0);
}

static void
rygel_media_export_root_container_on_initial_harvesting_done_rygel_media_export_harvester_done (RygelMediaExportHarvester *sender G_GNUC_UNUSED,
                                                                                                gpointer                   self) {
  rygel_media_export_root_container_on_initial_harvesting_done (self);
}

static void
rygel_media_export_root_container_dispose (GObject *object) {
  RygelMediaExportRootContainer *self = RYGEL_MEDIA_EXPORT_ROOT_CONTAINER (object);
  RygelMediaExportRootContainerPrivate *priv = self->priv;

  if (priv->harvester) {
    RygelMediaExportHarvester *harvester = priv->harvester;

    priv->harvester = NULL;
    g_object_unref (harvester);
  }
  if (priv->cancellable) {
    GCancellable *cancellable = priv->cancellable;

    priv->cancellable = NULL;
    g_object_unref (cancellable);
  }
  if (priv->filesystem_container) {
    RygelMediaContainer *container = priv->filesystem_container;

    priv->filesystem_container = NULL;
    g_object_unref (container);
  }

  G_OBJECT_CLASS (rygel_media_export_root_container_parent_class)->dispose (object);
}

static void
rygel_media_export_root_container_class_init (RygelMediaExportRootContainerClass *root_class) {
  RygelMediaContainerClass *container_class = RYGEL_MEDIA_CONTAINER_CLASS (root_class);
  GObjectClass *object_class = G_OBJECT_CLASS (root_class);

  container_class->find_object = rygel_media_export_root_container_real_find_object;
  container_class->find_object_finish = rygel_media_export_root_container_real_find_object_finish;
  object_class->dispose = rygel_media_export_root_container_dispose;

  g_type_class_add_private (root_class, sizeof (RygelMediaExportRootContainerPrivate));
}

static void
rygel_media_export_root_container_init (RygelMediaExportRootContainer *self) {
  self->priv = RYGEL_MEDIA_EXPORT_ROOT_CONTAINER_GET_PRIVATE (self);
}

static void
rygel_media_export_root_container_rygel_searchable_container_interface_init (RygelSearchableContainerIface *iface) {
  parent_searchable_container_iface = g_type_interface_peek_parent (iface);
  iface->search = rygel_media_export_root_container_real_search;
  iface->search_finish = rygel_media_export_root_container_real_search_finish;
  /* iface should be a copy of parent_searchable_container_iface, so
   * we don't have to override the {get,set}_search_classes funcs.
   *
   * iface->get_search_classes = parent_searchable_container_iface->get_search_classes;
   * iface->set_search_classes = parent_searchable_container_iface->set_search_classes;
   */
}

static void
rygel_media_export_root_container_rygel_trackable_container_interface_init (RygelTrackableContainerIface *iface)
{
  parent_trackable_container_iface = g_type_interface_peek_parent (iface);
  iface->get_system_update_id = rygel_media_export_root_container_real_get_system_update_id;
  /* iface should be a copy of parent_trackable_container_iface, so
   * we don't have to override the rest of functions.
   *
   * iface->add_child = parent_trackable_container_iface->add_child;
   * iface->add_child_finish = parent_trackable_container_iface->add_child_finish;
   * iface->remove_child = parent_trackable_container_iface->remove_child;
   * iface->remove_child_finish = parent_trackable_container_iface->remove_child_finish;
   * iface->get_service_reset_token = parent_trackable_container_iface->get_service_reset_token;
   * iface->set_service_reset_token = parent_trackable_container_iface->set_service_reset_token;
   */
}
