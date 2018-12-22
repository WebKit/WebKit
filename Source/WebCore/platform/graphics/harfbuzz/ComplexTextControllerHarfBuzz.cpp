/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ComplexTextController.h"

#include "CairoUtilities.h"
#include "FontCascade.h"
#include "HbUniquePtr.h"
#include "SurrogatePairAwareTextIterator.h"
#include <hb-ft.h>
#include <hb-icu.h>
#include <hb-ot.h>

#if ENABLE(VARIATION_FONTS)
#include FT_MULTIPLE_MASTERS_H
#endif

namespace WebCore {

static inline float harfBuzzPositionToFloat(hb_position_t value)
{
    return static_cast<float>(value) / (1 << 16);
}

static inline hb_position_t floatToHarfBuzzPosition(float value)
{
    return static_cast<hb_position_t>(value * (1 << 16));
}

static inline hb_position_t doubleToHarfBuzzPosition(double value)
{
    return static_cast<hb_position_t>(value * (1 << 16));
}

static hb_font_funcs_t* harfBuzzFontFunctions()
{
    static hb_font_funcs_t* fontFunctions = nullptr;

    // We don't set callback functions which we can't support.
    // Harfbuzz will use the fallback implementation if they aren't set.
    if (!fontFunctions) {
        fontFunctions = hb_font_funcs_create();
#if HB_VERSION_ATLEAST(1, 2, 3)
        hb_font_funcs_set_nominal_glyph_func(fontFunctions, [](hb_font_t*, void* context, hb_codepoint_t unicode, hb_codepoint_t* glyph, void*) -> hb_bool_t {
            auto& font = *static_cast<Font*>(context);
            *glyph = font.glyphForCharacter(unicode);
            return !!*glyph;
        }, nullptr, nullptr);

        hb_font_funcs_set_variation_glyph_func(fontFunctions, [](hb_font_t*, void* context, hb_codepoint_t unicode, hb_codepoint_t variation, hb_codepoint_t* glyph, void*) -> hb_bool_t {
            auto& font = *static_cast<Font*>(context);
            auto* scaledFont = font.platformData().scaledFont();
            ASSERT(scaledFont);

            CairoFtFaceLocker cairoFtFaceLocker(scaledFont);
            if (FT_Face ftFace = cairoFtFaceLocker.ftFace()) {
                *glyph = FT_Face_GetCharVariantIndex(ftFace, unicode, variation);
                return true;
            }
            return false;
            }, nullptr, nullptr);
#else
        hb_font_funcs_set_glyph_func(fontFunctions, [](hb_font_t*, void* context, hb_codepoint_t unicode, hb_codepoint_t, hb_codepoint_t* glyph, void*) -> hb_bool_t {
            auto& font = *static_cast<Font*>(context);
            *glyph = font.glyphForCharacter(unicode);
            return !!*glyph;
        }, nullptr, nullptr);
#endif
        hb_font_funcs_set_glyph_h_advance_func(fontFunctions, [](hb_font_t*, void* context, hb_codepoint_t point, void*) -> hb_bool_t {
            auto& font = *static_cast<Font*>(context);
            auto* scaledFont = font.platformData().scaledFont();
            ASSERT(scaledFont);

            cairo_text_extents_t glyphExtents;
            cairo_glyph_t glyph = { point, 0, 0 };
            cairo_scaled_font_glyph_extents(scaledFont, &glyph, 1, &glyphExtents);

            bool hasVerticalGlyphs = glyphExtents.y_advance;
            return doubleToHarfBuzzPosition(hasVerticalGlyphs ? -glyphExtents.y_advance : glyphExtents.x_advance);
        }, nullptr, nullptr);

        hb_font_funcs_set_glyph_h_origin_func(fontFunctions, [](hb_font_t*, void*, hb_codepoint_t, hb_position_t*, hb_position_t*, void*) -> hb_bool_t {
            // Just return true, following the way that Harfbuzz-FreeType implementation does.
            return true;
        }, nullptr, nullptr);

        hb_font_funcs_set_glyph_extents_func(fontFunctions, [](hb_font_t*, void* context, hb_codepoint_t point, hb_glyph_extents_t* extents, void*) -> hb_bool_t {
            auto& font = *static_cast<Font*>(context);
            auto* scaledFont = font.platformData().scaledFont();
            ASSERT(scaledFont);

            cairo_text_extents_t glyphExtents;
            cairo_glyph_t glyph = { point, 0, 0 };
            cairo_scaled_font_glyph_extents(scaledFont, &glyph, 1, &glyphExtents);

            bool hasVerticalGlyphs = glyphExtents.y_advance;
            extents->x_bearing = doubleToHarfBuzzPosition(glyphExtents.x_bearing);
            extents->y_bearing = doubleToHarfBuzzPosition(hasVerticalGlyphs ? -glyphExtents.y_bearing : glyphExtents.y_bearing);
            extents->width = doubleToHarfBuzzPosition(hasVerticalGlyphs ? -glyphExtents.height : glyphExtents.width);
            extents->height = doubleToHarfBuzzPosition(hasVerticalGlyphs ? glyphExtents.width : glyphExtents.height);
            return true;
        }, nullptr, nullptr);

        hb_font_funcs_make_immutable(fontFunctions);
    }
    return fontFunctions;
}

ComplexTextController::ComplexTextRun::ComplexTextRun(hb_buffer_t* buffer, const Font& font, const UChar* characters, unsigned stringLocation, unsigned stringLength, unsigned indexBegin, unsigned indexEnd)
    : m_initialAdvance(0, 0)
    , m_font(font)
    , m_characters(characters)
    , m_stringLength(stringLength)
    , m_indexBegin(indexBegin)
    , m_indexEnd(indexEnd)
    , m_glyphCount(hb_buffer_get_length(buffer))
    , m_stringLocation(stringLocation)
    , m_isLTR(HB_DIRECTION_IS_FORWARD(hb_buffer_get_direction(buffer)))
{
    if (!m_glyphCount)
        return;

    m_glyphs.grow(m_glyphCount);
    m_baseAdvances.grow(m_glyphCount);
    m_glyphOrigins.grow(m_glyphCount);
    m_coreTextIndices.grow(m_glyphCount);

    hb_glyph_info_t* glyphInfos = hb_buffer_get_glyph_infos(buffer, nullptr);
    hb_glyph_position_t* glyphPositions = hb_buffer_get_glyph_positions(buffer, nullptr);

    // HarfBuzz returns the shaping result in visual order. We don't need to flip for RTL.
    for (unsigned i = 0; i < m_glyphCount; ++i) {
        m_coreTextIndices[i] = glyphInfos[i].cluster;

        uint16_t glyph = glyphInfos[i].codepoint;
        if (m_font.isZeroWidthSpaceGlyph(glyph) || !m_font.platformData().size()) {
            m_glyphs[i] = glyph;
            m_baseAdvances[i] = { };
            m_glyphOrigins[i] = { };
            continue;
        }

        float offsetX = harfBuzzPositionToFloat(glyphPositions[i].x_offset);
        float offsetY = -harfBuzzPositionToFloat(glyphPositions[i].y_offset);
        float advanceX = harfBuzzPositionToFloat(glyphPositions[i].x_advance);
        float advanceY = harfBuzzPositionToFloat(glyphPositions[i].y_advance);

        m_glyphs[i] = glyph;
        m_baseAdvances[i] = { advanceX, advanceY };
        m_glyphOrigins[i] = { offsetX, offsetY };
    }
}

static const hb_tag_t s_vertTag = HB_TAG('v', 'e', 'r', 't');
static const hb_tag_t s_vrt2Tag = HB_TAG('v', 'r', 't', '2');
static const hb_tag_t s_kernTag = HB_TAG('k', 'e', 'r', 'n');
static const unsigned s_hbEnd = static_cast<unsigned>(-1);

static Vector<hb_feature_t, 4> fontFeatures(const FontCascade& font, FontOrientation orientation)
{
    Vector<hb_feature_t, 4> features;

    if (orientation == FontOrientation::Vertical) {
        features.append({ s_vertTag, 1, 0, s_hbEnd });
        features.append({ s_vrt2Tag, 1, 0, s_hbEnd });
    }

    hb_feature_t kerning = { s_kernTag, 0, 0, s_hbEnd };
    if (font.enableKerning())
        kerning.value = 1;
    features.append(WTFMove(kerning));

    for (auto& feature : font.fontDescription().featureSettings()) {
        auto& tag = feature.tag();
        features.append({ HB_TAG(tag[0], tag[1], tag[2], tag[3]), static_cast<uint32_t>(feature.value()), 0, s_hbEnd });
    }

    return features;
}

static Optional<UScriptCode> characterScript(UChar32 character)
{
    UErrorCode errorCode = U_ZERO_ERROR;
    UScriptCode script = uscript_getScript(character, &errorCode);
    if (U_FAILURE(errorCode))
        return WTF::nullopt;
    return script;
}

struct HBRun {
    unsigned startIndex;
    unsigned endIndex;
    UScriptCode script;
};

static Optional<HBRun> findNextRun(const UChar* characters, unsigned length, unsigned offset)
{
    SurrogatePairAwareTextIterator textIterator(characters + offset, offset, length, length);
    UChar32 character;
    unsigned clusterLength = 0;
    if (!textIterator.consume(character, clusterLength))
        return WTF::nullopt;

    auto currentScript = characterScript(character);
    if (!currentScript)
        return WTF::nullopt;

    unsigned startIndex = offset;
    for (textIterator.advance(clusterLength); textIterator.consume(character, clusterLength); textIterator.advance(clusterLength)) {
        if (FontCascade::treatAsZeroWidthSpace(character))
            continue;

        auto nextScript = characterScript(character);
        if (!nextScript)
            return WTF::nullopt;

        // §5.1 Handling Characters with the Common Script Property.
        // Programs must resolve any of the special Script property values, such as Common,
        // based on the context of the surrounding characters. A simple heuristic uses the
        // script of the preceding character, which works well in many cases.
        // http://www.unicode.org/reports/tr24/#Common.
        //
        // FIXME: cover all other cases mentioned in the spec (ie. brackets or quotation marks).
        // https://bugs.webkit.org/show_bug.cgi?id=177003.
        //
        // If next script is inherited or common, keep using the current script.
        if (nextScript == USCRIPT_INHERITED || nextScript == USCRIPT_COMMON)
            continue;
        // If current script is inherited or common, set the next script as current.
        if (currentScript == USCRIPT_INHERITED || currentScript == USCRIPT_COMMON) {
            currentScript = nextScript;
            continue;
        }

        if (currentScript != nextScript && !uscript_hasScript(character, currentScript.value()))
            return Optional<HBRun>({ startIndex, textIterator.currentIndex(), currentScript.value() });
    }

    return Optional<HBRun>({ startIndex, textIterator.currentIndex(), currentScript.value() });
}

static hb_script_t findScriptForVerticalGlyphSubstitution(hb_face_t* face)
{
    static const unsigned maxCount = 32;

    unsigned scriptCount = maxCount;
    hb_tag_t scriptTags[maxCount];
    hb_ot_layout_table_get_script_tags(face, HB_OT_TAG_GSUB, 0, &scriptCount, scriptTags);
    for (unsigned scriptIndex = 0; scriptIndex < scriptCount; ++scriptIndex) {
        unsigned languageCount = maxCount;
        hb_tag_t languageTags[maxCount];
        hb_ot_layout_script_get_language_tags(face, HB_OT_TAG_GSUB, scriptIndex, 0, &languageCount, languageTags);
        for (unsigned languageIndex = 0; languageIndex < languageCount; ++languageIndex) {
            unsigned featureIndex;
            if (hb_ot_layout_language_find_feature(face, HB_OT_TAG_GSUB, scriptIndex, languageIndex, s_vertTag, &featureIndex)
                || hb_ot_layout_language_find_feature(face, HB_OT_TAG_GSUB, scriptIndex, languageIndex, s_vrt2Tag, &featureIndex))
                return hb_ot_tag_to_script(scriptTags[scriptIndex]);
        }
    }
    return HB_SCRIPT_INVALID;
}

void ComplexTextController::collectComplexTextRunsForCharacters(const UChar* characters, unsigned length, unsigned stringLocation, const Font* font)
{
    if (!font) {
        // Create a run of missing glyphs from the primary font.
        m_complexTextRuns.append(ComplexTextRun::create(m_font.primaryFont(), characters, stringLocation, length, 0, length, m_run.ltr()));
        return;
    }

    Vector<HBRun> runList;
    unsigned offset = 0;
    while (offset < length) {
        auto run = findNextRun(characters, length, offset);
        if (!run)
            break;
        runList.append(run.value());
        offset = run->endIndex;
    }

    size_t runCount = runList.size();
    if (!runCount)
        return;

    const auto& fontPlatformData = font->platformData();
    auto* scaledFont = fontPlatformData.scaledFont();
    CairoFtFaceLocker cairoFtFaceLocker(scaledFont);
    FT_Face ftFace = cairoFtFaceLocker.ftFace();
    if (!ftFace)
        return;

    HbUniquePtr<hb_face_t> face(hb_ft_face_create_cached(ftFace));
    HbUniquePtr<hb_font_t> harfBuzzFont(hb_font_create(face.get()));
    hb_font_set_funcs(harfBuzzFont.get(), harfBuzzFontFunctions(), const_cast<Font*>(font), nullptr);
    const float size = fontPlatformData.size();
    if (floorf(size) == size)
        hb_font_set_ppem(harfBuzzFont.get(), size, size);
    int scale = floatToHarfBuzzPosition(size);
    hb_font_set_scale(harfBuzzFont.get(), scale, scale);

#if ENABLE(VARIATION_FONTS)
    FT_MM_Var* ftMMVar;
    if (!FT_Get_MM_Var(ftFace, &ftMMVar)) {
        Vector<FT_Fixed, 4> coords;
        coords.resize(ftMMVar->num_axis);
        if (!FT_Get_Var_Design_Coordinates(ftFace, coords.size(), coords.data())) {
            Vector<hb_variation_t, 4> variations(coords.size());
            for (FT_UInt i = 0; i < ftMMVar->num_axis; ++i) {
                variations[i].tag = ftMMVar->axis[i].tag;
                variations[i].value = coords[i] / 65536.0;
            }
            hb_font_set_variations(harfBuzzFont.get(), variations.data(), variations.size());
        }
        FT_Done_MM_Var(ftFace->glyph->library, ftMMVar);
    }
#endif

    hb_font_make_immutable(harfBuzzFont.get());

    auto features = fontFeatures(m_font, fontPlatformData.orientation());
    HbUniquePtr<hb_buffer_t> buffer(hb_buffer_create());
    if (fontPlatformData.orientation() == FontOrientation::Vertical)
        hb_buffer_set_script(buffer.get(), findScriptForVerticalGlyphSubstitution(face.get()));

    for (unsigned i = 0; i < runCount; ++i) {
        auto& run = runList[m_run.rtl() ? runCount - i - 1 : i];

        if (fontPlatformData.orientation() != FontOrientation::Vertical)
            hb_buffer_set_script(buffer.get(), hb_icu_script_to_script(run.script));
        if (!m_mayUseNaturalWritingDirection || m_run.directionalOverride())
            hb_buffer_set_direction(buffer.get(), m_run.rtl() ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
        else {
            // Leaving direction to HarfBuzz to guess is *really* bad, but will do for now.
            hb_buffer_guess_segment_properties(buffer.get());
        }
        hb_buffer_add_utf16(buffer.get(), reinterpret_cast<const uint16_t*>(characters), length, run.startIndex, run.endIndex - run.startIndex);

        hb_shape(harfBuzzFont.get(), buffer.get(), features.isEmpty() ? nullptr : features.data(), features.size());
        m_complexTextRuns.append(ComplexTextRun::create(buffer.get(), *font, characters, stringLocation, length, run.startIndex, run.endIndex));
        hb_buffer_reset(buffer.get());
    }
}

} // namespace WebCore
