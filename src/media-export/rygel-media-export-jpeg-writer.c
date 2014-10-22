/*
 * Copyright (C) 2010 Jens Georg <mail@jensge.org>.
 * Copyright (C) 2013 Intel Corporation.
 *
 * Author: Jens Georg <mail@jensge.org>
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

#include <gst/app/gstappsrc.h>

#include "rygel-media-export-jpeg-writer.h"

/**
 * Utility class to write media-art content to JPEG files
 *
 * This uses a gstreamer pipeline to transcode the image tag as contained in
 * MP3 files. This class is single-shot, use and then throw away.
 */

G_DEFINE_TYPE (RygelMediaExportJPEGWriter, rygel_media_export_jpeg_writer, G_TYPE_OBJECT)

struct _RygelMediaExportJPEGWriterPrivate {
  GstBin *bin;
  GstAppSrc *appsrc;
  GMainLoop *loop;
  GstElement *sink;
};

#define RYGEL_MEDIA_EXPORT_JPEG_WRITER_GET_PRIVATE(o)                   \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o),                                    \
                                RYGEL_MEDIA_EXPORT_TYPE_JPEG_WRITER,    \
                                RygelMediaExportJPEGWriterPrivate))

enum  {
  RYGEL_MEDIA_EXPORT_JPEG_WRITER_DUMMY_PROPERTY,
  RYGEL_MEDIA_EXPORT_JPEG_WRITER_BIN
};

static void
quit_loop (GstBus     *sender G_GNUC_UNUSED,
           GstMessage *message G_GNUC_UNUSED,
           gpointer    user_data) {
  RygelMediaExportJPEGWriter *self = RYGEL_MEDIA_EXPORT_JPEG_WRITER (user_data);

  g_main_loop_quit (self->priv->loop);
}

RygelMediaExportJPEGWriter *
rygel_media_export_jpeg_writer_new (GError **error) {
  GError *inner_error = NULL;
  GstElement *element = gst_parse_launch ("appsrc name=src ! decodebin2 ! ffmpegcolorspace ! jpegenc ! giosink name=sink", &inner_error);

  if (inner_error) {
    g_propagate_error (error, inner_error);
    /* gst_parse_launch docs say that it can return some element, even
     * if error was thrown. Stupid. */
    if (element) {
      gst_object_unref (element);
    }

    return NULL;
  }

  return RYGEL_MEDIA_EXPORT_JPEG_WRITER (g_object_new (RYGEL_MEDIA_EXPORT_TYPE_JPEG_WRITER,
                                                       "bin", GST_BIN (element),
                                                       NULL));}

/**
 * Write a Gst.Buffer as retrieved from the Gst.TagList to disk.
 *
 * @param buffer The Gst.Buffer as obtained from tag list
 * @param file   A GLib.File pointing to the target location
 *
 * FIXME This uses a nested main-loop to block which is ugly.
 */
void
rygel_media_export_jpeg_writer_write (RygelMediaExportJPEGWriter *self,
                                      GstBuffer                  *buffer,
                                      GFile                      *file) {
  RygelMediaExportJPEGWriterPrivate *priv;
  GstElement* element;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_JPEG_WRITER (self));
  g_return_if_fail (GST_IS_BUFFER (buffer));
  g_return_if_fail (G_IS_FILE (file));

  priv = self->priv;
  element = GST_ELEMENT (priv->bin);
  g_object_set (priv->sink, "file", file, NULL);
  gst_app_src_push_buffer (priv->appsrc, gst_buffer_ref (buffer));
  gst_app_src_end_of_stream (priv->appsrc);
  gst_element_set_state (element, GST_STATE_PLAYING);
  g_main_loop_run (priv->loop);
  gst_element_set_state (element, GST_STATE_NULL);
}

static void
rygel_media_export_jpeg_writer_dispose (GObject *object) {
  RygelMediaExportJPEGWriter *self = RYGEL_MEDIA_EXPORT_JPEG_WRITER (object);
  RygelMediaExportJPEGWriterPrivate *priv = self->priv;

  if (priv->bin) {
    GstBin *bin = priv->bin;

    priv->bin = NULL;
    gst_object_unref (bin);
  }
  if (priv->appsrc) {
    GstAppSrc *appsrc = priv->appsrc;

    priv->appsrc = NULL;
    gst_object_unref (appsrc);
  }
  if (priv->loop) {
    GMainLoop *loop = priv->loop;

    priv->loop = NULL;
    g_main_loop_unref (loop);
  }
  if (priv->sink) {
    GstElement *sink = priv->sink;

    priv->sink = NULL;
    gst_object_unref (sink);
  }

  G_OBJECT_CLASS (rygel_media_export_jpeg_writer_parent_class)->dispose (object);
}

static void
rygel_media_export_jpeg_writer_constructed (GObject *object) {
  RygelMediaExportJPEGWriter *self = RYGEL_MEDIA_EXPORT_JPEG_WRITER (object);
  RygelMediaExportJPEGWriterPrivate *priv = self->priv;
  GstBus *bus;

  G_OBJECT_CLASS (rygel_media_export_jpeg_writer_parent_class)->constructed (object);

  priv->appsrc = GST_APP_SRC (gst_bin_get_by_name (priv->bin, "src"));
  priv->sink = gst_bin_get_by_name (priv->bin, "sink");
  bus = gst_element_get_bus (GST_ELEMENT (priv->bin));
  gst_bus_add_signal_watch (bus);
  g_signal_connect_object (bus, "message::eos", G_CALLBACK (quit_loop), self, 0);
  g_signal_connect_object (bus, "message::error", G_CALLBACK (quit_loop), self, 0);
  priv->loop = g_main_loop_new (NULL, FALSE);
  gst_object_unref (bus);
}

static void
rygel_media_export_jpeg_writer_set_property (GObject      *object,
                                             guint         property_id,
                                             const GValue *value,
                                             GParamSpec   *pspec) {
  RygelMediaExportJPEGWriter *self = RYGEL_MEDIA_EXPORT_JPEG_WRITER (object);

  switch (property_id) {
  case RYGEL_MEDIA_EXPORT_JPEG_WRITER_BIN:
    self->priv->bin = g_value_dup_object (value);
  break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  break;
  }
}

static void
rygel_media_export_jpeg_writer_class_init (RygelMediaExportJPEGWriterClass *writer_class) {
  GObjectClass *object_class = G_OBJECT_CLASS (writer_class);

  object_class->dispose = rygel_media_export_jpeg_writer_dispose;
  object_class->constructed = rygel_media_export_jpeg_writer_constructed;
  object_class->set_property = rygel_media_export_jpeg_writer_set_property;

  g_object_class_install_property (object_class,
                                   RYGEL_MEDIA_EXPORT_JPEG_WRITER_BIN,
                                   g_param_spec_object ("bin",
                                                        "bin",
                                                        "bin",
                                                        GST_TYPE_BIN,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (writer_class, sizeof (RygelMediaExportJPEGWriterPrivate));
}

static void
rygel_media_export_jpeg_writer_init (RygelMediaExportJPEGWriter *self) {
  self->priv = RYGEL_MEDIA_EXPORT_JPEG_WRITER_GET_PRIVATE (self);
}
