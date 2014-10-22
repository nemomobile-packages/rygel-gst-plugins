/*
 * Copyright (C) 2010, 2011 Jens Georg <mail@jensge.org>.
 * Copyright (C) 2013 Intel Corporation
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

#include "rygel-media-export-sql-factory.h"


G_DEFINE_TYPE (RygelMediaExportSQLFactory, rygel_media_export_sql_factory, G_TYPE_OBJECT)

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_SAVE_META_DATA_STRING \
  "INSERT OR REPLACE INTO meta_data " \
  "(size, mime_type, width, height, class, " \
  "author, album, date, bitrate, " \
  "sample_freq, bits_per_sample, channels, " \
  "track, color_depth, duration, object_fk, dlna_profile, genre, disc) VALUES " \
  "(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_INSERT_OBJECT_STRING \
  "INSERT OR REPLACE INTO Object " \
  "(upnp_id, title, type_fk, parent, timestamp, uri, " \
  "object_update_id, deleted_child_count, container_update_id) " \
  "VALUES (?,?,?,?,?,?,?,?,?)"

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_DELETE_BY_ID_STRING \
  "DELETE FROM Object WHERE upnp_id IN " \
  "(SELECT descendant FROM closure WHERE ancestor = ?)"

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_ALL_DETAILS_STRING \
  "o.type_fk, o.title, m.size, m.mime_type, m.width, " \
  "m.height, m.class, m.author, m.album, m.date, m.bitrate, " \
  "m.sample_freq, m.bits_per_sample, m.channels, m.track, " \
  "m.color_depth, m.duration, o.upnp_id, o.parent, o.timestamp, " \
  "o.uri, m.dlna_profile, m.genre, m.disc, o.object_update_id, " \
  "o.deleted_child_count, o.container_update_id "

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_GET_OBJECT_WITH_PATH \
  "SELECT DISTINCT " RYGEL_MEDIA_EXPORT_SQL_FACTORY_ALL_DETAILS_STRING \
  "FROM Object o " \
  "JOIN Closure c ON (o.upnp_id = c.ancestor) " \
  "LEFT OUTER JOIN meta_data m ON (o.upnp_id = m.object_fk) " \
  "WHERE c.descendant = ? ORDER BY c.depth DESC"

/**
 * This is the database query used to retrieve the children for a
 * given object.
 *
 * Sorting is as follows:
 *   - by type: containers first, then items if both are present
 *   - by upnp_class: items are sorted according to their class
 *   - by track: sorted by track
 *   - and after that alphabetically
 */
