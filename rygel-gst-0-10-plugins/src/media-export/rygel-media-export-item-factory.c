/*
 * Copyright (C) 2008 Zeeshan Ali <zeenix@gmail.com>.
 * Copyright (C) 2008 Nokia Corporation.
 * Copyright (C) 2013 Intel Corporation
 *
 * Author: Zeeshan Ali <zeenix@gmail.com>
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

#include <gst/tag/tag.h>

#include "rygel-media-export-item-factory.h"
#include "rygel-media-export-jpeg-writer.h"

static RygelMediaItem *
rygel_media_export_item_factory_fill_photo_item (RygelMediaExportPhotoItem *item,
                                                 GFile                     *file,
                                                 GstDiscovererInfo         *info,
                                                 GUPnPDLNAProfile          *profile,
                                                 GstDiscovererVideoInfo    *video_info,
                                                 GFileInfo                 *file_info);

static RygelMediaItem *
rygel_media_export_item_factory_fill_video_item (RygelMediaExportVideoItem *item,
                                                 GFile                     *file,
                                                 GstDiscovererInfo         *info,
                                                 GUPnPDLNAProfile          *profile,
                                                 GstDiscovererVideoInfo    *video_info,
                                                 GstDiscovererAudioInfo    *audio_info,
                                                 GFileInfo                 *file_info);

static RygelMediaItem *
rygel_media_export_item_factory_fill_music_item (RygelMediaExportMusicItem *item,
                                                 GFile                     *file,
                                                 GstDiscovererInfo         *info,
                                                 GUPnPDLNAProfile          *profile,
                                                 GstDiscovererAudioInfo    *audio_info,
                                                 GFileInfo                 *file_info);

static void
rygel_media_export_item_factory_fill_media_item (RygelMediaItem       *item,
                                                 GFile                *file,
                                                 GstDiscovererInfo    *info,
                                                 GUPnPDLNAProfile     *profile,
                                                 GFileInfo            *file_info);

RygelMediaItem *
rygel_media_export_item_factory_create_simple (RygelMediaContainer *parent,
                                               GFile               *file,
                                               GFileInfo           *info) {
  RygelMediaItem *item;
  gchar *title;
  gchar *mime;
  gchar *id;
  gchar *uri;

  g_return_val_if_fail (RYGEL_IS_MEDIA_CONTAINER (parent), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_FILE_INFO (info), NULL);

  title =  g_strdup (g_file_info_get_display_name (info));
  mime = g_content_type_get_mime_type (g_file_info_get_content_type (info));
  id = rygel_media_export_media_cache_get_id (file);

  if (g_str_has_prefix (mime, "video/")) {
    item = RYGEL_MEDIA_ITEM (rygel_media_export_video_item_new (id, parent, title, RYGEL_VIDEO_ITEM_UPNP_CLASS));
  } else if (g_str_has_prefix (mime, "image/")) {
    item = RYGEL_MEDIA_ITEM (rygel_media_export_photo_item_new (id, parent, title, RYGEL_PHOTO_ITEM_UPNP_CLASS));
  } else /* if (g_str_has_prefix (mime, "audio/") ||
    g_strcmp0 (mime, "application/ogg") == 0) */ {
    item = RYGEL_MEDIA_ITEM (rygel_media_export_music_item_new (id, parent, title, RYGEL_MUSIC_ITEM_UPNP_CLASS));
  } /* TODO: playlist */
  g_free (id);
  g_free (title);

  rygel_media_item_set_mime_type (item, mime);
  g_free (mime);

  rygel_media_item_set_size (item, (gint64) g_file_info_get_size (info));
  rygel_media_object_set_modified (RYGEL_MEDIA_OBJECT (item),
                                   g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED));

  uri = g_file_get_uri (file);
  rygel_media_item_add_uri (item, uri);
  g_free (uri);

  return item;
}

