/*
 * Copyright (C) 2011 Jens Georg <mail@jensge.org>.
 * Copyright (C) 2012, 2013 Intel Corporation.
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


#include "rygel-media-export-sqlite-wrapper.h"
#include "rygel-media-export-errors.h"

G_DEFINE_TYPE (RygelMediaExportSqliteWrapper,
               rygel_media_export_sqlite_wrapper,
               G_TYPE_OBJECT)

struct _RygelMediaExportSqliteWrapperPrivate {
  sqlite3 *database;
  sqlite3 *reference;
};


#define RYGEL_MEDIA_EXPORT_SQLITE_WRAPPER_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                RYGEL_MEDIA_EXPORT_TYPE_SQLITE_WRAPPER, \
                                RygelMediaExportSqliteWrapperPrivate))

enum  {
  RYGEL_MEDIA_EXPORT_SQLITE_WRAPPER_DUMMY_PROPERTY,
  RYGEL_MEDIA_EXPORT_SQLITE_WRAPPER_DB,
  RYGEL_MEDIA_EXPORT_SQLITE_WRAPPER_OWN_DB
};

/**
 * Convert a SQLite return code to a DatabaseError
 */
void
rygel_media_export_sqlite_wrapper_throw_if_code_is_error (RygelMediaExportSqliteWrapper  *self,
                                                          gint                            sqlite_error,
                                                          GError                        **error) {
  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_SQLITE_WRAPPER (self));

  switch (sqlite_error) {
  case SQLITE_OK:
  case SQLITE_DONE:
  case SQLITE_ROW:
    {
      return;
    }
  default:
    {
      g_set_error (error,
                   RYGEL_MEDIA_EXPORT_DATABASE_ERROR,
                   RYGEL_MEDIA_EXPORT_DATABASE_ERROR_SQLITE_ERROR,
                   "SQLite error %d (%d): %s",
                   sqlite_error,
                   sqlite3_extended_errcode (self->priv->reference),
                   sqlite3_errmsg (self->priv->reference));
    }
  }
}

/**
 * Check if the last operation on the database was an error
 */
void
rygel_media_export_sqlite_wrapper_throw_if_db_has_error (RygelMediaExportSqliteWrapper  *self,
                                                         GError                        **error) {
  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_SQLITE_WRAPPER (self));

  rygel_media_export_sqlite_wrapper_throw_if_code_is_error (self,
                                                            sqlite3_errcode (self->priv->reference),
                                                            error);
}

sqlite3 *
rygel_media_export_sqlite_wrapper_get_db (RygelMediaExportSqliteWrapper *self) {
  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_SQLITE_WRAPPER (self), NULL);

  return self->priv->reference;
}

static void
rygel_media_export_sqlite_wrapper_finalize (GObject *object) {
  RygelMediaExportSqliteWrapper *self = RYGEL_MEDIA_EXPORT_SQLITE_WRAPPER (object);

  sqlite3_close (self->priv->database);

  G_OBJECT_CLASS (rygel_media_export_sqlite_wrapper_parent_class)->finalize (object);
}

static void
rygel_media_export_sqlite_wrapper_get_property (GObject    *object,
                                                guint       property_id,
                                                GValue     *value,
                                                GParamSpec *pspec) {
  RygelMediaExportSqliteWrapper *self = RYGEL_MEDIA_EXPORT_SQLITE_WRAPPER (object);
  RygelMediaExportSqliteWrapperPrivate *priv = self->priv;

  switch (property_id) {
  case RYGEL_MEDIA_EXPORT_SQLITE_WRAPPER_DB:
    g_value_set_pointer (value, priv->reference);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
rygel_media_export_sqlite_wrapper_set_property (GObject      *object,
                                                guint         property_id,
                                                const GValue *value,
                                                GParamSpec   *pspec) {
  RygelMediaExportSqliteWrapper *self = RYGEL_MEDIA_EXPORT_SQLITE_WRAPPER (object);
  RygelMediaExportSqliteWrapperPrivate *priv = self->priv;

  switch (property_id) {
  case RYGEL_MEDIA_EXPORT_SQLITE_WRAPPER_DB:
    priv->reference = g_value_get_pointer (value);
    break;

  case RYGEL_MEDIA_EXPORT_SQLITE_WRAPPER_OWN_DB:
    priv->database = g_value_get_pointer (value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
rygel_media_export_sqlite_wrapper_class_init (RygelMediaExportSqliteWrapperClass *wrapper_class) {
  GObjectClass *object_class = G_OBJECT_CLASS (wrapper_class);

  object_class->get_property = rygel_media_export_sqlite_wrapper_get_property;
  object_class->set_property = rygel_media_export_sqlite_wrapper_set_property;
  object_class->finalize = rygel_media_export_sqlite_wrapper_finalize;
  /**
   * Property to access the wrapped database
   */
  g_object_class_install_property (object_class,
                                   RYGEL_MEDIA_EXPORT_SQLITE_WRAPPER_DB,
                                   g_param_spec_pointer ("db",
                                                         "db",
                                                         "db",
                                                         G_PARAM_STATIC_NAME |
                                                         G_PARAM_STATIC_NICK |
                                                         G_PARAM_STATIC_BLURB |
                                                         G_PARAM_READABLE |
                                                         G_PARAM_WRITABLE |
                                                         G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
                                   RYGEL_MEDIA_EXPORT_SQLITE_WRAPPER_OWN_DB,
                                   g_param_spec_pointer ("own-db",
                                                         "own-db",
                                                         "own-db",
                                                         G_PARAM_STATIC_NAME |
                                                         G_PARAM_STATIC_NICK |
                                                         G_PARAM_STATIC_BLURB |
                                                         G_PARAM_WRITABLE |
                                                         G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (wrapper_class, sizeof (RygelMediaExportSqliteWrapperPrivate));
}

static void
rygel_media_export_sqlite_wrapper_init (RygelMediaExportSqliteWrapper *self) {
  self->priv = RYGEL_MEDIA_EXPORT_SQLITE_WRAPPER_GET_PRIVATE (self);
}
