/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Kristian Høgsberg <krh@redhat.com>
 */

#include "cairoint.h"
#include "cairo-font-subset-private.h"

/* XXX: Eventually, we need to handle other font backends */
#include "cairo-ft-private.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_TRUETYPE_TAGS_H
#include FT_TRUETYPE_TABLES_H

typedef struct ft_subset_glyph ft_subset_glyph_t;
struct ft_subset_glyph {
    int parent_index;
    unsigned long location;
};

struct cairo_font_subset_backend {
    int			(*use_glyph)	(void *abstract_font,
					 int glyph);
    cairo_status_t	(*generate)	(void *abstract_font,
					 const char **data,
					 unsigned long *length);
    void		(*destroy)	(void *abstract_font);
};

typedef struct cairo_pdf_ft_font cairo_pdf_ft_font_t;
struct cairo_pdf_ft_font {
    cairo_font_subset_t base;
    ft_subset_glyph_t *glyphs;
    FT_Face face;
    int checksum_index;
    cairo_array_t output;
    int *parent_to_subset;
    cairo_status_t status;
};

static int
cairo_pdf_ft_font_use_glyph (void *abstract_font, int glyph);


#define ARRAY_LENGTH(a) ( (sizeof (a)) / (sizeof ((a)[0])) )

#define SFNT_VERSION			0x00010000

#ifdef WORDS_BIGENDIAN

#define cpu_to_be16(v) (v)
#define be16_to_cpu(v) (v)
#define cpu_to_be32(v) (v)
#define be32_to_cpu(v) (v)

#else

static unsigned short
cpu_to_be16(unsigned short v)
{
    return (v << 8) | (v >> 8);
}

static unsigned short
be16_to_cpu(unsigned short v)
{
    return cpu_to_be16 (v);
}

static unsigned long
cpu_to_be32(unsigned long v)
{
    return (cpu_to_be16 (v) << 16) | cpu_to_be16 (v >> 16);
}

static unsigned long
be32_to_cpu(unsigned long v)
{
    return cpu_to_be32 (v);
}

#endif

static cairo_font_subset_backend_t cairo_pdf_ft_font_backend;

int
_cairo_font_subset_use_glyph (cairo_font_subset_t *font, int glyph)
{
    return font->backend->use_glyph (font, glyph);
}

cairo_status_t
_cairo_font_subset_generate (cairo_font_subset_t *font,
			 const char **data, unsigned long *length)
{
    return font->backend->generate (font, data, length);
}

void
_cairo_font_subset_destroy (cairo_font_subset_t *font)
{
    font->backend->destroy (font);
}

cairo_font_subset_t *
_cairo_font_subset_create (cairo_unscaled_font_t *unscaled_font)
{
    cairo_ft_unscaled_font_t *ft_unscaled_font;
    FT_Face face;
    cairo_pdf_ft_font_t *font;
    unsigned long size;
    int i, j;

    /* XXX: Need to fix this to work with a general cairo_unscaled_font_t. */
    if (! _cairo_unscaled_font_is_ft (unscaled_font))
	return NULL;

    ft_unscaled_font = (cairo_ft_unscaled_font_t *) unscaled_font;

    face = _cairo_ft_unscaled_font_lock_face (ft_unscaled_font);

    /* We currently only support freetype truetype fonts. */
    size = 0;
    if (!FT_IS_SFNT (face) ||
	FT_Load_Sfnt_Table (face, TTAG_glyf, 0, NULL, &size) != 0)
	return NULL;

    font = malloc (sizeof (cairo_pdf_ft_font_t));
    if (font == NULL)
	return NULL;

    font->base.unscaled_font = _cairo_unscaled_font_reference (unscaled_font);
    font->base.backend = &cairo_pdf_ft_font_backend;

    _cairo_array_init (&font->output, sizeof (char));
    if (_cairo_array_grow_by (&font->output, 4096) != CAIRO_STATUS_SUCCESS)
	goto fail1;

    font->glyphs = calloc (face->num_glyphs + 1, sizeof (ft_subset_glyph_t));
    if (font->glyphs == NULL)
	goto fail2;

    font->parent_to_subset = calloc (face->num_glyphs, sizeof (int));
    if (font->parent_to_subset == NULL)
	goto fail3;

    font->base.num_glyphs = 1;
    font->base.x_min = face->bbox.xMin;
    font->base.y_min = face->bbox.yMin;
    font->base.x_max = face->bbox.xMax;
    font->base.y_max = face->bbox.yMax;
    font->base.ascent = face->ascender;
    font->base.descent = face->descender;
    font->base.base_font = strdup (face->family_name);
    if (font->base.base_font == NULL)
	goto fail4;

    for (i = 0, j = 0; font->base.base_font[j]; j++) {
	if (font->base.base_font[j] == ' ')
	    continue;
	font->base.base_font[i++] = font->base.base_font[j];
    }
    font->base.base_font[i] = '\0';

    font->base.widths = calloc (face->num_glyphs, sizeof (int));
    if (font->base.widths == NULL)
	goto fail5;

    _cairo_ft_unscaled_font_unlock_face (ft_unscaled_font);

    font->status = CAIRO_STATUS_SUCCESS;

    return &font->base;

 fail5:
    free (font->base.base_font);
 fail4:
    free (font->parent_to_subset);
 fail3:
    free (font->glyphs);
 fail2:
    _cairo_array_fini (&font->output);
 fail1:
    free (font);
    return NULL;
}

