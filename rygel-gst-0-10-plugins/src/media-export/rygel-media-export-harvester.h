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

#ifndef __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_HARVESTER_H__
#define __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_HARVESTER_H__

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gee.h>
#include <rygel-server.h>

G_BEGIN_DECLS

#define RYGEL_MEDIA_EXPORT_TYPE_HARVESTER (rygel_media_export_harvester_get_type ())
#define RYGEL_MEDIA_EXPORT_HARVESTER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RYGEL_MEDIA_EXPORT_TYPE_HARVESTER, RygelMediaExportHarvester))
#define RYGEL_MEDIA_EXPORT_HARVESTER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RYGEL_MEDIA_EXPORT_TYPE_HARVESTER, RygelMediaExportHarvesterClass))
#define RYGEL_MEDIA_EXPORT_IS_HARVESTER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RYGEL_MEDIA_EXPORT_TYPE_HARVESTER))
#define RYGEL_MEDIA_EXPORT_IS_HARVESTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RYGEL_MEDIA_EXPORT_TYPE_HARVESTER))
#define RYGEL_MEDIA_EXPORT_HARVESTER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RYGEL_MEDIA_EXPORT_TYPE_HARVESTER, RygelMediaExportHarvesterClass))

typedef struct _RygelMediaExportHarvester RygelMediaExportHarvester;
typedef struct _RygelMediaExportHarvesterClass RygelMediaExportHarvesterClass;
typedef struct _RygelMediaExportHarvesterPrivate RygelMediaExportHarvesterPrivate;

struct _RygelMediaExportHarvester {
  GObject parent_instance;
  RygelMediaExportHarvesterPrivate *priv;
};

struct _RygelMediaExportHarvesterClass {
  GObjectClass parent_class;
};

GType
rygel_media_export_harvester_get_type (void) G_GNUC_CONST;

RygelMediaExportHarvester *
rygel_media_export_harvester_new (GCancellable *cancellable,
                                  GeeArrayList *locations);

GeeArrayList *
rygel_media_export_harvester_get_locations (RygelMediaExportHarvester *self);

void
rygel_media_export_harvester_schedule (RygelMediaExportHarvester *self,
                                       GFile                     *file,
                                       RygelMediaContainer       *parent,
                                       const gchar               *flag);

void
rygel_media_export_harvester_cancel (RygelMediaExportHarvester *self,
                                     GFile                     *file);

gboolean
rygel_media_export_harvester_is_eligible (GFileInfo *info);

G_END_DECLS

#endif /* __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_HARVESTER_H__ */
