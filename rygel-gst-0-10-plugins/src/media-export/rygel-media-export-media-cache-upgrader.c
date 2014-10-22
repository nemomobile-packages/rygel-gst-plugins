/*
 * Copyright (C) 2010 Jens Georg <mail@jensge.org>.
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
#include <gee.h>

#include "rygel-media-export-errors.h"
#include "rygel-media-export-media-cache-upgrader.h"
#include "rygel-media-export-query-container.h"
#include "rygel-media-export-root-container.h"
#include "rygel-media-export-sql-factory.h"
#include "rygel-media-export-uuid.h"

G_DEFINE_TYPE (RygelMediaExportMediaCacheUpgrader, rygel_media_export_media_cache_upgrader, G_TYPE_OBJECT)

struct _RygelMediaExportMediaCacheUpgraderPrivate {
  RygelMediaExportDatabase* database;
  RygelMediaExportSQLFactory* sql;
};

#define RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), RYGEL_MEDIA_EXPORT_TYPE_MEDIA_CACHE_UPGRADER, RygelMediaExportMediaCacheUpgraderPrivate))
enum  {
  RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER_DUMMY_PROPERTY,
  RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER_DATABASE,
  RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER_SQL_FACTORY
};

#define RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER_UPDATE_V3_V4_STRING_2 "UPDATE meta_data SET object_fk = (SELECT upnp_id FROM Object WHERE metadata_fk = meta_data.id)"
#define RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER_UPDATE_V3_V4_STRING_3 "ALTER TABLE Object ADD timestamp INTEGER"
#define RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER_UPDATE_V3_V4_STRING_4 "UPDATE Object SET timestamp = 0"

RygelMediaExportMediaCacheUpgrader *
rygel_media_export_media_cache_upgrader_new (RygelMediaExportDatabase   *database,
                                             RygelMediaExportSQLFactory *sql) {
  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_DATABASE (database), NULL);
  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_SQL_FACTORY (sql), NULL);

  return RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER (g_object_new (RYGEL_MEDIA_EXPORT_TYPE_MEDIA_CACHE_UPGRADER,
                                                                "database", database,
                                                                "sql-factory", sql,
                                                                NULL));
}

gboolean
rygel_media_export_media_cache_upgrader_needs_upgrade (RygelMediaExportMediaCacheUpgrader  *self,
                                                       gint                                *current_version,
                                                       GError                             **error) {
  RygelMediaExportMediaCacheUpgraderPrivate *priv;
  gint version;
  GError *inner_error;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE_UPGRADER (self), FALSE);

  priv = self->priv;
  inner_error = NULL;
  version = rygel_media_export_database_query_value (priv->database, "SELECT version FROM schema_info", NULL, 0, &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return FALSE;
  }
  if (current_version) {
    *current_version = version;
  }
  return (version < atoi (RYGEL_MEDIA_EXPORT_SQL_FACTORY_SCHEMA_VERSION));
}

void
rygel_media_export_media_cache_upgrader_fix_schema (RygelMediaExportMediaCacheUpgrader  *self,
                                                    GError                             **error) {
  RygelMediaExportMediaCacheUpgraderPrivate *priv;
  gint matching_schema_count;
  GError *inner_error;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE_UPGRADER (self));

  priv = self->priv;
  inner_error = NULL;
  matching_schema_count = rygel_media_export_database_query_value (priv->database,
                                                                   "SELECT count(*) FROM sqlite_master WHERE sql LIKE 'CREATE TABLE Meta_Data%object_fk TEXT UNIQUE%'",
                                                                   NULL,
                                                                   0,
                                                                   &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return;
  }
  if (!matching_schema_count) {
    g_message ("Found faulty schema, forcing full reindex");
    rygel_media_export_database_begin (priv->database, &inner_error);
    if (inner_error) {
      goto out;
    }
    rygel_media_export_database_exec (priv->database,
                                      "DELETE FROM Object WHERE upnp_id IN (SELECT DISTINCT object_fk FROM meta_data)",
                                      NULL,
                                      0,
                                      &inner_error);
    if (inner_error) {
      goto out;
    }
    rygel_media_export_database_exec (priv->database,
                                      "DROP TABLE Meta_Data",
                                      NULL,
                                      0,
                                      &inner_error);
    if (inner_error) {
      goto out;
    }
    rygel_media_export_database_exec (priv->database,
                                      rygel_media_export_sql_factory_make (priv->sql,
                                                                           RYGEL_MEDIA_EXPORT_SQL_STRING_TABLE_METADATA),
                                      NULL,
                                      0,
                                      &inner_error);
    if (inner_error) {
      goto out;
    }
    rygel_media_export_database_commit (priv->database, &inner_error);
    if (inner_error) {
      goto out;
    }
  out:
    if (inner_error) {
      rygel_media_export_database_rollback (priv->database);
      g_warning ("Failed to force reindex to fix database: %s", inner_error->message);
      g_error_free (inner_error);
      inner_error = NULL;
    }
  }
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return;
  }
}

void
rygel_media_export_media_cache_upgrader_ensure_indices (RygelMediaExportMediaCacheUpgrader *self) {
  GError *inner_error;
  RygelMediaExportMediaCacheUpgraderPrivate *priv;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE_UPGRADER (self));

  inner_error = NULL;
  priv = self->priv;
  rygel_media_export_database_exec (priv->database,
                                    rygel_media_export_sql_factory_make (priv->sql,
                                                                         RYGEL_MEDIA_EXPORT_SQL_STRING_INDEX_COMMON),
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    g_warning ("Failed to create indices: %s", inner_error->message);
    g_error_free (inner_error);
    return;
  }
  rygel_media_export_database_analyze (priv->database);
}

static void
rygel_media_export_media_cache_upgrader_force_reindex (RygelMediaExportMediaCacheUpgrader  *self,
                                                       GError                            **error) {
  GError *inner_error;
  RygelMediaExportMediaCacheUpgraderPrivate *priv;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE_UPGRADER (self));

  priv = self->priv;
  inner_error = NULL;
  rygel_media_export_database_exec (priv->database,
                                    "UPDATE Object SET timestamp = 0",
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return;
  }
}

static void
rygel_media_export_media_cache_upgrader_update_v3_v4 (RygelMediaExportMediaCacheUpgrader *self) {
  GError *inner_error;
  RygelMediaExportMediaCacheUpgraderPrivate *priv;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE_UPGRADER (self));

  priv = self->priv;
  inner_error = NULL;
  rygel_media_export_database_begin (priv->database, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database,
                                    "ALTER TABLE Meta_Data RENAME TO _Meta_Data",
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database,
                                    rygel_media_export_sql_factory_make (priv->sql,
                                                                         RYGEL_MEDIA_EXPORT_SQL_STRING_TABLE_METADATA),
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database,
                                    "INSERT INTO meta_data (size, mime_type, duration, width, height, class, author, album, date, bitrate, "
                                    "sample_freq, bits_per_sample, channels, track, color_depth, object_fk) SELECT size, mime_type, duration, "
                                    "width, height, class, author, album, date, bitrate, sample_freq, bits_per_sample, channels, track, "
                                    "color_depth, o.upnp_id FROM _Meta_Data JOIN object o ON id = o.metadata_fk",
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "DROP TABLE _Meta_Data", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER_UPDATE_V3_V4_STRING_3, NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER_UPDATE_V3_V4_STRING_4, NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database,
                                    rygel_media_export_sql_factory_make (priv->sql,
                                                                         RYGEL_MEDIA_EXPORT_SQL_STRING_TRIGGER_COMMON),
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "UPDATE schema_info SET version = '4'", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_commit (priv->database, &inner_error);
  if (inner_error) {
    goto out;
  }
 out:
  if (inner_error) {
    rygel_media_export_database_rollback (priv->database);
    g_warning ("Database upgrade failed: %s", inner_error->message);
    priv->database = NULL;
    g_error_free (inner_error);
  }
}

static void
rygel_media_export_media_cache_upgrader_update_v4_v5 (RygelMediaExportMediaCacheUpgrader *self) {
  GeeQueue* queue;
  GeeCollection *collection;
  GError *inner_error;
  RygelMediaExportMediaCacheUpgraderPrivate *priv;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE_UPGRADER (self));

  queue = NULL;
  inner_error = NULL;
  priv = self->priv;
  rygel_media_export_database_begin (priv->database, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "DROP TRIGGER IF EXISTS trgr_delete_children", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database,
                                    rygel_media_export_sql_factory_make (priv->sql,
                                                                         RYGEL_MEDIA_EXPORT_SQL_STRING_TABLE_CLOSURE),
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "ALTER TABLE Object RENAME TO _Object", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "CREATE TABLE Object AS SELECT * FROM _Object", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "DELETE FROM Object", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database,
                                    rygel_media_export_sql_factory_make (priv->sql,
                                                                         RYGEL_MEDIA_EXPORT_SQL_STRING_TRIGGER_CLOSURE),
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database,
                                    "INSERT INTO _Object (upnp_id, type_fk, title, timestamp) VALUES ('0', 0, 'Root', 0)",
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database,
                                    "INSERT INTO Object (upnp_id, type_fk, title, timestamp) VALUES ('0', 0, 'Root', 0)",
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }

  queue = GEE_QUEUE (gee_linked_list_new (G_TYPE_STRING, (GBoxedCopyFunc) g_strdup, g_free, NULL, NULL, NULL));
  collection = GEE_COLLECTION (queue);
  gee_queue_offer (queue, "0");
  while (!gee_collection_get_is_empty (collection)) {
    GValue value = G_VALUE_INIT;
    RygelMediaExportDatabaseCursor *cursor;

    g_value_init (&value, G_TYPE_STRING);
    g_value_take_string (&value, gee_queue_poll (queue));

    cursor = rygel_media_export_database_exec_cursor (priv->database,
                                                      "SELECT upnp_id FROM _Object WHERE parent = ?",
                                                      &value,
                                                      1,
                                                      &inner_error);
    if (inner_error) {
      g_value_unset (&value);
      goto out;
    }

    while (rygel_media_export_database_cursor_has_next (cursor)) {
      sqlite3_stmt *stmt = rygel_media_export_database_cursor_next (cursor, &inner_error);

      if (inner_error) {
        g_value_unset (&value);
        goto out;
      }
      gee_queue_offer (queue, sqlite3_column_text (stmt, 0));
    }
    g_object_unref (cursor);
    rygel_media_export_database_exec (priv->database,
                                      "INSERT INTO Object SELECT * FROM _OBJECT WHERE parent = ?",
                                      &value,
                                      1,
                                      &inner_error);
    g_value_unset (&value);
    if (inner_error) {
      goto out;
    }
  }
  g_object_unref (queue);
  queue = NULL;
  rygel_media_export_database_exec (priv->database, "DROP TABLE Object", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "ALTER TABLE _Object RENAME TO Object", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database,
                                    rygel_media_export_sql_factory_make (priv->sql,
                                                                         RYGEL_MEDIA_EXPORT_SQL_STRING_TRIGGER_CLOSURE),
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database,
                                    rygel_media_export_sql_factory_make (priv->sql,
                                                                         RYGEL_MEDIA_EXPORT_SQL_STRING_INDEX_COMMON),
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "UPDATE schema_info SET version = '5'", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_commit (priv->database, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "VACUUM", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_analyze (priv->database);
 out:
  if (inner_error) {
    rygel_media_export_database_rollback (priv->database);
    g_warning ("Database upgrade failed: %s", inner_error->message);
    priv->database = NULL;
    g_error_free (inner_error);
    if (queue) {
        g_object_unref (queue);
    }
  }
}

static void rygel_media_export_media_cache_upgrader_update_v5_v6 (RygelMediaExportMediaCacheUpgrader* self) {
  GError *inner_error;
  RygelMediaExportMediaCacheUpgraderPrivate *priv;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE_UPGRADER (self));

  priv = self->priv;
  inner_error = NULL;
  rygel_media_export_database_begin (priv->database, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "DROP TABLE object_type", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "DROP TRIGGER IF EXISTS trgr_delete_uris", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "ALTER TABLE Object ADD COLUMN uri TEXT", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "UPDATE Object SET uri = (SELECT uri FROM uri WHERE Uri.object_fk == Object.upnp_id LIMIT 1)", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "DROP INDEX IF EXISTS idx_uri_fk", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "DROP TABLE Uri", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "UPDATE schema_info SET version = '6'", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_commit (priv->database, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "VACUUM", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_analyze (priv->database);
 out:
  if (inner_error) {
    rygel_media_export_database_rollback (priv->database);
    g_warning ("Database upgrade failed: %s", inner_error->message);
    priv->database = NULL;
    g_error_free (inner_error);
  }
}


static void
rygel_media_export_media_cache_upgrader_update_v6_v7 (RygelMediaExportMediaCacheUpgrader *self) {
  GError *inner_error;
  RygelMediaExportMediaCacheUpgraderPrivate *priv;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER (self));

  inner_error = NULL;
  priv = self->priv;
  rygel_media_export_database_begin (priv->database, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "ALTER TABLE meta_data ADD COLUMN dlna_profile TEXT", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "UPDATE schema_info SET version = '7'", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_media_cache_upgrader_force_reindex (self, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_commit (priv->database, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "VACUUM", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_analyze (priv->database);
 out:
  if (inner_error) {
    rygel_media_export_database_rollback (priv->database);
    g_warning ("Database upgrade failed: %s", inner_error->message);
    priv->database = NULL;
    g_error_free (inner_error);
  }
}

static void
rygel_media_export_media_cache_upgrader_update_v7_v8 (RygelMediaExportMediaCacheUpgrader *self) {
  GError *inner_error;
  RygelMediaExportMediaCacheUpgraderPrivate *priv;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER (self));

  priv = self->priv;
  inner_error = NULL;
  rygel_media_export_database_begin (priv->database, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "ALTER TABLE object ADD COLUMN flags TEXT", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "ALTER TABLE meta_data ADD COLUMN genre TEXT", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "UPDATE schema_info SET version = '8'", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_media_cache_upgrader_force_reindex (self, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_commit (priv->database, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "VACUUM", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_analyze (priv->database);
 out:
  if (inner_error) {
    rygel_media_export_database_rollback (priv->database);
    g_warning ("Database upgrade failed: %s", inner_error->message);
    priv->database = NULL;
    g_error_free (inner_error);
  }
}

static void
rygel_media_export_media_cache_upgrader_update_v8_v9 (RygelMediaExportMediaCacheUpgrader *self) {
  GError *inner_error;
  RygelMediaExportMediaCacheUpgraderPrivate *priv;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE_UPGRADER (self));

  inner_error = NULL;
  priv = self->priv;
  rygel_media_export_database_begin (priv->database, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "DROP TRIGGER trgr_update_closure", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "DROP TRIGGER trgr_delete_closure", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "ALTER TABLE Closure RENAME TO _Closure", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database,
                                    rygel_media_export_sql_factory_make (priv->sql,
                                                                         RYGEL_MEDIA_EXPORT_SQL_STRING_TABLE_CLOSURE),
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "INSERT INTO Closure (ancestor, descendant, depth) SELECT DISTINCT ancestor, descendant, depth FROM _Closure", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database,
                                    rygel_media_export_sql_factory_make (priv->sql,
                                                                         RYGEL_MEDIA_EXPORT_SQL_STRING_TRIGGER_CLOSURE),
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "DROP TABLE _Closure", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "UPDATE schema_info SET version = '9'", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_commit (priv->database, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "VACUUM", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
 out:
  if (inner_error) {
    rygel_media_export_database_rollback (priv->database);
    g_warning ("Database upgrade failed: %s", inner_error->message);
    priv->database = NULL;
    g_error_free (inner_error);
  }
}

static void
rygel_media_export_media_cache_upgrader_update_v9_v10 (RygelMediaExportMediaCacheUpgrader *self) {
  GError *inner_error;
  RygelMediaExportMediaCacheUpgraderPrivate *priv;
  GeeQueue *queue;
  GeeCollection *collection;
  gchar *sql_string;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE_UPGRADER (self));

  queue = NULL;
  priv = self->priv;
  inner_error = NULL;

  rygel_media_export_database_begin (priv->database, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "DELETE FROM Object WHERE upnp_id LIKE '" RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_PREFIX "%'", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "DROP TRIGGER trgr_update_closure", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "DROP TRIGGER trgr_delete_closure", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "DROP INDEX idx_parent", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "DROP INDEX idx_meta_data_fk", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "DROP INDEX IF EXISTS idx_closure", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "DROP TABLE Closure", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "DROP TRIGGER trgr_delete_metadata", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  sql_string = g_strconcat ("INSERT OR REPLACE INTO Object (parent, upnp_id, type_fk, title, timestamp) VALUES ('0', '"
                            RYGEL_MEDIA_EXPORT_ROOT_CONTAINER_FILESYSTEM_FOLDER_ID
                            "', 0, '",
                            _(RYGEL_MEDIA_EXPORT_ROOT_CONTAINER_FILESYSTEM_FOLDER_NAME),
                            "', 0)",
                            NULL);
  rygel_media_export_database_exec (priv->database, sql_string, NULL, 0, &inner_error);
  g_free (sql_string);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database,
                                    "UPDATE Object SET parent = '"
                                    RYGEL_MEDIA_EXPORT_ROOT_CONTAINER_FILESYSTEM_FOLDER_ID
                                    "' WHERE parent = '0' AND upnp_id NOT LIKE 'virtual-%' AND upnp_id <> '"
                                    RYGEL_MEDIA_EXPORT_ROOT_CONTAINER_FILESYSTEM_FOLDER_ID
                                    "'",
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "ALTER TABLE Object RENAME TO _Object", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "CREATE TABLE Object AS SELECT * FROM _Object", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "DELETE FROM Object", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database,
                                    rygel_media_export_sql_factory_make (priv->sql,
                                                                         RYGEL_MEDIA_EXPORT_SQL_STRING_TABLE_CLOSURE),
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database,
                                    rygel_media_export_sql_factory_make (priv->sql,
                                                                         RYGEL_MEDIA_EXPORT_SQL_STRING_TRIGGER_CLOSURE),
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "INSERT INTO Closure (ancestor, descendant, depth) VALUES ('0','0',0)", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }

  queue = GEE_QUEUE (gee_linked_list_new (G_TYPE_STRING, (GBoxedCopyFunc) g_strdup, g_free, NULL, NULL, NULL));
  collection = GEE_COLLECTION (queue);

  gee_queue_offer (queue, "0");
  while (!gee_collection_get_is_empty (collection)) {
    GValue value = G_VALUE_INIT;
    RygelMediaExportDatabaseCursor *cursor;

    g_value_init (&value, G_TYPE_STRING);
    g_value_take_string (&value, gee_queue_poll (queue));
    cursor = rygel_media_export_database_exec_cursor (priv->database, "SELECT upnp_id FROM _Object WHERE parent = ?", &value, 1, &inner_error);
    if (inner_error) {
      g_value_unset (&value);
      goto out;
    }
    while (rygel_media_export_database_cursor_has_next (cursor)) {
      sqlite3_stmt* statement = rygel_media_export_database_cursor_next (cursor, &inner_error);

      if (inner_error) {
        g_object_unref (cursor);
        g_value_unset (&value);
        goto out;
      }
      gee_queue_offer (queue, sqlite3_column_text (statement, 0));
    }
    g_object_unref (cursor);
    rygel_media_export_database_exec (priv->database,
                                      "INSERT INTO Object SELECT * FROM _Object WHERE parent = ?",
                                      &value,
                                      1,
                                      &inner_error);
    g_value_unset (&value);
    if (inner_error) {
      goto out;
    }
  }
  g_object_unref (queue);
  queue = NULL;
  rygel_media_export_database_exec (priv->database, "DROP TABLE Object", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "ALTER TABLE _Object RENAME TO Object", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database,
                                    rygel_media_export_sql_factory_make (priv->sql,
                                                                         RYGEL_MEDIA_EXPORT_SQL_STRING_INDEX_COMMON),
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database,
                                    rygel_media_export_sql_factory_make (priv->sql,
                                                                         RYGEL_MEDIA_EXPORT_SQL_STRING_TRIGGER_COMMON),
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database,
                                    rygel_media_export_sql_factory_make (priv->sql,
                                                                         RYGEL_MEDIA_EXPORT_SQL_STRING_TRIGGER_CLOSURE),
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "UPDATE schema_info SET version = '10'", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_commit (priv->database, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "VACUUM", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_analyze (priv->database);
 out:
  if (inner_error) {
    rygel_media_export_database_rollback (priv->database);
    g_warning ("Database upgrade failed: %s", inner_error->message);
    priv->database = NULL;
    g_error_free (inner_error);
    if (queue) {
      g_object_unref (queue);
    }
  }
}

static void
rygel_media_export_media_cache_upgrader_update_v10_v11 (RygelMediaExportMediaCacheUpgrader* self) {
  GError *inner_error;
  RygelMediaExportMediaCacheUpgraderPrivate *priv;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE_UPGRADER (self));

  inner_error = NULL;
  priv = self->priv;
  rygel_media_export_database_begin (priv->database, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "ALTER TABLE Meta_Data    ADD COLUMN disc INTEGER", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "UPDATE Object SET timestamp = 0 WHERE   upnp_id IN (SELECT object_fk FROM Meta_Data WHERE   class LIKE 'object.item.audioItem.%')", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "UPDATE schema_info SET version = '11'", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_commit (priv->database, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_exec (priv->database, "VACUUM", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_analyze (priv->database);
 out:
  if (inner_error) {
    rygel_media_export_database_rollback (priv->database);
    g_warning ("Database upgrade failed: %s", inner_error->message);
    priv->database = NULL;
    g_error_free (inner_error);
  }
}

static void
rygel_media_export_media_cache_upgrader_update_v11_v12 (RygelMediaExportMediaCacheUpgrader* self) {
  GError *inner_error;
  RygelMediaExportDatabase *db;
  gchar *uu;
  gchar *sql;
  guint32 count;
  GList *ids;
  GList *iter;
  RygelMediaExportDatabaseCursor *cursor;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE_UPGRADER (self));

  inner_error = NULL;
  db = self->priv->database;
  rygel_media_export_database_begin (db, &inner_error);
  if (inner_error) {
    goto out;
  }

  rygel_media_export_database_exec (db,
                                    "ALTER TABLE schema_info ADD COLUMN reset_token TEXT",
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }

  uu = rygel_media_export_uuid_get ();
  sql = g_strdup_printf ("UPDATE schema_info SET reset_token = '%s'", uu);
  g_free (uu);
  rygel_media_export_database_exec (db,
                                    sql,
                                    NULL,
                                    0,
                                    &inner_error);
  g_free (sql);
  if (inner_error) {
    goto out;
  }

  rygel_media_export_database_exec (db,
                                    "UPDATE schema_info SET version = '12'",
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }

  rygel_media_export_database_exec (db,
                                    "ALTER TABLE object ADD COLUMN object_update_id INTEGER",
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }

  rygel_media_export_database_exec (db,
                                    "ALTER TABLE object ADD COLUMN deleted_child_count INTEGER",
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }

  rygel_media_export_database_exec (db,
                                    "ALTER TABLE object ADD COLUMN container_update_id INTEGER",
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }

  cursor = rygel_media_export_database_exec_cursor (db,
                                                    "SELECT upnp_id FROM object",
                                                    NULL,
                                                    0,
                                                    &inner_error);
  if (inner_error) {
    goto out;
  }

  ids = NULL;
  while (rygel_media_export_database_cursor_has_next (cursor)) {
    sqlite3_stmt *statement = rygel_media_export_database_cursor_next (cursor,
                                                                       &inner_error);

    if (inner_error) {
      g_object_unref (cursor);
      g_list_free_full (ids, g_free);
      goto out;
    }
    ids = g_list_prepend (ids, g_strdup ((const gchar *) sqlite3_column_text (statement, 0)));
  }
  g_object_unref (cursor);
  ids = g_list_reverse (ids);
  count = 1;

  for (iter = ids; iter; iter = iter->next) {
    GValue values[] = {G_VALUE_INIT,
                       G_VALUE_INIT,
                       G_VALUE_INIT};
    guint value_iter;

    g_value_init (&(values[0]), G_TYPE_UINT);
    g_value_set_uint (&(values[0]), count);

    g_value_init (&(values[1]), G_TYPE_UINT);
    g_value_set_uint (&(values[0]), count);

    g_value_init (&(values[2]), G_TYPE_STRING);
    g_value_take_string (&(values[2]), iter->data);
    iter->data = NULL;

    ++count;
    rygel_media_export_database_exec (db,
                                      "UPDATE object SET "
                                      "container_update_id = ?, "
                                      "object_update_id = ?, "
                                      "deleted_child_count = ?",
                                      values,
                                      G_N_ELEMENTS (values),
                                      &inner_error);
    for (value_iter = 0; value_iter < G_N_ELEMENTS (values); ++value_iter) {
      g_value_unset (&(values[value_iter]));
    }
    if (inner_error) {
      g_list_free_full (ids, g_free);
      goto out;
    }
  }
  /* if we get there then all strings in it were already taken by
   * GValues in loop above and freed. */
  g_list_free (ids);

  rygel_media_export_database_commit (db, &inner_error);
  if (inner_error) {
    goto out;
  }

  rygel_media_export_database_exec (db, "VACUUM", NULL, 0, &inner_error);
  if (inner_error) {
    goto out;
  }

  rygel_media_export_database_analyze (db);

 out:
  if (inner_error) {
    rygel_media_export_database_rollback (db);
    g_warning ("Database upgrade failed: %s", inner_error->message);
    g_error_free (inner_error);
    self->priv->database = NULL;
  }
}

