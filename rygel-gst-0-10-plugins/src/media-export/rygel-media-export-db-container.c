/*
 * Copyright (C) 2009 Jens Georg <mail@jensge.org>.
 * Copyright (C) 2012, 2013 Intel Corporation.
 *
 * Author: Jens Georg <mail@jensge.org>
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

#include "rygel-media-export-db-container.h"
#include "rygel-media-export-errors.h"

static void
rygel_media_export_db_container_rygel_searchable_container_interface_init (RygelSearchableContainerIface *iface);

G_DEFINE_TYPE_WITH_CODE (RygelMediaExportDBContainer,
                         rygel_media_export_db_container,
                         RYGEL_TYPE_MEDIA_CONTAINER,
                         G_IMPLEMENT_INTERFACE (RYGEL_TYPE_SEARCHABLE_CONTAINER,
                                                rygel_media_export_db_container_rygel_searchable_container_interface_init))

typedef struct _RygelMediaExportDBContainerSearchData RygelMediaExportDBContainerSearchData;

struct _RygelMediaExportDBContainerPrivate {
  GeeArrayList *search_classes;
  RygelMediaExportMediaCache *media_db;
};

struct _RygelMediaExportDBContainerSearchData {
  RygelMediaObjects *result;
  guint total_matches;
  GSimpleAsyncResult *async_result;
};

#define RYGEL_MEDIA_EXPORT_DB_CONTAINER_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                RYGEL_MEDIA_EXPORT_TYPE_DB_CONTAINER, \
                                RygelMediaExportDBContainerPrivate))

enum  {
  RYGEL_MEDIA_EXPORT_DB_CONTAINER_DUMMY_PROPERTY,
  RYGEL_MEDIA_EXPORT_DB_CONTAINER_SEARCH_CLASSES,
  RYGEL_MEDIA_EXPORT_DB_CONTAINER_MEDIA_DB
};

static gint
rygel_media_export_db_container_real_count_children (RygelMediaExportDBContainer* self) {
  const gchar *id = rygel_media_object_get_id (RYGEL_MEDIA_OBJECT (self));
  GError *inner_error = NULL;
  gint count = rygel_media_export_media_cache_get_child_count (self->priv->media_db,
                                                               id,
                                                               &inner_error);

  if (inner_error) {
    g_debug ("Could not get child count from database: %s", inner_error->message);
    count = 0;
    g_error_free (inner_error);
  }
  return count;
}

static void
on_media_container_updated (RygelMediaContainer  *sender G_GNUC_UNUSED,
                            RygelMediaContainer  *container G_GNUC_UNUSED,
                            RygelMediaObject     *object G_GNUC_UNUSED,
                            RygelObjectEventType  event_type G_GNUC_UNUSED,
                            gboolean              sub_tree_update G_GNUC_UNUSED,
                            gpointer              user_data) {
  RygelMediaExportDBContainer *self = RYGEL_MEDIA_EXPORT_DB_CONTAINER (user_data);

  rygel_media_export_db_container_count_children (self);
}

static void
rygel_media_export_db_container_constructed (GObject *object)
{
  RygelMediaExportMediaCache *cache = rygel_media_export_media_cache_get_default ();
  RygelMediaExportDBContainer *self = RYGEL_MEDIA_EXPORT_DB_CONTAINER (object);
  GeeArrayList *classes = gee_array_list_new (G_TYPE_STRING,
                                              (GBoxedCopyFunc) g_strdup,
                                              g_free,
                                              NULL,
                                              NULL,
                                              NULL);

  G_OBJECT_CLASS (rygel_media_export_db_container_parent_class)->constructed (object);

  rygel_searchable_container_set_search_classes (RYGEL_SEARCHABLE_CONTAINER (self),
                                                 classes);
  g_object_unref (classes);
  self->priv->media_db = cache;
  g_signal_connect_object (self,
                           "container-updated",
                           G_CALLBACK (on_media_container_updated),
                           self,
                           0);
  rygel_media_container_set_child_count (RYGEL_MEDIA_CONTAINER (self),
                                         rygel_media_export_db_container_count_children (self));
}

RygelMediaExportDBContainer *
rygel_media_export_db_container_new (const gchar *id,
                                     const gchar *title) {
  return RYGEL_MEDIA_EXPORT_DB_CONTAINER (g_object_new (RYGEL_MEDIA_EXPORT_TYPE_DB_CONTAINER,
                                                        "id", id,
                                                        "parent", NULL,
                                                        "title", title,
                                                        "child-count", 0,
                                                        NULL));
}

static void
rygel_media_export_db_container_real_get_children (RygelMediaContainer *base,
                                                   guint                offset,
                                                   guint                max_count,
                                                   const gchar         *sort_criteria,
                                                   GCancellable        *cancellable G_GNUC_UNUSED,
                                                   GAsyncReadyCallback  callback,
                                                   gpointer             user_data) {
  RygelMediaExportDBContainer *self = RYGEL_MEDIA_EXPORT_DB_CONTAINER (base);
  GError *inner_error = NULL;
  RygelMediaObjects *objects = rygel_media_export_media_cache_get_children (self->priv->media_db,
                                                                            base,
                                                                            sort_criteria,
                                                                            offset,
                                                                            max_count,
                                                                            &inner_error);
  GSimpleAsyncResult *simple;

  if (inner_error) {
    simple = g_simple_async_result_new_take_error (G_OBJECT (self),
                                                   callback,
                                                   user_data,
                                                   inner_error);
  } else {
    simple = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        rygel_media_export_db_container_real_get_children);
    g_simple_async_result_set_op_res_gpointer (simple,
                                               objects,
                                               g_object_unref);

  }
  g_simple_async_result_complete_in_idle (simple);
  g_object_unref (simple);
}

static RygelMediaObjects *
rygel_media_export_db_container_real_get_children_finish (RygelMediaContainer  *base G_GNUC_UNUSED,
                                                          GAsyncResult         *res,
                                                          GError              **error) {
  RygelMediaObjects *result;
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);

  if (g_simple_async_result_propagate_error (simple, error)) {
    return NULL;
  }

  result = RYGEL_MEDIA_OBJECTS (g_simple_async_result_get_op_res_gpointer (simple));

  if (result) {
    g_object_ref (result);
  }

  return result;
}

static void
rygel_media_export_db_container_real_search_data_free (gpointer user_data) {
  RygelMediaExportDBContainerSearchData *data = (RygelMediaExportDBContainerSearchData *) user_data;
  if (!data) {
    return;
  }

  if (data->result) {
    g_object_unref (data->result);
  }
  g_slice_free (RygelMediaExportDBContainerSearchData, data);
}

static void
rygel_media_export_db_container_search_ready (GObject      *source_object,
                                              GAsyncResult *res,
                                              gpointer      user_data) {
  RygelMediaExportDBContainerSearchData *data = (RygelMediaExportDBContainerSearchData *) user_data;
  RygelSearchableContainer *searchable = RYGEL_SEARCHABLE_CONTAINER (source_object);
  GError *inner_error = NULL;
  RygelMediaObjects *objects = rygel_searchable_container_simple_search_finish (searchable,
                                                                                res,
                                                                                &data->total_matches,
                                                                                &inner_error);

  if (inner_error) {
    g_simple_async_result_take_error (data->async_result, inner_error);
  } else {
    data->result = objects;
  }
  g_simple_async_result_complete (data->async_result);
  g_object_unref (data->async_result);
}

static void
rygel_media_export_db_container_real_search (RygelSearchableContainer *base,
                                             RygelSearchExpression    *expression,
                                             guint                     offset,
                                             guint                     max_count,
                                             const gchar              *sort_criteria,
                                             GCancellable             *cancellable,
                                             GAsyncReadyCallback       callback,
                                             gpointer                  user_data) {
  RygelMediaExportDBContainer *self = RYGEL_MEDIA_EXPORT_DB_CONTAINER (base);
  const gchar *id = rygel_media_object_get_id (RYGEL_MEDIA_OBJECT (self));
  GError *inner_error = NULL;
  guint total_matches = 0;
  GSimpleAsyncResult *simple;
  gboolean do_simple_search = FALSE;
  RygelMediaObjects *objects = rygel_media_export_media_cache_get_objects_by_search_expression (self->priv->media_db,
                                                                                                expression,
                                                                                                id,
                                                                                                sort_criteria,
                                                                                                offset,
                                                                                                max_count,
                                                                                                &total_matches,
                                                                                                &inner_error);

  if (inner_error &&
      g_error_matches (inner_error,
                       RYGEL_MEDIA_EXPORT_MEDIA_CACHE_ERROR,
                       RYGEL_MEDIA_EXPORT_MEDIA_CACHE_ERROR_UNSUPPORTED_SEARCH)) {
    do_simple_search = TRUE;
    g_error_free (inner_error);
    inner_error = NULL;
  }

  if (inner_error) {
    simple = g_simple_async_result_new_take_error (G_OBJECT (self),
                                                   callback,
                                                   user_data,
                                                   inner_error);
  } else {
    RygelMediaExportDBContainerSearchData *data = g_slice_new0 (RygelMediaExportDBContainerSearchData);

    simple = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        rygel_media_export_db_container_real_search);
    g_simple_async_result_set_op_res_gpointer (simple,
                                               data,
                                               rygel_media_export_db_container_real_search_data_free);

    if (do_simple_search) {
      data->async_result = simple;
      rygel_searchable_container_simple_search (base,
                                                expression,
                                                offset,
                                                max_count,
                                                sort_criteria,
                                                cancellable,
                                                rygel_media_export_db_container_search_ready,
                                                data);
      return;
    } else {
      data->result = objects;
      data->total_matches = total_matches;
    }
  }

  g_simple_async_result_complete_in_idle (simple);
  g_object_unref (simple);
}

static RygelMediaObjects *
rygel_media_export_db_container_real_search_finish (RygelSearchableContainer  *base G_GNUC_UNUSED,
                                                    GAsyncResult              *res,
                                                    guint                     *total_matches,
                                                    GError                   **error) {
  RygelMediaObjects* result;
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  RygelMediaExportDBContainerSearchData* data;

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
rygel_media_export_db_container_real_find_object (RygelMediaContainer *base,
                                                  const gchar         *id,
                                                  GCancellable        *cancellable G_GNUC_UNUSED,
                                                  GAsyncReadyCallback  callback,
                                                  gpointer             user_data) {
  GError *inner_error = NULL;
  RygelMediaExportDBContainer *self = RYGEL_MEDIA_EXPORT_DB_CONTAINER (base);
  RygelMediaObject *object = rygel_media_export_media_cache_get_object (self->priv->media_db,
                                                                        id,
                                                                        &inner_error);
  GSimpleAsyncResult *simple;

  if (inner_error) {
    simple = g_simple_async_result_new_take_error (G_OBJECT (self),
                                                   callback,
                                                   user_data,
                                                   inner_error);
  } else {
    simple = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        rygel_media_export_db_container_real_find_object);
    if (object) {
      g_simple_async_result_set_op_res_gpointer (simple,
                                                 object,
                                                 g_object_unref);
    }
  }
  g_simple_async_result_complete_in_idle (simple);
  g_object_unref (simple);
}


static RygelMediaObject *
rygel_media_export_db_container_real_find_object_finish (RygelMediaContainer  *base G_GNUC_UNUSED,
                                                         GAsyncResult         *res,
                                                         GError              **error) {
  RygelMediaObject *result;
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);

  if (g_simple_async_result_propagate_error (simple, error)) {
    return NULL;
  }
  result = g_simple_async_result_get_op_res_gpointer (simple);

  if (result) {
    g_object_ref (result);
  }

  return RYGEL_MEDIA_OBJECT (result);
}

static GeeArrayList *
rygel_media_export_db_container_real_get_search_classes (RygelSearchableContainer *base) {
  RygelMediaExportDBContainer* self = RYGEL_MEDIA_EXPORT_DB_CONTAINER (base);

  return self->priv->search_classes;
}


static void
rygel_media_export_db_container_real_set_search_classes (RygelSearchableContainer *base,
                                                         GeeArrayList             *value) {
  RygelMediaExportDBContainer* self = RYGEL_MEDIA_EXPORT_DB_CONTAINER (base);
  GeeArrayList *old = self->priv->search_classes;

  if (value) {
    g_object_ref (value);
  }
  self->priv->search_classes = value;
  if (old) {
    g_object_unref (old);
  }

  g_object_notify (G_OBJECT (self), "search-classes");
}

static void
rygel_media_export_db_container_dispose (GObject *object) {
  RygelMediaExportDBContainer *self = RYGEL_MEDIA_EXPORT_DB_CONTAINER (object);
  RygelMediaExportDBContainerPrivate *priv = self->priv;

  if (priv->media_db) {
    RygelMediaExportMediaCache *cache = priv->media_db;

    priv->media_db = NULL;
    g_object_unref (cache);
  }
  if (priv->search_classes) {
    GeeArrayList *list = priv->search_classes;

    priv->search_classes = NULL;
    g_object_unref (list);
  }

  G_OBJECT_CLASS (rygel_media_export_db_container_parent_class)->dispose (object);
}

static void
rygel_media_export_db_container_get_property (GObject    *object,
                                              guint       property_id,
                                              GValue     *value,
                                              GParamSpec *pspec) {
  RygelMediaExportDBContainer *self = RYGEL_MEDIA_EXPORT_DB_CONTAINER (object);
  RygelMediaExportDBContainerPrivate *priv = self->priv;

  switch (property_id) {
  case RYGEL_MEDIA_EXPORT_DB_CONTAINER_SEARCH_CLASSES:
    g_value_set_object (value, priv->search_classes);
    break;

  case RYGEL_MEDIA_EXPORT_DB_CONTAINER_MEDIA_DB:
    g_value_set_object (value, priv->media_db);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
rygel_media_export_db_container_set_property (GObject      *object,
                                              guint         property_id,
                                              const GValue *value,
                                              GParamSpec   *pspec) {
  RygelMediaExportDBContainer *self = RYGEL_MEDIA_EXPORT_DB_CONTAINER (object);

  switch (property_id) {
  case RYGEL_MEDIA_EXPORT_DB_CONTAINER_SEARCH_CLASSES:
    rygel_searchable_container_set_search_classes (RYGEL_SEARCHABLE_CONTAINER (self),
                                                   g_value_get_object (value));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
rygel_media_export_db_container_class_init (RygelMediaExportDBContainerClass *db_container_class) {
  GObjectClass *object_class = G_OBJECT_CLASS (db_container_class);
  RygelMediaContainerClass *media_container_class = RYGEL_MEDIA_CONTAINER_CLASS (db_container_class);

  db_container_class->count_children = rygel_media_export_db_container_real_count_children;
  media_container_class->get_children = rygel_media_export_db_container_real_get_children;
  media_container_class->get_children_finish = rygel_media_export_db_container_real_get_children_finish;
  media_container_class->find_object = rygel_media_export_db_container_real_find_object;
  media_container_class->find_object_finish = rygel_media_export_db_container_real_find_object_finish;
  object_class->get_property = rygel_media_export_db_container_get_property;
  object_class->set_property = rygel_media_export_db_container_set_property;
  object_class->dispose = rygel_media_export_db_container_dispose;
  object_class->constructed = rygel_media_export_db_container_constructed;

  g_object_class_install_property (object_class,
                                   RYGEL_MEDIA_EXPORT_DB_CONTAINER_SEARCH_CLASSES,
                                   g_param_spec_object ("search-classes",
                                                        "search-classes",
                                                        "search-classes",
                                                        GEE_TYPE_ARRAY_LIST,
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE));
  g_object_class_install_property (object_class,
                                   RYGEL_MEDIA_EXPORT_DB_CONTAINER_MEDIA_DB,
                                   g_param_spec_object ("media-db",
                                                        "media-db",
                                                        "media-db",
                                                        RYGEL_MEDIA_EXPORT_TYPE_MEDIA_CACHE,
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_READABLE));

  g_type_class_add_private (db_container_class,
                            sizeof (RygelMediaExportDBContainerPrivate));
}

static void
rygel_media_export_db_container_rygel_searchable_container_interface_init (RygelSearchableContainerIface *iface) {
  iface->search = rygel_media_export_db_container_real_search;
  iface->search_finish = rygel_media_export_db_container_real_search_finish;
  iface->get_search_classes = rygel_media_export_db_container_real_get_search_classes;
  iface->set_search_classes = rygel_media_export_db_container_real_set_search_classes;
}

static void
rygel_media_export_db_container_init (RygelMediaExportDBContainer *self) {
  self->priv = RYGEL_MEDIA_EXPORT_DB_CONTAINER_GET_PRIVATE (self);
}

RygelMediaExportMediaCache *
rygel_media_export_db_container_get_media_db (RygelMediaExportDBContainer *self) {
  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_DB_CONTAINER (self), NULL);

  return self->priv->media_db;
}

gint
rygel_media_export_db_container_count_children (RygelMediaExportDBContainer *self) {
  RygelMediaExportDBContainerClass *db_container_class;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_DB_CONTAINER (self), 0);

  db_container_class = RYGEL_MEDIA_EXPORT_DB_CONTAINER_GET_CLASS (self);

  g_return_val_if_fail (db_container_class != NULL, 0);
  g_return_val_if_fail (db_container_class->count_children != NULL, 0);

  return db_container_class->count_children (self);
}
