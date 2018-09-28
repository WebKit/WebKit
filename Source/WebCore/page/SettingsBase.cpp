/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All rights reserved.
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
#include "SettingsBase.h"

#include "AudioSession.h"
#include "BackForwardController.h"
#include "CachedResourceLoader.h"
#include "CookieStorage.h"
#include "DOMTimer.h"
#include "Database.h"
#include "Document.h"
#include "FontCascade.h"
#include "FontGenericFamilies.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HistoryItem.h"
#include "Page.h"
#include "PageCache.h"
#include "RenderWidget.h"
#include "RuntimeApplicationChecks.h"
#include "Settings.h"
#include "StorageMap.h"
#include <limits>
#include <wtf/StdLibExtras.h>

namespace WebCore {

static void invalidateAfterGenericFamilyChange(Page* page)
{
    invalidateFontCascadeCache();
    if (page)
        page->setNeedsRecalcStyleInAllFrames();
}

// This amount of time must have elapsed before we will even consider scheduling a layout without a delay.
// FIXME: For faster machines this value can really be lowered to 200. 250 is adequate, but a little high
// for dual G5s. :)
static const Seconds layoutScheduleThreshold = 250_ms;

SettingsBase::SettingsBase(Page* page)
    : m_page(nullptr)
    , m_fontGenericFamilies(std::make_unique<FontGenericFamilies>())
    , m_layoutInterval(layoutScheduleThreshold)
    , m_minimumDOMTimerInterval(DOMTimer::defaultMinimumInterval())
    , m_setImageLoadingSettingsTimer(*this, &SettingsBase::imageLoadingSettingsTimerFired)
{
    // A Frame may not have been created yet, so we initialize the AtomicString
    // hash before trying to use it.
    AtomicString::init();
    initializeDefaultFontFamilies();
    m_page = page; // Page is not yet fully initialized when constructing Settings, so keeping m_page null over initializeDefaultFontFamilies() call.
}

SettingsBase::~SettingsBase() = default;

float SettingsBase::defaultMinimumZoomFontSize()
{
#if PLATFORM(WATCHOS)
    return 30;
#else
    return 15;
#endif
}

#if !PLATFORM(IOS)
bool SettingsBase::defaultTextAutosizingEnabled()
{
    return false;
}
#endif

bool SettingsBase::defaultDownloadableBinaryFontsEnabled()
{
#if PLATFORM(WATCHOS)
    return false;
#else
    return true;
#endif
}

#if !PLATFORM(COCOA)
const String& SettingsBase::defaultMediaContentTypesRequiringHardwareSupport()
{
    return emptyString();
}
#endif

#if !PLATFORM(COCOA)
void SettingsBase::initializeDefaultFontFamilies()
{
    // Other platforms can set up fonts from a client, but on Mac, we want it in WebCore to share code between WebKit1 and WebKit2.
}
#endif

const AtomicString& SettingsBase::standardFontFamily(UScriptCode script) const
{
    return m_fontGenericFamilies->standardFontFamily(script);
}

void SettingsBase::setStandardFontFamily(const AtomicString& family, UScriptCode script)
{
    bool changes = m_fontGenericFamilies->setStandardFontFamily(family, script);
    if (changes)
        invalidateAfterGenericFamilyChange(m_page);
}

const AtomicString& SettingsBase::fixedFontFamily(UScriptCode script) const
{
    return m_fontGenericFamilies->fixedFontFamily(script);
}

void SettingsBase::setFixedFontFamily(const AtomicString& family, UScriptCode script)
{
    bool changes = m_fontGenericFamilies->setFixedFontFamily(family, script);
    if (changes)
        invalidateAfterGenericFamilyChange(m_page);
}

const AtomicString& SettingsBase::serifFontFamily(UScriptCode script) const
{
    return m_fontGenericFamilies->serifFontFamily(script);
}

void SettingsBase::setSerifFontFamily(const AtomicString& family, UScriptCode script)
{
    bool changes = m_fontGenericFamilies->setSerifFontFamily(family, script);
    if (changes)
        invalidateAfterGenericFamilyChange(m_page);
}

const AtomicString& SettingsBase::sansSerifFontFamily(UScriptCode script) const
{
    return m_fontGenericFamilies->sansSerifFontFamily(script);
}

void SettingsBase::setSansSerifFontFamily(const AtomicString& family, UScriptCode script)
{
    bool changes = m_fontGenericFamilies->setSansSerifFontFamily(family, script);
    if (changes)
        invalidateAfterGenericFamilyChange(m_page);
}

const AtomicString& SettingsBase::cursiveFontFamily(UScriptCode script) const
{
    return m_fontGenericFamilies->cursiveFontFamily(script);
}

void SettingsBase::setCursiveFontFamily(const AtomicString& family, UScriptCode script)
{
    bool changes = m_fontGenericFamilies->setCursiveFontFamily(family, script);
    if (changes)
        invalidateAfterGenericFamilyChange(m_page);
}

const AtomicString& SettingsBase::fantasyFontFamily(UScriptCode script) const
{
    return m_fontGenericFamilies->fantasyFontFamily(script);
}

void SettingsBase::setFantasyFontFamily(const AtomicString& family, UScriptCode script)
{
    bool changes = m_fontGenericFamilies->setFantasyFontFamily(family, script);
    if (changes)
        invalidateAfterGenericFamilyChange(m_page);
}

const AtomicString& SettingsBase::pictographFontFamily(UScriptCode script) const
{
    return m_fontGenericFamilies->pictographFontFamily(script);
}

void SettingsBase::setPictographFontFamily(const AtomicString& family, UScriptCode script)
{
    bool changes = m_fontGenericFamilies->setPictographFontFamily(family, script);
    if (changes)
        invalidateAfterGenericFamilyChange(m_page);
}

void SettingsBase::setMinimumDOMTimerInterval(Seconds interval)
{
    auto oldTimerInterval = std::exchange(m_minimumDOMTimerInterval, interval);

    if (!m_page)
        return;

    for (Frame* frame = &m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (frame->document())
            frame->document()->adjustMinimumDOMTimerInterval(oldTimerInterval);
    }
}

void SettingsBase::setLayoutInterval(Seconds layoutInterval)
{
    // FIXME: It seems weird that this function may disregard the specified layout interval.
    // We should either expose layoutScheduleThreshold or better communicate this invariant.
    m_layoutInterval = std::max(layoutInterval, layoutScheduleThreshold);
}

void SettingsBase::setMediaContentTypesRequiringHardwareSupport(const String& contentTypes)
{
    m_mediaContentTypesRequiringHardwareSupport.shrink(0);
    for (auto type : StringView(contentTypes).split(':'))
        m_mediaContentTypesRequiringHardwareSupport.append(ContentType { type.toString() });
}

void SettingsBase::setMediaContentTypesRequiringHardwareSupport(const Vector<ContentType>& contentTypes)
{
    m_mediaContentTypesRequiringHardwareSupport = contentTypes;
}



// MARK - onChange handlers

void SettingsBase::setNeedsRecalcStyleInAllFrames()
{
    if (m_page)
        m_page->setNeedsRecalcStyleInAllFrames();
}

void SettingsBase::setNeedsRelayoutAllFrames()
{
    if (!m_page)
        return;

    for (Frame* frame = &m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->ownerRenderer())
            continue;
        frame->ownerRenderer()->setNeedsLayoutAndPrefWidthsRecalc();
    }
}