void
rygel_media_export_media_cache_upgrader_upgrade (RygelMediaExportMediaCacheUpgrader *self,
                                                 gint                                old_version) {
  gint current_version;
  RygelMediaExportMediaCacheUpgraderPrivate *priv;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE_UPGRADER (self));

  g_debug ("Older schema detected. Upgrading...");
  priv = self->priv;
  current_version = atoi (RYGEL_MEDIA_EXPORT_SQL_FACTORY_SCHEMA_VERSION);
  while (old_version < current_version) {
    if (!priv->database) {
      break;
    }

    switch (old_version) {
    case 3:
      rygel_media_export_media_cache_upgrader_update_v3_v4 (self);
      break;
    case 4:
      rygel_media_export_media_cache_upgrader_update_v4_v5 (self);
      break;
    case 5:
      rygel_media_export_media_cache_upgrader_update_v5_v6 (self);
      break;
    case 6:
      rygel_media_export_media_cache_upgrader_update_v6_v7 (self);
      break;
    case 7:
      rygel_media_export_media_cache_upgrader_update_v7_v8 (self);
      break;
    case 8:
      rygel_media_export_media_cache_upgrader_update_v8_v9 (self);
      break;
    case 9:
      rygel_media_export_media_cache_upgrader_update_v9_v10 (self);
      break;
    case 10:
      rygel_media_export_media_cache_upgrader_update_v10_v11 (self);
      break;
    case 11:
      rygel_media_export_media_cache_upgrader_update_v11_v12 (self);
      break;
    default:
      g_warning ("Cannot upgrade");
      priv->database = NULL;
      break;
    }
    ++old_version;
  }
}

