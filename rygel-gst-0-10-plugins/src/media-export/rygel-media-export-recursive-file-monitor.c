/*
 * Copyright (C) 2009 Jens Georg <mail@jensge.org>.
 * Copyright (C) 2013 Intel Corporation.
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


#include "rygel-media-export-recursive-file-monitor.h"
#include "rygel-media-export-plugin.h"
#include <glib/gi18n-lib.h>

G_DEFINE_TYPE (RygelMediaExportRecursiveFileMonitor, rygel_media_export_recursive_file_monitor, G_TYPE_OBJECT)

typedef struct _RygelMediaExportRecursiveFileMonitorAddData RygelMediaExportRecursiveFileMonitorAddData;


struct _RygelMediaExportRecursiveFileMonitorPrivate {
  GCancellable *cancellable;
  GeeHashMap *monitors;
  gboolean monitor_changes;
};

struct _RygelMediaExportRecursiveFileMonitorAddData {
  RygelMediaExportRecursiveFileMonitor *self;
  GSimpleAsyncResult *async_result;
};


#define RYGEL_MEDIA_EXPORT_RECURSIVE_FILE_MONITOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), RYGEL_MEDIA_EXPORT_TYPE_RECURSIVE_FILE_MONITOR, RygelMediaExportRecursiveFileMonitorPrivate))

static void rygel_media_export_recursive_file_monitor_on_monitor_changed (GFileMonitor *file_monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, gpointer user_data);
static void rygel_media_export_recursive_file_monitor_add_data_complete_and_free (RygelMediaExportRecursiveFileMonitorAddData *data);
static void rygel_media_export_recursive_file_monitor_add_on_query_info (GObject *source_object, GAsyncResult *async_result, gpointer user_data);
static void g_cclosure_user_marshal_VOID__OBJECT_OBJECT_ENUM (GClosure  *closure, GValue  *return_value, guint n_param_values, const GValue  *param_values, gpointer invocation_hint, gpointer marshal_data);
static void rygel_media_export_recursive_file_monitor_finalize (GObject *obj);
static void rygel_media_export_recursive_file_monitor_on_cancelled (GCancellable *cancellable G_GNUC_UNUSED, gpointer user_data);


static void
rygel_media_export_recursive_file_monitor_on_config_changed (RygelConfiguration *config, const gchar *section, const gchar *key, gpointer user_data) {
  RygelMediaExportRecursiveFileMonitor *self = RYGEL_MEDIA_EXPORT_RECURSIVE_FILE_MONITOR (user_data);
  gboolean setting;
  GError *inner_error = NULL;

  g_return_if_fail (self);
  g_return_if_fail (config);
  g_return_if_fail (section);
  g_return_if_fail (key);

  if ((g_strcmp0 (section, RYGEL_MEDIA_EXPORT_PLUGIN_NAME) != 0) ||
    (g_strcmp0 (key, "monitor-changes") != 0)) {
    return;
  }

  setting = rygel_configuration_get_bool (config,
                                          RYGEL_MEDIA_EXPORT_PLUGIN_NAME,
                                          "monitor-changes",
                                          &inner_error);
  if (inner_error) {
    setting = TRUE;
    g_error_free (inner_error);
  }
  self->priv->monitor_changes = setting;
}


static RygelMediaExportRecursiveFileMonitor*
rygel_media_export_recursive_file_monitor_construct (GType object_type, GCancellable *cancellable) {
  RygelMediaExportRecursiveFileMonitor *self;
  RygelMetaConfig *config;
  GError *inner_error;
  gboolean setting;

  self = RYGEL_MEDIA_EXPORT_RECURSIVE_FILE_MONITOR (g_object_new (object_type, NULL));

  config = rygel_meta_config_get_default ();

  g_signal_connect_object (RYGEL_CONFIGURATION (config), "setting-changed",
    (GCallback)rygel_media_export_recursive_file_monitor_on_config_changed,
    self, 0);
  rygel_media_export_recursive_file_monitor_on_config_changed (RYGEL_CONFIGURATION (config),
    RYGEL_MEDIA_EXPORT_PLUGIN_NAME, "monitor-changes", self);

  inner_error = NULL;
  setting = rygel_configuration_get_bool (RYGEL_CONFIGURATION (config),
                                          "MediaExport",
                                          "monitor-changes",
                                          &inner_error);

  if (inner_error) {
    setting = TRUE;
    g_error_free (inner_error);
  }

  if (!setting) {
    g_message (_("Will not monitor file changes"));
  }

  self->priv->monitor_changes = setting;

  self->priv->monitors = gee_hash_map_new (G_TYPE_FILE,
    g_object_ref, g_object_unref,
    g_file_monitor_get_type (),
    g_object_ref, g_object_unref,
    (GeeHashDataFunc) g_file_hash,
    NULL, NULL,
    (GeeEqualDataFunc) g_file_equal,
    NULL, NULL, NULL, NULL, NULL);

  if (cancellable) {
    self->priv->cancellable = cancellable;
    g_object_ref (self->priv->cancellable);

    g_signal_connect_object (self->priv->cancellable, "cancelled",
      (GCallback) rygel_media_export_recursive_file_monitor_on_cancelled,
      self, 0);
  }

  return self;
}


RygelMediaExportRecursiveFileMonitor*
rygel_media_export_recursive_file_monitor_new (GCancellable *cancellable) {
  return rygel_media_export_recursive_file_monitor_construct (RYGEL_MEDIA_EXPORT_TYPE_RECURSIVE_FILE_MONITOR, cancellable);
}


static void
rygel_media_export_recursive_file_monitor_on_monitor_changed (GFileMonitor *file_monitor G_GNUC_UNUSED, GFile *file, GFile *other_file, GFileMonitorEvent event_type, gpointer user_data) {
  RygelMediaExportRecursiveFileMonitor *self = RYGEL_MEDIA_EXPORT_RECURSIVE_FILE_MONITOR (user_data);

  g_return_if_fail (self);
  g_return_if_fail (file);

  if (self->priv->monitor_changes) {
    g_signal_emit_by_name (self, "changed", file, other_file, event_type);
  }

  switch (event_type) {
    case G_FILE_MONITOR_EVENT_CREATED:
    {
      rygel_media_export_recursive_file_monitor_add (self, file, NULL, NULL);
      break;
    }
    case G_FILE_MONITOR_EVENT_DELETED:
    {
      GFileMonitor *file_monitor = G_FILE_MONITOR (gee_abstract_map_get ((GeeAbstractMap*) self->priv->monitors, file));
      if (file_monitor) {
        gchar *uri = g_file_get_uri (file);
        g_debug ("Folder %s gone. Removing watch", uri);
        g_free (uri);

        gee_abstract_map_unset ((GeeAbstractMap*) self->priv->monitors, file, NULL);
        g_file_monitor_cancel (file_monitor);

        guint signal_id = 0U;
        g_signal_parse_name ("changed", g_file_monitor_get_type (), &signal_id, NULL, FALSE);
        g_signal_handlers_disconnect_matched (file_monitor,
          G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
          signal_id, 0, NULL,
          (GCallback) rygel_media_export_recursive_file_monitor_on_monitor_changed, self);

       g_object_unref (file_monitor);
      }

      break;
    }
    default:
    {
      break;
    }
  }
}

static void
rygel_media_export_recursive_file_monitor_add_data_complete_and_free (RygelMediaExportRecursiveFileMonitorAddData *data) {
  g_simple_async_result_complete (data->async_result);
  g_object_unref (data->async_result);

  g_object_unref (data->self);
  g_slice_free (RygelMediaExportRecursiveFileMonitorAddData, data);
}


void rygel_media_export_recursive_file_monitor_add (RygelMediaExportRecursiveFileMonitor *self, GFile *file, GAsyncReadyCallback callback, gpointer user_data) {
  RygelMediaExportRecursiveFileMonitorAddData *data;

  g_return_if_fail (self);
  g_return_if_fail (file);

  /* Setup the async result.
   */
  GSimpleAsyncResult *async_result =
    g_simple_async_result_new (G_OBJECT (self), callback, user_data,
      rygel_media_export_recursive_file_monitor_add);

  /* Do the work that could take a while.
   */
  if (gee_abstract_map_has_key ((GeeAbstractMap*) self->priv->monitors, file)) {
    /* Let the caller know that the async operation is finished,
     * and that its result is now available.
     */
    g_simple_async_result_complete (async_result);
    g_object_unref (async_result);
    return;
  }

  /* Call another async function, whose callback will complete our async result,
   * passing a data instance to hold our self and async result.
   */
  data = g_slice_new0 (RygelMediaExportRecursiveFileMonitorAddData);
  data->self = self;
  g_object_ref (data->self);
  data->async_result = async_result;
  g_file_query_info_async (file, G_FILE_ATTRIBUTE_STANDARD_TYPE,
      G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT, NULL, rygel_media_export_recursive_file_monitor_add_on_query_info, data);
}

