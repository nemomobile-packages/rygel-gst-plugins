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

#include "rygel-media-export-node-query-container.h"
#include "rygel-media-export-query-container-factory.h"
#include "rygel-media-export-string-utils.h"

G_DEFINE_TYPE (RygelMediaExportNodeQueryContainer,
               rygel_media_export_node_query_container,
               RYGEL_MEDIA_EXPORT_TYPE_QUERY_CONTAINER)

typedef struct _RygelMediaExportNodeQueryContainerGetChildrenData RygelMediaExportNodeQueryContainerGetChildrenData;

struct _RygelMediaExportNodeQueryContainerPrivate {
  gchar *template;
  gchar *attribute;
};

#define RYGEL_MEDIA_EXPORT_NODE_QUERY_CONTAINER_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                RYGEL_MEDIA_EXPORT_TYPE_NODE_QUERY_CONTAINER, \
                                RygelMediaExportNodeQueryContainerPrivate))
enum  {
  RYGEL_MEDIA_EXPORT_NODE_QUERY_CONTAINER_DUMMY_PROPERTY,
  RYGEL_MEDIA_EXPORT_NODE_QUERY_CONTAINER_TEMPLATE,
  RYGEL_MEDIA_EXPORT_NODE_QUERY_CONTAINER_ATTRIBUTE
};

