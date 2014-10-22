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

#include "rygel-media-export-dummy-container.h"
#include "rygel-media-export-media-cache.h"

G_DEFINE_TYPE (RygelMediaExportDummyContainer,
               rygel_media_export_dummy_container,
               RYGEL_MEDIA_EXPORT_TYPE_TRACKABLE_DB_CONTAINER)

struct _RygelMediaExportDummyContainerPrivate {
  GFile *file;
  GeeList *children_list;
};

#define RYGEL_MEDIA_EXPORT_DUMMY_CONTAINER_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                RYGEL_MEDIA_EXPORT_TYPE_DUMMY_CONTAINER, \
                                RygelMediaExportDummyContainerPrivate))

enum  {
  RYGEL_MEDIA_EXPORT_DUMMY_CONTAINER_DUMMY_PROPERTY,
  RYGEL_MEDIA_EXPORT_DUMMY_CONTAINER_CHILDREN_LIST,
  RYGEL_MEDIA_EXPORT_DUMMY_CONTAINER_FILE
};

RygelMediaExportDummyContainer *
rygel_media_export_dummy_container_new (GFile               *file,
                                        RygelMediaContainer *parent) {
  RygelMediaExportDummyContainer *self;
  gchar *id;
  gchar *basename;
  RygelMediaExportMediaCache *media_db;
  GError *inner_error;
  GeeList *children_list;
  gint child_count;
  GeeArrayList *child_ids;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (RYGEL_IS_MEDIA_CONTAINER (parent), NULL);

  id = rygel_media_export_media_cache_get_id (file);
  basename = g_file_get_basename (file);

  inner_error = NULL;
  media_db = rygel_media_export_media_cache_get_default ();
  child_ids = rygel_media_export_media_cache_get_child_ids (media_db,
                                                            id,
                                                            &inner_error);
  g_object_unref (media_db);
  if (inner_error)
  {
    children_list = GEE_LIST (gee_array_list_new (G_TYPE_STRING,
                                                  (GBoxedCopyFunc) g_strdup,
                                                  g_free,
                                                  NULL,
                                                  NULL,
                                                  NULL));
    child_count = 0;
    g_error_free (inner_error);
  } else {
    children_list = GEE_LIST (child_ids);
    child_count = gee_collection_get_size (GEE_COLLECTION (child_ids));
  }

  self = RYGEL_MEDIA_EXPORT_DUMMY_CONTAINER (g_object_new (RYGEL_MEDIA_EXPORT_TYPE_DUMMY_CONTAINER,
                                                           "id", id,
                                                           "parent-ref", parent,
                                                           "title", basename,
                                                           "child-count", child_count,
                                                           "children-list", children_list,
                                                           "file", file,
                                                           NULL));
  g_free (id);
  g_free (basename);
  return self;
}

void
rygel_media_export_dummy_container_seen (RygelMediaExportDummyContainer *self,
                                         GFile *file) {
  gchar* id;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_DUMMY_CONTAINER (self));
  g_return_if_fail (G_IS_FILE (file));

  id = rygel_media_export_media_cache_get_id (file);
  gee_collection_remove ((GeeCollection*) self->priv->children_list, id);
  g_free (id);
}

static void rygel_media_export_dummy_container_dispose (GObject *object) {
  RygelMediaExportDummyContainer *self = RYGEL_MEDIA_EXPORT_DUMMY_CONTAINER (object);
  RygelMediaExportDummyContainerPrivate *priv = self->priv;

  if (priv->file) {
    GFile *file = priv->file;

    priv->file = NULL;
    g_object_unref (file);
  }
  if (priv->children_list) {
    GeeList *list = priv->children_list;

    priv->children_list = NULL;
    g_object_unref (list);
  }
  G_OBJECT_CLASS (rygel_media_export_dummy_container_parent_class)->dispose (object);
}