void
rygel_media_export_recursive_file_monitor_add_finish (RygelMediaExportRecursiveFileMonitor *self G_GNUC_UNUSED, GAsyncResult *res G_GNUC_UNUSED) {
}


static void
rygel_media_export_recursive_file_monitor_add_on_query_info (GObject *source_object, GAsyncResult *async_result, gpointer user_data) {
  RygelMediaExportRecursiveFileMonitorAddData *data = (RygelMediaExportRecursiveFileMonitorAddData*)user_data;
  GFile *file = G_FILE (source_object);
  GFileInfo *info;

  g_return_if_fail (data);
  g_return_if_fail (async_result);
  g_return_if_fail (file);

  /* async_result is the result of our query.
   * data->async_result is the result, in progress, of our own operation.
   */

  GError *error = NULL;
  info = g_file_query_info_finish (file, async_result, &error);
  g_return_if_fail (info);
  if (error) {
    gchar *uri = g_file_get_uri (file);
    g_warning ( _("Failed to get file info for %s"), uri);
    g_free (uri);

    /* Set the error in the async result.
     */
    g_simple_async_result_set_from_error (data->async_result, error);
    g_error_free (error);
  }

  if (!info || error) {
    /* Let the caller know that the async operation is finished,
     * and that its result is now available.
     */
    rygel_media_export_recursive_file_monitor_add_data_complete_and_free (data);
    return;
  }

  if (g_file_info_get_file_type (info) !=  G_FILE_TYPE_DIRECTORY) {
    /* Let the caller know that the async operation is finished,
     * and that its result is now available.
     */
    rygel_media_export_recursive_file_monitor_add_data_complete_and_free (data);
    return;
  }

  error = NULL;
  GFileMonitor *file_monitor = g_file_monitor_directory (file, G_FILE_MONITOR_NONE, data->self->priv->cancellable, &error);
  if (error) {
    gchar *uri = g_file_get_uri (file);
    g_warning ( _ ("Failed to get file info for %s"), uri);
    g_free (uri);

    /* Set the error in the async result.
     */
    g_simple_async_result_set_from_error (data->async_result, error);
    g_error_free (error);
  }

  if (file_monitor) {
    gee_abstract_map_set ((GeeAbstractMap*) data->self->priv->monitors, file, file_monitor);
    g_signal_connect_object (file_monitor, "changed", (GCallback) rygel_media_export_recursive_file_monitor_on_monitor_changed, data->self, 0);
  }

  /* Let the caller know that the async operation is finished,
   * and that its result is now available.
   */
  rygel_media_export_recursive_file_monitor_add_data_complete_and_free (data);
}