RygelMediaExportNodeQueryContainer *
rygel_media_export_node_query_container_new (RygelSearchExpression *expression,
                                             const gchar           *id,
                                             const gchar           *name,
                                             const gchar           *template,
                                             const gchar           *attribute) {
  g_return_val_if_fail (RYGEL_IS_SEARCH_EXPRESSION (expression), NULL);
  g_return_val_if_fail (id != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (template != NULL, NULL);
  g_return_val_if_fail (attribute != NULL, NULL);

  return RYGEL_MEDIA_EXPORT_NODE_QUERY_CONTAINER (g_object_new (RYGEL_MEDIA_EXPORT_TYPE_NODE_QUERY_CONTAINER,
                                                                "expression", expression,
                                                                "id", id,
                                                                "title", name,
                                                                "parent", NULL,
                                                                "child-count", 0,
                                                                "template", template,
                                                                "attribute", attribute,
                                                                NULL));
}

static void
rygel_media_export_node_query_container_real_get_children (RygelMediaContainer *base,
                                                           guint                offset,
                                                           guint                max_count,
                                                           const gchar         *sort_criteria G_GNUC_UNUSED,
                                                           GCancellable        *cancellable G_GNUC_UNUSED,
                                                           GAsyncReadyCallback  callback,
                                                           gpointer             user_data) {
  RygelMediaExportNodeQueryContainer *self = RYGEL_MEDIA_EXPORT_NODE_QUERY_CONTAINER (base);
  RygelMediaExportNodeQueryContainerPrivate *priv = self->priv;
  RygelMediaExportMediaCache *cache = rygel_media_export_db_container_get_media_db (RYGEL_MEDIA_EXPORT_DB_CONTAINER (base));
  RygelSearchExpression *expression = rygel_media_export_query_container_get_expression (RYGEL_MEDIA_EXPORT_QUERY_CONTAINER (base));
  GError *error = NULL;
  GeeList *data = rygel_media_export_media_cache_get_object_attribute_by_search_expression (cache,
                                                                                            priv->attribute,
                                                                                            expression,
                                                                                            (glong) offset,
                                                                                            max_count,
                                                                                            &error);
  GSimpleAsyncResult *simple;

  if (error) {
    simple = g_simple_async_result_new_take_error (G_OBJECT (base),
                                                   callback,
                                                   user_data,
                                                   error);
  } else {
    RygelMediaObjects *objects = rygel_media_objects_new ();
    gint collection_size = gee_collection_get_size (GEE_COLLECTION (data));
    RygelMediaExportQueryContainerFactory *factory = rygel_media_export_query_container_factory_get_default ();
    gint iter;

    for (iter = 0; iter < collection_size; ++iter) {
      gchar *meta_data = gee_list_get (data, iter);
      gchar *tmp_id = g_uri_escape_string (meta_data, "", TRUE);
      gchar *new_id = rygel_media_export_string_replace (priv->template, "%s", tmp_id, &error);

      if (error) {
        g_warning ("Failed to replace a %%s placeholder in template (%s) with an id (%s): %s",
                   priv->template,
                   tmp_id,
                   error->message);
        g_error_free (error);
        error = NULL;
      } else {
        RygelMediaExportQueryContainer *container = rygel_media_export_query_container_factory_create_from_description (factory,
                                                                                                                        new_id,
                                                                                                                        meta_data);
        RygelMediaObject *object = RYGEL_MEDIA_OBJECT (container);

        rygel_media_object_set_parent (object, base);
        gee_abstract_collection_add (GEE_ABSTRACT_COLLECTION (objects), object);
        g_object_unref (container);
        g_free (new_id);
      }
      g_free (tmp_id);
      g_free (meta_data);
    }
    g_object_unref (factory);
    g_object_unref (data);

    simple = g_simple_async_result_new (G_OBJECT (base),
                                        callback,
                                        user_data,
                                        rygel_media_export_node_query_container_real_get_children);
    g_simple_async_result_set_op_res_gpointer (simple,
                                               objects,
                                               g_object_unref);
  }
  g_simple_async_result_complete_in_idle (simple);
  g_object_unref (simple);
}


static RygelMediaObjects *
rygel_media_export_node_query_container_real_get_children_finish (RygelMediaContainer  *base G_GNUC_UNUSED,
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
rygel_media_export_node_query_container_real_count_children (RygelMediaExportDBContainer *base) {
  RygelMediaExportNodeQueryContainer *self = RYGEL_MEDIA_EXPORT_NODE_QUERY_CONTAINER (base);
  RygelMediaExportNodeQueryContainerPrivate *priv = self->priv;
  RygelMediaExportQueryContainer *query_container = RYGEL_MEDIA_EXPORT_QUERY_CONTAINER (base);
  RygelSearchExpression *expression = rygel_media_export_query_container_get_expression (query_container);
  RygelMediaExportMediaCache *cache = rygel_media_export_db_container_get_media_db (base);
  GError *error = NULL;
  GeeList *data = rygel_media_export_media_cache_get_object_attribute_by_search_expression (cache,
                                                                                            priv->attribute,
                                                                                            expression,
                                                                                            0,
                                                                                            -1,
                                                                                            &error);
  gint count;

  if (error) {
    g_error_free (error);
    return 0;
  }

  count = gee_collection_get_size (GEE_COLLECTION (data));
  g_object_unref (data);

  return count;
}

static void
rygel_media_export_node_query_container_finalize (GObject *object) {
  RygelMediaExportNodeQueryContainer *self = RYGEL_MEDIA_EXPORT_NODE_QUERY_CONTAINER (object);
  RygelMediaExportNodeQueryContainerPrivate *priv = self->priv;

  g_free (priv->template);
  g_free (priv->attribute);

  G_OBJECT_CLASS (rygel_media_export_node_query_container_parent_class)->finalize (object);
}

static void
rygel_media_export_node_query_container_get_property (GObject    *object,
                                                      guint       property_id,
                                                      GValue     *value,
                                                      GParamSpec *pspec) {
  RygelMediaExportNodeQueryContainer *self = RYGEL_MEDIA_EXPORT_NODE_QUERY_CONTAINER (object);
  RygelMediaExportNodeQueryContainerPrivate *priv = self->priv;

  switch (property_id) {
  case RYGEL_MEDIA_EXPORT_NODE_QUERY_CONTAINER_TEMPLATE:
    g_value_set_string (value, priv->template);
    break;

  case RYGEL_MEDIA_EXPORT_NODE_QUERY_CONTAINER_ATTRIBUTE:
    g_value_set_string (value, priv->attribute);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
rygel_media_export_node_query_container_set_property (GObject      *object,
                                                      guint         property_id,
                                                      const GValue *value,
                                                      GParamSpec   *pspec) {
  RygelMediaExportNodeQueryContainer *self = RYGEL_MEDIA_EXPORT_NODE_QUERY_CONTAINER (object);
  RygelMediaExportNodeQueryContainerPrivate *priv = self->priv;

  switch (property_id) {
  case RYGEL_MEDIA_EXPORT_NODE_QUERY_CONTAINER_TEMPLATE:
    /* construct-only property */
    priv->template = g_value_dup_string (value);
    break;

  case RYGEL_MEDIA_EXPORT_NODE_QUERY_CONTAINER_ATTRIBUTE:
    /* construct-only property */
    priv->attribute = g_value_dup_string (value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
rygel_media_export_node_query_container_class_init (RygelMediaExportNodeQueryContainerClass *node_query_container_class) {
  GObjectClass *object_class = G_OBJECT_CLASS (node_query_container_class);
  RygelMediaContainerClass *container_class = RYGEL_MEDIA_CONTAINER_CLASS (node_query_container_class);
  RygelMediaExportDBContainerClass *db_container_class = RYGEL_MEDIA_EXPORT_DB_CONTAINER_CLASS (node_query_container_class);

  container_class->get_children = rygel_media_export_node_query_container_real_get_children;
  container_class->get_children_finish = rygel_media_export_node_query_container_real_get_children_finish;
  db_container_class->count_children = rygel_media_export_node_query_container_real_count_children;
  object_class->finalize = rygel_media_export_node_query_container_finalize;
  object_class->get_property = rygel_media_export_node_query_container_get_property;
  object_class->set_property = rygel_media_export_node_query_container_set_property;

  g_object_class_install_property (object_class,
                                   RYGEL_MEDIA_EXPORT_NODE_QUERY_CONTAINER_TEMPLATE,
                                   g_param_spec_string ("template",
                                                        "template",
                                                        "template",
                                                        NULL,
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   RYGEL_MEDIA_EXPORT_NODE_QUERY_CONTAINER_ATTRIBUTE,
                                   g_param_spec_string ("attribute",
                                                        "attribute",
                                                        "attribute",
                                                        NULL,
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (node_query_container_class,
                            sizeof (RygelMediaExportNodeQueryContainerPrivate));
}


static void rygel_media_export_node_query_container_init (RygelMediaExportNodeQueryContainer *self) {
  self->priv = RYGEL_MEDIA_EXPORT_NODE_QUERY_CONTAINER_GET_PRIVATE (self);
}
