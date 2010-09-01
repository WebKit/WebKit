/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "BackForwardList.h"
#include "Database.h"
#include "DocLoader.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HistoryItem.h"
#include "Page.h"
#include "PageCache.h"
#include "StorageMap.h"
#include <limits>

using namespace std;

namespace WebCore {

static void setNeedsRecalcStyleInAllFrames(Page* page)
{
    for (Frame* frame = page->mainFrame(); frame; frame = frame->tree()->traverseNext())
        frame->document()->styleSelectorChanged(DeferRecalcStyle);
}

static void setLoadsImagesAutomaticallyInAllFrames(Page* page)
{
    for (Frame* frame = page->mainFrame(); frame; frame = frame->tree()->traverseNext())
        frame->document()->docLoader()->setAutoLoadImages(page->settings()->loadsImagesAutomatically());
}

#if USE(SAFARI_THEME)
bool Settings::gShouldPaintNativeControls = true;
#endif

#if PLATFORM(WIN) || (OS(WINDOWS) && PLATFORM(WX))
bool Settings::gShouldUseHighResolutionTimers = true;
#endif

Settings::Settings(Page* page)
    : m_page(page)
    , m_editableLinkBehavior(EditableLinkDefaultBehavior)
    , m_textDirectionSubmenuInclusionBehavior(TextDirectionSubmenuAutomaticallyIncluded)
    , m_minimumFontSize(0)
    , m_minimumLogicalFontSize(0)
    , m_defaultFontSize(0)
    , m_defaultFixedFontSize(0)
    , m_maximumDecodedImageSize(numeric_limits<size_t>::max())
#if ENABLE(DOM_STORAGE)
    , m_sessionStorageQuota(StorageMap::noQuota)
#endif
    , m_pluginAllowedRunTime(numeric_limits<unsigned>::max())
    , m_zoomMode(ZoomPage)
    , m_isSpatialNavigationEnabled(false)
    , m_isJavaEnabled(false)
    , m_loadsImagesAutomatically(false)
    , m_privateBrowsingEnabled(false)
    , m_caretBrowsingEnabled(false)
    , m_areImagesEnabled(true)
    , m_isMediaEnabled(true)
    , m_arePluginsEnabled(false)
    , m_localStorageEnabled(false)
    , m_isJavaScriptEnabled(false)
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
    , m_needsTigerMailQuirks(false)
    , m_isDOMPasteAllowed(false)
    , m_shrinksStandaloneImagesToFit(true)
    , m_usesPageCache(false)
    , m_showsURLsInToolTips(false)
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
    , m_shouldPaintCustomScrollbars(false)
    , m_enforceCSSMIMETypeInNoQuirksMode(true)
    , m_usesEncodingDetector(false)
    , m_allowScriptsToCloseWindows(false)
    , m_editingBehaviorType(
#if PLATFORM(MAC) || (PLATFORM(CHROMIUM) && OS(DARWIN))
        // (PLATFORM(MAC) is always false in Chromium, hence the extra condition.)
        EditingMacBehavior
#else
        EditingWindowsBehavior
#endif
        )
    // FIXME: This should really be disabled by default as it makes platforms that don't support the feature download files
    // they can't use by. Leaving enabled for now to not change existing behavior.
    , m_downloadableBinaryFontsEnabled(true)
    , m_xssAuditorEnabled(false)
    , m_acceleratedCompositingEnabled(true)
    , m_showDebugBorders(false)
    , m_showRepaintCounter(false)
    , m_experimentalNotificationsEnabled(false)
    , m_webGLEnabled(false)
    , m_acceleratedCanvas2dEnabled(false)
    , m_loadDeferringEnabled(true)
    , m_tiledBackingStoreEnabled(false)
    , m_paginateDuringLayoutEnabled(false)
    , m_dnsPrefetchingEnabled(true)
#if ENABLE(FULLSCREEN_API)
    , m_fullScreenAPIEnabled(false)
#endif
    , m_memoryInfoEnabled(false)
    , m_interactiveFormValidation(false)
{
    // A Frame may not have been created yet, so we initialize the AtomicString 
    // hash before trying to use it.
    AtomicString::init();
}

void Settings::setStandardFontFamily(const AtomicString& standardFontFamily)
{
    if (standardFontFamily == m_standardFontFamily)
        return;

    m_standardFontFamily = standardFontFamily;
    setNeedsRecalcStyleInAllFrames(m_page);
}

void Settings::setFixedFontFamily(const AtomicString& fixedFontFamily)
{
    if (m_fixedFontFamily == fixedFontFamily)
        return;
        
    m_fixedFontFamily = fixedFontFamily;
    setNeedsRecalcStyleInAllFrames(m_page);
}

void Settings::setSerifFontFamily(const AtomicString& serifFontFamily)
{
    if (m_serifFontFamily == serifFontFamily)
        return;
        
    m_serifFontFamily = serifFontFamily;
    setNeedsRecalcStyleInAllFrames(m_page);
}

void Settings::setSansSerifFontFamily(const AtomicString& sansSerifFontFamily)
{
    if (m_sansSerifFontFamily == sansSerifFontFamily)
        return;
        
    m_sansSerifFontFamily = sansSerifFontFamily; 
    setNeedsRecalcStyleInAllFrames(m_page);
}

void Settings::setCursiveFontFamily(const AtomicString& cursiveFontFamily)
{
    if (m_cursiveFontFamily == cursiveFontFamily)
        return;
        
    m_cursiveFontFamily = cursiveFontFamily;
    setNeedsRecalcStyleInAllFrames(m_page);
}

void Settings::setFantasyFontFamily(const AtomicString& fantasyFontFamily)
{
    if (m_fantasyFontFamily == fantasyFontFamily)
        return;
        
    m_fantasyFontFamily = fantasyFontFamily;
    setNeedsRecalcStyleInAllFrames(m_page);
}

void Settings::setMinimumFontSize(int minimumFontSize)
{
    if (m_minimumFontSize == minimumFontSize)
        return;

    m_minimumFontSize = minimumFontSize;
    setNeedsRecalcStyleInAllFrames(m_page);
}

void Settings::setMinimumLogicalFontSize(int minimumLogicalFontSize)
{
    if (m_minimumLogicalFontSize == minimumLogicalFontSize)
        return;

    m_minimumLogicalFontSize = minimumLogicalFontSize;
    setNeedsRecalcStyleInAllFrames(m_page);
}

void Settings::setDefaultFontSize(int defaultFontSize)
{
    if (m_defaultFontSize == defaultFontSize)
        return;

    m_defaultFontSize = defaultFontSize;
    setNeedsRecalcStyleInAllFrames(m_page);
}

void Settings::setDefaultFixedFontSize(int defaultFontSize)
{
    if (m_defaultFixedFontSize == defaultFontSize)
        return;

    m_defaultFixedFontSize = defaultFontSize;
    setNeedsRecalcStyleInAllFrames(m_page);
}

void Settings::setLoadsImagesAutomatically(bool loadsImagesAutomatically)
{
    m_loadsImagesAutomatically = loadsImagesAutomatically;
    setLoadsImagesAutomaticallyInAllFrames(m_page);
}

void Settings::setJavaScriptEnabled(bool isJavaScriptEnabled)
{
    m_isJavaScriptEnabled = isJavaScriptEnabled;
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

#if ENABLE(DOM_STORAGE)
void Settings::setSessionStorageQuota(unsigned sessionStorageQuota)
{
    m_sessionStorageQuota = sessionStorageQuota;
}
#endif

void Settings::setPrivateBrowsingEnabled(bool privateBrowsingEnabled)
{
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
    setNeedsRecalcStyleInAllFrames(m_page);
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

void Settings::setNeedsTigerMailQuirks(bool needsQuirks)
{
    m_needsTigerMailQuirks = needsQuirks;
}
    
void Settings::setDOMPasteAllowed(bool DOMPasteAllowed)
{
    m_isDOMPasteAllowed = DOMPasteAllowed;
}

void Settings::setUsesPageCache(bool usesPageCache)
{
    if (m_usesPageCache == usesPageCache)
        return;
        
    m_usesPageCache = usesPageCache;
    if (!m_usesPageCache) {
        HistoryItemVector& historyItems = m_page->backForwardList()->entries();
        for (unsigned i = 0; i < historyItems.size(); i++)
            pageCache()->remove(historyItems[i].get());
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
    setNeedsRecalcStyleInAllFrames(m_page);
}

void Settings::setFontRenderingMode(FontRenderingMode mode)
{
    if (fontRenderingMode() == mode)
        return;
    m_fontRenderingMode = mode;
    setNeedsRecalcStyleInAllFrames(m_page);
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

void Settings::setWebArchiveDebugModeEnabled(bool enabled)
{
    m_webArchiveDebugModeEnabled = enabled;
}

void Settings::setLocalFileContentSniffingEnabled(bool enabled)
{
    m_localFileContentSniffingEnabled = enabled;
}

void Settings::setLocalStorageDatabasePath(const String& path)
{
    m_localStorageDatabasePath = path;
}

void Settings::setFileSystemRootPath(const String& path)
{
    m_fileSystemRootPath = path;
}

void Settings::setApplicationChromeMode(bool mode)
{
    m_inApplicationChromeMode = mode;
}

void Settings::setOfflineWebApplicationCacheEnabled(bool enabled)
{
    m_offlineWebApplicationCacheEnabled = enabled;
}

void Settings::setShouldPaintCustomScrollbars(bool shouldPaintCustomScrollbars)
{
    m_shouldPaintCustomScrollbars = shouldPaintCustomScrollbars;
}

void Settings::setZoomMode(ZoomMode mode)
{
    if (mode == m_zoomMode)
        return;
    
    m_zoomMode = mode;
    setNeedsRecalcStyleInAllFrames(m_page);
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
    m_dnsPrefetchingEnabled = dnsPrefetchingEnabled;
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
    setNeedsRecalcStyleInAllFrames(m_page);
}

void Settings::setShowDebugBorders(bool enabled)
{
    if (m_showDebugBorders == enabled)
        return;
        
    m_showDebugBorders = enabled;
    setNeedsRecalcStyleInAllFrames(m_page);
}

void Settings::setShowRepaintCounter(bool enabled)
{
    if (m_showRepaintCounter == enabled)
        return;
        
    m_showRepaintCounter = enabled;
    setNeedsRecalcStyleInAllFrames(m_page);
}

void Settings::setExperimentalNotificationsEnabled(bool enabled)
{
    m_experimentalNotificationsEnabled = enabled;
}

void Settings::setPluginAllowedRunTime(unsigned runTime)
{
    m_pluginAllowedRunTime = runTime;
    m_page->pluginAllowedRunTimeChanged();
}

#if PLATFORM(WIN) || (OS(WINDOWS) && PLATFORM(WX))
void Settings::setShouldUseHighResolutionTimers(bool shouldUseHighResolutionTimers)
{
    gShouldUseHighResolutionTimers = shouldUseHighResolutionTimers;
}
#endif

void Settings::setWebGLEnabled(bool enabled)
{
    m_webGLEnabled = enabled;
}

void Settings::setAccelerated2dCanvasEnabled(bool enabled)
{
    m_acceleratedCanvas2dEnabled = enabled;
}

void Settings::setLoadDeferringEnabled(bool enabled)
{
    m_loadDeferringEnabled = enabled;
}

void Settings::setTiledBackingStoreEnabled(bool enabled)
{
    m_tiledBackingStoreEnabled = enabled;
#if ENABLE(TILED_BACKING_STORE)
    if (m_page->mainFrame())
        m_page->mainFrame()->setTiledBackingStoreEnabled(enabled);
#endif
}

} // namespace WebCore
