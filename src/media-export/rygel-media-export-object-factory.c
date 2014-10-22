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

#include "rygel-media-export-db-container.h"
#include "rygel-media-export-music-item.h"
#include "rygel-media-export-photo-item.h"
#include "rygel-media-export-query-container-factory.h"
#include "rygel-media-export-query-container.h"
#include "rygel-media-export-root-container.h"
#include "rygel-media-export-object-factory.h"
#include "rygel-media-export-trackable-db-container.h"
#include "rygel-media-export-video-item.h"
#include "rygel-media-export-writable-db-container.h"

G_DEFINE_TYPE (RygelMediaExportObjectFactory, rygel_media_export_object_factory, G_TYPE_OBJECT)

static RygelMediaExportDBContainer* rygel_media_export_object_factory_real_get_container (RygelMediaExportObjectFactory* self, const gchar* id, const gchar* title, guint child_count, const gchar* uri);
static RygelMediaItem* rygel_media_export_object_factory_real_get_item (RygelMediaExportObjectFactory* self, RygelMediaContainer* parent, const gchar* id, const gchar* title, const gchar* upnp_class);

/**
 * Return a new instance of DBContainer
 *
 * @param media_db instance of MediaDB
 * @param title title of the container
 * @param child_count number of children in the container
 */
static RygelMediaExportDBContainer *
rygel_media_export_object_factory_real_get_container (RygelMediaExportObjectFactory *self G_GNUC_UNUSED,
                                                      const gchar                   *id,
                                                      const gchar                   *title,
                                                      guint                          child_count G_GNUC_UNUSED,
                                                      const gchar                   *uri) {
  g_return_val_if_fail (id != NULL, NULL);
  g_return_val_if_fail (title != NULL, NULL);

  if (g_strcmp0 (id, "0") == 0) {
    return RYGEL_MEDIA_EXPORT_DB_CONTAINER (rygel_media_export_root_container_get_instance ());
  }

  if (!g_strcmp0 (id, RYGEL_MEDIA_EXPORT_ROOT_CONTAINER_FILESYSTEM_FOLDER_ID)) {
    RygelMediaExportRootContainer *root_container = RYGEL_MEDIA_EXPORT_ROOT_CONTAINER (rygel_media_export_root_container_get_instance ());
    RygelMediaExportDBContainer *container = RYGEL_MEDIA_EXPORT_DB_CONTAINER (rygel_media_export_root_container_get_filesystem_container (root_container));

    g_object_unref (root_container);
    return container;
  }

  if (g_str_has_prefix (id, RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_PREFIX)) {
    RygelMediaExportQueryContainerFactory* factory = rygel_media_export_query_container_factory_get_default ();
    RygelMediaExportDBContainer *container = RYGEL_MEDIA_EXPORT_DB_CONTAINER (rygel_media_export_query_container_factory_create_from_id (factory, id, title));

    g_object_unref (factory);
    return container;
  }

  if (!uri) {
    return RYGEL_MEDIA_EXPORT_DB_CONTAINER (rygel_media_export_trackable_db_container_new (id, title));
  }

  return RYGEL_MEDIA_EXPORT_DB_CONTAINER (rygel_media_export_writable_db_container_new (id, title));
}

RygelMediaExportDBContainer *
rygel_media_export_object_factory_get_container (RygelMediaExportObjectFactory *self,
                                                 const gchar                   *id,
                                                 const gchar                   *title,
                                                 guint                          child_count,
                                                 const gchar                   *uri) {
  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_OBJECT_FACTORY (self), NULL);

  return RYGEL_MEDIA_EXPORT_OBJECT_FACTORY_GET_CLASS (self)->get_container (self, id, title, child_count, uri);
}

/**
 * Return a new instance of MediaItem
 *
 * @param id id of the item
 * @param title title of the item
 * @param upnp_class upnp_class of the item
 */
static RygelMediaItem *
rygel_media_export_object_factory_real_get_item (RygelMediaExportObjectFactory *self G_GNUC_UNUSED,
                                                 RygelMediaContainer *parent,
                                                 const gchar         *id,
                                                 const gchar         *title,
                                                 const gchar         *upnp_class) {
  g_return_val_if_fail (RYGEL_IS_MEDIA_CONTAINER (parent), NULL);
  g_return_val_if_fail (id != NULL, NULL);
  g_return_val_if_fail (title != NULL, NULL);
  g_return_val_if_fail (upnp_class != NULL, NULL);

  if (!g_strcmp0 (upnp_class, RYGEL_MUSIC_ITEM_UPNP_CLASS) || !g_strcmp0 (upnp_class, RYGEL_AUDIO_ITEM_UPNP_CLASS)) {
    return RYGEL_MEDIA_ITEM (rygel_media_export_music_item_new (id, parent, title, RYGEL_MUSIC_ITEM_UPNP_CLASS));
  }
  if (!g_strcmp0 (upnp_class, RYGEL_VIDEO_ITEM_UPNP_CLASS)) {
    return RYGEL_MEDIA_ITEM (rygel_media_export_video_item_new (id, parent, title, RYGEL_VIDEO_ITEM_UPNP_CLASS));
  }
  if (!g_strcmp0 (upnp_class, RYGEL_PHOTO_ITEM_UPNP_CLASS) || !g_strcmp0 (upnp_class, RYGEL_IMAGE_ITEM_UPNP_CLASS)) {
    return RYGEL_MEDIA_ITEM (rygel_media_export_photo_item_new (id, parent, title, RYGEL_PHOTO_ITEM_UPNP_CLASS));
  }
  g_assert_not_reached ();
}

RygelMediaItem *
rygel_media_export_object_factory_get_item (RygelMediaExportObjectFactory *self,
                                            RygelMediaContainer           *parent,
                                            const gchar                   *id,
                                            const gchar                   *title,
                                            const gchar                   *upnp_class) {
  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_OBJECT_FACTORY (self), NULL);

  return RYGEL_MEDIA_EXPORT_OBJECT_FACTORY_GET_CLASS (self)->get_item (self, parent, id, title, upnp_class);
}

RygelMediaExportObjectFactory *
rygel_media_export_object_factory_new (void) {
  return RYGEL_MEDIA_EXPORT_OBJECT_FACTORY (g_object_new (RYGEL_MEDIA_EXPORT_TYPE_OBJECT_FACTORY, NULL));
}

static void
rygel_media_export_object_factory_class_init (RygelMediaExportObjectFactoryClass *factory_class) {


  factory_class->get_container = rygel_media_export_object_factory_real_get_container;
  factory_class->get_item = rygel_media_export_object_factory_real_get_item;
}

static void
rygel_media_export_object_factory_init (RygelMediaExportObjectFactory *self G_GNUC_UNUSED) {
}
