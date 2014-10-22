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

#ifndef __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_SQLITE_WRAPPER_H__
#define __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_SQLITE_WRAPPER_H__

#include <glib.h>
#include <glib-object.h>
#include <sqlite3.h>

G_BEGIN_DECLS

#define RYGEL_MEDIA_EXPORT_TYPE_SQLITE_WRAPPER (rygel_media_export_sqlite_wrapper_get_type ())
#define RYGEL_MEDIA_EXPORT_SQLITE_WRAPPER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RYGEL_MEDIA_EXPORT_TYPE_SQLITE_WRAPPER, RygelMediaExportSqliteWrapper))
#define RYGEL_MEDIA_EXPORT_SQLITE_WRAPPER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RYGEL_MEDIA_EXPORT_TYPE_SQLITE_WRAPPER, RygelMediaExportSqliteWrapperClass))
#define RYGEL_MEDIA_EXPORT_IS_SQLITE_WRAPPER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RYGEL_MEDIA_EXPORT_TYPE_SQLITE_WRAPPER))
#define RYGEL_MEDIA_EXPORT_IS_SQLITE_WRAPPER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RYGEL_MEDIA_EXPORT_TYPE_SQLITE_WRAPPER))
#define RYGEL_MEDIA_EXPORT_SQLITE_WRAPPER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RYGEL_MEDIA_EXPORT_TYPE_SQLITE_WRAPPER, RygelMediaExportSqliteWrapperClass))

typedef struct _RygelMediaExportSqliteWrapper RygelMediaExportSqliteWrapper;
typedef struct _RygelMediaExportSqliteWrapperClass RygelMediaExportSqliteWrapperClass;
typedef struct _RygelMediaExportSqliteWrapperPrivate RygelMediaExportSqliteWrapperPrivate;

struct _RygelMediaExportSqliteWrapper {
  GObject parent_instance;
  RygelMediaExportSqliteWrapperPrivate *priv;
};

struct _RygelMediaExportSqliteWrapperClass {
  GObjectClass parent_class;
};

GType
rygel_media_export_sqlite_wrapper_get_type (void) G_GNUC_CONST;

void
rygel_media_export_sqlite_wrapper_throw_if_db_has_error (RygelMediaExportSqliteWrapper  *self,
                                                         GError                        **error);

void
rygel_media_export_sqlite_wrapper_throw_if_code_is_error (RygelMediaExportSqliteWrapper  *self,
                                                          gint                            sqlite_error,
                                                          GError                        **error);

sqlite3 *
rygel_media_export_sqlite_wrapper_get_db (RygelMediaExportSqliteWrapper *self);

G_END_DECLS

#endif /* __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_SQLITE_WRAPPER_H__ */
