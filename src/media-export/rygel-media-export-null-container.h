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

#ifndef __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_NULL_CONTAINER_H__
#define __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_NULL_CONTAINER_H__

#include <glib.h>
#include <glib-object.h>
#include <rygel-server.h>

G_BEGIN_DECLS

#define RYGEL_MEDIA_EXPORT_TYPE_NULL_CONTAINER (rygel_media_export_null_container_get_type ())
#define RYGEL_MEDIA_EXPORT_NULL_CONTAINER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RYGEL_MEDIA_EXPORT_TYPE_NULL_CONTAINER, RygelMediaExportNullContainer))
#define RYGEL_MEDIA_EXPORT_NULL_CONTAINER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RYGEL_MEDIA_EXPORT_TYPE_NULL_CONTAINER, RygelMediaExportNullContainerClass))
#define RYGEL_MEDIA_EXPORT_IS_NULL_CONTAINER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RYGEL_MEDIA_EXPORT_TYPE_NULL_CONTAINER))
#define RYGEL_MEDIA_EXPORT_IS_NULL_CONTAINER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RYGEL_MEDIA_EXPORT_TYPE_NULL_CONTAINER))
#define RYGEL_MEDIA_EXPORT_NULL_CONTAINER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RYGEL_MEDIA_EXPORT_TYPE_NULL_CONTAINER, RygelMediaExportNullContainerClass))

typedef struct _RygelMediaExportNullContainer RygelMediaExportNullContainer;
typedef struct _RygelMediaExportNullContainerClass RygelMediaExportNullContainerClass;

struct _RygelMediaExportNullContainer {
  RygelMediaContainer parent_instance;
};

struct _RygelMediaExportNullContainerClass {
  RygelMediaContainerClass parent_class;
};

GType
rygel_media_export_null_container_get_type (void) G_GNUC_CONST;

RygelMediaExportNullContainer *
rygel_media_export_null_container_new (void);

G_END_DECLS

#endif /* __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_NULL_CONTAINER_H__ */
