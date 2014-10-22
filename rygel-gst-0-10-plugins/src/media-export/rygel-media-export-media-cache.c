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

#include <glib/gi18n-lib.h>
#include <sqlite3.h>

#include "rygel-media-export-database-cursor.h"
#include "rygel-media-export-database.h"
#include "rygel-media-export-errors.h"
#include "rygel-media-export-media-cache-upgrader.h"
#include "rygel-media-export-media-cache.h"
#include "rygel-media-export-music-item.h"
#include "rygel-media-export-null-container.h"
#include "rygel-media-export-object-factory.h"
#include "rygel-media-export-sql-factory.h"
#include "rygel-media-export-sql-function.h"
#include "rygel-media-export-sql-operator.h"
#include "rygel-media-export-string-utils.h"
#include "rygel-media-export-uuid.h"
#include "rygel-media-export-video-item.h"

/**
 * Persistent storage of media objects
 *
 *  MediaExportDB is a sqlite3 backed persistent storage of media objects
 */

G_DEFINE_TYPE (RygelMediaExportMediaCache,
               rygel_media_export_media_cache,
               G_TYPE_OBJECT)



#define RYGEL_MEDIA_EXPORT_TYPE_EXISTS_CACHE_ENTRY (rygel_media_export_exists_cache_entry_get_type ())
typedef struct _RygelMediaExportExistsCacheEntry RygelMediaExportExistsCacheEntry;

typedef enum  {
  RYGEL_MEDIA_EXPORT_OBJECT_TYPE_CONTAINER,
  RYGEL_MEDIA_EXPORT_OBJECT_TYPE_ITEM
} RygelMediaExportObjectType;

struct _RygelMediaExportExistsCacheEntry {
  gint64 mtime;
  gint64 size;
};


struct _RygelMediaExportMediaCachePrivate {
  RygelMediaExportDatabase* db;
  RygelMediaExportObjectFactory* factory;
  RygelMediaExportSQLFactory* sql;
  GeeHashMap* exists_cache;
};

static RygelMediaExportMediaCache* rygel_media_export_media_cache_instance = NULL;

GType rygel_media_export_exists_cache_entry_get_type (void) G_GNUC_CONST;
RygelMediaExportExistsCacheEntry* rygel_media_export_exists_cache_entry_dup (const RygelMediaExportExistsCacheEntry* self);
void rygel_media_export_exists_cache_entry_free (RygelMediaExportExistsCacheEntry* self);
#define RYGEL_MEDIA_EXPORT_MEDIA_CACHE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), RYGEL_MEDIA_EXPORT_TYPE_MEDIA_CACHE, RygelMediaExportMediaCachePrivate))
enum  {
  RYGEL_MEDIA_EXPORT_MEDIA_CACHE_DUMMY_PROPERTY
};

static void rygel_media_export_media_cache_open_db (RygelMediaExportMediaCache* self, const gchar* name, GError** error);
static void rygel_media_export_media_cache_get_exists_cache (RygelMediaExportMediaCache* self, GError** error);
static void rygel_media_export_media_cache_create_object (RygelMediaExportMediaCache* self, RygelMediaObject* item, GError** error);
static void rygel_media_export_media_cache_save_metadata (RygelMediaExportMediaCache* self, RygelMediaItem* item, GError** error);
static RygelMediaExportDatabaseCursor* rygel_media_export_media_cache_exec_cursor (RygelMediaExportMediaCache* self, RygelMediaExportSQLString id, GValue* values, int values_length1, GError** error);
static RygelMediaObject* rygel_media_export_media_cache_get_object_from_statement (RygelMediaExportMediaCache* self, RygelMediaContainer* parent, sqlite3_stmt* statement);
static gint rygel_media_export_media_cache_query_value (RygelMediaExportMediaCache* self, RygelMediaExportSQLString id, GValue* values, int values_length1, GError** error);
static gchar* rygel_media_export_media_cache_translate_sort_criteria (const gchar* sort_criteria);
static gchar* rygel_media_export_media_cache_translate_search_expression (RygelSearchExpression* expression, GArray* args, const gchar* prefix, GError** error);
static guint rygel_media_export_media_cache_modify_limit (guint max_count);
static gchar* rygel_media_export_media_cache_map_operand_to_column (const gchar* operand, gchar** collate, GError** error);
static gboolean rygel_media_export_media_cache_create_schema (RygelMediaExportMediaCache* self);
static void rygel_media_export_media_cache_fill_item (RygelMediaExportMediaCache* self, sqlite3_stmt* statement, RygelMediaItem* item);
static gchar* rygel_media_export_media_cache_search_expression_to_sql (RygelSearchExpression* expression, GArray* args, GError** error);
static gchar* rygel_media_export_media_cache_logical_expression_to_sql (RygelLogicalExpression* expression, GArray* args, GError** error);
static gchar* rygel_media_export_media_cache_relational_expression_to_sql (RygelRelationalExpression* exp, GArray* args, GError** error);
static void rygel_media_export_media_cache_dispose (GObject* obj);

static GArray *
our_g_value_array_new (void)
{
  GArray *array = g_array_new (FALSE, FALSE, sizeof (GValue));

  g_array_set_clear_func (array, (GDestroyNotify) g_value_unset);

  return array;
}

#define our_g_value_array_index(a, i) (g_array_index ((a), GValue, (i)))

static void
our_g_value_clear (GValue *value) {
  if (value) {
    GValue v = G_VALUE_INIT;

    *value = v;
  }
}

RygelMediaExportExistsCacheEntry *
rygel_media_export_exists_cache_entry_dup (const RygelMediaExportExistsCacheEntry* self) {
  RygelMediaExportExistsCacheEntry *dup;

  dup = g_new0 (RygelMediaExportExistsCacheEntry, 1);
  memcpy (dup, self, sizeof (RygelMediaExportExistsCacheEntry));
  return dup;
}

void
rygel_media_export_exists_cache_entry_free (RygelMediaExportExistsCacheEntry *self) {
  g_free (self);
}

GType
rygel_media_export_exists_cache_entry_get_type (void) {
  static volatile gsize rygel_media_export_exists_cache_entry_type_id__volatile = 0;

  if (g_once_init_enter (&rygel_media_export_exists_cache_entry_type_id__volatile)) {
    GType rygel_media_export_exists_cache_entry_type_id = g_boxed_type_register_static ("RygelMediaExportExistsCacheEntry",
                                                                                        (GBoxedCopyFunc) rygel_media_export_exists_cache_entry_dup,
                                                                                        (GBoxedFreeFunc) rygel_media_export_exists_cache_entry_free);
    g_once_init_leave (&rygel_media_export_exists_cache_entry_type_id__volatile, rygel_media_export_exists_cache_entry_type_id);
  }

  return rygel_media_export_exists_cache_entry_type_id__volatile;
}

RygelMediaExportMediaCache*
rygel_media_export_media_cache_new (GError **error) {
  RygelMediaExportMediaCache *self;
  RygelMediaExportMediaCachePrivate *priv;
  GError *inner_error;

  self = RYGEL_MEDIA_EXPORT_MEDIA_CACHE (g_object_new (RYGEL_MEDIA_EXPORT_TYPE_MEDIA_CACHE, NULL));
  priv = self->priv;

  priv->sql = rygel_media_export_sql_factory_new ();
  inner_error = NULL;
  rygel_media_export_media_cache_open_db (self, "media-export-gst-0-10", &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    g_object_unref (self);
    return NULL;
  }
  priv->factory = rygel_media_export_object_factory_new ();
  rygel_media_export_media_cache_get_exists_cache (self, &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    g_object_unref (self);
    return NULL;
  }

  return self;
}

gchar *
rygel_media_export_media_cache_get_id (GFile *file) {
  gchar* result = NULL;
  gchar* uri;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  uri = g_file_get_uri (file);
  result = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, -1);
  g_free (uri);
  return result;
}

gboolean
rygel_media_export_media_cache_ensure_exists (GError **error) {
  if (!rygel_media_export_media_cache_instance) {
    GError *inner_error = NULL;
    RygelMediaExportMediaCache* cache = rygel_media_export_media_cache_new (&inner_error);

    if (inner_error) {
      g_propagate_error (error, inner_error);
      return FALSE;
    }
    rygel_media_export_media_cache_instance = cache;
  }

  return TRUE;
}

RygelMediaExportMediaCache *
rygel_media_export_media_cache_get_default (void) {
  if (rygel_media_export_media_cache_instance) {
    return g_object_ref (rygel_media_export_media_cache_instance);
  }

  return NULL;
}

void
rygel_media_export_media_cache_remove_by_id (RygelMediaExportMediaCache  *self,
                                             const gchar                 *id,
                                             GError                     **error) {
  GValue value = G_VALUE_INIT;
  RygelMediaExportMediaCachePrivate *priv;
  GError *inner_error;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self));
  g_return_if_fail (id != NULL);

  priv = self->priv;
  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, id);

  inner_error = NULL;
  rygel_media_export_database_exec (priv->db,
                                    rygel_media_export_sql_factory_make (priv->sql,
                                                                         RYGEL_MEDIA_EXPORT_SQL_STRING_DELETE),
                                    &value,
                                    1,
                                    &inner_error);
  g_value_unset (&value);
  if (inner_error) {
    g_propagate_error (error, inner_error);
  }
}

void
rygel_media_export_media_cache_remove_object (RygelMediaExportMediaCache  *self,
                                              RygelMediaObject            *object,
                                              GError                     **error) {
  GError *inner_error;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self));
  g_return_if_fail (RYGEL_IS_MEDIA_OBJECT (object));

  inner_error = NULL;
  rygel_media_export_media_cache_remove_by_id (self, rygel_media_object_get_id (object), &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
  }
}

