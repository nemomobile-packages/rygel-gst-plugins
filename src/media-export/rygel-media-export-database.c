/*
 * Copyright (C) 2009,2011 Jens Georg <mail@jensge.org>.
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
#include <sqlite3.h>

#include "rygel-media-export-collate.h"
#include "rygel-media-export-database.h"

/**
 * This class is a thin wrapper around SQLite's database object.
 *
 * It adds statement preparation based on GValue and a cancellable exec
 * function.
 */

G_DEFINE_TYPE (RygelMediaExportDatabase,
               rygel_media_export_database,
               RYGEL_MEDIA_EXPORT_TYPE_SQLITE_WRAPPER)


/**
 * Function to implement the custom SQL function 'contains'
 */
static
void rygel_media_export_database_utf8_contains (sqlite3_context  *context,
                                                sqlite3_value   **args,
                                                int               args_length) {
  const gchar *args_1_text;
  gchar* pattern;

  g_return_if_fail (context != NULL);
  g_return_if_fail (args_length == 2);

  args_1_text = (const gchar *) sqlite3_value_text (args[1]);

  if (args_1_text == NULL) {
    sqlite3_result_int (context, 0);
    return;
  }
  pattern = g_regex_escape_string (args_1_text, -1);

  if (g_regex_match_simple (pattern, (const gchar *) sqlite3_value_text (args[0]), G_REGEX_CASELESS, 0)) {
    sqlite3_result_int (context, 1);
  } else {
    sqlite3_result_int (context, 0);
  }
  g_free (pattern);
}

/**
 * Function to implement the custom SQLite collation 'CASEFOLD'.
 *
 * Uses utf8 case-fold to compare the strings.
 */
static gint
rygel_media_export_database_utf8_collate (gint          alen G_GNUC_UNUSED,
                                          gconstpointer a,
                                          gint          blen G_GNUC_UNUSED,
                                          gconstpointer b) {
  return rygel_media_export_utf8_collate_str (a, b);
}

static void
rygel_media_export_database_utf8_contains_sqlite_user_func_callback (sqlite3_context  *context,
                                                                     int               values_length,
                                                                     sqlite3_value   **values) {
        rygel_media_export_database_utf8_contains (context, values, values_length);
}

static gint rygel_media_export_database_utf8_collate_sqlite_compare_callback (gpointer      self G_GNUC_UNUSED,
                                                                              gint          alen,
                                                                              gconstpointer a,
                                                                              gint          blen,
                                                                              gconstpointer b) {
        return rygel_media_export_database_utf8_collate (alen, a, blen, b);
}

/**
 * Open a database in the user's cache directory as defined by XDG
 *
 * @param name of the database, used to build full path
 * (<cache-dir>/rygel/<name>.db)
 */
RygelMediaExportDatabase*
rygel_media_export_database_new (const gchar  *name,
                                 GError      **error) {
  RygelMediaExportDatabase *self;
  gchar *dirname;
  gchar *dbname;
  gchar *db_file;
  sqlite3 *db;
  GError *inner_error;

  g_return_val_if_fail (name != NULL, NULL);

  dirname = g_build_filename (g_get_user_cache_dir (), "rygel", NULL);
  g_mkdir_with_parents (dirname, 0750);
  dbname = g_strdup_printf ("%s.db", name);
  db_file = g_build_filename (dirname, dbname, NULL);
  g_free (dbname);
  g_free (dirname);
  db = NULL;
  sqlite3_open (db_file, &db);
  g_free (db_file);
  sqlite3_extended_result_codes (db, 1);
  self = RYGEL_MEDIA_EXPORT_DATABASE (g_object_new (RYGEL_MEDIA_EXPORT_TYPE_DATABASE,
                                                    "db", db,
                                                    "own-db", db,
                                                    NULL));
  inner_error = NULL;
  rygel_media_export_sqlite_wrapper_throw_if_db_has_error (RYGEL_MEDIA_EXPORT_SQLITE_WRAPPER (self),
                                                           &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    g_object_unref (self);
    return NULL;
  }

  rygel_media_export_database_exec (self, "PRAGMA synchronous = OFF", NULL, 0, &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    g_object_unref (self);
    return NULL;
  }
  rygel_media_export_database_exec (self, "PRAGMA temp_store = MEMORY", NULL, 0, &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    g_object_unref (self);
    return NULL;
  }
  rygel_media_export_database_exec (self, "PRAGMA count_changes = OFF", NULL, 0, &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    g_object_unref (self);
    return NULL;
  }
  sqlite3_create_function (db,
                           "contains",
                           2,
                           SQLITE_UTF8,
                           NULL,
                           rygel_media_export_database_utf8_contains_sqlite_user_func_callback,
                           NULL,
                           NULL);
  sqlite3_create_collation (db,
                            "CASEFOLD",
                            SQLITE_UTF8,
                            NULL,
                            rygel_media_export_database_utf8_collate_sqlite_compare_callback);

  return self;
}

/**
 * SQL query function.
 *
 * Use for all queries that return a result set.
 *
 * @param sql The SQL query to run.
 * @param args Values to bind in the SQL query or null.
 * @throws DatabaseError if the underlying SQLite operation fails.
 */
