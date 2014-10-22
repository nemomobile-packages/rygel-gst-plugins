/*
 * Copyright (C) 2011 Jens Georg <mail@jensge.org>.
 * Copyright (C) 2013 Intel Corporation.
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

#include "rygel-media-export-leaf-query-container.h"
#include "rygel-media-export-node-query-container.h"
#include "rygel-media-export-query-container-factory.h"
#include "rygel-media-export-string-utils.h"

G_DEFINE_TYPE (RygelMediaExportQueryContainerFactory,
               rygel_media_export_query_container_factory,
               G_TYPE_OBJECT)

#define rygel_search_expression_unref(var) ((var == NULL) ? NULL : (var = (rygel_search_expression_unref (var), NULL)))

struct _RygelMediaExportQueryContainerFactoryPrivate {
  GeeHashMap *virtual_container_map;
};

static RygelMediaExportQueryContainerFactory* rygel_media_export_query_container_factory_instance;

#define RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_FACTORY_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                RYGEL_MEDIA_EXPORT_TYPE_QUERY_CONTAINER_FACTORY, \
                                RygelMediaExportQueryContainerFactoryPrivate))

static RygelMediaExportQueryContainerFactory* rygel_media_export_query_container_factory_new (void);
static RygelSearchExpression* rygel_media_export_query_container_factory_parse_description (const gchar* description, gchar** pattern, gchar** attribute, gchar** upnp_class, gchar** name);
static gchar* rygel_media_export_query_container_factory_map_upnp_class (const gchar* attribute);
static void rygel_media_export_query_container_factory_update_search_expression (RygelSearchExpression** expression, const gchar* key, const gchar* value);

RygelMediaExportQueryContainerFactory *
rygel_media_export_query_container_factory_get_default (void) {
  if (G_UNLIKELY (!rygel_media_export_query_container_factory_instance)) {
    rygel_media_export_query_container_factory_instance = rygel_media_export_query_container_factory_new ();
  }

  return g_object_ref (rygel_media_export_query_container_factory_instance);
}

static RygelMediaExportQueryContainerFactory *
rygel_media_export_query_container_factory_new (void) {
  return RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_FACTORY (g_object_new (RYGEL_MEDIA_EXPORT_TYPE_QUERY_CONTAINER_FACTORY,
                                                                   NULL));
}

/**
 * Register a plaintext description for a query container. The passed
 * string will be modified to the checksum id of the container.
 *
 * @param id Originally contains the plaintext id which is replaced with
 *           the hashed id on return.
 */
void
rygel_media_export_query_container_factory_register_id (RygelMediaExportQueryContainerFactory  *self,
                                                        gchar                                 **id) {
  gchar *md5;
  gchar *tmp;
  GeeAbstractMap *abstract_virtuals;
  RygelMediaExportQueryContainerFactoryPrivate *priv;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_QUERY_CONTAINER_FACTORY (self));
  g_return_if_fail (id != NULL);
  g_return_if_fail (*id != NULL);

  priv = self->priv;
  md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, *id, (gsize) (-1));
  abstract_virtuals = GEE_ABSTRACT_MAP (priv->virtual_container_map);

  if (!gee_abstract_map_has_key (abstract_virtuals, md5)) {
    gee_abstract_map_set (abstract_virtuals, md5, *id);
    g_debug ("Registering %s for %s", md5, *id);
  }
  tmp = g_strconcat (RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_PREFIX, md5, NULL);
  g_free (md5);
  g_free (*id);
  *id = tmp;
}

/**
 * Get the plaintext definition from a hashed id.
 *
 * Inverse function of register_id().
 *
 * @param hash A hashed id
 * @return the plaintext defintion of the virtual folder
 */