void
rygel_media_export_media_cache_save_container (RygelMediaExportMediaCache  *self,
                                               RygelMediaContainer         *container,
                                               GError                     **error) {
  GError *inner_error;
  RygelMediaExportMediaCachePrivate *priv;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self));
  g_return_if_fail (RYGEL_IS_MEDIA_CONTAINER (container));

  priv = self->priv;
  inner_error = NULL;
  rygel_media_export_database_begin (priv->db, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_media_cache_create_object (self, RYGEL_MEDIA_OBJECT (container), &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_commit (priv->db, &inner_error);
 out:
  if (inner_error) {
    rygel_media_export_database_rollback (priv->db);
    g_propagate_error (error, inner_error);
  }
}

void
rygel_media_export_media_cache_save_item (RygelMediaExportMediaCache  *self,
                                          RygelMediaItem              *item,
                                          GError                     **error) {
  GError *inner_error;
  RygelMediaExportMediaCachePrivate *priv;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self));
  g_return_if_fail (RYGEL_IS_MEDIA_ITEM (item));

  priv = self->priv;
  inner_error = NULL;
  rygel_media_export_database_begin (priv->db, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_media_cache_save_metadata (self, item, &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_media_cache_create_object (self, RYGEL_MEDIA_OBJECT (item), &inner_error);
  if (inner_error) {
    goto out;
  }
  rygel_media_export_database_commit (priv->db, &inner_error);
 out:
  if (inner_error) {
    g_warning (_("Failed to add item with ID %s: %s"),
               rygel_media_object_get_id (RYGEL_MEDIA_OBJECT (item)),
               inner_error->message);
    rygel_media_export_database_rollback (priv->db);
    g_propagate_error (error, inner_error);
  }
}

RygelMediaObject *
rygel_media_export_media_cache_get_object (RygelMediaExportMediaCache  *self,
                                           const gchar                 *object_id,
                                           GError                     **error) {
  RygelMediaObject *parent;
  RygelMediaExportDatabaseCursor *cursor;
  GError *inner_error;
  GValue value = G_VALUE_INIT;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self), NULL);
  g_return_val_if_fail (object_id != NULL, NULL);

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, object_id);
  inner_error = NULL;
  cursor = rygel_media_export_media_cache_exec_cursor (self, RYGEL_MEDIA_EXPORT_SQL_STRING_GET_OBJECT, &value, 1, &inner_error);
  g_value_unset (&value);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return NULL;
  }
  parent = NULL;
  while (rygel_media_export_database_cursor_has_next (cursor)) {
    sqlite3_stmt* statement = rygel_media_export_database_cursor_next (cursor, &inner_error);
    RygelMediaContainer *parent_container;
    RygelMediaObject *object;

    if (inner_error) {
      g_object_unref (cursor);
      g_propagate_error (error, inner_error);
      return NULL;
    }
    if (parent) {
      parent_container = RYGEL_MEDIA_CONTAINER (parent);
    } else {
      parent_container = NULL;
    }
    object = rygel_media_export_media_cache_get_object_from_statement (self, parent_container, statement);
    rygel_media_object_set_parent_ref (object, parent_container);
    if (parent) {
      g_object_unref (parent);
    }
    parent = object;
  }
  g_object_unref (cursor);
  return parent;
}

RygelMediaContainer *
rygel_media_export_media_cache_get_container (RygelMediaExportMediaCache  *self,
                                              const gchar                 *container_id,
                                              GError                     **error) {
  RygelMediaObject *object;
  GError *inner_error;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self), NULL);
  g_return_val_if_fail (container_id != NULL, NULL);

  inner_error = NULL;
  object = rygel_media_export_media_cache_get_object (self, container_id, &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return NULL;
  }
  if (object) {
    if (!RYGEL_IS_MEDIA_CONTAINER (object)) {
      g_set_error (error,
                   RYGEL_MEDIA_EXPORT_MEDIA_CACHE_ERROR,
                   RYGEL_MEDIA_EXPORT_MEDIA_CACHE_ERROR_INVALID_TYPE,
                   "Object with id %s is not a MediaContainer",
                   container_id);
      g_object_unref (object);
      return NULL;
    }
    return RYGEL_MEDIA_CONTAINER (object);
  }

  g_object_unref (object);
  return NULL;
}

gint
rygel_media_export_media_cache_get_child_count (RygelMediaExportMediaCache  *self,
                                                const gchar                 *container_id,
                                                GError                     **error) {
  GValue value = G_VALUE_INIT;
  GError *inner_error;
  gint count;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self), 0);
  g_return_val_if_fail (container_id != NULL, 0);

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, container_id);
  inner_error = NULL;
  count = rygel_media_export_media_cache_query_value (self, RYGEL_MEDIA_EXPORT_SQL_STRING_CHILD_COUNT, &value, 1, &inner_error);
  g_value_unset (&value);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return 0;
  }
  return count;
}

guint32
rygel_media_export_media_cache_get_update_id (RygelMediaExportMediaCache *self)
{
  GError *inner_error;
  guint32 id;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self), 0);

  inner_error = NULL;
  id = (guint32) rygel_media_export_media_cache_query_value (self,
                                                             RYGEL_MEDIA_EXPORT_SQL_STRING_MAX_UPDATE_ID,
                                                             NULL,
                                                             0,
                                                             &inner_error);

  if (inner_error) {
    g_error_free (inner_error);
    id = 0;
  }

  return id;
}

#define TRACK_PROPERTIES_SQL \
  "SELECT object_update_id, " \
  "container_update_id, " \
  "deleted_child_count " \
  "FROM Object WHERE upnp_id = ?"

void
rygel_media_export_media_cache_get_track_properties (RygelMediaExportMediaCache *self,
                                                     const gchar                *id,
                                                     guint32                    *object_update_id,
                                                     guint32                    *container_update_id,
                                                     guint32                    *total_deleted_child_count)
{
  GValue value = G_VALUE_INIT;
  GError *inner_error;
  RygelMediaExportDatabaseCursor *cursor;
  sqlite3_stmt *statement;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self));

  inner_error = NULL;
  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, id);
  cursor = rygel_media_export_database_exec_cursor (self->priv->db,
                                                    TRACK_PROPERTIES_SQL,
                                                    &value,
                                                    1,
                                                    &inner_error);
  g_value_unset (&value);
  if (inner_error) {
    goto out;
  }

  statement = rygel_media_export_database_cursor_next (cursor, &inner_error);
  if (inner_error) {
    g_object_unref (cursor);
    goto out;
  }

  if (object_update_id) {
    *object_update_id = (guint32) sqlite3_column_int64 (statement, 0);
  }
  if (container_update_id) {
    *container_update_id = (guint32) sqlite3_column_int64 (statement, 1);
  }
  if (total_deleted_child_count) {
    *total_deleted_child_count = (guint32) sqlite3_column_int64 (statement, 2);
  }
  g_object_unref (cursor);

 out:
  if (inner_error) {
    g_warning ("Failed to get updated ids: %s", inner_error->message);
    g_error_free (inner_error);
  }
}

gboolean
rygel_media_export_media_cache_exists (RygelMediaExportMediaCache  *self,
                                       GFile                       *file,
                                       gint64                      *timestamp,
                                       gint64                      *size,
                                       GError                     **error) {
  gboolean result;
  gchar *uri;
  RygelMediaExportDatabaseCursor *cursor;
  sqlite3_stmt *statement;
  GError *inner_error;
  RygelMediaExportMediaCachePrivate *priv;
  GeeAbstractMap *abstract_exists_cache;
  GValue value = G_VALUE_INIT;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  priv = self->priv;
  uri = g_file_get_uri (file);
  abstract_exists_cache = GEE_ABSTRACT_MAP (priv->exists_cache);
  if (gee_abstract_map_has_key (abstract_exists_cache, uri)) {
    RygelMediaExportExistsCacheEntry *entry = (RygelMediaExportExistsCacheEntry *) gee_abstract_map_get (abstract_exists_cache, uri);

    gee_abstract_map_unset (abstract_exists_cache, uri, NULL);
    g_free (uri);
    if (timestamp) {
      *timestamp = entry->mtime;
    }
    if (size) {
      *size = entry->size;
    }
    rygel_media_export_exists_cache_entry_free (entry);
    return TRUE;
  }

  g_value_init (&value, G_TYPE_STRING);
  g_value_take_string (&value, uri);

  inner_error = NULL;
  cursor = rygel_media_export_media_cache_exec_cursor (self, RYGEL_MEDIA_EXPORT_SQL_STRING_EXISTS, &value, 1, &inner_error);
  g_value_unset (&value);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return FALSE;
  }
  statement = rygel_media_export_database_cursor_next (cursor, &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    g_object_unref (cursor);
    return FALSE;
  }
  result = (sqlite3_column_int (statement, 0) == 1);
  if (timestamp) {
    *timestamp = sqlite3_column_int64 (statement , 1);
  }
  if (size) {
    *size = sqlite3_column_int64 (statement, 2);
  }
  g_object_unref (cursor);

  return result;
}

