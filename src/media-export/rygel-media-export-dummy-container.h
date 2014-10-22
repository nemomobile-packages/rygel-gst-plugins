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

#ifndef __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_DUMMY_CONTAINER_H__
#define __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_DUMMY_CONTAINER_H__

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gee.h>

#include "rygel-media-export-trackable-db-container.h"

G_BEGIN_DECLS

#define RYGEL_MEDIA_EXPORT_TYPE_DUMMY_CONTAINER (rygel_media_export_dummy_container_get_type ())
#define RYGEL_MEDIA_EXPORT_DUMMY_CONTAINER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RYGEL_MEDIA_EXPORT_TYPE_DUMMY_CONTAINER, RygelMediaExportDummyContainer))
#define RYGEL_MEDIA_EXPORT_DUMMY_CONTAINER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RYGEL_MEDIA_EXPORT_TYPE_DUMMY_CONTAINER, RygelMediaExportDummyContainerClass))
#define RYGEL_MEDIA_EXPORT_IS_DUMMY_CONTAINER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RYGEL_MEDIA_EXPORT_TYPE_DUMMY_CONTAINER))
#define RYGEL_MEDIA_EXPORT_IS_DUMMY_CONTAINER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RYGEL_MEDIA_EXPORT_TYPE_DUMMY_CONTAINER))
#define RYGEL_MEDIA_EXPORT_DUMMY_CONTAINER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RYGEL_MEDIA_EXPORT_TYPE_DUMMY_CONTAINER, RygelMediaExportDummyContainerClass))

typedef struct _RygelMediaExportDummyContainer RygelMediaExportDummyContainer;
typedef struct _RygelMediaExportDummyContainerClass RygelMediaExportDummyContainerClass;
typedef struct _RygelMediaExportDummyContainerPrivate RygelMediaExportDummyContainerPrivate;

struct _RygelMediaExportDummyContainer {
  RygelMediaExportTrackableDbContainer parent_instance;
  RygelMediaExportDummyContainerPrivate* priv;
};

struct _RygelMediaExportDummyContainerClass {
  RygelMediaExportTrackableDbContainerClass parent_class;
};

GType
rygel_media_export_dummy_container_get_type (void) G_GNUC_CONST;

RygelMediaExportDummyContainer *
rygel_media_export_dummy_container_new (GFile               *file,
                                        RygelMediaContainer *parent);

GFile *
rygel_media_export_dummy_container_get_g_file (RygelMediaExportDummyContainer *self);

GeeList *
rygel_media_export_dummy_container_get_children_list (RygelMediaExportDummyContainer *self);

void
rygel_media_export_dummy_container_seen (RygelMediaExportDummyContainer *self,
                                         GFile                          *file);

G_END_DECLS

#endif /* __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_DUMMY_CONTAINER_H__ */
