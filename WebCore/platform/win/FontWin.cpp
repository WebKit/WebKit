/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "FontDataSet.h"
#include "GraphicsContext.h"
#include <cairo.h>
#include <cairo-win32.h>


namespace WebCore {

class FontData
{
public:
    FontData(HFONT font, const FontDescription& fontDescription) {
        m_font = font;
        m_fontFace = cairo_win32_font_face_create_for_hfont(font);
        cairo_matrix_t sizeMatrix, ctm;
        cairo_matrix_init_identity(&ctm);
        cairo_matrix_init_scale(&sizeMatrix, fontDescription.computedPixelSize(), fontDescription.computedPixelSize());
        cairo_font_options_t* fontOptions = cairo_font_options_create();
        m_scaledFont = cairo_scaled_font_create(m_fontFace, &sizeMatrix, &ctm, fontOptions);
        cairo_font_options_destroy(fontOptions);
    }

    ~FontData() {
        cairo_font_face_destroy(m_fontFace);
        cairo_scaled_font_destroy(m_scaledFont);
        DeleteObject(m_font);
    }

    HFONT hfont() const { return m_font; }
    cairo_scaled_font_t* scaledFont() const { return m_scaledFont; }

    void setMetrics(int ascent, int descent, int xHeight, int lineSpacing)
    {
        m_ascent = ascent;
        m_descent = descent;
        m_xHeight = xHeight;
        m_lineSpacing = lineSpacing;
    }

    int ascent() const { return m_ascent; }
    int descent() const { return m_descent; }
    int xHeight() const { return m_xHeight; }
    int lineSpacing() const { return m_lineSpacing; }

private:
    HFONT m_font;

    int m_ascent;
    int m_descent;
    int m_xHeight;
    int m_lineSpacing;

