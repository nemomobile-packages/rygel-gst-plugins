/*
 * Copyright (C) 2008 Zeeshan Ali (Khattak) <zeeshanak@gnome.org>.
 * Copyright (C) 2009 Jens Georg <mail@jensge.org>.
 * Copyright (C) 2013 Intel Corporation.
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
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

#include "rygel-media-export-harvesting-task.h"
#include <libgupnp-dlna/gupnp-dlna.h>
#include <libgupnp-dlna/gupnp-dlna-gst-legacy-utils.h>
#include <gst/pbutils/pbutils.h>
#include <glib/gi18n-lib.h>
#include "rygel-media-export-plugin.h"

G_DEFINE_TYPE (RygelMediaExportMetadataExtractor, rygel_media_export_metadata_extractor, G_TYPE_OBJECT)

struct _RygelMediaExportMetadataExtractorPrivate {
  GstDiscoverer *discoverer;
  GeeHashMap *file_hash;
  gboolean extract_metadata;
  GUPnPDLNAProfileGuesser *guesser;
};

enum {
  EXTRACTION_DONE,
  ERROR,

  SIGNALS_COUNT
};

static guint signals [SIGNALS_COUNT];

#define RYGEL_MEDIA_EXPORT_METADATA_EXTRACTOR_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                RYGEL_MEDIA_EXPORT_TYPE_METADATA_EXTRACTOR, \
                                RygelMediaExportMetadataExtractorPrivate))

#define EXTRACTOR_OPTION "extract-metadata"
#define EXTRACTOR_TIMEOUT G_GUINT64_CONSTANT(10)

static void
on_config_changed (RygelConfiguration *config,
                   const gchar *section,
                   const gchar *key,
                   gpointer user_data) {
  RygelMediaExportMetadataExtractor *self;
  GError *error;
  gboolean option;

  if (g_strcmp0 (section, RYGEL_MEDIA_EXPORT_PLUGIN_NAME) || g_strcmp0 (key, EXTRACTOR_OPTION)) {
    return;
  }

  self = RYGEL_MEDIA_EXPORT_METADATA_EXTRACTOR (user_data);
  error = NULL;
  option = rygel_configuration_get_bool (config, RYGEL_MEDIA_EXPORT_PLUGIN_NAME, EXTRACTOR_OPTION, &error);

  if (error) {
    option = TRUE;
    g_error_free (error);
  }
  self->priv->extract_metadata = option;
}

RygelMediaExportMetadataExtractor *
rygel_media_export_metadata_extractor_new (void) {
  return RYGEL_MEDIA_EXPORT_METADATA_EXTRACTOR (g_object_new (RYGEL_MEDIA_EXPORT_TYPE_METADATA_EXTRACTOR, NULL));
}

/* Needed because of cycle: dlna_discoverer_done calls on_done, which
 * calls dlna_discoverer_done.
 */
static void
rygel_media_export_metadata_extractor_on_done_gst_discoverer_discovered (GstDiscoverer     *sender G_GNUC_UNUSED,
                                                                         GstDiscovererInfo *dlna,
                                                                         GError            *err,
                                                                         gpointer           self);