RygelMediaObjects *
rygel_media_export_media_cache_get_children (RygelMediaExportMediaCache  *self,
                                             RygelMediaContainer         *container,
                                             const gchar                 *sort_criteria,
                                             glong                        offset,
                                             glong                        max_count,
                                             GError                     **error) {
  GeeAbstractCollection *abstract_children;
  GValue values [] = {G_VALUE_INIT, G_VALUE_INIT, G_VALUE_INIT};
  guint iter;
  RygelMediaObjects *children;
  gchar *sql;
  gchar *sort_order;
  RygelMediaExportDatabaseCursor *cursor;
  GError *inner_error;
  RygelMediaExportMediaCachePrivate *priv;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self), NULL);
  g_return_val_if_fail (RYGEL_IS_MEDIA_CONTAINER (container), NULL);
  g_return_val_if_fail (sort_criteria != NULL, NULL);

  g_value_init (&(values[0]), G_TYPE_STRING);
  g_value_set_string (&(values[0]), rygel_media_object_get_id (RYGEL_MEDIA_OBJECT (container)));
  g_value_init (&(values[1]), G_TYPE_LONG);
  g_value_set_long (&(values[1]), offset);
  g_value_init (&(values[2]), G_TYPE_LONG);
  g_value_set_long (&(values[2]), max_count);

  sort_order = rygel_media_export_media_cache_translate_sort_criteria (sort_criteria);
  priv = self->priv;
  sql = g_strdup_printf (rygel_media_export_sql_factory_make (priv->sql, RYGEL_MEDIA_EXPORT_SQL_STRING_GET_CHILDREN),
                         sort_order);
  g_free (sort_order);
  inner_error = NULL;
  cursor = rygel_media_export_database_exec_cursor (priv->db, sql, values, G_N_ELEMENTS (values), &inner_error);
  g_free (sql);

  for (iter = 0; iter < G_N_ELEMENTS (values); ++iter) {
    g_value_unset (&(values[iter]));
  }

  if (inner_error) {
    g_propagate_error (error, inner_error);
    return NULL;
  }

  children = rygel_media_objects_new ();
  abstract_children = GEE_ABSTRACT_COLLECTION (children);
  {
    while (rygel_media_export_database_cursor_has_next (cursor)) {
      sqlite3_stmt *statement = rygel_media_export_database_cursor_next (cursor, &inner_error);
      RygelMediaObject *object;

      if (inner_error) {
        g_propagate_error (error, inner_error);
        g_object_unref (cursor);
        g_object_unref (children);
        return NULL;
      }
      object = rygel_media_export_media_cache_get_object_from_statement (self, container, statement);
      gee_abstract_collection_add (abstract_children, object);
      rygel_media_object_set_parent_ref (object, container);
      g_object_unref (object);
    }
  }
  g_object_unref (cursor);
  return children;
}

RygelMediaObjects *
rygel_media_export_media_cache_get_objects_by_search_expression (RygelMediaExportMediaCache  *self,
                                                                 RygelSearchExpression       *expression,
                                                                 const gchar                 *container_id,
                                                                 const gchar                 *sort_criteria,
                                                                 guint                        offset,
                                                                 guint                        max_count,
                                                                 guint                       *total_matches,
                                                                 GError                     **error) {
  guint matches;
  RygelMediaObjects *objects;
  GArray *args;
  gchar *filter;
  guint max_objects;
  GError *inner_error;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self), NULL);
  g_return_val_if_fail (sort_criteria != NULL, NULL);

  args = our_g_value_array_new ();
  inner_error = NULL;
  filter = rygel_media_export_media_cache_translate_search_expression (expression, args, "WHERE", &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    g_array_unref (args);
    return NULL;
  }
  if (expression) {
    gchar *ex_str = rygel_search_expression_to_string (expression);

    g_debug ("Original search: %s", ex_str);
    g_free (ex_str);
    g_debug ("Parsed search expression: %s", filter);
  }
  max_objects = rygel_media_export_media_cache_modify_limit (max_count);
  matches = (guint) rygel_media_export_media_cache_get_object_count_by_filter (self, filter, args, container_id, &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    g_free (filter);
    g_array_unref (args);
    return NULL;
  }
  objects = rygel_media_export_media_cache_get_objects_by_filter (self,
                                                                  filter,
                                                                  args,
                                                                  container_id,
                                                                  sort_criteria,
                                                                  (glong) offset,
                                                                  (glong) max_objects,
                                                                  &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    g_free (filter);
    g_array_unref (args);
    return NULL;
  }
  g_free (filter);
  g_array_unref (args);
  if (total_matches) {
    *total_matches = matches;
  }
  return objects;
}

glong
rygel_media_export_media_cache_get_object_count_by_search_expression (RygelMediaExportMediaCache  *self,
                                                                      RygelSearchExpression       *expression,
                                                                      const gchar                 *container_id,
                                                                      GError                     **error) {
  glong count;
  GArray *args;
  gchar *filter;
  GError *inner_error;
  guint iter;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self), 0);

  args = our_g_value_array_new ();
  inner_error = NULL;
  filter = rygel_media_export_media_cache_translate_search_expression (expression, args, "WHERE", &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    g_array_unref (args);
    return 0;
  }
  if (expression) {
    gchar* ex_str = rygel_search_expression_to_string (expression);

    g_debug ("Original search: %s", ex_str);
    g_free (ex_str);
    g_debug ("Parsed search expression: %s", filter);
  }
  for (iter = 0; iter < args->len; ++iter) {
    GValue *value = &our_g_value_array_index (args, iter);
    gchar *to_free;
    const gchar *contents;

    if (G_VALUE_HOLDS (value, G_TYPE_STRING)) {
      contents = g_value_get_string (value);
      to_free = NULL;
    } else {
      to_free = g_strdup_value_contents (value);
      contents = to_free;
    }

    g_debug ("Arg %d: %s", iter, contents);
    g_free (to_free);
  }
  count = rygel_media_export_media_cache_get_object_count_by_filter (self, filter, args, container_id, &inner_error);
  g_free (filter);
  g_array_unref (args);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return 0;
  }
  return count;
}


glong
rygel_media_export_media_cache_get_object_count_by_filter (RygelMediaExportMediaCache  *self,
                                                           const gchar                 *filter,
                                                           GArray                      *args,
                                                           const gchar                 *container_id,
                                                           GError                     **error) {
  glong count;
  gchar *sql;
  RygelMediaExportSQLString string_id;
  GError *inner_error;
  RygelMediaExportMediaCachePrivate *priv;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self), 0);
  g_return_val_if_fail (filter != NULL, 0);
  g_return_val_if_fail (args != NULL, 0);

  if (container_id) {
    GValue v = G_VALUE_INIT;

    g_value_init (&v, G_TYPE_STRING);
    g_value_set_string (&v, container_id);
    g_array_prepend_val (args, v);
  }
  g_debug ("Parameters to bind: %u", args->len);
  if (container_id) {
    string_id = RYGEL_MEDIA_EXPORT_SQL_STRING_GET_OBJECT_COUNT_BY_FILTER_WITH_ANCESTOR;
  } else {
    string_id = RYGEL_MEDIA_EXPORT_SQL_STRING_GET_OBJECT_COUNT_BY_FILTER;
  }
  priv = self->priv;
  sql = g_strdup_printf (rygel_media_export_sql_factory_make (priv->sql, string_id), filter);
  inner_error = NULL;
  count = rygel_media_export_database_query_value (priv->db, sql, (GValue *) args->data, (gint) args->len, &inner_error);
  g_free (sql);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return 0;
  }
  return count;
}

RygelMediaObjects *
rygel_media_export_media_cache_get_objects_by_filter (RygelMediaExportMediaCache  *self,
                                                      const gchar                 *filter,
                                                      GArray                      *args,
                                                      const gchar                 *container_id,
                                                      const gchar                 *sort_criteria,
                                                      glong                        offset,
                                                      glong                        max_count,
                                                      GError                     **error) {
  RygelMediaExportSQLString string_id;
  RygelMediaObjects *children;
  GValue v = G_VALUE_INIT;
  RygelMediaContainer *parent;
  gchar *sql;
  gchar *sort_order;
  RygelMediaExportDatabaseCursor *cursor;
  GError *inner_error;
  guint iter;
  RygelMediaExportMediaCachePrivate *priv;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self), NULL);
  g_return_val_if_fail (filter != NULL, NULL);
  g_return_val_if_fail (args != NULL, NULL);
  g_return_val_if_fail (sort_criteria != NULL, NULL);

  g_value_init (&v, G_TYPE_LONG);
  g_value_set_long (&v, offset);
  g_array_append_val (args, v);
  our_g_value_clear (&v);
  g_value_init (&v, G_TYPE_LONG);
  g_value_set_long (&v, max_count);
  g_array_append_val (args, v);
  our_g_value_clear (&v);
  parent = NULL;

  g_debug ("Parameters to bind: %u", args->len);
  for (iter = 0; iter < args->len; ++iter) {
    GValue *value = &our_g_value_array_index (args, iter);
    gchar *to_free;
    const gchar *contents;

    if (G_VALUE_HOLDS (value, G_TYPE_STRING)) {
      contents = g_value_get_string (value);
      to_free = NULL;
    } else {
      to_free = g_strdup_value_contents (value);
      contents = to_free;
    }

    g_debug ("Arg %d: %s", iter, contents);
    g_free (to_free);
  }

  if (container_id) {
    string_id = RYGEL_MEDIA_EXPORT_SQL_STRING_GET_OBJECTS_BY_FILTER_WITH_ANCESTOR;
  } else {
    string_id = RYGEL_MEDIA_EXPORT_SQL_STRING_GET_OBJECTS_BY_FILTER;
  }
  sort_order = rygel_media_export_media_cache_translate_sort_criteria (sort_criteria);
  priv = self->priv;
  sql = g_strdup_printf (rygel_media_export_sql_factory_make (priv->sql, string_id), filter, sort_order);
  g_free (sort_order);
  inner_error = NULL;
  cursor = rygel_media_export_database_exec_cursor (priv->db, sql, (GValue *) args->data, (gint) args->len, &inner_error);
  g_free (sql);
  if (inner_error != NULL) {
    g_propagate_error (error, inner_error);
    return NULL;
  }

  parent = NULL;
  children = rygel_media_objects_new ();

  while (rygel_media_export_database_cursor_has_next (cursor)) {
    sqlite3_stmt* statement = rygel_media_export_database_cursor_next (cursor, &inner_error);
    const gchar *parent_id;

    if (inner_error) {
      g_propagate_error (error, inner_error);
      g_object_unref (cursor);
      g_object_unref (children);
      return NULL;
    }

    parent_id = (const gchar *) sqlite3_column_text (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_PARENT);
    if (!parent || g_strcmp0 (parent_id, rygel_media_object_get_id (RYGEL_MEDIA_OBJECT (parent)))) {
      parent = RYGEL_MEDIA_CONTAINER (rygel_media_export_null_container_new ());
      rygel_media_object_set_id (RYGEL_MEDIA_OBJECT (parent), parent_id);
    }
    if (parent) {
      RygelMediaObject *object = rygel_media_export_media_cache_get_object_from_statement (self, parent, statement);

      gee_abstract_collection_add (GEE_ABSTRACT_COLLECTION (children), object);
      rygel_media_object_set_parent_ref (object, parent);
      g_object_unref (object);
    } else {
      g_warning ("Inconsistent database: item %s has no parent %s",
                 (const gchar *) sqlite3_column_text (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_ID),
                 parent_id);
    }
  }

  g_object_unref (cursor);
  return children;
}

