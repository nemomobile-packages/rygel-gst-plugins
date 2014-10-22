/*
 * Copyright (C) 2009,2010 Jens Georg <mail@jensge.org>.
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

#ifndef __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_DATABASE_CURSOR_H__
#define __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_DATABASE_CURSOR_H__

#include <glib.h>
#include <glib-object.h>
#include <sqlite3.h>

#include "rygel-media-export-sqlite-wrapper.h"

G_BEGIN_DECLS

#define RYGEL_MEDIA_EXPORT_TYPE_DATABASE_CURSOR (rygel_media_export_database_cursor_get_type ())
#define RYGEL_MEDIA_EXPORT_DATABASE_CURSOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RYGEL_MEDIA_EXPORT_TYPE_DATABASE_CURSOR, RygelMediaExportDatabaseCursor))
#define RYGEL_MEDIA_EXPORT_DATABASE_CURSOR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RYGEL_MEDIA_EXPORT_TYPE_DATABASE_CURSOR, RygelMediaExportDatabaseCursorClass))
#define RYGEL_MEDIA_EXPORT_IS_DATABASE_CURSOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RYGEL_MEDIA_EXPORT_TYPE_DATABASE_CURSOR))
#define RYGEL_MEDIA_EXPORT_IS_DATABASE_CURSOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RYGEL_MEDIA_EXPORT_TYPE_DATABASE_CURSOR))
#define RYGEL_MEDIA_EXPORT_DATABASE_CURSOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RYGEL_MEDIA_EXPORT_TYPE_DATABASE_CURSOR, RygelMediaExportDatabaseCursorClass))

typedef struct _RygelMediaExportDatabaseCursor RygelMediaExportDatabaseCursor;
typedef struct _RygelMediaExportDatabaseCursorClass RygelMediaExportDatabaseCursorClass;
typedef struct _RygelMediaExportDatabaseCursorPrivate RygelMediaExportDatabaseCursorPrivate;

struct _RygelMediaExportDatabaseCursor {
  RygelMediaExportSqliteWrapper parent_instance;
  RygelMediaExportDatabaseCursorPrivate *priv;
};

struct _RygelMediaExportDatabaseCursorClass {
  RygelMediaExportSqliteWrapperClass parent_class;
};

GType
rygel_media_export_database_cursor_get_type (void) G_GNUC_CONST;

RygelMediaExportDatabaseCursor*
rygel_media_export_database_cursor_new (sqlite3      *db,
                                        const gchar  *sql,
                                        GValue       *arguments,
                                        int           arguments_length,
                                        GError      **error);

gboolean
rygel_media_export_database_cursor_has_next (RygelMediaExportDatabaseCursor *self);

sqlite3_stmt *
rygel_media_export_database_cursor_next (RygelMediaExportDatabaseCursor  *self,
                                         GError                         **error);



G_END_DECLS

#endif /* __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_DATABASE_CURSOR_H__ */
