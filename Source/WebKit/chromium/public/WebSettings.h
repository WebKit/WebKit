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

#ifndef WebSettings_h
#define WebSettings_h

#include "platform/WebCommon.h"
#include "platform/WebSize.h"
#include <unicode/uscript.h>

#define HAS_WEBAUDIO_FEATURE_ENABLE 1

namespace WebKit {

class WebString;
class WebURL;

// WebSettings is owned by the WebView and allows code to modify the settings for
// the WebView's page without any knowledge of WebCore itself.  For the most part,
// these functions have a 1:1 mapping with the methods in WebCore/page/Settings.h.
class WebSettings {
public:
    enum EditingBehavior {
        EditingBehaviorMac,
        EditingBehaviorWin,
        EditingBehaviorUnix
    };

    virtual void setStandardFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON) = 0;
    virtual void setFixedFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON) = 0;
    virtual void setSerifFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON) = 0;
    virtual void setSansSerifFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON) = 0;
    virtual void setCursiveFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON) = 0;
    virtual void setFantasyFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON) = 0;
    virtual void setDefaultFontSize(int) = 0;
    virtual void setDefaultFixedFontSize(int) = 0;
    virtual void setMinimumFontSize(int) = 0;
    virtual void setMinimumLogicalFontSize(int) = 0;
    virtual void setDefaultDeviceScaleFactor(int) = 0;
    virtual void setApplyDefaultDeviceScaleFactorInCompositor(bool) = 0;
    virtual void setDefaultTextEncodingName(const WebString&) = 0;
    virtual void setJavaScriptEnabled(bool) = 0;
    virtual void setWebSecurityEnabled(bool) = 0;
    virtual void setJavaScriptCanOpenWindowsAutomatically(bool) = 0;
    virtual void setLoadsImagesAutomatically(bool) = 0;
    virtual void setImagesEnabled(bool) = 0;
    virtual void setPluginsEnabled(bool) = 0;
    virtual void setDOMPasteAllowed(bool) = 0;
    virtual void setDeveloperExtrasEnabled(bool) = 0;
    virtual void setNeedsSiteSpecificQuirks(bool) = 0;
    virtual void setShrinksStandaloneImagesToFit(bool) = 0;
    virtual void setUsesEncodingDetector(bool) = 0;
    virtual void setTextAreasAreResizable(bool) = 0;
    virtual void setJavaEnabled(bool) = 0;
    virtual void setAllowScriptsToCloseWindows(bool) = 0;
    virtual void setUserStyleSheetLocation(const WebURL&) = 0;
    virtual void setAuthorAndUserStylesEnabled(bool) = 0;
    virtual void setUsesPageCache(bool) = 0;
    virtual void setPageCacheSupportsPlugins(bool) = 0;
    virtual void setDownloadableBinaryFontsEnabled(bool) = 0;
    virtual void setJavaScriptCanAccessClipboard(bool) = 0;
    virtual void setXSSAuditorEnabled(bool) = 0;
    virtual void setDNSPrefetchingEnabled(bool) = 0;
    virtual void setFixedElementsLayoutRelativeToFrame(bool) = 0;
    virtual void setLocalStorageEnabled(bool) = 0;
    virtual void setEditableLinkBehaviorNeverLive() = 0;
    virtual void setFrameFlatteningEnabled(bool) = 0;
    virtual void setFontRenderingModeNormal() = 0;
    virtual void setAllowUniversalAccessFromFileURLs(bool) = 0;
    virtual void setAllowFileAccessFromFileURLs(bool) = 0;
    virtual void setTextDirectionSubmenuInclusionBehaviorNeverIncluded() = 0;
    virtual void setOfflineWebApplicationCacheEnabled(bool) = 0;
    virtual void setWebAudioEnabled(bool) = 0;
    virtual void setExperimentalWebGLEnabled(bool) = 0;
    virtual void setExperimentalCSSRegionsEnabled(bool) = 0;
    virtual void setExperimentalCSSGridLayoutEnabled(bool) = 0;
    virtual void setExperimentalCSSCustomFilterEnabled(bool) = 0;
    virtual void setOpenGLMultisamplingEnabled(bool) = 0;
    virtual void setPrivilegedWebGLExtensionsEnabled(bool) = 0;
    virtual void setWebGLErrorsToConsoleEnabled(bool) = 0;
    virtual void setShowDebugBorders(bool) = 0;
    virtual void setShowFPSCounter(bool) = 0;
    virtual void setShowPlatformLayerTree(bool) = 0;
    virtual void setShowPaintRects(bool) = 0;
    virtual void setEditingBehavior(EditingBehavior) = 0;
    virtual void setAcceleratedCompositingEnabled(bool) = 0;
    virtual void setForceCompositingMode(bool) = 0;
    virtual void setMockScrollbarsEnabled(bool) = 0;
    virtual void setAcceleratedCompositingFor3DTransformsEnabled(bool) = 0;
    virtual void setAcceleratedCompositingForVideoEnabled(bool) = 0;
    virtual void setAcceleratedCompositingForCanvasEnabled(bool) = 0;
    virtual void setAcceleratedCompositingForPluginsEnabled(bool) = 0;
    virtual void setAcceleratedCompositingForAnimationEnabled(bool) = 0;
    virtual void setAccelerated2dCanvasEnabled(bool) = 0;
    virtual void setDeferred2dCanvasEnabled(bool) = 0;
    virtual void setAcceleratedCompositingForFixedPositionEnabled(bool)  = 0;
    virtual void setMinimumAccelerated2dCanvasSize(int) = 0;
    virtual void setAcceleratedFiltersEnabled(bool) = 0;
    virtual void setMemoryInfoEnabled(bool) = 0;
    virtual void setHyperlinkAuditingEnabled(bool) = 0;
    virtual void setLayoutFallbackWidth(int) = 0;
    virtual void setAsynchronousSpellCheckingEnabled(bool) = 0;
    virtual void setUnifiedTextCheckerEnabled(bool) = 0;
    virtual void setCaretBrowsingEnabled(bool) = 0;
    virtual void setInteractiveFormValidationEnabled(bool) = 0;
    virtual void setValidationMessageTimerMagnification(int) = 0;
    virtual void setMinimumTimerInterval(double) = 0;
    virtual void setFullScreenEnabled(bool) = 0;
    virtual void setAllowDisplayOfInsecureContent(bool) = 0;
    virtual void setAllowRunningOfInsecureContent(bool) = 0;
    virtual void setPasswordEchoEnabled(bool) = 0;
    virtual void setPasswordEchoDurationInSeconds(double) = 0;
    virtual void setShouldPrintBackgrounds(bool) = 0;
    virtual void setEnableScrollAnimator(bool) = 0;
    virtual bool scrollAnimatorEnabled() const = 0;
    virtual void setHixie76WebSocketProtocolEnabled(bool) = 0;
    virtual void setVisualWordMovementEnabled(bool) = 0;
    virtual void setAcceleratedPaintingEnabled(bool) = 0;
    virtual void setPerTilePaintingEnabled(bool) = 0;
    virtual void setPartialSwapEnabled(bool) = 0;
    virtual void setThreadedAnimationEnabled(bool) = 0;
    virtual void setViewportEnabled(bool) = 0;
    virtual bool viewportEnabled() const = 0;
    virtual void setDefaultTileSize(WebSize) = 0;
    virtual void setMaxUntiledLayerSize(WebSize) = 0;


protected:
    ~WebSettings() { }
};

} // namespace WebKit

#endif