void
rygel_media_export_media_cache_debug_statistics (RygelMediaExportMediaCache *self) {
  GError *inner_error;
  RygelMediaExportDatabaseCursor *cursor;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self));

  g_debug ("Database statistics:");
  inner_error = NULL;
  cursor = rygel_media_export_media_cache_exec_cursor (self, RYGEL_MEDIA_EXPORT_SQL_STRING_STATISTICS, NULL, 0, &inner_error);
  if (inner_error != NULL) {
    goto out;
  }

  while (rygel_media_export_database_cursor_has_next (cursor)) {
    sqlite3_stmt *statement = rygel_media_export_database_cursor_next (cursor, &inner_error);

    if (inner_error) {
      g_object_unref (cursor);
      goto out;
    }
    g_debug ("%s: %d",
             (const gchar *) sqlite3_column_text (statement, 0),
             sqlite3_column_int (statement, 1));
  }
  g_object_unref (cursor);
 out:
  if (inner_error) {
    g_error_free (inner_error);
  }
}


GeeArrayList *
rygel_media_export_media_cache_get_child_ids (RygelMediaExportMediaCache  *self,
                                              const gchar                 *container_id,
                                              GError                     **error) {
  GeeArrayList *children;
  GeeAbstractCollection *abstract_children;
  GValue value = G_VALUE_INIT;
  RygelMediaExportDatabaseCursor *cursor;
  GError *inner_error;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self), NULL);
  g_return_val_if_fail (container_id != NULL, NULL);

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, container_id);

  inner_error = NULL;
  cursor = rygel_media_export_media_cache_exec_cursor (self, RYGEL_MEDIA_EXPORT_SQL_STRING_CHILD_IDS, &value, 1, &inner_error);
  g_value_unset (&value);

  if (inner_error) {
    g_propagate_error (error, inner_error);
    return NULL;
  }

  children = gee_array_list_new (G_TYPE_STRING, (GBoxedCopyFunc) g_strdup, g_free, NULL, NULL, NULL);
  abstract_children = GEE_ABSTRACT_COLLECTION (children);

  while (rygel_media_export_database_cursor_has_next (cursor)) {
    sqlite3_stmt* statement = rygel_media_export_database_cursor_next (cursor, &inner_error);

    if (inner_error) {
      g_propagate_error (error, inner_error);
      g_object_unref (children);
      g_object_unref (cursor);
      return NULL;
    }
    gee_abstract_collection_add (abstract_children, (const gchar *) sqlite3_column_text (statement, 0));
  }
  g_object_unref (cursor);
  return children;
}

GeeList *
rygel_media_export_media_cache_get_meta_data_column_by_filter (RygelMediaExportMediaCache  *self,
                                                               const gchar                 *column,
                                                               const gchar                 *filter,
                                                               GArray                      *args,
                                                               glong                        offset,
                                                               glong                        max_count,
                                                               GError                     **error) {
  GValue v = G_VALUE_INIT;
  GeeArrayList *data;
  GeeAbstractCollection *abstract_data;
  gchar *sql;
  RygelMediaExportDatabaseCursor *cursor;
  GError *inner_error;
  RygelMediaExportMediaCachePrivate *priv;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self), NULL);
  g_return_val_if_fail (column != NULL, NULL);
  g_return_val_if_fail (filter != NULL, NULL);
  g_return_val_if_fail (args != NULL, NULL);

  g_value_init (&v, G_TYPE_LONG);
  g_value_set_long (&v, offset);
  g_array_append_val (args, v);
  our_g_value_clear (&v);
  g_value_init (&v, G_TYPE_LONG);
  g_value_set_long (&v, max_count);
  g_array_append_val (args, v);
  our_g_value_clear (&v);

  priv = self->priv;
  sql = g_strdup_printf (rygel_media_export_sql_factory_make (priv->sql, RYGEL_MEDIA_EXPORT_SQL_STRING_GET_META_DATA_COLUMN),
                         column,
                         filter);
  inner_error = NULL;
  cursor = rygel_media_export_database_exec_cursor (priv->db, sql, (GValue *) args->data, (gint) args->len, &inner_error);
  g_free (sql);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return NULL;
  }

  data = gee_array_list_new (G_TYPE_STRING, (GBoxedCopyFunc) g_strdup, g_free, NULL, NULL, NULL);
  abstract_data = GEE_ABSTRACT_COLLECTION (data);
  while (rygel_media_export_database_cursor_has_next (cursor)) {
    sqlite3_stmt *statement = rygel_media_export_database_cursor_next (cursor, &inner_error);

    if (inner_error) {
      g_propagate_error (error, inner_error);
      g_object_unref (cursor);
      g_object_unref (data);
      return NULL;
    }
    gee_abstract_collection_add (abstract_data, (const gchar *) sqlite3_column_text (statement, 0));
  }
  g_object_unref (cursor);

  return GEE_LIST (data);
}

GeeList *
rygel_media_export_media_cache_get_object_attribute_by_search_expression (RygelMediaExportMediaCache  *self,
                                                                          const gchar                 *attribute,
                                                                          RygelSearchExpression       *expression,
                                                                          glong                        offset,
                                                                          guint                        max_count,
                                                                          GError                     **error) {
  GeeList *attributes;
  GArray *args;
  gchar *filter;
  gchar *column;
  guint max_objects;
  GError *inner_error;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self), NULL);
  g_return_val_if_fail (attribute != NULL, NULL);

  args = our_g_value_array_new ();
  inner_error = NULL;
  filter = rygel_media_export_media_cache_translate_search_expression (expression, args, "AND", &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    g_array_unref (args);
    return NULL;
  }
  g_debug ("Parsed filter: %s", filter);
  column = rygel_media_export_media_cache_map_operand_to_column (attribute, NULL, &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    g_free (filter);
    g_array_unref (args);
    return NULL;
  }
  max_objects = rygel_media_export_media_cache_modify_limit (max_count);
  attributes = rygel_media_export_media_cache_get_meta_data_column_by_filter (self, column, filter, args, offset, (glong) max_objects, &inner_error);
  g_free (column);
  g_free (filter);
  g_array_unref (args);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return NULL;
  }
  return attributes;
}

void
rygel_media_export_media_cache_flag_object (RygelMediaExportMediaCache  *self,
                                            GFile                       *file,
                                            const gchar                 *flag,
                                            GError                     **error) {
  GValue values[] = {G_VALUE_INIT, G_VALUE_INIT};
  guint iter;
  GError *inner_error;
  RygelMediaExportMediaCachePrivate *priv;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (flag != NULL);

  g_value_init (&(values[0]), G_TYPE_STRING);
  g_value_set_string (&(values[0]), flag);
  g_value_init (&(values[1]), G_TYPE_STRING);
  g_value_take_string (&(values[1]), g_file_get_uri (file));

  inner_error = NULL;
  priv = self->priv;
  rygel_media_export_database_exec (priv->db, "UPDATE Object SET flags = ? WHERE uri = ?", values, G_N_ELEMENTS(values), &inner_error);
  for (iter = 0; iter < G_N_ELEMENTS (values); ++iter) {
    g_value_unset (&(values[iter]));
  }
  if (inner_error) {
    g_propagate_error (error, inner_error);
  }
}

GeeList *
rygel_media_export_media_cache_get_flagged_uris (RygelMediaExportMediaCache  *self,
                                                 const gchar                 *flag,
                                                 GError                     **error) {
  GValue value = G_VALUE_INIT;
  GeeArrayList *uris;
  GeeAbstractCollection *abstract_uris;
  RygelMediaExportDatabaseCursor *cursor;
  GError *inner_error;
  RygelMediaExportMediaCachePrivate *priv;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self), NULL);
  g_return_val_if_fail (flag != NULL, NULL);

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, flag);

  inner_error = NULL;
  priv = self->priv;
  cursor = rygel_media_export_database_exec_cursor (priv->db, "SELECT uri FROM object WHERE flags = ?", &value, 1, &inner_error);
  g_value_unset (&value);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return NULL;
  }

  uris = gee_array_list_new (G_TYPE_STRING, (GBoxedCopyFunc) g_strdup, g_free, NULL, NULL, NULL);
  abstract_uris = GEE_ABSTRACT_COLLECTION (uris);
  while (rygel_media_export_database_cursor_has_next (cursor)) {
    sqlite3_stmt *statement = rygel_media_export_database_cursor_next (cursor, &inner_error);

    if (inner_error) {
      g_propagate_error (error, inner_error);
      g_object_unref (cursor);
      g_object_unref (uris);
      return NULL;
    }
    gee_abstract_collection_add (abstract_uris, (const gchar *) sqlite3_column_text (statement, 0));
  }
  g_object_unref (cursor);
  return GEE_LIST (uris);
}

gchar *
rygel_media_export_media_cache_get_reset_token (RygelMediaExportMediaCache *self)
{
  GError *inner_error;
  RygelMediaExportDatabaseCursor *cursor;
  sqlite3_stmt *statement;
  gchar *token;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self), NULL);

  inner_error = NULL;
  cursor = rygel_media_export_media_cache_exec_cursor (self,
                                                       RYGEL_MEDIA_EXPORT_SQL_STRING_RESET_TOKEN,
                                                       NULL,
                                                       0,
                                                       &inner_error);
  if (inner_error) {
    goto out;
  }

  statement = rygel_media_export_database_cursor_next (cursor, &inner_error);
  if (inner_error) {
    g_object_unref (cursor);
    goto out;
  }

  token = g_strdup ((const gchar *) sqlite3_column_text (statement, 0));
  g_object_unref (cursor);

 out:
  if (inner_error) {
    g_warning ("Failed to get reset token: %s", inner_error->message);
    g_error_free (inner_error);
    token = rygel_media_export_uuid_get ();
  }

  return token;
}

