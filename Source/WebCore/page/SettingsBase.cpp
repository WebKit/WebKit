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
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLMediaElement.h"
#include "HistoryItem.h"
#include "MainFrame.h"
#include "Page.h"
#include "PageCache.h"
#include "RuntimeApplicationChecks.h"
#include "Settings.h"
#include "StorageMap.h"
#include <limits>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>

#if PLATFORM(COCOA)
#include <wtf/spi/darwin/dyldSPI.h>
#endif

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)
#include "RealtimeMediaSourceCenterMac.h"
#endif

#if ENABLE(MEDIA_STREAM)
#include "MockRealtimeMediaSourceCenter.h"
#endif

namespace WebCore {

static void setImageLoadingSettings(Page* page)
{
    if (!page)
        return;

    for (Frame* frame = &page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document())
            continue;
        frame->document()->cachedResourceLoader().setImagesEnabled(page->settings().areImagesEnabled());
        frame->document()->cachedResourceLoader().setAutoLoadImages(page->settings().loadsImagesAutomatically());
    }
}

static void invalidateAfterGenericFamilyChange(Page* page)
{
    invalidateFontCascadeCache();
    if (page)
        page->setNeedsRecalcStyleInAllFrames();
}

#if USE(AVFOUNDATION)
bool SettingsBase::gAVFoundationEnabled = true;
bool SettingsBase::gAVFoundationNSURLSessionEnabled = true;
#endif

#if PLATFORM(COCOA)
bool SettingsBase::gQTKitEnabled = false;
#endif

#if USE(GSTREAMER)
bool SettingsBase::gGStreamerEnabled = true;
#endif

bool SettingsBase::gMockScrollbarsEnabled = false;
bool SettingsBase::gUsesOverlayScrollbars = false;
bool SettingsBase::gMockScrollAnimatorEnabled = false;

#if ENABLE(MEDIA_STREAM)
bool SettingsBase::gMockCaptureDevicesEnabled = false;
bool SettingsBase::gMediaCaptureRequiresSecureConnection = true;
#endif

#if PLATFORM(WIN)
bool SettingsBase::gShouldUseHighResolutionTimers = true;
#endif
    
bool SettingsBase::gShouldRespectPriorityInCSSAttributeSetters = false;
bool SettingsBase::gLowPowerVideoAudioBufferSizeEnabled = false;
bool SettingsBase::gResourceLoadStatisticsEnabledEnabled = false;
bool SettingsBase::gAllowsAnySSLCertificate = false;

#if PLATFORM(IOS)
bool SettingsBase::gNetworkDataUsageTrackingEnabled = false;
bool SettingsBase::gAVKitEnabled = false;
bool SettingsBase::gShouldOptOutOfNetworkStateObservation = false;
#endif
bool SettingsBase::gManageAudioSession = false;
bool SettingsBase::gCustomPasteboardDataEnabled = false;

