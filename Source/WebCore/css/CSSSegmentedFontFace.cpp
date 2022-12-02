/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
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
#include "CSSSegmentedFontFace.h"

#include "CSSFontFace.h"
#include "Font.h"
#include "FontCache.h"
#include "FontCreationContext.h"
#include "FontDescription.h"
#include "FontPaletteValues.h"
#include "FontSelector.h"

namespace WebCore {

CSSSegmentedFontFace::CSSSegmentedFontFace()
{
}

CSSSegmentedFontFace::~CSSSegmentedFontFace()
{
    for (auto& face : m_fontFaces)
        face->removeClient(*this);
}

void CSSSegmentedFontFace::appendFontFace(Ref<CSSFontFace>&& fontFace)
{
    m_cache.clear();
    fontFace->addClient(*this);
    m_fontFaces.append(WTFMove(fontFace));
}

void CSSSegmentedFontFace::fontLoaded(CSSFontFace&)
{
    m_cache.clear();
}

class CSSFontAccessor final : public FontAccessor {
public:
    static Ref<CSSFontAccessor> create(CSSFontFace& fontFace, const FontDescription& fontDescription, const FontPaletteValues& fontPaletteValues, RefPtr<FontFeatureValues> fontFeatureValues, bool syntheticBold, bool syntheticItalic)
    {
        return adoptRef(*new CSSFontAccessor(fontFace, fontDescription, fontPaletteValues, fontFeatureValues, syntheticBold, syntheticItalic));
    }

    const Font* font(ExternalResourceDownloadPolicy policy) const final
    {
        if (!m_result || (policy == ExternalResourceDownloadPolicy::Allow
            && (m_fontFace->status() == CSSFontFace::Status::Pending || m_fontFace->status() == CSSFontFace::Status::Loading || m_fontFace->status() == CSSFontFace::Status::TimedOut))) {
            const auto result = m_fontFace->font(m_fontDescription, m_syntheticBold, m_syntheticItalic, policy, m_fontPaletteValues, m_fontFeatureValues);
            if (!m_result)
                m_result = result;
        }
        return m_result.value().get();
    }

private:
    CSSFontAccessor(CSSFontFace& fontFace, const FontDescription& fontDescription, const FontPaletteValues& fontPaletteValues, RefPtr<FontFeatureValues> fontFeatureValues, bool syntheticBold, bool syntheticItalic)
        : m_fontFace(fontFace)
        , m_fontDescription(fontDescription)
        , m_fontFeatureValues(fontFeatureValues)
        , m_fontPaletteValues(fontPaletteValues)
        , m_syntheticBold(syntheticBold)
        , m_syntheticItalic(syntheticItalic)
    {
    }

    bool isLoading() const final
    {
        return m_result && m_result.value() && m_result.value()->isInterstitial();
    }

    mutable std::optional<RefPtr<Font>> m_result; // Caches nullptr too
    mutable Ref<CSSFontFace> m_fontFace;
    FontDescription m_fontDescription;
    RefPtr<FontFeatureValues> m_fontFeatureValues;
    FontPaletteValues m_fontPaletteValues;
    bool m_syntheticBold;
    bool m_syntheticItalic;
};

static void appendFont(FontRanges& ranges, Ref<FontAccessor>&& fontAccessor, const Vector<CSSFontFace::UnicodeRange>& unicodeRanges)
{
    if (unicodeRanges.isEmpty()) {
        ranges.appendRange({ 0, 0x7FFFFFFF, WTFMove(fontAccessor) });
        return;
    }

    for (auto& range : unicodeRanges)
        ranges.appendRange({ range.from, range.to, fontAccessor.copyRef() });
}

FontRanges CSSSegmentedFontFace::fontRanges(const FontDescription& fontDescription, const FontPaletteValues& fontPaletteValues, RefPtr<FontFeatureValues> fontFeatureValues)
{
    auto addResult = m_cache.add(std::make_tuple(FontDescriptionKey(fontDescription), fontPaletteValues), FontRanges());
    auto& ranges = addResult.iterator->value;

    if (!addResult.isNewEntry)
        return ranges;

    auto desiredRequest = fontDescription.fontSelectionRequest();

    for (auto& face : m_fontFaces) {
        if (face->computeFailureState())
            continue;

        auto selectionCapabilities = face->fontSelectionCapabilities();

        bool syntheticBold = fontDescription.hasAutoFontSynthesisWeight() && !isFontWeightBold(selectionCapabilities.weight.maximum) && isFontWeightBold(desiredRequest.weight);
        bool syntheticItalic = fontDescription.hasAutoFontSynthesisStyle() && !isItalic(selectionCapabilities.slope.maximum) && isItalic(desiredRequest.slope);

        // Metrics used for layout come from FontRanges::fontForFirstRange(), which assumes that the first font is non-null.
        auto fontAccessor = CSSFontAccessor::create(face, fontDescription, fontPaletteValues, fontFeatureValues, syntheticBold, syntheticItalic);
        if (ranges.isNull() && !fontAccessor->font(ExternalResourceDownloadPolicy::Forbid))
            continue;
        
        appendFont(ranges, WTFMove(fontAccessor), face->ranges());
    }
    
    ranges.shrinkToFit();
    return ranges;
}

}
