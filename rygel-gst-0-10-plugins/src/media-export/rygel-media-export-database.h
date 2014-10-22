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

#ifndef __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_DATABASE_H__
#define __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_DATABASE_H__

#include <glib.h>
#include <glib-object.h>

#include "rygel-media-export-database-cursor.h"
#include "rygel-media-export-sqlite-wrapper.h"

G_BEGIN_DECLS

#define RYGEL_MEDIA_EXPORT_TYPE_DATABASE (rygel_media_export_database_get_type ())
#define RYGEL_MEDIA_EXPORT_DATABASE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RYGEL_MEDIA_EXPORT_TYPE_DATABASE, RygelMediaExportDatabase))
#define RYGEL_MEDIA_EXPORT_DATABASE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RYGEL_MEDIA_EXPORT_TYPE_DATABASE, RygelMediaExportDatabaseClass))
#define RYGEL_MEDIA_EXPORT_IS_DATABASE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RYGEL_MEDIA_EXPORT_TYPE_DATABASE))
#define RYGEL_MEDIA_EXPORT_IS_DATABASE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RYGEL_MEDIA_EXPORT_TYPE_DATABASE))
#define RYGEL_MEDIA_EXPORT_DATABASE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RYGEL_MEDIA_EXPORT_TYPE_DATABASE, RygelMediaExportDatabaseClass))

typedef struct _RygelMediaExportDatabase RygelMediaExportDatabase;
typedef struct _RygelMediaExportDatabaseClass RygelMediaExportDatabaseClass;
typedef struct _RygelMediaExportDatabasePrivate RygelMediaExportDatabasePrivate;

struct _RygelMediaExportDatabase {
  RygelMediaExportSqliteWrapper parent_instance;
  RygelMediaExportDatabasePrivate *priv;
};

struct _RygelMediaExportDatabaseClass {
  RygelMediaExportSqliteWrapperClass parent_class;
};

GType
rygel_media_export_database_get_type (void) G_GNUC_CONST;

RygelMediaExportDatabase *
rygel_media_export_database_new (const gchar* name,
                                 GError** error);

void
rygel_media_export_database_exec (RygelMediaExportDatabase  *self,
                                  const gchar               *sql,
                                  GValue                    *arguments,
                                  int                        arguments_length,
                                  GError                   **error);

RygelMediaExportDatabaseCursor *
rygel_media_export_database_exec_cursor (RygelMediaExportDatabase  *self,
                                         const gchar               *sql,
                                         GValue                    *arguments,
                                         int                        arguments_length,
                                         GError                   **error);

gint
rygel_media_export_database_query_value (RygelMediaExportDatabase  *self,
                                         const gchar               *sql,
                                         GValue                    *args,
                                         int                        args_length,
                                         GError                   **error);

void
rygel_media_export_database_analyze (RygelMediaExportDatabase *self);

void
rygel_media_export_database_null (GValue *result);

void
rygel_media_export_database_begin (RygelMediaExportDatabase  *self,
                                   GError                   **error);

void
rygel_media_export_database_commit (RygelMediaExportDatabase  *self,
                                    GError                   **error);

void
rygel_media_export_database_rollback (RygelMediaExportDatabase* self);


G_END_DECLS

#endif /* __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_DATABASE_H__ */
