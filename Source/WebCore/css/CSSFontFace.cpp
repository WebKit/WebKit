/*
 * Copyright (C) 2007-2019 Apple Inc. All rights reserved.
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
#include "CSSFontFeatureValue.h"
#include "CSSFontSelector.h"
#include "CSSFontStyleRangeValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSValue.h"
#include "CSSValueList.h"
#include "CachedFont.h"
#include "Document.h"
#include "Font.h"
#include "FontCache.h"
#include "FontDescription.h"
#include "FontFace.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "StyleBuilderConverter.h"
#include "StyleProperties.h"
#include "StyleRule.h"

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSFontFace);

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

        foundSVGFont = item.isSVGFontFaceSrc() || item.svgFontFaceElement();
        fontFaceElement = item.svgFontFaceElement();
        if (!item.isLocal()) {
            const Settings* settings = document ? &document->settings() : nullptr;
            bool allowDownloading = foundSVGFont || (settings && settings->downloadableBinaryFontsEnabled());
            if (allowDownloading && item.isSupportedFormat() && document) {
                if (CachedFont* cachedFont = item.cachedFont(document, foundSVGFont, isInitiatingElementInUserAgentShadowTree))
                    source = makeUnique<CSSFontFaceSource>(fontFace, item.resource(), cachedFont);
            }
        } else
            source = makeUnique<CSSFontFaceSource>(fontFace, item.resource(), nullptr, fontFaceElement);

        if (source)
            fontFace.adoptSource(WTFMove(source));
    }
    fontFace.sourcesPopulated();
}

CSSFontFace::CSSFontFace(CSSFontSelector* fontSelector, StyleRuleFontFace* cssConnection, FontFace* wrapper, bool isLocalFallback)
    : m_fontSelector(makeWeakPtr(fontSelector))
    , m_cssConnection(cssConnection)
    , m_wrapper(makeWeakPtr(wrapper))
    , m_isLocalFallback(isLocalFallback)
    , m_mayBePurged(!wrapper)
    , m_timeoutTimer(*this, &CSSFontFace::timeoutFired)
{
}

CSSFontFace::~CSSFontFace() = default;

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

FontFace* CSSFontFace::existingWrapper()
{
    return m_wrapper.get();
}

static FontSelectionRange calculateWeightRange(CSSValue& value)
{
    if (value.isValueList()) {
        auto& valueList = downcast<CSSValueList>(value);
        ASSERT(valueList.length() == 2);
        if (valueList.length() != 2)
            return { normalWeightValue(), normalWeightValue() };
        ASSERT(valueList.item(0)->isPrimitiveValue());
        ASSERT(valueList.item(1)->isPrimitiveValue());
        auto& value0 = downcast<CSSPrimitiveValue>(*valueList.item(0));
        auto& value1 = downcast<CSSPrimitiveValue>(*valueList.item(1));
        auto result0 = Style::BuilderConverter::convertFontWeightFromValue(value0);
        auto result1 = Style::BuilderConverter::convertFontWeightFromValue(value1);
        return { result0, result1 };
    }

    ASSERT(is<CSSPrimitiveValue>(value));
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    FontSelectionValue result = Style::BuilderConverter::convertFontWeightFromValue(primitiveValue);
    return { result, result };
}

void CSSFontFace::setWeight(CSSValue& weight)
{
    auto range = calculateWeightRange(weight);
    if (m_fontSelectionCapabilities.weight == range)
        return;

    setWeight(range);

    if (m_cssConnection)
        m_cssConnection->mutableProperties().setProperty(CSSPropertyFontWeight, &weight);

    iterateClients(m_clients, [&](Client& client) {
        client.fontPropertyChanged(*this);
    });
}

static FontSelectionRange calculateStretchRange(CSSValue& value)
{
    if (value.isValueList()) {
        auto& valueList = downcast<CSSValueList>(value);
        ASSERT(valueList.length() == 2);
        if (valueList.length() != 2)
            return { normalStretchValue(), normalStretchValue() };
        ASSERT(valueList.item(0)->isPrimitiveValue());
        ASSERT(valueList.item(1)->isPrimitiveValue());
        auto& value0 = downcast<CSSPrimitiveValue>(*valueList.item(0));
        auto& value1 = downcast<CSSPrimitiveValue>(*valueList.item(1));
        auto result0 = Style::BuilderConverter::convertFontStretchFromValue(value0);
        auto result1 = Style::BuilderConverter::convertFontStretchFromValue(value1);
        return { result0, result1 };
    }

    ASSERT(is<CSSPrimitiveValue>(value));
    const auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    FontSelectionValue result = Style::BuilderConverter::convertFontStretchFromValue(primitiveValue);
    return { result, result };
}

void CSSFontFace::setStretch(CSSValue& style)
{
    auto range = calculateStretchRange(style);
    if (m_fontSelectionCapabilities.width == range)
        return;

    setStretch(range);

    if (m_cssConnection)
        m_cssConnection->mutableProperties().setProperty(CSSPropertyFontStretch, &style);

    iterateClients(m_clients, [&](Client& client) {
        client.fontPropertyChanged(*this);
    });
}

static FontSelectionRange calculateItalicRange(CSSValue& value)
{
    if (value.isFontStyleValue()) {
        auto result = Style::BuilderConverter::convertFontStyleFromValue(value);
        return { result.valueOr(normalItalicValue()), result.valueOr(normalItalicValue()) };
    }

    ASSERT(value.isFontStyleRangeValue());
    auto& rangeValue = downcast<CSSFontStyleRangeValue>(value);
    ASSERT(rangeValue.fontStyleValue->isValueID());
    auto valueID = rangeValue.fontStyleValue->valueID();
    if (!rangeValue.obliqueValues) {
        if (valueID == CSSValueNormal)
            return { normalItalicValue(), normalItalicValue() };
        ASSERT(valueID == CSSValueItalic || valueID == CSSValueOblique);
        return { italicValue(), italicValue() };
    }
    ASSERT(valueID == CSSValueOblique);
    auto length = rangeValue.obliqueValues->length();
    if (length == 1) {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(*rangeValue.obliqueValues->item(0));
        FontSelectionValue result(primitiveValue.value<float>(CSSUnitType::CSS_DEG));
        return { result, result };
    }
    ASSERT(length == 2);
    auto& primitiveValue1 = downcast<CSSPrimitiveValue>(*rangeValue.obliqueValues->item(0));
    auto& primitiveValue2 = downcast<CSSPrimitiveValue>(*rangeValue.obliqueValues->item(1));
    FontSelectionValue result1(primitiveValue1.value<float>(CSSUnitType::CSS_DEG));
    FontSelectionValue result2(primitiveValue2.value<float>(CSSUnitType::CSS_DEG));
    return { result1, result2 };
}

void CSSFontFace::setStyle(CSSValue& style)
{
    auto range = calculateItalicRange(style);
    if (m_fontSelectionCapabilities.slope == range)
        return;

    setStyle(range);

    if (m_cssConnection)
        m_cssConnection->mutableProperties().setProperty(CSSPropertyFontStyle, &style);

    iterateClients(m_clients, [&](Client& client) {
        client.fontPropertyChanged(*this);
    });
}

bool CSSFontFace::setUnicodeRange(CSSValue& unicodeRange)
{
    if (!is<CSSValueList>(unicodeRange))
        return false;

    Vector<UnicodeRange> ranges;
    auto& list = downcast<CSSValueList>(unicodeRange);
    for (auto& rangeValue : list) {
        auto& range = downcast<CSSUnicodeRangeValue>(rangeValue.get());
        ranges.append({ range.from(), range.to() });
    }

    if (ranges == m_ranges)
        return true;

    m_ranges = WTFMove(ranges);

    if (m_cssConnection)
        m_cssConnection->mutableProperties().setProperty(CSSPropertyUnicodeRange, &unicodeRange);

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

void CSSFontFace::setLoadingBehavior(CSSValue& loadingBehaviorValue)
{
    auto loadingBehavior = static_cast<FontLoadingBehavior>(downcast<CSSPrimitiveValue>(loadingBehaviorValue));

    if (m_loadingBehavior == loadingBehavior)
        return;

    m_loadingBehavior = loadingBehavior;

    if (m_cssConnection)
        m_cssConnection->mutableProperties().setProperty(CSSPropertyFontDisplay, &loadingBehaviorValue);

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
    // If the font is already in the cache, CSSFontFaceSource may report it's loaded before it is added here as a source.
    // Let's not pump the state machine until we've got all our sources. font() and load() are smart enough to act correctly
    // when a source is failed or succeeded before we have asked it to load.
    if (m_sourcesPopulated)
        pump(ExternalResourceDownloadPolicy::Forbid);

    if (m_fontSelector)
        m_fontSelector->fontLoaded();

    iterateClients(m_clients, [&](Client& client) {
        client.fontLoaded(*this);
    });
}

void CSSFontFace::timeoutFired()
{
    Ref<CSSFontFace> protectedThis(*this);
    
    switch (status()) {
    case Status::Loading:
        setStatus(Status::TimedOut);
        break;
    case Status::TimedOut:
        // Cancelling the network request here could lead to a situation where a site's font never gets
        // shown as the user navigates around to different pages on the site. This would occur if the
        // download always takes longer than the timeout (even though the user may spend substantial time
        // on each page). Therefore, we shouldn't cancel the network request here, but should use the
        // loading infrastructure's timeout policies instead.
        setStatus(Status::Failure);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    fontLoadEventOccurred();
}

bool CSSFontFace::computeFailureState() const
{
    if (status() == Status::Failure)
        return true;
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
    m_wrapper = makeWeakPtr(wrapper.get());
    initializeWrapper();
    return wrapper;
}

void CSSFontFace::setWrapper(FontFace& newWrapper)
{
    m_wrapper = makeWeakPtr(newWrapper);
    initializeWrapper();
}

void CSSFontFace::adoptSource(std::unique_ptr<CSSFontFaceSource>&& source)
{
    m_sources.append(WTFMove(source));

    // We should never add sources in the middle of loading.
    ASSERT(!m_sourcesPopulated);
}

Document* CSSFontFace::document() const
{
    return m_fontSelector ? m_fontSelector->document() : nullptr;
}

AllowUserInstalledFonts CSSFontFace::allowUserInstalledFonts() const
{
    if (m_fontSelector && m_fontSelector->document())
        return m_fontSelector->document()->settings().shouldAllowUserInstalledFonts() ? AllowUserInstalledFonts::Yes : AllowUserInstalledFonts::No;
    return AllowUserInstalledFonts::Yes;
}

static Settings::FontLoadTimingOverride fontLoadTimingOverride(CSSFontSelector* fontSelector)
{
    auto overrideValue = Settings::FontLoadTimingOverride::None;
    if (fontSelector && fontSelector->document())
        overrideValue = fontSelector->document()->settings().fontLoadTimingOverride();
    return overrideValue;
}

auto CSSFontFace::fontLoadTiming() const -> FontLoadTiming
{
    switch (fontLoadTimingOverride(m_fontSelector.get())) {
    case Settings::FontLoadTimingOverride::None:
        switch (m_loadingBehavior) {
        case FontLoadingBehavior::Auto:
        case FontLoadingBehavior::Block:
            return { 3_s, Seconds::infinity() };
        case FontLoadingBehavior::Swap:
            return { 0_s, Seconds::infinity() };
        case FontLoadingBehavior::Fallback:
            return { 0.1_s, 3_s };
        case FontLoadingBehavior::Optional:
            return { 0.1_s, 0_s };
        }
        RELEASE_ASSERT_NOT_REACHED();
    case Settings::FontLoadTimingOverride::Block:
        return { Seconds::infinity(), 0_s };
    case Settings::FontLoadTimingOverride::Swap:
        return { 0_s, Seconds::infinity() };
    case Settings::FontLoadTimingOverride::Failure:
        return { 0_s, 0_s };
    }
    RELEASE_ASSERT_NOT_REACHED();
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

    iterateClients(m_clients, [&](Client& client) {
        client.fontStateChanged(*this, m_status, newStatus);
    });

    m_status = newStatus;

    Seconds blockPeriodTimeout;
    Seconds swapPeriodTimeout;
    auto timeouts = fontLoadTiming();
    blockPeriodTimeout = timeouts.blockPeriod;
    swapPeriodTimeout = timeouts.swapPeriod;

    // Transfer across 0-delay timers synchronously. Layouts/script may
    // take arbitrarily long time, and we shouldn't be in a 0-duration
    // state for an arbitrarily long time. Also it's necessary for
    // testing so we don't have a race with the font load.
    switch (newStatus) {
    case Status::Pending:
        ASSERT_NOT_REACHED();
        break;
    case Status::Loading:
        if (blockPeriodTimeout == 0_s)
            setStatus(Status::TimedOut);
        else if (std::isfinite(blockPeriodTimeout.value()))
            m_timeoutTimer.startOneShot(blockPeriodTimeout);
        break;
    case Status::TimedOut:
        if (swapPeriodTimeout == 0_s)
            setStatus(Status::Failure);
        else if (std::isfinite(swapPeriodTimeout.value()))
            m_timeoutTimer.startOneShot(swapPeriodTimeout);
        break;
    case Status::Success:
    case Status::Failure:
        m_timeoutTimer.stop();
        break;
    }
}

void CSSFontFace::fontLoaded(CSSFontFaceSource&)
{
    Ref<CSSFontFace> protectedThis(*this);
    
    fontLoadEventOccurred();
}

bool CSSFontFace::shouldIgnoreFontLoadCompletions() const
{
    if (m_fontSelector && m_fontSelector->document())
        return m_fontSelector->document()->settings().shouldIgnoreFontLoadCompletions();
    return false;
}

void CSSFontFace::opportunisticallyStartFontDataURLLoading(CSSFontSelector& fontSelector)
{
    // We don't want to go crazy here and blow the cache. Usually these data URLs are the first item in the src: list, so let's just check that one.
    if (!m_sources.isEmpty())
        m_sources[0]->opportunisticallyStartFontDataURLLoading(fontSelector);
}

size_t CSSFontFace::pump(ExternalResourceDownloadPolicy policy)
{
    if (status() == Status::Failure)
        return 0;

    size_t i;
    for (i = 0; i < m_sources.size(); ++i) {
        auto& source = m_sources[i];

        if (source->status() == CSSFontFaceSource::Status::Pending) {
            ASSERT(m_status == Status::Pending || m_status == Status::Loading || m_status == Status::TimedOut);
            // This is a little tricky. After calling CSSFontFace::font(Forbid), a font must never fail later in
            // this turn of the runloop because the return value of CSSFontFace::font() shouldn't get nulled out
            // from under an existing FontRanges object. Remote fonts are all downloaded asynchronously, so this
            // isn't a problem for them because CSSFontFace::font() will always return the interstitial font.
            // However, local fonts may synchronously fail when you call load() on them. Therefore, we have to call
            // load() here in order to guarantee that, if the font synchronously fails, it happens now during the
            // first call to CSSFontFace::font() and the FontRanges object sees a consistent view of the
            // CSSFontFace. This means we eagerly create some internal font objects when they may not be needed,
            // but it seems that this behavior is a requirement of the design of FontRanges. FIXME: Perhaps rethink
            // this design.
            if (policy == ExternalResourceDownloadPolicy::Allow || !source->requiresExternalResource()) {
                if (policy == ExternalResourceDownloadPolicy::Allow && m_status == Status::Pending)
                    setStatus(Status::Loading);
                source->load(m_fontSelector.get());
            }
        }

        switch (source->status()) {
        case CSSFontFaceSource::Status::Pending:
            ASSERT(policy == ExternalResourceDownloadPolicy::Forbid);
            return i;
        case CSSFontFaceSource::Status::Loading:
            ASSERT(m_status == Status::Pending || m_status == Status::Loading || m_status == Status::TimedOut || m_status == Status::Failure);
            if (policy == ExternalResourceDownloadPolicy::Allow && m_status == Status::Pending)
                setStatus(Status::Loading);
            return i;
        case CSSFontFaceSource::Status::Success:
            ASSERT(m_status == Status::Pending || m_status == Status::Loading || m_status == Status::TimedOut || m_status == Status::Success || m_status == Status::Failure);
            if (m_status == Status::Pending)
                setStatus(Status::Loading);
            if (m_status == Status::Loading || m_status == Status::TimedOut)
                setStatus(Status::Success);
            return i;
        case CSSFontFaceSource::Status::Failure:
            if (policy == ExternalResourceDownloadPolicy::Allow && m_status == Status::Pending)
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
    pump(ExternalResourceDownloadPolicy::Allow);
}

static Font::Visibility visibility(CSSFontFace::Status status, CSSFontFace::FontLoadTiming timing)
{
    switch (status) {
    case CSSFontFace::Status::Pending:
        return timing.blockPeriod == 0_s ? Font::Visibility::Visible : Font::Visibility::Invisible;
    case CSSFontFace::Status::Loading:
        return Font::Visibility::Invisible;
    case CSSFontFace::Status::TimedOut:
    case CSSFontFace::Status::Failure:
    case CSSFontFace::Status::Success:
    default:
        return Font::Visibility::Visible;
    }
}

RefPtr<Font> CSSFontFace::font(const FontDescription& fontDescription, bool syntheticBold, bool syntheticItalic, ExternalResourceDownloadPolicy policy)
{
    if (computeFailureState())
        return nullptr;

    Ref<CSSFontFace> protectedThis(*this);
    
    // Our status is derived from the first non-failed source. However, this source may
    // return null from font(), which means we need to continue looping through the remainder
    // of the sources to try to find a font to use. These subsequent tries should not affect
    // our own state, though.
    size_t startIndex = pump(policy);

    if (computeFailureState())
        return nullptr;

    for (size_t i = startIndex; i < m_sources.size(); ++i) {
        auto& source = m_sources[i];
        if (source->status() == CSSFontFaceSource::Status::Pending && (policy == ExternalResourceDownloadPolicy::Allow || !source->requiresExternalResource()))
            source->load(m_fontSelector.get());

        switch (source->status()) {
        case CSSFontFaceSource::Status::Pending:
        case CSSFontFaceSource::Status::Loading: {
            Font::Visibility visibility = WebCore::visibility(status(), fontLoadTiming());
            return Font::create(FontCache::singleton().lastResortFallbackFont(fontDescription)->platformData(), Font::Origin::Local, Font::Interstitial::Yes, visibility);
        }
        case CSSFontFaceSource::Status::Success:
            if (auto result = source->font(fontDescription, syntheticBold, syntheticItalic, m_featureSettings, m_fontSelectionCapabilities))
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

bool CSSFontFace::hasSVGFontFaceSource() const
{
    size_t size = m_sources.size();
    for (size_t i = 0; i < size; i++) {
        if (m_sources[i]->isSVGFontFaceSource())
            return true;
    }
    return false;
}

void CSSFontFace::setErrorState()
{
    switch (m_status) {
    case Status::Pending:
        setStatus(Status::Loading);
        break;
    case Status::Success:
        ASSERT_NOT_REACHED();
        break;
    case Status::Failure:
        return;
    default:
        break;
    }
    
    setStatus(Status::Failure);
}

}
