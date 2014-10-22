/*
 * Copyright (C) 2009 Jens Georg <mail@jensge.org>.
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

#include "rygel-media-export-null-container.h"

/**
 * This is an empty container used to satisfy rygel if no mediadb could be
 * created
 */

G_DEFINE_TYPE (RygelMediaExportNullContainer,
               rygel_media_export_null_container,
               RYGEL_TYPE_MEDIA_CONTAINER)

RygelMediaExportNullContainer *
rygel_media_export_null_container_new (void) {
  return RYGEL_MEDIA_EXPORT_NULL_CONTAINER (g_object_new (RYGEL_MEDIA_EXPORT_TYPE_NULL_CONTAINER,
                                                          "id", "0",
                                                          "parent", NULL,
                                                          "title", "MediaExport",
                                                          "child-count", 0,
                                                          NULL));
}

static void
rygel_media_export_null_container_real_get_children (RygelMediaContainer *base,
                                                     guint                offset G_GNUC_UNUSED,
                                                     guint                max_count G_GNUC_UNUSED,
                                                     const gchar         *sort_criteria G_GNUC_UNUSED,
                                                     GCancellable        *cancellable G_GNUC_UNUSED,
                                                     GAsyncReadyCallback  callback,
                                                     gpointer             user_data) {
  GSimpleAsyncResult *simple = g_simple_async_result_new (G_OBJECT (base),
                                                          callback,
                                                          user_data,
                                                          rygel_media_export_null_container_real_get_children);

  g_simple_async_result_set_op_res_gpointer (simple,
                                             rygel_media_objects_new (),
                                             g_object_unref);
  g_simple_async_result_complete_in_idle (simple);
  g_object_unref (simple);
}

static RygelMediaObjects *
rygel_media_export_null_container_real_get_children_finish (RygelMediaContainer  *base G_GNUC_UNUSED,
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
rygel_media_export_null_container_real_find_object (RygelMediaContainer *base,
                                                    const gchar         *id G_GNUC_UNUSED,
                                                    GCancellable        *cancellable G_GNUC_UNUSED,
                                                    GAsyncReadyCallback  callback,
                                                    gpointer             user_data) {
  GSimpleAsyncResult *simple = g_simple_async_result_new (G_OBJECT (base),
                                                          callback,
                                                          user_data,
                                                          rygel_media_export_null_container_real_find_object);

  g_simple_async_result_complete_in_idle (simple);
  g_object_unref (simple);
}

static RygelMediaObject *
rygel_media_export_null_container_real_find_object_finish (RygelMediaContainer  *base G_GNUC_UNUSED,
                                                           GAsyncResult         *res,
                                                           GError              **error) {
  g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
  return NULL;
}

static void rygel_media_export_null_container_class_init (RygelMediaExportNullContainerClass *null_class) {
  RygelMediaContainerClass *container_class = RYGEL_MEDIA_CONTAINER_CLASS (null_class);

  container_class->get_children = rygel_media_export_null_container_real_get_children;
  container_class->get_children_finish = rygel_media_export_null_container_real_get_children_finish;
  container_class->find_object = rygel_media_export_null_container_real_find_object;
  container_class->find_object_finish = rygel_media_export_null_container_real_find_object_finish;
}

static void rygel_media_export_null_container_init (RygelMediaExportNullContainer *self G_GNUC_UNUSED) {
}