void SettingsBase::mediaTypeOverrideChanged()
{
    if (!m_page)
        return;

    FrameView* view = m_page->mainFrame().view();
    if (view)
        view->setMediaType(m_page->settings().mediaTypeOverride());

    m_page->setNeedsRecalcStyleInAllFrames();
}

void SettingsBase::imagesEnabledChanged()
{
    // Changing this setting to true might immediately start new loads for images that had previously had loading disabled.
    // If this happens while a WebView is being dealloc'ed, and we don't know the WebView is being dealloc'ed, these new loads
    // can cause crashes downstream when the WebView memory has actually been free'd.
    // One example where this can happen is in Mac apps that subclass WebView then do work in their overridden dealloc methods.
    // Starting these loads synchronously is not important. By putting it on a 0-delay, properly closing the Page cancels them
    // before they have a chance to really start.
    // See http://webkit.org/b/60572 for more discussion.
    m_setImageLoadingSettingsTimer.startOneShot(0_s);
}

void SettingsBase::imageLoadingSettingsTimerFired()
{
    if (!m_page)
        return;

    for (Frame* frame = &m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document())
            continue;
        frame->document()->cachedResourceLoader().setImagesEnabled(m_page->settings().areImagesEnabled());
        frame->document()->cachedResourceLoader().setAutoLoadImages(m_page->settings().loadsImagesAutomatically());
    }
}