RygelMediaExportDatabaseCursor*
rygel_media_export_database_exec_cursor (RygelMediaExportDatabase  *self,
                                         const gchar               *sql,
                                         GValue                    *arguments,
                                         int                        arguments_length,
                                         GError                   **error) {
  RygelMediaExportDatabaseCursor *result;
  sqlite3 *db;
  GError *inner_error;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_DATABASE (self), NULL);
  g_return_val_if_fail (sql != NULL, NULL);

  db = rygel_media_export_sqlite_wrapper_get_db (RYGEL_MEDIA_EXPORT_SQLITE_WRAPPER (self));
  inner_error = NULL;
  result = rygel_media_export_database_cursor_new (db,
                                                   sql,
                                                   arguments,
                                                   arguments_length,
                                                   &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return NULL;
  }
  return result;
}

/**
 * Simple SQL query execution function.
 *
 * Use for all queries that don't return anything.
 *
 * @param sql The SQL query to run.
 * @param args Values to bind in the SQL query or null.
 * @throws DatabaseError if the underlying SQLite operation fails.
 */
void
rygel_media_export_database_exec (RygelMediaExportDatabase  *self,
                                  const gchar               *sql,
                                  GValue                    *arguments,
                                  int                        arguments_length,
                                  GError                   **error) {
  RygelMediaExportDatabaseCursor *cursor;
  GError *inner_error;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_DATABASE (self));
  g_return_if_fail (sql != NULL);

  inner_error = NULL;
  if (!arguments) {
    RygelMediaExportSqliteWrapper* self_wrapper = RYGEL_MEDIA_EXPORT_SQLITE_WRAPPER (self);
    sqlite3* db = rygel_media_export_sqlite_wrapper_get_db (self_wrapper);

    rygel_media_export_sqlite_wrapper_throw_if_code_is_error (self_wrapper,
                                                              sqlite3_exec (db, sql, NULL, NULL, NULL),
                                                              &inner_error);
    if (inner_error) {
      g_propagate_error (error, inner_error);
    }
    return;
  }
  cursor = rygel_media_export_database_exec_cursor (self,
                                                    sql,
                                                    arguments,
                                                    arguments_length,
                                                    &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return;
  }

  while (rygel_media_export_database_cursor_has_next (cursor)) {
    rygel_media_export_database_cursor_next (cursor, &inner_error);
    if (inner_error) {
      g_propagate_error (error, inner_error);
      break;
    }
  }
  g_object_unref (cursor);
}

/**
 * Execute a SQL query that returns a single number.
 *
 * @param sql The SQL query to run.
 * @param args Values to bind in the SQL query or null.
 * @return The contents of the first row's column as an int.
 * @throws DatabaseError if the underlying SQLite operation fails.
 */
gint
rygel_media_export_database_query_value (RygelMediaExportDatabase  *self,
                                         const gchar               *sql,
                                         GValue                    *args,
                                         int                        args_length,
                                         GError                   **error) {
  gint result;
  RygelMediaExportDatabaseCursor *cursor;
  sqlite3_stmt *statement;
  GError *inner_error;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_DATABASE (self), 0);
  g_return_val_if_fail (sql != NULL, 0);

  inner_error = NULL;
  cursor = rygel_media_export_database_exec_cursor (self, sql, args, args_length, &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return 0;
  }
  statement = rygel_media_export_database_cursor_next (cursor, &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    g_object_unref (cursor);
    return 0;
  }
  result = sqlite3_column_int (statement, 0);
  g_object_unref (cursor);
  return result;
}

/**
 * Analyze triggers of database
 */
void
rygel_media_export_database_analyze (RygelMediaExportDatabase *self) {
  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_DATABASE (self));

  sqlite3_exec (rygel_media_export_sqlite_wrapper_get_db (RYGEL_MEDIA_EXPORT_SQLITE_WRAPPER (self)),
                "ANALYZE",
                NULL,
                NULL,
                NULL);
}

/**
 * Special GValue to pass to exec or exec_cursor to bind a column to
 * NULL
 */
void
rygel_media_export_database_null (GValue *result) {
  g_value_init (result, G_TYPE_POINTER);
  g_value_set_pointer (result, NULL);
}

/**
 * Start a transaction
 */
void rygel_media_export_database_begin (RygelMediaExportDatabase* self, GError** error) {
  GError *inner_error;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_DATABASE (self));

  inner_error = NULL;
  rygel_media_export_database_exec (self, "BEGIN", NULL, 0, &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
  }
}


/**
 * Commit a transaction
 */
void rygel_media_export_database_commit (RygelMediaExportDatabase* self, GError** error) {
  GError *inner_error;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_DATABASE (self));

  inner_error = NULL;
  rygel_media_export_database_exec (self,
                                    "COMMIT",
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
  }
}


/**
 * Rollback a transaction
 */
void
rygel_media_export_database_rollback (RygelMediaExportDatabase *self) {
  GError *inner_error;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_DATABASE (self));

  inner_error = NULL;
  rygel_media_export_database_exec (self, "ROLLBACK", NULL, 0, &inner_error);
  if (inner_error) {
    g_critical (_("Failed to roll back transaction: %s"), inner_error->message);
    g_error_free (inner_error);
  }
}

static void
rygel_media_export_database_class_init (RygelMediaExportDatabaseClass *database_class G_GNUC_UNUSED) {
}

static void
rygel_media_export_database_init (RygelMediaExportDatabase *self G_GNUC_UNUSED) {
}