#define SAVE_RESET_TOKEN_SQL \
  "UPDATE schema_info SET reset_token = ?"

void
rygel_media_export_media_cache_save_reset_token (RygelMediaExportMediaCache *self,
                                                 const gchar                *token)
{
  GValue value = G_VALUE_INIT;
  GError *inner_error;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self));
  g_return_if_fail (token != NULL);

  inner_error = NULL;
  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, token);
  rygel_media_export_database_exec (self->priv->db,
                                    SAVE_RESET_TOKEN_SQL,
                                    &value,
                                    1,
                                    &inner_error);
  g_value_unset (&value);
  if (inner_error) {
    g_warning ("Failed to persist ServiceResetToken: %s", inner_error->message);
    g_error_free (inner_error);
  }
}

static void
rygel_media_export_media_cache_get_exists_cache (RygelMediaExportMediaCache  *self,
                                                 GError                     **error) {
  RygelMediaExportDatabaseCursor *cursor;
  GError *inner_error;
  RygelMediaExportMediaCachePrivate *priv;
  GeeAbstractMap *abstract_cache;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self));

  inner_error = NULL;
  cursor = rygel_media_export_media_cache_exec_cursor (self, RYGEL_MEDIA_EXPORT_SQL_STRING_EXISTS_CACHE, NULL, 0, &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return;
  }
  priv = self->priv;
  priv->exists_cache = gee_hash_map_new (G_TYPE_STRING,
                                         (GBoxedCopyFunc) g_strdup,
                                         g_free,
                                         RYGEL_MEDIA_EXPORT_TYPE_EXISTS_CACHE_ENTRY,
                                         (GBoxedCopyFunc) rygel_media_export_exists_cache_entry_dup,
                                         (GDestroyNotify) rygel_media_export_exists_cache_entry_free,
                                         NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  abstract_cache = GEE_ABSTRACT_MAP (priv->exists_cache);
  while (rygel_media_export_database_cursor_has_next (cursor)) {
    sqlite3_stmt *statement = rygel_media_export_database_cursor_next (cursor, &inner_error);
    RygelMediaExportExistsCacheEntry entry;

    if (inner_error) {
      g_propagate_error (error, inner_error);
      g_object_unref (cursor);
      return;
    }
    entry.mtime = sqlite3_column_int64 (statement, 1);
    entry.size = sqlite3_column_int64 (statement, 0);
    gee_abstract_map_set (abstract_cache, (const gchar *) sqlite3_column_text (statement, 2), &entry);
  }
  g_object_unref (cursor);
}

static guint
rygel_media_export_media_cache_modify_limit (guint max_count) {
  if (max_count) {
    return max_count;
  } else {
    return (guint) -1;
  }
}

static void
rygel_media_export_media_cache_open_db (RygelMediaExportMediaCache  *self,
                                        const gchar                 *name,
                                        GError                     **error) {
  RygelMediaExportMediaCacheUpgrader *upgrader;
  gboolean needs_update;
  gint old_version;
  gint current_version;
  GError *inner_error;
  RygelMediaExportMediaCachePrivate *priv;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self));
  g_return_if_fail (name != NULL);

  inner_error = NULL;
  priv = self->priv;
  priv->db = rygel_media_export_database_new (name, &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return;
  }

  old_version = -1;
  current_version = atoi (RYGEL_MEDIA_EXPORT_SQL_FACTORY_SCHEMA_VERSION);

  upgrader = rygel_media_export_media_cache_upgrader_new (priv->db, priv->sql);
  needs_update = rygel_media_export_media_cache_upgrader_needs_upgrade (upgrader, &old_version, &inner_error);
  if (inner_error) {
    g_object_unref (upgrader);
    goto out;
  }

  if (needs_update) {
    rygel_media_export_media_cache_upgrader_upgrade (upgrader, old_version);
  } else if (old_version == current_version) {
    rygel_media_export_media_cache_upgrader_fix_schema (upgrader, &inner_error);
    if (inner_error) {
      g_object_unref (upgrader);
      goto out;
    }
  } else {
    g_warning ("The version \"%d\" of the detected database is newer than our supported version \"%d\"",
               old_version,
               current_version);
    g_object_unref (priv->db);
    priv->db = NULL;
    g_set_error_literal (error,
                         RYGEL_MEDIA_EXPORT_MEDIA_CACHE_ERROR,
                         RYGEL_MEDIA_EXPORT_MEDIA_CACHE_ERROR_GENERAL_ERROR,
                         "Database format not supported");
    g_object_unref (upgrader);
    return;
  }
  rygel_media_export_media_cache_upgrader_ensure_indices (upgrader);
  g_object_unref (upgrader);
 out:
  if (inner_error) {
    gint rows;

    if (inner_error->domain != RYGEL_MEDIA_EXPORT_DATABASE_ERROR) {
      g_propagate_error (error, inner_error);
      return;
    }
    g_error_free (inner_error);
    inner_error = NULL;
    g_debug ("Could not find schema version; checking for empty database...");
    rows = rygel_media_export_database_query_value (priv->db, "SELECT count(type) FROM sqlite_master WHERE rowid=1", NULL, 0, &inner_error);
    if (inner_error) {
      if (inner_error->domain == RYGEL_MEDIA_EXPORT_DATABASE_ERROR) {
        goto db_error;
      }
      g_propagate_error (error, inner_error);
      return;
    }
    if (!rows) {
      g_debug ("Empty database, creating new schema version %s", RYGEL_MEDIA_EXPORT_SQL_FACTORY_SCHEMA_VERSION);
      if (!rygel_media_export_media_cache_create_schema (self)) {
        g_object_unref (priv->db);
        priv->db = NULL;
        return;
      }
    } else {
      g_warning ("Incompatible schema... cannot proceed");
      g_object_unref (priv->db);
      self->priv->db = NULL;
      return;
    }
  db_error:
    if (inner_error) {
      g_warning ("Something weird going on: %s", inner_error->message);
      g_error_free (inner_error);
      g_object_unref (priv->db);
      priv->db = NULL;
      g_set_error_literal (error,
                           RYGEL_MEDIA_EXPORT_MEDIA_CACHE_ERROR,
                           RYGEL_MEDIA_EXPORT_MEDIA_CACHE_ERROR_GENERAL_ERROR,
                           "Invalid database");
    }
  }
}

static void
rygel_media_export_media_cache_save_metadata (RygelMediaExportMediaCache  *self,
                                              RygelMediaItem              *item,
                                              GError                     **error) {
  GValue values [] = {G_VALUE_INIT,  /* item size */
                      G_VALUE_INIT,  /* item mime type */
                      G_VALUE_INIT,  /* visual item width */
                      G_VALUE_INIT,  /* visual item height */
                      G_VALUE_INIT,  /* item upnp class */
                      G_VALUE_INIT,  /* music item artist / video item author / playlist item creator */
                      G_VALUE_INIT,  /* music item album */
                      G_VALUE_INIT,  /* item date */
                      G_VALUE_INIT,  /* audio item bitrate */
                      G_VALUE_INIT,  /* audio item sample frequency */
                      G_VALUE_INIT,  /* audio item bits per sample */
                      G_VALUE_INIT,  /* audio item channels */
                      G_VALUE_INIT,  /* music item track number  */
                      G_VALUE_INIT,  /* visual item color depth */
                      G_VALUE_INIT,  /* audio item duration */
                      G_VALUE_INIT,  /* item id */
                      G_VALUE_INIT,  /* item dlna profile */
                      G_VALUE_INIT,  /* music item genre */
                      G_VALUE_INIT}; /* music item disc */
  GError *inner_error;
  RygelMediaObject *object;
  guint iter;
  RygelMediaExportMediaCachePrivate *priv;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self));
  g_return_if_fail (RYGEL_IS_MEDIA_ITEM (item));

  priv = self->priv;
  object = RYGEL_MEDIA_OBJECT (item);

  g_value_init (&(values[0]), G_TYPE_INT64);
  g_value_set_int64 (&(values[0]), rygel_media_item_get_size (item));

  g_value_init (&(values[1]), G_TYPE_STRING);
  g_value_set_string (&(values[1]), rygel_media_item_get_mime_type (item));

  g_value_init (&(values[2]), G_TYPE_INT);
  g_value_set_int (&(values[2]), -1);

  g_value_init (&(values[3]), G_TYPE_INT);
  g_value_set_int (&(values[3]), -1);

  g_value_init (&(values[4]), G_TYPE_STRING);
  g_value_set_string (&(values[4]), rygel_media_object_get_upnp_class (object));

  rygel_media_export_database_null (&(values[5]));

  rygel_media_export_database_null (&(values[6]));

  g_value_init (&(values[7]), G_TYPE_STRING);
  g_value_set_string (&(values[7]), rygel_media_item_get_date (item));

  g_value_init (&(values[8]), G_TYPE_INT);
  g_value_set_int (&(values[8]), -1);

  g_value_init (&(values[9]), G_TYPE_INT);
  g_value_set_int (&(values[9]), -1);

  g_value_init (&(values[10]), G_TYPE_INT);
  g_value_set_int (&(values[10]), -1);

  g_value_init (&(values[11]), G_TYPE_INT);
  g_value_set_int (&(values[11]), -1);

  g_value_init (&(values[12]), G_TYPE_INT);
  g_value_set_int (&(values[12]), -1);

  g_value_init (&(values[13]), G_TYPE_INT);
  g_value_set_int (&(values[13]), -1);

  g_value_init (&(values[14]), G_TYPE_LONG);
  g_value_set_long (&(values[14]), -1);

  g_value_init (&(values[15]), G_TYPE_STRING);
  g_value_set_string (&(values[15]), rygel_media_object_get_id (object));

  g_value_init (&(values[16]), G_TYPE_STRING);
  g_value_set_string (&(values[16]), rygel_media_item_get_dlna_profile (item));

  rygel_media_export_database_null (&(values[17]));

  g_value_init (&(values[18]), G_TYPE_INT);
  g_value_set_int (&(values[18]), -1);

  if (RYGEL_IS_AUDIO_ITEM (item)) {
    RygelAudioItem *audio_item = RYGEL_AUDIO_ITEM (item);

    g_value_set_long (&(values[14]), rygel_audio_item_get_duration (audio_item));
    g_value_set_int (&(values[8]), rygel_audio_item_get_bitrate (audio_item));
    g_value_set_int (&(values[9]), rygel_audio_item_get_sample_freq (audio_item));
    g_value_set_int (&(values[10]), rygel_audio_item_get_bits_per_sample (audio_item));
    g_value_set_int (&(values[11]), rygel_audio_item_get_channels (audio_item));
    if (RYGEL_IS_MUSIC_ITEM (item)) {
      RygelMusicItem *music_item = RYGEL_MUSIC_ITEM (item);

      g_value_unset (&(values[5]));
      g_value_init (&(values[5]), G_TYPE_STRING);
      g_value_set_string (&(values[5]), rygel_music_item_get_artist (music_item));

      g_value_unset (&(values[6]));
      g_value_init (&(values[6]), G_TYPE_STRING);
      g_value_set_string (&(values[6]), rygel_music_item_get_album (music_item));

      g_value_unset (&(values[17]));
      g_value_init (&(values[17]), G_TYPE_STRING);
      g_value_set_string (&(values[17]), rygel_music_item_get_genre (music_item));

      g_value_set_int (&(values[12]), rygel_music_item_get_track_number (music_item));
      if (RYGEL_MEDIA_EXPORT_IS_MUSIC_ITEM (item)) {
        g_value_set_int (&(values[18]), RYGEL_MEDIA_EXPORT_MUSIC_ITEM (item)->disc);
      }
    }
  }
  if (RYGEL_IS_VISUAL_ITEM (item)) {
    RygelVisualItem *visual_item = RYGEL_VISUAL_ITEM (item);

    g_value_set_int (&(values[2]), rygel_visual_item_get_width (visual_item));
    g_value_set_int (&(values[3]), rygel_visual_item_get_height (visual_item));
    g_value_set_int (&(values[13]), rygel_visual_item_get_color_depth (visual_item));

    if (RYGEL_IS_VIDEO_ITEM (item)) {
      RygelVideoItem *video_item = RYGEL_VIDEO_ITEM (item);

      const char *author = rygel_video_item_get_author (video_item);
      if (author) {
        g_value_unset (&(values[5]));
        g_value_init (&(values[5]), G_TYPE_STRING);
        g_value_set_string (&(values[5]), author);
      }
    }
  }
  inner_error = NULL;
  rygel_media_export_database_exec (priv->db,
                                    rygel_media_export_sql_factory_make (priv->sql, RYGEL_MEDIA_EXPORT_SQL_STRING_SAVE_METADATA),
                                    values,
                                    G_N_ELEMENTS (values),
                                    &inner_error);
  for (iter = 0; iter < G_N_ELEMENTS (values); ++iter) {
    g_value_unset (&(values[iter]));
  }
  if (inner_error) {
    g_propagate_error (error, inner_error);
  }
}

