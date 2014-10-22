/*
 * Copyright (C) 2012 Jens Georg <mail@jensge.org>.
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

#include "rygel-media-export-music-item.h"
#include "rygel-media-export-media-cache.h"

static void
rygel_media_export_music_item_rygel_updatable_object_interface_init (RygelUpdatableObjectIface *iface);

static void
rygel_media_export_music_item_rygel_trackable_item_interface_init (RygelTrackableItemIface *iface);

G_DEFINE_TYPE_WITH_CODE (RygelMediaExportMusicItem,
                         rygel_media_export_music_item,
                         RYGEL_TYPE_MUSIC_ITEM,
                         G_IMPLEMENT_INTERFACE (RYGEL_TYPE_UPDATABLE_OBJECT,
                                                rygel_media_export_music_item_rygel_updatable_object_interface_init)
                         G_IMPLEMENT_INTERFACE (RYGEL_TYPE_TRACKABLE_ITEM,
                                                rygel_media_export_music_item_rygel_trackable_item_interface_init))

static void rygel_media_export_music_item_real_commit (RygelUpdatableObject *base, GAsyncReadyCallback callback_, gpointer user_data);

/* TODO: Remove the construct function? */
RygelMediaExportMusicItem *rygel_media_export_music_item_construct (GType object_type, const gchar *id, RygelMediaContainer *parent, const gchar *title, const gchar *upnp_class) {
  g_return_val_if_fail (id, NULL);
  g_return_val_if_fail (parent, NULL);
  g_return_val_if_fail (title, NULL);
  g_return_val_if_fail (upnp_class, NULL);

  return RYGEL_MEDIA_EXPORT_MUSIC_ITEM (rygel_music_item_construct (object_type, id, parent, title, upnp_class));
}

RygelMediaExportMusicItem*
rygel_media_export_music_item_new (const gchar *id, RygelMediaContainer *parent, const gchar *title, const gchar *upnp_class) {
  return rygel_media_export_music_item_construct (RYGEL_MEDIA_EXPORT_TYPE_MUSIC_ITEM, id, parent, title, upnp_class);
}

static void
rygel_media_export_music_item_real_commit (RygelUpdatableObject *base, GAsyncReadyCallback callback, gpointer user_data) {
  RygelMediaExportMusicItem *self = RYGEL_MEDIA_EXPORT_MUSIC_ITEM (base);

  g_return_if_fail (self);
  g_return_if_fail (callback);

  rygel_trackable_item_changed (RYGEL_TRACKABLE_ITEM (self));
  /* Setup the async result.
   */
  GSimpleAsyncResult *async_result =
    g_simple_async_result_new (G_OBJECT (self), callback, user_data,
      rygel_media_export_music_item_real_commit);

  /* Do the work that could take a while.
   */
  GError *error = NULL;
  RygelMediaExportMediaCache *cache = rygel_media_export_media_cache_get_default ();

  rygel_media_export_media_cache_save_item (cache, RYGEL_MEDIA_ITEM (self), &error);

  /* Set any error in the async result, if necessary.
   */
  if (error) {
    g_simple_async_result_set_from_error (async_result, error);
    g_error_free (error);
  }

  /* Let the caller know that the async operation is finished,
   * and that its result is now available.
   */
  g_simple_async_result_complete (async_result);

  /* Free our data structure. */
  g_object_unref (async_result);
  g_object_unref (cache);
}

static void
rygel_media_export_music_item_real_commit_finish (RygelUpdatableObject *base G_GNUC_UNUSED, GAsyncResult *result, GError **error) {
  g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error);
}

static void
rygel_media_export_music_item_class_init (RygelMediaExportMusicItemClass *klass G_GNUC_UNUSED) {
}

static void
rygel_media_export_music_item_rygel_updatable_object_interface_init (RygelUpdatableObjectIface *iface) {
  iface->commit = rygel_media_export_music_item_real_commit;
  iface->commit_finish = rygel_media_export_music_item_real_commit_finish;
}

static void
rygel_media_export_music_item_rygel_trackable_item_interface_init (RygelTrackableItemIface *iface G_GNUC_UNUSED) {
}

static void
rygel_media_export_music_item_init (RygelMediaExportMusicItem *self G_GNUC_UNUSED) {
}