static void
cairo_pdf_ft_font_destroy (void *abstract_font)
{
    cairo_pdf_ft_font_t *font = abstract_font;

    _cairo_unscaled_font_destroy (font->base.unscaled_font);
    free (font->base.base_font);
    free (font->parent_to_subset);
    free (font->glyphs);
    _cairo_array_fini (&font->output);
    free (font);
}

static cairo_status_t
cairo_pdf_ft_font_allocate_write_buffer (cairo_pdf_ft_font_t	 *font,
					 size_t			  length,
					 unsigned char		**buffer)
{
    cairo_status_t status;

    status = _cairo_array_allocate (&font->output, length, (void **) buffer);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_pdf_ft_font_write (cairo_pdf_ft_font_t *font,
			 const void *data, size_t length)
{
    cairo_status_t status;

    status = _cairo_array_append_multiple (&font->output, data, length);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

static void
cairo_pdf_ft_font_write_be16 (cairo_pdf_ft_font_t *font,
			      unsigned short value)
{
    unsigned short be16_value;

    be16_value = cpu_to_be16 (value);
    cairo_pdf_ft_font_write (font, &be16_value, sizeof be16_value);
}

static void
cairo_pdf_ft_font_write_be32 (cairo_pdf_ft_font_t *font, unsigned long value)
{
    unsigned long be32_value;

    be32_value = cpu_to_be32 (value);
    cairo_pdf_ft_font_write (font, &be32_value, sizeof be32_value);
}

static unsigned long
cairo_pdf_ft_font_align_output (cairo_pdf_ft_font_t *font)
{
    int length, aligned, pad;
    unsigned char *ignored;

    length = _cairo_array_num_elements (&font->output);
    aligned = (length + 3) & ~3;
    pad = aligned - length;

    if (pad)
	cairo_pdf_ft_font_allocate_write_buffer (font, pad, &ignored);

    return aligned;
}

static int
cairo_pdf_ft_font_write_cmap_table (cairo_pdf_ft_font_t *font, unsigned long tag)
{
    int i;

    cairo_pdf_ft_font_write_be16 (font, 0);
    cairo_pdf_ft_font_write_be16 (font, 1);

    cairo_pdf_ft_font_write_be16 (font, 1);
    cairo_pdf_ft_font_write_be16 (font, 0);
    cairo_pdf_ft_font_write_be32 (font, 12);

    /* Output a format 6 encoding table. */

    cairo_pdf_ft_font_write_be16 (font, 6);
    cairo_pdf_ft_font_write_be16 (font, 10 + 2 * (font->base.num_glyphs - 1));
    cairo_pdf_ft_font_write_be16 (font, 0);
    cairo_pdf_ft_font_write_be16 (font, 1); /* First glyph */
    cairo_pdf_ft_font_write_be16 (font, font->base.num_glyphs - 1);
    for (i = 1; i < font->base.num_glyphs; i++)
	cairo_pdf_ft_font_write_be16 (font, i);

    return font->status;
}

static int
cairo_pdf_ft_font_write_generic_table (cairo_pdf_ft_font_t *font,
				       unsigned long tag)
{
    cairo_status_t status;
    unsigned char *buffer;
    unsigned long size;

    size = 0;
    FT_Load_Sfnt_Table (font->face, tag, 0, NULL, &size);
    status = cairo_pdf_ft_font_allocate_write_buffer (font, size, &buffer);
    /* XXX: Need to check status here. */
    FT_Load_Sfnt_Table (font->face, tag, 0, buffer, &size);
    
    return 0;
}


typedef struct composite_glyph composite_glyph_t;
struct composite_glyph {
    unsigned short flags;
    unsigned short index;
    unsigned short args[7]; /* 1 to 7 arguments depending on value of flags */
};

typedef struct glyph_data glyph_data_t;
struct glyph_data {
    short             num_contours;
    char              data[8];
    composite_glyph_t glyph;
};

/* composite_glyph_t flags */
#define ARG_1_AND_2_ARE_WORDS     0x0001
#define WE_HAVE_A_SCALE           0x0008
#define MORE_COMPONENTS           0x0020
#define WE_HAVE_AN_X_AND_Y_SCALE  0x0040
#define WE_HAVE_A_TWO_BY_TWO      0x0080

static void
cairo_pdf_ft_font_remap_composite_glyph (cairo_pdf_ft_font_t *font,
					 unsigned char *buffer)
{
    glyph_data_t *glyph_data;
    composite_glyph_t *composite_glyph;
    int num_args;
    int has_more_components;
    unsigned short flags;
    unsigned short index;

    glyph_data = (glyph_data_t *) buffer;
    if ((short)be16_to_cpu (glyph_data->num_contours) >= 0)
        return;
    
    composite_glyph = &glyph_data->glyph;
    do {
        flags = be16_to_cpu (composite_glyph->flags);
        has_more_components = flags & MORE_COMPONENTS;
        index = cairo_pdf_ft_font_use_glyph (font, be16_to_cpu (composite_glyph->index));
        composite_glyph->index = cpu_to_be16 (index);
        num_args = 1;
        if (flags & ARG_1_AND_2_ARE_WORDS)
            num_args += 1;
        if (flags & WE_HAVE_A_SCALE)
            num_args += 1;
        else if (flags & WE_HAVE_AN_X_AND_Y_SCALE)
            num_args += 2;
        else if (flags & WE_HAVE_A_TWO_BY_TWO)
            num_args += 3;
        composite_glyph = (composite_glyph_t *) &(composite_glyph->args[num_args]);
    } while (has_more_components);
}

static int
cairo_pdf_ft_font_write_glyf_table (cairo_pdf_ft_font_t *font,
				    unsigned long tag)
{
    cairo_status_t status;
    unsigned long start_offset, index, size;
    TT_Header *header;
    unsigned long begin, end;
    unsigned char *buffer;
    int i;
    union {
	unsigned char *bytes;
	unsigned short *short_offsets;
	unsigned long *long_offsets;
    } u;

    header = FT_Get_Sfnt_Table (font->face, ft_sfnt_head);
    if (header->Index_To_Loc_Format == 0)
	size = sizeof (short) * (font->face->num_glyphs + 1);
    else
	size = sizeof (long) * (font->face->num_glyphs + 1);

    u.bytes = malloc (size);
    if (u.bytes == NULL) {
	font->status = CAIRO_STATUS_NO_MEMORY;
	return font->status;
    }
    FT_Load_Sfnt_Table (font->face, TTAG_loca, 0, u.bytes, &size);

    start_offset = _cairo_array_num_elements (&font->output);
    for (i = 0; i < font->base.num_glyphs; i++) {
	index = font->glyphs[i].parent_index;
	if (header->Index_To_Loc_Format == 0) {
	    begin = be16_to_cpu (u.short_offsets[index]) * 2;
	    end = be16_to_cpu (u.short_offsets[index + 1]) * 2;
	}
	else {
	    begin = be32_to_cpu (u.long_offsets[index]);
	    end = be32_to_cpu (u.long_offsets[index + 1]);
	}

	size = end - begin;

	font->glyphs[i].location =
	    cairo_pdf_ft_font_align_output (font) - start_offset;
	status = cairo_pdf_ft_font_allocate_write_buffer (font, size, &buffer);
	if (status)
	    break;
        if (size != 0) {
            FT_Load_Sfnt_Table (font->face, TTAG_glyf, begin, buffer, &size);
            cairo_pdf_ft_font_remap_composite_glyph (font, buffer);
        }
    }

    font->glyphs[i].location =
	cairo_pdf_ft_font_align_output (font) - start_offset;

    free (u.bytes);

    return font->status;
}

static int
cairo_pdf_ft_font_write_head_table (cairo_pdf_ft_font_t *font,
				    unsigned long tag)
{
    TT_Header *head;

    head = FT_Get_Sfnt_Table (font->face, ft_sfnt_head);

    cairo_pdf_ft_font_write_be32 (font, head->Table_Version);
    cairo_pdf_ft_font_write_be32 (font, head->Font_Revision);

    font->checksum_index = _cairo_array_num_elements (&font->output);
    cairo_pdf_ft_font_write_be32 (font, 0);
    cairo_pdf_ft_font_write_be32 (font, head->Magic_Number);

    cairo_pdf_ft_font_write_be16 (font, head->Flags);
    cairo_pdf_ft_font_write_be16 (font, head->Units_Per_EM);

    cairo_pdf_ft_font_write_be32 (font, head->Created[0]);
    cairo_pdf_ft_font_write_be32 (font, head->Created[1]);
    cairo_pdf_ft_font_write_be32 (font, head->Modified[0]);
    cairo_pdf_ft_font_write_be32 (font, head->Modified[1]);

    cairo_pdf_ft_font_write_be16 (font, head->xMin);
    cairo_pdf_ft_font_write_be16 (font, head->yMin);
    cairo_pdf_ft_font_write_be16 (font, head->xMax);
    cairo_pdf_ft_font_write_be16 (font, head->yMax);

    cairo_pdf_ft_font_write_be16 (font, head->Mac_Style);
    cairo_pdf_ft_font_write_be16 (font, head->Lowest_Rec_PPEM);

    cairo_pdf_ft_font_write_be16 (font, head->Font_Direction);
    cairo_pdf_ft_font_write_be16 (font, head->Index_To_Loc_Format);
    cairo_pdf_ft_font_write_be16 (font, head->Glyph_Data_Format);

    return font->status;
}

static int cairo_pdf_ft_font_write_hhea_table (cairo_pdf_ft_font_t *font, unsigned long tag)
{
    TT_HoriHeader *hhea;

    hhea = FT_Get_Sfnt_Table (font->face, ft_sfnt_hhea);

    cairo_pdf_ft_font_write_be32 (font, hhea->Version);
    cairo_pdf_ft_font_write_be16 (font, hhea->Ascender);
    cairo_pdf_ft_font_write_be16 (font, hhea->Descender);
    cairo_pdf_ft_font_write_be16 (font, hhea->Line_Gap);

    cairo_pdf_ft_font_write_be16 (font, hhea->advance_Width_Max);

    cairo_pdf_ft_font_write_be16 (font, hhea->min_Left_Side_Bearing);
    cairo_pdf_ft_font_write_be16 (font, hhea->min_Right_Side_Bearing);
    cairo_pdf_ft_font_write_be16 (font, hhea->xMax_Extent);
    cairo_pdf_ft_font_write_be16 (font, hhea->caret_Slope_Rise);
    cairo_pdf_ft_font_write_be16 (font, hhea->caret_Slope_Run);
    cairo_pdf_ft_font_write_be16 (font, hhea->caret_Offset);

    cairo_pdf_ft_font_write_be16 (font, 0);
    cairo_pdf_ft_font_write_be16 (font, 0);
    cairo_pdf_ft_font_write_be16 (font, 0);
    cairo_pdf_ft_font_write_be16 (font, 0);

    cairo_pdf_ft_font_write_be16 (font, hhea->metric_Data_Format);
    cairo_pdf_ft_font_write_be16 (font, font->base.num_glyphs);

    return font->status;
}

static int
cairo_pdf_ft_font_write_hmtx_table (cairo_pdf_ft_font_t *font,
				    unsigned long tag)
{
    cairo_status_t status;
    unsigned long entry_size;
    short *p;
    int i;

    for (i = 0; i < font->base.num_glyphs; i++) {
	entry_size = 2 * sizeof (short);
	status = cairo_pdf_ft_font_allocate_write_buffer (font, entry_size,
							  (unsigned char **) &p);
	/* XXX: Need to check status here. */
	FT_Load_Sfnt_Table (font->face, TTAG_hmtx, 
			    font->glyphs[i].parent_index * entry_size,
			    (FT_Byte *) p, &entry_size);
	font->base.widths[i] = be16_to_cpu (p[0]);
    }

    return font->status;
}

static int
cairo_pdf_ft_font_write_loca_table (cairo_pdf_ft_font_t *font,
				    unsigned long tag)
{
    int i;
    TT_Header *header;

    header = FT_Get_Sfnt_Table (font->face, ft_sfnt_head);

    if (header->Index_To_Loc_Format == 0) {
	for (i = 0; i < font->base.num_glyphs + 1; i++)
	    cairo_pdf_ft_font_write_be16 (font, font->glyphs[i].location / 2);
    }
    else {
	for (i = 0; i < font->base.num_glyphs + 1; i++)
	    cairo_pdf_ft_font_write_be32 (font, font->glyphs[i].location);
    }

    return font->status;
}

static int
cairo_pdf_ft_font_write_maxp_table (cairo_pdf_ft_font_t *font,
				    unsigned long tag)
{
    TT_MaxProfile *maxp;

    maxp = FT_Get_Sfnt_Table (font->face, ft_sfnt_maxp);
    
    cairo_pdf_ft_font_write_be32 (font, maxp->version);
    cairo_pdf_ft_font_write_be16 (font, font->base.num_glyphs);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxPoints);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxContours);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxCompositePoints);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxCompositeContours);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxZones);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxTwilightPoints);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxStorage);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxFunctionDefs);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxInstructionDefs);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxStackElements);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxSizeOfInstructions);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxComponentElements);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxComponentDepth);

    return font->status;
}