static void
rygel_media_export_media_cache_create_object (RygelMediaExportMediaCache  *self,
                                              RygelMediaObject            *object,
                                              GError                     **error) {
  GValue values[] = {G_VALUE_INIT,  /* object id */
                     G_VALUE_INIT,  /* object title */
                     G_VALUE_INIT,  /* type */
                     G_VALUE_INIT,  /* parent */
                     G_VALUE_INIT,  /* object modified */
                     G_VALUE_INIT,  /* object uri */
                     G_VALUE_INIT,  /* object update id */
                     G_VALUE_INIT,  /* total deleted child count */
                     G_VALUE_INIT}; /* container update id */
  gint type;
  GError *inner_error;
  RygelMediaObject *object_parent;
  gchar *uri;
  guint iter;
  RygelMediaExportMediaCachePrivate *priv;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self));
  g_return_if_fail (RYGEL_IS_MEDIA_OBJECT (object));

  priv = self->priv;
  g_value_init (&(values[0]), G_TYPE_STRING);
  g_value_set_string (&(values[0]), rygel_media_object_get_id (object));

  g_value_init (&(values[1]), G_TYPE_STRING);
  g_value_set_string (&(values[1]), rygel_media_object_get_title (object));

  if (RYGEL_IS_MEDIA_ITEM (object)) {
    type = (gint) RYGEL_MEDIA_EXPORT_OBJECT_TYPE_ITEM;
  } else {
    type = (gint) RYGEL_MEDIA_EXPORT_OBJECT_TYPE_CONTAINER;
  }

  g_value_init (&(values[2]), G_TYPE_INT);
  g_value_set_int (&(values[2]), type);

  object_parent = RYGEL_MEDIA_OBJECT (rygel_media_object_get_parent (object));
  if (object_parent) {
    g_value_init (&(values[3]), G_TYPE_STRING);
    g_value_set_string (&(values[3]), rygel_media_object_get_id (object_parent));
  } else {
    rygel_media_export_database_null (&(values[3]));
  }

  g_value_init (&(values[4]), G_TYPE_UINT64);
  g_value_set_uint64 (&(values[4]), rygel_media_object_get_modified (object));

  if (gee_collection_get_is_empty (GEE_COLLECTION (object->uris))) {
    uri = NULL;
  } else {
    uri = (gchar *) gee_list_first (GEE_LIST (object->uris));
  }
  g_value_init (&(values[5]), G_TYPE_STRING);
  g_value_take_string (&(values[5]), uri);

  g_value_init (&(values[6]), G_TYPE_UINT);
  g_value_set_uint (&(values[6]), rygel_media_object_get_object_update_id (object));

  if (RYGEL_IS_MEDIA_CONTAINER (object)) {
    RygelMediaContainer *container = RYGEL_MEDIA_CONTAINER (object);

    g_value_init (&(values[7]), G_TYPE_INT64);
    g_value_set_int64 (&(values[7]), container->total_deleted_child_count);

    g_value_init (&(values[8]), G_TYPE_UINT);
    g_value_set_uint (&(values[8]), container->update_id);
  } else {
    g_value_init (&(values[7]), G_TYPE_INT);
    g_value_set_int (&(values[7]), -1);

    g_value_init (&(values[8]), G_TYPE_INT);
    g_value_set_int (&(values[8]), -1);
  }

  inner_error = NULL;
  rygel_media_export_database_exec (priv->db,
                                    rygel_media_export_sql_factory_make (priv->sql, RYGEL_MEDIA_EXPORT_SQL_STRING_INSERT),
                                    values,
                                    G_N_ELEMENTS (values),
                                    &inner_error);
  for (iter = 0; iter < G_N_ELEMENTS (values); ++iter) {
    g_value_unset (&(values[iter]));
  }
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return;
  }
}

/**
 * Create the current schema.
 *
 * If schema creation fails, schema will be rolled back
 * completely.
 *
 * @returns: true on success, false on failure
 */
static gboolean
rygel_media_export_media_cache_create_schema (RygelMediaExportMediaCache *self) {
  GError *inner_error;
  RygelMediaExportMediaCachePrivate *priv;
  gchar *uu;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self), FALSE);

  priv = self->priv;
  inner_error = NULL;
  rygel_media_export_database_begin (priv->db, &inner_error);
  if (inner_error) {
    goto out;
  }

  rygel_media_export_database_exec (priv->db,
                                    rygel_media_export_sql_factory_make (priv->sql, RYGEL_MEDIA_EXPORT_SQL_STRING_SCHEMA),
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }

  rygel_media_export_database_exec (priv->db,
                                    rygel_media_export_sql_factory_make (priv->sql, RYGEL_MEDIA_EXPORT_SQL_STRING_TRIGGER_COMMON),
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }

  rygel_media_export_database_exec (priv->db,
                                    rygel_media_export_sql_factory_make (priv->sql, RYGEL_MEDIA_EXPORT_SQL_STRING_TABLE_CLOSURE),
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }

  rygel_media_export_database_exec (priv->db,
                                    rygel_media_export_sql_factory_make (priv->sql, RYGEL_MEDIA_EXPORT_SQL_STRING_INDEX_COMMON),
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }

  rygel_media_export_database_exec (priv->db,
                                    rygel_media_export_sql_factory_make (priv->sql, RYGEL_MEDIA_EXPORT_SQL_STRING_TRIGGER_CLOSURE),
                                    NULL,
                                    0,
                                    &inner_error);
  if (inner_error) {
    goto out;
  }

  rygel_media_export_database_commit (priv->db, &inner_error);
  if (inner_error) {
    goto out;
  }

  rygel_media_export_database_analyze (priv->db);

  uu = rygel_media_export_uuid_get ();
  rygel_media_export_media_cache_save_reset_token (self, uu);
  g_free (uu);
 out:
  if (inner_error) {
    g_warning ("Failed to create schema: %s", inner_error->message);
    rygel_media_export_database_rollback (priv->db);
    g_error_free (inner_error);
    return FALSE;
  }
  return TRUE;
}

