/*
 * Copyright (C) 2009, 2010 Jens Georg <mail@jensge.org>.
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

#include <string.h>

#include "rygel-media-export-string-utils.h"

gchar *
rygel_media_export_string_replace (const gchar  *self,
                                   const gchar  *old,
                                   const gchar  *replacement,
                                   GError      **error) {
  gchar* result;
  gchar *escaped;
  GError *inner_error;
  GRegex *regex;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (old != NULL, NULL);
  g_return_val_if_fail (replacement != NULL, NULL);

  result = NULL;
  escaped = g_regex_escape_string (old, -1);
  inner_error = NULL;
  regex = g_regex_new (escaped, 0, 0, &inner_error);
  g_free (escaped);
  if (inner_error) {
    g_propagate_error (error, inner_error);
  } else {
    result = g_regex_replace_literal (regex, self, -1, 0, replacement, 0, &inner_error);

    if (inner_error) {
      g_propagate_error (error, inner_error);
    }
    g_regex_unref (regex);
  }

  return result;
}

gchar *
rygel_media_export_string_slice (const gchar *self,
                                 glong        start,
                                 glong        end) {
  glong string_length;
  gboolean outside;

  g_return_val_if_fail (self != NULL, NULL);

  string_length = strlen (self);
  if (start < 0) {
    start += string_length;
  }
  if (end < 0) {
    end += string_length;
  }
  if (start >= 0) {
    outside = (start > string_length);
  } else {
    outside = TRUE;
  }
  if (outside) {
    g_warning ("Start offset (%ld) is outside string (%s).", start, self);
    return NULL;
  }
  if (end >= 0) {
    outside = (end > string_length);
  } else {
    outside = TRUE;
  }
  if (outside) {
    g_warning ("End offset (%ld) is outside string (%s).", end, self);
    return NULL;
  }
  if (start > end) {
    g_warning ("Start offset (%ld) is later than end offset (%ld).", start, end);
    return NULL;
  }
  return g_strndup (self + start, (gsize) (end - start));
}
