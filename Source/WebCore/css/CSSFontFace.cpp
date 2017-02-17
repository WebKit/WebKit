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
#include "CSSFontFaceSrcValue.h"
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
#include "FontFace.h"
#include "FontVariantBuilder.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include "StyleProperties.h"
#include "StyleRule.h"

namespace WebCore {

template<typename T> void iterateClients(HashSet<CSSFontFace::Client*>& clients, T callback)
{
    Vector<Ref<CSSFontFace::Client>> clientsCopy;
    clientsCopy.reserveInitialCapacity(clients.size());
    for (auto* client : clients)
        clientsCopy.uncheckedAppend(*client);

    for (auto* client : clients)
        callback(*client);
}

void CSSFontFace::appendSources(CSSFontFace& fontFace, CSSValueList& srcList, Document* document, bool isInitiatingElementInUserAgentShadowTree)
{
    for (auto& src : srcList) {
        // An item in the list either specifies a string (local font name) or a URL (remote font to download).
        CSSFontFaceSrcValue& item = downcast<CSSFontFaceSrcValue>(src.get());
        std::unique_ptr<CSSFontFaceSource> source;
        SVGFontFaceElement* fontFaceElement = nullptr;
        bool foundSVGFont = false;

#if ENABLE(SVG_FONTS)
        foundSVGFont = item.isSVGFontFaceSrc() || item.svgFontFaceElement();
        fontFaceElement = item.svgFontFaceElement();
#endif
        if (!item.isLocal()) {
            const Settings* settings = document ? &document->settings() : nullptr;
            bool allowDownloading = foundSVGFont || (settings && settings->downloadableBinaryFontsEnabled());
            if (allowDownloading && item.isSupportedFormat() && document) {
                if (CachedFont* cachedFont = item.cachedFont(document, foundSVGFont, isInitiatingElementInUserAgentShadowTree))
                    source = std::make_unique<CSSFontFaceSource>(fontFace, item.resource(), cachedFont);
            }
        } else
            source = std::make_unique<CSSFontFaceSource>(fontFace, item.resource(), nullptr, fontFaceElement);

        if (source)
            fontFace.adoptSource(WTFMove(source));
    }
    fontFace.sourcesPopulated();
}

CSSFontFace::CSSFontFace(CSSFontSelector* fontSelector, StyleRuleFontFace* cssConnection, FontFace* wrapper, bool isLocalFallback)
    : m_timeoutTimer(*this, &CSSFontFace::timeoutFired)
    , m_fontSelector(fontSelector)
    , m_cssConnection(cssConnection)
    , m_wrapper(wrapper ? wrapper->createWeakPtr() : WeakPtr<FontFace>())
    , m_isLocalFallback(isLocalFallback)
    , m_mayBePurged(!wrapper)
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

    RefPtr<CSSValueList> oldFamilies = m_families;
    m_families = &familyList;

    if (m_cssConnection)
        m_cssConnection->mutableProperties().setProperty(CSSPropertyFontFamily, &family);

    iterateClients(m_clients, [&](Client& client) {
        client.fontPropertyChanged(*this, oldFamilies.get());
    });

    return true;
}

std::optional<FontTraitsMask> CSSFontFace::calculateStyleMask(CSSValue& style)
{
    if (!is<CSSPrimitiveValue>(style))
        return std::nullopt;

    switch (downcast<CSSPrimitiveValue>(style).valueID()) {
    case CSSValueNormal:
        return FontStyleNormalMask;
    case CSSValueItalic:
    case CSSValueOblique:
        return FontStyleItalicMask;
    default:
        return FontStyleNormalMask;
    }

    return FontStyleNormalMask;
}

bool CSSFontFace::setStyle(CSSValue& style)
{
    if (auto mask = calculateStyleMask(style)) {
        m_traitsMask = static_cast<FontTraitsMask>((static_cast<unsigned>(m_traitsMask) & (~FontStyleMask)) | mask.value());

        if (m_cssConnection)
            m_cssConnection->mutableProperties().setProperty(CSSPropertyFontStyle, &style);

        iterateClients(m_clients, [&](Client& client) {
            client.fontPropertyChanged(*this);
        });

        return true;
    }
    return false;
}

