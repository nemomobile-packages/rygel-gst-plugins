/*
 * Copyright (C) 2008, 2009 Jens Georg <mail@jensge.org>.
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

#include <gst/gst.h>

#include "rygel-media-export-plugin.h"
#include "rygel-media-export-root-container.h"

G_DEFINE_TYPE (RygelMediaExportPlugin, rygel_media_export_plugin, RYGEL_TYPE_MEDIA_SERVER_PLUGIN)

/**
 * Simple plugin which exposes the media contents of a directory via UPnP.
 *
 */

void module_init (RygelPluginLoader *loader) {
  RygelMediaExportPlugin *plugin;
  GError *error;

  if (rygel_plugin_loader_plugin_disabled (loader, RYGEL_MEDIA_EXPORT_PLUGIN_NAME)) {
    g_message ("rygel-media-export-plugin.c: Plugin '%s' disabled by user. Ignoring.", RYGEL_MEDIA_EXPORT_PLUGIN_NAME);
    return;
  }

  gst_init (NULL, NULL);

  error = NULL;
  /* Instantiate the plugin object */
  plugin = rygel_media_export_plugin_new (&error);

  if (error) {
    g_warning ("Failed to initialize plugin '" RYGEL_MEDIA_EXPORT_PLUGIN_NAME "': %s. Ignoring...",
               error->message);
    g_error_free (error);
    return;
  }

  rygel_plugin_loader_add_plugin (loader, RYGEL_PLUGIN (plugin));
  g_object_unref (plugin);
}

RygelMediaExportPlugin*
rygel_media_export_plugin_new (GError **error) {
  RygelMediaContainer *root;
  GError *inner_error = NULL;
  RygelMediaExportPlugin* plugin;

  rygel_media_export_root_container_ensure_exists (&inner_error);
  if (inner_error) {
    g_propagate_error (error, inner_error);
    return  NULL;
  }
  root = rygel_media_export_root_container_get_instance ();
  plugin = RYGEL_MEDIA_EXPORT_PLUGIN (rygel_media_server_plugin_construct (RYGEL_MEDIA_EXPORT_TYPE_PLUGIN,
                                                                           root,
                                                                           RYGEL_MEDIA_EXPORT_PLUGIN_NAME,
                                                                           NULL,
                                                                           RYGEL_PLUGIN_CAPABILITIES_UPLOAD |
                                                                           RYGEL_PLUGIN_CAPABILITIES_TRACK_CHANGES));
  g_object_unref (root);
  return plugin;
}

static void
rygel_media_export_plugin_class_init (RygelMediaExportPluginClass *klass) {
  rygel_media_export_plugin_parent_class = g_type_class_peek_parent (klass);
}

static void rygel_media_export_plugin_init (RygelMediaExportPlugin *self G_GNUC_UNUSED) {
}