RygelMediaItem *
rygel_media_export_item_factory_create_from_info (RygelMediaContainer *parent,
                                                  GFile               *file,
                                                  GstDiscovererInfo   *info,
                                                  GUPnPDLNAProfile    *profile,
                                                  GFileInfo           *file_info) {
  RygelMediaItem *result;
  gchar *id;
  GList *audio_streams;
  GList *video_streams;

  g_return_val_if_fail (RYGEL_IS_MEDIA_CONTAINER (parent), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (GST_IS_DISCOVERER_INFO (info), NULL);
  g_return_val_if_fail (profile == NULL || GUPNP_IS_DLNA_PROFILE (profile), NULL);
  g_return_val_if_fail (G_IS_FILE_INFO (file_info), NULL);

  id = rygel_media_export_media_cache_get_id (file);

  audio_streams = gst_discoverer_info_get_audio_streams (info);
  video_streams = gst_discoverer_info_get_video_streams (info);

  if (!audio_streams && !video_streams) {
    gchar *uri = g_file_get_uri (file);

    g_debug ("%s had neither audio nor video/picture streams. Ignoring.", uri);
    g_free (uri);

    result = NULL;
  } else if (!audio_streams &&
             gst_discoverer_video_info_is_image ((GstDiscovererVideoInfo *) video_streams->data)) {
    RygelMediaExportPhotoItem *item = rygel_media_export_photo_item_new (id,
                                                                         parent,
                                                                         "",
                                                                         RYGEL_PHOTO_ITEM_UPNP_CLASS);

    result = rygel_media_export_item_factory_fill_photo_item (item,
                                                              file,
                                                              info,
                                                              profile,
                                                              (GstDiscovererVideoInfo *) video_streams->data,
                                                              file_info);
  } else if (video_streams) {
    RygelMediaExportVideoItem *item = rygel_media_export_video_item_new (id,
                                                                         parent,
                                                                         "",
                                                                         RYGEL_VIDEO_ITEM_UPNP_CLASS);
    GstDiscovererAudioInfo *audio_info;

    if (audio_streams) {
      audio_info = (GstDiscovererAudioInfo*) audio_streams->data;
    } else {
      audio_info = NULL;
    }

    result = rygel_media_export_item_factory_fill_video_item (item,
                                                              file,
                                                              info,
                                                              profile,
                                                              (GstDiscovererVideoInfo *) video_streams->data,
                                                              audio_info,
                                                              file_info);
  } else if (audio_streams) {
    RygelMediaExportMusicItem *item = rygel_media_export_music_item_new (id,
                                                                         parent,
                                                                         "",
                                                                         RYGEL_MUSIC_ITEM_UPNP_CLASS);

    result = rygel_media_export_item_factory_fill_music_item (item,
                                                              file,
                                                              info,
                                                              profile,
                                                              (GstDiscovererAudioInfo*) audio_streams->data,
                                                              file_info);
  } else {
    result = NULL;
  }

  gst_discoverer_stream_info_list_free (audio_streams);
  gst_discoverer_stream_info_list_free (video_streams);

  g_free (id);

  return result;
}

static void
rygel_media_export_item_factory_fill_audio_item (RygelAudioItem         *item,
                                                 GstDiscovererInfo      *info,
                                                 GUPnPDLNAProfile       *profile,
                                                 GstDiscovererAudioInfo *audio_info) {
  GstClockTime duration;
  const GstTagList *tags;

  g_return_if_fail (RYGEL_IS_AUDIO_ITEM (item));
  g_return_if_fail (GST_IS_DISCOVERER_INFO (info));
  g_return_if_fail (profile == NULL || GUPNP_IS_DLNA_PROFILE (profile));
  g_return_if_fail (audio_info == NULL || GST_IS_DISCOVERER_AUDIO_INFO (audio_info));

  duration = gst_discoverer_info_get_duration (info);
  if (duration > (GstClockTime) 0) {
    rygel_audio_item_set_duration (item, (glong) (duration / GST_SECOND));
  } else {
    rygel_audio_item_set_duration (item, (glong) (-1));
  }

  if (!audio_info) {
    return;
  }

  tags = gst_discoverer_stream_info_get_tags ((GstDiscovererStreamInfo*) audio_info);
  if (tags) {
    guint tmp = 0U;

    gst_tag_list_get_uint (tags, GST_TAG_BITRATE, &tmp);
    rygel_audio_item_set_bitrate (item, (gint) tmp / 8);
  }

  rygel_audio_item_set_channels (item,
                                 (gint) gst_discoverer_audio_info_get_channels (audio_info));
  rygel_audio_item_set_sample_freq (item,
                                    (gint) gst_discoverer_audio_info_get_sample_rate (audio_info));
}

static RygelMediaItem *
rygel_media_export_item_factory_fill_video_item (RygelMediaExportVideoItem *item,
                                                 GFile                     *file,
                                                 GstDiscovererInfo         *info,
                                                 GUPnPDLNAProfile          *profile,
                                                 GstDiscovererVideoInfo    *video_info,
                                                 GstDiscovererAudioInfo    *audio_info,
                                                 GFileInfo                 *file_info) {
  RygelVisualItem *visual_item;
  RygelMediaItem *media_item;
  gint color_depth;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_VIDEO_ITEM (item), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (GST_IS_DISCOVERER_INFO (info), NULL);
  g_return_val_if_fail (profile == NULL || GUPNP_IS_DLNA_PROFILE (profile), NULL);
  g_return_val_if_fail (GST_IS_DISCOVERER_VIDEO_INFO (video_info), NULL);
  g_return_val_if_fail (audio_info == NULL || GST_IS_DISCOVERER_AUDIO_INFO (audio_info), NULL);
  g_return_val_if_fail (G_IS_FILE_INFO (file_info), NULL);

  rygel_media_export_item_factory_fill_audio_item (RYGEL_AUDIO_ITEM (item),
                                                   info,
                                                   profile,
                                                   audio_info);
  media_item = RYGEL_MEDIA_ITEM (item);
  rygel_media_export_item_factory_fill_media_item (media_item,
                                                   file,
                                                   info,
                                                   profile,
                                                   file_info);
  visual_item = RYGEL_VISUAL_ITEM (item);

  rygel_visual_item_set_width (visual_item,
                               (gint) gst_discoverer_video_info_get_width (video_info));
  rygel_visual_item_set_height (visual_item,
                                (gint) gst_discoverer_video_info_get_height (video_info));

  color_depth = gst_discoverer_video_info_get_depth (video_info);
  rygel_visual_item_set_color_depth (visual_item,
                                     ((color_depth == 0) ? -1 : color_depth));

  return media_item;
}

static RygelMediaItem *
rygel_media_export_item_factory_fill_photo_item (RygelMediaExportPhotoItem *item,
                                                 GFile                     *file,
                                                 GstDiscovererInfo         *info,
                                                 GUPnPDLNAProfile          *profile,
                                                 GstDiscovererVideoInfo    *video_info,
                                                 GFileInfo                 *file_info) {
  RygelVisualItem *visual_item;
  RygelMediaItem *media_item;
  gint color_depth;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_PHOTO_ITEM (item), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (GST_IS_DISCOVERER_INFO (info), NULL);
  g_return_val_if_fail (profile == NULL || GUPNP_IS_DLNA_PROFILE (profile), NULL);
  g_return_val_if_fail (GST_IS_DISCOVERER_VIDEO_INFO (video_info), NULL);
  g_return_val_if_fail (G_IS_FILE_INFO (file_info), NULL);

  media_item = RYGEL_MEDIA_ITEM (item);
  rygel_media_export_item_factory_fill_media_item (media_item,
                                                   file,
                                                   info,
                                                   profile,
                                                   file_info);

  visual_item = RYGEL_VISUAL_ITEM (item);
  rygel_visual_item_set_width (visual_item,
                               (gint) gst_discoverer_video_info_get_width (video_info));
  rygel_visual_item_set_height (visual_item,
                                (gint) gst_discoverer_video_info_get_height (video_info));

  color_depth = gst_discoverer_video_info_get_depth (video_info);
  rygel_visual_item_set_color_depth (visual_item,
                                     ((color_depth == 0) ? -1 : color_depth));

  return media_item;
}

static RygelMediaItem *
rygel_media_export_item_factory_fill_music_item (RygelMediaExportMusicItem *item,
                                                 GFile                     *file,
                                                 GstDiscovererInfo         *info,
                                                 GUPnPDLNAProfile          *profile,
                                                 GstDiscovererAudioInfo    *audio_info,
                                                 GFileInfo                 *file_info) {
  RygelMediaItem *media_item;
  const GstTagList *tags;
  gchar *artist;
  gchar *album;
  gchar *genre;
  guint volume_number;
  guint track_number;
  GstBuffer *buffer;
  GstStructure *structure;
  gint image_type;

  g_return_val_if_fail (RYGEL_MEDIA_EXPORT_IS_MUSIC_ITEM (item), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (GST_IS_DISCOVERER_INFO (info), NULL);
  g_return_val_if_fail (profile == NULL || GUPNP_IS_DLNA_PROFILE (profile), NULL);
  g_return_val_if_fail (audio_info == NULL || GST_IS_DISCOVERER_AUDIO_INFO (audio_info), NULL);
  g_return_val_if_fail (G_IS_FILE_INFO (file_info), NULL);

  rygel_media_export_item_factory_fill_audio_item (RYGEL_AUDIO_ITEM (item),
                                                   info,
                                                   profile,
                                                   audio_info);
  media_item = RYGEL_MEDIA_ITEM (item);
  rygel_media_export_item_factory_fill_media_item (media_item,
                                                   file,
                                                   info,
                                                   profile,
                                                   file_info);

  if (!audio_info) {
    return media_item;
  }

  tags = gst_discoverer_stream_info_get_tags ((GstDiscovererStreamInfo*) audio_info);
  if (!tags) {
    return media_item;
  }

  artist = NULL;
  gst_tag_list_get_string (tags, GST_TAG_ARTIST, &artist);
  rygel_music_item_set_artist (RYGEL_MUSIC_ITEM (item), artist);
  g_free (artist);

  album = NULL;
  gst_tag_list_get_string (tags, GST_TAG_ALBUM, &album);
  rygel_music_item_set_album (RYGEL_MUSIC_ITEM (item), album);
  g_free (album);

  genre = NULL;
  gst_tag_list_get_string (tags, GST_TAG_GENRE, &genre);
  rygel_music_item_set_genre (RYGEL_MUSIC_ITEM (item), genre);
  g_free (genre);

  volume_number = 0U;
  gst_tag_list_get_uint (tags, GST_TAG_ALBUM_VOLUME_NUMBER, &volume_number);
  item->disc = volume_number;

  track_number = 0U;
  gst_tag_list_get_uint (tags, GST_TAG_TRACK_NUMBER, &track_number);
  rygel_music_item_set_track_number (RYGEL_MUSIC_ITEM (item), track_number);

  rygel_audio_item_set_sample_freq (RYGEL_AUDIO_ITEM (item),
                                    (gint) gst_discoverer_audio_info_get_sample_rate (audio_info));

  buffer = NULL;
  gst_tag_list_get_buffer (tags, GST_TAG_IMAGE, &buffer);
  if (!buffer || !buffer->caps) {
    return media_item;
  }

  structure = gst_caps_get_structure (buffer->caps,  0);
  if (!structure)
    return media_item;

  image_type = 0;
  gst_structure_get_enum (structure,
                          "image-type",
                          GST_TYPE_TAG_IMAGE_TYPE,
                          &image_type);
  switch (image_type) {
  case GST_TAG_IMAGE_TYPE_UNDEFINED:
  case GST_TAG_IMAGE_TYPE_FRONT_COVER: {
    RygelMediaArtStore *store = rygel_media_art_store_get_default ();
    GFile* thumb = rygel_media_art_store_get_media_art_file (store,
                                                             "album",
                                                             RYGEL_MUSIC_ITEM (item),
                                                             TRUE);
    RygelMediaExportJPEGWriter *writer = rygel_media_export_jpeg_writer_new (NULL);

    if (writer) {
      rygel_media_export_jpeg_writer_write (writer, buffer, thumb);
      g_object_unref (writer);
    }

    g_object_unref (thumb);
    g_object_unref (store); /* TODO: Did get_default() return a reference()? */
    break;
  }
  default:
    break;
  }


  return media_item;
}

static void
rygel_media_export_item_factory_fill_media_item (RygelMediaItem       *item,
                                                 GFile                *file,
                                                 GstDiscovererInfo    *info,
                                                 GUPnPDLNAProfile     *profile,
                                                 GFileInfo            *file_info) {
  const GstTagList *tags;
  gchar *title;
  GDate *date;
  guint64 mtime;
  RygelMediaObject *media_object;
  gchar *uri;
  gboolean mime_set;

  g_return_if_fail (RYGEL_IS_MEDIA_ITEM (item));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (GST_IS_DISCOVERER_INFO (info));
  g_return_if_fail (profile == NULL || GUPNP_IS_DLNA_PROFILE (profile));
  g_return_if_fail (G_IS_FILE_INFO (file_info));

  tags = gst_discoverer_info_get_tags (info);
  title = NULL;
  if (!tags || !gst_tag_list_get_string (tags, GST_TAG_TITLE, &title)) {
    title = g_strdup (g_file_info_get_display_name (file_info));
  }

  media_object = RYGEL_MEDIA_OBJECT (item);
  rygel_media_object_set_title (media_object, title);
  g_free (title);

  date = NULL;
  if (tags) {
    if (gst_tag_list_get_date (tags, GST_TAG_DATE, &date)) {
      gint datestr_length = 30;
      gchar *datestr = g_new0 (gchar, datestr_length);

      g_date_strftime (datestr, datestr_length, "%F", date);
      rygel_media_item_set_date (item, datestr);
      g_free (datestr);
    }
  }

  // use mtime if no time tag was available
  mtime = g_file_info_get_attribute_uint64 (file_info,
    G_FILE_ATTRIBUTE_TIME_MODIFIED);
  if (!date) {
    GTimeVal tv = {0, 0};
    gchar *datestr;

    tv.tv_sec = (glong) mtime;
    tv.tv_usec = (glong) 0;
    datestr = g_time_val_to_iso8601 (&tv);
    rygel_media_item_set_date (item, datestr);
    g_free (datestr);
  } else {
    g_date_free (date);
  }

  rygel_media_item_set_size (item, (gint64) g_file_info_get_size (file_info));
  rygel_media_object_set_modified (media_object, (guint64) mtime);

  mime_set = FALSE;
  if (profile) {
    const gchar *name = gupnp_dlna_profile_get_name (profile);

    if (name) {
      mime_set = TRUE;
      rygel_media_item_set_dlna_profile (item, name);

      rygel_media_item_set_mime_type (item, gupnp_dlna_profile_get_mime (profile));
    }
  }
  if (!mime_set) {
    const gchar *content_type = g_file_info_get_content_type (file_info);
    gchar *content_type_mime_type = g_content_type_get_mime_type (content_type);

    rygel_media_item_set_mime_type (item, content_type_mime_type);
    g_free (content_type_mime_type);
  }

  uri = g_file_get_uri (file);
  rygel_media_item_add_uri (item, uri);
  g_free (uri);
}