#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_GET_CHILDREN_STRING \
  "SELECT " RYGEL_MEDIA_EXPORT_SQL_FACTORY_ALL_DETAILS_STRING \
  "FROM Object o " \
  "JOIN Closure c ON (o.upnp_id = c.descendant) " \
  "LEFT OUTER JOIN meta_data m " \
  "ON c.descendant = m.object_fk " \
  "WHERE c.ancestor = ? AND c.depth = 1 %s " \
  "LIMIT ?,?"

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_GET_OBJECTS_BY_FILTER_STRING_WITH_ANCESTOR \
  "SELECT DISTINCT " RYGEL_MEDIA_EXPORT_SQL_FACTORY_ALL_DETAILS_STRING \
  "FROM Object o " \
  "JOIN Closure c ON o.upnp_id = c.descendant AND c.ancestor = ? " \
  "LEFT OUTER JOIN meta_data m " \
  "ON o.upnp_id = m.object_fk %s %s " \
  "LIMIT ?,?"

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_GET_OBJECTS_BY_FILTER_STRING \
  "SELECT DISTINCT " RYGEL_MEDIA_EXPORT_SQL_FACTORY_ALL_DETAILS_STRING \
  "FROM Object o " \
  "LEFT OUTER JOIN meta_data m " \
  "ON o.upnp_id = m.object_fk %s %s " \
  "LIMIT ?,?"

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_GET_OBJECT_COUNT_BY_FILTER_STRING_WITH_ANCESTOR \
  "SELECT COUNT(o.type_fk) FROM Object o " \
  "JOIN Closure c ON o.upnp_id = c.descendant AND c.ancestor = ? " \
  "LEFT OUTER JOIN meta_data m " \
  "ON o.upnp_id = m.object_fk %s"

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_GET_OBJECT_COUNT_BY_FILTER_STRING \
  "SELECT COUNT(1) FROM meta_data m %s"

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_CHILDREN_COUNT_STRING \
  "SELECT COUNT(upnp_id) FROM Object WHERE Object.parent = ?"

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_OBJECT_EXISTS_STRING \
  "SELECT COUNT(1), timestamp, m.size FROM Object " \
  "JOIN meta_data m ON m.object_fk = upnp_id " \
  "WHERE Object.uri = ?"

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_GET_CHILD_ID_STRING \
  "SELECT upnp_id FROM OBJECT WHERE parent = ?"

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_GET_META_DATA_COLUMN_STRING \
  "SELECT DISTINCT %s AS _column FROM meta_data AS m " \
  "WHERE _column IS NOT NULL %s ORDER BY _column COLLATE CASEFOLD " \
  "LIMIT ?,?"

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_CREATE_META_DATA_TABLE_STRING \
  "CREATE TABLE meta_data (size INTEGER NOT NULL, " \
  "mime_type TEXT NOT NULL, " \
  "dlna_profile TEXT, " \
  "duration INTEGER, " \
  "width INTEGER, " \
  "height INTEGER, " \
  "class TEXT NOT NULL, " \
  "author TEXT, " \
  "album TEXT, " \
  "genre TEXT, " \
  "date TEXT, " \
  "bitrate INTEGER, " \
  "sample_freq INTEGER, " \
  "bits_per_sample INTEGER, " \
  "channels INTEGER, " \
  "track INTEGER, " \
  "disc INTEGER, " \
  "color_depth INTEGER, " \
  "object_fk TEXT UNIQUE CONSTRAINT " \
  "object_fk_id REFERENCES Object(upnp_id) " \
  "ON DELETE CASCADE); "

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_SCHEMA_STRING \
  "CREATE TABLE schema_info (version TEXT NOT NULL, " \
  "reset_token TEXT); " \
  RYGEL_MEDIA_EXPORT_SQL_FACTORY_CREATE_META_DATA_TABLE_STRING \
  "CREATE TABLE object (parent TEXT CONSTRAINT parent_fk_id " \
  "REFERENCES Object(upnp_id), " \
  "upnp_id TEXT PRIMARY KEY, " \
  "type_fk INTEGER, " \
  "title TEXT NOT NULL, " \
  "timestamp INTEGER NOT NULL, " \
  "uri TEXT, " \
  "flags TEXT, " \
  "object_update_id INTEGER, " \
  "deleted_child_count INTEGER, " \
  "container_update_id INTEGER); " \
  "INSERT INTO schema_info (version) VALUES ('" \
  RYGEL_MEDIA_EXPORT_SQL_FACTORY_SCHEMA_VERSION "'); "

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_CREATE_CLOSURE_TABLE \
  "CREATE TABLE closure (ancestor TEXT, descendant TEXT, depth INTEGER)"

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_CREATE_CLOSURE_TRIGGER_STRING \
  "CREATE TRIGGER trgr_update_closure " \
  "AFTER INSERT ON Object " \
  "FOR EACH ROW BEGIN " \
  "SELECT RAISE(IGNORE) WHERE (SELECT COUNT(*) FROM Closure " \
  "WHERE ancestor = NEW.upnp_id " \
  "AND descendant = NEW.upnp_id " \
  "AND depth = 0) != 0; " \
  "INSERT INTO Closure (ancestor, descendant, depth) " \
  "VALUES (NEW.upnp_id, NEW.upnp_id, 0); " \
  "INSERT INTO Closure (ancestor, descendant, depth) " \
  "SELECT ancestor, NEW.upnp_id, depth + 1 FROM Closure " \
  "WHERE descendant = NEW.parent; " \
  "END; " \
  "CREATE TRIGGER trgr_delete_closure " \
  "AFTER DELETE ON Object " \
  "FOR EACH ROW BEGIN " \
  "DELETE FROM Closure WHERE descendant = OLD.upnp_id; " \
  "END;"

// these triggers emulate ON DELETE CASCADE
#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_CREATE_TRIGGER_STRING \
  "CREATE TRIGGER trgr_delete_metadata " \
  "BEFORE DELETE ON Object " \
  "FOR EACH ROW BEGIN " \
  "DELETE FROM meta_data WHERE meta_data.object_fk = OLD.upnp_id; " \
  "END;"

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_CREATE_INDICES_STRING \
  "CREATE INDEX IF NOT EXISTS idx_parent on Object(parent); " \
  "CREATE INDEX IF NOT EXISTS idx_object_upnp_id on Object(upnp_id); " \
  "CREATE INDEX IF NOT EXISTS idx_meta_data_fk on meta_data(object_fk); " \
  "CREATE INDEX IF NOT EXISTS idx_closure on Closure(descendant,depth); " \
  "CREATE INDEX IF NOT EXISTS idx_closure_descendant on Closure(descendant); " \
  "CREATE INDEX IF NOT EXISTS idx_closure_ancestor on Closure(ancestor); " \
  "CREATE INDEX IF NOT EXISTS idx_uri on Object(uri); " \
  "CREATE INDEX IF NOT EXISTS idx_meta_data_date on meta_data(date); " \
  "CREATE INDEX IF NOT EXISTS idx_meta_data_genre on meta_data(genre); " \
  "CREATE INDEX IF NOT EXISTS idx_meta_data_album on meta_data(album); " \
  "CREATE INDEX IF NOT EXISTS idx_meta_data_artist_album on " \
  "meta_data(author, album);"

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_EXISTS_CACHE_STRING \
  "SELECT m.size, o.timestamp, o.uri FROM Object o " \
  "JOIN meta_data m ON o.upnp_id = m.object_fk"

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_STATISTICS_STRING \
  "SELECT class, count(1) FROM meta_data GROUP BY class"

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_RESET_TOKEN_STRING \
  "SELECT reset_token FROM schema_info"

