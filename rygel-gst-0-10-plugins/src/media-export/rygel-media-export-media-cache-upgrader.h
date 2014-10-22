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

#ifndef __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER_H__
#define __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER_H__

#include <glib.h>
#include <glib-object.h>

#include "rygel-media-export-database.h"
#include "rygel-media-export-sql-factory.h"

G_BEGIN_DECLS

#define RYGEL_MEDIA_EXPORT_TYPE_MEDIA_CACHE_UPGRADER (rygel_media_export_media_cache_upgrader_get_type ())
#define RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RYGEL_MEDIA_EXPORT_TYPE_MEDIA_CACHE_UPGRADER, RygelMediaExportMediaCacheUpgrader))
#define RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RYGEL_MEDIA_EXPORT_TYPE_MEDIA_CACHE_UPGRADER, RygelMediaExportMediaCacheUpgraderClass))
#define RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE_UPGRADER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RYGEL_MEDIA_EXPORT_TYPE_MEDIA_CACHE_UPGRADER))
#define RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE_UPGRADER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RYGEL_MEDIA_EXPORT_TYPE_MEDIA_CACHE_UPGRADER))
#define RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RYGEL_MEDIA_EXPORT_TYPE_MEDIA_CACHE_UPGRADER, RygelMediaExportMediaCacheUpgraderClass))

typedef struct _RygelMediaExportMediaCacheUpgrader RygelMediaExportMediaCacheUpgrader;
typedef struct _RygelMediaExportMediaCacheUpgraderClass RygelMediaExportMediaCacheUpgraderClass;
typedef struct _RygelMediaExportMediaCacheUpgraderPrivate RygelMediaExportMediaCacheUpgraderPrivate;

struct _RygelMediaExportMediaCacheUpgrader {
  GObject parent_instance;
  RygelMediaExportMediaCacheUpgraderPrivate * priv;
};

struct _RygelMediaExportMediaCacheUpgraderClass {
  GObjectClass parent_class;
};

GType
rygel_media_export_media_cache_upgrader_get_type (void) G_GNUC_CONST;

RygelMediaExportMediaCacheUpgrader *
rygel_media_export_media_cache_upgrader_new (RygelMediaExportDatabase   *database,
                                             RygelMediaExportSQLFactory *sql);

gboolean
rygel_media_export_media_cache_upgrader_needs_upgrade (RygelMediaExportMediaCacheUpgrader  *self,
                                                       gint                                *current_version,
                                                       GError                             **error);

void
rygel_media_export_media_cache_upgrader_fix_schema (RygelMediaExportMediaCacheUpgrader  *self,
                                                    GError                             **error);

void
rygel_media_export_media_cache_upgrader_ensure_indices (RygelMediaExportMediaCacheUpgrader *self);

void
rygel_media_export_media_cache_upgrader_upgrade (RygelMediaExportMediaCacheUpgrader *self,
                                                 gint                                old_version);

G_END_DECLS

#endif /* __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER_H__ */
