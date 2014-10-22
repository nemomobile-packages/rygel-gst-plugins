/*
 * Copyright (C) 2009, 2010 Jens Georg <mail@jensge.org>.
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

#include "rygel-media-export-errors.h"
#include "rygel-media-export-query-container.h"

static void
rygel_media_export_query_container_rygel_searchable_container_interface_init (RygelSearchableContainerIface *iface);

G_DEFINE_TYPE_WITH_CODE (RygelMediaExportQueryContainer,
                         rygel_media_export_query_container,
                         RYGEL_MEDIA_EXPORT_TYPE_DB_CONTAINER,
                         G_IMPLEMENT_INTERFACE (RYGEL_TYPE_SEARCHABLE_CONTAINER,
                                                rygel_media_export_query_container_rygel_searchable_container_interface_init))

struct _RygelMediaExportQueryContainerPrivate {
  RygelSearchExpression *expression;
};

typedef struct _RygelMediaExportQueryContainerSearchData RygelMediaExportQueryContainerSearchData;

struct _RygelMediaExportQueryContainerSearchData {
  guint total_matches;
  RygelMediaObjects *result;
};

#define RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                RYGEL_MEDIA_EXPORT_TYPE_QUERY_CONTAINER, \
                                RygelMediaExportQueryContainerPrivate))

enum  {
  RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_DUMMY_PROPERTY,
  RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_EXPRESSION
};

static void
rygel_media_export_query_container_real_search_data_free (gpointer user_data) {
  RygelMediaExportQueryContainerSearchData *data = (RygelMediaExportQueryContainerSearchData *) user_data;

  if (data->result) {
    g_object_unref (data->result);
  }
}


static void
rygel_media_export_query_container_real_search (RygelSearchableContainer *base,
                                                RygelSearchExpression    *expression,
                                                guint                     offset,
                                                guint                     max_count,
                                                const gchar              *sort_criteria,
                                                GCancellable             *cancellable G_GNUC_UNUSED,
                                                GAsyncReadyCallback       callback,
                                                gpointer                  user_data) {
  RygelMediaExportQueryContainer *self = RYGEL_MEDIA_EXPORT_QUERY_CONTAINER (base);
  GSimpleAsyncResult *simple;
  guint matches = 0;
  RygelMediaObjects *objects;
  RygelSearchExpression *combined;
  GError *error = NULL;
  RygelMediaExportMediaCache *cache = rygel_media_export_db_container_get_media_db (RYGEL_MEDIA_EXPORT_DB_CONTAINER (self));

  if (!expression) {
    combined = rygel_search_expression_ref (self->priv->expression);
  } else {
    combined = RYGEL_SEARCH_EXPRESSION (rygel_logical_expression_new ());
    combined->operand1 = rygel_search_expression_ref (self->priv->expression);
    combined->op = RYGEL_LOGICAL_OPERATOR_AND;
    combined->operand2 = rygel_search_expression_ref (expression);
  }

  objects = rygel_media_export_media_cache_get_objects_by_search_expression (cache,
                                                                             combined,
                                                                             NULL,
                                                                             sort_criteria,
                                                                             offset,
                                                                             max_count,
                                                                             &matches,
                                                                             &error);

  if (error) {
    if (g_error_matches (error,
                         RYGEL_MEDIA_EXPORT_MEDIA_CACHE_ERROR,
                         RYGEL_MEDIA_EXPORT_MEDIA_CACHE_ERROR_UNSUPPORTED_SEARCH)) {
      objects = rygel_media_objects_new ();
      matches = 0;
      g_error_free (error);
      error = NULL;
    }
  }

  if (error) {
    simple = g_simple_async_result_new_take_error (G_OBJECT (self),
                                                   callback,
                                                   user_data,
                                                   error);
  } else {
    RygelMediaExportQueryContainerSearchData *data = g_slice_new0 (RygelMediaExportQueryContainerSearchData);

    simple = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        rygel_media_export_query_container_real_search);
    data->result = objects;
    data->total_matches = matches;
    g_simple_async_result_set_op_res_gpointer (simple,
                                               data,
                                               rygel_media_export_query_container_real_search_data_free);
  }
  g_simple_async_result_complete_in_idle (simple);
  g_object_unref (simple);
  rygel_search_expression_unref (combined);
}


static RygelMediaObjects *
rygel_media_export_query_container_real_search_finish (RygelSearchableContainer  *base G_GNUC_UNUSED,
                                                       GAsyncResult              *res,
                                                       guint                     *total_matches,
                                                       GError                   **error) {
  RygelMediaObjects* result;
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  RygelMediaExportQueryContainerSearchData* data;

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

static void rygel_media_export_query_container_dispose (GObject *object) {
  RygelMediaExportQueryContainer *self = RYGEL_MEDIA_EXPORT_QUERY_CONTAINER (object);
  RygelMediaExportQueryContainerPrivate *priv = self->priv;

  if (priv->expression) {
    RygelSearchExpression *expression = priv->expression;

    priv->expression = NULL;
    rygel_search_expression_unref (expression);
  }
  G_OBJECT_CLASS (rygel_media_export_query_container_parent_class)->dispose (object);
}

static void
rygel_media_export_query_container_get_property (GObject    *object,
                                                 guint       property_id,
                                                 GValue     *value,
                                                 GParamSpec *pspec) {
  RygelMediaExportQueryContainer *self = RYGEL_MEDIA_EXPORT_QUERY_CONTAINER (object);
  RygelMediaExportQueryContainerPrivate *priv = self->priv;

  switch (property_id) {
  case RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_EXPRESSION:
    rygel_value_set_search_expression (value, priv->expression);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
rygel_media_export_query_container_set_property (GObject      *object,
                                                 guint         property_id,
                                                 const GValue *value,
                                                 GParamSpec   *pspec) {
  RygelMediaExportQueryContainer *self = RYGEL_MEDIA_EXPORT_QUERY_CONTAINER (object);
  RygelMediaExportQueryContainerPrivate *priv = self->priv;

  switch (property_id) {
  case RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_EXPRESSION:
    /* construct only property */
    priv->expression = rygel_value_get_search_expression (value);
    rygel_search_expression_ref (priv->expression);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
