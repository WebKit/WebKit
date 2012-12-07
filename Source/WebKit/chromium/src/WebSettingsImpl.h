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

    virtual bool deviceSupportsTouch();
    virtual bool scrollAnimatorEnabled() const;
    virtual bool viewportEnabled() const { return m_viewportEnabled; }
    virtual void setAccelerated2dCanvasEnabled(bool);
    virtual void setAcceleratedAnimationEnabled(bool);
    virtual void setAcceleratedCompositingEnabled(bool);
    virtual void setAcceleratedCompositingFor3DTransformsEnabled(bool);
    virtual void setAcceleratedCompositingForAnimationEnabled(bool);
    virtual void setAcceleratedCompositingForCanvasEnabled(bool);
    virtual void setAcceleratedCompositingForFixedPositionEnabled(bool);
    virtual void setAcceleratedCompositingForOverflowScrollEnabled(bool);
    virtual void setAcceleratedCompositingForPluginsEnabled(bool);
    virtual void setAcceleratedCompositingForScrollableFramesEnabled(bool);
    virtual void setAcceleratedCompositingForVideoEnabled(bool);
    virtual void setAcceleratedFiltersEnabled(bool);
    virtual void setAcceleratedPaintingEnabled(bool);
    virtual void setAllowDisplayOfInsecureContent(bool);
    virtual void setAllowFileAccessFromFileURLs(bool);
    virtual void setAllowCustomScrollbarInMainFrame(bool);
    virtual void setAllowRunningOfInsecureContent(bool);
    virtual void setAllowScriptsToCloseWindows(bool);
    virtual void setAllowUniversalAccessFromFileURLs(bool);
    virtual void setApplyDeviceScaleFactorInCompositor(bool);
    virtual void setApplyPageScaleFactorInCompositor(bool);
    virtual void setAsynchronousSpellCheckingEnabled(bool);
    virtual void setAuthorAndUserStylesEnabled(bool);
    virtual void setAutoZoomFocusedNodeToLegibleScale(bool);
    virtual void setCaretBrowsingEnabled(bool);
    virtual void setCookieEnabled(bool);
    virtual void setCursiveFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON);
    virtual void setDNSPrefetchingEnabled(bool);
    virtual void setDOMPasteAllowed(bool);
    virtual void setDefaultFixedFontSize(int);
    virtual void setDefaultFontSize(int);
    virtual void setDefaultTextEncodingName(const WebString&);
    virtual void setDefaultTileSize(WebSize);
    virtual void setDeferred2dCanvasEnabled(bool);
    virtual void setDeferredImageDecodingEnabled(bool);
    virtual void setDeveloperExtrasEnabled(bool);
    virtual void setDeviceSupportsMouse(bool);
    virtual void setDeviceSupportsTouch(bool);
    virtual void setDoubleTapToZoomEnabled(bool);
    virtual void setDownloadableBinaryFontsEnabled(bool);
    virtual void setEditableLinkBehaviorNeverLive();
    virtual void setEditingBehavior(EditingBehavior);
    virtual void setEnableScrollAnimator(bool);
    virtual void setEnableTouchAdjustment(bool);
    virtual void setExperimentalCSSCustomFilterEnabled(bool);
    virtual void setExperimentalCSSGridLayoutEnabled(bool);
    virtual void setCSSStickyPositionEnabled(bool);
    virtual void setExperimentalCSSVariablesEnabled(bool);
    virtual void setExperimentalWebGLEnabled(bool);
    virtual void setFantasyFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON);
    virtual void setFixedElementsLayoutRelativeToFrame(bool);
    virtual void setFixedFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON);
    virtual void setFixedPositionCreatesStackingContext(bool);
    virtual void setFontRenderingModeNormal();
    virtual void setForceCompositingMode(bool);
    virtual void setFrameFlatteningEnabled(bool);
    virtual void setFullScreenEnabled(bool);
    virtual void setGestureTapHighlightEnabled(bool);
    virtual void setHyperlinkAuditingEnabled(bool);
    virtual void setImagesEnabled(bool);
    virtual void setInteractiveFormValidationEnabled(bool);
    virtual void setJavaEnabled(bool);
    virtual void setJavaScriptCanAccessClipboard(bool);
    virtual void setJavaScriptCanOpenWindowsAutomatically(bool);
    virtual void setJavaScriptEnabled(bool);
    virtual void setLayoutFallbackWidth(int);
    virtual void setLoadsImagesAutomatically(bool);
    virtual void setLocalStorageEnabled(bool);
    virtual void setLowLatencyRenderingEnabled(bool);
    virtual void setMaxUntiledLayerSize(WebSize);
    virtual void setMediaPlaybackRequiresUserGesture(bool);
    virtual void setMemoryInfoEnabled(bool);
    virtual void setMinimumAccelerated2dCanvasSize(int);
    virtual void setMinimumFontSize(int);
    virtual void setMinimumLogicalFontSize(int);
    virtual void setMinimumTimerInterval(double);
    virtual void setMockScrollbarsEnabled(bool);
    virtual void setNeedsSiteSpecificQuirks(bool);
    virtual void setOfflineWebApplicationCacheEnabled(bool);
    virtual void setOpenGLMultisamplingEnabled(bool);
    virtual void setPageCacheSupportsPlugins(bool);
    virtual void setPasswordEchoDurationInSeconds(double);
    virtual void setPasswordEchoEnabled(bool);
    virtual void setPerTilePaintingEnabled(bool);
    virtual void setPictographFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON);
    virtual void setPluginsEnabled(bool);
    virtual void setPrivilegedWebGLExtensionsEnabled(bool);
    virtual void setRenderVSyncEnabled(bool);
    virtual void setSansSerifFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON);
    virtual void setSerifFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON);
    virtual void setShouldDisplayCaptions(bool);
    virtual void setShouldDisplaySubtitles(bool);
    virtual void setShouldDisplayTextDescriptions(bool);
    virtual void setShouldPrintBackgrounds(bool);
    virtual void setShouldRespectImageOrientation(bool);
    virtual void setShowDebugBorders(bool);
    virtual void setShowFPSCounter(bool);
    virtual void setShowPaintRects(bool);
    virtual void setShowPlatformLayerTree(bool);
    virtual void setShrinksStandaloneImagesToFit(bool);
    virtual void setStandardFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON);
    virtual void setSupportsMultipleWindows(bool);
    virtual void setSyncXHRInDocumentsEnabled(bool);
    virtual void setTextAreasAreResizable(bool);
    virtual void setTextAutosizingEnabled(bool);
    virtual void setTextAutosizingFontScaleFactor(float);
    virtual void setTextDirectionSubmenuInclusionBehaviorNeverIncluded();
    virtual void setTouchDragDropEnabled(bool);
    virtual void setUnifiedTextCheckerEnabled(bool);
    virtual void setUserStyleSheetLocation(const WebURL&);
    virtual void setUsesEncodingDetector(bool);
    virtual void setUsesPageCache(bool);
    virtual void setValidationMessageTimerMagnification(int);
    virtual void setViewportEnabled(bool);
    virtual void setVisualWordMovementEnabled(bool);
    virtual void setWebAudioEnabled(bool);
    virtual void setWebGLErrorsToConsoleEnabled(bool);
    virtual void setWebSecurityEnabled(bool);
    virtual void setXSSAuditorEnabled(bool);

    bool showFPSCounter() const { return m_showFPSCounter; }
    bool showPlatformLayerTree() const { return m_showPlatformLayerTree; }
    bool showPaintRects() const { return m_showPaintRects; }
    bool renderVSyncEnabled() const { return m_renderVSyncEnabled; }
    bool lowLatencyRenderingEnabled() const { return m_lowLatencyRenderingEnabled; }
    bool applyDeviceScaleFactorInCompositor() const { return m_applyDeviceScaleFactorInCompositor; }
    bool applyPageScaleFactorInCompositor() const;
    bool autoZoomFocusedNodeToLegibleScale() const { return m_autoZoomFocusedNodeToLegibleScale; }
    bool gestureTapHighlightEnabled() const { return m_gestureTapHighlightEnabled; }
    bool doubleTapToZoomEnabled() const { return m_doubleTapToZoomEnabled; }
    bool perTilePaintingEnabled() const { return m_perTilePaintingEnabled; }
    bool acceleratedAnimationEnabled() const { return m_acceleratedAnimationEnabled; }
    WebSize defaultTileSize() const { return m_defaultTileSize; }
    WebSize maxUntiledLayerSize() const { return m_maxUntiledLayerSize; }

private:
    WebCore::Settings* m_settings;
    bool m_showFPSCounter;
    bool m_showPlatformLayerTree;
    bool m_showPaintRects;
    bool m_renderVSyncEnabled;
    bool m_lowLatencyRenderingEnabled;
    bool m_viewportEnabled;
    bool m_applyDeviceScaleFactorInCompositor;
    bool m_gestureTapHighlightEnabled;
    bool m_autoZoomFocusedNodeToLegibleScale;
    bool m_deferredImageDecodingEnabled;
    bool m_doubleTapToZoomEnabled;
    bool m_perTilePaintingEnabled;
    bool m_acceleratedAnimationEnabled;
    WebSize m_defaultTileSize;
    WebSize m_maxUntiledLayerSize;
};

} // namespace WebKit

#endif