void SettingsBase::pluginsEnabledChanged()
{
    Page::refreshPlugins(false);
}

#if ENABLE(TEXT_AUTOSIZING)

void SettingsBase::shouldEnableTextAutosizingBoostChanged()
{
    if (!m_page)
        return;

    bool boostAutosizing = m_page->settings().shouldEnableTextAutosizingBoost();
    m_oneLineTextMultiplierCoefficient = boostAutosizing ? boostedOneLineTextMultiplierCoefficient : defaultOneLineTextMultiplierCoefficient;
    m_multiLineTextMultiplierCoefficient = boostAutosizing ? boostedMultiLineTextMultiplierCoefficient : defaultMultiLineTextMultiplierCoefficient;
    m_maxTextAutosizingScaleIncrease = boostAutosizing ? boostedMaxTextAutosizingScaleIncrease : defaultMaxTextAutosizingScaleIncrease;

    setNeedsRecalcStyleInAllFrames();
}

#endif

void SettingsBase::userStyleSheetLocationChanged()
{
    if (m_page)
        m_page->userStyleSheetLocationChanged();
}

void SettingsBase::usesPageCacheChanged()
{
    if (!m_page)
        return;

    if (!m_page->settings().usesPageCache())
        PageCache::singleton().pruneToSizeNow(0, PruningReason::None);
}

void SettingsBase::dnsPrefetchingEnabledChanged()
{
    if (m_page)
        m_page->dnsPrefetchingStateChanged();
}

void SettingsBase::storageBlockingPolicyChanged()
{
    if (m_page)
        m_page->storageBlockingStateChanged();
}

void SettingsBase::backgroundShouldExtendBeyondPageChanged()
{
    if (m_page)
        m_page->mainFrame().view()->updateExtendBackgroundIfNecessary();
}

void SettingsBase::scrollingPerformanceLoggingEnabledChanged()
{
    if (m_page && m_page->mainFrame().view())
        m_page->mainFrame().view()->setScrollingPerformanceLoggingEnabled(m_page->settings().scrollingPerformanceLoggingEnabled());
}

void SettingsBase::hiddenPageDOMTimerThrottlingStateChanged()
{
    if (m_page)
        m_page->hiddenPageDOMTimerThrottlingStateChanged();
}

void SettingsBase::hiddenPageCSSAnimationSuspensionEnabledChanged()
{
    if (m_page)
        m_page->hiddenPageCSSAnimationSuspensionStateChanged();
}

void SettingsBase::resourceUsageOverlayVisibleChanged()
{
#if ENABLE(RESOURCE_USAGE)
    if (m_page)
        m_page->setResourceUsageOverlayVisible(m_page->settings().resourceUsageOverlayVisible());
#endif
}

} // namespace WebCore