gchar *
rygel_media_export_query_container_factory_get_virtual_container_definition (RygelMediaExportQueryContainerFactory *self,
                                                                             const gchar                           *hash) {
  gchar *definition;
  gchar *id;
  GError *inner_error;
  RygelMediaExportQueryContainerFactoryPrivate *priv;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_QUERY_CONTAINER_FACTORY (self), NULL);
  g_return_val_if_fail (hash != NULL, NULL);

  priv = self->priv;
  inner_error = NULL;
  id = rygel_media_export_string_replace (hash,
                                          RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_PREFIX,
                                          "",
                                          &inner_error);
  if (inner_error) {
    g_warning ("Failed to remove " RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_PREFIX " from %s: %s",
               hash,
               inner_error->message);
    g_error_free (inner_error);
    return NULL;
  }
  definition = gee_abstract_map_get (GEE_ABSTRACT_MAP (priv->virtual_container_map), id);
  g_free (id);
  return definition;
}

/**
 * Factory method.
 *
 * Create a QueryContainer directly from MD5 hashed id.
 *
 * @param cache An instance of the meta-data cache
 * @param id    The hashed id of the container
 * @param name  An the title of the container. If not supplied, it will
 *              be derived from the plain-text description of the
 *              container
 * @return A new instance of QueryContainer or null if id does not exist
 */
RygelMediaExportQueryContainer *
rygel_media_export_query_container_factory_create_from_id (RygelMediaExportQueryContainerFactory *self,
                                                           const gchar                           *id,
                                                           const gchar                           *name) {
  RygelMediaExportQueryContainer *container;
  gchar *definition;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_QUERY_CONTAINER_FACTORY (self), NULL);
  g_return_val_if_fail (id != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  definition = rygel_media_export_query_container_factory_get_virtual_container_definition (self, id);
  if (!definition) {
    g_free (definition);
    return NULL;
  }

  container = rygel_media_export_query_container_factory_create_from_description (self, definition, name);
  g_free (definition);
  return container;
}

/**
 * Factory method.
 *
 * Create a QueryContainer from a plain-text description string.
 *
 * @param definition Plain-text defintion of the query-container
 * @param name       The title of the container. If not supplied, it
 *                   will be derived from the plain-text description of
 *                   the container
 * @return A new instance of QueryContainer
 */
RygelMediaExportQueryContainer *
rygel_media_export_query_container_factory_create_from_description (RygelMediaExportQueryContainerFactory *self,
                                                                    const gchar                           *definition,
                                                                    const gchar                           *name) {
  gchar *title;
  gchar *attribute;
  gchar *pattern;
  gchar *upnp_class;
  gchar *id;
  RygelMediaExportQueryContainer *container;
  RygelSearchExpression *expression;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_QUERY_CONTAINER_FACTORY (self), NULL);
  g_return_val_if_fail (definition != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  title = g_strdup (name);
  attribute = NULL;
  pattern = NULL;
  upnp_class = NULL;

  id = g_strdup (definition);
  rygel_media_export_query_container_factory_register_id (self, &id);
  expression = rygel_media_export_query_container_factory_parse_description (definition,
                                                                             &pattern,
                                                                             &attribute,
                                                                             &upnp_class,
                                                                             &title);
  if (!pattern || pattern[0] == '\0') {
    container = RYGEL_MEDIA_EXPORT_QUERY_CONTAINER (rygel_media_export_leaf_query_container_new (expression, id, title));
  } else {
    container = RYGEL_MEDIA_EXPORT_QUERY_CONTAINER (rygel_media_export_node_query_container_new (expression, id, title, pattern, attribute));
  }

  if (upnp_class) {
    rygel_media_object_set_upnp_class (RYGEL_MEDIA_OBJECT (container), upnp_class);
    if (!g_strcmp0 (upnp_class, RYGEL_MEDIA_CONTAINER_MUSIC_ALBUM)) {
      rygel_media_container_set_sort_criteria (RYGEL_MEDIA_CONTAINER (container),
                                               RYGEL_MEDIA_CONTAINER_ALBUM_SORT_CRITERIA);
    }
  }
  rygel_search_expression_unref (expression);
  g_free (id);
  g_free (upnp_class);
  g_free (pattern);
  g_free (attribute);
  g_free (title);

  return container;
}

