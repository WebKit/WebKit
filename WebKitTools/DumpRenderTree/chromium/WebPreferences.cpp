/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "WebView.h"

using namespace WebKit;

void WebPreferences::reset()
{
#if OS(MAC_OS_X)
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

    DOMPasteAllowed = true;
    XSSAuditorEnabled = false;
    allowFileAccessFromFileURLs = true;
    authorAndUserStylesEnabled = true;
    defaultTextEncodingName = WebString::fromUTF8("ISO-8859-1");
    developerExtrasEnabled = false;
    experimentalWebGLEnabled = false;
    javaEnabled = false;
    javaScriptCanAccessClipboard = true;
    javaScriptCanOpenWindowsAutomatically = true;
    javaScriptEnabled = true;
    loadsImagesAutomatically = true;
    localStorageEnabled = true;
    offlineWebApplicationCacheEnabled = true;
    pluginsEnabled = true;
    shrinksStandaloneImagesToFit = false;
    textAreasAreResizable = false;
    userStyleSheetLocation = WebURL();
    usesPageCache = false;
    webSecurityEnabled = true;
    caretBrowsingEnabled = false;

    // Allow those layout tests running as local files, i.e. under
    // LayoutTests/http/tests/local, to access http server.
    allowUniversalAccessFromFileURLs = true;

#if OS(DARWIN)
    editingBehavior = WebSettings::EditingBehaviorMac;
#else
    editingBehavior = WebSettings::EditingBehaviorWin;
#endif

    tabsToLinks = false;
    hyperlinkAuditingEnabled = false;
    acceleratedCompositingEnabled = false;
    accelerated2dCanvasEnabled = false;
}

void WebPreferences::applyTo(WebView* webView)
{
    WebSettings* settings = webView->settings();
    settings->setCursiveFontFamily(cursiveFontFamily);
    settings->setFantasyFontFamily(fantasyFontFamily);
    settings->setSerifFontFamily(serifFontFamily);
    settings->setStandardFontFamily(standardFontFamily);
    settings->setFixedFontFamily(fixedFontFamily);
    settings->setSansSerifFontFamily(sansSerifFontFamily);

    settings->setDefaultFontSize(defaultFontSize);
    settings->setDefaultFixedFontSize(defaultFixedFontSize);
    settings->setMinimumFontSize(minimumFontSize);
    settings->setMinimumLogicalFontSize(minimumLogicalFontSize);

    settings->setDOMPasteAllowed(DOMPasteAllowed);
    settings->setXSSAuditorEnabled(XSSAuditorEnabled);
    settings->setAllowFileAccessFromFileURLs(allowFileAccessFromFileURLs);
    settings->setAuthorAndUserStylesEnabled(authorAndUserStylesEnabled);
    settings->setDefaultTextEncodingName(defaultTextEncodingName);
    settings->setDeveloperExtrasEnabled(developerExtrasEnabled);
    settings->setExperimentalWebGLEnabled(experimentalWebGLEnabled);
    settings->setJavaEnabled(javaEnabled);
    settings->setJavaScriptCanAccessClipboard(javaScriptCanAccessClipboard);
    settings->setJavaScriptCanOpenWindowsAutomatically(javaScriptCanOpenWindowsAutomatically);
    settings->setJavaScriptEnabled(javaScriptEnabled);
    settings->setLoadsImagesAutomatically(loadsImagesAutomatically);
    settings->setLocalStorageEnabled(localStorageEnabled);
    settings->setOfflineWebApplicationCacheEnabled(offlineWebApplicationCacheEnabled);
    settings->setPluginsEnabled(pluginsEnabled);
    settings->setShrinksStandaloneImagesToFit(shrinksStandaloneImagesToFit);
    settings->setTextAreasAreResizable(textAreasAreResizable);
    settings->setUserStyleSheetLocation(userStyleSheetLocation);
    settings->setUsesPageCache(usesPageCache);
    settings->setWebSecurityEnabled(webSecurityEnabled);
    settings->setAllowUniversalAccessFromFileURLs(allowUniversalAccessFromFileURLs);
    settings->setEditingBehavior(editingBehavior);
    settings->setHyperlinkAuditingEnabled(hyperlinkAuditingEnabled);
    // LayoutTests were written with Safari Mac in mind which does not allow
    // tabbing to links by default.
    webView->setTabsToLinks(tabsToLinks);
    settings->setCaretBrowsingEnabled(caretBrowsingEnabled);

    // Fixed values.
    settings->setShouldPaintCustomScrollbars(true);
    settings->setTextDirectionSubmenuInclusionBehaviorNeverIncluded();
    settings->setDownloadableBinaryFontsEnabled(true);
    settings->setAllowScriptsToCloseWindows(false);
    settings->setNeedsSiteSpecificQuirks(true);
    settings->setEditableLinkBehaviorNeverLive();
    settings->setFontRenderingModeNormal();
    settings->setTextDirectionSubmenuInclusionBehaviorNeverIncluded();
    settings->setUsesEncodingDetector(false);
    settings->setImagesEnabled(true);
    settings->setAcceleratedCompositingEnabled(acceleratedCompositingEnabled);
    settings->setAccelerated2dCanvasEnabled(accelerated2dCanvasEnabled);
}

