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

#ifndef __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_ITEM_FACTORY_H__
#define __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_ITEM_FACTORY_H__

#include <glib.h>
#include <gio/gio.h>
#include <rygel-server.h>
#include <gst/pbutils/pbutils.h>
#include <libgupnp-dlna/gupnp-dlna.h>

#include "rygel-media-export-music-item.h"
#include "rygel-media-export-photo-item.h"
#include "rygel-media-export-video-item.h"

G_BEGIN_DECLS

RygelMediaItem *
rygel_media_export_item_factory_create_simple (RygelMediaContainer *parent,
                                               GFile               *file,
                                               GFileInfo           *info);

gchar *
rygel_media_export_media_cache_get_id (GFile *file);

RygelMediaItem *
rygel_media_export_item_factory_create_playlist_item (GFile *file,
                                                      RygelMediaContainer *parent,
                                                      const gchar *fallback_title);

RygelMediaItem *
rygel_media_export_item_factory_create_from_info (RygelMediaContainer *parent,
                                                  GFile               *file,
                                                  GstDiscovererInfo   *info,
                                                  GUPnPDLNAProfile    *profile,
                                                  GFileInfo           *file_info);

G_END_DECLS

#endif /* __RYGEL_0_10_PLUGINS_MEDIA_EXPORT_ITEM_FACTORY_H__ */
