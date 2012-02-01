/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebSettingsImpl.h"

#include "FontRenderingMode.h"
#include "Settings.h"
#include "platform/WebString.h"
#include "platform/WebURL.h"
#include <wtf/UnusedParam.h>

#if defined(OS_WIN)
#include "RenderThemeChromiumWin.h"
#endif

using namespace WebCore;

namespace WebKit {

WebSettingsImpl::WebSettingsImpl(Settings* settings)
    : m_settings(settings)
    , m_compositeToTextureEnabled(false)
    , m_showFPSCounter(false)
    , m_showPlatformLayerTree(false)
{
    ASSERT(settings);
}

void WebSettingsImpl::setStandardFontFamily(const WebString& font, UScriptCode script)
{
    m_settings->setStandardFontFamily(font, script);
}

void WebSettingsImpl::setFixedFontFamily(const WebString& font, UScriptCode script)
{
    m_settings->setFixedFontFamily(font, script);
}

void WebSettingsImpl::setSerifFontFamily(const WebString& font, UScriptCode script)
{
    m_settings->setSerifFontFamily(font, script);
}

void WebSettingsImpl::setSansSerifFontFamily(const WebString& font, UScriptCode script)
{
    m_settings->setSansSerifFontFamily(font, script);
}

void WebSettingsImpl::setCursiveFontFamily(const WebString& font, UScriptCode script)
{
    m_settings->setCursiveFontFamily(font, script);
}

void WebSettingsImpl::setFantasyFontFamily(const WebString& font, UScriptCode script)
{
    m_settings->setFantasyFontFamily(font, script);
}

void WebSettingsImpl::setDefaultFontSize(int size)
{
    m_settings->setDefaultFontSize(size);
#if defined(OS_WIN)
    // RenderTheme is a singleton that needs to know the default font size to
    // draw some form controls.  We let it know each time the size changes.
    WebCore::RenderThemeChromiumWin::setDefaultFontSize(size);
#endif
}

void WebSettingsImpl::setDefaultFixedFontSize(int size)
{
    m_settings->setDefaultFixedFontSize(size);
}

void WebSettingsImpl::setMinimumFontSize(int size)
{
    m_settings->setMinimumFontSize(size);
}

void WebSettingsImpl::setMinimumLogicalFontSize(int size)
{
    m_settings->setMinimumLogicalFontSize(size);
}

void WebSettingsImpl::setDefaultTextEncodingName(const WebString& encoding)
{
    m_settings->setDefaultTextEncodingName((String)encoding);
}

void WebSettingsImpl::setJavaScriptEnabled(bool enabled)
{
    m_settings->setScriptEnabled(enabled);
}

void WebSettingsImpl::setWebSecurityEnabled(bool enabled)
{
    m_settings->setWebSecurityEnabled(enabled);
}

void WebSettingsImpl::setJavaScriptCanOpenWindowsAutomatically(bool canOpenWindows)
{
    m_settings->setJavaScriptCanOpenWindowsAutomatically(canOpenWindows);
}

void WebSettingsImpl::setLoadsImagesAutomatically(bool loadsImagesAutomatically)
{
    m_settings->setLoadsImagesAutomatically(loadsImagesAutomatically);
}

void WebSettingsImpl::setImagesEnabled(bool enabled)
{
    m_settings->setImagesEnabled(enabled);
}

void WebSettingsImpl::setPluginsEnabled(bool enabled)
{
    m_settings->setPluginsEnabled(enabled);
}

void WebSettingsImpl::setDOMPasteAllowed(bool enabled)
{
    m_settings->setDOMPasteAllowed(enabled);
}

void WebSettingsImpl::setDeveloperExtrasEnabled(bool enabled)
{
    m_settings->setDeveloperExtrasEnabled(enabled);
}

void WebSettingsImpl::setNeedsSiteSpecificQuirks(bool enabled)
{
    m_settings->setNeedsSiteSpecificQuirks(enabled);
}

void WebSettingsImpl::setShrinksStandaloneImagesToFit(bool shrinkImages)
{
    m_settings->setShrinksStandaloneImagesToFit(shrinkImages);
}

void WebSettingsImpl::setUsesEncodingDetector(bool usesDetector)
{
    m_settings->setUsesEncodingDetector(usesDetector);
}

void WebSettingsImpl::setTextAreasAreResizable(bool areResizable)
{
    m_settings->setTextAreasAreResizable(areResizable);
}

void WebSettingsImpl::setJavaEnabled(bool enabled)
{
    m_settings->setJavaEnabled(enabled);
}

void WebSettingsImpl::setAllowScriptsToCloseWindows(bool allow)
{
    m_settings->setAllowScriptsToCloseWindows(allow);
}

void WebSettingsImpl::setUserStyleSheetLocation(const WebURL& location)
{
    m_settings->setUserStyleSheetLocation(location);
}

void WebSettingsImpl::setAuthorAndUserStylesEnabled(bool enabled)
{
    m_settings->setAuthorAndUserStylesEnabled(enabled);
}

void WebSettingsImpl::setUsesPageCache(bool usesPageCache)
{
    m_settings->setUsesPageCache(usesPageCache);
}

void WebSettingsImpl::setPageCacheSupportsPlugins(bool pageCacheSupportsPlugins)
{
    m_settings->setPageCacheSupportsPlugins(pageCacheSupportsPlugins);
}

void WebSettingsImpl::setDownloadableBinaryFontsEnabled(bool enabled)
{
    m_settings->setDownloadableBinaryFontsEnabled(enabled);
}

void WebSettingsImpl::setJavaScriptCanAccessClipboard(bool enabled)
{
    m_settings->setJavaScriptCanAccessClipboard(enabled);
}

void WebSettingsImpl::setXSSAuditorEnabled(bool enabled)
{
    m_settings->setXSSAuditorEnabled(enabled);
}

void WebSettingsImpl::setDNSPrefetchingEnabled(bool enabled)
{
    m_settings->setDNSPrefetchingEnabled(enabled);
}

void WebSettingsImpl::setFixedElementsLayoutRelativeToFrame(bool fixedElementsLayoutRelativeToFrame)
{
    m_settings->setFixedElementsLayoutRelativeToFrame(fixedElementsLayoutRelativeToFrame);
}

void WebSettingsImpl::setLocalStorageEnabled(bool enabled)
{
    m_settings->setLocalStorageEnabled(enabled);
}

void WebSettingsImpl::setEditableLinkBehaviorNeverLive()
{
    // FIXME: If you ever need more behaviors than this, then we should probably
    //        define an enum in WebSettings.h and have a switch statement that
    //        translates.  Until then, this is probably fine, though.
    m_settings->setEditableLinkBehavior(WebCore::EditableLinkNeverLive);
}

void WebSettingsImpl::setFrameFlatteningEnabled(bool enabled)
{
    m_settings->setFrameFlatteningEnabled(enabled);
}

void WebSettingsImpl::setFontRenderingModeNormal()
{
    // FIXME: If you ever need more behaviors than this, then we should probably
    //        define an enum in WebSettings.h and have a switch statement that
    //        translates.  Until then, this is probably fine, though.
    m_settings->setFontRenderingMode(WebCore::NormalRenderingMode);
}

void WebSettingsImpl::setAllowUniversalAccessFromFileURLs(bool allow)
{
    m_settings->setAllowUniversalAccessFromFileURLs(allow);
}

void WebSettingsImpl::setAllowFileAccessFromFileURLs(bool allow)
{
    m_settings->setAllowFileAccessFromFileURLs(allow);
}

void WebSettingsImpl::setTextDirectionSubmenuInclusionBehaviorNeverIncluded()
{
    // FIXME: If you ever need more behaviors than this, then we should probably
    //        define an enum in WebSettings.h and have a switch statement that
    //        translates.  Until then, this is probably fine, though.
    m_settings->setTextDirectionSubmenuInclusionBehavior(WebCore::TextDirectionSubmenuNeverIncluded);
}

void WebSettingsImpl::setOfflineWebApplicationCacheEnabled(bool enabled)
{
    m_settings->setOfflineWebApplicationCacheEnabled(enabled);
}

void WebSettingsImpl::setWebAudioEnabled(bool enabled)
{
    m_settings->setWebAudioEnabled(enabled);
}

void WebSettingsImpl::setExperimentalWebGLEnabled(bool enabled)
{
    m_settings->setWebGLEnabled(enabled);
}

void WebSettingsImpl::setOpenGLMultisamplingEnabled(bool enabled)
{
    m_settings->setOpenGLMultisamplingEnabled(enabled);
}

void WebSettingsImpl::setPrivilegedWebGLExtensionsEnabled(bool enabled)
{
    m_settings->setPrivilegedWebGLExtensionsEnabled(enabled);
}

void WebSettingsImpl::setShowDebugBorders(bool show)
{
    m_settings->setShowDebugBorders(show);
}

void WebSettingsImpl::setShowFPSCounter(bool show)
{
    m_showFPSCounter = show;
}

void WebSettingsImpl::setShowPlatformLayerTree(bool show)
{
    m_showPlatformLayerTree = show;
}

void WebSettingsImpl::setEditingBehavior(EditingBehavior behavior)
{
    m_settings->setEditingBehaviorType(static_cast<WebCore::EditingBehaviorType>(behavior));
}

void WebSettingsImpl::setAcceleratedCompositingEnabled(bool enabled)
{
    m_settings->setAcceleratedCompositingEnabled(enabled);
}

void WebSettingsImpl::setForceCompositingMode(bool enabled)
{
    m_settings->setForceCompositingMode(enabled);
}

void WebSettingsImpl::setMockScrollbarsEnabled(bool enabled)
{
    m_settings->setMockScrollbarsEnabled(enabled);
}

void WebSettingsImpl::setCompositeToTextureEnabled(bool enabled)
{
    m_compositeToTextureEnabled = enabled;
}

void WebSettingsImpl::setAcceleratedCompositingFor3DTransformsEnabled(bool enabled)
{
    m_settings->setAcceleratedCompositingFor3DTransformsEnabled(enabled);
}

void WebSettingsImpl::setAcceleratedCompositingForVideoEnabled(bool enabled)
{
    m_settings->setAcceleratedCompositingForVideoEnabled(enabled);
}

void WebSettingsImpl::setAcceleratedCompositingForPluginsEnabled(bool enabled)
{
    m_settings->setAcceleratedCompositingForPluginsEnabled(enabled);
}

void WebSettingsImpl::setAcceleratedCompositingForCanvasEnabled(bool enabled)
{
    m_settings->setAcceleratedCompositingForCanvasEnabled(enabled);
}

void WebSettingsImpl::setAcceleratedCompositingForAnimationEnabled(bool enabled)
{
    m_settings->setAcceleratedCompositingForAnimationEnabled(enabled);
}

void WebSettingsImpl::setAcceleratedFiltersEnabled(bool enabled)
{
    m_settings->setAcceleratedFiltersEnabled(enabled);
}

void WebSettingsImpl::setAccelerated2dCanvasEnabled(bool enabled)
{
    m_settings->setAccelerated2dCanvasEnabled(enabled);
}

void WebSettingsImpl::setDeferred2dCanvasEnabled(bool enabled)
{
    m_settings->setDeferred2dCanvasEnabled(enabled);
}

void WebSettingsImpl::setAcceleratedCompositingForFixedPositionEnabled(bool enabled)
{
    m_settings->setAcceleratedCompositingForFixedPositionEnabled(enabled);
}

void WebSettingsImpl::setMinimumAccelerated2dCanvasSize(int numPixels)
{
    m_settings->setMinimumAccelerated2dCanvasSize(numPixels);
}

void WebSettingsImpl::setMemoryInfoEnabled(bool enabled)
{
    m_settings->setMemoryInfoEnabled(enabled);
}

void WebSettingsImpl::setHyperlinkAuditingEnabled(bool enabled)
{
    m_settings->setHyperlinkAuditingEnabled(enabled);
}

void WebSettingsImpl::setLayoutFallbackWidth(int width)
{
    m_settings->setLayoutFallbackWidth(width);
}

void WebSettingsImpl::setAsynchronousSpellCheckingEnabled(bool enabled)
{
    m_settings->setAsynchronousSpellCheckingEnabled(enabled);
}

void WebSettingsImpl::setUnifiedTextCheckerEnabled(bool enabled)
{
    m_settings->setUnifiedTextCheckerEnabled(enabled);
}

void WebSettingsImpl::setCaretBrowsingEnabled(bool enabled)
{
    m_settings->setCaretBrowsingEnabled(enabled);
}

void WebSettingsImpl::setInteractiveFormValidationEnabled(bool enabled)
{
    m_settings->setInteractiveFormValidationEnabled(enabled);
}

void WebSettingsImpl::setValidationMessageTimerMagnification(int newValue)
{
    m_settings->setValidationMessageTimerMagnification(newValue);
}

void WebSettingsImpl::setMinimumTimerInterval(double interval)
{
    m_settings->setMinDOMTimerInterval(interval);
}

void WebSettingsImpl::setFullScreenEnabled(bool enabled)
{
#if ENABLE(FULLSCREEN_API)
    m_settings->setFullScreenEnabled(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

void WebSettingsImpl::setAllowDisplayOfInsecureContent(bool enabled)
{
    m_settings->setAllowDisplayOfInsecureContent(enabled);
}

void WebSettingsImpl::setAllowRunningOfInsecureContent(bool enabled)
{
    m_settings->setAllowRunningOfInsecureContent(enabled);
}

void WebSettingsImpl::setShouldPrintBackgrounds(bool enabled)
{
    m_settings->setShouldPrintBackgrounds(enabled);
}

void WebSettingsImpl::setEnableScrollAnimator(bool enabled)
{
#if ENABLE(SMOOTH_SCROLLING)
    m_settings->setEnableScrollAnimator(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

void WebSettingsImpl::setHixie76WebSocketProtocolEnabled(bool enabled)
{
#if ENABLE(WEB_SOCKETS)
    m_settings->setUseHixie76WebSocketProtocol(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

void WebSettingsImpl::setVisualWordMovementEnabled(bool enabled)
{
    m_settings->setVisualWordMovementEnabled(enabled);
}

void WebSettingsImpl::setShouldDisplaySubtitles(bool enabled)
{
#if ENABLE(VIDEO_TRACK)
    m_settings->setShouldDisplaySubtitles(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

void WebSettingsImpl::setShouldDisplayCaptions(bool enabled)
{
#if ENABLE(VIDEO_TRACK)
    m_settings->setShouldDisplayCaptions(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

void WebSettingsImpl::setShouldDisplayTextDescriptions(bool enabled)
{
#if ENABLE(VIDEO_TRACK)
    m_settings->setShouldDisplayTextDescriptions(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

void WebSettingsImpl::setAcceleratedPaintingEnabled(bool enabled)
{
    m_settings->setAcceleratedDrawingEnabled(enabled);
}

void WebSettingsImpl::setPerTilePaintingEnabled(bool enabled)
{
    m_settings->setPerTileDrawingEnabled(enabled);
}

void WebSettingsImpl::setPartialSwapEnabled(bool enabled)
{
    m_settings->setPartialSwapEnabled(enabled);
}

} // namespace WebKit