static void
rygel_media_export_metadata_extractor_extract_basic_information (RygelMediaExportMetadataExtractor *self,
                                                                 GFile                             *file,
                                                                 GstDiscovererInfo                 *info,
                                                                 GUPnPDLNAProfile                  *profile) {
  GError *error;
  GFileInfo *file_info;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_METADATA_EXTRACTOR (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (info == NULL || GST_IS_DISCOVERER_INFO (info));
  g_return_if_fail (profile == NULL || GUPNP_IS_DLNA_PROFILE (profile));

  error = NULL;
  file_info = g_file_query_info (file,
                                 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
                                 G_FILE_ATTRIBUTE_STANDARD_SIZE ","
                                 G_FILE_ATTRIBUTE_TIME_MODIFIED ","
                                 G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                 G_FILE_QUERY_INFO_NONE,
                                 NULL,
                                 &error);
  if (error) {
    gchar *uri = g_file_get_uri (file);

    g_warning (_("Failed to query content type for '%s'"), uri);
    g_debug ("Failed to extract basic metadata from %s: %s", uri, error->message);
    g_signal_emit (self, signals[ERROR], 0, file, error);
    g_error_free (error);
    return;
  }
  g_signal_emit (self, signals[EXTRACTION_DONE], 0, file, info, profile, file_info);
  g_object_unref (file_info);
}

static void
rygel_media_export_metadata_extractor_on_done (RygelMediaExportMetadataExtractor *self,
                                               GstDiscovererInfo                 *gst_info,
                                               GError                            *err) {
  RygelMediaExportMetadataExtractorPrivate *priv;
  guint signal_id;
  const gchar *uri;
  GeeAbstractMap *abstract_file_hash;
  GstDiscovererResult result;
  GFile *file;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_METADATA_EXTRACTOR (self));
  g_return_if_fail (GST_IS_DISCOVERER_INFO (gst_info));

  priv = self->priv;
  signal_id = 0;
  g_signal_parse_name ("discovered", GST_TYPE_DISCOVERER, &signal_id, NULL, FALSE);
  g_signal_handlers_disconnect_matched (priv->discoverer,
                                        G_SIGNAL_MATCH_ID |
                                        G_SIGNAL_MATCH_FUNC |
                                        G_SIGNAL_MATCH_DATA, signal_id,
                                        0,
                                        NULL,
                                        G_CALLBACK (rygel_media_export_metadata_extractor_on_done_gst_discoverer_discovered),
                                        self);

  g_object_unref (priv->discoverer);
  priv->discoverer = NULL;

  uri = gst_discoverer_info_get_uri (gst_info);
  abstract_file_hash = GEE_ABSTRACT_MAP (priv->file_hash);
  file = G_FILE (gee_abstract_map_get (abstract_file_hash, uri));
  if (!file) {
    g_warning ("File %s already handled, ignoring event", uri);
    return;
  }
  gee_abstract_map_unset (abstract_file_hash, uri, NULL);
  result = gst_discoverer_info_get_result (gst_info);

  if ((result & GST_DISCOVERER_ERROR) == GST_DISCOVERER_ERROR) {
    g_signal_emit (self, signals[ERROR], 0, file, err);
  } else {
    GUPnPDLNAProfile *dlna_profile;

    if ((result & GST_DISCOVERER_TIMEOUT) == GST_DISCOVERER_TIMEOUT) {
      gchar* file_uri = g_file_get_uri (file);

      g_debug ("Extraction timed out on %s", file_uri);
      g_free (file_uri);
      dlna_profile = NULL;
    } else {
      GUPnPDLNAInformation *dlna_info = gupnp_dlna_gst_utils_information_from_discoverer_info (gst_info);

      dlna_profile = gupnp_dlna_profile_guesser_guess_profile_from_info (priv->guesser,
                                                                         dlna_info);
      g_object_unref (dlna_info);
    }

    rygel_media_export_metadata_extractor_extract_basic_information (self,
                                                                     file,
                                                                     gst_info,
                                                                     dlna_profile);
  }

  g_object_unref (file);
}

static void
rygel_media_export_metadata_extractor_on_done_gst_discoverer_discovered (GstDiscoverer     *sender G_GNUC_UNUSED,
                                                                         GstDiscovererInfo *info,
                                                                         GError            *err,
                                                                         gpointer           self) {
  rygel_media_export_metadata_extractor_on_done (self, info, err);
}

void
rygel_media_export_metadata_extractor_extract (RygelMediaExportMetadataExtractor *self,
                                               GFile                             *file) {
  RygelMediaExportMetadataExtractorPrivate *priv;

  g_return_if_fail (RYGEL_MEDIA_EXPORT_IS_METADATA_EXTRACTOR (self));
  g_return_if_fail (G_IS_FILE (file));

  priv = self->priv;

  if (priv->extract_metadata) {
    gchar *uri;
    GstClockTime gst_timeout;
    GstDiscoverer *discoverer;
    GError *error = NULL;
    GUPnPDLNAProfileGuesser *guesser;

    gst_timeout = (GstClockTime) (EXTRACTOR_TIMEOUT * GST_SECOND);
    discoverer = gst_discoverer_new (gst_timeout, &error);
    if (error) {
      g_error_free (error);
      g_debug ("Failed to create a discoverer. Doing basic extraction.");
      rygel_media_export_metadata_extractor_extract_basic_information (self,
                                                                       file,
                                                                       NULL,
                                                                       NULL);
      return;
    }
    if (priv->discoverer) {
      g_object_unref (priv->discoverer);
    }
    priv->discoverer = discoverer;
    uri = g_file_get_uri (file);
    gee_abstract_map_set (GEE_ABSTRACT_MAP (priv->file_hash), uri, file);
    g_signal_connect_object (discoverer,
                             "discovered",
                             G_CALLBACK (rygel_media_export_metadata_extractor_on_done_gst_discoverer_discovered),
                             self,
                             0);
    gst_discoverer_start (discoverer);
    gst_discoverer_discover_uri_async (discoverer, uri);
    g_free (uri);
    guesser = gupnp_dlna_profile_guesser_new (TRUE, TRUE);
    if (priv->guesser) {
      g_object_unref (priv->guesser);
    }
    priv->guesser = guesser;
  } else {
    rygel_media_export_metadata_extractor_extract_basic_information (self, file, NULL, NULL);
  }
}

