/*
 * Copyright (C) 2024 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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

#include "GlyphBuffer.h"
#include "NotImplemented.h"
#include "PathSkia.h"
#include "SkiaHarfBuzzFont.h"
#include <hb-ot.h>
#include <skia/core/SkFont.h>
#include <skia/core/SkFontMetrics.h>

namespace WebCore {

Path Font::platformPathForGlyph(Glyph glyph) const
{
    auto path = PathSkia::create();
    const auto& font = m_platformData.skFont();
    font.getPath(glyph, path->platformPath());
    return { path };
}

FloatRect Font::platformBoundsForGlyph(Glyph glyph) const
{
    if (!m_platformData.size())
        return { };

    const auto& font = m_platformData.skFont();
    SkRect bounds = font.getBounds(glyph, nullptr);
    if (!font.isSubpixel()) {
        SkIRect rect;
        bounds.roundOut(&rect);
        bounds.set(rect);
    }
    return bounds;
}

float Font::platformWidthForGlyph(Glyph glyph) const
{
    if (!m_platformData.size())
        return 0;

    const auto& font = m_platformData.skFont();
    SkScalar width = font.getWidth(glyph);

    if (!font.isSubpixel())
        width = SkScalarRoundToInt(width);

    return SkScalarToFloat(width);
}

void Font::platformInit()
{
    if (!m_platformData.size())
        return;

    const auto& font = m_platformData.skFont();
    SkFontMetrics metrics;
    font.getMetrics(&metrics);

    auto ascent = SkScalarRoundToScalar(-metrics.fAscent);
    auto descent = SkScalarRoundToScalar(metrics.fDescent);
    m_fontMetrics.setAscent(ascent);
    m_fontMetrics.setDescent(descent);

    auto lineGap = SkScalarToFloat(metrics.fLeading);
    m_fontMetrics.setLineGap(lineGap);
    m_fontMetrics.setLineSpacing(lroundf(ascent) + lroundf(descent) + lroundf(lineGap));

    m_fontMetrics.setCapHeight(metrics.fCapHeight);

    float underlinePosition;
    if (metrics.hasUnderlinePosition(&underlinePosition))
        m_fontMetrics.setUnderlinePosition(underlinePosition);
    float underlineThickness;
    if (metrics.hasUnderlineThickness(&underlineThickness))
        m_fontMetrics.setUnderlineThickness(underlineThickness);

    if (metrics.fXHeight)
        m_fontMetrics.setXHeight(metrics.fXHeight);

    m_maxCharWidth = SkScalarRoundToInt(metrics.fXMax - metrics.fXMin);
    if (metrics.fAvgCharWidth)
        m_avgCharWidth = SkScalarToFloat(metrics.fAvgCharWidth);

    m_fontMetrics.setUnitsPerEm(font.getTypeface()->getUnitsPerEm());

    // FIXME: add support for SomeEmojiGlyphs once Skia provides API for that.
    // See https://issues.skia.org/issues/374078818.
    if (m_platformData.isColorBitmapFont())
        m_emojiType = AllEmojiGlyphs { };
    else
        m_emojiType = NoEmojiGlyphs { };

    SkString familyName;
    font.getTypeface()->getFamilyName(&familyName);
    if (equalIgnoringASCIICase(familyName.c_str(), "Ahem"_s))
        m_allowsAntialiasing = false;
}

void Font::platformCharWidthInit()
{
    m_avgCharWidth = 0.f;
    m_maxCharWidth = 0.f;
    initCharWidths();
}

RefPtr<Font> Font::platformCreateScaledFont(const FontDescription&, float scaleFactor) const
{
    return Font::create(FontPlatformData(m_platformData.skFont().refTypeface(), scaleFactor * m_platformData.size(),
        m_platformData.syntheticBold(),
        m_platformData.syntheticOblique(),
        m_platformData.orientation(),
        m_platformData.widthVariant(),
        m_platformData.textRenderingMode(),
        Vector<hb_feature_t> { m_platformData.features() },
        m_platformData.customPlatformData()),
        origin(), IsInterstitial::No);
}

RefPtr<Font> Font::platformCreateHalfWidthFont() const
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=281333 : implement half width font for this platform.
    notImplemented();
    return nullptr;
}

void Font::determinePitch()
{
    m_treatAsFixedPitch = m_platformData.isFixedPitch();
}

bool Font::variantCapsSupportedForSynthesis(FontVariantCaps fontVariantCaps) const
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

bool Font::platformSupportsCodePoint(char32_t character, std::optional<char32_t> variation) const
{
    if (auto* skiaHarfBuzzFont = m_platformData.skiaHarfBuzzFont())
        return !!skiaHarfBuzzFont->glyph(character, variation);

    return m_platformData.skFont().getTypeface()->unicharToGlyph(character);
}

static inline float harfBuzzPositionToFloat(hb_position_t value)
{
    return static_cast<float>(value) / (1 << 16);
}

GlyphBufferAdvance Font::applyTransforms(GlyphBuffer& glyphBuffer, unsigned beginningGlyphIndex, unsigned beginningStringIndex, bool enableKerning, bool requiresShaping, const AtomString& locale, StringView text, TextDirection textDirection) const
{
    if (!enableKerning && !requiresShaping)
        return makeGlyphBufferAdvance();

    if (!platformData().size())
        return makeGlyphBufferAdvance();

    bool hasVisibleGlyphs = [&] {
        for (unsigned i = beginningGlyphIndex; i < glyphBuffer.size(); ++i) {
            // For now we only considered deletedGlyph.
            if (glyphBuffer.glyphAt(i) != deletedGlyph)
                return true;
        }
        return false;
    }();
    if (!hasVisibleGlyphs)
        return makeGlyphBufferAdvance();

    auto* hbFont = platformData().hbFont();
    RELEASE_ASSERT(hbFont);

    const auto& features = platformData().features();

    bool requiresHarfbuzzShaping = [&] {
        bool hasEnabledFeatures = features.containsIf([](const auto& feature) {
            return !!feature.value;
        });
        if (hasEnabledFeatures)
            return true;

        if (enableKerning && platformData().skFont().getTypeface()->getKerningPairAdjustments({ }, { }))
            return true;

        if (!requiresShaping)
            return false;

        auto* hbFace = hb_font_get_face(hbFont);
        if (!hbFace || !hb_ot_layout_has_substitution(hbFace))
            return false;

        return true;
    }();
    if (!requiresHarfbuzzShaping)
        return makeGlyphBufferAdvance();

    // Kerning is not handled as font features, so only in case it's explicitly disabled
    // we need to create a new vector to include kern feature.
    const hb_feature_t* featuresData = features.isEmpty() ? nullptr : features.span().data();
    unsigned featuresSize = features.size();
    Vector<hb_feature_t> featuresWithKerning;
    if (!enableKerning) {
        featuresWithKerning.reserveInitialCapacity(featuresSize + 1);
        static constexpr hb_feature_t kernFeature { HB_TAG('k', 'e', 'r', 'n'), 0, 0, static_cast<unsigned>(-1) };
        featuresWithKerning.append(kernFeature);
        featuresWithKerning.appendVector(features);
        featuresData = featuresWithKerning.span().data();
        featuresSize = featuresWithKerning.size();
    }

    static thread_local HbUniquePtr<hb_buffer_t> buffer(hb_buffer_create());

    // The computed "locale" equals the "lang" attribute. The latter must be a valid BCP 47 language tag,
    // according to <https://html.spec.whatwg.org/multipage/dom.html#attr-lang>.
    // This is exactly what hb_language_from_string() expects, so we can pass directly.
    ASSERT(locale.is8Bit());
    auto language = hb_language_from_string(reinterpret_cast<const char*>(locale.span8().data()), -1);
    hb_buffer_set_language(buffer.get(), language);
    hb_buffer_set_direction(buffer.get(), textDirection == TextDirection::RTL ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);

    auto glyphBufferGlyphCount = glyphBuffer.size() - beginningGlyphIndex;
    auto characters = text.substring(beginningStringIndex, glyphBufferGlyphCount);
    if (characters.is8Bit())
        hb_buffer_add_latin1(buffer.get(), characters.span8().data(), characters.span8().size(), 0, -1);
    else
        hb_buffer_add_utf16(buffer.get(), reinterpret_cast<const uint16_t*>(characters.span16().data()), characters.span16().size(), 0, -1);
    hb_shape(hbFont, buffer.get(), featuresData, featuresSize);

    unsigned glyphCount = hb_buffer_get_length(buffer.get());
    if (glyphCount < glyphBufferGlyphCount)
        glyphBuffer.shrink(beginningGlyphIndex + glyphCount);
    else if (glyphCount > glyphBufferGlyphCount)
        glyphBuffer.makeHole(beginningGlyphIndex + glyphBufferGlyphCount, glyphCount - glyphBufferGlyphCount, this);

    auto* infos = hb_buffer_get_glyph_infos(buffer.get(), nullptr);
    auto* positions = hb_buffer_get_glyph_positions(buffer.get(), nullptr);
    for (unsigned i = 0; i < glyphCount; ++i) {
        if (glyphBuffer.glyphAt(i) == deletedGlyph)
            continue;

        Glyph glyph = infos[i].codepoint;
        glyphBuffer.glyphs(beginningGlyphIndex).data()[i] = glyph;
        glyphBuffer.offsetsInString(beginningGlyphIndex).data()[i] = beginningStringIndex + infos[i].cluster;
        if (isZeroWidthSpaceGlyph(glyph))
            continue;

        glyphBuffer.advances(beginningGlyphIndex).data()[i] = { harfBuzzPositionToFloat(positions[i].x_advance), harfBuzzPositionToFloat(positions[i].y_advance) };
        glyphBuffer.origins(beginningGlyphIndex).data()[i] = { harfBuzzPositionToFloat(positions[i].x_offset), harfBuzzPositionToFloat(positions[i].y_offset) };
    }

    hb_buffer_reset(buffer.get());

    if (textDirection == TextDirection::RTL)
        glyphBuffer.reverse(beginningGlyphIndex, glyphBuffer.size() - beginningGlyphIndex);

    return makeGlyphBufferAdvance();
}

} // namespace WebCore
