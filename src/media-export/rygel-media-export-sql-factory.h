/*
 * Copyright (C) 2010, 2011 Jens Georg <mail@jensge.org>.
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

#ifndef __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_SQL_FACTORY_H__
#define __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_SQL_FACTORY_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define RYGEL_MEDIA_EXPORT_TYPE_SQL_FACTORY (rygel_media_export_sql_factory_get_type ())
#define RYGEL_MEDIA_EXPORT_SQL_FACTORY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RYGEL_MEDIA_EXPORT_TYPE_SQL_FACTORY, RygelMediaExportSQLFactory))
#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RYGEL_MEDIA_EXPORT_TYPE_SQL_FACTORY, RygelMediaExportSQLFactoryClass))
#define RYGEL_MEDIA_EXPORT_IS_SQL_FACTORY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RYGEL_MEDIA_EXPORT_TYPE_SQL_FACTORY))
#define RYGEL_MEDIA_EXPORT_IS_SQL_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RYGEL_MEDIA_EXPORT_TYPE_SQL_FACTORY))
#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RYGEL_MEDIA_EXPORT_TYPE_SQL_FACTORY, RygelMediaExportSQLFactoryClass))

typedef struct _RygelMediaExportSQLFactory RygelMediaExportSQLFactory;
typedef struct _RygelMediaExportSQLFactoryClass RygelMediaExportSQLFactoryClass;
typedef struct _RygelMediaExportSQLFactoryPrivate RygelMediaExportSQLFactoryPrivate;

struct _RygelMediaExportSQLFactory {
  GObject parent_instance;
  RygelMediaExportSQLFactoryPrivate *priv;
};

struct _RygelMediaExportSQLFactoryClass {
  GObjectClass parent_class;
};

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_SCHEMA_VERSION "12"

typedef enum  {
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_TYPE,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_TITLE,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_SIZE,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_MIME_TYPE,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_WIDTH,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_HEIGHT,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_CLASS,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_AUTHOR,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_ALBUM,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_DATE,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_BITRATE,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_SAMPLE_FREQ,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_BITS_PER_SAMPLE,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_CHANNELS,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_TRACK,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_COLOR_DEPTH,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_DURATION,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_ID,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_PARENT,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_TIMESTAMP,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_URI,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_DLNA_PROFILE,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_GENRE,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_DISC,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_OBJECT_UPDATE_ID,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_DELETED_CHILD_COUNT,
  RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_CONTAINER_UPDATE_ID
} RygelMediaExportDetailColumn;

typedef enum  {
  RYGEL_MEDIA_EXPORT_SQL_STRING_SAVE_METADATA,
  RYGEL_MEDIA_EXPORT_SQL_STRING_INSERT,
  RYGEL_MEDIA_EXPORT_SQL_STRING_DELETE,
  RYGEL_MEDIA_EXPORT_SQL_STRING_GET_OBJECT,
  RYGEL_MEDIA_EXPORT_SQL_STRING_GET_CHILDREN,
  RYGEL_MEDIA_EXPORT_SQL_STRING_GET_OBJECTS_BY_FILTER,
  RYGEL_MEDIA_EXPORT_SQL_STRING_GET_OBJECTS_BY_FILTER_WITH_ANCESTOR,
  RYGEL_MEDIA_EXPORT_SQL_STRING_GET_OBJECT_COUNT_BY_FILTER,
  RYGEL_MEDIA_EXPORT_SQL_STRING_GET_OBJECT_COUNT_BY_FILTER_WITH_ANCESTOR,
  RYGEL_MEDIA_EXPORT_SQL_STRING_GET_META_DATA_COLUMN,
  RYGEL_MEDIA_EXPORT_SQL_STRING_CHILD_COUNT,
  RYGEL_MEDIA_EXPORT_SQL_STRING_EXISTS,
  RYGEL_MEDIA_EXPORT_SQL_STRING_CHILD_IDS,
  RYGEL_MEDIA_EXPORT_SQL_STRING_TABLE_METADATA,
  RYGEL_MEDIA_EXPORT_SQL_STRING_TABLE_CLOSURE,
  RYGEL_MEDIA_EXPORT_SQL_STRING_TRIGGER_CLOSURE,
  RYGEL_MEDIA_EXPORT_SQL_STRING_TRIGGER_COMMON,
  RYGEL_MEDIA_EXPORT_SQL_STRING_INDEX_COMMON,
  RYGEL_MEDIA_EXPORT_SQL_STRING_SCHEMA,
  RYGEL_MEDIA_EXPORT_SQL_STRING_EXISTS_CACHE,
  RYGEL_MEDIA_EXPORT_SQL_STRING_STATISTICS,
  RYGEL_MEDIA_EXPORT_SQL_STRING_RESET_TOKEN,
  RYGEL_MEDIA_EXPORT_SQL_STRING_MAX_UPDATE_ID
} RygelMediaExportSQLString;

GType
rygel_media_export_sql_factory_get_type (void) G_GNUC_CONST;

RygelMediaExportSQLFactory *
rygel_media_export_sql_factory_new (void);

const gchar *
rygel_media_export_sql_factory_make (RygelMediaExportSQLFactory *self,
                                     RygelMediaExportSQLString   query);

G_END_DECLS

#endif /* __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_SQL_FACTORY_H__ */
