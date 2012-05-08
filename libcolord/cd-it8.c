/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
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

/**
 * SECTION:cd-it8
 * @short_description: Read and write IT8 color sample exchange files
 *
 * This object represents .ti1 and .ti3 files which can contain raw
 * or normalized sample data.
 */

#include "config.h"

#include <glib.h>
#include <lcms2.h>
#include <stdlib.h>

#include "cd-it8.h"
#include "cd-color.h"

static void	cd_it8_class_init	(CdIt8Class	*klass);
static void	cd_it8_init		(CdIt8		*it8);
static void	cd_it8_finalize		(GObject	*object);

#define CD_IT8_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_IT8, CdIt8Private))

/**
 * CdIt8Private:
 *
 * Private #CdIt8 data
 **/
struct _CdIt8Private
{
	CdIt8Kind		 kind;
	CdMat3x3		 matrix;
	gboolean		 normalized;
	gboolean		 spectral;
	gchar			*instrument;
	gchar			*reference;
	gchar			*originator;
	GPtrArray		*array_rgb;
	GPtrArray		*array_xyz;
};

enum {
	PROP_0,
	PROP_KIND,
	PROP_INSTRUMENT,
	PROP_REFERENCE,
	PROP_NORMALIZED,
	PROP_ORIGINATOR,
	PROP_SPECTRAL,
	PROP_LAST
};

G_DEFINE_TYPE (CdIt8, cd_it8, G_TYPE_OBJECT)

/**
 * cd_it8_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.1.0
 **/
GQuark
cd_it8_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark) {
		quark = g_quark_from_static_string ("cd_it8_error");
	}
	return quark;
}

/**
 * cd_it8_set_matrix:
 * @it8: a #CdIt8 instance.
 * @matrix: a #CdMat3x3.
 *
 * Set the calibration matrix in the it8 file.
 *
 * Since: 0.1.20
 **/
void
cd_it8_set_matrix (CdIt8 *it8, const CdMat3x3 *matrix)
{
	g_return_if_fail (CD_IS_IT8 (it8));
	cd_mat33_copy (matrix, &it8->priv->matrix);
}

/**
 * cd_it8_get_matrix:
 * @it8: a #CdIt8 instance.
 *
 * Gets the calibration matrix in the it8 file.
 *
 * Return value: a #CdMat3x3.
 *
 * Since: 0.1.20
 **/
const CdMat3x3 *
cd_it8_get_matrix (CdIt8 *it8)
{
	g_return_val_if_fail (CD_IS_IT8 (it8), NULL);
	return &it8->priv->matrix;
}

/**
 * cd_it8_set_kind:
 * @it8: a #CdIt8 instance.
 * @kind: a #CdIt8Kind, e.g %CD_IT8_KIND_TI3.
 *
 * Set the kind of IT8 file.
 *
 * Since: 0.1.20
 **/
void
cd_it8_set_kind (CdIt8 *it8, CdIt8Kind kind)
{
	g_return_if_fail (CD_IS_IT8 (it8));
	it8->priv->kind = kind;
}

/**
 * cd_it8_get_kind:
 * @it8: a #CdIt8 instance.
 *
 * Gets the kind of IT8 file.
 *
 * Return value: a #CdIt8Kind, e.g %CD_IT8_KIND_TI3.
 *
 * Since: 0.1.20
 **/
CdIt8Kind
cd_it8_get_kind (CdIt8 *it8)
{
	g_return_val_if_fail (CD_IS_IT8 (it8), 0);
	return it8->priv->kind;
}

/**
 * cd_it8_lcms_error_cb:
 **/
static void
cd_it8_lcms_error_cb (cmsContext ContextID,
		      cmsUInt32Number error_code,
		      const char *text)
{
	g_warning ("LCMS: %s", text);
}

/**
 * cd_it8_parse_luminance:
 **/
static gboolean
cd_it8_parse_luminance (const gchar *text, CdColorXYZ *xyz, GError **error)
{
	gboolean ret = TRUE;
	gchar **split;

	split = g_strsplit (text, " ", -1);
	if (g_strv_length (split) != 3) {
		ret = FALSE;
		g_set_error (error,
			     1, 0,
			     "LUMINANCE_XYZ_CDM2 format invalid: %s",
			     text);
		goto out;
	}
	xyz->X = atof (split[0]);
	xyz->Y = atof (split[1]);
	xyz->Z = atof (split[2]);
out:
	g_strfreev (split);
	return ret;
}

