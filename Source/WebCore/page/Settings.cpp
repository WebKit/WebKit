/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "Settings.h"

#include "BackForwardController.h"
#include "CachedResourceLoader.h"
#include "CookieStorage.h"
#include "DOMTimer.h"
#include "Database.h"
#include "Document.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HistoryItem.h"
#include "Page.h"
#include "PageCache.h"
#include "ResourceHandle.h"
#include "StorageMap.h"
#include <limits>

using namespace std;

namespace WebCore {

static void setLoadsImagesAutomaticallyInAllFrames(Page* page)
{
    for (Frame* frame = page->mainFrame(); frame; frame = frame->tree()->traverseNext())
        frame->document()->cachedResourceLoader()->setAutoLoadImages(page->settings()->loadsImagesAutomatically());
}

static inline void setGenericFontFamilyMap(ScriptFontFamilyMap& fontMap, const AtomicString& family, UScriptCode script, Page* page)
{
    ScriptFontFamilyMap::iterator it = fontMap.find(static_cast<int>(script));
    if (it != fontMap.end() && it->second == family)
        return;
    fontMap.set(static_cast<int>(script), family);
    page->setNeedsRecalcStyleInAllFrames();
}

static inline const AtomicString& getGenericFontFamilyForScript(const ScriptFontFamilyMap& fontMap, UScriptCode script)
{
    ScriptFontFamilyMap::const_iterator it = fontMap.find(static_cast<int>(script));
    if (it != fontMap.end())
        return it->second;
    if (script != USCRIPT_COMMON)
        return getGenericFontFamilyForScript(fontMap, USCRIPT_COMMON);
    return emptyAtom;
}

#if USE(SAFARI_THEME)
bool Settings::gShouldPaintNativeControls = true;
#endif

#if USE(AVFOUNDATION)
bool Settings::gAVFoundationEnabled(false);
#endif

bool Settings::gMockScrollbarsEnabled = false;

#if PLATFORM(WIN) || (OS(WINDOWS) && PLATFORM(WX))
bool Settings::gShouldUseHighResolutionTimers = true;
#endif

// NOTEs
//  1) EditingMacBehavior comprises Tiger, Leopard, SnowLeopard and iOS builds, as well QtWebKit and Chromium when built on Mac;
//  2) EditingWindowsBehavior comprises Win32 and WinCE builds, as well as QtWebKit and Chromium when built on Windows;
//  3) EditingUnixBehavior comprises all unix-based systems, but Darwin/MacOS (and then abusing the terminology);
// 99) MacEditingBehavior is used a fallback.
static EditingBehaviorType editingBehaviorTypeForPlatform()
{
    return
#if OS(DARWIN)
    EditingMacBehavior
#elif OS(WINDOWS)
    EditingWindowsBehavior
#elif OS(UNIX)
    EditingUnixBehavior
#else
    // Fallback
    EditingMacBehavior
#endif
    ;
}

Settings::Settings(Page* page)
    : m_page(page)
    , m_editableLinkBehavior(EditableLinkDefaultBehavior)
    , m_textDirectionSubmenuInclusionBehavior(TextDirectionSubmenuAutomaticallyIncluded)
    , m_passwordEchoDurationInSeconds(1)
    , m_minimumFontSize(0)
    , m_minimumLogicalFontSize(0)
    , m_defaultFontSize(0)
    , m_defaultFixedFontSize(0)
    , m_validationMessageTimerMagnification(50)
    , m_minimumAccelerated2dCanvasSize(128 * 128)
    , m_layoutFallbackWidth(980)
    , m_deviceDPI(240)
    , m_maximumDecodedImageSize(numeric_limits<size_t>::max())
    , m_deviceWidth(480)
    , m_deviceHeight(854)
    , m_sessionStorageQuota(StorageMap::noQuota)
    , m_editingBehaviorType(editingBehaviorTypeForPlatform())
    , m_maximumHTMLParserDOMTreeDepth(defaultMaximumHTMLParserDOMTreeDepth)
    , m_isSpatialNavigationEnabled(false)
    , m_isJavaEnabled(false)
    , m_loadsImagesAutomatically(false)
    , m_loadsSiteIconsIgnoringImageLoadingSetting(false)
    , m_privateBrowsingEnabled(false)
    , m_caretBrowsingEnabled(false)
    , m_areImagesEnabled(true)
    , m_isMediaEnabled(true)
    , m_arePluginsEnabled(false)
    , m_localStorageEnabled(false)
    , m_isScriptEnabled(false)
    , m_isWebSecurityEnabled(true)
    , m_allowUniversalAccessFromFileURLs(true)
    , m_allowFileAccessFromFileURLs(true)
    , m_javaScriptCanOpenWindowsAutomatically(false)
    , m_javaScriptCanAccessClipboard(false)
    , m_shouldPrintBackgrounds(false)
    , m_textAreasAreResizable(false)
#if ENABLE(DASHBOARD_SUPPORT)
    , m_usesDashboardBackwardCompatibilityMode(false)
#endif
    , m_needsAdobeFrameReloadingQuirk(false)
    , m_needsKeyboardEventDisambiguationQuirks(false)
    , m_treatsAnyTextCSSLinkAsStylesheet(false)
    , m_needsLeopardMailQuirks(false)
    , m_isDOMPasteAllowed(false)
    , m_shrinksStandaloneImagesToFit(true)
    , m_usesPageCache(false)
    , m_pageCacheSupportsPlugins(false)
    , m_showsURLsInToolTips(false)
    , m_showsToolTipOverTruncatedText(false)
    , m_forceFTPDirectoryListings(false)
    , m_developerExtrasEnabled(false)
    , m_authorAndUserStylesEnabled(true)
    , m_needsSiteSpecificQuirks(false)
    , m_fontRenderingMode(0)
    , m_frameFlatteningEnabled(false)
    , m_webArchiveDebugModeEnabled(false)
    , m_localFileContentSniffingEnabled(false)
    , m_inApplicationChromeMode(false)
    , m_offlineWebApplicationCacheEnabled(false)
    , m_enforceCSSMIMETypeInNoQuirksMode(true)
    , m_usesEncodingDetector(false)
    , m_allowScriptsToCloseWindows(false)
    , m_canvasUsesAcceleratedDrawing(false)
    , m_acceleratedDrawingEnabled(false)
    , m_acceleratedFiltersEnabled(false)
    // FIXME: This should really be disabled by default as it makes platforms that don't support the feature download files
    // they can't use by. Leaving enabled for now to not change existing behavior.
    , m_downloadableBinaryFontsEnabled(true)
    , m_xssAuditorEnabled(false)
    , m_acceleratedCompositingEnabled(true)
    , m_acceleratedCompositingFor3DTransformsEnabled(true)
    , m_acceleratedCompositingForVideoEnabled(true)
    , m_acceleratedCompositingForPluginsEnabled(true)
    , m_acceleratedCompositingForCanvasEnabled(true)
    , m_acceleratedCompositingForAnimationEnabled(true)
    , m_acceleratedCompositingForFixedPositionEnabled(false)
    , m_acceleratedCompositingForScrollableFramesEnabled(false)
    , m_showDebugBorders(false)
    , m_showRepaintCounter(false)
    , m_experimentalNotificationsEnabled(false)
    , m_webGLEnabled(false)
    , m_openGLMultisamplingEnabled(true)
    , m_privilegedWebGLExtensionsEnabled(false)
    , m_webAudioEnabled(false)
    , m_acceleratedCanvas2dEnabled(false)
    , m_loadDeferringEnabled(true)
    , m_tiledBackingStoreEnabled(false)
    , m_paginateDuringLayoutEnabled(false)
    , m_dnsPrefetchingEnabled(false)
#if ENABLE(FULLSCREEN_API)
    , m_fullScreenAPIEnabled(false)
#endif
    , m_asynchronousSpellCheckingEnabled(false)
#if USE(UNIFIED_TEXT_CHECKING)
    , m_unifiedTextCheckerEnabled(true)
#else
    , m_unifiedTextCheckerEnabled(false)
#endif
    , m_memoryInfoEnabled(false)
    , m_interactiveFormValidation(false)
    , m_usePreHTML5ParserQuirks(false)
    , m_hyperlinkAuditingEnabled(false)
    , m_crossOriginCheckInGetMatchedCSSRulesDisabled(false)
    , m_forceCompositingMode(false)
    , m_shouldInjectUserScriptsInInitialEmptyDocument(false)
    , m_allowDisplayOfInsecureContent(true)
    , m_allowRunningOfInsecureContent(true)
#if ENABLE(SMOOTH_SCROLLING)
    , m_scrollAnimatorEnabled(true)
#endif
#if ENABLE(WEB_SOCKETS)
    , m_useHixie76WebSocketProtocol(true)
#endif
    , m_mediaPlaybackRequiresUserGesture(false)
    , m_mediaPlaybackAllowsInline(true)
    , m_passwordEchoEnabled(false)
    , m_suppressIncrementalRendering(false)
    , m_backspaceKeyNavigationEnabled(true)
    , m_visualWordMovementEnabled(false)
#if ENABLE(VIDEO_TRACK)
    , m_shouldDisplaySubtitles(false)
    , m_shouldDisplayCaptions(false)
    , m_shouldDisplayTextDescriptions(false)
#endif
    , m_perTileDrawingEnabled(false)
    , m_partialSwapEnabled(false)
#if ENABLE(THREADED_SCROLLING)
    , m_scrollingCoordinatorEnabled(false)
#endif
    , m_loadsImagesAutomaticallyTimer(this, &Settings::loadsImagesAutomaticallyTimerFired)
{
    // A Frame may not have been created yet, so we initialize the AtomicString 
    // hash before trying to use it.
    AtomicString::init();
}

PassOwnPtr<Settings> Settings::create(Page* page)
{
    return adoptPtr(new Settings(page));
} 

const AtomicString& Settings::standardFontFamily(UScriptCode script) const
{
    return getGenericFontFamilyForScript(m_standardFontFamilyMap, script);
}

void Settings::setStandardFontFamily(const AtomicString& family, UScriptCode script)
{
    setGenericFontFamilyMap(m_standardFontFamilyMap, family, script, m_page);
}

const AtomicString& Settings::fixedFontFamily(UScriptCode script) const
{
    return getGenericFontFamilyForScript(m_fixedFontFamilyMap, script);
}

void Settings::setFixedFontFamily(const AtomicString& family, UScriptCode script)
{
    setGenericFontFamilyMap(m_fixedFontFamilyMap, family, script, m_page);
}

const AtomicString& Settings::serifFontFamily(UScriptCode script) const
{
    return getGenericFontFamilyForScript(m_serifFontFamilyMap, script);
}

void Settings::setSerifFontFamily(const AtomicString& family, UScriptCode script)
{
     setGenericFontFamilyMap(m_serifFontFamilyMap, family, script, m_page);
}

const AtomicString& Settings::sansSerifFontFamily(UScriptCode script) const
{
    return getGenericFontFamilyForScript(m_sansSerifFontFamilyMap, script);
}

void Settings::setSansSerifFontFamily(const AtomicString& family, UScriptCode script)
{
    setGenericFontFamilyMap(m_sansSerifFontFamilyMap, family, script, m_page);
}

const AtomicString& Settings::cursiveFontFamily(UScriptCode script) const
{
    return getGenericFontFamilyForScript(m_cursiveFontFamilyMap, script);
}

void Settings::setCursiveFontFamily(const AtomicString& family, UScriptCode script)
{
    setGenericFontFamilyMap(m_cursiveFontFamilyMap, family, script, m_page);
}

const AtomicString& Settings::fantasyFontFamily(UScriptCode script) const
{
    return getGenericFontFamilyForScript(m_fantasyFontFamilyMap, script);
}

void Settings::setFantasyFontFamily(const AtomicString& family, UScriptCode script)
{
    setGenericFontFamilyMap(m_fantasyFontFamilyMap, family, script, m_page);
}

const AtomicString& Settings::pictographFontFamily(UScriptCode script) const
{
    return getGenericFontFamilyForScript(m_pictographFontFamilyMap, script);
}

void Settings::setPictographFontFamily(const AtomicString& family, UScriptCode script)
{
    setGenericFontFamilyMap(m_pictographFontFamilyMap, family, script, m_page);
}

void Settings::setMinimumFontSize(int minimumFontSize)
{
    if (m_minimumFontSize == minimumFontSize)
        return;

    m_minimumFontSize = minimumFontSize;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setMinimumLogicalFontSize(int minimumLogicalFontSize)
{
    if (m_minimumLogicalFontSize == minimumLogicalFontSize)
        return;

    m_minimumLogicalFontSize = minimumLogicalFontSize;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setDefaultFontSize(int defaultFontSize)
{
    if (m_defaultFontSize == defaultFontSize)
        return;

    m_defaultFontSize = defaultFontSize;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setDefaultFixedFontSize(int defaultFontSize)
{
    if (m_defaultFixedFontSize == defaultFontSize)
        return;

    m_defaultFixedFontSize = defaultFontSize;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setLoadsImagesAutomatically(bool loadsImagesAutomatically)
{
    m_loadsImagesAutomatically = loadsImagesAutomatically;
    
    // Changing this setting to true might immediately start new loads for images that had previously had loading disabled.
    // If this happens while a WebView is being dealloc'ed, and we don't know the WebView is being dealloc'ed, these new loads
    // can cause crashes downstream when the WebView memory has actually been free'd.
    // One example where this can happen is in Mac apps that subclass WebView then do work in their overridden dealloc methods.
    // Starting these loads synchronously is not important.  By putting it on a 0-delay, properly closing the Page cancels them
    // before they have a chance to really start.
    // See http://webkit.org/b/60572 for more discussion.
    m_loadsImagesAutomaticallyTimer.startOneShot(0);
}

void Settings::loadsImagesAutomaticallyTimerFired(Timer<Settings>*)
{
    setLoadsImagesAutomaticallyInAllFrames(m_page);
}

void Settings::setLoadsSiteIconsIgnoringImageLoadingSetting(bool loadsSiteIcons)
{
    m_loadsSiteIconsIgnoringImageLoadingSetting = loadsSiteIcons;
}

void Settings::setScriptEnabled(bool isScriptEnabled)
{
    m_isScriptEnabled = isScriptEnabled;
}

void Settings::setWebSecurityEnabled(bool isWebSecurityEnabled)
{
    m_isWebSecurityEnabled = isWebSecurityEnabled;
}

void Settings::setAllowUniversalAccessFromFileURLs(bool allowUniversalAccessFromFileURLs)
{
    m_allowUniversalAccessFromFileURLs = allowUniversalAccessFromFileURLs;
}

void Settings::setAllowFileAccessFromFileURLs(bool allowFileAccessFromFileURLs)
{
    m_allowFileAccessFromFileURLs = allowFileAccessFromFileURLs;
}

void Settings::setSpatialNavigationEnabled(bool isSpatialNavigationEnabled)
{
    m_isSpatialNavigationEnabled = isSpatialNavigationEnabled;
}

void Settings::setJavaEnabled(bool isJavaEnabled)
{
    m_isJavaEnabled = isJavaEnabled;
}

void Settings::setImagesEnabled(bool areImagesEnabled)
{
    m_areImagesEnabled = areImagesEnabled;
}

void Settings::setMediaEnabled(bool isMediaEnabled)
{
    m_isMediaEnabled = isMediaEnabled;
}

void Settings::setPluginsEnabled(bool arePluginsEnabled)
{
    m_arePluginsEnabled = arePluginsEnabled;
}

void Settings::setLocalStorageEnabled(bool localStorageEnabled)
{
    m_localStorageEnabled = localStorageEnabled;
}

void Settings::setSessionStorageQuota(unsigned sessionStorageQuota)
{
    m_sessionStorageQuota = sessionStorageQuota;
}

void Settings::setPrivateBrowsingEnabled(bool privateBrowsingEnabled)
{
    // FIXME http://webkit.org/b/67870: The private browsing storage session and cookie private
    // browsing mode (which is used if storage sessions are not available) are global settings, so
    // it is misleading to have them as per-page settings.
    // In addition, if they are treated as a per Page settings, the global values can get out of
    // sync with the per Page value in the following situation:
    // 1. The global values get set to true when setPrivateBrowsingEnabled(true) is called.
    // 2. All Pages are closed, so all Settings objects go away.
    // 3. A new Page is created, and a corresponding new Settings object is created - with
    //    m_privateBrowsingEnabled starting out as false in the constructor.
    // 4. The WebPage settings get applied to the new Page and setPrivateBrowsingEnabled(false)
    //    is called, but an if (m_privateBrowsingEnabled == privateBrowsingEnabled) early return
    //    prevents the global values from getting changed from true to false.
#if USE(CFURLSTORAGESESSIONS)
    ResourceHandle::setPrivateBrowsingEnabled(privateBrowsingEnabled);
#endif
    setCookieStoragePrivateBrowsingEnabled(privateBrowsingEnabled);

    if (m_privateBrowsingEnabled == privateBrowsingEnabled)
        return;

    m_privateBrowsingEnabled = privateBrowsingEnabled;
    m_page->privateBrowsingStateChanged();
}

void Settings::setJavaScriptCanOpenWindowsAutomatically(bool javaScriptCanOpenWindowsAutomatically)
{
    m_javaScriptCanOpenWindowsAutomatically = javaScriptCanOpenWindowsAutomatically;
}

void Settings::setJavaScriptCanAccessClipboard(bool javaScriptCanAccessClipboard)
{
    m_javaScriptCanAccessClipboard = javaScriptCanAccessClipboard;
}

void Settings::setDefaultTextEncodingName(const String& defaultTextEncodingName)
{
    m_defaultTextEncodingName = defaultTextEncodingName;
}

void Settings::setUserStyleSheetLocation(const KURL& userStyleSheetLocation)
{
    if (m_userStyleSheetLocation == userStyleSheetLocation)
        return;

    m_userStyleSheetLocation = userStyleSheetLocation;

    m_page->userStyleSheetLocationChanged();
}

void Settings::setShouldPrintBackgrounds(bool shouldPrintBackgrounds)
{
    m_shouldPrintBackgrounds = shouldPrintBackgrounds;
}

void Settings::setTextAreasAreResizable(bool textAreasAreResizable)
{
    if (m_textAreasAreResizable == textAreasAreResizable)
        return;

    m_textAreasAreResizable = textAreasAreResizable;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setEditableLinkBehavior(EditableLinkBehavior editableLinkBehavior)
{
    m_editableLinkBehavior = editableLinkBehavior;
}

void Settings::setTextDirectionSubmenuInclusionBehavior(TextDirectionSubmenuInclusionBehavior behavior)
{
    m_textDirectionSubmenuInclusionBehavior = behavior;
}

#if ENABLE(DASHBOARD_SUPPORT)
void Settings::setUsesDashboardBackwardCompatibilityMode(bool usesDashboardBackwardCompatibilityMode)
{
    m_usesDashboardBackwardCompatibilityMode = usesDashboardBackwardCompatibilityMode;
}
#endif

// FIXME: This quirk is needed because of Radar 4674537 and 5211271. We need to phase it out once Adobe
// can fix the bug from their end.
void Settings::setNeedsAdobeFrameReloadingQuirk(bool shouldNotReloadIFramesForUnchangedSRC)
{
    m_needsAdobeFrameReloadingQuirk = shouldNotReloadIFramesForUnchangedSRC;
}

// This is a quirk we are pro-actively applying to old applications. It changes keyboard event dispatching,
// making keyIdentifier available on keypress events, making charCode available on keydown/keyup events,
// and getting keypress dispatched in more cases.
void Settings::setNeedsKeyboardEventDisambiguationQuirks(bool needsQuirks)
{
    m_needsKeyboardEventDisambiguationQuirks = needsQuirks;
}

void Settings::setTreatsAnyTextCSSLinkAsStylesheet(bool treatsAnyTextCSSLinkAsStylesheet)
{
    m_treatsAnyTextCSSLinkAsStylesheet = treatsAnyTextCSSLinkAsStylesheet;
}

void Settings::setNeedsLeopardMailQuirks(bool needsQuirks)
{
    m_needsLeopardMailQuirks = needsQuirks;
}

void Settings::setDOMPasteAllowed(bool DOMPasteAllowed)
{
    m_isDOMPasteAllowed = DOMPasteAllowed;
}

void Settings::setDefaultMinDOMTimerInterval(double interval)
{
    DOMTimer::setDefaultMinTimerInterval(interval);
}

double Settings::defaultMinDOMTimerInterval()
{
    return DOMTimer::defaultMinTimerInterval();
}

void Settings::setMinDOMTimerInterval(double interval)
{
    m_page->setMinimumTimerInterval(interval);
}

double Settings::minDOMTimerInterval()
{
    return m_page->minimumTimerInterval();
}

void Settings::setUsesPageCache(bool usesPageCache)
{
    if (m_usesPageCache == usesPageCache)
        return;
        
    m_usesPageCache = usesPageCache;
    if (!m_usesPageCache) {
        int first = -m_page->backForward()->backCount();
        int last = m_page->backForward()->forwardCount();
        for (int i = first; i <= last; i++)
            pageCache()->remove(m_page->backForward()->itemAtIndex(i));
        pageCache()->releaseAutoreleasedPagesNow();
    }
}

void Settings::setShrinksStandaloneImagesToFit(bool shrinksStandaloneImagesToFit)
{
    m_shrinksStandaloneImagesToFit = shrinksStandaloneImagesToFit;
}

void Settings::setShowsURLsInToolTips(bool showsURLsInToolTips)
{
    m_showsURLsInToolTips = showsURLsInToolTips;
}

void Settings::setShowsToolTipOverTruncatedText(bool showsToolTipForTruncatedText)
{
    m_showsToolTipOverTruncatedText = showsToolTipForTruncatedText;
}

void Settings::setFTPDirectoryTemplatePath(const String& path)
{
    m_ftpDirectoryTemplatePath = path;
}

void Settings::setForceFTPDirectoryListings(bool force)
{
    m_forceFTPDirectoryListings = force;
}

void Settings::setDeveloperExtrasEnabled(bool developerExtrasEnabled)
{
    m_developerExtrasEnabled = developerExtrasEnabled;
}

void Settings::setAuthorAndUserStylesEnabled(bool authorAndUserStylesEnabled)
{
    if (m_authorAndUserStylesEnabled == authorAndUserStylesEnabled)
        return;

    m_authorAndUserStylesEnabled = authorAndUserStylesEnabled;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setFontRenderingMode(FontRenderingMode mode)
{
    if (fontRenderingMode() == mode)
        return;
    m_fontRenderingMode = mode;
    m_page->setNeedsRecalcStyleInAllFrames();
}

FontRenderingMode Settings::fontRenderingMode() const
{
    return static_cast<FontRenderingMode>(m_fontRenderingMode);
}

void Settings::setNeedsSiteSpecificQuirks(bool needsQuirks)
{
    m_needsSiteSpecificQuirks = needsQuirks;
}

void Settings::setFrameFlatteningEnabled(bool frameFlatteningEnabled)
{
    m_frameFlatteningEnabled = frameFlatteningEnabled;
}

#if ENABLE(WEB_ARCHIVE)
void Settings::setWebArchiveDebugModeEnabled(bool enabled)
{
    m_webArchiveDebugModeEnabled = enabled;
}
#endif

void Settings::setLocalFileContentSniffingEnabled(bool enabled)
{
    m_localFileContentSniffingEnabled = enabled;
}

void Settings::setLocalStorageDatabasePath(const String& path)
{
    m_localStorageDatabasePath = path;
}

void Settings::setApplicationChromeMode(bool mode)
{
    m_inApplicationChromeMode = mode;
}

void Settings::setOfflineWebApplicationCacheEnabled(bool enabled)
{
    m_offlineWebApplicationCacheEnabled = enabled;
}

void Settings::setEnforceCSSMIMETypeInNoQuirksMode(bool enforceCSSMIMETypeInNoQuirksMode)
{
    m_enforceCSSMIMETypeInNoQuirksMode = enforceCSSMIMETypeInNoQuirksMode;
}

#if USE(SAFARI_THEME)
void Settings::setShouldPaintNativeControls(bool shouldPaintNativeControls)
{
    gShouldPaintNativeControls = shouldPaintNativeControls;
}
#endif

void Settings::setUsesEncodingDetector(bool usesEncodingDetector)
{
    m_usesEncodingDetector = usesEncodingDetector;
}

void Settings::setDNSPrefetchingEnabled(bool dnsPrefetchingEnabled)
{
    if (m_dnsPrefetchingEnabled == dnsPrefetchingEnabled)
        return;

    m_dnsPrefetchingEnabled = dnsPrefetchingEnabled;
    m_page->dnsPrefetchingStateChanged();
}

void Settings::setAllowScriptsToCloseWindows(bool allowScriptsToCloseWindows)
{
    m_allowScriptsToCloseWindows = allowScriptsToCloseWindows;
}

void Settings::setCaretBrowsingEnabled(bool caretBrowsingEnabled)
{
    m_caretBrowsingEnabled = caretBrowsingEnabled;
}

void Settings::setDownloadableBinaryFontsEnabled(bool downloadableBinaryFontsEnabled)
{
    m_downloadableBinaryFontsEnabled = downloadableBinaryFontsEnabled;
}

void Settings::setXSSAuditorEnabled(bool xssAuditorEnabled)
{
    m_xssAuditorEnabled = xssAuditorEnabled;
}

void Settings::setAcceleratedCompositingEnabled(bool enabled)
{
    if (m_acceleratedCompositingEnabled == enabled)
        return;
        
    m_acceleratedCompositingEnabled = enabled;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setCanvasUsesAcceleratedDrawing(bool enabled)
{
    m_canvasUsesAcceleratedDrawing = enabled;
}

void Settings::setAcceleratedCompositingFor3DTransformsEnabled(bool enabled)
{
    m_acceleratedCompositingFor3DTransformsEnabled = enabled;
}

void Settings::setAcceleratedCompositingForVideoEnabled(bool enabled)
{
    m_acceleratedCompositingForVideoEnabled = enabled;
}

void Settings::setAcceleratedCompositingForPluginsEnabled(bool enabled)
{
    m_acceleratedCompositingForPluginsEnabled = enabled;
}

void Settings::setAcceleratedCompositingForCanvasEnabled(bool enabled)
{
    m_acceleratedCompositingForCanvasEnabled = enabled;
}

void Settings::setAcceleratedCompositingForAnimationEnabled(bool enabled)
{
    m_acceleratedCompositingForAnimationEnabled = enabled;
}

void Settings::setShowDebugBorders(bool enabled)
{
    if (m_showDebugBorders == enabled)
        return;
        
    m_showDebugBorders = enabled;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setShowRepaintCounter(bool enabled)
{
    if (m_showRepaintCounter == enabled)
        return;
        
    m_showRepaintCounter = enabled;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setExperimentalNotificationsEnabled(bool enabled)
{
    m_experimentalNotificationsEnabled = enabled;
}

#if PLATFORM(WIN) || (OS(WINDOWS) && PLATFORM(WX))
void Settings::setShouldUseHighResolutionTimers(bool shouldUseHighResolutionTimers)
{
    gShouldUseHighResolutionTimers = shouldUseHighResolutionTimers;
}
#endif

void Settings::setWebAudioEnabled(bool enabled)
{
    m_webAudioEnabled = enabled;
}

void Settings::setWebGLEnabled(bool enabled)
{
    m_webGLEnabled = enabled;
}

void Settings::setOpenGLMultisamplingEnabled(bool enabled)
{
    m_openGLMultisamplingEnabled = enabled;
}

void Settings::setPrivilegedWebGLExtensionsEnabled(bool enabled)
{
    m_privilegedWebGLExtensionsEnabled = enabled;
}

void Settings::setAccelerated2dCanvasEnabled(bool enabled)
{
    m_acceleratedCanvas2dEnabled = enabled;
}

void Settings::setMinimumAccelerated2dCanvasSize(int numPixels)
{
    m_minimumAccelerated2dCanvasSize = numPixels;
}

void Settings::setLoadDeferringEnabled(bool enabled)
{
    m_loadDeferringEnabled = enabled;
}

void Settings::setTiledBackingStoreEnabled(bool enabled)
{
    m_tiledBackingStoreEnabled = enabled;
#if USE(TILED_BACKING_STORE)
    if (m_page->mainFrame())
        m_page->mainFrame()->setTiledBackingStoreEnabled(enabled);
#endif
}

void Settings::setMockScrollbarsEnabled(bool flag)
{
    gMockScrollbarsEnabled = flag;
}

bool Settings::mockScrollbarsEnabled()
{
    return gMockScrollbarsEnabled;
}

} // namespace WebCore