#define RYGEL_MEDIA_EXPORT_SQL_FACTORY_MAX_UPDATE_ID_STRING \
  "SELECT MAX(MAX(object_update_id), MAX(container_update_id)) FROM Object"

const gchar*
rygel_media_export_sql_factory_make (RygelMediaExportSQLFactory *self, RygelMediaExportSQLString query) {
  g_return_val_if_fail (self, NULL);

  switch (query) {
  case RYGEL_MEDIA_EXPORT_SQL_STRING_SAVE_METADATA:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_SAVE_META_DATA_STRING;
  case RYGEL_MEDIA_EXPORT_SQL_STRING_INSERT:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_INSERT_OBJECT_STRING;
  case RYGEL_MEDIA_EXPORT_SQL_STRING_DELETE:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_DELETE_BY_ID_STRING;
  case RYGEL_MEDIA_EXPORT_SQL_STRING_GET_OBJECT:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_GET_OBJECT_WITH_PATH;
  case RYGEL_MEDIA_EXPORT_SQL_STRING_GET_CHILDREN:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_GET_CHILDREN_STRING;
  case RYGEL_MEDIA_EXPORT_SQL_STRING_GET_OBJECTS_BY_FILTER:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_GET_OBJECTS_BY_FILTER_STRING;
  case RYGEL_MEDIA_EXPORT_SQL_STRING_GET_OBJECTS_BY_FILTER_WITH_ANCESTOR:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_GET_OBJECTS_BY_FILTER_STRING_WITH_ANCESTOR;
  case RYGEL_MEDIA_EXPORT_SQL_STRING_GET_OBJECT_COUNT_BY_FILTER:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_GET_OBJECT_COUNT_BY_FILTER_STRING;
  case RYGEL_MEDIA_EXPORT_SQL_STRING_GET_OBJECT_COUNT_BY_FILTER_WITH_ANCESTOR:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_GET_OBJECT_COUNT_BY_FILTER_STRING_WITH_ANCESTOR;
  case RYGEL_MEDIA_EXPORT_SQL_STRING_GET_META_DATA_COLUMN:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_GET_META_DATA_COLUMN_STRING;
  case RYGEL_MEDIA_EXPORT_SQL_STRING_CHILD_COUNT:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_CHILDREN_COUNT_STRING;
  case RYGEL_MEDIA_EXPORT_SQL_STRING_EXISTS:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_OBJECT_EXISTS_STRING;
  case RYGEL_MEDIA_EXPORT_SQL_STRING_CHILD_IDS:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_GET_CHILD_ID_STRING;
  case RYGEL_MEDIA_EXPORT_SQL_STRING_TABLE_METADATA:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_CREATE_META_DATA_TABLE_STRING;
  case RYGEL_MEDIA_EXPORT_SQL_STRING_TRIGGER_COMMON:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_CREATE_TRIGGER_STRING;
  case RYGEL_MEDIA_EXPORT_SQL_STRING_TRIGGER_CLOSURE:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_CREATE_CLOSURE_TRIGGER_STRING;
  case RYGEL_MEDIA_EXPORT_SQL_STRING_INDEX_COMMON:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_CREATE_INDICES_STRING;
  case RYGEL_MEDIA_EXPORT_SQL_STRING_SCHEMA:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_SCHEMA_STRING;
  case RYGEL_MEDIA_EXPORT_SQL_STRING_EXISTS_CACHE:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_EXISTS_CACHE_STRING;
  case RYGEL_MEDIA_EXPORT_SQL_STRING_TABLE_CLOSURE:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_CREATE_CLOSURE_TABLE;
  case RYGEL_MEDIA_EXPORT_SQL_STRING_STATISTICS:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_STATISTICS_STRING;
  case RYGEL_MEDIA_EXPORT_SQL_STRING_RESET_TOKEN:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_RESET_TOKEN_STRING;
  case RYGEL_MEDIA_EXPORT_SQL_STRING_MAX_UPDATE_ID:
    return RYGEL_MEDIA_EXPORT_SQL_FACTORY_MAX_UPDATE_ID_STRING;
  default:
    g_assert_not_reached ();
  }
}

RygelMediaExportSQLFactory *
rygel_media_export_sql_factory_new (void) {
  return RYGEL_MEDIA_EXPORT_SQL_FACTORY (g_object_new (RYGEL_MEDIA_EXPORT_TYPE_SQL_FACTORY, NULL));
}

static void
rygel_media_export_sql_factory_class_init (RygelMediaExportSQLFactoryClass *klass G_GNUC_UNUSED) {
}

static void
rygel_media_export_sql_factory_init (RygelMediaExportSQLFactory *self G_GNUC_UNUSED) {
}
