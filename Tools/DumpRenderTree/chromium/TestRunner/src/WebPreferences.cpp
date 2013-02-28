/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "WebPreferences.h"

#include "WebRuntimeFeatures.h"
#include "WebView.h"

using namespace WebKit;

namespace WebTestRunner {

void WebPreferences::reset()
{
#ifdef __APPLE__
    cursiveFontFamily = WebString::fromUTF8("Apple Chancery");
    fantasyFontFamily = WebString::fromUTF8("Papyrus");
    WebString serif = WebString::fromUTF8("Times");
#else
    // These two fonts are picked from the intersection of
    // Win XP font list and Vista font list :
    //   http://www.microsoft.com/typography/fonts/winxp.htm
    //   http://blogs.msdn.com/michkap/archive/2006/04/04/567881.aspx
    // Some of them are installed only with CJK and complex script
    // support enabled on Windows XP and are out of consideration here.
    // (although we enabled both on our buildbots.)
    // They (especially Impact for fantasy) are not typical cursive
    // and fantasy fonts, but it should not matter for layout tests
    // as long as they're available.
    cursiveFontFamily = WebString::fromUTF8("Comic Sans MS");
    fantasyFontFamily = WebString::fromUTF8("Impact");
    // NOTE: case matters here, this must be 'times new roman', else
    // some layout tests fail.
    WebString serif = WebString::fromUTF8("times new roman");
#endif
    serifFontFamily = serif;
    standardFontFamily = serif;
    fixedFontFamily = WebString::fromUTF8("Courier");
    sansSerifFontFamily = WebString::fromUTF8("Helvetica");

    defaultFontSize = 16;
    defaultFixedFontSize = 13;
    minimumFontSize = 0;
    minimumLogicalFontSize = 9;
    // Do not disable acceleration for 2d canvas based on size.
    // This makes having test expectations consistent.
    minimumAccelerated2dCanvasSize = 0;

    DOMPasteAllowed = true;
    XSSAuditorEnabled = false;
    allowDisplayOfInsecureContent = true;
    allowFileAccessFromFileURLs = true;
    allowRunningOfInsecureContent = true;
    authorAndUserStylesEnabled = true;
    defaultTextEncodingName = WebString::fromUTF8("ISO-8859-1");
    developerExtrasEnabled = true;
    experimentalWebGLEnabled = false;
    experimentalCSSRegionsEnabled = true;
    experimentalCSSGridLayoutEnabled = false;
    javaEnabled = false;
    javaScriptCanAccessClipboard = true;
    javaScriptCanOpenWindowsAutomatically = true;
    supportsMultipleWindows = true;
    javaScriptEnabled = true;
    loadsImagesAutomatically = true;
    localStorageEnabled = true;
    offlineWebApplicationCacheEnabled = true;
    pluginsEnabled = true;
    shrinksStandaloneImagesToFit = false;
    textAreasAreResizable = true;
    userStyleSheetLocation = WebURL();
    usesPageCache = false;
    pageCacheSupportsPlugins = false;
    webSecurityEnabled = true;
    caretBrowsingEnabled = false;

    // Allow those layout tests running as local files, i.e. under
    // LayoutTests/http/tests/local, to access http server.
    allowUniversalAccessFromFileURLs = true;

#ifdef __APPLE__
    editingBehavior = WebSettings::EditingBehaviorMac;
#else
    editingBehavior = WebSettings::EditingBehaviorWin;
#endif

    tabsToLinks = false;
    hyperlinkAuditingEnabled = false;
    acceleratedCompositingForVideoEnabled = false;
    acceleratedCompositingForFixedPositionEnabled = false;
    acceleratedCompositingForOverflowScrollEnabled = false;
    acceleratedCompositingEnabled = false;
    accelerated2dCanvasEnabled = false;
    deferred2dCanvasEnabled = false;
    acceleratedPaintingEnabled = false;
    forceCompositingMode = false;
    threadedHTMLParser = true;
    perTilePaintingEnabled = false;
    acceleratedAnimationEnabled = false;
    deferredImageDecodingEnabled = false;
    mediaPlaybackRequiresUserGesture = false;
    mockScrollbarsEnabled = false;
    cssCustomFilterEnabled = false;
    shouldRespectImageOrientation = false;
    asynchronousSpellCheckingEnabled = false;
    touchDragDropEnabled = false;
}

void WebPreferences::applyTo(WebView* webView)
{
    WebSettings* settings = webView->settings();
    settings->setStandardFontFamily(standardFontFamily);
    settings->setFixedFontFamily(fixedFontFamily);
    settings->setSerifFontFamily(serifFontFamily);
    settings->setSansSerifFontFamily(sansSerifFontFamily);
    settings->setCursiveFontFamily(cursiveFontFamily);
    settings->setFantasyFontFamily(fantasyFontFamily);

    settings->setDefaultFontSize(defaultFontSize);
    settings->setDefaultFixedFontSize(defaultFixedFontSize);
    settings->setMinimumFontSize(minimumFontSize);
    settings->setMinimumLogicalFontSize(minimumLogicalFontSize);
    settings->setMinimumAccelerated2dCanvasSize(minimumAccelerated2dCanvasSize);

    settings->setDOMPasteAllowed(DOMPasteAllowed);
    settings->setXSSAuditorEnabled(XSSAuditorEnabled);
    settings->setAllowDisplayOfInsecureContent(allowDisplayOfInsecureContent);
    settings->setAllowFileAccessFromFileURLs(allowFileAccessFromFileURLs);
    settings->setAllowRunningOfInsecureContent(allowRunningOfInsecureContent);
    settings->setAuthorAndUserStylesEnabled(authorAndUserStylesEnabled);
    settings->setDefaultTextEncodingName(defaultTextEncodingName);
    settings->setDeveloperExtrasEnabled(developerExtrasEnabled);
    settings->setExperimentalWebGLEnabled(experimentalWebGLEnabled);
    WebRuntimeFeatures::enableCSSRegions(experimentalCSSRegionsEnabled);
    settings->setExperimentalCSSGridLayoutEnabled(experimentalCSSGridLayoutEnabled);
    settings->setExperimentalCSSCustomFilterEnabled(cssCustomFilterEnabled);
    settings->setJavaEnabled(javaEnabled);
    settings->setJavaScriptCanAccessClipboard(javaScriptCanAccessClipboard);
    settings->setJavaScriptCanOpenWindowsAutomatically(javaScriptCanOpenWindowsAutomatically);
    settings->setSupportsMultipleWindows(supportsMultipleWindows);
    settings->setJavaScriptEnabled(javaScriptEnabled);
    settings->setLoadsImagesAutomatically(loadsImagesAutomatically);
    settings->setLocalStorageEnabled(localStorageEnabled);
    settings->setOfflineWebApplicationCacheEnabled(offlineWebApplicationCacheEnabled);
    settings->setPluginsEnabled(pluginsEnabled);
    settings->setShrinksStandaloneImagesToFit(shrinksStandaloneImagesToFit);
    settings->setTextAreasAreResizable(textAreasAreResizable);
    settings->setUserStyleSheetLocation(userStyleSheetLocation);
    settings->setUsesPageCache(usesPageCache);
    settings->setPageCacheSupportsPlugins(pageCacheSupportsPlugins);
    settings->setWebSecurityEnabled(webSecurityEnabled);
    settings->setAllowUniversalAccessFromFileURLs(allowUniversalAccessFromFileURLs);
    settings->setEditingBehavior(editingBehavior);
    settings->setHyperlinkAuditingEnabled(hyperlinkAuditingEnabled);
    // LayoutTests were written with Safari Mac in mind which does not allow
    // tabbing to links by default.
    webView->setTabsToLinks(tabsToLinks);
    settings->setCaretBrowsingEnabled(caretBrowsingEnabled);
    settings->setAcceleratedCompositingEnabled(acceleratedCompositingEnabled);
    settings->setAcceleratedCompositingForVideoEnabled(acceleratedCompositingForVideoEnabled);
    settings->setAcceleratedCompositingForFixedPositionEnabled(acceleratedCompositingForFixedPositionEnabled);
    settings->setAcceleratedCompositingForOverflowScrollEnabled(acceleratedCompositingForOverflowScrollEnabled);
    settings->setFixedPositionCreatesStackingContext(acceleratedCompositingForFixedPositionEnabled);
    settings->setForceCompositingMode(forceCompositingMode);
    settings->setThreadedHTMLParser(threadedHTMLParser);
    settings->setAccelerated2dCanvasEnabled(accelerated2dCanvasEnabled);
    settings->setDeferred2dCanvasEnabled(deferred2dCanvasEnabled);
    settings->setAcceleratedPaintingEnabled(acceleratedPaintingEnabled);
    settings->setPerTilePaintingEnabled(perTilePaintingEnabled);
    settings->setAcceleratedAnimationEnabled(acceleratedAnimationEnabled);
    settings->setDeferredImageDecodingEnabled(deferredImageDecodingEnabled);
    settings->setMediaPlaybackRequiresUserGesture(mediaPlaybackRequiresUserGesture);
    settings->setMockScrollbarsEnabled(mockScrollbarsEnabled);
    settings->setShouldRespectImageOrientation(shouldRespectImageOrientation);
    settings->setAsynchronousSpellCheckingEnabled(asynchronousSpellCheckingEnabled);
    settings->setTouchDragDropEnabled(touchDragDropEnabled);

    // Fixed values.
    settings->setTextDirectionSubmenuInclusionBehaviorNeverIncluded();
    settings->setDownloadableBinaryFontsEnabled(true);
    settings->setAllowScriptsToCloseWindows(false);
    settings->setNeedsSiteSpecificQuirks(true);
    settings->setEditableLinkBehaviorNeverLive();
    settings->setEnableScrollAnimator(false);
    settings->setFontRenderingModeNormal();
    settings->setTextDirectionSubmenuInclusionBehaviorNeverIncluded();
    settings->setUsesEncodingDetector(false);
    settings->setImagesEnabled(true);
    settings->setInteractiveFormValidationEnabled(true);
    // Enable fullscreen so the fullscreen layout tests can run.
    settings->setFullScreenEnabled(true);
    settings->setValidationMessageTimerMagnification(-1);
    settings->setVisualWordMovementEnabled(false);
    settings->setPasswordEchoEnabled(false);
    settings->setApplyDeviceScaleFactorInCompositor(true);
}

}