static void
rygel_media_export_recursive_file_monitor_on_cancelled (GCancellable *cancellable G_GNUC_UNUSED, gpointer user_data) {
  RygelMediaExportRecursiveFileMonitor *self = RYGEL_MEDIA_EXPORT_RECURSIVE_FILE_MONITOR (user_data);
  GeeCollection * values;
  GeeIterator *iter;
  g_return_if_fail (self);

  values = gee_abstract_map_get_values ((GeeAbstractMap*) self->priv->monitors);
  iter = gee_iterable_iterator ((GeeIterable*) values);
  g_object_unref (values);

  while (TRUE) {
    if (!gee_iterator_next (iter))
      break;

    GFileMonitor *monitor = G_FILE_MONITOR (gee_iterator_get (iter));
    if (!monitor)
      continue;

    g_file_monitor_cancel (monitor);
    g_object_unref (monitor);
  }

  g_object_unref (iter);

  gee_abstract_map_clear ((GeeAbstractMap*) self->priv->monitors);
}

static void
g_cclosure_user_marshal_VOID__OBJECT_OBJECT_ENUM (GClosure  *closure, GValue *return_value G_GNUC_UNUSED, guint n_param_values, const GValue *param_values, gpointer invocation_hint G_GNUC_UNUSED, gpointer marshal_data) {
  typedef void (*GMarshalFunc_VOID__OBJECT_OBJECT_ENUM) (gpointer data1, gpointer arg_1, gpointer arg_2, gint arg_3, gpointer data2);
  register GMarshalFunc_VOID__OBJECT_OBJECT_ENUM callback;
  register GCClosure  *cc;
  register gpointer data1;
  register gpointer data2;

  cc = (GCClosure *) closure;
  g_return_if_fail (n_param_values == 4);

  if (G_CCLOSURE_SWAP_DATA (closure)) {
    data1 = closure->data;
    data2 = param_values->data[0].v_pointer;
  } else {
    data1 = param_values->data[0].v_pointer;
    data2 = closure->data;
  }

  callback = (GMarshalFunc_VOID__OBJECT_OBJECT_ENUM) (marshal_data ? marshal_data : cc->callback);
  callback (data1, g_value_get_object (param_values + 1), g_value_get_object (param_values + 2), g_value_get_enum (param_values + 3), data2);
}