std::optional<FontTraitsMask> CSSFontFace::calculateWeightMask(CSSValue& weight)
{
    if (!is<CSSPrimitiveValue>(weight))
        return std::nullopt;

    switch (downcast<CSSPrimitiveValue>(weight).valueID()) {
    case CSSValueBold:
    case CSSValueBolder:
    case CSSValue700:
        return FontWeight700Mask;
    case CSSValueNormal:
    case CSSValue400:
        return FontWeight400Mask;
    case CSSValue900:
        return FontWeight900Mask;
    case CSSValue800:
        return FontWeight800Mask;
    case CSSValue600:
        return FontWeight600Mask;
    case CSSValue500:
        return FontWeight500Mask;
    case CSSValue300:
        return FontWeight300Mask;
    case CSSValueLighter:
    case CSSValue200:
        return FontWeight200Mask;
    case CSSValue100:
        return FontWeight100Mask;
    default:
        return FontWeight400Mask;
    }

    return FontWeight400Mask;
}

bool CSSFontFace::setWeight(CSSValue& weight)
{
    if (auto mask = calculateWeightMask(weight)) {
        m_traitsMask = static_cast<FontTraitsMask>((static_cast<unsigned>(m_traitsMask) & (~FontWeightMask)) | mask.value());

        if (m_cssConnection)
            m_cssConnection->mutableProperties().setProperty(CSSPropertyFontWeight, &weight);

        iterateClients(m_clients, [&](Client& client) {
            client.fontPropertyChanged(*this);
        });

        return true;
    }

    return false;
}

bool CSSFontFace::setUnicodeRange(CSSValue& unicodeRange)
{
    if (!is<CSSValueList>(unicodeRange))
        return false;

    m_ranges.clear();
    auto& list = downcast<CSSValueList>(unicodeRange);
    for (auto& rangeValue : list) {
        auto& range = downcast<CSSUnicodeRangeValue>(rangeValue.get());
        m_ranges.append({ range.from(), range.to() });
    }

    if (m_cssConnection)
        m_cssConnection->mutableProperties().setProperty(CSSPropertyUnicodeRange, &unicodeRange);

    iterateClients(m_clients, [&](Client& client) {
        client.fontPropertyChanged(*this);
    });

    return true;
}

bool CSSFontFace::setVariantLigatures(CSSValue& variantLigatures)
{
    auto ligatures = extractFontVariantLigatures(variantLigatures);

    m_variantSettings.commonLigatures = ligatures.commonLigatures;
    m_variantSettings.discretionaryLigatures = ligatures.discretionaryLigatures;
    m_variantSettings.historicalLigatures = ligatures.historicalLigatures;
    m_variantSettings.contextualAlternates = ligatures.contextualAlternates;

    if (m_cssConnection)
        m_cssConnection->mutableProperties().setProperty(CSSPropertyFontVariantLigatures, &variantLigatures);

    iterateClients(m_clients, [&](Client& client) {
        client.fontPropertyChanged(*this);
    });

    return true;
}

bool CSSFontFace::setVariantPosition(CSSValue& variantPosition)
{
    if (!is<CSSPrimitiveValue>(variantPosition))
        return false;

    m_variantSettings.position = downcast<CSSPrimitiveValue>(variantPosition);

    if (m_cssConnection)
        m_cssConnection->mutableProperties().setProperty(CSSPropertyFontVariantPosition, &variantPosition);

    iterateClients(m_clients, [&](Client& client) {
        client.fontPropertyChanged(*this);
    });

    return true;
}