typedef struct table table_t;
struct table {
    unsigned long tag;
    int (*write) (cairo_pdf_ft_font_t *font, unsigned long tag);
};

static const table_t truetype_tables[] = {
    /* As we write out the glyf table we remap composite glyphs.
     * Remapping composite glyphs will reference the sub glyphs the
     * composite glyph is made up of.  That needs to be done first so
     * we have all the glyphs in the subset before going further. */
    { TTAG_glyf, cairo_pdf_ft_font_write_glyf_table },
    { TTAG_cmap, cairo_pdf_ft_font_write_cmap_table },
    { TTAG_cvt,  cairo_pdf_ft_font_write_generic_table },
    { TTAG_fpgm, cairo_pdf_ft_font_write_generic_table },
    { TTAG_head, cairo_pdf_ft_font_write_head_table },
    { TTAG_hhea, cairo_pdf_ft_font_write_hhea_table },
    { TTAG_hmtx, cairo_pdf_ft_font_write_hmtx_table },
    { TTAG_loca, cairo_pdf_ft_font_write_loca_table },
    { TTAG_maxp, cairo_pdf_ft_font_write_maxp_table },
    { TTAG_name, cairo_pdf_ft_font_write_generic_table },
    { TTAG_prep, cairo_pdf_ft_font_write_generic_table },
};

