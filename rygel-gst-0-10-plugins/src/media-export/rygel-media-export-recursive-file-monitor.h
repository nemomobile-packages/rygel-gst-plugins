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

#ifndef __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_RECURSIVE_FILE_MONITOR_H__
#define __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_RECURSIVE_FILE_MONITOR_H__

#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <string.h>
#include <rygel-core.h>
#include <rygel-server.h>
#include <gee.h>

G_BEGIN_DECLS

#define RYGEL_MEDIA_EXPORT_TYPE_RECURSIVE_FILE_MONITOR (rygel_media_export_recursive_file_monitor_get_type ())
#define RYGEL_MEDIA_EXPORT_RECURSIVE_FILE_MONITOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RYGEL_MEDIA_EXPORT_TYPE_RECURSIVE_FILE_MONITOR, RygelMediaExportRecursiveFileMonitor))
#define RYGEL_MEDIA_EXPORT_RECURSIVE_FILE_MONITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RYGEL_MEDIA_EXPORT_TYPE_RECURSIVE_FILE_MONITOR, RygelMediaExportRecursiveFileMonitorClass))
#define RYGEL_MEDIA_EXPORT_IS_RECURSIVE_FILE_MONITOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RYGEL_MEDIA_EXPORT_TYPE_RECURSIVE_FILE_MONITOR))
#define RYGEL_MEDIA_EXPORT_IS_RECURSIVE_FILE_MONITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RYGEL_MEDIA_EXPORT_TYPE_RECURSIVE_FILE_MONITOR))
#define RYGEL_MEDIA_EXPORT_RECURSIVE_FILE_MONITOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RYGEL_MEDIA_EXPORT_TYPE_RECURSIVE_FILE_MONITOR, RygelMediaExportRecursiveFileMonitorClass))

typedef struct _RygelMediaExportRecursiveFileMonitor RygelMediaExportRecursiveFileMonitor;
typedef struct _RygelMediaExportRecursiveFileMonitorClass RygelMediaExportRecursiveFileMonitorClass;
typedef struct _RygelMediaExportRecursiveFileMonitorPrivate RygelMediaExportRecursiveFileMonitorPrivate;

struct _RygelMediaExportRecursiveFileMonitor {
  GObject parent_instance;
  RygelMediaExportRecursiveFileMonitorPrivate *priv;
};

struct _RygelMediaExportRecursiveFileMonitorClass {
  GObjectClass parent_class;
};

GType
rygel_media_export_recursive_file_monitor_get_type (void) G_GNUC_CONST;

RygelMediaExportRecursiveFileMonitor *
rygel_media_export_recursive_file_monitor_new (GCancellable *cancellable);

void
rygel_media_export_recursive_file_monitor_add (RygelMediaExportRecursiveFileMonitor *self,
                                               GFile                                *file,
                                               GAsyncReadyCallback                   callback,
                                               gpointer                              user_data);

void
rygel_media_export_recursive_file_monitor_add_finish (RygelMediaExportRecursiveFileMonitor *self,
                                                      GAsyncResult                         *res);

G_END_DECLS

#endif /* __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_RECURSIVE_FILE_MONITOR_H__ */