/**
 * cd_it8_get_originator:
 * @it8: a #CdIt8 instance.
 *
 * Gets the file orginator.
 *
 * Return value: The originator, or %NULL if unset
 *
 * Since: 0.1.20
 **/
const gchar *
cd_it8_get_originator (CdIt8 *it8)
{
	g_return_val_if_fail (CD_IS_IT8 (it8), NULL);
	return it8->priv->originator;
}

/**
 * cd_it8_get_instrument:
 * @it8: a #CdIt8 instance.
 *
 * Gets the instrument the file was created by.
 *
 * Return value: The instrument, or %NULL if unset
 *
 * Since: 0.1.20
 **/
const gchar *
cd_it8_get_instrument (CdIt8 *it8)
{
	g_return_val_if_fail (CD_IS_IT8 (it8), NULL);
	return it8->priv->instrument;
}

/**
 * cd_it8_get_reference:
 * @it8: a #CdIt8 instance.
 *
 * Gets the reference the file was created against.
 *
 * Return value: The reference, or %NULL if unset
 *
 * Since: 0.1.20
 **/
const gchar *
cd_it8_get_reference (CdIt8 *it8)
{
	g_return_val_if_fail (CD_IS_IT8 (it8), NULL);
	return it8->priv->reference;
}

/**
 * cd_it8_get_normalized:
 * @it8: a #CdIt8 instance.
 *
 * Gets if the data should be written normlaised to y=100.
 *
 * Return value: %TRUE if the data should be normalised.
 *
 * Since: 0.1.20
 **/
gboolean
cd_it8_get_normalized (CdIt8 *it8)
{
	g_return_val_if_fail (CD_IS_IT8 (it8), FALSE);
	return it8->priv->normalized;
}

/**
 * cd_it8_get_spectral:
 * @it8: a #CdIt8 instance.
 *
 * Gets if the data is spectral or XYZ.
 *
 * Return value: %TRUE if the data is in spectral bands.
 *
 * Since: 0.1.20
 **/
gboolean
cd_it8_get_spectral (CdIt8 *it8)
{
	g_return_val_if_fail (CD_IS_IT8 (it8), FALSE);
	return it8->priv->spectral;
}

/**
 * cd_it8_load_ti1_ti3:
 **/
static gboolean
cd_it8_load_ti1_ti3 (CdIt8 *it8, cmsHANDLE it8_lcms, GError **error)
{
	CdColorRGB *rgb;
	CdColorXYZ luminance;
	CdColorXYZ *xyz;
	const gchar *tmp;
	gboolean ret = TRUE;
	gboolean scaled_to_y100 = FALSE;
	guint i;
	guint number_of_sets = 0;

	tmp = cmsIT8GetProperty (it8_lcms, "COLOR_REP");
	if (g_strcmp0 (tmp, "RGB_XYZ") != 0) {
		ret = FALSE;
		g_set_error (error, 1, 0, "Invalid data format: %s", tmp);
		goto out;
	}

	/* if normalized, then scale back up */
	tmp = cmsIT8GetProperty (it8_lcms, "NORMALIZED_TO_Y_100");
	if (g_strcmp0 (tmp, "YES") == 0) {
		scaled_to_y100 = TRUE;
		tmp = cmsIT8GetProperty (it8_lcms, "LUMINANCE_XYZ_CDM2");
		ret = cd_it8_parse_luminance (tmp, &luminance, error);
		if (!ret)
			goto out;
	}

	/* set spectral flag */
	tmp = cmsIT8GetProperty (it8_lcms, "INSTRUMENT_TYPE_SPECTRAL");
	cd_it8_set_spectral (it8, g_strcmp0 (tmp, "YES") == 0);

	/* set instrument */
	cd_it8_set_instrument (it8, cmsIT8GetProperty (it8_lcms, "TARGET_INSTRUMENT"));

	/* copy out data entries */
	number_of_sets = cmsIT8GetPropertyDbl (it8_lcms, "NUMBER_OF_SETS");
	for (i = 0; i < number_of_sets; i++) {
		rgb = cd_color_rgb_new ();
		rgb->R = cmsIT8GetDataRowColDbl(it8_lcms, i, 1);
		rgb->G = cmsIT8GetDataRowColDbl(it8_lcms, i, 2);
		rgb->B = cmsIT8GetDataRowColDbl(it8_lcms, i, 3);
		if (scaled_to_y100) {
			rgb->R /= 100.0f;
			rgb->G /= 100.0f;
			rgb->B /= 100.0f;
		}
		g_ptr_array_add (it8->priv->array_rgb, rgb);
		xyz = cd_color_xyz_new ();
		xyz->X = cmsIT8GetDataRowColDbl(it8_lcms, i, 4);
		xyz->Y = cmsIT8GetDataRowColDbl(it8_lcms, i, 5);
		xyz->Z = cmsIT8GetDataRowColDbl(it8_lcms, i, 6);
		if (scaled_to_y100) {
			xyz->X /= 100.0f;
			xyz->Y /= 100.0f;
			xyz->Z /= 100.0f;
			xyz->X *= luminance.X;
			xyz->Y *= luminance.Y;
			xyz->Z *= luminance.Z;
		}
		g_ptr_array_add (it8->priv->array_xyz, xyz);
	}
out:
	return ret;
}

