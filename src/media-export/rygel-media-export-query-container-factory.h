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

#ifndef __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_QUERY_CONTAINER_FACTORY_H__
#define __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_QUERY_CONTAINER_FACTORY_H__

#include <glib.h>
#include <glib-object.h>

#include "rygel-media-export-media-cache.h"
#include "rygel-media-export-query-container.h"

G_BEGIN_DECLS

#define RYGEL_MEDIA_EXPORT_TYPE_QUERY_CONTAINER_FACTORY (rygel_media_export_query_container_factory_get_type ())
#define RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_FACTORY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RYGEL_MEDIA_EXPORT_TYPE_QUERY_CONTAINER_FACTORY, RygelMediaExportQueryContainerFactory))
#define RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RYGEL_MEDIA_EXPORT_TYPE_QUERY_CONTAINER_FACTORY, RygelMediaExportQueryContainerFactoryClass))
#define RYGEL_MEDIA_EXPORT_IS_QUERY_CONTAINER_FACTORY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RYGEL_MEDIA_EXPORT_TYPE_QUERY_CONTAINER_FACTORY))
#define RYGEL_MEDIA_EXPORT_IS_QUERY_CONTAINER_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RYGEL_MEDIA_EXPORT_TYPE_QUERY_CONTAINER_FACTORY))
#define RYGEL_MEDIA_EXPORT_QUERY_CONTAINER_FACTORY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RYGEL_MEDIA_EXPORT_TYPE_QUERY_CONTAINER_FACTORY, RygelMediaExportQueryContainerFactoryClass))

typedef struct _RygelMediaExportQueryContainerFactory RygelMediaExportQueryContainerFactory;
typedef struct _RygelMediaExportQueryContainerFactoryClass RygelMediaExportQueryContainerFactoryClass;
typedef struct _RygelMediaExportQueryContainerFactoryPrivate RygelMediaExportQueryContainerFactoryPrivate;

struct _RygelMediaExportQueryContainerFactory {
  GObject parent_instance;
  RygelMediaExportQueryContainerFactoryPrivate *priv;
};

struct _RygelMediaExportQueryContainerFactoryClass {
  GObjectClass parent_class;
};

GType
rygel_media_export_query_container_factory_get_type (void) G_GNUC_CONST;

RygelMediaExportQueryContainerFactory *
rygel_media_export_query_container_factory_get_default (void);

RygelMediaExportQueryContainer *
rygel_media_export_query_container_factory_create_from_id (RygelMediaExportQueryContainerFactory *self,
                                                           const gchar                           *id,
                                                           const gchar                           *name);

RygelMediaExportQueryContainer *
rygel_media_export_query_container_factory_create_from_description (RygelMediaExportQueryContainerFactory *self,
                                                                    const gchar                           *definition,
                                                                    const gchar                           *name);

void
rygel_media_export_query_container_factory_register_id (RygelMediaExportQueryContainerFactory  *self,
                                                        gchar                                 **id);

gchar *
rygel_media_export_query_container_factory_get_virtual_container_definition (RygelMediaExportQueryContainerFactory *self,
                                                                             const gchar                           *hash);

G_END_DECLS

#endif
