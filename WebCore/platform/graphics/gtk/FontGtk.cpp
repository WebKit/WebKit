/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (c) 2007 Hiroyuki Ikezoe
 * Copyright (c) 2007 Kouhei Sutou
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Xan Lopez <xan@gnome.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Font.h"

#include "GraphicsContext.h"
#include "NotImplemented.h"
#include "SimpleFontData.h"

#include <cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

namespace WebCore {

#define IS_HIGH_SURROGATE(u)  ((UChar)(u) >= (UChar)0xd800 && (UChar)(u) <= (UChar)0xdbff)
#define IS_LOW_SURROGATE(u)  ((UChar)(u) >= (UChar)0xdc00 && (UChar)(u) <= (UChar)0xdfff)

static void utf16_to_utf8(const UChar* aText, gint aLength, char* &text, gint &length)
{
  gboolean need_copy = FALSE;
  int i;

  for (i = 0; i < aLength; i++) {
    if (!aText[i] || IS_LOW_SURROGATE(aText[i])) {
      need_copy = TRUE;
      break;
    }
    else if (IS_HIGH_SURROGATE(aText[i])) {
      if (i < aLength - 1 && IS_LOW_SURROGATE(aText[i+1]))
        i++;
      else {
        need_copy = TRUE;
        break;
      }
    }
  }

  if (need_copy) {

    /* Pango doesn't correctly handle nuls.  We convert them to 0xff. */
    /* Also "validate" UTF-16 text to make sure conversion doesn't fail. */

    UChar* p = (UChar*)g_memdup(aText, aLength * sizeof(aText[0]));

    /* don't need to reset i */
    for (i = 0; i < aLength; i++) {
      if (!p[i] || IS_LOW_SURROGATE(p[i]))
        p[i] = 0xFFFD;
      else if (IS_HIGH_SURROGATE(p[i])) {
        if (i < aLength - 1 && IS_LOW_SURROGATE(aText[i+1]))
          i++;
        else
          p[i] = 0xFFFD;
      }
    }

    aText = p;
  }

  glong items_written;
  text = g_utf16_to_utf8(aText, aLength, NULL, &items_written, NULL);
  length = items_written;

  if (need_copy)
    g_free((gpointer)aText);

}

static gchar* convertUniCharToUTF8(const UChar* characters, gint length, int from, int to)
{
    gchar* utf8 = 0;
    gint new_length = 0;
    utf16_to_utf8(characters, length, utf8, new_length);
    if (!utf8)
        return NULL;

    if (from > 0) {
        // discard the first 'from' characters
        // FIXME: we should do this before the conversion probably
        gchar* str_left = g_utf8_offset_to_pointer(utf8, from);
        gchar* tmp = g_strdup(str_left);
        g_free(utf8);
        utf8 = tmp;
    }

    gchar* pos = utf8;
    gint len = strlen(pos);
    GString* ret = g_string_new_len(NULL, len);

    // replace line break by space
    while (len > 0) {
        gint index, start;
        pango_find_paragraph_boundary(pos, len, &index, &start);
        g_string_append_len(ret, pos, index);
        if (index == start)
            break;
        g_string_append_c(ret, ' ');
        pos += start;
        len -= start;
    }
    return g_string_free(ret, FALSE);
}

static void setPangoAttributes(const Font* font, const TextRun& run, PangoLayout* layout)
{
    PangoAttrList* list = pango_attr_list_new();
    PangoAttribute* attr;

    attr = pango_attr_size_new_absolute((int)(font->size() * PANGO_SCALE));
    attr->end_index = G_MAXUINT;
    pango_attr_list_insert_before(list, attr);

    if (!run.spacingDisabled()) {
        attr = pango_attr_letter_spacing_new(font->letterSpacing() * PANGO_SCALE);
        attr->end_index = G_MAXUINT;
        pango_attr_list_insert_before(list, attr);
    }

    // Pango does not yet support synthesising small caps
    // See http://bugs.webkit.org/show_bug.cgi?id=15610

    pango_layout_set_attributes(layout, list);
    pango_attr_list_unref(list);

    pango_layout_set_auto_dir(layout, FALSE);

    PangoContext* pangoContext = pango_layout_get_context(layout);
    PangoDirection direction = run.rtl() ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR;
    pango_context_set_base_dir(pangoContext, direction);
}

void Font::drawGlyphs(GraphicsContext* graphicsContext, const SimpleFontData* font, const GlyphBuffer& glyphBuffer,
                      int from, int numGlyphs, const FloatPoint& point) const
{
    cairo_t* cr = graphicsContext->platformContext();
    cairo_save(cr);

    // Set the text color to use for drawing.
    float red, green, blue, alpha;
    Color penColor = graphicsContext->fillColor();
    penColor.getRGBA(red, green, blue, alpha);
    cairo_set_source_rgba(cr, red, green, blue, alpha);

    font->setFont(cr);

    GlyphBufferGlyph* glyphs = (GlyphBufferGlyph*)glyphBuffer.glyphs(from);

    float offset = point.x();

    for (int i = 0; i < numGlyphs; i++) {
        glyphs[i].x = offset;
        glyphs[i].y = point.y();
        offset += glyphBuffer.advanceAt(from + i);
    }
    cairo_show_glyphs(cr, glyphs, numGlyphs);

    cairo_restore(cr);
}

void Font::drawComplexText(GraphicsContext* context, const TextRun& run, const FloatPoint& point, int from, int to) const
{
    cairo_t* cr = context->platformContext();
    cairo_save(cr);

    PangoLayout* layout = pango_cairo_create_layout(cr);

    gchar* utf8 = convertUniCharToUTF8(run.characters(), run.length(), from, to);
    pango_layout_set_text(layout, utf8, -1);
    g_free(utf8);

    setPangoAttributes(this, run, layout);

    // Set the text color to use for drawing.
    float red, green, blue, alpha;
    Color penColor = context->fillColor();
    penColor.getRGBA(red, green, blue, alpha);
    cairo_set_source_rgba(cr, red, green, blue, alpha);

    // Our layouts are single line
    cairo_move_to(cr, point.x(), point.y());
    PangoLayoutLine* layoutLine = pango_layout_get_line_readonly(layout, 0);
    pango_cairo_show_layout_line(cr, layoutLine);

    g_object_unref(layout);
    cairo_restore(cr);
}

// FIXME: we should create the layout with our actual context, but it seems
// we can't access it from here
static PangoLayout* getDefaultPangoLayout(const TextRun& run)
{
    PangoFontMap* map = pango_cairo_font_map_get_default();
    PangoContext* pangoContext = pango_cairo_font_map_create_context(PANGO_CAIRO_FONT_MAP(map));
    PangoLayout* layout = pango_layout_new(pangoContext);
    g_object_unref(pangoContext);

    return layout;
}

float Font::floatWidthForComplexText(const TextRun& run) const
{
    if (run.length() == 0)
        return 0.0f;

    PangoLayout* layout = getDefaultPangoLayout(run);
    setPangoAttributes(this, run, layout);

    gchar* utf8 = convertUniCharToUTF8(run.characters(), run.length(), 0, run.length());
    pango_layout_set_text(layout, utf8, -1);
    g_free(utf8);

    int layoutWidth;
    pango_layout_get_size(layout, &layoutWidth, 0);
    float width = (float)layoutWidth / (double)PANGO_SCALE;
    g_object_unref(layout);

    return width;
}

int Font::offsetForPositionForComplexText(const TextRun& run, int x, bool includePartialGlyphs) const
{
    PangoLayout* layout = getDefaultPangoLayout(run);
    setPangoAttributes(this, run, layout);

    gchar* utf8 = convertUniCharToUTF8(run.characters(), run.length(), 0, run.length());
    pango_layout_set_text(layout, utf8, -1);

    int index, trailing;
    pango_layout_xy_to_index(layout, x * PANGO_SCALE, 1, &index, &trailing);
    glong offset = g_utf8_pointer_to_offset(utf8, utf8 + index);
    g_free(utf8);
    g_object_unref(layout);

    return offset;
}

FloatRect Font::selectionRectForComplexText(const TextRun& run, const IntPoint& point, int h, int from, int to) const
{
    notImplemented();
    return FloatRect();
}

}