static cairo_status_t
cairo_pdf_ft_font_write_offset_table (cairo_pdf_ft_font_t *font)
{
    cairo_status_t status;
    unsigned char *table_buffer;
    size_t table_buffer_length;
    unsigned short search_range, entry_selector, range_shift;
    int num_tables;

    num_tables = ARRAY_LENGTH (truetype_tables);
    search_range = 1;
    entry_selector = 0;
    while (search_range * 2 <= num_tables) {
	search_range *= 2;
	entry_selector++;
    }
    search_range *= 16;
    range_shift = num_tables * 16 - search_range;

    cairo_pdf_ft_font_write_be32 (font, SFNT_VERSION);
    cairo_pdf_ft_font_write_be16 (font, num_tables);
    cairo_pdf_ft_font_write_be16 (font, search_range);
    cairo_pdf_ft_font_write_be16 (font, entry_selector);
    cairo_pdf_ft_font_write_be16 (font, range_shift);

    /* XXX: Why are we allocating a table here and then ignoring the
     * returned buffer? This should result in garbage in the output
     * file, correct? Is this just unfinished code? -cworth. */
    table_buffer_length = ARRAY_LENGTH (truetype_tables) * 16;
    status = cairo_pdf_ft_font_allocate_write_buffer (font, table_buffer_length,
						      &table_buffer);
    if (status)
	return status;

    return font->status;
}    