bool CSSFontFace::setVariantCaps(CSSValue& variantCaps)
{
    if (!is<CSSPrimitiveValue>(variantCaps))
        return false;

    m_variantSettings.caps = downcast<CSSPrimitiveValue>(variantCaps);

    if (m_cssConnection)
        m_cssConnection->mutableProperties().setProperty(CSSPropertyFontVariantCaps, &variantCaps);

    iterateClients(m_clients, [&](Client& client) {
        client.fontPropertyChanged(*this);
    });

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

    if (m_cssConnection)
        m_cssConnection->mutableProperties().setProperty(CSSPropertyFontVariantNumeric, &variantNumeric);

    iterateClients(m_clients, [&](Client& client) {
        client.fontPropertyChanged(*this);
    });

    return true;
}

bool CSSFontFace::setVariantAlternates(CSSValue& variantAlternates)
{
    if (!is<CSSPrimitiveValue>(variantAlternates))
        return false;

    m_variantSettings.alternates = downcast<CSSPrimitiveValue>(variantAlternates);

    if (m_cssConnection)
        m_cssConnection->mutableProperties().setProperty(CSSPropertyFontVariantAlternates, &variantAlternates);

    iterateClients(m_clients, [&](Client& client) {
        client.fontPropertyChanged(*this);
    });

    return true;
}

bool CSSFontFace::setVariantEastAsian(CSSValue& variantEastAsian)
{
    auto eastAsian = extractFontVariantEastAsian(variantEastAsian);

    m_variantSettings.eastAsianVariant = eastAsian.variant;
    m_variantSettings.eastAsianWidth = eastAsian.width;
    m_variantSettings.eastAsianRuby = eastAsian.ruby;

    if (m_cssConnection)
        m_cssConnection->mutableProperties().setProperty(CSSPropertyFontVariantEastAsian, &variantEastAsian);

    iterateClients(m_clients, [&](Client& client) {
        client.fontPropertyChanged(*this);
    });

    return true;
}

void CSSFontFace::setFeatureSettings(CSSValue& featureSettings)
{
    // Can only call this with a primitive value of normal, or a value list containing font feature values.
    ASSERT(is<CSSPrimitiveValue>(featureSettings) || is<CSSValueList>(featureSettings));

    FontFeatureSettings settings;

    if (is<CSSValueList>(featureSettings)) {
        auto& list = downcast<CSSValueList>(featureSettings);
        for (auto& rangeValue : list) {
            auto& feature = downcast<CSSFontFeatureValue>(rangeValue.get());
            settings.insert({ feature.tag(), feature.value() });
        }
    }

    if (m_featureSettings == settings)
        return;

    m_featureSettings = WTFMove(settings);

    if (m_cssConnection)
        m_cssConnection->mutableProperties().setProperty(CSSPropertyFontFeatureSettings, &featureSettings);

    iterateClients(m_clients, [&](Client& client) {
        client.fontPropertyChanged(*this);
    });
}

bool CSSFontFace::rangesMatchCodePoint(UChar32 character) const
{
    if (m_ranges.isEmpty())
        return true;

    for (auto& range : m_ranges) {
        if (range.from <= character && character <= range.to)
            return true;
    }
    return false;
}

void CSSFontFace::fontLoadEventOccurred()
{
    Ref<CSSFontFace> protectedThis(*this);

    // If the font is already in the cache, CSSFontFaceSource may report it's loaded before it is added here as a source.
    // Let's not pump the state machine until we've got all our sources. font() and load() are smart enough to act correctly
    // when a source is failed or succeeded before we have asked it to load.
    if (m_sourcesPopulated)
        pump();

    ASSERT(m_fontSelector);
    m_fontSelector->fontLoaded();

    iterateClients(m_clients, [&](Client& client) {
        client.fontLoaded(*this);
    });
}

void CSSFontFace::timeoutFired()
{
    setStatus(Status::TimedOut);

    fontLoadEventOccurred();
}

bool CSSFontFace::allSourcesFailed() const
{
    for (auto& source : m_sources) {
        if (source->status() != CSSFontFaceSource::Status::Failure)
            return false;
    }
    return true;
}

void CSSFontFace::addClient(Client& client)
{
    m_clients.add(&client);
}

void CSSFontFace::removeClient(Client& client)
{
    ASSERT(m_clients.contains(&client));
    m_clients.remove(&client);
}

