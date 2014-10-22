/*
 * Copyright (C) 2011 Jens Georg <mail@jensge.org>.
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

#include "rygel-media-export-leaf-query-container.h"

G_DEFINE_TYPE (RygelMediaExportLeafQueryContainer,
               rygel_media_export_leaf_query_container,
               RYGEL_MEDIA_EXPORT_TYPE_QUERY_CONTAINER);

RygelMediaExportLeafQueryContainer*
rygel_media_export_leaf_query_container_new (RygelSearchExpression *expression,
                                             const gchar           *id,
                                             const gchar           *name) {
  g_return_val_if_fail (RYGEL_IS_SEARCH_EXPRESSION (expression), NULL);
  g_return_val_if_fail (id != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return RYGEL_MEDIA_EXPORT_LEAF_QUERY_CONTAINER (g_object_new (RYGEL_MEDIA_EXPORT_TYPE_LEAF_QUERY_CONTAINER,
                                                                "id", id,
                                                                "title", name,
                                                                "child-count", 0,
                                                                "expression", expression,
                                                                "parent", NULL,
                                                                NULL));
}

static void
rygel_media_export_leaf_query_container_get_children_ready (GObject      *source_object,
                                                            GAsyncResult *res,
                                                            gpointer      user_data) {
  RygelSearchableContainer *container = RYGEL_SEARCHABLE_CONTAINER (source_object);
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
  GError *error = NULL;
  RygelMediaObjects *children = rygel_searchable_container_search_finish (container, res, NULL, &error);

  if (error) {
    g_simple_async_result_take_error (simple, error);
  } else {
    gint iter;
    gint size = gee_abstract_collection_get_size (GEE_ABSTRACT_COLLECTION (children));
    RygelMediaContainer *media_container = RYGEL_MEDIA_CONTAINER (source_object);

    for (iter = 0; iter < size; ++iter) {
      RygelMediaObject *child = RYGEL_MEDIA_OBJECT (gee_abstract_list_get (GEE_ABSTRACT_LIST (children), iter));

      rygel_media_object_set_parent (child, media_container);
      g_object_unref (child);
    }

    g_simple_async_result_set_op_res_gpointer (simple, children, g_object_unref);
  }
  g_simple_async_result_complete (simple);
  g_object_unref (simple);
}

static void
rygel_media_export_leaf_query_container_real_get_children (RygelMediaContainer *base,
                                                           guint                offset,
                                                           guint                max_count,
                                                           const gchar         *sort_criteria,
                                                           GCancellable        *cancellable,
                                                           GAsyncReadyCallback  callback,
                                                           gpointer             user_data) {
  GSimpleAsyncResult *simple = g_simple_async_result_new (G_OBJECT (base),
                                                          callback,
                                                          user_data,
                                                          rygel_media_export_leaf_query_container_real_get_children);
  RygelSearchableContainer *searchable = RYGEL_SEARCHABLE_CONTAINER (base);

  rygel_searchable_container_search (searchable,
                                     NULL,
                                     offset,
                                     max_count,
                                     sort_criteria,
                                     cancellable,
                                     rygel_media_export_leaf_query_container_get_children_ready,
                                     simple);
}

static RygelMediaObjects *
rygel_media_export_leaf_query_container_real_get_children_finish (RygelMediaContainer  *base G_GNUC_UNUSED,
                                                                  GAsyncResult         *res,
                                                                  GError              **error) {
  RygelMediaObjects* result;
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

static gint
rygel_media_export_leaf_query_container_real_count_children (RygelMediaExportDBContainer  *base) {
  GError *error = NULL;
  RygelMediaExportQueryContainer *query_container = RYGEL_MEDIA_EXPORT_QUERY_CONTAINER (base);
  RygelMediaExportMediaCache *cache = rygel_media_export_db_container_get_media_db (base);
  RygelSearchExpression *expression = rygel_media_export_query_container_get_expression (query_container);
  gint result = (gint) rygel_media_export_media_cache_get_object_count_by_search_expression (cache,
                                                                                             expression,
                                                                                             NULL,
                                                                                             &error);

  if (error) {
    g_error_free (error);
    return 0;
  }

  return result;
}

static void
rygel_media_export_leaf_query_container_class_init (RygelMediaExportLeafQueryContainerClass *leaf_class) {
  RygelMediaContainerClass *container_class = RYGEL_MEDIA_CONTAINER_CLASS (leaf_class);
  RygelMediaExportDBContainerClass *db_container_class = RYGEL_MEDIA_EXPORT_DB_CONTAINER_CLASS (leaf_class);

  container_class->get_children = rygel_media_export_leaf_query_container_real_get_children;
  container_class->get_children_finish = rygel_media_export_leaf_query_container_real_get_children_finish;
  db_container_class->count_children = rygel_media_export_leaf_query_container_real_count_children;
}

static void
rygel_media_export_leaf_query_container_init (RygelMediaExportLeafQueryContainer *self G_GNUC_UNUSED) {
}
