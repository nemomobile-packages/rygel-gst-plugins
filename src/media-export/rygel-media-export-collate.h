/*
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

#ifndef __RYGEL_GST_0_10_PLUGINS_MEDIA_EXPORT_COLLATE_H__
#define __RYGEL_GST_0_10_PLUGINS_MEDIA_EXPORT_COLLATE_H__

#include <glib.h>

G_BEGIN_DECLS

gint
rygel_media_export_utf8_collate_str (const char *a,
                                     const char *b);

#endif /* __RYGEL_GST_0_10_PLUGINS_MEDIA_EXPORT_COLLATE_H__ */
