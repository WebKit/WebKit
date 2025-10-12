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

#include "FontCascade.h"
#include "FontFeatureValues.h"
#include "FontTaggedSettings.h"
#include "HbUniquePtr.h"
#include "SurrogatePairAwareTextIterator.h"
#include "text/TextFlags.h"
#include <hb-icu.h>
#include <hb-ot.h>
#include <hb.h>

namespace WebCore {

static inline float harfBuzzPositionToFloat(hb_position_t value)
{
    return static_cast<float>(value) / (1 << 16);
}

ComplexTextController::ComplexTextRun::ComplexTextRun(hb_buffer_t* buffer, const Font& font, std::span<const char16_t> characters, unsigned stringLocation, unsigned indexBegin, unsigned indexEnd)
    : m_initialAdvance(0, 0)
    , m_font(font)
    , m_characters(characters)
    , m_indexBegin(indexBegin)
    , m_indexEnd(indexEnd)
    , m_glyphCount(hb_buffer_get_length(buffer))
    , m_stringLocation(stringLocation)
    , m_isLTR(HB_DIRECTION_IS_FORWARD(hb_buffer_get_direction(buffer)))
    , m_textAutospaceSize(TextAutospace::textAutospaceSize(font))
{
    if (!m_glyphCount)
        return;

    m_glyphs.grow(m_glyphCount);
    m_baseAdvances.grow(m_glyphCount);
    m_glyphOrigins.grow(m_glyphCount);
    m_coreTextIndices.grow(m_glyphCount);

    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib/Win port
    hb_glyph_info_t* glyphInfos = hb_buffer_get_glyph_infos(buffer, nullptr);
    hb_glyph_position_t* glyphPositions = hb_buffer_get_glyph_positions(buffer, nullptr);
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

    // HarfBuzz returns the shaping result in visual order. We don't need to flip for RTL.
    for (unsigned i = 0; i < m_glyphCount; ++i) {
        m_coreTextIndices[i] = glyphInfos[i].cluster;

        uint16_t glyph = glyphInfos[i].codepoint;
        if (m_font->isZeroWidthSpaceGlyph(glyph) || !m_font->platformData().size()) {
            m_glyphs[i] = glyph;
            m_baseAdvances[i] = { };
            m_glyphOrigins[i] = { };
            continue;
        }

        float offsetX = harfBuzzPositionToFloat(glyphPositions[i].x_offset);
        float offsetY = harfBuzzPositionToFloat(glyphPositions[i].y_offset);
        float advanceX = harfBuzzPositionToFloat(glyphPositions[i].x_advance);
        float advanceY = harfBuzzPositionToFloat(glyphPositions[i].y_advance);

        m_glyphs[i] = glyph;
        m_baseAdvances[i] = { advanceX, advanceY };
        m_glyphOrigins[i] = { offsetX, offsetY };
    }
    m_initialAdvance = toFloatSize(m_glyphOrigins[0]);
}

static std::optional<UScriptCode> characterScript(char32_t character)
{
    UErrorCode errorCode = U_ZERO_ERROR;
    UScriptCode script = uscript_getScript(character, &errorCode);
    if (U_FAILURE(errorCode))
        return std::nullopt;
    return script;
}

struct HBRun {
    unsigned startIndex;
    unsigned endIndex;
    UScriptCode script;
};

static std::optional<HBRun> findNextRun(std::span<const char16_t> characters, unsigned offset)
{
    SurrogatePairAwareTextIterator textIterator(characters.subspan(offset), offset, characters.size());
    char32_t character;
    unsigned clusterLength = 0;
    if (!textIterator.consume(character, clusterLength))
        return std::nullopt;

    auto currentScript = characterScript(character);
    if (!currentScript)
        return std::nullopt;

    unsigned startIndex = offset;
    for (textIterator.advance(clusterLength); textIterator.consume(character, clusterLength); textIterator.advance(clusterLength)) {
        if (FontCascade::treatAsZeroWidthSpace(character))
            continue;

        auto nextScript = characterScript(character);
        if (!nextScript)
            return std::nullopt;

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
            return std::optional<HBRun>({ startIndex, textIterator.currentIndex(), currentScript.value() });
    }

    return std::optional<HBRun>({ startIndex, textIterator.currentIndex(), currentScript.value() });
}

struct LTR {
    size_t offset { 0 };
};

struct RTL {
    size_t offset { 0 };
    Vector<HBRun, 1> runList;
};

template <typename IterationData>
static void forEachHBRun(const std::span<const char16_t>& characters, Function<void(const HBRun&)>&& callback)
{
    IterationData data;

    while (data.offset < characters.size()) {
        auto run = findNextRun(characters, data.offset);
        if (!run)
            break;
        data.offset = run->endIndex;
        if constexpr (std::is_same_v<IterationData, LTR>)
            callback(*run);
        else
            data.runList.append(run.value());
    }

    if constexpr (std::is_same_v<IterationData, RTL>) {
        for (auto reverseIterator = data.runList.rbegin(); reverseIterator != data.runList.rend(); ++reverseIterator)
            callback(*reverseIterator);
    }
}

void ComplexTextController::collectComplexTextRunsForCharacters(std::span<const char16_t> characters, unsigned stringLocation, const Font* font)
{
    ASSERT(!characters.empty());

    if (!font) {
        // Create a run of missing glyphs from the primary font.
        m_complexTextRuns.append(ComplexTextRun::create(m_fontCascade->primaryFont(), characters, stringLocation, 0, characters.size(), m_run->ltr()));
        return;
    }

    const auto& fontPlatformData = font->platformData();
    auto* hbFont = fontPlatformData.hbFont();
    RELEASE_ASSERT(hbFont);

    const auto& features = fontPlatformData.features();
    // Kerning is not handled as font features, so only in case it's explicitly disabled
    // we need to create a new vector to include kern feature.
    const hb_feature_t* featuresData = features.isEmpty() ? nullptr : features.span().data();
    unsigned featuresSize = features.size();
    Vector<hb_feature_t> featuresWithKerning;
    if (!m_fontCascade->enableKerning()) {
        featuresWithKerning.reserveInitialCapacity(featuresSize + 1);
        static hb_feature_t kernFeature { HB_TAG('k', 'e', 'r', 'n'), 0, 0, static_cast<unsigned>(-1) };
        featuresWithKerning.append(kernFeature);
        featuresWithKerning.appendVector(features);
        featuresData = featuresWithKerning.span().data();
        featuresSize = featuresWithKerning.size();
    }

    static thread_local HbUniquePtr<hb_buffer_t> buffer(hb_buffer_create());

    // The computed "locale" equals the "lang" attribute. The latter must be a valid BCP 47 language tag,
    // according to <https://html.spec.whatwg.org/multipage/dom.html#attr-lang>.
    // This is exactly what hb_language_from_string() expects, so we can pass directly.
    ASSERT(m_fontCascade->fontDescription().computedLocale().is8Bit());
    auto language = hb_language_from_string(reinterpret_cast<const char*>(m_fontCascade->fontDescription().computedLocale().span8().data()), -1);

    auto shapeFunction = [&](const HBRun& run) {
        hb_buffer_set_language(buffer.get(), language);

        hb_buffer_set_script(buffer.get(), hb_icu_script_to_script(run.script));

        if (!m_mayUseNaturalWritingDirection || m_run->directionalOverride())
            hb_buffer_set_direction(buffer.get(), m_run->rtl() ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
        else
            hb_buffer_set_direction(buffer.get(), hb_script_get_horizontal_direction(hb_icu_script_to_script(run.script)));

        hb_buffer_add_utf16(buffer.get(), reinterpret_cast<const uint16_t*>(characters.data()), characters.size(), run.startIndex, run.endIndex - run.startIndex);

        hb_shape(hbFont, buffer.get(), featuresData, featuresSize);
        m_complexTextRuns.append(ComplexTextRun::create(buffer.get(), *font, characters, stringLocation, run.startIndex, run.endIndex));
        hb_buffer_reset(buffer.get());
    };
    if (m_run->ltr())
        forEachHBRun<LTR>(characters, WTFMove(shapeFunction));
    else
        forEachHBRun<RTL>(characters, WTFMove(shapeFunction));
}

} // namespace WebCore