static void
rygel_media_export_metadata_extractor_dispose (GObject *object) {
  RygelMediaExportMetadataExtractor *self = RYGEL_MEDIA_EXPORT_METADATA_EXTRACTOR (object);
  RygelMediaExportMetadataExtractorPrivate *priv = self->priv;

  if (priv->discoverer) {
    GstDiscoverer *discoverer = priv->discoverer;

    priv->discoverer = NULL;
    g_object_unref (discoverer);
  }
  if (priv->file_hash) {
    GeeHashMap *file_hash = priv->file_hash;

    priv->file_hash = NULL;
    g_object_unref (file_hash);
  }

  G_OBJECT_CLASS (rygel_media_export_metadata_extractor_parent_class)->dispose (object);
}

static void
rygel_media_export_metadata_extractor_constructed (GObject *object) {
  RygelMediaExportMetadataExtractor *self = RYGEL_MEDIA_EXPORT_METADATA_EXTRACTOR (object);
  RygelMediaExportMetadataExtractorPrivate *priv = self->priv;
  RygelConfiguration *config;

  G_OBJECT_CLASS (rygel_media_export_metadata_extractor_parent_class)->constructed (object);

  priv->file_hash = gee_hash_map_new (G_TYPE_STRING,
                                      (GBoxedCopyFunc) g_strdup,
                                      g_free,
                                      G_TYPE_FILE,
                                      g_object_ref,
                                      g_object_unref,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL);
  config = RYGEL_CONFIGURATION (rygel_meta_config_get_default ());
  g_signal_connect (config, "setting-changed", G_CALLBACK (on_config_changed), self);
  on_config_changed (config, RYGEL_MEDIA_EXPORT_PLUGIN_NAME, EXTRACTOR_OPTION, self);
  g_object_unref (config);
}

static void
rygel_media_export_metadata_extractor_class_init (RygelMediaExportMetadataExtractorClass *extractor_class) {
  GObjectClass *object_class = G_OBJECT_CLASS (extractor_class);

  object_class->dispose = rygel_media_export_metadata_extractor_dispose;
  object_class->constructed = rygel_media_export_metadata_extractor_constructed;
  signals[EXTRACTION_DONE] = g_signal_new ("extraction_done",
                                           RYGEL_MEDIA_EXPORT_TYPE_METADATA_EXTRACTOR,
                                           G_SIGNAL_RUN_LAST,
                                           0,
                                           NULL,
                                           NULL,
                                           NULL, /* libffi based marshaller */
                                           G_TYPE_NONE,
                                           4,
                                           G_TYPE_FILE,
                                           /* should be
                                            * GST_TYPE_DISCOVERER_INFO, but
                                            * libffi does not know
                                            * GstMiniObject. */
                                           G_TYPE_POINTER,
                                           GUPNP_TYPE_DLNA_PROFILE,
                                           G_TYPE_FILE_INFO);
  /**
   * Signal that an error occured during metadata extraction.
   */
  signals[ERROR] = g_signal_new ("error",
                                 RYGEL_MEDIA_EXPORT_TYPE_METADATA_EXTRACTOR,
                                 G_SIGNAL_RUN_LAST,
                                 0,
                                 NULL,
                                 NULL,
                                 NULL, /* libffi based marshaller */
                                 G_TYPE_NONE,
                                 2,
                                 G_TYPE_FILE,
                                 G_TYPE_ERROR);

  g_type_class_add_private (extractor_class, sizeof (RygelMediaExportMetadataExtractorPrivate));
}

static void
rygel_media_export_metadata_extractor_init (RygelMediaExportMetadataExtractor *self) {
  self->priv = RYGEL_MEDIA_EXPORT_METADATA_EXTRACTOR_GET_PRIVATE (self);
}
