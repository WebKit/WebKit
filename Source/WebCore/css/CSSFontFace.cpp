/*
 * Copyright (C) 2007-2022 Apple Inc. All rights reserved.
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
#include "FontPaletteValues.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "StyleBuilderConverter.h"
#include "StyleProperties.h"
#include "StyleRule.h"

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSFontFace);

template<typename T> void iterateClients(HashSet<CSSFontFace::Client*>& clients, T callback)
{
    for (auto& client : copyToVectorOf<RefPtr<CSSFontFace::Client>>(clients))
        callback(*client);
}

void CSSFontFace::appendSources(CSSFontFace& fontFace, CSSValueList& srcList, ScriptExecutionContext* context, bool isInitiatingElementInUserAgentShadowTree)
{
    bool allowDownloading = context && context->settingsValues().downloadableBinaryFontsEnabled;
    for (auto& src : srcList) {
        // An item in the list either specifies a string (local font name) or a URL (remote font to download).
        if (auto local = dynamicDowncast<CSSFontFaceSrcLocalValue>(src.get())) {
            if (!local->svgFontFaceElement())
                fontFace.adoptSource(makeUnique<CSSFontFaceSource>(fontFace, local->fontFaceName()));
            else if (allowDownloading)
                fontFace.adoptSource(makeUnique<CSSFontFaceSource>(fontFace, local->fontFaceName(), *local->svgFontFaceElement()));
        } else {
            if (allowDownloading) {
                if (auto request = downcast<CSSFontFaceSrcResourceValue>(src.get()).fontLoadRequest(*context, isInitiatingElementInUserAgentShadowTree))
                    fontFace.adoptSource(makeUnique<CSSFontFaceSource>(fontFace, *context->cssFontSelector(), makeUniqueRefFromNonNullUniquePtr(WTFMove(request))));
            }
        }
    }
    fontFace.sourcesPopulated();
}

Ref<CSSFontFace> CSSFontFace::create(CSSFontSelector& fontSelector, StyleRuleFontFace* cssConnection, FontFace* wrapper, bool isLocalFallback)
{
    auto* context = fontSelector.scriptExecutionContext();
    const auto* settings = context ? &context->settingsValues() : nullptr;
    auto result = adoptRef(*new CSSFontFace(settings, cssConnection, wrapper, isLocalFallback));
    result->addClient(fontSelector);
    return result;
}

static std::variant<Ref<MutableStyleProperties>, Ref<StyleRuleFontFace>> propertiesOrCSSConnection(StyleRuleFontFace* connection)
{
    if (connection)
        return Ref { *connection };
    return MutableStyleProperties::create();
}

CSSFontFace::CSSFontFace(const Settings::Values* settings, StyleRuleFontFace* connection, FontFace* wrapper, bool isLocalFallback)
    : m_propertiesOrCSSConnection(propertiesOrCSSConnection(connection))
    , m_wrapper(wrapper)
    , m_isLocalFallback(isLocalFallback)
    , m_mayBePurged(!wrapper)
    , m_shouldIgnoreFontLoadCompletions(settings && settings->shouldIgnoreFontLoadCompletions)
    , m_fontLoadTimingOverride(settings ? settings->fontLoadTimingOverride : FontLoadTimingOverride::None)
    , m_allowUserInstalledFonts(settings && !settings->shouldAllowUserInstalledFonts ? AllowUserInstalledFonts::No : AllowUserInstalledFonts::Yes)
    , m_timeoutTimer(*this, &CSSFontFace::timeoutFired)
{
}

CSSFontFace::~CSSFontFace() = default;

const StyleProperties& CSSFontFace::properties() const
{
    return WTF::switchOn(m_propertiesOrCSSConnection,
        [] (const Ref<MutableStyleProperties>& properties) -> const StyleProperties& {
            return properties;
        },
        [] (const Ref<StyleRuleFontFace>& connection) -> const StyleProperties& {
            return connection->properties();
        }
    );
}

MutableStyleProperties& CSSFontFace::mutableProperties()
{
    return WTF::switchOn(m_propertiesOrCSSConnection,
        [] (const Ref<MutableStyleProperties>& properties) -> MutableStyleProperties& {
            return properties;
        },
        [] (const Ref<StyleRuleFontFace>& connection) -> MutableStyleProperties& {
            return connection->mutableProperties();
        }
    );
}

StyleRuleFontFace* CSSFontFace::cssConnection() const
{
    return WTF::switchOn(m_propertiesOrCSSConnection,
        [] (const Ref<MutableStyleProperties>&) -> StyleRuleFontFace* {
            return nullptr;
        },
        [] (const Ref<StyleRuleFontFace>& connection) {
            return connection.ptr();
        }
    );
}

// FIXME: Don't use a list here and rename to setFamily. https://bugs.webkit.org/show_bug.cgi?id=196381
void CSSFontFace::setFamilies(CSSValueList& family)
{
    ASSERT(family.length());

    RefPtr oldFamily = std::exchange(m_families, &family);
    mutableProperties().setProperty(CSSPropertyFontFamily, &family);

    iterateClients(m_clients, [&](Client& client) {
        client.fontPropertyChanged(*this, oldFamily.get());
    });
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
    mutableProperties().setProperty(CSSPropertyFontWeight, &weight);

    auto range = calculateWeightRange(weight);
    if (m_fontSelectionCapabilities.weight == range)
        return;

    m_fontSelectionCapabilities.weight = range;

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
    mutableProperties().setProperty(CSSPropertyFontStretch, &style);

    auto range = calculateStretchRange(style);
    if (m_fontSelectionCapabilities.width == range)
        return;

    m_fontSelectionCapabilities.width = range;

    iterateClients(m_clients, [&](Client& client) {
        client.fontPropertyChanged(*this);
    });
}

static FontSelectionRange calculateItalicRange(CSSValue& value)
{
    if (!is<CSSFontStyleRangeValue>(value))
        return FontSelectionRange { Style::BuilderConverter::convertFontStyleFromValue(value).value_or(normalItalicValue()) };

    auto& rangeValue = downcast<CSSFontStyleRangeValue>(value);
    auto keyword = rangeValue.fontStyleValue->valueID();
    if (!rangeValue.obliqueValues) {
        if (keyword == CSSValueNormal)
            return FontSelectionRange { normalItalicValue() };
        ASSERT(keyword == CSSValueItalic || keyword == CSSValueOblique);
        return FontSelectionRange { italicValue() };
    }
    ASSERT(keyword == CSSValueOblique);
    auto length = rangeValue.obliqueValues->length();
    ASSERT(length == 1 || length == 2);
    auto angleAtIndex = [&] (size_t index) {
        return Style::BuilderConverter::convertFontStyleAngle(*rangeValue.obliqueValues->itemWithoutBoundsCheck(index));
    };
    if (length == 1)
        return FontSelectionRange { angleAtIndex(0) };
    return { angleAtIndex(0), angleAtIndex(1) };
}

void CSSFontFace::setStyle(CSSValue& style)
{
    mutableProperties().setProperty(CSSPropertyFontStyle, &style);

    auto range = calculateItalicRange(style);
    if (m_fontSelectionCapabilities.slope == range)
        return;

    m_fontSelectionCapabilities.slope = range;

    iterateClients(m_clients, [&](Client& client) {
        client.fontPropertyChanged(*this);
    });
}

void CSSFontFace::setUnicodeRange(CSSValueList& list)
{
    mutableProperties().setProperty(CSSPropertyUnicodeRange, &list);

    Vector<UnicodeRange> ranges;
    ranges.reserveInitialCapacity(list.length());

    for (auto& rangeValue : list) {
        auto& range = downcast<CSSUnicodeRangeValue>(rangeValue.get());
        ranges.uncheckedAppend({ range.from(), range.to() });
    }

    if (ranges == m_ranges)
        return;

    m_ranges = WTFMove(ranges);

    iterateClients(m_clients, [&](Client& client) {
        client.fontPropertyChanged(*this);
    });
}

void CSSFontFace::setFeatureSettings(CSSValue& featureSettings)
{
    // Can only call this with a primitive value of normal, or a value list containing font feature values.
    ASSERT(is<CSSPrimitiveValue>(featureSettings) || is<CSSValueList>(featureSettings));

    mutableProperties().setProperty(CSSPropertyFontFeatureSettings, &featureSettings);

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

    iterateClients(m_clients, [&](Client& client) {
        client.fontPropertyChanged(*this);
    });
}

void CSSFontFace::setDisplay(CSSPrimitiveValue& loadingBehaviorValue)
{
    mutableProperties().setProperty(CSSPropertyFontDisplay, &loadingBehaviorValue);

    FontLoadingBehavior loadingBehavior = loadingBehaviorValue;

    if (m_loadingBehavior == loadingBehavior)
        return;

    m_loadingBehavior = loadingBehavior;

    iterateClients(m_clients, [&](Client& client) {
        client.fontPropertyChanged(*this);
    });
}

String CSSFontFace::family() const
{
    // Code to extract the name of the first family is needed because we incorrectly use a list of families.
    // FIXME: Consider switching to getPropertyValue after https://bugs.webkit.org/show_bug.cgi?id=196381 is fixed.
    auto family = properties().getPropertyCSSValue(CSSPropertyFontFamily);
    auto familyList = dynamicDowncast<CSSValueList>(family.get());
    if (!familyList)
        return { };
    auto firstFamily = dynamicDowncast<CSSPrimitiveValue>(familyList->item(0));
    if (!firstFamily || !firstFamily->isFontFamily())
        return { };
    return firstFamily->fontFamily().familyName;
}

String CSSFontFace::style() const
{
    return properties().getPropertyValue(CSSPropertyFontStyle);
}

String CSSFontFace::weight() const
{
    return properties().getPropertyValue(CSSPropertyFontWeight);
}

String CSSFontFace::stretch() const
{
    return properties().getPropertyValue(CSSPropertyFontStretch);
}

String CSSFontFace::unicodeRange() const
{
    return properties().getPropertyValue(CSSPropertyUnicodeRange);
}

String CSSFontFace::featureSettings() const
{
    return properties().getPropertyValue(CSSPropertyFontFeatureSettings);
}

String CSSFontFace::display() const
{
    return properties().getPropertyValue(CSSPropertyFontDisplay);
}

RefPtr<CSSValueList> CSSFontFace::families() const
{
    return m_families;
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

Document* CSSFontFace::document()
{
    if (m_wrapper && is<Document>(m_wrapper->scriptExecutionContext()))
        return &downcast<Document>(*m_wrapper->scriptExecutionContext());
    return nullptr;
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

Ref<FontFace> CSSFontFace::wrapper(ScriptExecutionContext* context)
{
    if (m_wrapper) {
        ASSERT(m_wrapper->scriptExecutionContext() == context);
        return *m_wrapper.get();
    }

    auto wrapper = FontFace::create(context, *this);
    m_wrapper = wrapper;
    initializeWrapper();
    return wrapper;
}

void CSSFontFace::setWrapper(FontFace& newWrapper)
{
    m_wrapper = newWrapper;
    initializeWrapper();
}

void CSSFontFace::adoptSource(std::unique_ptr<CSSFontFaceSource>&& source)
{
    m_sources.append(WTFMove(source));

    // We should never add sources in the middle of loading.
    ASSERT(!m_sourcesPopulated);
}

auto CSSFontFace::fontLoadTiming() const -> FontLoadTiming
{
    switch (m_fontLoadTimingOverride) {
    case FontLoadTimingOverride::None:
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
    case FontLoadTimingOverride::Block:
        return { Seconds::infinity(), 0_s };
    case FontLoadTimingOverride::Swap:
        return { 0_s, Seconds::infinity() };
    case FontLoadTimingOverride::Failure:
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

void CSSFontFace::opportunisticallyStartFontDataURLLoading()
{
    // We don't want to go crazy here and blow the cache. Usually these data URLs are the first item in the src: list, so let's just check that one.
    if (!m_sources.isEmpty())
        m_sources[0]->opportunisticallyStartFontDataURLLoading();
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
                source->load(document());
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

RefPtr<Font> CSSFontFace::font(const FontDescription& fontDescription, bool syntheticBold, bool syntheticItalic, ExternalResourceDownloadPolicy policy, const FontPaletteValues& fontPaletteValues, RefPtr<FontFeatureValues> fontFeatureValues)
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
            source->load(document());

        switch (source->status()) {
        case CSSFontFaceSource::Status::Pending:
        case CSSFontFaceSource::Status::Loading: {
            Font::Visibility visibility = WebCore::visibility(status(), fontLoadTiming());
            return Font::create(FontCache::forCurrentThread().lastResortFallbackFont(fontDescription)->platformData(), Font::Origin::Local, Font::Interstitial::Yes, visibility);
        }
        case CSSFontFaceSource::Status::Success: {
            FontCreationContext fontCreationContext { m_featureSettings, m_fontSelectionCapabilities, fontPaletteValues, fontFeatureValues };
            if (auto result = source->font(fontDescription, syntheticBold, syntheticItalic, fontCreationContext))
                return result;
            break;
        }
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
    iterateClients(m_clients, [&](Client& client) {
        client.updateStyleIfNeeded(*this);
    });
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
