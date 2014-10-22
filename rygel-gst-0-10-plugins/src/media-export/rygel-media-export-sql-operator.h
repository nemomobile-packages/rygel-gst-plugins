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

#ifndef __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_SQL_OPERATOR_H__
#define __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_SQL_OPERATOR_H__

#include <glib.h>
#include <glib-object.h>
#include <libgupnp-av/gupnp-av.h>

G_BEGIN_DECLS

#define RYGEL_MEDIA_EXPORT_TYPE_SQL_OPERATOR (rygel_media_export_sql_operator_get_type ())
#define RYGEL_MEDIA_EXPORT_SQL_OPERATOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RYGEL_MEDIA_EXPORT_TYPE_SQL_OPERATOR, RygelMediaExportSqlOperator))
#define RYGEL_MEDIA_EXPORT_SQL_OPERATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RYGEL_MEDIA_EXPORT_TYPE_SQL_OPERATOR, RygelMediaExportSqlOperatorClass))
#define RYGEL_MEDIA_EXPORT_IS_SQL_OPERATOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RYGEL_MEDIA_EXPORT_TYPE_SQL_OPERATOR))
#define RYGEL_MEDIA_EXPORT_IS_SQL_OPERATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RYGEL_MEDIA_EXPORT_TYPE_SQL_OPERATOR))
#define RYGEL_MEDIA_EXPORT_SQL_OPERATOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RYGEL_MEDIA_EXPORT_TYPE_SQL_OPERATOR, RygelMediaExportSqlOperatorClass))

typedef struct _RygelMediaExportSqlOperator RygelMediaExportSqlOperator;
typedef struct _RygelMediaExportSqlOperatorClass RygelMediaExportSqlOperatorClass;
typedef struct _RygelMediaExportSqlOperatorPrivate RygelMediaExportSqlOperatorPrivate;

struct _RygelMediaExportSqlOperator {
  GObject parent_instance;
  RygelMediaExportSqlOperatorPrivate * priv;
};

struct _RygelMediaExportSqlOperatorClass {
  GObjectClass parent_class;

  gchar * (* to_string) (RygelMediaExportSqlOperator *self);
};

GType
rygel_media_export_sql_operator_get_type (void) G_GNUC_CONST;

RygelMediaExportSqlOperator *
rygel_media_export_sql_operator_new (const gchar *name,
                                     const gchar *arg,
                                     const gchar *collate);

RygelMediaExportSqlOperator *
rygel_media_export_sql_operator_new_from_search_criteria_op (GUPnPSearchCriteriaOp  op,
                                                             const gchar           *arg,
                                                             const gchar           *collate);

gchar *
rygel_media_export_sql_operator_to_string (RygelMediaExportSqlOperator *self);

const gchar *
rygel_media_export_sql_operator_get_name (RygelMediaExportSqlOperator *self);

const gchar *
rygel_media_export_sql_operator_get_arg (RygelMediaExportSqlOperator *self);

G_END_DECLS

#endif /* __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_SQL_OPERATOR_H__ */
