/*
 * Copyright (C) 2010 Jens Georg <mail@jensge.org>.
 * Copyright (C) 2013 Intel Corporation.
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

#ifndef __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_OBJECT_FACTORY_H__
#define __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_OBJECT_FACTORY_H__

#include <glib.h>
#include <glib-object.h>
#include <rygel-server.h>

#include "rygel-media-export-db-container.h"
#include "rygel-media-export-media-cache.h"

G_BEGIN_DECLS

#define RYGEL_MEDIA_EXPORT_TYPE_OBJECT_FACTORY (rygel_media_export_object_factory_get_type ())
#define RYGEL_MEDIA_EXPORT_OBJECT_FACTORY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RYGEL_MEDIA_EXPORT_TYPE_OBJECT_FACTORY, RygelMediaExportObjectFactory))
#define RYGEL_MEDIA_EXPORT_OBJECT_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RYGEL_MEDIA_EXPORT_TYPE_OBJECT_FACTORY, RygelMediaExportObjectFactoryClass))
#define RYGEL_MEDIA_EXPORT_IS_OBJECT_FACTORY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RYGEL_MEDIA_EXPORT_TYPE_OBJECT_FACTORY))
#define RYGEL_MEDIA_EXPORT_IS_OBJECT_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RYGEL_MEDIA_EXPORT_TYPE_OBJECT_FACTORY))
#define RYGEL_MEDIA_EXPORT_OBJECT_FACTORY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RYGEL_MEDIA_EXPORT_TYPE_OBJECT_FACTORY, RygelMediaExportObjectFactoryClass))

typedef struct _RygelMediaExportObjectFactory RygelMediaExportObjectFactory;
typedef struct _RygelMediaExportObjectFactoryClass RygelMediaExportObjectFactoryClass;
typedef struct _RygelMediaExportObjectFactoryPrivate RygelMediaExportObjectFactoryPrivate;

struct _RygelMediaExportObjectFactory {
  GObject parent_instance;
  RygelMediaExportObjectFactoryPrivate *priv;
};

struct _RygelMediaExportObjectFactoryClass {
  GObjectClass parent_class;
  RygelMediaExportDBContainer* (* get_container) (RygelMediaExportObjectFactory *self,
                                                  const gchar                   *id,
                                                  const gchar                   *title,
                                                  guint                          child_count,
                                                  const gchar                   *uri);
  RygelMediaItem* (* get_item) (RygelMediaExportObjectFactory *self,
                                RygelMediaContainer           *parent,
                                const gchar                   *id,
                                const gchar                   *title,
                                const gchar                   *upnp_class);
};

GType
rygel_media_export_object_factory_get_type (void) G_GNUC_CONST;

RygelMediaExportDBContainer *
rygel_media_export_object_factory_get_container (RygelMediaExportObjectFactory* self,
                                                 const gchar* id,
                                                 const gchar* title,
                                                 guint child_count,
                                                 const gchar* uri);

RygelMediaItem *
rygel_media_export_object_factory_get_item (RygelMediaExportObjectFactory *self,
                                            RygelMediaContainer           *parent,
                                            const gchar                   *id,
                                            const gchar                   *title,
                                            const gchar                   *upnp_class);

RygelMediaExportObjectFactory *
rygel_media_export_object_factory_new (void);

G_END_DECLS

#endif