/**
 * Map a DIDL attribute to a UPnP container class.
 *
 * @return A matching UPnP class for the attribute or null if it can't be
 *         mapped.
 */
static gchar *
rygel_media_export_query_container_factory_map_upnp_class (const gchar *attribute) {
  const gchar *upnp_class;

  g_return_val_if_fail (attribute != NULL, NULL);

  if (!g_strcmp0 (attribute, "upnp:album")) {
    upnp_class = RYGEL_MEDIA_CONTAINER_MUSIC_ALBUM;
  } else if (!g_strcmp0 (attribute, "dc:creator") || !g_strcmp0 (attribute, "upnp:artist")) {
    upnp_class = RYGEL_MEDIA_CONTAINER_MUSIC_ARTIST;
  } else if (!g_strcmp0 (attribute, "dc:genre")) {
    upnp_class = RYGEL_MEDIA_CONTAINER_MUSIC_GENRE;
  } else {
    upnp_class = NULL;
  }

  return g_strdup (upnp_class);
}

/**
 * Parse a plaintext container description into a search expression.
 *
 * Also generates a name for the container and other meta-data necessary
 * for node containers.
 *
 * @param description The plaintext container description
 * @param pattern     Contains the pattern used for child containers if
 *                    descrption is for a node container, null otherwise.
 * @param attribute   Contains the UPnP attribute the container describes.
 * @param name        If passed empty, name will be generated from the
 *                    description.
 * @return A SearchExpression corresponding to the non-variable part of
 *         the description.
 */