static void
rygel_media_export_media_cache_upgrader_get_property (GObject    *object,
                                                      guint       property_id,
                                                      GValue     *value,
                                                      GParamSpec *pspec) {
  RygelMediaExportMediaCacheUpgrader *self = RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER (object);
  RygelMediaExportMediaCacheUpgraderPrivate *priv = self->priv;

  switch (property_id) {
  case RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER_DATABASE:
    g_value_set_object (value, priv->database);
    break;

  case RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER_SQL_FACTORY:
    g_value_set_object (value, priv->sql);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
rygel_media_export_media_cache_upgrader_set_property (GObject      *object,
                                                      guint         property_id,
                                                      const GValue *value,
                                                      GParamSpec   *pspec) {
  RygelMediaExportMediaCacheUpgrader *self = RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER (object);
  RygelMediaExportMediaCacheUpgraderPrivate *priv = self->priv;

  switch (property_id) {
  case RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER_DATABASE:
    /* construct only property, unowned */
    priv->database = g_value_get_object (value);
    break;

  case RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER_SQL_FACTORY:
    /* construct only property, unowned */
    priv->sql = g_value_get_object (value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
rygel_media_export_media_cache_upgrader_class_init (RygelMediaExportMediaCacheUpgraderClass *upgrader_class) {
  GObjectClass *object_class = G_OBJECT_CLASS (upgrader_class);

  object_class->get_property = rygel_media_export_media_cache_upgrader_get_property;
  object_class->set_property = rygel_media_export_media_cache_upgrader_set_property;

  g_object_class_install_property (object_class,
                                   RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER_DATABASE,
                                   g_param_spec_object ("database",
                                                        "database",
                                                        "database",
                                                        RYGEL_MEDIA_EXPORT_TYPE_DATABASE,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER_SQL_FACTORY,
                                   g_param_spec_object ("sql-factory",
                                                        "sql-factory",
                                                        "sql-factory",
                                                        RYGEL_MEDIA_EXPORT_TYPE_SQL_FACTORY,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (upgrader_class, sizeof (RygelMediaExportMediaCacheUpgraderPrivate));
}

static void
rygel_media_export_media_cache_upgrader_init (RygelMediaExportMediaCacheUpgrader *self) {
  self->priv = RYGEL_MEDIA_EXPORT_MEDIA_CACHE_UPGRADER_GET_PRIVATE (self);
}
