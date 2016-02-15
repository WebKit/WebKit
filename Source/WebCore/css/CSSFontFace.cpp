/*
 * Copyright (C) 2007, 2008, 2011, 2013 Apple Inc. All rights reserved.
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
#include "CSSFontFace.h"

#include "CSSFontFaceSource.h"
#include "CSSFontFamily.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontSelector.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSSegmentedFontFace.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSValue.h"
#include "CSSValueList.h"
#include "Document.h"
#include "Font.h"
#include "FontDescription.h"
#include "FontLoader.h"
#include "FontVariantBuilder.h"
#include "RuntimeEnabledFeatures.h"
#include "StyleProperties.h"

namespace WebCore {

CSSFontFace::CSSFontFace(CSSFontFaceClient& client, CSSFontSelector& fontSelector, bool isLocalFallback)
    : m_fontSelector(fontSelector)
    , m_client(client)
    , m_isLocalFallback(isLocalFallback)
{
}

CSSFontFace::~CSSFontFace()
{
}

bool CSSFontFace::setFamilies(CSSValue& family)
{
    if (!is<CSSValueList>(family))
        return false;

    CSSValueList& familyList = downcast<CSSValueList>(family);
    if (!familyList.length())
        return false;

    m_families = &familyList;
    return true;
}

bool CSSFontFace::setStyle(CSSValue& style)
{
    if (!is<CSSPrimitiveValue>(style))
        return false;

    unsigned styleMask = 0;
    switch (downcast<CSSPrimitiveValue>(style).getValueID()) {
    case CSSValueNormal:
        styleMask = FontStyleNormalMask;
        break;
    case CSSValueItalic:
    case CSSValueOblique:
        styleMask = FontStyleItalicMask;
        break;
    default:
        styleMask = FontStyleNormalMask;
        break;
    }

    m_traitsMask = static_cast<FontTraitsMask>((static_cast<unsigned>(m_traitsMask) & (~FontStyleMask)) | styleMask);
    return true;
}

bool CSSFontFace::setWeight(CSSValue& weight)
{
    if (!is<CSSPrimitiveValue>(weight))
        return false;

    unsigned weightMask = 0;
    switch (downcast<CSSPrimitiveValue>(weight).getValueID()) {
    case CSSValueBold:
    case CSSValueBolder:
    case CSSValue700:
        weightMask = FontWeight700Mask;
        break;
    case CSSValueNormal:
    case CSSValue400:
        weightMask = FontWeight400Mask;
        break;
    case CSSValue900:
        weightMask = FontWeight900Mask;
        break;
    case CSSValue800:
        weightMask = FontWeight800Mask;
        break;
    case CSSValue600:
        weightMask = FontWeight600Mask;
        break;
    case CSSValue500:
        weightMask = FontWeight500Mask;
        break;
    case CSSValue300:
        weightMask = FontWeight300Mask;
        break;
    case CSSValueLighter:
    case CSSValue200:
        weightMask = FontWeight200Mask;
        break;
    case CSSValue100:
        weightMask = FontWeight100Mask;
        break;
    default:
        weightMask = FontWeight400Mask;
        break;
    }

    m_traitsMask = static_cast<FontTraitsMask>((static_cast<unsigned>(m_traitsMask) & (~FontWeightMask)) | weightMask);
    return true;
}

bool CSSFontFace::setUnicodeRange(CSSValue& unicodeRange)
{
    if (!is<CSSValueList>(unicodeRange))
        return false;

    m_ranges.clear();
    auto& list = downcast<CSSValueList>(unicodeRange);
    for (auto& rangeValue : list) {
        CSSUnicodeRangeValue& range = downcast<CSSUnicodeRangeValue>(rangeValue.get());
        m_ranges.append(UnicodeRange(range.from(), range.to()));
    }
    return true;
}

bool CSSFontFace::setVariantLigatures(CSSValue& variantLigatures)
{
    auto ligatures = extractFontVariantLigatures(variantLigatures);
    m_variantSettings.commonLigatures = ligatures.commonLigatures;
    m_variantSettings.discretionaryLigatures = ligatures.discretionaryLigatures;
    m_variantSettings.historicalLigatures = ligatures.historicalLigatures;
    m_variantSettings.contextualAlternates = ligatures.contextualAlternates;
    return true;
}

bool CSSFontFace::setVariantPosition(CSSValue& variantPosition)
{
    if (!is<CSSPrimitiveValue>(variantPosition))
        return false;
    m_variantSettings.position = downcast<CSSPrimitiveValue>(variantPosition);
    return true;
}

bool CSSFontFace::setVariantCaps(CSSValue& variantCaps)
{
    if (!is<CSSPrimitiveValue>(variantCaps))
        return false;
    m_variantSettings.caps = downcast<CSSPrimitiveValue>(variantCaps);
    return true;
}

bool CSSFontFace::setVariantNumeric(CSSValue& variantNumeric)
{
    auto numeric = extractFontVariantNumeric(variantNumeric);
    m_variantSettings.numericFigure = numeric.figure;
    m_variantSettings.numericSpacing = numeric.spacing;
    m_variantSettings.numericFraction = numeric.fraction;
    m_variantSettings.numericOrdinal = numeric.ordinal;
    m_variantSettings.numericSlashedZero = numeric.slashedZero;
    return true;
}

bool CSSFontFace::setVariantAlternates(CSSValue& variantAlternates)
{
    if (!is<CSSPrimitiveValue>(variantAlternates))
        return false;
    m_variantSettings.alternates = downcast<CSSPrimitiveValue>(variantAlternates);
    return true;
}

bool CSSFontFace::setVariantEastAsian(CSSValue& variantEastAsian)
{
    auto eastAsian = extractFontVariantEastAsian(variantEastAsian);
    m_variantSettings.eastAsianVariant = eastAsian.variant;
    m_variantSettings.eastAsianWidth = eastAsian.width;
    m_variantSettings.eastAsianRuby = eastAsian.ruby;
    return true;
}

bool CSSFontFace::setFeatureSettings(CSSValue& featureSettings)
{
    if (!is<CSSValueList>(featureSettings))
        return false;

    m_featureSettings = FontFeatureSettings();
    auto& list = downcast<CSSValueList>(featureSettings);
    for (auto& rangeValue : list) {
        CSSFontFeatureValue& feature = downcast<CSSFontFeatureValue>(rangeValue.get());
        m_featureSettings.insert(FontFeature(feature.tag(), feature.value()));
    }
    return true;
}

bool CSSFontFace::allSourcesFailed() const
{
    for (auto& source : m_sources) {
        if (source->status() != CSSFontFaceSource::Status::Failure)
            return false;
    }
    return true;
}

void CSSFontFace::addedToSegmentedFontFace(CSSSegmentedFontFace& segmentedFontFace)
{
    m_segmentedFontFaces.add(&segmentedFontFace);
}

void CSSFontFace::removedFromSegmentedFontFace(CSSSegmentedFontFace& segmentedFontFace)
{
    m_segmentedFontFaces.remove(&segmentedFontFace);
}

void CSSFontFace::adoptSource(std::unique_ptr<CSSFontFaceSource>&& source)
{
    m_sources.append(WTFMove(source));

    // We should never add sources in the middle of loading.
    ASSERT(!m_sourcesPopulated);
}

void CSSFontFace::setStatus(Status newStatus)
{
    switch (newStatus) {
    case Status::Pending:
        ASSERT_NOT_REACHED();
        break;
    case Status::Loading:
        ASSERT(m_status == Status::Pending);
        break;
    case Status::TimedOut:
        ASSERT(m_status == Status::Loading);
        break;
    case Status::Success:
        ASSERT(m_status == Status::Loading || m_status == Status::TimedOut);
        break;
    case Status::Failure:
        ASSERT(m_status == Status::Loading || m_status == Status::TimedOut);
        break;
    }

    m_status = newStatus;

    if (m_status == Status::Success || m_status == Status::Failure)
        m_client.kick(*this);
}

void CSSFontFace::fontLoaded(CSSFontFaceSource&)
{
    // If the font is already in the cache, CSSFontFaceSource may report it's loaded before it is added here as a source.
    // Let's not pump the state machine until we've got all our sources. font() and load() are smart enough to act correctly
    // when a source is failed or succeeded before we have asked it to load.
    if (m_sourcesPopulated)
        pump();

    if (m_segmentedFontFaces.isEmpty())
        return;

    m_fontSelector->fontLoaded();

    for (auto* face : m_segmentedFontFaces)
        face->fontLoaded(*this);
}

size_t CSSFontFace::pump()
{
    size_t i;
    for (i = 0; i < m_sources.size(); ++i) {
        auto& source = m_sources[i];

        if (source->status() == CSSFontFaceSource::Status::Pending) {
            ASSERT(m_status == Status::Pending || m_status == Status::Loading || m_status == Status::TimedOut);
            if (m_status == Status::Pending)
                setStatus(Status::Loading);
            source->load(m_fontSelector.get());
        }

        switch (source->status()) {
        case CSSFontFaceSource::Status::Pending:
            ASSERT_NOT_REACHED();
            break;
        case CSSFontFaceSource::Status::Loading:
            ASSERT(m_status == Status::Pending || m_status == Status::Loading);
            if (m_status == Status::Pending)
                setStatus(Status::Loading);
            return i;
        case CSSFontFaceSource::Status::Success:
            ASSERT(m_status == Status::Pending || m_status == Status::Loading || m_status == Status::TimedOut || m_status == Status::Success);
            if (m_status == Status::Pending)
                setStatus(Status::Loading);
            if (m_status == Status::Loading || m_status == Status::TimedOut)
                setStatus(Status::Success);
            return i;
        case CSSFontFaceSource::Status::Failure:
            if (m_status == Status::Pending)
                setStatus(Status::Loading);
            break;
        }
    }
    if (m_status == Status::Loading || m_status == Status::TimedOut)
        setStatus(Status::Failure);
    return m_sources.size();
}

void CSSFontFace::load()
{
    pump();
}

RefPtr<Font> CSSFontFace::font(const FontDescription& fontDescription, bool syntheticBold, bool syntheticItalic)
{
    if (allSourcesFailed())
        return nullptr;

    // Our status is derived from the first non-failed source. However, this source may
    // return null from font(), which means we need to continue looping through the remainder
    // of the sources to try to find a font to use. These subsequent tries should not affect
    // our own state, though.
    size_t startIndex = pump();
    for (size_t i = startIndex; i < m_sources.size(); ++i) {
        auto& source = m_sources[i];
        if (source->status() == CSSFontFaceSource::Status::Pending)
            source->load(m_fontSelector.get());

        switch (source->status()) {
        case CSSFontFaceSource::Status::Pending:
            ASSERT_NOT_REACHED();
            break;
        case CSSFontFaceSource::Status::Loading:
            return Font::create(FontCache::singleton().lastResortFallbackFont(fontDescription)->platformData(), true, true);
        case CSSFontFaceSource::Status::Success:
            if (RefPtr<Font> result = source->font(fontDescription, syntheticBold, syntheticItalic, m_featureSettings, m_variantSettings))
                return result;
            break;
        case CSSFontFaceSource::Status::Failure:
            break;
        }
    }

    return nullptr;
}

#if ENABLE(SVG_FONTS)
bool CSSFontFace::hasSVGFontFaceSource() const
{
    size_t size = m_sources.size();
    for (size_t i = 0; i < size; i++) {
        if (m_sources[i]->isSVGFontFaceSource())
            return true;
    }
    return false;
}
#endif

}
