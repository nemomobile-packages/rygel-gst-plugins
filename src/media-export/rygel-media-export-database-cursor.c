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

#include <glib/gi18n-lib.h>

#include "rygel-media-export-database-cursor.h"

G_DEFINE_TYPE (RygelMediaExportDatabaseCursor,
               rygel_media_export_database_cursor,
               RYGEL_MEDIA_EXPORT_TYPE_SQLITE_WRAPPER)

struct _RygelMediaExportDatabaseCursorPrivate {
  sqlite3_stmt *statement;
  gint current_state;
  gboolean dirty;
};

#define RYGEL_MEDIA_EXPORT_DATABASE_CURSOR_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                RYGEL_MEDIA_EXPORT_TYPE_DATABASE_CURSOR, \
                                RygelMediaExportDatabaseCursorPrivate))

/**
 * Prepare a SQLite statement from a SQL string
 *
 * This function uses the type of the GValue passed in values to determine
 * which _bind function to use.
 *
 * Supported types are: int, long, int64, uint64, string and pointer.
 * @note the only pointer supported is the null pointer as provided by
 * Database.@null. This is a special value to bind a column to NULL
 *
 * @param db SQLite database this cursor belongs to
 * @param sql statement to execute
 * @param values array of values to bind to the SQL statement or null if
 * none
 */
RygelMediaExportDatabaseCursor *
rygel_media_export_database_cursor_new (sqlite3      *db,
                                        const gchar  *sql,
                                        GValue       *arguments,
                                        int           arguments_length,
                                        GError      **error) {
  RygelMediaExportDatabaseCursor *self;
  RygelMediaExportSqliteWrapper *self_wrapper;
  GError *inner_error = NULL;
  gint iter;

  g_return_val_if_fail (db != NULL, NULL);
  g_return_val_if_fail (sql != NULL, NULL);

  self = RYGEL_MEDIA_EXPORT_DATABASE_CURSOR (g_object_new (RYGEL_MEDIA_EXPORT_TYPE_DATABASE_CURSOR,
                                                           "db", db,
                                                           NULL));
  self_wrapper = RYGEL_MEDIA_EXPORT_SQLITE_WRAPPER (self);
  rygel_media_export_sqlite_wrapper_throw_if_code_is_error (self_wrapper,
                                                            sqlite3_prepare_v2 (db, sql, -1, &self->priv->statement, NULL),
                                                            &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    g_object_unref (self);
    return NULL;
  }
  if (!arguments) {
    return self;
  }
  for (iter = 1; iter <= arguments_length; ++iter) {
    GValue *current_value = &(arguments[iter - 1]);
    sqlite3_stmt* stmt = self->priv->statement;

    if (G_VALUE_HOLDS (current_value, G_TYPE_INT)) {
      sqlite3_bind_int (stmt, iter, g_value_get_int (current_value));
    } else if (G_VALUE_HOLDS (current_value, G_TYPE_INT64)) {
      sqlite3_bind_int64 (stmt, iter, g_value_get_int64 (current_value));
    } else if (G_VALUE_HOLDS (current_value, G_TYPE_UINT64)) {
      sqlite3_bind_int64 (stmt, iter, (gint64) g_value_get_uint64 (current_value));
    } else if (G_VALUE_HOLDS (current_value, G_TYPE_LONG)) {
      sqlite3_bind_int64 (stmt, iter, (gint64) g_value_get_long (current_value));
    } else if (G_VALUE_HOLDS (current_value, G_TYPE_UINT)) {
      sqlite3_bind_int64 (stmt, iter, (gint64) g_value_get_uint (current_value));
    } else if (G_VALUE_HOLDS (current_value, G_TYPE_STRING)) {
      sqlite3_bind_text (stmt, iter, g_value_dup_string (current_value), -1, g_free);
    } else if (G_VALUE_HOLDS (current_value, G_TYPE_POINTER)) {
      if (g_value_peek_pointer (current_value) == NULL) {
        sqlite3_bind_null (stmt, iter);
      } else {
        g_assert_not_reached ();
      }
    } else {
      g_warning (_("Unsupported type %s"), G_VALUE_TYPE_NAME (current_value));
      g_assert_not_reached ();
    }

    rygel_media_export_sqlite_wrapper_throw_if_db_has_error (self_wrapper,
                                                             &inner_error);
    if (inner_error != NULL) {
      g_propagate_error (error, inner_error);
      g_object_unref (self);
      return NULL;
    }
  }

  return self;
}

/**
 * Check if the cursor has more rows left
 *
 * @return true if more rows left, false otherwise
 */
gboolean
rygel_media_export_database_cursor_has_next (RygelMediaExportDatabaseCursor *self) {
  RygelMediaExportDatabaseCursorPrivate *priv;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_DATABASE_CURSOR (self), FALSE);

  priv = self->priv;
  if (priv->dirty) {
    priv->current_state = sqlite3_step (priv->statement);
    priv->dirty = FALSE;
  }
  return ((priv->current_state == SQLITE_ROW) || (priv->current_state == -1));
}


/**
 * Get the next row of this cursor.
 *
 * This function uses pointers instead of unowned because var doesn't work
 * with unowned.
 *
 * @return a pointer to the current row
 */
sqlite3_stmt *
rygel_media_export_database_cursor_next (RygelMediaExportDatabaseCursor  *self,
                                         GError                         **error) {
  GError *inner_error = NULL;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_DATABASE_CURSOR (self), NULL);

  rygel_media_export_database_cursor_has_next (self);
  rygel_media_export_sqlite_wrapper_throw_if_code_is_error (RYGEL_MEDIA_EXPORT_SQLITE_WRAPPER (self),
                                                            self->priv->current_state,
                                                            &inner_error);
  if (inner_error != NULL) {
    g_propagate_error (error, inner_error);
    return NULL;
  }
  self->priv->dirty = TRUE;
  return self->priv->statement;
}

static void rygel_media_export_database_cursor_finalize (GObject *object) {
  RygelMediaExportDatabaseCursor *self = RYGEL_MEDIA_EXPORT_DATABASE_CURSOR (object);

  sqlite3_finalize (self->priv->statement);
  G_OBJECT_CLASS (rygel_media_export_database_cursor_parent_class)->finalize (object);
}

static void
rygel_media_export_database_cursor_class_init (RygelMediaExportDatabaseCursorClass *cursor_class) {
  GObjectClass *object_class = G_OBJECT_CLASS (cursor_class);

  object_class->finalize = rygel_media_export_database_cursor_finalize;
  g_type_class_add_private (cursor_class,
                            sizeof (RygelMediaExportDatabaseCursorPrivate));
}

static void
rygel_media_export_database_cursor_init (RygelMediaExportDatabaseCursor *self) {
  self->priv = RYGEL_MEDIA_EXPORT_DATABASE_CURSOR_GET_PRIVATE (self);
  self->priv->current_state = -1;
  self->priv->dirty = TRUE;
}