/**
 * cd_it8_load_ccmx:
 **/
static gboolean
cd_it8_load_ccmx (CdIt8 *it8, cmsHANDLE it8_lcms, GError **error)
{
	gboolean ret = TRUE;
	const gchar *tmp;

	/* check color format */
	tmp = cmsIT8GetProperty (it8_lcms, "COLOR_REP");
	if (g_strcmp0 (tmp, "XYZ") != 0) {
		ret = FALSE;
		g_set_error (error, 1, 0, "Invalid CCMX data format: %s", tmp);
		goto out;
	}

	/* set instrument */
	cd_it8_set_instrument (it8, cmsIT8GetProperty (it8_lcms, "INSTRUMENT"));

	/* just load the matrix */
	it8->priv->matrix.m00 = cmsIT8GetDataRowColDbl(it8_lcms, 0, 0);
	it8->priv->matrix.m01 = cmsIT8GetDataRowColDbl(it8_lcms, 0, 1);
	it8->priv->matrix.m02 = cmsIT8GetDataRowColDbl(it8_lcms, 0, 2);
	it8->priv->matrix.m10 = cmsIT8GetDataRowColDbl(it8_lcms, 1, 0);
	it8->priv->matrix.m11 = cmsIT8GetDataRowColDbl(it8_lcms, 1, 1);
	it8->priv->matrix.m12 = cmsIT8GetDataRowColDbl(it8_lcms, 1, 2);
	it8->priv->matrix.m20 = cmsIT8GetDataRowColDbl(it8_lcms, 2, 0);
	it8->priv->matrix.m21 = cmsIT8GetDataRowColDbl(it8_lcms, 2, 1);
	it8->priv->matrix.m22 = cmsIT8GetDataRowColDbl(it8_lcms, 2, 2);
out:
	return ret;
}

/**
 * cd_it8_load:
 * @it8: a #CdIt8 instance.
 * @file: a #GFile
 * @error: a #GError, or %NULL
 *
 * Loads a it8 file from disk.
 *
 * Return value: %TRUE if a valid it8 file was read.
 *
 * Since: 0.1.20
 **/
gboolean
cd_it8_load (CdIt8 *it8, GFile *file, GError **error)
{
	cmsHANDLE it8_lcms = NULL;
	const gchar *tmp;
	gboolean ret;
	gchar *data = NULL;
	gsize size = 0;

	g_return_val_if_fail (CD_IS_IT8 (it8), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	/* clear old data */
	g_ptr_array_set_size (it8->priv->array_rgb, 0);
	g_ptr_array_set_size (it8->priv->array_xyz, 0);
	cd_mat33_clear (&it8->priv->matrix);

	/* load file */
	ret = g_file_load_contents (file, NULL, &data, &size, NULL, error);
	if (!ret)
		goto out;

	/* load the it8 data */
	cmsSetLogErrorHandler (cd_it8_lcms_error_cb);
	it8_lcms = cmsIT8LoadFromMem (NULL, data, size);
	if (it8_lcms == NULL) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "Cannot open %s",
			     g_file_get_path (file));
		goto out;
	}

	/* get sheet type */
	tmp = cmsIT8GetSheetType (it8_lcms);
	if (g_str_has_prefix (tmp, "CTI1")) {
		cd_it8_set_kind (it8, CD_IT8_KIND_TI1);
	} else if (g_str_has_prefix (tmp, "CTI3")) {
		cd_it8_set_kind (it8, CD_IT8_KIND_TI3);
	} else if (g_str_has_prefix (tmp, "CCMX")) {
		cd_it8_set_kind (it8, CD_IT8_KIND_CCMX);
	} else {
		ret = FALSE;
		g_set_error (error, 1, 0, "Invalid sheet type: %s", tmp);
		goto out;
	}

	/* get ti1 and ti3 specific data */
	if (it8->priv->kind == CD_IT8_KIND_TI1 ||
	    it8->priv->kind == CD_IT8_KIND_TI3) {
		ret = cd_it8_load_ti1_ti3 (it8, it8_lcms, error);
		if (!ret)
			goto out;
	} else if (it8->priv->kind == CD_IT8_KIND_CCMX) {
		ret = cd_it8_load_ccmx (it8, it8_lcms, error);
		if (!ret)
			goto out;
	}

	/* set common bits */
	cd_it8_set_originator (it8, cmsIT8GetProperty (it8_lcms, "ORIGINATOR"));
	cd_it8_set_reference (it8, cmsIT8GetProperty (it8_lcms, "REFERENCE"));