static RygelSearchExpression *
rygel_media_export_query_container_factory_parse_description (const gchar  *description,
                                                              gchar       **pattern,
                                                              gchar       **attribute,
                                                              gchar       **upnp_class,
                                                              gchar       **name) {
  gchar *local_pattern;
  gchar *local_attribute;
  gchar *local_upnp_class;
  gchar **args;
  gint args_length;
  RygelSearchExpression* expression;
  gint i;

  g_return_val_if_fail (description != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (*name != NULL, NULL);

  args = g_strsplit (description, ",", 0);
  args_length = g_strv_length (args);
  expression = NULL;
  local_pattern = NULL;
  local_attribute = NULL;
  local_upnp_class = NULL;
  i = 0;

  while (i < args_length) {
    gchar *previous_attribute = g_strdup (local_attribute);
    GError *error = NULL;
    gchar *tmp_attribute = rygel_media_export_string_replace (args[i],
                                                              RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_PREFIX,
                                                              "",
                                                              &error);

    if (error) {
      g_warning ("Failed to remove " RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_PREFIX " from %s: %s",
                 args[i],
                 error->message);
      g_error_free (error);
    }
    g_free (local_attribute);
    local_attribute = g_uri_unescape_string (tmp_attribute, NULL);
    g_free (tmp_attribute);
    if (i + 1 == args_length) {
      /* i is always even - incremented by 2 after every iteration. */
      g_warning ("Count of args in description should be even.");
      g_free (local_attribute);
    }
    if (g_strcmp0 (args[i + 1], "?")) {
      rygel_media_export_query_container_factory_update_search_expression (&expression, args[i], args[i + 1]);
      if ((i + 2) == args_length) {
        local_upnp_class = rygel_media_export_query_container_factory_map_upnp_class (local_attribute);
      }
    } else {
      g_free (args[i + 1]);
      args[i + 1] = g_strdup ("%s");

      local_pattern = g_strjoinv (",", args);
      local_upnp_class = rygel_media_export_query_container_factory_map_upnp_class (previous_attribute);

      if (*name[0] == '\0' && i > 0) {
        g_free (*name);
        *name = g_uri_unescape_string (args[i - 1], NULL);
      }

      g_free (previous_attribute);
      break;
    }

    i += 2;
    g_free (previous_attribute);
  }
  g_strfreev (args);

  if (pattern) {
    *pattern = local_pattern;
  } else {
    g_free (local_pattern);
  }
  if (attribute) {
    *attribute = local_attribute;
  } else {
    g_free (local_attribute);
  }
  if (upnp_class) {
    *upnp_class = local_upnp_class;
  } else {
    g_free (local_upnp_class);
  }
  return expression;
}


/**
 * Update a SearchExpression with a new key = value condition.
 *
 * Will modifiy the passed expression to (expression AND (key = value))
 *
 * @param expression The expression to update or null to create a new one
 * @param key        Key of the key/value condition
 * @param value      Value of the key/value condition
 */
static void
rygel_media_export_query_container_factory_update_search_expression (RygelSearchExpression **expression,
                                                                     const gchar            *key,
                                                                     const gchar            *value) {
  RygelSearchExpression *search_sub;
  RygelRelationalExpression *subexpression;
  gchar *clean_key;
  GError *error;

  g_return_if_fail (expression != NULL);
  g_return_if_fail (*expression == NULL || RYGEL_IS_SEARCH_EXPRESSION (*expression));
  g_return_if_fail (key != NULL);
  g_return_if_fail (value != NULL);

  subexpression = rygel_relational_expression_new ();
  search_sub = RYGEL_SEARCH_EXPRESSION (subexpression);
  error = NULL;
  clean_key = rygel_media_export_string_replace (key,
                                                 RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_PREFIX,
                                                 "",
                                                 &error);
  if (error) {
    g_warning ("Failed to remove "  " from %s: %s",
               key,
               error->message);
    g_error_free (error);
  }
  search_sub->operand1 = g_uri_unescape_string (clean_key, NULL);
  search_sub->op = (gpointer) ((gintptr) GUPNP_SEARCH_CRITERIA_OP_EQ);
  search_sub->operand2 = g_uri_unescape_string (value, NULL);
  g_free (clean_key);
  if (*expression) {
    RygelLogicalExpression *conjunction = rygel_logical_expression_new ();
    RygelSearchExpression *search_conj = RYGEL_SEARCH_EXPRESSION (conjunction);

    search_conj->operand1 = *expression;
    search_conj->operand2 = subexpression;
    search_conj->op = (gpointer) ((gintptr) RYGEL_LOGICAL_OPERATOR_AND);
    *expression = search_conj;
  } else {
    *expression = search_sub;
  }
}

static void
rygel_media_export_query_container_factory_dispose (GObject *object) {
  RygelMediaExportQueryContainerFactory *self = RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_FACTORY (object);
  RygelMediaExportQueryContainerFactoryPrivate *priv = self->priv;

  if (priv->virtual_container_map) {
    GeeHashMap *map = priv->virtual_container_map;

    priv->virtual_container_map = NULL;
    g_object_unref (map);
  }
  G_OBJECT_CLASS (rygel_media_export_query_container_factory_parent_class)->dispose (object);
}

static void
rygel_media_export_query_container_factory_constructed (GObject *object) {
  RygelMediaExportQueryContainerFactory *self = RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_FACTORY (object);

  G_OBJECT_CLASS (rygel_media_export_query_container_factory_parent_class)->constructed (object);

  self->priv->virtual_container_map = gee_hash_map_new (G_TYPE_STRING,
                                                        (GBoxedCopyFunc) g_strdup,
                                                        g_free,
                                                        G_TYPE_STRING,
                                                        (GBoxedCopyFunc) g_strdup,
                                                        g_free,
                                                        NULL, NULL, NULL, NULL,
                                                        NULL, NULL, NULL, NULL, NULL);
}

static void
rygel_media_export_query_container_factory_class_init (RygelMediaExportQueryContainerFactoryClass *factory_class) {
  GObjectClass *object_class = G_OBJECT_CLASS (factory_class);

  object_class->dispose = rygel_media_export_query_container_factory_dispose;
  object_class->constructed = rygel_media_export_query_container_factory_constructed;
  g_type_class_add_private (factory_class,
                            sizeof (RygelMediaExportQueryContainerFactoryPrivate));
}

static void
rygel_media_export_query_container_factory_init (RygelMediaExportQueryContainerFactory *self) {
  self->priv = RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_FACTORY_GET_PRIVATE (self);
}
