/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007, 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Font.h"

#include "CairoUniquePtr.h"
#include "CairoUtilities.h"
#include "FloatConversion.h"
#include "FloatRect.h"
#include "FontCache.h"
#include "FontDescription.h"
#include "GlyphBuffer.h"
#include "OpenTypeTypes.h"
#include "RefPtrCairo.h"
#include "SurrogatePairAwareTextIterator.h"
#include "UTF16UChar32Iterator.h"
#include <cairo-ft.h>
#include <cairo.h>
#include <fontconfig/fcfreetype.h>
#include <ft2build.h>
#include FT_TRUETYPE_TABLES_H
#include FT_TRUETYPE_TAGS_H
#include <wtf/MathExtras.h>

namespace WebCore {

static RefPtr<cairo_scaled_font_t> scaledFontWithoutMetricsHinting(cairo_scaled_font_t* scaledFont)
{
    CairoUniquePtr<cairo_font_options_t> fontOptions(cairo_font_options_create());
    cairo_scaled_font_get_font_options(scaledFont, fontOptions.get());
    cairo_font_options_set_hint_metrics(fontOptions.get(), CAIRO_HINT_METRICS_OFF);
    cairo_matrix_t fontMatrix;
    cairo_scaled_font_get_font_matrix(scaledFont, &fontMatrix);
    cairo_matrix_t fontCTM;
    cairo_scaled_font_get_ctm(scaledFont, &fontCTM);
    return adoptRef(cairo_scaled_font_create(cairo_scaled_font_get_font_face(scaledFont), &fontMatrix, &fontCTM, fontOptions.get()));
}

static float scaledFontScaleFactor(cairo_scaled_font_t* scaledFont)
{
    cairo_matrix_t fontMatrix;
    cairo_scaled_font_get_font_matrix(scaledFont, &fontMatrix);

    float determinant = fontMatrix.xx * fontMatrix.yy - fontMatrix.yx * fontMatrix.xy;
    if (!std::isfinite(determinant))
        return 1;

    determinant = std::abs(determinant);
    if (!determinant)
        return 0;

    double x = 1;
    double y = 0;
    cairo_matrix_transform_distance(&fontMatrix, &x, &y);
    double xScale = std::hypot(x, y);
    return xScale ? narrowPrecisionToFloat(determinant / xScale) : 0.;
}

static Optional<unsigned> fontUnitsPerEm(FT_Face freeTypeFace)
{
    if (freeTypeFace->units_per_EM)
        return freeTypeFace->units_per_EM;

    if (auto* ttHeader = static_cast<TT_Header*>(FT_Get_Sfnt_Table(freeTypeFace, ft_sfnt_head)))
        return ttHeader->Units_Per_EM;

    return WTF::nullopt;
}

void Font::platformInit()
{
    if (!m_platformData.size())
        return;

    ASSERT(m_platformData.scaledFont());
    // Temporarily create a clone that doesn't have metrics hinting in order to avoid incorrect
    // rounding resulting in incorrect baseline positioning since the sum of ascent and descent
    // becomes larger than the line height.
    auto fontWithoutMetricsHinting = scaledFontWithoutMetricsHinting(m_platformData.scaledFont());
    cairo_font_extents_t fontExtents;
    cairo_scaled_font_extents(fontWithoutMetricsHinting.get(), &fontExtents);

    float ascent = narrowPrecisionToFloat(fontExtents.ascent);
    float descent = narrowPrecisionToFloat(fontExtents.descent);
    float capHeight = narrowPrecisionToFloat(fontExtents.height);
    float lineGap = narrowPrecisionToFloat(fontExtents.height - fontExtents.ascent - fontExtents.descent);
    Optional<float> xHeight;
    Optional<unsigned> unitsPerEm;
    Optional<float> underlinePosition;
    Optional<float> underlineThickness;

    {
        CairoFtFaceLocker cairoFtFaceLocker(m_platformData.scaledFont());
        if (FT_Face freeTypeFace = cairoFtFaceLocker.ftFace()) {
            unitsPerEm = fontUnitsPerEm(freeTypeFace);

            if (freeTypeFace->face_flags & FT_FACE_FLAG_SCALABLE) {
                // If the USE_TYPO_METRICS flag is set in the OS/2 table then we use typo metrics instead.
                if (auto* OS2Table = static_cast<TT_OS2*>(FT_Get_Sfnt_Table(freeTypeFace, ft_sfnt_os2))) {
                    const FT_Short kUseTypoMetricsMask = 1 << 7;
                    // FT_Size_Metrics::y_scale is in 16.16 fixed point format.
                    // Its (fractional) value is a factor that converts vertical metrics from design units to units of 1/64 pixels.
                    double yscale = (freeTypeFace->size->metrics.y_scale / 65536.0) / 64.0;
                    if (OS2Table->fsSelection & kUseTypoMetricsMask) {
                        ascent = narrowPrecisionToFloat(yscale * OS2Table->sTypoAscender);
                        descent = -narrowPrecisionToFloat(yscale * OS2Table->sTypoDescender);
                        lineGap = narrowPrecisionToFloat(yscale * OS2Table->sTypoLineGap);
                    }
                    xHeight = narrowPrecisionToFloat(yscale * OS2Table->sxHeight);
                }

                if (unitsPerEm) {
                    float scaleFactor = scaledFontScaleFactor(fontWithoutMetricsHinting.get());
                    underlinePosition = -((freeTypeFace->underline_position + freeTypeFace->underline_thickness / 2.) / static_cast<float>(unitsPerEm.value())) * scaleFactor;
                    underlineThickness = (freeTypeFace->underline_thickness / static_cast<float>(unitsPerEm.value())) * scaleFactor;
                }
            }
        }
    }

    if (!xHeight) {
        cairo_text_extents_t textExtents;
        cairo_scaled_font_text_extents(m_platformData.scaledFont(), "x", &textExtents);
        xHeight = narrowPrecisionToFloat((platformData().orientation() == FontOrientation::Horizontal) ? textExtents.height : textExtents.width);
    }

    m_fontMetrics.setAscent(ascent);
    m_fontMetrics.setDescent(descent);
    m_fontMetrics.setCapHeight(capHeight);
    m_fontMetrics.setLineSpacing(lroundf(ascent) + lroundf(descent) + lroundf(lineGap));
    m_fontMetrics.setLineGap(lineGap);
    m_fontMetrics.setXHeight(xHeight.value());
    if (unitsPerEm)
        m_fontMetrics.setUnitsPerEm(unitsPerEm.value());
    if (underlinePosition)
        m_fontMetrics.setUnderlinePosition(underlinePosition.value());
    if (underlineThickness)
        m_fontMetrics.setUnderlineThickness(underlineThickness.value());

    cairo_text_extents_t textExtents;
    cairo_scaled_font_text_extents(m_platformData.scaledFont(), " ", &textExtents);
    m_spaceWidth = narrowPrecisionToFloat((platformData().orientation() == FontOrientation::Horizontal) ? textExtents.x_advance : -textExtents.y_advance);
    m_syntheticBoldOffset = m_platformData.syntheticBold() ? 1.0f : 0.f;

    FcChar8* fontConfigFamilyName;
    if (FcPatternGetString(m_platformData.fcPattern(), FC_FAMILY, 0, &fontConfigFamilyName) == FcResultMatch) {
        String familyName = String::fromUTF8(reinterpret_cast<char*>(fontConfigFamilyName));
        // Disable antialiasing for the Ahem font because many tests require this.
        if (equalIgnoringASCIICase(familyName, "Ahem"))
            m_allowsAntialiasing = false;
    }
}

void Font::platformCharWidthInit()
{
    m_avgCharWidth = 0.f;
    m_maxCharWidth = 0.f;
    initCharWidths();
}

RefPtr<Font> Font::platformCreateScaledFont(const FontDescription& fontDescription, float scaleFactor) const
{
    ASSERT(m_platformData.scaledFont());
    return Font::create(FontPlatformData(cairo_scaled_font_get_font_face(m_platformData.scaledFont()),
        m_platformData.fcPattern(),
        scaleFactor * fontDescription.computedSize(),
        m_platformData.isFixedWidth(),
        m_platformData.syntheticBold(),
        m_platformData.syntheticOblique(),
        fontDescription.orientation()),
        origin(), Interstitial::No);
}

void Font::determinePitch()
{
    m_treatAsFixedPitch = m_platformData.isFixedPitch();
}

FloatRect Font::platformBoundsForGlyph(Glyph glyph) const
{
    if (!m_platformData.size())
        return FloatRect();

    cairo_glyph_t cglyph = { glyph, 0, 0 };
    cairo_text_extents_t extents;
    cairo_scaled_font_glyph_extents(m_platformData.scaledFont(), &cglyph, 1, &extents);

    if (cairo_scaled_font_status(m_platformData.scaledFont()) == CAIRO_STATUS_SUCCESS)
        return FloatRect(extents.x_bearing, extents.y_bearing, extents.width, extents.height);

    return FloatRect();
}

float Font::platformWidthForGlyph(Glyph glyph) const
{
    if (!m_platformData.size())
        return 0;

    if (cairo_scaled_font_status(m_platformData.scaledFont()) != CAIRO_STATUS_SUCCESS)
        return m_spaceWidth;

    cairo_glyph_t cairoGlyph = { glyph, 0, 0 };
    cairo_text_extents_t extents;
    cairo_scaled_font_glyph_extents(m_platformData.scaledFont(), &cairoGlyph, 1, &extents);
    float width = platformData().orientation() == FontOrientation::Horizontal ? extents.x_advance : -extents.y_advance;
    return width ? width : m_spaceWidth;
}

bool Font::variantCapsSupportsCharacterForSynthesis(FontVariantCaps fontVariantCaps, UChar32) const
{
    switch (fontVariantCaps) {
    case FontVariantCaps::Small:
    case FontVariantCaps::Petite:
    case FontVariantCaps::AllSmall:
    case FontVariantCaps::AllPetite:
        return false;
    default:
        // Synthesis only supports the variant-caps values listed above.
        return true;
    }
}

bool Font::platformSupportsCodePoint(UChar32 character, Optional<UChar32> variation) const
{
    CairoFtFaceLocker cairoFtFaceLocker(m_platformData.scaledFont());
    if (FT_Face face = cairoFtFaceLocker.ftFace())
        return variation ? !!FT_Face_GetCharVariantIndex(face, character, variation.value()) : !!FcFreeTypeCharIndex(face, character);

    return false;
}

}