static RygelMediaObject *
rygel_media_export_media_cache_get_object_from_statement (RygelMediaExportMediaCache *self,
                                                          RygelMediaContainer        *parent,
                                                          sqlite3_stmt               *statement) {
  RygelMediaObject *object;
  const gchar *title;
  const gchar *object_id;
  const gchar *uri;
  RygelMediaExportMediaCachePrivate *priv;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self), NULL);
  g_return_val_if_fail (statement != NULL, NULL);

  priv = self->priv;
  title = (const gchar *) sqlite3_column_text (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_TITLE);
  object_id = (const gchar *) sqlite3_column_text (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_ID);
  uri = (const gchar *) sqlite3_column_text (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_URI);
  switch (sqlite3_column_int (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_TYPE)) {
  case 0: { /* container */
    RygelMediaContainer *container = RYGEL_MEDIA_CONTAINER (rygel_media_export_object_factory_get_container (priv->factory,
                                                                                                             object_id,
                                                                                                             title,
                                                                                                             (guint) 0,
                                                                                                             uri));

    object = RYGEL_MEDIA_OBJECT (container);
    if (uri) {
      gee_abstract_collection_add (GEE_ABSTRACT_COLLECTION (object->uris), uri);
    }
    container->total_deleted_child_count = (guint32) sqlite3_column_int64 (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_DELETED_CHILD_COUNT);
    container->update_id = (guint) sqlite3_column_int64 (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_CONTAINER_UPDATE_ID);

    break;
  }

  case 1: { /* item */
    RygelMediaItem *item = rygel_media_export_object_factory_get_item (priv->factory,
                                                                       parent,
                                                                       object_id,
                                                                       title,
                                                                       (const gchar *) sqlite3_column_text (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_CLASS));

    object = RYGEL_MEDIA_OBJECT (item);
    rygel_media_export_media_cache_fill_item (self, statement, item);
    if (uri) {
      rygel_media_item_add_uri (item, uri);
    }
    break;
  }

  default:
    g_assert_not_reached ();
  }
  if (object) {
    guint64 modified;

    rygel_media_object_set_modified (object,
                                     (guint64) sqlite3_column_int64 (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_TIMESTAMP));
    modified = rygel_media_object_get_modified (object);
    if ((modified == G_MAXINT64) && RYGEL_IS_MEDIA_ITEM (object)) {
      rygel_media_object_set_modified (object, 0);
      rygel_media_item_set_place_holder (RYGEL_MEDIA_ITEM (object), TRUE);
    }

    rygel_media_object_set_object_update_id (object, (guint) sqlite3_column_int64 (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_OBJECT_UPDATE_ID));
  }

  return object;
}

static void
rygel_media_export_media_cache_fill_item (RygelMediaExportMediaCache *self,
                                          sqlite3_stmt               *statement,
                                          RygelMediaItem             *item) {
  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self));
  g_return_if_fail (statement != NULL);
  g_return_if_fail (RYGEL_IS_MEDIA_ITEM (item));

  rygel_media_item_set_date (item,
                             (const gchar *) sqlite3_column_text (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_DATE));
  rygel_media_item_set_mime_type (item,
                                  (const gchar *) sqlite3_column_text (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_MIME_TYPE));
  rygel_media_item_set_dlna_profile (item, (const gchar *) sqlite3_column_text (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_DLNA_PROFILE));
  rygel_media_item_set_size (item,
                             sqlite3_column_int64 (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_SIZE));
  if (RYGEL_IS_AUDIO_ITEM (item)) {
    RygelAudioItem *audio_item = RYGEL_AUDIO_ITEM (item);

    rygel_audio_item_set_duration (audio_item,
                                   (glong) sqlite3_column_int64 (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_DURATION));
    rygel_audio_item_set_bitrate (audio_item,
                                  sqlite3_column_int (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_BITRATE));
    rygel_audio_item_set_sample_freq (audio_item,
                                      sqlite3_column_int (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_SAMPLE_FREQ));
    rygel_audio_item_set_bits_per_sample (audio_item,
                                          sqlite3_column_int (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_BITS_PER_SAMPLE));
    rygel_audio_item_set_channels (audio_item, sqlite3_column_int (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_CHANNELS));
    if (RYGEL_IS_MUSIC_ITEM (item)) {
      RygelMusicItem *music_item = RYGEL_MUSIC_ITEM (item);

      rygel_music_item_set_artist (music_item, (const gchar *) sqlite3_column_text (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_AUTHOR));
      rygel_music_item_set_album (music_item, (const gchar *) sqlite3_column_text (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_ALBUM));
      rygel_music_item_set_genre (music_item, (const gchar *) sqlite3_column_text (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_GENRE));
      rygel_music_item_set_track_number (music_item, sqlite3_column_int (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_TRACK));
      rygel_music_item_lookup_album_art (music_item);
    }
  }
  if (RYGEL_IS_VISUAL_ITEM (item)) {
    RygelVisualItem *visual_item = RYGEL_VISUAL_ITEM (item);

    rygel_visual_item_set_width (visual_item, sqlite3_column_int (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_WIDTH));
    rygel_visual_item_set_height (visual_item, sqlite3_column_int (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_HEIGHT));
    rygel_visual_item_set_color_depth (visual_item, sqlite3_column_int (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_COLOR_DEPTH));

    if (RYGEL_IS_VIDEO_ITEM (item)) {
      RygelVideoItem *video_item = RYGEL_VIDEO_ITEM (item);

      rygel_video_item_set_author (video_item,
                                   (const gchar *) sqlite3_column_text (statement, (gint) RYGEL_MEDIA_EXPORT_DETAIL_COLUMN_AUTHOR));
    }
  }
}

static gchar *
rygel_media_export_media_cache_translate_search_expression (RygelSearchExpression  *expression,
                                                            GArray                 *args,
                                                            const gchar            *prefix,
                                                            GError                **error) {
  gchar *str;
  gchar* filter;
  GError *inner_error;

  g_return_val_if_fail (expression == NULL || RYGEL_IS_SEARCH_EXPRESSION (expression), NULL);
  g_return_val_if_fail (args != NULL, NULL);
  g_return_val_if_fail (prefix != NULL, NULL);

  if (!expression) {
    return g_strdup ("");
  }

  inner_error = NULL;
  filter = rygel_media_export_media_cache_search_expression_to_sql (expression, args, &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return NULL;
  }
  str = g_strdup_printf (" %s %s", prefix, filter);
  g_free (filter);
  return str;
}

static gchar *
rygel_media_export_media_cache_search_expression_to_sql (RygelSearchExpression  *expression,
                                                         GArray                 *args,
                                                         GError                **error) {
  GError *inner_error;

  g_return_val_if_fail (args != NULL, NULL);

  if (!expression) {
    return g_strdup ("");
  }

  inner_error = NULL;
  if (RYGEL_IS_LOGICAL_EXPRESSION (expression)) {
    gchar *str = rygel_media_export_media_cache_logical_expression_to_sql (RYGEL_LOGICAL_EXPRESSION (expression),
                                                                           args,
                                                                           &inner_error);
    if (inner_error) {
      g_propagate_error (error, inner_error);
      return NULL;
    }

    return str;
  } else {
    gchar *str = rygel_media_export_media_cache_relational_expression_to_sql (RYGEL_RELATIONAL_EXPRESSION (expression),
                                                                              args,
                                                                              &inner_error);
    if (inner_error) {
      g_propagate_error (error, inner_error);
      return NULL;
    }
    return str;
  }
}

static gchar *
rygel_media_export_media_cache_logical_expression_to_sql (RygelLogicalExpression  *expression,
                                                          GArray                  *args,
                                                          GError                 **error) {
  RygelSearchExpression *search_expression;
  gchar *str;
  gchar *left_sql_string;
  gchar *right_sql_string;
  const gchar *operator_sql_string;
  GError *inner_error;

  g_return_val_if_fail (RYGEL_IS_LOGICAL_EXPRESSION (expression), NULL);
  g_return_val_if_fail (args != NULL, NULL);

  search_expression = RYGEL_SEARCH_EXPRESSION (expression);
  inner_error = NULL;
  left_sql_string = rygel_media_export_media_cache_search_expression_to_sql (RYGEL_SEARCH_EXPRESSION (search_expression->operand1),
                                                                             args,
                                                                             &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return NULL;
  }

  right_sql_string = rygel_media_export_media_cache_search_expression_to_sql (RYGEL_SEARCH_EXPRESSION (search_expression->operand2),
                                                                              args,
                                                                              &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    g_free (left_sql_string);
    return NULL;
  }

  if (((RygelLogicalOperator) ((gintptr) search_expression->op)) == RYGEL_LOGICAL_OPERATOR_AND) {
    operator_sql_string = "AND";
  } else {
    operator_sql_string = "OR";
  }

  str = g_strdup_printf ("(%s %s %s)", left_sql_string, operator_sql_string, right_sql_string);
  g_free (right_sql_string);
  g_free (left_sql_string);
  return str;
}

static gchar *
rygel_media_export_media_cache_map_operand_to_column (const gchar  *operand,
                                                      gchar       **collate,
                                                      GError      **error) {
  const gchar *column;
  gboolean use_collation;

  g_return_val_if_fail (operand != NULL, NULL);

  column = NULL;
  use_collation = FALSE;

  if (!g_strcmp0 (operand, "res")) {
    column = "o.uri";
  } else if (!g_strcmp0 (operand, "res@duration")) {
    column = "m.duration";
  } else if (!g_strcmp0 (operand, "@refID")) {
    column = "NULL";
  } else if (!g_strcmp0 (operand, "@id")) {
    column = "o.upnp_id";
  } else if (!g_strcmp0 (operand, "@parentID")) {
    column = "o.parent";
  } else if (!g_strcmp0 (operand, "upnp:class")) {
    column = "m.class";
  } else if (!g_strcmp0 (operand, "dc:title")) {
    column = "o.title";
    use_collation = TRUE;
  } else if (!g_strcmp0 (operand, "upnp:artist") || !g_strcmp0(operand, "dc:creator")) {
    column = "m.author";
    use_collation = TRUE;
  } else if (!g_strcmp0 (operand, "dc:date")) {
    column = "strftime(\"%Y\", m.date)";
  } else if (!g_strcmp0 (operand, "upnp:album")) {
    column = "m.album";
    use_collation = TRUE;
  } else if (!g_strcmp0 (operand, "upnp:genre") || !g_strcmp0(operand, "dc:genre")) {
    column = "m.genre";
    use_collation = TRUE;
  } else if (!g_strcmp0 (operand, "upnp:originalTrackNumber")) {
    column = "m.track";
  } else if (!g_strcmp0 (operand, "rygel:originalVolumeNumber")) {
    column = "m.disc";
  } else if (!g_strcmp0 (operand, "upnp:objectUpdateID")) {
    column = "o.object_update_id";
  } else if (!g_strcmp0 (operand, "upnp:containerUpdateID")) {
    column = "o.container_update_id";
  } else {
    g_set_error (error,
                 RYGEL_MEDIA_EXPORT_MEDIA_CACHE_ERROR,
                 RYGEL_MEDIA_EXPORT_MEDIA_CACHE_ERROR_UNSUPPORTED_SEARCH,
                 "Unsupported column %s",
                 operand);
    return NULL;
  }
  if (collate) {
    *collate = g_strdup (use_collation ? "COLLATE CASEFOLD" : "");
  }
  return g_strdup (column);
}