rygel_media_export_query_container_class_init (RygelMediaExportQueryContainerClass *query_container_class) {
  GObjectClass *object_class = G_OBJECT_CLASS (query_container_class);

  object_class->dispose = rygel_media_export_query_container_dispose;
  object_class->set_property = rygel_media_export_query_container_set_property;
  object_class->get_property = rygel_media_export_query_container_get_property;

  g_object_class_install_property (object_class,
                                   RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_EXPRESSION,
                                   rygel_param_spec_search_expression ("expression",
                                                                       "expression",
                                                                       "expression",
                                                                       RYGEL_TYPE_SEARCH_EXPRESSION,
                                                                       G_PARAM_STATIC_NAME |
                                                                       G_PARAM_STATIC_NICK |
                                                                       G_PARAM_STATIC_BLURB |
                                                                       G_PARAM_READABLE |
                                                                       G_PARAM_WRITABLE |
                                                                       G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (query_container_class,
                            sizeof (RygelMediaExportQueryContainerPrivate));
}


static void rygel_media_export_query_container_init (RygelMediaExportQueryContainer *self) {
  self->priv = RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_GET_PRIVATE (self);
}

static void
rygel_media_export_query_container_rygel_searchable_container_interface_init (RygelSearchableContainerIface *iface) {
  iface->search = rygel_media_export_query_container_real_search;
  iface->search_finish = rygel_media_export_query_container_real_search_finish;
  /* iface should be a copy of parent_searchable_container_iface, so
   * we don't have to override the {get,set}_search_classes funcs.
   *
   * iface->get_search_classes = parent_searchable_container_iface->get_search_classes;
   * iface->set_search_classes = parent_searchable_container_iface->set_search_classes;
   */
}

RygelSearchExpression *
rygel_media_export_query_container_get_expression (RygelMediaExportQueryContainer *self)
{
  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_QUERY_CONTAINER (self), NULL);

  return self->priv->expression;
}
