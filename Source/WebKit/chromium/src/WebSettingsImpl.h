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

#ifndef WebSettingsImpl_h
#define WebSettingsImpl_h

#include "WebSettings.h"

namespace WebCore {
class Settings;
}

namespace WebKit {

class WebSettingsImpl : public WebSettings {
public:
    explicit WebSettingsImpl(WebCore::Settings*);
    virtual ~WebSettingsImpl() { }

    virtual void setStandardFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON);
    virtual void setFixedFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON);
    virtual void setSerifFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON);
    virtual void setSansSerifFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON);
    virtual void setCursiveFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON);
    virtual void setFantasyFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON);
    virtual void setDefaultFontSize(int);
    virtual void setDefaultFixedFontSize(int);
    virtual void setMinimumFontSize(int);
    virtual void setMinimumLogicalFontSize(int);
    virtual void setDefaultDeviceScaleFactor(int);
    virtual void setApplyDefaultDeviceScaleFactorInCompositor(bool);
    virtual void setDefaultTextEncodingName(const WebString&);
    virtual void setJavaScriptEnabled(bool);
    virtual void setWebSecurityEnabled(bool);
    virtual void setJavaScriptCanOpenWindowsAutomatically(bool);
    virtual void setLoadsImagesAutomatically(bool);
    virtual void setImagesEnabled(bool);
    virtual void setPluginsEnabled(bool);
    virtual void setDOMPasteAllowed(bool);
    virtual void setDeveloperExtrasEnabled(bool);
    virtual void setNeedsSiteSpecificQuirks(bool);
    virtual void setShrinksStandaloneImagesToFit(bool);
    virtual void setUsesEncodingDetector(bool);
    virtual void setTextAreasAreResizable(bool);
    virtual void setJavaEnabled(bool);
    virtual void setAllowScriptsToCloseWindows(bool);
    virtual void setUserStyleSheetLocation(const WebURL&);
    virtual void setAuthorAndUserStylesEnabled(bool);
    virtual void setUsesPageCache(bool);
    virtual void setPageCacheSupportsPlugins(bool);
    virtual void setDownloadableBinaryFontsEnabled(bool);
    virtual void setJavaScriptCanAccessClipboard(bool);
    virtual void setXSSAuditorEnabled(bool);
    virtual void setDNSPrefetchingEnabled(bool);
    virtual void setFixedElementsLayoutRelativeToFrame(bool);
    virtual void setLocalStorageEnabled(bool);
    virtual void setEditableLinkBehaviorNeverLive();
    virtual void setFrameFlatteningEnabled(bool);
    virtual void setFontRenderingModeNormal();
    virtual void setAllowUniversalAccessFromFileURLs(bool);
    virtual void setAllowFileAccessFromFileURLs(bool);
    virtual void setTextDirectionSubmenuInclusionBehaviorNeverIncluded();
    virtual void setOfflineWebApplicationCacheEnabled(bool);
    virtual void setWebAudioEnabled(bool);
    virtual void setExperimentalWebGLEnabled(bool);
    virtual void setExperimentalCSSRegionsEnabled(bool);
    virtual void setExperimentalCSSCustomFilterEnabled(bool);
    virtual void setOpenGLMultisamplingEnabled(bool);
    virtual void setPrivilegedWebGLExtensionsEnabled(bool);
    virtual void setWebGLErrorsToConsoleEnabled(bool);
    virtual void setShowDebugBorders(bool);
    virtual void setShowFPSCounter(bool);
    virtual void setShowPlatformLayerTree(bool);
    virtual void setShowPaintRects(bool);
    virtual void setDefaultTileSize(WebSize);
    virtual void setMaxUntiledLayerSize(WebSize);
    virtual void setEditingBehavior(EditingBehavior);
    virtual void setAcceleratedCompositingEnabled(bool);
    virtual void setForceCompositingMode(bool);
    virtual void setMockScrollbarsEnabled(bool);
    virtual void setAcceleratedCompositingFor3DTransformsEnabled(bool);
    virtual void setAcceleratedCompositingForVideoEnabled(bool);
    virtual void setAcceleratedCompositingForPluginsEnabled(bool);
    virtual void setAcceleratedCompositingForCanvasEnabled(bool);
    virtual void setAcceleratedCompositingForAnimationEnabled(bool);
    virtual void setAccelerated2dCanvasEnabled(bool);
    virtual void setDeferred2dCanvasEnabled(bool);
    virtual void setAcceleratedCompositingForFixedPositionEnabled(bool);
    virtual void setMinimumAccelerated2dCanvasSize(int);
    virtual void setAcceleratedFiltersEnabled(bool);
    virtual void setMemoryInfoEnabled(bool);
    virtual void setHyperlinkAuditingEnabled(bool);
    virtual void setLayoutFallbackWidth(int);
    virtual void setAsynchronousSpellCheckingEnabled(bool);
    virtual void setUnifiedTextCheckerEnabled(bool);
    virtual void setCaretBrowsingEnabled(bool);
    virtual void setInteractiveFormValidationEnabled(bool);
    virtual void setValidationMessageTimerMagnification(int);
    virtual void setMinimumTimerInterval(double);
    virtual void setFullScreenEnabled(bool);
    virtual void setAllowDisplayOfInsecureContent(bool);
    virtual void setAllowRunningOfInsecureContent(bool);
    virtual void setPasswordEchoEnabled(bool);
    virtual void setPasswordEchoDurationInSeconds(double);
    virtual void setShouldPrintBackgrounds(bool);
    virtual void setEnableScrollAnimator(bool);
    virtual bool scrollAnimatorEnabled() const;
    virtual void setHixie76WebSocketProtocolEnabled(bool);
    virtual void setVisualWordMovementEnabled(bool);
    virtual void setShouldDisplaySubtitles(bool);
    virtual void setShouldDisplayCaptions(bool);
    virtual void setShouldDisplayTextDescriptions(bool);
    virtual void setAcceleratedPaintingEnabled(bool);
    virtual void setPerTilePaintingEnabled(bool);
    virtual void setPartialSwapEnabled(bool);
    virtual void setThreadedAnimationEnabled(bool);
    virtual void setViewportEnabled(bool);
    virtual bool viewportEnabled() const { return m_viewportEnabled; }

    bool showFPSCounter() const { return m_showFPSCounter; }
    bool showPlatformLayerTree() const { return m_showPlatformLayerTree; }
    bool showPaintRects() const { return m_showPaintRects; }
    bool applyDefaultDeviceScaleFactorInCompositor() const { return m_applyDefaultDeviceScaleFactorInCompositor; }
    WebSize defaultTileSize() const { return m_defaultTileSize; }
    WebSize maxUntiledLayerSize() const { return m_maxUntiledLayerSize; }

private:
    WebCore::Settings* m_settings;
    bool m_showFPSCounter;
    bool m_showPlatformLayerTree;
    bool m_showPaintRects;
    bool m_viewportEnabled;
    bool m_applyDefaultDeviceScaleFactorInCompositor;
    WebSize m_defaultTileSize;
    WebSize m_maxUntiledLayerSize;
};

} // namespace WebKit

#endif