out:
	if (it8_lcms != NULL)
		cmsIT8Free (it8_lcms);
	g_free (data);
	return ret;
}

/**
 * cd_it8_color_match:
 **/
static gboolean
cd_it8_color_match (CdColorRGB *rgb, gdouble r, gdouble g, gdouble b)
{
	if (ABS (rgb->R - r) > 0.01f)
		return FALSE;
	if (ABS (rgb->G - g) > 0.01f)
		return FALSE;
	if (ABS (rgb->B - b) > 0.01f)
		return FALSE;
	return TRUE;
}

/**
 * cd_it8_save_ti1_ti3:
 **/
static gboolean
cd_it8_save_ti1_ti3 (CdIt8 *it8, cmsHANDLE it8_lcms, GError **error)
{
	CdColorRGB *rgb_tmp;
	CdColorXYZ lumi_xyz;
	CdColorXYZ *xyz_tmp;
	gboolean ret = TRUE;
	gchar *lumi_str = NULL;
	gdouble normalize = 0.0f;
	guint i;
	guint luminance_samples = 0;

	/* calculate the absolute XYZ in candelas per meter squared */
	cd_color_clear_xyz (&lumi_xyz);
	if (it8->priv->normalized) {
		for (i = 0; i < it8->priv->array_rgb->len; i++) {
			rgb_tmp = g_ptr_array_index (it8->priv->array_rgb, i);

			/* is this 100% white? */
			ret = cd_it8_color_match (rgb_tmp, 1.0f, 1.0f, 1.0f);
			if (!ret)
				continue;
			luminance_samples++;
			xyz_tmp = g_ptr_array_index (it8->priv->array_xyz, i);
			lumi_xyz.X += xyz_tmp->X;
			lumi_xyz.Y += xyz_tmp->Y;
			lumi_xyz.Z += xyz_tmp->Z;
			if (xyz_tmp->Y > normalize)
				normalize = xyz_tmp->Y;
		}
		if (luminance_samples == 0) {
			ret = FALSE;
			g_set_error_literal (error, 1, 0,
					     "Failed to find any white samples");
			goto out;
		}
		lumi_xyz.X /= luminance_samples;
		lumi_xyz.Y /= luminance_samples;
		lumi_xyz.Z /= luminance_samples;

		/* scale all the readings to 100 */
		normalize = 100.0f / normalize;
	}
	lumi_str = g_strdup_printf ("%f %f %f", lumi_xyz.X, lumi_xyz.Y, lumi_xyz.Z);

	/* write data */
	if (it8->priv->kind == CD_IT8_KIND_TI1) {
		cmsIT8SetSheetType (it8_lcms, "CTI1");
		cmsIT8SetPropertyStr (it8_lcms, "DESCRIPTOR",
				      "Calibration Target chart information 1");
	} else if (it8->priv->kind == CD_IT8_KIND_TI3) {
		cmsIT8SetSheetType (it8_lcms, "CTI3");
		cmsIT8SetPropertyStr (it8_lcms, "DESCRIPTOR",
				      "Calibration Target chart information 3");
	}
	if (it8->priv->kind == CD_IT8_KIND_TI3) {
		cmsIT8SetPropertyStr (it8_lcms, "DEVICE_CLASS",
				      "DISPLAY");
	}
	cmsIT8SetPropertyStr (it8_lcms, "COLOR_REP", "RGB_XYZ");
	if (it8->priv->instrument != NULL) {
		cmsIT8SetPropertyStr (it8_lcms, "TARGET_INSTRUMENT",
				      it8->priv->instrument);
	}
	cmsIT8SetPropertyStr (it8_lcms, "INSTRUMENT_TYPE_SPECTRAL",
			      it8->priv->spectral ? "YES" : "NO");
	if (it8->priv->normalized) {
		cmsIT8SetPropertyStr (it8_lcms, "NORMALIZED_TO_Y_100", "YES");
		cmsIT8SetPropertyStr (it8_lcms, "LUMINANCE_XYZ_CDM2", lumi_str);
	} else {
		cmsIT8SetPropertyStr (it8_lcms, "NORMALIZED_TO_Y_100", "NO");
	}
	cmsIT8SetPropertyDbl (it8_lcms, "NUMBER_OF_FIELDS", 7);
	cmsIT8SetPropertyDbl (it8_lcms, "NUMBER_OF_SETS", it8->priv->array_rgb->len);
	cmsIT8SetDataFormat (it8_lcms, 0, "SAMPLE_ID");
	cmsIT8SetDataFormat (it8_lcms, 1, "RGB_R");
	cmsIT8SetDataFormat (it8_lcms, 2, "RGB_G");
	cmsIT8SetDataFormat (it8_lcms, 3, "RGB_B");
	cmsIT8SetDataFormat (it8_lcms, 4, "XYZ_X");
	cmsIT8SetDataFormat (it8_lcms, 5, "XYZ_Y");
	cmsIT8SetDataFormat (it8_lcms, 6, "XYZ_Z");

	/* write to the it8 file */
	for (i = 0; i < it8->priv->array_rgb->len; i++) {
		rgb_tmp = g_ptr_array_index (it8->priv->array_rgb, i);
		xyz_tmp = g_ptr_array_index (it8->priv->array_xyz, i);

		cmsIT8SetDataRowColDbl(it8_lcms, i, 0, i + 1);
		cmsIT8SetDataRowColDbl(it8_lcms, i, 1, rgb_tmp->R);
		cmsIT8SetDataRowColDbl(it8_lcms, i, 2, rgb_tmp->G);
		cmsIT8SetDataRowColDbl(it8_lcms, i, 3, rgb_tmp->B);
		if (it8->priv->normalized) {
			cmsIT8SetDataRowColDbl(it8_lcms, i, 4, xyz_tmp->X * normalize);
			cmsIT8SetDataRowColDbl(it8_lcms, i, 5, xyz_tmp->Y * normalize);
			cmsIT8SetDataRowColDbl(it8_lcms, i, 6, xyz_tmp->Z * normalize);
		} else {
			cmsIT8SetDataRowColDbl(it8_lcms, i, 4, xyz_tmp->X);
			cmsIT8SetDataRowColDbl(it8_lcms, i, 5, xyz_tmp->Y);
			cmsIT8SetDataRowColDbl(it8_lcms, i, 6, xyz_tmp->Z);
		}
	}
out:
	return ret;
}

