/*
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

#ifndef __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_LEAF_QUERY_CONTAINER_H__
#define __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_LEAF_QUERY_CONTAINER_H__

#include <glib.h>
#include <glib-object.h>
#include <rygel-server.h>

#include "rygel-media-export-query-container.h"

G_BEGIN_DECLS

#define RYGEL_MEDIA_EXPORT_TYPE_LEAF_QUERY_CONTAINER (rygel_media_export_leaf_query_container_get_type ())
#define RYGEL_MEDIA_EXPORT_LEAF_QUERY_CONTAINER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RYGEL_MEDIA_EXPORT_TYPE_LEAF_QUERY_CONTAINER, RygelMediaExportLeafQueryContainer))
#define RYGEL_MEDIA_EXPORT_LEAF_QUERY_CONTAINER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RYGEL_MEDIA_EXPORT_TYPE_LEAF_QUERY_CONTAINER, RygelMediaExportLeafQueryContainerClass))
#define RYGEL_MEDIA_EXPORT_IS_LEAF_QUERY_CONTAINER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RYGEL_MEDIA_EXPORT_TYPE_LEAF_QUERY_CONTAINER))
#define RYGEL_MEDIA_EXPORT_IS_LEAF_QUERY_CONTAINER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RYGEL_MEDIA_EXPORT_TYPE_LEAF_QUERY_CONTAINER))
#define RYGEL_MEDIA_EXPORT_LEAF_QUERY_CONTAINER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RYGEL_MEDIA_EXPORT_TYPE_LEAF_QUERY_CONTAINER, RygelMediaExportLeafQueryContainerClass))

typedef struct _RygelMediaExportLeafQueryContainer RygelMediaExportLeafQueryContainer;
typedef struct _RygelMediaExportLeafQueryContainerClass RygelMediaExportLeafQueryContainerClass;
typedef struct _RygelMediaExportLeafQueryContainerPrivate RygelMediaExportLeafQueryContainerPrivate;

struct _RygelMediaExportLeafQueryContainer {
  RygelMediaExportQueryContainer parent_instance;
  RygelMediaExportLeafQueryContainerPrivate *priv;
};

struct _RygelMediaExportLeafQueryContainerClass {
  RygelMediaExportQueryContainerClass parent_class;
};

GType
rygel_media_export_leaf_query_container_get_type (void) G_GNUC_CONST;

RygelMediaExportLeafQueryContainer *
rygel_media_export_leaf_query_container_new (RygelSearchExpression *expression,
                                             const gchar           *id,
                                             const gchar           *name);

G_END_DECLS

#endif /* __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_LEAF_QUERY_CONTAINER_H__ */