bool SettingsBase::defaultCustomPasteboardDataEnabled()
{
#if PLATFORM(IOS) && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110300
    return IOSApplication::isMobileSafari() || dyld_get_program_sdk_version() >= DYLD_IOS_VERSION_11_3;
#elif PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101300
    return MacApplication::isSafari() || dyld_get_program_sdk_version() > DYLD_MACOSX_VERSION_10_13;
#elif PLATFORM(MAC)
    return MacApplication::isSafari();
#else
    return false;
#endif
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
    , m_isJavaEnabled(false)
    , m_isJavaEnabledForLocalFiles(true)
    , m_loadsImagesAutomatically(false)
    , m_areImagesEnabled(true)
    , m_preferMIMETypeForImages(false)
    , m_arePluginsEnabled(false)
    , m_isScriptEnabled(false)
    , m_needsAdobeFrameReloadingQuirk(false)
    , m_usesPageCache(false)
    , m_fontRenderingMode(0)
    , m_showTiledScrollingIndicator(false)
    , m_backgroundShouldExtendBeyondPage(false)
    , m_dnsPrefetchingEnabled(false)
#if ENABLE(TOUCH_EVENTS)
    , m_touchEventEmulationEnabled(false)
#endif
    , m_scrollingPerformanceLoggingEnabled(false)
    , m_setImageLoadingSettingsTimer(*this, &SettingsBase::imageLoadingSettingsTimerFired)
    , m_hiddenPageDOMTimerThrottlingEnabled(false)
    , m_hiddenPageCSSAnimationSuspensionEnabled(false)
    , m_fontFallbackPrefersPictographs(false)
    , m_forcePendingWebGLPolicy(false)
{
    // A Frame may not have been created yet, so we initialize the AtomicString
    // hash before trying to use it.
    AtomicString::init();
    initializeDefaultFontFamilies();
    m_page = page; // Page is not yet fully initialized when constructing Settings, so keeping m_page null over initializeDefaultFontFamilies() call.
}

SettingsBase::~SettingsBase()
{
}

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

float SettingsBase::defaultMinimumZoomFontSize()
{
    return 15;
}

#if !PLATFORM(IOS)
bool SettingsBase::defaultTextAutosizingEnabled()
{
    return false;
}
#endif

void SettingsBase::setMediaTypeOverride(const String& mediaTypeOverride)
{
    if (m_mediaTypeOverride == mediaTypeOverride)
        return;

    m_mediaTypeOverride = mediaTypeOverride;

    if (!m_page)
        return;

    FrameView* view = m_page->mainFrame().view();
    ASSERT(view);

    view->setMediaType(mediaTypeOverride);
    m_page->setNeedsRecalcStyleInAllFrames();
}

void SettingsBase::setLoadsImagesAutomatically(bool loadsImagesAutomatically)
{
    m_loadsImagesAutomatically = loadsImagesAutomatically;
    
    // Changing this setting to true might immediately start new loads for images that had previously had loading disabled.
    // If this happens while a WebView is being dealloc'ed, and we don't know the WebView is being dealloc'ed, these new loads
    // can cause crashes downstream when the WebView memory has actually been free'd.
    // One example where this can happen is in Mac apps that subclass WebView then do work in their overridden dealloc methods.
    // Starting these loads synchronously is not important.  By putting it on a 0-delay, properly closing the Page cancels them
    // before they have a chance to really start.
    // See http://webkit.org/b/60572 for more discussion.
    m_setImageLoadingSettingsTimer.startOneShot(0_s);
}

void SettingsBase::imageLoadingSettingsTimerFired()
{
    setImageLoadingSettings(m_page);
}

void SettingsBase::setScriptEnabled(bool isScriptEnabled)
{
    if (m_isScriptEnabled == isScriptEnabled)
        return;

    m_isScriptEnabled = isScriptEnabled;

    if (!m_page)
        return;

#if PLATFORM(IOS)
    m_page->setNeedsRecalcStyleInAllFrames();
#endif
}

void SettingsBase::setJavaEnabled(bool isJavaEnabled)
{
    m_isJavaEnabled = isJavaEnabled;
}

void SettingsBase::setJavaEnabledForLocalFiles(bool isJavaEnabledForLocalFiles)
{
    m_isJavaEnabledForLocalFiles = isJavaEnabledForLocalFiles;
}

void SettingsBase::setImagesEnabled(bool areImagesEnabled)
{
    m_areImagesEnabled = areImagesEnabled;

    // See comment in setLoadsImagesAutomatically.
    m_setImageLoadingSettingsTimer.startOneShot(0_s);
}

void SettingsBase::setPreferMIMETypeForImages(bool preferMIMETypeForImages)
{
    m_preferMIMETypeForImages = preferMIMETypeForImages;
}

void SettingsBase::setForcePendingWebGLPolicy(bool forced)
{
    m_forcePendingWebGLPolicy = forced;
}

void SettingsBase::setPluginsEnabled(bool arePluginsEnabled)
{
    if (m_arePluginsEnabled == arePluginsEnabled)
        return;

    m_arePluginsEnabled = arePluginsEnabled;
    Page::refreshPlugins(false);
}

void SettingsBase::setUserStyleSheetLocation(const URL& userStyleSheetLocation)
{
    if (m_userStyleSheetLocation == userStyleSheetLocation)
        return;

    m_userStyleSheetLocation = userStyleSheetLocation;

    if (m_page)
        m_page->userStyleSheetLocationChanged();
}

// FIXME: This quirk is needed because of Radar 4674537 and 5211271. We need to phase it out once Adobe
// can fix the bug from their end.
void SettingsBase::setNeedsAdobeFrameReloadingQuirk(bool shouldNotReloadIFramesForUnchangedSRC)
{
    m_needsAdobeFrameReloadingQuirk = shouldNotReloadIFramesForUnchangedSRC;
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

void SettingsBase::setUsesPageCache(bool usesPageCache)
{
    if (m_usesPageCache == usesPageCache)
        return;
        
    m_usesPageCache = usesPageCache;

    if (!m_page)
        return;

    if (!m_usesPageCache)
        PageCache::singleton().pruneToSizeNow(0, PruningReason::None);
}

void SettingsBase::setFontRenderingMode(FontRenderingMode mode)
{
    if (fontRenderingMode() == mode)
        return;
    m_fontRenderingMode = static_cast<int>(mode);
    if (m_page)
        m_page->setNeedsRecalcStyleInAllFrames();
}

FontRenderingMode SettingsBase::fontRenderingMode() const
{
    return static_cast<FontRenderingMode>(m_fontRenderingMode);
}

void SettingsBase::setDNSPrefetchingEnabled(bool dnsPrefetchingEnabled)
{
    if (m_dnsPrefetchingEnabled == dnsPrefetchingEnabled)
        return;

    m_dnsPrefetchingEnabled = dnsPrefetchingEnabled;
    if (m_page)
        m_page->dnsPrefetchingStateChanged();
}

void SettingsBase::setShowTiledScrollingIndicator(bool enabled)
{
    if (m_showTiledScrollingIndicator == enabled)
        return;
        
    m_showTiledScrollingIndicator = enabled;
}

#if ENABLE(RESOURCE_USAGE)
void SettingsBase::setResourceUsageOverlayVisible(bool visible)
{
    if (m_resourceUsageOverlayVisible == visible)
        return;

    m_resourceUsageOverlayVisible = visible;
    if (m_page)
        m_page->setResourceUsageOverlayVisible(visible);
}
#endif

#if PLATFORM(WIN)
void SettingsBase::setShouldUseHighResolutionTimers(bool shouldUseHighResolutionTimers)
{
    gShouldUseHighResolutionTimers = shouldUseHighResolutionTimers;
}
#endif

void SettingsBase::setStorageBlockingPolicy(SecurityOrigin::StorageBlockingPolicy enabled)
{
    if (m_storageBlockingPolicy == enabled)
        return;

    m_storageBlockingPolicy = enabled;
    if (m_page)
        m_page->storageBlockingStateChanged();
}

void SettingsBase::setBackgroundShouldExtendBeyondPage(bool shouldExtend)
{
    if (m_backgroundShouldExtendBeyondPage == shouldExtend)
        return;

    m_backgroundShouldExtendBeyondPage = shouldExtend;

    if (m_page)
        m_page->mainFrame().view()->updateExtendBackgroundIfNecessary();
}

#if USE(AVFOUNDATION)
void SettingsBase::setAVFoundationEnabled(bool enabled)
{
    if (gAVFoundationEnabled == enabled)
        return;

    gAVFoundationEnabled = enabled;
    HTMLMediaElement::resetMediaEngines();
}

void SettingsBase::setAVFoundationNSURLSessionEnabled(bool enabled)
{
    if (gAVFoundationNSURLSessionEnabled == enabled)
        return;

    gAVFoundationNSURLSessionEnabled = enabled;
}
#endif

#if PLATFORM(COCOA)
void SettingsBase::setQTKitEnabled(bool enabled)
{
    if (gQTKitEnabled == enabled)
        return;

    gQTKitEnabled = enabled;
    HTMLMediaElement::resetMediaEngines();
}
#endif

#if USE(GSTREAMER)
void SettingsBase::setGStreamerEnabled(bool enabled)
{
    if (gGStreamerEnabled == enabled)
        return;

    gGStreamerEnabled = enabled;
    HTMLMediaElement::resetMediaEngines();
}
#endif

#if ENABLE(MEDIA_STREAM)
bool SettingsBase::mockCaptureDevicesEnabled()
{
    return gMockCaptureDevicesEnabled;
}

void SettingsBase::setMockCaptureDevicesEnabled(bool enabled)
{
    gMockCaptureDevicesEnabled = enabled;
    MockRealtimeMediaSourceCenter::setMockRealtimeMediaSourceCenterEnabled(enabled);
}

bool SettingsBase::mediaCaptureRequiresSecureConnection() const
{
    return gMediaCaptureRequiresSecureConnection;
}

void SettingsBase::setMediaCaptureRequiresSecureConnection(bool mediaCaptureRequiresSecureConnection)
{
    gMediaCaptureRequiresSecureConnection = mediaCaptureRequiresSecureConnection;
}
#endif

void SettingsBase::setScrollingPerformanceLoggingEnabled(bool enabled)
{
    m_scrollingPerformanceLoggingEnabled = enabled;

    if (m_page && m_page->mainFrame().view())
        m_page->mainFrame().view()->setScrollingPerformanceLoggingEnabled(enabled);
}

// It's very important that this setting doesn't change in the middle of a document's lifetime.
// The Mac port uses this flag when registering and deregistering platform-dependent scrollbar
// objects. Therefore, if this changes at an unexpected time, deregistration may not happen
// correctly, which may cause the platform to follow dangling pointers.
void SettingsBase::setMockScrollbarsEnabled(bool flag)
{
    gMockScrollbarsEnabled = flag;
    // FIXME: This should update scroll bars in existing pages.
}

bool SettingsBase::mockScrollbarsEnabled()
{
    return gMockScrollbarsEnabled;
}

void SettingsBase::setUsesOverlayScrollbars(bool flag)
{
    gUsesOverlayScrollbars = flag;
    // FIXME: This should update scroll bars in existing pages.
}

bool SettingsBase::usesOverlayScrollbars()
{
    return gUsesOverlayScrollbars;
}

void SettingsBase::setUsesMockScrollAnimator(bool flag)
{
    gMockScrollAnimatorEnabled = flag;
}

bool SettingsBase::usesMockScrollAnimator()
{
    return gMockScrollAnimatorEnabled;
}

void SettingsBase::setShouldRespectPriorityInCSSAttributeSetters(bool flag)
{
    gShouldRespectPriorityInCSSAttributeSetters = flag;
}

bool SettingsBase::shouldRespectPriorityInCSSAttributeSetters()
{
    return gShouldRespectPriorityInCSSAttributeSetters;
}

void SettingsBase::setHiddenPageDOMTimerThrottlingEnabled(bool flag)
{
    if (m_hiddenPageDOMTimerThrottlingEnabled == flag)
        return;
    m_hiddenPageDOMTimerThrottlingEnabled = flag;
    if (m_page)
        m_page->hiddenPageDOMTimerThrottlingStateChanged();
}

void SettingsBase::setHiddenPageDOMTimerThrottlingAutoIncreases(bool flag)
{
    if (m_hiddenPageDOMTimerThrottlingAutoIncreases == flag)
        return;
    m_hiddenPageDOMTimerThrottlingAutoIncreases = flag;
    if (m_page)
        m_page->hiddenPageDOMTimerThrottlingStateChanged();
}

void SettingsBase::setHiddenPageCSSAnimationSuspensionEnabled(bool flag)
{
    if (m_hiddenPageCSSAnimationSuspensionEnabled == flag)
        return;
    m_hiddenPageCSSAnimationSuspensionEnabled = flag;
    if (m_page)
        m_page->hiddenPageCSSAnimationSuspensionStateChanged();
}

void SettingsBase::setFontFallbackPrefersPictographs(bool preferPictographs)
{
    if (m_fontFallbackPrefersPictographs == preferPictographs)
        return;

    m_fontFallbackPrefersPictographs = preferPictographs;
    if (m_page)
        m_page->setNeedsRecalcStyleInAllFrames();
}

void SettingsBase::setLowPowerVideoAudioBufferSizeEnabled(bool flag)
{
    gLowPowerVideoAudioBufferSizeEnabled = flag;
}

void SettingsBase::setResourceLoadStatisticsEnabled(bool flag)
{
    gResourceLoadStatisticsEnabledEnabled = flag;
}

#if PLATFORM(IOS)
void SettingsBase::setAudioSessionCategoryOverride(unsigned sessionCategory)
{
    AudioSession::sharedSession().setCategoryOverride(static_cast<AudioSession::CategoryType>(sessionCategory));
}

unsigned SettingsBase::audioSessionCategoryOverride()
{
    return AudioSession::sharedSession().categoryOverride();
}

void SettingsBase::setNetworkDataUsageTrackingEnabled(bool trackingEnabled)
{
    gNetworkDataUsageTrackingEnabled = trackingEnabled;
}

bool SettingsBase::networkDataUsageTrackingEnabled()
{
    return gNetworkDataUsageTrackingEnabled;
}

static String& sharedNetworkInterfaceNameGlobal()
{
    static NeverDestroyed<String> networkInterfaceName;
    return networkInterfaceName;
}

void SettingsBase::setNetworkInterfaceName(const String& networkInterfaceName)
{
    sharedNetworkInterfaceNameGlobal() = networkInterfaceName;
}

const String& SettingsBase::networkInterfaceName()
{
    return sharedNetworkInterfaceNameGlobal();
}
#endif

bool SettingsBase::globalConstRedeclarationShouldThrow()
{
#if PLATFORM(MAC)
    return !MacApplication::isIBooks();
#elif PLATFORM(IOS)
    return !IOSApplication::isIBooks();
#else
    return true;
#endif
}

void SettingsBase::setAllowsAnySSLCertificate(bool allowAnySSLCertificate)
{
    gAllowsAnySSLCertificate = allowAnySSLCertificate;
}

bool SettingsBase::allowsAnySSLCertificate()
{
    return gAllowsAnySSLCertificate;
}

#if !PLATFORM(COCOA)
const String& SettingsBase::defaultMediaContentTypesRequiringHardwareSupport()
{
    return emptyString();
}
#endif

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

} // namespace WebCore