static unsigned long
cairo_pdf_ft_font_calculate_checksum (cairo_pdf_ft_font_t *font,
			   unsigned long start, unsigned long end)
{
    unsigned long *padded_end;
    unsigned long *p;
    unsigned long checksum;
    char *data;

    checksum = 0; 
    data = _cairo_array_index (&font->output, 0);
    p = (unsigned long *) (data + start);
    padded_end = (unsigned long *) (data + ((end + 3) & ~3));
    while (p < padded_end)
	checksum += *p++;

    return checksum;
}

static void
cairo_pdf_ft_font_update_entry (cairo_pdf_ft_font_t *font, int index, unsigned long tag,
			unsigned long start, unsigned long end)
{
    unsigned long *entry;

    entry = _cairo_array_index (&font->output, 12 + 16 * index);
    entry[0] = cpu_to_be32 (tag);
    entry[1] = cpu_to_be32 (cairo_pdf_ft_font_calculate_checksum (font, start, end));
    entry[2] = cpu_to_be32 (start);
    entry[3] = cpu_to_be32 (end - start);
}

static cairo_status_t
cairo_pdf_ft_font_generate (void *abstract_font,
			    const char **data, unsigned long *length)
{
    cairo_ft_unscaled_font_t *ft_unscaled_font;
    cairo_pdf_ft_font_t *font = abstract_font;
    unsigned long start, end, next, checksum, *checksum_location;
    int i;

    /* XXX: It would be cleaner to do something besides this cast
     * here. Perhaps cairo_pdf_ft_font_t should just have the
     * cairo_ft_unscaled_font_t rather than having the generic
     * cairo_unscaled_font_t in the base class? */
    ft_unscaled_font = (cairo_ft_unscaled_font_t *) font->base.unscaled_font;

    font->face = _cairo_ft_unscaled_font_lock_face (ft_unscaled_font);

    if (cairo_pdf_ft_font_write_offset_table (font))
	goto fail;

    start = cairo_pdf_ft_font_align_output (font);
    end = start;

    end = 0;
    for (i = 0; i < ARRAY_LENGTH (truetype_tables); i++) {
	if (truetype_tables[i].write (font, truetype_tables[i].tag))
	    goto fail;

	end = _cairo_array_num_elements (&font->output);
	next = cairo_pdf_ft_font_align_output (font);
	cairo_pdf_ft_font_update_entry (font, i, truetype_tables[i].tag,
					start, end);
	start = next;
    }

    checksum =
	0xb1b0afba - cairo_pdf_ft_font_calculate_checksum (font, 0, end);
    checksum_location = _cairo_array_index (&font->output, font->checksum_index);
    *checksum_location = cpu_to_be32 (checksum);

    *data = _cairo_array_index (&font->output, 0);
    *length = _cairo_array_num_elements (&font->output);

 fail:
    _cairo_ft_unscaled_font_unlock_face (ft_unscaled_font);
    font->face = NULL;

    return font->status;
}

static int
cairo_pdf_ft_font_use_glyph (void *abstract_font, int glyph)
{
    cairo_pdf_ft_font_t *font = abstract_font;

    if (font->parent_to_subset[glyph] == 0) {
	font->parent_to_subset[glyph] = font->base.num_glyphs;
	font->glyphs[font->base.num_glyphs].parent_index = glyph;
	font->base.num_glyphs++;
    }

    return font->parent_to_subset[glyph];
}

static cairo_font_subset_backend_t cairo_pdf_ft_font_backend = {
    cairo_pdf_ft_font_use_glyph,
    cairo_pdf_ft_font_generate,
    cairo_pdf_ft_font_destroy
};
