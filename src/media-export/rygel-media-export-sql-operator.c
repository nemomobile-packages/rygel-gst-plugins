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

#include "rygel-media-export-sql-operator.h"

G_DEFINE_TYPE (RygelMediaExportSqlOperator, rygel_media_export_sql_operator, G_TYPE_OBJECT)

#define RYGEL_MEDIA_EXPORT_SQL_OPERATOR_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                RYGEL_MEDIA_EXPORT_TYPE_SQL_OPERATOR, \
                                RygelMediaExportSqlOperatorPrivate))

enum {
  PROP_0,
  PROP_NAME,
  PROP_ARG,
  PROP_COLLATE
};

struct _RygelMediaExportSqlOperatorPrivate {
  gchar *name;
  gchar *arg;
  gchar *collate;
};

RygelMediaExportSqlOperator *
rygel_media_export_sql_operator_new (const gchar *name,
                                     const gchar *arg,
                                     const gchar *collate) {
  return RYGEL_MEDIA_EXPORT_SQL_OPERATOR (g_object_new (RYGEL_MEDIA_EXPORT_TYPE_SQL_OPERATOR,
                                                        "name", name,
                                                        "arg", arg,
                                                        "collate", collate,
                                                        NULL));
}

RygelMediaExportSqlOperator *
rygel_media_export_sql_operator_new_from_search_criteria_op (GUPnPSearchCriteriaOp  op,
                                                             const gchar           *arg,
                                                             const gchar           *collate) {
  const gchar* sql;

  g_return_val_if_fail (arg != NULL, NULL);
  g_return_val_if_fail (collate != NULL, NULL);

  switch (op) {
  case GUPNP_SEARCH_CRITERIA_OP_EQ:
    sql = "=";
    break;
  case GUPNP_SEARCH_CRITERIA_OP_NEQ:
    sql = "!=";
    break;
  case GUPNP_SEARCH_CRITERIA_OP_LESS:
    sql = "<";
    break;
  case GUPNP_SEARCH_CRITERIA_OP_LEQ:
    sql = "<=";
    break;
  case GUPNP_SEARCH_CRITERIA_OP_GREATER:
    sql = ">";
    break;
  case GUPNP_SEARCH_CRITERIA_OP_GEQ:
    sql = ">=";
    break;
  default:
    g_assert_not_reached ();
  }

  return rygel_media_export_sql_operator_new (sql, arg, collate);
}

static gchar *
rygel_media_export_sql_operator_real_to_string (RygelMediaExportSqlOperator *self) {
  RygelMediaExportSqlOperatorPrivate *priv = self->priv;

  return g_strdup_printf ("(%s %s ? %s)",
                          priv->arg,
                          priv->name,
                          priv->collate);
}

gchar*
rygel_media_export_sql_operator_to_string (RygelMediaExportSqlOperator *self) {
  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_SQL_OPERATOR (self), NULL);

  return RYGEL_MEDIA_EXPORT_SQL_OPERATOR_GET_CLASS (self)->to_string (self);
}

static void
rygel_media_export_sql_operator_get_property (GObject    *object,
                                              guint       property_id,
                                              GValue     *value,
                                              GParamSpec *pspec) {
  RygelMediaExportSqlOperator *self = RYGEL_MEDIA_EXPORT_SQL_OPERATOR (object);
  RygelMediaExportSqlOperatorPrivate *priv = self->priv;

  switch (property_id) {
  case PROP_NAME:
    g_value_set_string (value, priv->name);
    break;

  case PROP_ARG:
    g_value_set_string (value, priv->arg);
    break;

  case PROP_COLLATE:
    g_value_set_string (value, priv->collate);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
rygel_media_export_sql_operator_set_property (GObject      *object,
                                              guint         property_id,
                                              const GValue *value,
                                              GParamSpec   *pspec) {
  RygelMediaExportSqlOperator *self = RYGEL_MEDIA_EXPORT_SQL_OPERATOR (object);
  RygelMediaExportSqlOperatorPrivate *priv = self->priv;

  switch (property_id) {
  case PROP_NAME:
    priv->name = g_value_dup_string (value);
    break;

  case PROP_ARG:
    priv->arg = g_value_dup_string (value);
    break;

  case PROP_COLLATE:
    priv->collate = g_value_dup_string (value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
rygel_media_export_sql_operator_finalize (GObject *object) {
  RygelMediaExportSqlOperator *self = RYGEL_MEDIA_EXPORT_SQL_OPERATOR (object);
  RygelMediaExportSqlOperatorPrivate *priv = self->priv;

  g_free (priv->name);
  g_free (priv->arg);
  g_free (priv->collate);

  G_OBJECT_CLASS (rygel_media_export_sql_operator_parent_class)->finalize (object);
}

static void
rygel_media_export_sql_operator_class_init (RygelMediaExportSqlOperatorClass *operator_class) {
  GObjectClass *object_class = G_OBJECT_CLASS (operator_class);

  operator_class->to_string = rygel_media_export_sql_operator_real_to_string;
  object_class->finalize = rygel_media_export_sql_operator_finalize;
  object_class->get_property = rygel_media_export_sql_operator_get_property;
  object_class->set_property = rygel_media_export_sql_operator_set_property;

  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "name",
                                                        "name",
                                                        NULL,
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
                                   PROP_ARG,
                                   g_param_spec_string ("arg",
                                                        "arg",
                                                        "arg",
                                                        NULL,
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
                                   PROP_COLLATE,
                                   g_param_spec_string ("collate",
                                                        "collate",
                                                        "collate",
                                                        NULL,
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (operator_class,
                            sizeof (RygelMediaExportSqlOperatorPrivate));
}

static void rygel_media_export_sql_operator_init (RygelMediaExportSqlOperator *self) {
  self->priv = RYGEL_MEDIA_EXPORT_SQL_OPERATOR_GET_PRIVATE (self);
}

const gchar *
rygel_media_export_sql_operator_get_name (RygelMediaExportSqlOperator *self)
{
  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_SQL_OPERATOR (self), NULL);

  return self->priv->name;
}

const gchar *
rygel_media_export_sql_operator_get_arg (RygelMediaExportSqlOperator *self)
{
  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_SQL_OPERATOR (self), NULL);

  return self->priv->arg;
}