static void
rygel_media_export_recursive_file_monitor_class_init (RygelMediaExportRecursiveFileMonitorClass  *klass) {
  g_type_class_add_private (klass, sizeof (RygelMediaExportRecursiveFileMonitorPrivate));

  G_OBJECT_CLASS (klass)->finalize = rygel_media_export_recursive_file_monitor_finalize;

  g_signal_new ("changed", RYGEL_MEDIA_EXPORT_TYPE_RECURSIVE_FILE_MONITOR,
    G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_user_marshal_VOID__OBJECT_OBJECT_ENUM,
    G_TYPE_NONE, 3, G_TYPE_FILE, G_TYPE_FILE, g_file_monitor_event_get_type ());
}

static void
rygel_media_export_recursive_file_monitor_init (RygelMediaExportRecursiveFileMonitor  *self) {
  self->priv = RYGEL_MEDIA_EXPORT_RECURSIVE_FILE_MONITOR_GET_PRIVATE (self);
}

static void
rygel_media_export_recursive_file_monitor_finalize (GObject *obj) {
  RygelMediaExportRecursiveFileMonitor  *self = RYGEL_MEDIA_EXPORT_RECURSIVE_FILE_MONITOR (obj);

  g_return_if_fail (self);

  if (self->priv->cancellable)
    g_object_unref (self->priv->cancellable);

  /* TODO: Empty this properly? */
  if (self->priv->monitors)
    g_object_unref (self->priv->monitors);

  G_OBJECT_CLASS (rygel_media_export_recursive_file_monitor_parent_class)->finalize (obj);
}
