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


#include "rygel-media-export-sql-function.h"

G_DEFINE_TYPE (RygelMediaExportSqlFunction,
               rygel_media_export_sql_function,
               RYGEL_MEDIA_EXPORT_TYPE_SQL_OPERATOR)

RygelMediaExportSqlFunction *
rygel_media_export_sql_function_new (const gchar *name,
                                     const gchar *arg) {
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (arg != NULL, NULL);

  return RYGEL_MEDIA_EXPORT_SQL_FUNCTION (g_object_new (RYGEL_MEDIA_EXPORT_TYPE_SQL_FUNCTION,
                                                        "name", name,
                                                        "arg", arg,
                                                        "collate", "",
                                                        NULL));
}

static gchar *
rygel_media_export_sql_function_real_to_string (RygelMediaExportSqlOperator *base) {
  return g_strdup_printf ("%s(%s,?)",
                          rygel_media_export_sql_operator_get_name (base),
                          rygel_media_export_sql_operator_get_arg (base));
}

static void
rygel_media_export_sql_function_class_init (RygelMediaExportSqlFunctionClass *function_class) {
  RygelMediaExportSqlOperatorClass *operator_class = RYGEL_MEDIA_EXPORT_SQL_OPERATOR_CLASS (function_class);

  operator_class->to_string = rygel_media_export_sql_function_real_to_string;
}

static void rygel_media_export_sql_function_init (RygelMediaExportSqlFunction *self G_GNUC_UNUSED) {
}