static void
rygel_media_export_dummy_container_constructed (GObject *object)
{
  RygelMediaExportDummyContainer *dummy = RYGEL_MEDIA_EXPORT_DUMMY_CONTAINER (object);
  RygelMediaObject *media_object = RYGEL_MEDIA_OBJECT (object);
  RygelMediaContainer *media_container = RYGEL_MEDIA_CONTAINER (object);
  GeeAbstractCollection *uris;
  gchar *uri = g_file_get_uri (dummy->priv->file);
  RygelMediaExportMediaCache *media_db;
  guint32 object_update_id = 0;
  guint32 container_update_id = 0;
  guint32 total_deleted_child_count = 0;

  G_OBJECT_CLASS (rygel_media_export_dummy_container_parent_class)->constructed (object);
  uris = GEE_ABSTRACT_COLLECTION (rygel_media_object_get_uris (media_object));
  gee_abstract_collection_add (uris, uri);
  g_free (uri);

  media_db = rygel_media_export_media_cache_get_default ();
  rygel_media_export_media_cache_get_track_properties (media_db,
                                                       rygel_media_object_get_id (media_object),
                                                       &object_update_id,
                                                       &container_update_id,
                                                       &total_deleted_child_count);
  g_object_unref (media_db);

  rygel_media_object_set_object_update_id (media_object, object_update_id);
  media_container->update_id = container_update_id;
  media_container->total_deleted_child_count = total_deleted_child_count;
}

static void
rygel_media_export_dummy_container_get_property (GObject    *object,
                                                 guint       property_id,
                                                 GValue     *value,
                                                 GParamSpec *pspec) {
  RygelMediaExportDummyContainer *self = RYGEL_MEDIA_EXPORT_DUMMY_CONTAINER (object);
  RygelMediaExportDummyContainerPrivate *priv = self->priv;

  switch (property_id) {
  case RYGEL_MEDIA_EXPORT_DUMMY_CONTAINER_CHILDREN_LIST:
    g_value_set_object (value, priv->children_list);
    break;

  case RYGEL_MEDIA_EXPORT_DUMMY_CONTAINER_FILE:
    g_value_set_object (value, priv->file);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
rygel_media_export_dummy_container_set_property (GObject      *object,
                                                 guint         property_id,
                                                 const GValue *value,
                                                 GParamSpec   *pspec) {
  RygelMediaExportDummyContainer *self = RYGEL_MEDIA_EXPORT_DUMMY_CONTAINER (object);
  RygelMediaExportDummyContainerPrivate *priv = self->priv;

  switch (property_id) {
  case RYGEL_MEDIA_EXPORT_DUMMY_CONTAINER_CHILDREN_LIST:
    /* construct only property */
    priv->children_list = g_value_dup_object (value);
    break;

  case RYGEL_MEDIA_EXPORT_DUMMY_CONTAINER_FILE:
    /* construct only property */
    priv->file = g_value_dup_object (value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void rygel_media_export_dummy_container_class_init (RygelMediaExportDummyContainerClass *dummy_class) {
  GObjectClass *object_class = G_OBJECT_CLASS (dummy_class);

  object_class->dispose = rygel_media_export_dummy_container_dispose;
  object_class->constructed = rygel_media_export_dummy_container_constructed;
  object_class->get_property = rygel_media_export_dummy_container_get_property;
  object_class->set_property = rygel_media_export_dummy_container_set_property;

  g_object_class_install_property (object_class,
                                   RYGEL_MEDIA_EXPORT_DUMMY_CONTAINER_CHILDREN_LIST,
                                   g_param_spec_object ("children-list",
                                                        "children-list",
                                                        "children-list",
                                                        GEE_TYPE_LIST,
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   RYGEL_MEDIA_EXPORT_DUMMY_CONTAINER_FILE,
                                   g_param_spec_object ("file",
                                                        "file",
                                                        "file",
                                                        G_TYPE_FILE,
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (dummy_class,
                            sizeof (RygelMediaExportDummyContainerPrivate));
}

static void rygel_media_export_dummy_container_init (RygelMediaExportDummyContainer * self) {
  self->priv = RYGEL_MEDIA_EXPORT_DUMMY_CONTAINER_GET_PRIVATE (self);
}

GFile *
rygel_media_export_dummy_container_get_g_file (RygelMediaExportDummyContainer *self)
{
  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_DUMMY_CONTAINER (self), NULL);

  return self->priv->file;
}

GeeList *
rygel_media_export_dummy_container_get_children_list (RygelMediaExportDummyContainer *self)
{
  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_DUMMY_CONTAINER (self), NULL);

  return self->priv->children_list;
}