/**
 * cd_it8_save_ccmx:
 **/
static gboolean
cd_it8_save_ccmx (CdIt8 *it8, cmsHANDLE it8_lcms, GError **error)
{
	gboolean ret = TRUE;

	cmsIT8SetSheetType (it8_lcms, "CCMX");
	cmsIT8SetPropertyStr (it8_lcms, "DESCRIPTOR",
			      "Device Correction Matrix");

	cmsIT8SetPropertyStr (it8_lcms, "COLOR_REP", "XYZ");
	cmsIT8SetPropertyDbl (it8_lcms, "NUMBER_OF_FIELDS", 3);
	cmsIT8SetPropertyDbl (it8_lcms, "NUMBER_OF_SETS", 3);
	cmsIT8SetDataFormat (it8_lcms, 0, "XYZ_X");
	cmsIT8SetDataFormat (it8_lcms, 1, "XYZ_Y");
	cmsIT8SetDataFormat (it8_lcms, 2, "XYZ_Z");

	/* save instrument */
	if (it8->priv->instrument != NULL) {
		cmsIT8SetPropertyStr (it8_lcms, "INSTRUMENT",
				      it8->priv->instrument);
	}

	/* just load the matrix */
	cmsIT8SetDataRowColDbl (it8_lcms, 0, 0, it8->priv->matrix.m00);
	cmsIT8SetDataRowColDbl (it8_lcms, 0, 1, it8->priv->matrix.m01);
	cmsIT8SetDataRowColDbl (it8_lcms, 0, 2, it8->priv->matrix.m02);
	cmsIT8SetDataRowColDbl (it8_lcms, 1, 0, it8->priv->matrix.m10);
	cmsIT8SetDataRowColDbl (it8_lcms, 1, 1, it8->priv->matrix.m11);
	cmsIT8SetDataRowColDbl (it8_lcms, 1, 2, it8->priv->matrix.m12);
	cmsIT8SetDataRowColDbl (it8_lcms, 2, 0, it8->priv->matrix.m20);
	cmsIT8SetDataRowColDbl (it8_lcms, 2, 1, it8->priv->matrix.m21);
	cmsIT8SetDataRowColDbl (it8_lcms, 2, 2, it8->priv->matrix.m22);

	return ret;
}

