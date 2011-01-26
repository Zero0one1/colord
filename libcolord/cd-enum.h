/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#if !defined (__COLORD_H_INSIDE__) && !defined (CD_COMPILATION)
#error "Only <colord.h> can be included directly."
#endif

#ifndef __CD_TYPES_H
#define __CD_TYPES_H

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * CdDeviceKind:
 *
 * The device type.
 **/
typedef enum {
	CD_DEVICE_KIND_UNKNOWN,
	CD_DEVICE_KIND_DISPLAY,
	CD_DEVICE_KIND_SCANNER,
	CD_DEVICE_KIND_PRINTER,
	CD_DEVICE_KIND_CAMERA,
	CD_DEVICE_KIND_LAST
} CdDeviceKind;

/**
 * CdProfileKind:
 *
 * The profile type.
 **/
typedef enum {
	CD_PROFILE_KIND_UNKNOWN,
	CD_PROFILE_KIND_INPUT_DEVICE,
	CD_PROFILE_KIND_DISPLAY_DEVICE,
	CD_PROFILE_KIND_OUTPUT_DEVICE,
	CD_PROFILE_KIND_DEVICELINK,
	CD_PROFILE_KIND_COLORSPACE_CONVERSION,
	CD_PROFILE_KIND_ABSTRACT,
	CD_PROFILE_KIND_NAMED_COLOR,
	CD_PROFILE_KIND_LAST
} CdProfileKind;

/**
 * CdObjectScope:
 *
 * The options type.
 **/
typedef enum {
	CD_OBJECT_SCOPE_NORMAL		= 0,
	CD_OBJECT_SCOPE_TEMPORARY	= 1,
	CD_OBJECT_SCOPE_DISK		= 2
} CdObjectScope;

/**
 * CdRenderingIntent:
 *
 * The rendering intent.
 **/
typedef enum {
	CD_RENDERING_INTENT_UNKNOWN,
	CD_RENDERING_INTENT_PERCEPTUAL,
	CD_RENDERING_INTENT_RELATIVE_COLORMETRIC,
	CD_RENDERING_INTENT_SATURATION,
	CD_RENDERING_INTENT_ABSOLUTE_COLORMETRIC,
	CD_RENDERING_INTENT_LAST
} CdRenderingIntent;

/**
 * CdColorspace:
 *
 * The known colorspace.
 **/
typedef enum {
	CD_COLORSPACE_UNKNOWN,
	CD_COLORSPACE_XYZ,
	CD_COLORSPACE_LAB,
	CD_COLORSPACE_LUV,
	CD_COLORSPACE_YCBCR,
	CD_COLORSPACE_YXY,
	CD_COLORSPACE_RGB,
	CD_COLORSPACE_GRAY,
	CD_COLORSPACE_HSV,
	CD_COLORSPACE_CMYK,
	CD_COLORSPACE_CMY,
	CD_COLORSPACE_LAST
} CdColorspace;

#define COLORD_DBUS_SERVICE		"org.freedesktop.ColorManager"
#define COLORD_DBUS_PATH		"/org/freedesktop/ColorManager"
#define COLORD_DBUS_INTERFACE		"org.freedesktop.ColorManager"
#define COLORD_DBUS_INTERFACE_DEVICE	"org.freedesktop.ColorManager.Device"
#define COLORD_DBUS_INTERFACE_PROFILE	"org.freedesktop.ColorManager.Profile"

const gchar	*cd_device_kind_to_string		(CdDeviceKind		 kind_enum);
CdDeviceKind	 cd_device_kind_from_string		(const gchar		*kind);
const gchar	*cd_profile_kind_to_string		(CdProfileKind		 profile_kind);
CdProfileKind	 cd_profile_kind_from_string		(const gchar		*profile_kind);
CdRenderingIntent cd_rendering_intent_from_string	(const gchar		*rendering_intent);
const gchar	*cd_rendering_intent_to_string		(CdRenderingIntent	 rendering_intent);
const gchar	*cd_colorspace_to_string		(CdColorspace		 colorspace);
CdColorspace	 cd_colorspace_from_string		(const gchar		*colorspace);

G_END_DECLS

#endif /* __CD_TYPES_H */