    cairo_font_face_t* m_fontFace;
    cairo_scaled_font_t* m_scaledFont;
};

FontData* getFontData(const FontDescription& fontDescription, const AtomicString& fontFace)
{
    // FIXME: Look this up in a hashtable so that we can cache Cairo fonts.  For now we just grab the
    // font over and over.
    LOGFONTW winfont;
    winfont.lfHeight = -fontDescription.computedPixelSize(); // The negative number is intentional.
    winfont.lfWidth = 0;
    winfont.lfEscapement = 0;
    winfont.lfOrientation = 0;
    winfont.lfUnderline = false;
    winfont.lfStrikeOut = false;
    winfont.lfCharSet = DEFAULT_CHARSET;
    winfont.lfOutPrecision = OUT_DEFAULT_PRECIS;
    const int CLEARTYPE_QUALITY = 5;
    winfont.lfQuality = CLEARTYPE_QUALITY; // FIXME: This is the Windows XP constant to force ClearType on.
    winfont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    winfont.lfItalic = fontDescription.italic();
    winfont.lfWeight = fontDescription.weight() == cBoldWeight ? 700 : 400; // FIXME: Support weights for real.
    int len = min(fontFace.length(), LF_FACESIZE - 1);
    memcpy(winfont.lfFaceName, fontFace.unicode(), len * sizeof(WORD));
    winfont.lfFaceName[len] = '\0';

    HFONT font = CreateFontIndirectW(&winfont);
    
    // Windows will always give us a valid pointer here, even if the face name is non-existent.  We have to double-check
    // and see if the family name was really used.
    HDC dc = GetDC((HWND)0);
    SaveDC(dc);
    SelectObject(dc, font);
    
    int ascent, descent, xHeight, lineSpacing;

    TEXTMETRIC tm;
    GetTextMetrics(dc, &tm);
    ascent = tm.tmAscent;
    descent = tm.tmDescent;
    xHeight = ascent * 0.56f;  // Best guess for xHeight for non-Truetype fonts.
    lineSpacing = tm.tmExternalLeading + tm.tmHeight;

    OUTLINETEXTMETRIC otm;
    if (GetOutlineTextMetrics(dc, sizeof(otm), &otm) > 0) {
        // This is a TrueType font.  We might be able to get an accurate xHeight.
        GLYPHMETRICS gm;
        MAT2 mat = { 1, 0, 0, 1 }; // The identity matrix.
        DWORD len = GetGlyphOutlineW(dc, QChar('x'), GGO_METRICS, &gm, 0, 0, &mat);
        if (len != GDI_ERROR && gm.gmptGlyphOrigin.y > 0)
            xHeight = int(gm.gmptGlyphOrigin.y + 0.5f);
    }

    WCHAR name[LF_FACESIZE];
    unsigned resultLength = GetTextFaceW(dc, LF_FACESIZE, name);
    if (resultLength > 0)
        resultLength--; // Subtract the null terminator.
    RestoreDC(dc, -1);
    if (!equalIgnoringCase(fontFace, String((QChar*)name, resultLength))) {
        DeleteObject(font);
        return 0;
    }
    
    // This font face is valid.  Create a FontData now.
    FontData* result = new FontData(font, fontDescription);
    result->setMetrics(ascent, descent, xHeight, lineSpacing);
    return result;
}

FontDataSet::FontDataSet()
:m_pitch(UnknownPitch)
{
    
}

FontDataSet::~FontDataSet()
{
    deleteAllValues(m_fontSet);
}

void FontDataSet::determinePitch(const FontDescription& fontDescription) const
{
    // FIXME: Implement this.
    m_pitch = VariablePitch;
}

void FontDataSet::invalidate()
{
    // Delete the Cairo fonts.
    m_pitch = UnknownPitch;
    deleteAllValues(m_fontSet);
}

FontData* FontDataSet::primaryFont(const FontDescription& fontDescription) const
{
    if (!m_fontSet.isEmpty())
        return m_fontSet[0];

    // We want to ensure that the primary Cairo font face exists.
    for (const FontFamily* currFamily = &fontDescription.family(); 
         currFamily;
         currFamily = currFamily->next()) {
        if (!currFamily->familyIsEmpty()) {
            // Attempt to create a FontData.
            FontData* font = getFontData(fontDescription, currFamily->family());
            if (font) {
                m_fontSet.append(font);
                return font;
            }
        }
    }

    // FIXME: Go ahead and use the serif default.  For now hardcode Times New Roman.
    // We'll need a way to either get to the settings without going through a frame, or we'll
    // need this to be passed in as part of the fallback list.
    FontData* defaultFont = getFontData(fontDescription, AtomicString("Times New Roman"));
    if (defaultFont)
        m_fontSet.append(defaultFont);
    return defaultFont;
}

float Font::floatWidth(const QChar* str, int slen, int pos, int len,
                       int tabWidth, int xpos) const
{
    FontData* font = m_dataSet->primaryFont(fontDescription());
    if (!font)
        return 0;

    HDC dc = GetDC((HWND)0); // FIXME: Need a way to get to the real HDC.
    SaveDC(dc);

    SelectObject(dc, font->hfont());

    // Get the text extent of the characters.
    // FIXME: Eventually we will have to go glyph by glyph.  For now we just assume that all
    // glyphs are present in the primary font.
    // FIXME: Support letter-spacing, word-spacing, smallcaps.
    // FIXME: Handle tabs (the tabWidth and xpos parameters)
    // FIXME: Handle RTL.
    SIZE s;
    BOOL result = GetTextExtentPoint32W(dc, (WCHAR*)(str + pos), len, &s);

    RestoreDC(dc, -1);

    if (!result)
        return 0;

    return s.cx;
}

int Font::ascent() const
{ 
    FontData* font = m_dataSet->primaryFont(fontDescription());
    if (font)
        return font->ascent();
    return 0;
}

int Font::descent() const
{ 
    FontData* font = m_dataSet->primaryFont(fontDescription());
    if (font)
        return font->descent();
    return 0;
}

float Font::xHeight() const
{
    FontData* font = m_dataSet->primaryFont(fontDescription());
    if (font)
        return font->xHeight();
    return 0;
}

int Font::lineSpacing() const
{ 
    FontData* font = m_dataSet->primaryFont(fontDescription());
    if (font)
        return font->lineSpacing();
    return 0;
}

bool Font::isFixedPitch() const
{
    return m_dataSet->isFixedPitch(fontDescription());
}

void Font::drawText(const GraphicsContext* context, int x, int y, int tabWidth, int xpos, const QChar* str, int len, int from, int to,
                    int toAdd, TextDirection d, bool visuallyOrdered) const
{
    FontData* font = m_dataSet->primaryFont(fontDescription());
    if (!font)
        return;

    cairo_surface_t* surface = cairo_get_target(context->platformContext());
    HDC dc = cairo_win32_surface_get_dc(surface);

    SaveDC(dc);
    SelectObject(dc, font->hfont());

    int offset = 0;
    int length = len;
    if (from > 0) {
        offset = from;
        length = len - from;
    }
    if (to > 0)
        length = to - from;

    y -= font->ascent();

    SetBkMode(dc, TRANSPARENT);
    const Color& color = context->pen().color();
    SetTextColor(dc, RGB(color.red(), color.green(), color.blue())); // FIXME: Need to support alpha in the text color.
    TextOutW(dc, x, y, (LPCWSTR)(str+offset), length);

    RestoreDC(dc, -1);
}

void Font::drawHighlightForText(const GraphicsContext* context, int x, int y, int h, int tabWidth, int xpos, const QChar* str,
                                int len, int from, int to, int toAdd,
                                TextDirection d, bool visuallyOrdered, const Color& backgroundColor) const
{
}

void Font::drawLineForText(const GraphicsContext* context, int x, int y, int yOffset, int width) const
{
}

void Font::drawLineForMisspelling(const GraphicsContext* context, int x, int y, int width) const
{
}

int Font::misspellingLineThickness(const GraphicsContext* context) const
{
    return 0;
}


}
