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

#ifndef __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_ROOT_CONTAINER_H__
#define __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_ROOT_CONTAINER_H__

#include <glib.h>
#include <glib-object.h>

#include "rygel-media-export-trackable-db-container.h"

G_BEGIN_DECLS

#define RYGEL_MEDIA_EXPORT_TYPE_ROOT_CONTAINER (rygel_media_export_root_container_get_type ())
#define RYGEL_MEDIA_EXPORT_ROOT_CONTAINER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RYGEL_MEDIA_EXPORT_TYPE_ROOT_CONTAINER, RygelMediaExportRootContainer))
#define RYGEL_MEDIA_EXPORT_ROOT_CONTAINER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RYGEL_MEDIA_EXPORT_TYPE_ROOT_CONTAINER, RygelMediaExportRootContainerClass))
#define RYGEL_MEDIA_EXPORT_IS_ROOT_CONTAINER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RYGEL_MEDIA_EXPORT_TYPE_ROOT_CONTAINER))
#define RYGEL_MEDIA_EXPORT_IS_ROOT_CONTAINER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RYGEL_MEDIA_EXPORT_TYPE_ROOT_CONTAINER))
#define RYGEL_MEDIA_EXPORT_ROOT_CONTAINER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RYGEL_MEDIA_EXPORT_TYPE_ROOT_CONTAINER, RygelMediaExportRootContainerClass))

#define RYGEL_MEDIA_EXPORT_ROOT_CONTAINER_FILESYSTEM_FOLDER_NAME "Files & Folders"
#define RYGEL_MEDIA_EXPORT_ROOT_CONTAINER_FILESYSTEM_FOLDER_ID "Filesystem"

typedef struct _RygelMediaExportRootContainer RygelMediaExportRootContainer;
typedef struct _RygelMediaExportRootContainerClass RygelMediaExportRootContainerClass;
typedef struct _RygelMediaExportRootContainerPrivate RygelMediaExportRootContainerPrivate;

struct _RygelMediaExportRootContainer {
  RygelMediaExportTrackableDbContainer parent_instance;
  RygelMediaExportRootContainerPrivate *priv;
};

struct _RygelMediaExportRootContainerClass {
  RygelMediaExportTrackableDbContainerClass parent_class;
};

GType
rygel_media_export_root_container_get_type (void) G_GNUC_CONST;

gboolean
rygel_media_export_root_container_ensure_exists (GError **error);

RygelMediaContainer *
rygel_media_export_root_container_get_instance (void);

RygelMediaContainer *
rygel_media_export_root_container_get_filesystem_container (RygelMediaExportRootContainer *self);

void
rygel_media_export_root_container_shutdown (RygelMediaExportRootContainer *self);

void
rygel_media_export_root_container_add_uri (RygelMediaExportRootContainer *self,
                                           const gchar                   *uri);

void
rygel_media_export_root_container_remove_uri (RygelMediaExportRootContainer *self,
                                              const gchar                   *uri);

gchar **
rygel_media_export_root_container_get_dynamic_uris (RygelMediaExportRootContainer *self,
                                                    int                           *result_length);

G_END_DECLS

#endif /* __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_ROOT_CONTAINER_H__ */
