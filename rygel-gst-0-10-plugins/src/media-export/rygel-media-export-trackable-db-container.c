/*
 * Copyright (C) 2012, 2013 Intel Corporation.
 *
 * Author: Jens Georg <jensg@openismus.com>
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

#include "rygel-media-export-trackable-db-container.h"

static void
rygel_media_export_trackable_db_container_rygel_trackable_container_interface_init (RygelTrackableContainerIface *iface);

G_DEFINE_TYPE_WITH_CODE (RygelMediaExportTrackableDbContainer,
                         rygel_media_export_trackable_db_container,
                         RYGEL_MEDIA_EXPORT_TYPE_DB_CONTAINER,
                         G_IMPLEMENT_INTERFACE (RYGEL_TYPE_TRACKABLE_CONTAINER,
                                                rygel_media_export_trackable_db_container_rygel_trackable_container_interface_init))

RygelMediaExportTrackableDbContainer *
rygel_media_export_trackable_db_container_new (const gchar *id,
                                               const gchar *title) {
  g_return_val_if_fail (id != NULL, NULL);
  g_return_val_if_fail (title != NULL, NULL);

  return RYGEL_MEDIA_EXPORT_TRACKABLE_DB_CONTAINER (g_object_new (RYGEL_MEDIA_EXPORT_TYPE_TRACKABLE_DB_CONTAINER,
                                                                  "id", id,
                                                                  "title", title,
                                                                  "parent", NULL,
                                                                  "child_count", 0,
                                                                  NULL));
}

static void
rygel_media_export_trackable_db_container_on_child_added (RygelMediaExportTrackableDbContainer *self,
                                                          RygelMediaObject                     *object) {
  GError *inner_error;
  RygelMediaExportMediaCache *cache;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_TRACKABLE_DB_CONTAINER (self));
  g_return_if_fail (RYGEL_IS_MEDIA_OBJECT (object));

  inner_error = NULL;
  cache = rygel_media_export_db_container_get_media_db (RYGEL_MEDIA_EXPORT_DB_CONTAINER (self));
  if (RYGEL_IS_MEDIA_ITEM (object)) {
    rygel_media_export_media_cache_save_item (cache,
                                              RYGEL_MEDIA_ITEM (object),
                                              &inner_error);
    if (inner_error) {
      goto out;
    }
  } else if (RYGEL_IS_MEDIA_CONTAINER (object)) {
    rygel_media_export_media_cache_save_container (cache,
                                                   RYGEL_MEDIA_CONTAINER (object),
                                                   &inner_error);
    if (inner_error) {
      goto out;
    }
  } else {
    g_assert_not_reached ();
  }
  rygel_media_export_media_cache_save_container (cache,
                                                 RYGEL_MEDIA_CONTAINER (self),
                                                 &inner_error);
 out:
  if (inner_error) {
    g_warning ("Failed to save object: %s", inner_error->message);
    g_error_free (inner_error);
  }
}

static void
rygel_media_export_trackable_db_container_on_child_removed (RygelMediaExportTrackableDbContainer *self,
                                                            RygelMediaObject                     *object G_GNUC_UNUSED) {
  GError *inner_error;
  RygelMediaExportMediaCache *cache;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_TRACKABLE_DB_CONTAINER (self));

  inner_error = NULL;
  cache = rygel_media_export_db_container_get_media_db (RYGEL_MEDIA_EXPORT_DB_CONTAINER (self));

  rygel_media_export_media_cache_save_container (cache,
                                                 RYGEL_MEDIA_CONTAINER (self),
                                                 &inner_error);
  if (inner_error) {
    g_warning ("Failed to save object: %s", inner_error->message);
    g_error_free (inner_error);
  }
}

static void
rygel_media_export_trackable_db_container_real_add_child (RygelTrackableContainer *base,
                                                          RygelMediaObject        *object,
                                                          GAsyncReadyCallback      callback,
                                                          gpointer                 user_data) {
  RygelMediaExportDBContainer *self = RYGEL_MEDIA_EXPORT_DB_CONTAINER (base);
  RygelMediaExportMediaCache *cache = rygel_media_export_db_container_get_media_db (self);
  GError *inner_error = NULL;
  GSimpleAsyncResult *async_result = g_simple_async_result_new (G_OBJECT (base),
                                                                callback,
                                                                user_data,
                                                                rygel_media_export_trackable_db_container_real_add_child);

  if (RYGEL_IS_MEDIA_ITEM (object)) {
    rygel_media_export_media_cache_save_item (cache,
                                              RYGEL_MEDIA_ITEM (object),
                                              &inner_error);
  } else if (RYGEL_IS_MEDIA_CONTAINER (object)) {
    rygel_media_export_media_cache_save_container (cache,
                                                   RYGEL_MEDIA_CONTAINER (object),
                                                   &inner_error);
  } else {
    g_assert_not_reached ();
  }
  if (inner_error) {
    g_warning ("Failed to add object: %s", inner_error->message);
    g_error_free (inner_error);
  }
  g_simple_async_result_complete_in_idle (async_result);
  g_object_unref (async_result);
}

static void
rygel_media_export_trackable_db_container_real_add_child_finish (RygelTrackableContainer *base G_GNUC_UNUSED,
                                                                 GAsyncResult            *res G_GNUC_UNUSED) {
}

static void
rygel_media_export_trackable_db_container_real_remove_child (RygelTrackableContainer *base,
                                                             RygelMediaObject        *object,
                                                             GAsyncReadyCallback      callback,
                                                             gpointer                 user_data) {
  RygelMediaExportDBContainer *self = RYGEL_MEDIA_EXPORT_DB_CONTAINER (base);
  GError *inner_error = NULL;
  RygelMediaExportMediaCache *cache = rygel_media_export_db_container_get_media_db (self);
  GSimpleAsyncResult *async_result = g_simple_async_result_new (G_OBJECT (self),
                                                                callback,
                                                                user_data,
                                                                rygel_media_export_trackable_db_container_real_remove_child);

  rygel_media_export_media_cache_remove_object (cache, object, &inner_error);

  if (inner_error) {
    g_warning ("Failed to remove object: %s", inner_error->message);
    g_error_free (inner_error);
  }

  g_simple_async_result_complete_in_idle (async_result);
  g_object_unref (async_result);
}

static void
rygel_media_export_trackable_db_container_real_remove_child_finish (RygelTrackableContainer *base G_GNUC_UNUSED,
                                                                    GAsyncResult            *res G_GNUC_UNUSED) {
}

static gchar *
rygel_media_export_trackable_db_container_real_get_service_reset_token (RygelTrackableContainer *self) {
  RygelMediaExportMediaCache* cache = rygel_media_export_db_container_get_media_db (RYGEL_MEDIA_EXPORT_DB_CONTAINER (self));

  return rygel_media_export_media_cache_get_reset_token (cache);
}

static void
rygel_media_export_trackable_db_container_real_set_service_reset_token (RygelTrackableContainer *self,
                                                                        const gchar             *token) {
  RygelMediaExportMediaCache *cache;

  g_return_if_fail (token != NULL);

  cache = rygel_media_export_db_container_get_media_db (RYGEL_MEDIA_EXPORT_DB_CONTAINER (self));
  rygel_media_export_media_cache_save_reset_token (cache, token);
}

static guint32
rygel_media_export_trackable_db_container_real_get_system_update_id (RygelTrackableContainer *self) {
  RygelMediaExportMediaCache* cache = rygel_media_export_db_container_get_media_db (RYGEL_MEDIA_EXPORT_DB_CONTAINER (self));

  return rygel_media_export_media_cache_get_update_id (cache);
}

static void
rygel_media_export_trackable_db_container_real_constructed (GObject *object) {
  RygelMediaExportTrackableDbContainer *self = RYGEL_MEDIA_EXPORT_TRACKABLE_DB_CONTAINER (object);

  G_OBJECT_CLASS (rygel_media_export_trackable_db_container_parent_class)->constructed (object);
  g_signal_connect (self,
                    "child-added",
                    G_CALLBACK (rygel_media_export_trackable_db_container_on_child_added),
                    NULL);
  g_signal_connect (self,
                    "child-removed",
                    G_CALLBACK (rygel_media_export_trackable_db_container_on_child_removed),
                    NULL);
}

static void
rygel_media_export_trackable_db_container_class_init (RygelMediaExportTrackableDbContainerClass *trackable_db_container_class) {
  GObjectClass *object_class = G_OBJECT_CLASS (trackable_db_container_class);

  object_class->constructed = rygel_media_export_trackable_db_container_real_constructed;
}

static void
rygel_media_export_trackable_db_container_rygel_trackable_container_interface_init (RygelTrackableContainerIface *iface) {
  iface->add_child = rygel_media_export_trackable_db_container_real_add_child;
  iface->add_child_finish = rygel_media_export_trackable_db_container_real_add_child_finish;
  iface->remove_child = rygel_media_export_trackable_db_container_real_remove_child;
  iface->remove_child_finish = rygel_media_export_trackable_db_container_real_remove_child_finish;
  iface->get_service_reset_token = rygel_media_export_trackable_db_container_real_get_service_reset_token;
  iface->set_service_reset_token = rygel_media_export_trackable_db_container_real_set_service_reset_token;
  iface->get_system_update_id = rygel_media_export_trackable_db_container_real_get_system_update_id;
}

static void
rygel_media_export_trackable_db_container_init (RygelMediaExportTrackableDbContainer *self G_GNUC_UNUSED) {
}