/**
 * cd_it8_save:
 * @it8: a #CdIt8 instance.
 * @file: a #GFile
 * @error: a #GError, or %NULL
 *
 * Saves a it8 file to disk
 *
 * Return value: %TRUE if it8 file was saved.
 *
 * Since: 0.1.20
 **/
gboolean
cd_it8_save (CdIt8 *it8, GFile *file, GError **error)
{
	cmsHANDLE it8_lcms = NULL;
	gboolean ret;
	gchar *data = NULL;
	gsize size = 0;

	g_return_val_if_fail (CD_IS_IT8 (it8), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	/* set common data */
	it8_lcms = cmsIT8Alloc (NULL);
	if (it8->priv->originator != NULL) {
		cmsIT8SetPropertyStr (it8_lcms, "ORIGINATOR",
				      it8->priv->originator);
	}
	if (it8->priv->reference != NULL) {
		cmsIT8SetPropertyStr (it8_lcms, "REFERENCE",
				      it8->priv->reference);
	}

	/* set ti1 and ti3 specific data */
	if (it8->priv->kind == CD_IT8_KIND_TI1 ||
	    it8->priv->kind == CD_IT8_KIND_TI3) {
		ret = cd_it8_save_ti1_ti3 (it8, it8_lcms, error);
		if (!ret)
			goto out;
	} else if (it8->priv->kind == CD_IT8_KIND_CCMX) {
		ret = cd_it8_save_ccmx (it8, it8_lcms, error);
		if (!ret)
			goto out;
	}

	/* write the file */
	ret = cmsIT8SaveToMem (it8_lcms, NULL, (cmsUInt32Number *) &size);
	g_assert (ret);
	data = g_malloc (size);
	ret = cmsIT8SaveToMem (it8_lcms, data, (cmsUInt32Number *) &size);
	g_assert (ret);

	/* workaround LCMS bug */
	if (data[size-1] != '\0')
		data[size-1] = '\0';

	/* save file */
	ret = g_file_replace_contents (file, data, size, NULL,
				       FALSE, G_FILE_CREATE_NONE,
				       NULL, NULL, error);
	if (!ret)
		goto out;
out:
	if (it8_lcms != NULL)
		cmsIT8Free (it8_lcms);
	g_free (data);
	return ret;
}

/**
 * cd_it8_set_normalized:
 * @it8: a #CdIt8 instance.
 * @normalized: If the data is normalized
 *
 * Sets if normalized data should be written to the .it8 file.
 *
 * Since: 0.1.20
 **/
void
cd_it8_set_normalized (CdIt8 *it8, gboolean normalized)
{
	g_return_if_fail (CD_IS_IT8 (it8));
	it8->priv->normalized = normalized;
}

/**
 * cd_it8_set_spectral:
 * @it8: a #CdIt8 instance.
 * @spectral: If the data is spectral
 *
 * Sets if spectral data should be written to the .it8 file.
 *
 * Since: 0.1.20
 **/
void
cd_it8_set_spectral (CdIt8 *it8, gboolean spectral)
{
	g_return_if_fail (CD_IS_IT8 (it8));
	it8->priv->spectral = spectral;
}

/**
 * cd_it8_set_originator:
 * @it8: a #CdIt8 instance.
 * @originator: the program name, e.g. "gcm-calibrate"
 *
 * Sets the program name that created the .it8 file
 *
 * Since: 0.1.20
 **/
void
cd_it8_set_originator (CdIt8 *it8, const gchar *originator)
{
	g_return_if_fail (CD_IS_IT8 (it8));

	g_free (it8->priv->originator);
	it8->priv->originator = g_strdup (originator);
}

/**
 * cd_it8_set_instrument:
 * @it8: a #CdIt8 instance.
 * @instrument: the instruemnt name, e.g. "huey"
 *
 * Sets the measuring instrument that created the .it8 file
 *
 * Since: 0.1.20
 **/
void
cd_it8_set_instrument (CdIt8 *it8, const gchar *instrument)
{
	g_return_if_fail (CD_IS_IT8 (it8));

	g_free (it8->priv->instrument);
	it8->priv->instrument = g_strdup (instrument);
}

/**
 * cd_it8_set_reference:
 * @it8: a #CdIt8 instance.
 * @reference: the instruemnt name, e.g. "colormunki"
 *
 * Sets the reference that as used to create the .it8 reference
 *
 * Since: 0.1.20
 **/
void
cd_it8_set_reference (CdIt8 *it8, const gchar *reference)
{
	g_return_if_fail (CD_IS_IT8 (it8));

	g_free (it8->priv->reference);
	it8->priv->reference = g_strdup (reference);
}

/**
 * cd_it8_add_data:
 * @it8: a #CdIt8 instance.
 * @rgb: a #CdColorRGB, or %NULL
 * @xyz: a #CdColorXYZ, or %NULL
 *
 * Adds a reading to this object. If either of @rgb or @xyz is NULL then
 * a black reading (0.0, 0.0, 0.0) is added instead.
 *
 * Since: 0.1.20
 **/
void
cd_it8_add_data (CdIt8 *it8, const CdColorRGB *rgb, const CdColorXYZ *xyz)
{
	CdColorRGB *rgb_tmp;
	CdColorXYZ *xyz_tmp;

	g_return_if_fail (CD_IS_IT8 (it8));

	/* add RGB */
	if (rgb != NULL) {
		rgb_tmp = cd_color_rgb_dup (rgb);
	} else {
		rgb_tmp = cd_color_rgb_new ();
		cd_color_set_rgb (rgb_tmp, 0.0f, 0.0f, 0.0f);
	}
	g_ptr_array_add (it8->priv->array_rgb, rgb_tmp);

	/* add XYZ */
	if (rgb != NULL) {
		xyz_tmp = cd_color_xyz_dup (xyz);
	} else {
		xyz_tmp = cd_color_xyz_new ();
		cd_color_set_xyz (xyz_tmp, 0.0f, 0.0f, 0.0f);
	}
	g_ptr_array_add (it8->priv->array_xyz, xyz_tmp);
}

/**
 * cd_it8_get_data_size:
 * @it8: a #CdIt8 instance.
 *
 * Gets the data size.
 *
 * Return value: The number of RGB-XYZ readings in this object.
 *
 * Since: 0.1.20
 **/
guint
cd_it8_get_data_size (CdIt8 *it8)
{
	g_return_val_if_fail (CD_IS_IT8 (it8), G_MAXUINT);
	return it8->priv->array_xyz->len;
}

/**
 * cd_it8_get_data_item:
 * @it8: a #CdIt8 instance.
 *
 * Gets a specific bit of data from this object.
 * The returned data are absolute readings and are not normalised.
 *
 * Return value: %TRUE if the index existed.
 *
 * Since: 0.1.20
 **/
gboolean
cd_it8_get_data_item (CdIt8 *it8, guint idx, CdColorRGB *rgb, CdColorXYZ *xyz)
{
	const CdColorRGB *rgb_tmp;
	const CdColorXYZ *xyz_tmp;

	g_return_val_if_fail (CD_IS_IT8 (it8), FALSE);

	if (idx > it8->priv->array_xyz->len)
		return FALSE;
	if (rgb != NULL) {
		rgb_tmp = g_ptr_array_index (it8->priv->array_rgb, idx);
		cd_color_copy_rgb (rgb_tmp, rgb);
	}
	if (xyz != NULL) {
		xyz_tmp = g_ptr_array_index (it8->priv->array_xyz, idx);
		cd_color_copy_xyz (xyz_tmp, xyz);
	}
	return TRUE;
}

/**********************************************************************/

/*
 * cd_it8_get_property:
 */
static void
cd_it8_get_property (GObject *object,
		     guint prop_id,
		     GValue *value,
		     GParamSpec *pspec)
{
	CdIt8 *it8 = CD_IT8 (object);

	switch (prop_id) {
	case PROP_KIND:
		g_value_set_uint (value, it8->priv->kind);
		break;
	case PROP_NORMALIZED:
		g_value_set_boolean (value, it8->priv->normalized);
		break;
	case PROP_ORIGINATOR:
		g_value_set_string (value, it8->priv->originator);
		break;
	case PROP_INSTRUMENT:
		g_value_set_string (value, it8->priv->instrument);
		break;
	case PROP_REFERENCE:
		g_value_set_string (value, it8->priv->reference);
		break;
	case PROP_SPECTRAL:
		g_value_set_boolean (value, it8->priv->spectral);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}


/**
 * cd_it8_set_property:
 **/
static void
cd_it8_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	CdIt8 *it8 = CD_IT8 (object);

	switch (prop_id) {
	case PROP_KIND:
		it8->priv->kind = g_value_get_uint (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/*
 * cd_it8_class_init:
 */
static void
cd_it8_class_init (CdIt8Class *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = cd_it8_get_property;
	object_class->set_property = cd_it8_set_property;
	object_class->finalize = cd_it8_finalize;

	/**
	 * CdIt8:kind:
	 *
	 * The kind of IT8 file.
	 *
	 * Since: 0.1.20
	 **/
	g_object_class_install_property (object_class,
					 PROP_KIND,
					 g_param_spec_uint ("kind",
							    NULL, NULL,
							    0, G_MAXUINT, 0,
							    G_PARAM_READWRITE));

	/**
	 * CdIt8:normalized:
	 *
	 * If the results file is normalized.
	 *
	 * Since: 0.1.20
	 **/
	g_object_class_install_property (object_class,
					 PROP_NORMALIZED,
					 g_param_spec_boolean ("normalized",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READABLE));

	/**
	 * CdIt8:originator:
	 *
	 * The framework that created the results, e.g. "cd-self-test"
	 *
	 * Since: 0.1.20
	 **/
	g_object_class_install_property (object_class,
					 PROP_ORIGINATOR,
					 g_param_spec_string ("originator",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));

	/**
	 * CdIt8:instrument:
	 *
	 * The instrument that created the results, e.g. "huey"
	 *
	 * Since: 0.1.20
	 **/
	g_object_class_install_property (object_class,
					 PROP_INSTRUMENT,
					 g_param_spec_string ("instrument",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));

	/**
	 * CdIt8:reference:
	 *
	 * The reference that created the results, e.g. "colormunki"
	 *
	 * Since: 0.1.20
	 **/
	g_object_class_install_property (object_class,
					 PROP_REFERENCE,
					 g_param_spec_string ("reference",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));

	/**
	 * CdIt8:spectral:
	 *
	 * If the results file is spectral.
	 *
	 * Since: 0.1.20
	 **/
	g_object_class_install_property (object_class,
					 PROP_SPECTRAL,
					 g_param_spec_boolean ("spectral",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READABLE));

	g_type_class_add_private (klass, sizeof (CdIt8Private));
}

/*
 * cd_it8_init:
 */
static void
cd_it8_init (CdIt8 *it8)
{
	it8->priv = CD_IT8_GET_PRIVATE (it8);

	it8->priv->array_rgb = g_ptr_array_new_with_free_func (cd_color_rgb_free);
	it8->priv->array_xyz = g_ptr_array_new_with_free_func (cd_color_xyz_free);

	/* ensure the remote errors are registered */
	cd_it8_error_quark ();
}

/*
 * cd_it8_finalize:
 */
static void
cd_it8_finalize (GObject *object)
{
	CdIt8 *it8 = CD_IT8 (object);

	g_return_if_fail (CD_IS_IT8 (object));

	g_ptr_array_unref (it8->priv->array_rgb);
	g_ptr_array_unref (it8->priv->array_xyz);
	g_free (it8->priv->originator);
	g_free (it8->priv->instrument);
	g_free (it8->priv->reference);

	G_OBJECT_CLASS (cd_it8_parent_class)->finalize (object);
}

/**
 * cd_it8_new:
 *
 * Creates a new #CdIt8 object.
 *
 * Return value: a new CdIt8 object.
 *
 * Since: 0.1.20
 **/
CdIt8 *
cd_it8_new (void)
{
	CdIt8 *it8;
	it8 = g_object_new (CD_TYPE_IT8, NULL);
	return CD_IT8 (it8);
}

/**
 * cd_it8_new_with_kind:
 * @kind: a #CdIt8Kind, e.g %CD_IT8_KIND_TI3.
 *
 * Creates a new #CdIt8 object.
 *
 * Return value: a new CdIt8 object.
 *
 * Since: 0.1.20
 **/
CdIt8 *
cd_it8_new_with_kind (CdIt8Kind kind)
{
	CdIt8 *it8;
	it8 = g_object_new (CD_TYPE_IT8,
			    "kind", kind,
			    NULL);
	return CD_IT8 (it8);
}