void CSSFontFace::initializeWrapper()
{
    switch (m_status) {
    case Status::Pending:
        break;
    case Status::Loading:
        m_wrapper->fontStateChanged(*this, Status::Pending, Status::Loading);
        break;
    case Status::TimedOut:
        m_wrapper->fontStateChanged(*this, Status::Pending, Status::Loading);
        m_wrapper->fontStateChanged(*this, Status::Loading, Status::TimedOut);
        break;
    case Status::Success:
        m_wrapper->fontStateChanged(*this, Status::Pending, Status::Loading);
        m_wrapper->fontStateChanged(*this, Status::Pending, Status::Success);
        break;
    case Status::Failure:
        m_wrapper->fontStateChanged(*this, Status::Pending, Status::Loading);
        m_wrapper->fontStateChanged(*this, Status::Pending, Status::Failure);
        break;
    }
    m_mayBePurged = false;
}

Ref<FontFace> CSSFontFace::wrapper()
{
    if (m_wrapper)
        return *m_wrapper.get();

    auto wrapper = FontFace::create(*this);
    m_wrapper = wrapper->createWeakPtr();
    initializeWrapper();
    return wrapper;
}

void CSSFontFace::setWrapper(FontFace& newWrapper)
{
    m_wrapper = newWrapper.createWeakPtr();
    initializeWrapper();
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

    if (newStatus == Status::Loading)
        m_timeoutTimer.startOneShot(webFontsShouldAlwaysFallBack() ? 0 : 3);
    else if (newStatus == Status::Success || newStatus == Status::Failure)
        m_timeoutTimer.stop();

    iterateClients(m_clients, [&](Client& client) {
        client.fontStateChanged(*this, m_status, newStatus);
    });

    m_status = newStatus;
}

void CSSFontFace::fontLoaded(CSSFontFaceSource&)
{
    ASSERT(!webFontsShouldAlwaysFallBack());

    fontLoadEventOccurred();
}

bool CSSFontFace::webFontsShouldAlwaysFallBack() const
{
    return m_fontSelector && m_fontSelector->document() && m_fontSelector->document()->settings().webFontsAlwaysFallBack();
}

size_t CSSFontFace::pump()
{
    size_t i;
    for (i = 0; i < m_sources.size(); ++i) {
        auto& source = m_sources[i];

        if (source->status() == CSSFontFaceSource::Status::Pending) {
            ASSERT(m_status == Status::Pending || m_status == Status::Loading || m_status == Status::TimedOut);
            ASSERT(m_fontSelector);
            if (m_status == Status::Pending)
                setStatus(Status::Loading);
            source->load(*m_fontSelector);
        }

        switch (source->status()) {
        case CSSFontFaceSource::Status::Pending:
            ASSERT_NOT_REACHED();
            break;
        case CSSFontFaceSource::Status::Loading:
            ASSERT(m_status == Status::Pending || m_status == Status::Loading || m_status == Status::TimedOut);
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
    if (m_sources.isEmpty() && m_status == Status::Pending)
        setStatus(Status::Loading);
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
    bool fontIsLoading = false;
    for (size_t i = startIndex; i < m_sources.size(); ++i) {
        auto& source = m_sources[i];
        if (source->status() == CSSFontFaceSource::Status::Pending) {
            ASSERT(m_fontSelector);
            if (fontIsLoading)
                continue;
            source->load(*m_fontSelector);
        }

        switch (source->status()) {
        case CSSFontFaceSource::Status::Pending:
            ASSERT_NOT_REACHED();
            break;
        case CSSFontFaceSource::Status::Loading:
            ASSERT(!fontIsLoading);
            fontIsLoading = true;
            if (status() == Status::TimedOut)
                continue;
            return Font::create(FontCache::singleton().lastResortFallbackFontForEveryCharacter(fontDescription)->platformData(), true, true);
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

bool CSSFontFace::purgeable() const
{
    return cssConnection() && m_mayBePurged;
}

void CSSFontFace::updateStyleIfNeeded()
{
    if (m_fontSelector && m_fontSelector->document())
        m_fontSelector->document()->updateStyleIfNeeded();
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