static gchar *
rygel_media_export_media_cache_relational_expression_to_sql (RygelRelationalExpression  *exp,
                                                             GArray                     *args,
                                                             GError                    **error) {
  RygelSearchExpression *search_expression;
  gchar *str;
  GValue v = G_VALUE_INIT;
  gchar *collate;
  gchar *column;
  RygelMediaExportSqlOperator *operator;
  GError *inner_error;

  g_return_val_if_fail (RYGEL_IS_RELATIONAL_EXPRESSION (exp), NULL);
  g_return_val_if_fail (args != NULL, NULL);

  collate = NULL;
  search_expression = RYGEL_SEARCH_EXPRESSION (exp);
  inner_error = NULL;
  column = rygel_media_export_media_cache_map_operand_to_column ((const gchar *) search_expression->operand1, &collate, &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return NULL;
  }

  switch ((GUPnPSearchCriteriaOp) ((gintptr) search_expression->op)) {
  case GUPNP_SEARCH_CRITERIA_OP_EXISTS: {
    const gchar *sql_function;
    gchar *sql;

    if (g_strcmp0 ((const gchar *) search_expression->operand2, "true")) {
      sql_function = g_strdup ("%s IS NULL OR %s = ''");
    } else {
      sql_function = g_strdup ("%s IS NOT NULL AND %s != ''");
    }

    sql = g_strdup_printf (sql_function, column, column);
    g_free (column);
    g_free (collate);

    return sql;
  }
  case GUPNP_SEARCH_CRITERIA_OP_EQ:
  case GUPNP_SEARCH_CRITERIA_OP_NEQ:
  case GUPNP_SEARCH_CRITERIA_OP_LESS:
  case GUPNP_SEARCH_CRITERIA_OP_LEQ:
  case GUPNP_SEARCH_CRITERIA_OP_GREATER:
  case GUPNP_SEARCH_CRITERIA_OP_GEQ: {
    if (!g_strcmp0 (column, "m.class") &&
        ((GUPnPSearchCriteriaOp) ((gintptr) search_expression->op) == GUPNP_SEARCH_CRITERIA_OP_EQ) &&
        !g_strcmp0 ((const gchar *) search_expression->operand2, "object.container")) {
      g_value_init (&v, G_TYPE_INT);
      g_value_set_int (&v, (gint) RYGEL_MEDIA_EXPORT_OBJECT_TYPE_CONTAINER);
    } else {
      g_value_init (&v, G_TYPE_STRING);
      g_value_set_string (&v, (const gchar *) search_expression->operand2);
      operator = rygel_media_export_sql_operator_new_from_search_criteria_op ((GUPnPSearchCriteriaOp) ((gintptr) search_expression->op), column, collate);
    }
    break;
  }
  case GUPNP_SEARCH_CRITERIA_OP_CONTAINS: {
    operator = RYGEL_MEDIA_EXPORT_SQL_OPERATOR (rygel_media_export_sql_function_new ("contains", column));
    g_value_init (&v, G_TYPE_STRING);
    g_value_set_string (&v, (const gchar*) search_expression->operand2);
    break;
  }
  case GUPNP_SEARCH_CRITERIA_OP_DOES_NOT_CONTAIN: {
    operator = RYGEL_MEDIA_EXPORT_SQL_OPERATOR (rygel_media_export_sql_function_new ("NOT contains", column));
    g_value_init (&v, G_TYPE_STRING);
    g_value_set_string (&v, (const gchar*) search_expression->operand2);
    break;
  }
  case GUPNP_SEARCH_CRITERIA_OP_DERIVED_FROM: {
    if (!g_strcmp0 (column, "m.class") &&
        g_str_has_prefix ((const gchar *) search_expression->operand2, "object.container")) {
      operator = rygel_media_export_sql_operator_new ("=", "o.type_fk", "");
      g_value_init (&v, G_TYPE_INT);
      g_value_set_int (&v, (gint) RYGEL_MEDIA_EXPORT_OBJECT_TYPE_CONTAINER);
    } else {
      operator = rygel_media_export_sql_operator_new ("LIKE", column, "");
      g_value_init (&v, G_TYPE_STRING);
      g_value_take_string (&v, g_strdup_printf ("%s%%", (const gchar*) search_expression->operand2));
    }
    break;
  }
  default:
    g_warning ("Unsupported op %d",
               (gint) ((GUPnPSearchCriteriaOp) ((gintptr) search_expression->op)));
    g_free (column);
    g_free (collate);
    return NULL;
  }
  g_array_append_val (args, v);
  str = rygel_media_export_sql_operator_to_string (operator);
  g_object_unref (operator);
  g_free (column);
  g_free (collate);

  return str;
}

static RygelMediaExportDatabaseCursor *
rygel_media_export_media_cache_exec_cursor (RygelMediaExportMediaCache  *self,
                                            RygelMediaExportSQLString    id,
                                            GValue                      *values,
                                            int                          values_length,
                                            GError                     **error) {
  RygelMediaExportDatabaseCursor *cursor;
  GError *inner_error;
  RygelMediaExportMediaCachePrivate *priv;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self), NULL);

  priv = self->priv;
  inner_error = NULL;
  cursor = rygel_media_export_database_exec_cursor (priv->db,
                                                    rygel_media_export_sql_factory_make (priv->sql, id),
                                                    values,
                                                    values_length,
                                                    &inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return NULL;
  }

  return cursor;
}

static gint
rygel_media_export_media_cache_query_value (RygelMediaExportMediaCache  *self,
                                            RygelMediaExportSQLString    id,
                                            GValue                      *values,
                                            gint                         values_length,
                                            GError                     **error) {
  gint value;
  GError *inner_error;
  RygelMediaExportMediaCachePrivate *priv;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_MEDIA_CACHE (self), 0);

  priv = self->priv;
  inner_error = NULL;
  value = rygel_media_export_database_query_value (priv->db,
                                                   rygel_media_export_sql_factory_make (priv->sql, id),
                                                   values,
                                                   values_length,
                                                   &inner_error);

  if (inner_error) {
    g_propagate_error (error, inner_error);
    return 0;
  }

  return value;
}

static gchar *
rygel_media_export_media_cache_translate_sort_criteria (const gchar *sort_criteria) {
  gchar *order_str;
  gchar *str;
  gchar **iter;
  gchar **fields;
  GPtrArray *order_list;

  g_return_val_if_fail (sort_criteria != NULL, NULL);

  order_list = g_ptr_array_new_with_free_func (g_free);
  fields = g_strsplit (sort_criteria, ",", 0);

  for (iter = fields; *iter; ++iter) {
    const gchar *order;
    gchar *field = *iter;
    gchar *slice = rygel_media_export_string_slice (field, 1, strlen (field));
    gchar *collate = NULL;
    GError *inner_error = NULL;
    gchar *column = rygel_media_export_media_cache_map_operand_to_column (slice,
                                                                          &collate,
                                                                          &inner_error);

    g_free (slice);
    if (inner_error) {
      g_warning ("Skipping unsupported field: %s", field);
      g_error_free (inner_error);
      continue;
    }
    if (field[0] == '-') {
      order = "DESC";
    } else {
      order = "ASC";
    }

    g_ptr_array_add (order_list,
                     g_strdup_printf ("%s %s %s ", column, collate, order));
    g_free (column);
    g_free (collate);
  }
  g_ptr_array_add (order_list, NULL);
  order_str = g_strjoinv (",", (gchar **) order_list->pdata);
  g_ptr_array_unref (order_list);
  str = g_strdup_printf ("ORDER BY %s", order_str);
  g_free (order_str);
  g_strfreev (fields);

  return str;
}

static void
rygel_media_export_media_cache_class_init (RygelMediaExportMediaCacheClass *cache_class) {
  GObjectClass *object_class = G_OBJECT_CLASS (cache_class);

  object_class->dispose = rygel_media_export_media_cache_dispose;
  g_type_class_add_private (cache_class,
                            sizeof (RygelMediaExportMediaCachePrivate));
}

static void
rygel_media_export_media_cache_init (RygelMediaExportMediaCache *self) {
  self->priv = RYGEL_MEDIA_EXPORT_MEDIA_CACHE_GET_PRIVATE (self);
}

static void
rygel_media_export_media_cache_dispose (GObject *object) {
  RygelMediaExportMediaCache *self = RYGEL_MEDIA_EXPORT_MEDIA_CACHE (object);
  RygelMediaExportMediaCachePrivate *priv = self->priv;

  if (priv->db) {
    RygelMediaExportDatabase *db = priv->db;

    priv->db = NULL;
    g_object_unref (db);
  }
  if (priv->factory) {
    RygelMediaExportObjectFactory *factory = priv->factory;

    priv->factory = NULL;
    g_object_unref (factory);
  }
  if (priv->sql) {
    RygelMediaExportSQLFactory *sql = priv->sql;

    priv->sql = NULL;
    g_object_unref (sql);
  }
  if (priv->exists_cache) {
    GeeHashMap *exists_cache = priv->exists_cache;

    priv->exists_cache = NULL;
    g_object_unref (exists_cache);
  }
  G_OBJECT_CLASS (rygel_media_export_media_cache_parent_class)->dispose (object);
}
