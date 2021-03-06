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

#ifndef __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_HARVESTING_TASK_H__
#define __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_HARVESTING_TASK_H__

#include <glib.h>
#include <glib-object.h>
#include <rygel-server.h>
#include <gio/gio.h>

#include "rygel-media-export-metadata-extractor.h"
#include "rygel-media-export-recursive-file-monitor.h"

G_BEGIN_DECLS

#define RYGEL_MEDIA_EXPORT_TYPE_HARVESTING_TASK (rygel_media_export_harvesting_task_get_type ())
#define RYGEL_MEDIA_EXPORT_HARVESTING_TASK(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RYGEL_MEDIA_EXPORT_TYPE_HARVESTING_TASK, RygelMediaExportHarvestingTask))
#define RYGEL_MEDIA_EXPORT_HARVESTING_TASK_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RYGEL_MEDIA_EXPORT_TYPE_HARVESTING_TASK, RygelMediaExportHarvestingTaskClass))
#define RYGEL_MEDIA_EXPORT_IS_HARVESTING_TASK(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RYGEL_MEDIA_EXPORT_TYPE_HARVESTING_TASK))
#define RYGEL_MEDIA_EXPORT_IS_HARVESTING_TASK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RYGEL_MEDIA_EXPORT_TYPE_HARVESTING_TASK))
#define RYGEL_MEDIA_EXPORT_HARVESTING_TASK_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RYGEL_MEDIA_EXPORT_TYPE_HARVESTING_TASK, RygelMediaExportHarvestingTaskClass))

typedef struct _RygelMediaExportHarvestingTask RygelMediaExportHarvestingTask;
typedef struct _RygelMediaExportHarvestingTaskClass RygelMediaExportHarvestingTaskClass;
typedef struct _RygelMediaExportHarvestingTaskPrivate RygelMediaExportHarvestingTaskPrivate;

struct _RygelMediaExportHarvestingTask {
  GObject parent_instance;
  RygelMediaExportHarvestingTaskPrivate *priv;
};

struct _RygelMediaExportHarvestingTaskClass {
  GObjectClass parent_class;
};

GType
rygel_media_export_harvesting_task_get_type (void) G_GNUC_CONST;

RygelMediaExportHarvestingTask *
rygel_media_export_harvesting_task_new (RygelMediaExportMetadataExtractor    *extractor,
                                        RygelMediaExportRecursiveFileMonitor *monitor,
                                        GFile                                *file,
                                        RygelMediaContainer                  *parent,
                                        const gchar                          *flag);

void
rygel_media_export_harvesting_task_cancel (RygelMediaExportHarvestingTask *self);

GFile *
rygel_media_export_harvesting_task_get_origin (RygelMediaExportHarvestingTask *self);

G_END_DECLS

#endif /* __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_HARVESTING_TASK_H__ */
