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

#ifndef __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_MEDIA_CACHE_H__
#define __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_MEDIA_CACHE_H__

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <rygel-server.h>
#include <gee.h>

G_BEGIN_DECLS

#define RYGEL_MEDIA_EXPORT_TYPE_MEDIA_CACHE (rygel_media_export_media_cache_get_type ())
#define RYGEL_MEDIA_EXPORT_MEDIA_CACHE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RYGEL_MEDIA_EXPORT_TYPE_MEDIA_CACHE, RygelMediaExportMediaCache))
#define RYGEL_MEDIA_EXPORT_MEDIA_CACHE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RYGEL_MEDIA_EXPORT_TYPE_MEDIA_CACHE, RygelMediaExportMediaCacheClass))
#define RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RYGEL_MEDIA_EXPORT_TYPE_MEDIA_CACHE))
#define RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RYGEL_MEDIA_EXPORT_TYPE_MEDIA_CACHE))
#define RYGEL_MEDIA_EXPORT_MEDIA_CACHE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RYGEL_MEDIA_EXPORT_TYPE_MEDIA_CACHE, RygelMediaExportMediaCacheClass))

typedef struct _RygelMediaExportMediaCache RygelMediaExportMediaCache;
typedef struct _RygelMediaExportMediaCacheClass RygelMediaExportMediaCacheClass;
typedef struct _RygelMediaExportMediaCachePrivate RygelMediaExportMediaCachePrivate;

struct _RygelMediaExportMediaCache {
  GObject parent_instance;
  RygelMediaExportMediaCachePrivate *priv;
};

struct _RygelMediaExportMediaCacheClass {
  GObjectClass parent_class;
};

GType
rygel_media_export_media_cache_get_type (void) G_GNUC_CONST;

RygelMediaExportMediaCache *
rygel_media_export_media_cache_new (GError **error);

gchar *
rygel_media_export_media_cache_get_id (GFile *file);

gboolean
rygel_media_export_media_cache_ensure_exists (GError **error);

RygelMediaExportMediaCache *
rygel_media_export_media_cache_get_default (void);

void
rygel_media_export_media_cache_remove_by_id (RygelMediaExportMediaCache  *self,
                                             const gchar                 *id,
                                             GError                     **error);

void
rygel_media_export_media_cache_remove_object (RygelMediaExportMediaCache  *self,
                                              RygelMediaObject            *object,
                                              GError                     **error);

void
rygel_media_export_media_cache_save_container (RygelMediaExportMediaCache  *self,
                                               RygelMediaContainer         *container,
                                               GError                     **error);

void
rygel_media_export_media_cache_save_item (RygelMediaExportMediaCache  *self,
                                          RygelMediaItem              *item,
                                          GError                     **error);

RygelMediaObject *
rygel_media_export_media_cache_get_object (RygelMediaExportMediaCache  *self,
                                           const gchar                 *object_id,
                                           GError                     **error);

RygelMediaContainer *
rygel_media_export_media_cache_get_container (RygelMediaExportMediaCache  *self,
                                              const gchar                 *container_id,
                                              GError                     **error);

gint
rygel_media_export_media_cache_get_child_count (RygelMediaExportMediaCache  *self,
                                                const gchar                 *container_id,
                                                GError                     **error);

guint32
rygel_media_export_media_cache_get_update_id (RygelMediaExportMediaCache *self);

void
rygel_media_export_media_cache_get_track_properties (RygelMediaExportMediaCache *self,
                                                     const gchar                *id,
                                                     guint32                    *object_update_id,
                                                     guint32                    *container_update_id,
                                                     guint32                    *total_deleted_child_count);

gboolean
rygel_media_export_media_cache_exists (RygelMediaExportMediaCache  *self,
                                       GFile                       *file,
                                       gint64                      *timestamp,
                                       gint64                      *size,
                                       GError                     **error);

RygelMediaObjects *
rygel_media_export_media_cache_get_children (RygelMediaExportMediaCache  *self,
                                             RygelMediaContainer         *container,
                                             const gchar                 *sort_criteria,
                                             glong                        offset,
                                             glong                        max_count,
                                             GError                     **error);

RygelMediaObjects *
rygel_media_export_media_cache_get_objects_by_search_expression (RygelMediaExportMediaCache  *self,
                                                                 RygelSearchExpression       *expression,
                                                                 const gchar                 *container_id,
                                                                 const gchar                 *sort_criteria,
                                                                 guint                        offset,
                                                                 guint                        max_count,
                                                                 guint                       *total_matches,
                                                                 GError                     **error);

glong
rygel_media_export_media_cache_get_object_count_by_filter (RygelMediaExportMediaCache  *self,
                                                           const gchar                 *filter,
                                                           GArray                      *args,
                                                           const gchar                 *container_id,
                                                           GError                     **error);

RygelMediaObjects *
rygel_media_export_media_cache_get_objects_by_filter (RygelMediaExportMediaCache *self,
                                                      const gchar                *filter,
                                                      GArray                     *args,
                                                      const gchar                *container_id,
                                                      const gchar                *sort_criteria,
                                                      glong                       offset,
                                                      glong                       max_count,
                                                      GError                    **error);

glong
rygel_media_export_media_cache_get_object_count_by_search_expression (RygelMediaExportMediaCache  *self,
                                                                      RygelSearchExpression       *expression,
                                                                      const gchar                 *container_id,
                                                                      GError                     **error);

void
rygel_media_export_media_cache_debug_statistics (RygelMediaExportMediaCache *self);

GeeArrayList *
rygel_media_export_media_cache_get_child_ids (RygelMediaExportMediaCache  *self,
                                              const gchar                 *container_id,
                                              GError                     **error);

GeeList *
rygel_media_export_media_cache_get_meta_data_column_by_filter (RygelMediaExportMediaCache  *self,
                                                               const gchar                 *column,
                                                               const gchar                 *filter,
                                                               GArray                      *args,
                                                               glong                        offset,
                                                               glong                        max_count,
                                                               GError                     **error);

GeeList *
rygel_media_export_media_cache_get_object_attribute_by_search_expression (RygelMediaExportMediaCache  *self,
                                                                          const gchar                 *attribute,
                                                                          RygelSearchExpression       *expression,
                                                                          glong                        offset,
                                                                          guint                        max_count,
                                                                          GError                     **error);

void
rygel_media_export_media_cache_flag_object (RygelMediaExportMediaCache  *self,
                                            GFile                       *file,
                                            const gchar                 *flag,
                                            GError                     **error);

GeeList *
rygel_media_export_media_cache_get_flagged_uris (RygelMediaExportMediaCache  *self,
                                                 const gchar                 *flag,
                                                 GError                     **error);

gchar *
rygel_media_export_media_cache_get_reset_token (RygelMediaExportMediaCache *self);

void
rygel_media_export_media_cache_save_reset_token (RygelMediaExportMediaCache *self,
                                                 const gchar                *token);

G_END_DECLS

#endif /* __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_MEDIA_CACHE_H__ */
