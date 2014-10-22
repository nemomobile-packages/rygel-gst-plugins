/*
 * Copyright (C) 2010 Jens Georg <mail@jensge.org>.
 * Copyright (C) 2013 Intel Corporation.
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

#include "rygel-media-export-writable-db-container.h"

static void
rygel_media_export_writable_db_container_rygel_writable_container_interface_init (RygelWritableContainerIface * iface);

G_DEFINE_TYPE_WITH_CODE (RygelMediaExportWritableDbContainer,
                         rygel_media_export_writable_db_container,
                         RYGEL_MEDIA_EXPORT_TYPE_TRACKABLE_DB_CONTAINER,
                         G_IMPLEMENT_INTERFACE (RYGEL_TYPE_WRITABLE_CONTAINER,
                                                rygel_media_export_writable_db_container_rygel_writable_container_interface_init));

struct _RygelMediaExportWritableDbContainerPrivate {
  GeeArrayList* create_classes;
};

enum {
  RYGEL_MEDIA_EXPORT_WRITABLE_DB_CONTAINER_DUMMY_PROPERTY,
  RYGEL_MEDIA_EXPORT_WRITABLE_DB_CONTAINER_CREATE_CLASSES
};

#define RYGEL_MEDIA_EXPORT_WRITABLE_DB_CONTAINER_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                RYGEL_MEDIA_EXPORT_TYPE_WRITABLE_DB_CONTAINER, \
                                RygelMediaExportWritableDbContainerPrivate))

RygelMediaExportWritableDbContainer*
rygel_media_export_writable_db_container_new (const gchar *id,
                                              const gchar *title) {
  g_return_val_if_fail (id != NULL, NULL);
  g_return_val_if_fail (title != NULL, NULL);

  return RYGEL_MEDIA_EXPORT_WRITABLE_DB_CONTAINER (g_object_new (RYGEL_MEDIA_EXPORT_TYPE_WRITABLE_DB_CONTAINER,
                                                                 "id", id,
                                                                 "parent", NULL,
                                                                 "title", title,
                                                                 "child-count", 0,
                                                                 NULL));
}

static void
tracked_action_ready (GObject *source_object G_GNUC_UNUSED,
                      GAsyncResult *res G_GNUC_UNUSED,
                      gpointer user_data) {
  GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (user_data);

  g_simple_async_result_complete (result);
  g_object_unref (result);
}

static void
rygel_media_export_writable_db_container_real_add_item (RygelWritableContainer *base,
                                                        RygelMediaItem         *item,
                                                        GCancellable           *cancellable G_GNUC_UNUSED,
                                                        GAsyncReadyCallback     callback,
                                                        gpointer                user_data) {
  RygelMediaObject *object = RYGEL_MEDIA_OBJECT (item);
  gchar *uri;
  GFile *file;
  gchar *id;
  GSimpleAsyncResult *simple = g_simple_async_result_new (G_OBJECT (base),
                                                          callback,
                                                          user_data,
                                                          rygel_media_export_writable_db_container_real_add_item);

  rygel_media_object_set_parent (object, RYGEL_MEDIA_CONTAINER (base));
  uri = gee_abstract_list_get (GEE_ABSTRACT_LIST (rygel_media_object_get_uris (object)), 0);
  file = g_file_new_for_uri (uri);
  g_free (uri);
  if (g_file_is_native (file)) {
    rygel_media_object_set_modified (object, G_MAXINT64);
  }
  id = rygel_media_export_media_cache_get_id (file);
  g_object_unref (file);
  rygel_media_object_set_id (object, id);
  g_free (id);

  rygel_trackable_container_add_child_tracked (RYGEL_TRACKABLE_CONTAINER (base),
                                               object,
                                               tracked_action_ready,
                                               simple);
}

static void
rygel_media_export_writable_db_container_real_add_item_finish (RygelWritableContainer  *base G_GNUC_UNUSED,
                                                               GAsyncResult            *res,
                                                               GError                 **error) {
  g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
}

static void
rygel_media_export_writable_db_container_real_remove_item (RygelWritableContainer *base,
                                                           const gchar            *id,
                                                           GCancellable           *cancellable G_GNUC_UNUSED,
                                                           GAsyncReadyCallback     callback,
                                                           gpointer                user_data) {
  RygelMediaExportDBContainer *db_container = RYGEL_MEDIA_EXPORT_DB_CONTAINER (base);
  RygelMediaExportMediaCache *cache = rygel_media_export_db_container_get_media_db (db_container);
  GError *error = NULL;
  GSimpleAsyncResult *simple;
  RygelMediaObject *object = rygel_media_export_media_cache_get_object (cache, id, &error);

  if (error) {
    simple = g_simple_async_result_new_take_error (G_OBJECT (base),
                                                   callback,
                                                   user_data,
                                                   error);
    g_simple_async_result_complete_in_idle (simple);
    g_object_unref (simple);
  } else {
    simple = g_simple_async_result_new (G_OBJECT (base),
                                        callback,
                                        user_data,
                                        rygel_media_export_writable_db_container_real_remove_item);
    rygel_trackable_container_remove_child_tracked (RYGEL_TRACKABLE_CONTAINER (base),
                                                    object,
                                                    tracked_action_ready,
                                                    simple);
  }
}

static void
rygel_media_export_writable_db_container_real_remove_item_finish (RygelWritableContainer  *base G_GNUC_UNUSED,
                                                                  GAsyncResult            *res,
                                                                  GError                 **error) {
  g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
}

static GeeArrayList *
rygel_media_export_writable_db_container_real_get_create_classes (RygelWritableContainer *base) {
  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_WRITABLE_DB_CONTAINER (base), NULL);

  return RYGEL_MEDIA_EXPORT_WRITABLE_DB_CONTAINER (base)->priv->create_classes;
}

static void
rygel_media_export_writable_db_container_real_set_create_classes (RygelWritableContainer *base,
                                                                  GeeArrayList           *value) {
  RygelMediaExportWritableDbContainer *self;
  RygelMediaExportWritableDbContainerPrivate *priv;
  GeeArrayList *old;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_WRITABLE_DB_CONTAINER (base));

  self = RYGEL_MEDIA_EXPORT_WRITABLE_DB_CONTAINER (base);
  priv = self->priv;
  if (value) {
    g_object_ref (value);
  }
  old = priv->create_classes;
  priv->create_classes = value;
  if (old) {
    g_object_unref (old);
  }

  g_object_notify (G_OBJECT (self), "create-classes");
}

static void
rygel_media_export_writable_db_container_dispose (GObject *object) {
  RygelMediaExportWritableDbContainer *self = RYGEL_MEDIA_EXPORT_WRITABLE_DB_CONTAINER (object);
  RygelMediaExportWritableDbContainerPrivate *priv = self->priv;

  if (priv->create_classes) {
    GeeArrayList *list = priv->create_classes;

    priv->create_classes = NULL;
    g_object_unref (list);
  }

  G_OBJECT_CLASS (rygel_media_export_writable_db_container_parent_class)->dispose (object);
}

static void
rygel_media_export_writable_db_container_constructed (GObject *object) {
  GeeArrayList *create_classes;
  GeeAbstractCollection *collection;
  RygelWritableContainer *writable_container = RYGEL_WRITABLE_CONTAINER (object);

  G_OBJECT_CLASS (rygel_media_export_writable_db_container_parent_class)->constructed (object);

  create_classes = gee_array_list_new (G_TYPE_STRING, (GBoxedCopyFunc) g_strdup, g_free, NULL, NULL, NULL);
  collection = GEE_ABSTRACT_COLLECTION (create_classes);

  rygel_writable_container_set_create_classes (writable_container, create_classes);
  gee_abstract_collection_add (collection, RYGEL_IMAGE_ITEM_UPNP_CLASS);
  gee_abstract_collection_add (collection, RYGEL_PHOTO_ITEM_UPNP_CLASS);
  gee_abstract_collection_add (collection, RYGEL_VIDEO_ITEM_UPNP_CLASS);
  gee_abstract_collection_add (collection, RYGEL_AUDIO_ITEM_UPNP_CLASS);
  gee_abstract_collection_add (collection, RYGEL_MUSIC_ITEM_UPNP_CLASS);
  g_object_unref (create_classes);
}

static void
rygel_media_export_writable_db_container_get_property (GObject    *object,
                                                       guint       property_id,
                                                       GValue     *value,
                                                       GParamSpec *pspec) {
  RygelWritableContainer *writable = RYGEL_WRITABLE_CONTAINER (object);

  switch (property_id) {
  case RYGEL_MEDIA_EXPORT_WRITABLE_DB_CONTAINER_CREATE_CLASSES:
    g_value_set_object (value,
                        rygel_writable_container_get_create_classes (writable));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
rygel_media_export_writable_db_container_set_property (GObject      *object,
                                                       guint         property_id,
                                                       const GValue *value,
                                                       GParamSpec   *pspec) {
  RygelWritableContainer *writable = RYGEL_WRITABLE_CONTAINER (object);

  switch (property_id) {
  case RYGEL_MEDIA_EXPORT_WRITABLE_DB_CONTAINER_CREATE_CLASSES:
    rygel_writable_container_set_create_classes (writable,
                                                 g_value_get_object (value));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
rygel_media_export_writable_db_container_class_init (RygelMediaExportWritableDbContainerClass *writable_db_container_class) {
  GObjectClass *object_class = G_OBJECT_CLASS (writable_db_container_class);

  object_class->dispose = rygel_media_export_writable_db_container_dispose;
  object_class->constructed = rygel_media_export_writable_db_container_constructed;
  object_class->get_property = rygel_media_export_writable_db_container_get_property;
  object_class->set_property = rygel_media_export_writable_db_container_set_property;

  g_object_class_override_property (object_class,
                                    RYGEL_MEDIA_EXPORT_WRITABLE_DB_CONTAINER_CREATE_CLASSES,
                                    "create-classes");
  g_type_class_add_private (writable_db_container_class,
                            sizeof (RygelMediaExportWritableDbContainerPrivate));
}

static void
rygel_media_export_writable_db_container_rygel_writable_container_interface_init (RygelWritableContainerIface *iface) {
  iface->add_item = rygel_media_export_writable_db_container_real_add_item;
  iface->add_item_finish = rygel_media_export_writable_db_container_real_add_item_finish;
  iface->remove_item = rygel_media_export_writable_db_container_real_remove_item;
  iface->remove_item_finish = rygel_media_export_writable_db_container_real_remove_item_finish;
  iface->get_create_classes = rygel_media_export_writable_db_container_real_get_create_classes;
  iface->set_create_classes = rygel_media_export_writable_db_container_real_set_create_classes;
}

static void
rygel_media_export_writable_db_container_init (RygelMediaExportWritableDbContainer *self) {
  self->priv = RYGEL_MEDIA_EXPORT_WRITABLE_DB_CONTAINER_GET_PRIVATE (self);
}
