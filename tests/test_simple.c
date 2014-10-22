/*
 * Copyright (C) 2012 Intel Corporation
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

#include <gee.h>
#include <gst/gst.h>

#include "rygel-media-export-plugin.h"
#include "rygel-media-export-root-container.h"
#include "rygel-media-export-null-container.h"

int main(int argc, char *argv[])
{
  RygelPluginLoader *loader;
  GeeCollection *list_plugins;
  GeeIterator *iter;
  RygelMediaServerPlugin *plugin;
  RygelMediaContainer *root_container;

#if !GLIB_CHECK_VERSION(2,35,0)
  g_type_init ();
#endif
  gst_init (&argc, &argv);

  /* Some very simple checks that the plugin can be instantiated
   * and used in very simple ways.
   */

  /* Call the plugin's module_init() to make it load itself: */
  loader = rygel_plugin_loader_new ();
  g_assert (loader);

  module_init (loader);

  /* Get the loaded plugin: */
  list_plugins = rygel_plugin_loader_list_plugins (loader);
  g_assert (list_plugins);
  g_assert (!gee_collection_get_is_empty (list_plugins));
  g_assert (gee_collection_get_size (list_plugins) == 1);

  /* Check the plugin: */
  iter = gee_iterable_iterator (GEE_ITERABLE (list_plugins));
  g_assert (gee_iterator_next (iter));
  plugin = gee_iterator_get (iter);
  g_assert (plugin);
  g_assert (RYGEL_MEDIA_EXPORT_IS_PLUGIN (plugin));

  /* Check the plugin's root container: */
  root_container = rygel_media_server_plugin_get_root_container (plugin);
  g_assert (root_container);
  g_assert (RYGEL_MEDIA_EXPORT_IS_ROOT_CONTAINER (root_container));

  /* unref: */
  g_object_unref (plugin);
  g_object_unref (iter);
  g_object_unref (list_plugins);
  g_object_unref (loader);

  /* Root container is a singleton so unref it here. */
  g_object_unref (root_container);

  return 0;
}
