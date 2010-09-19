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

#ifndef WebPreferences_h
#define WebPerferences_h

#include "public/WebSettings.h"
#include "public/WebString.h"
#include "public/WebURL.h"

namespace WebKit {
class WebView;
}

struct WebPreferences {
    WebKit::WebString cursiveFontFamily;
    WebKit::WebString fantasyFontFamily;
    WebKit::WebString serifFontFamily;
    WebKit::WebString standardFontFamily;
    WebKit::WebString fixedFontFamily;
    WebKit::WebString sansSerifFontFamily;

    int defaultFontSize;
    int defaultFixedFontSize;
    int minimumFontSize;
    int minimumLogicalFontSize;

    bool DOMPasteAllowed;
    bool XSSAuditorEnabled;
    bool allowFileAccessFromFileURLs;
    bool authorAndUserStylesEnabled;
    WebKit::WebString defaultTextEncodingName;
    bool developerExtrasEnabled;
    bool experimentalWebGLEnabled;
    bool javaEnabled;
    bool javaScriptCanAccessClipboard;
    bool javaScriptCanOpenWindowsAutomatically;
    bool javaScriptEnabled;
    bool loadsImagesAutomatically;
    bool localStorageEnabled;
    bool offlineWebApplicationCacheEnabled;
    bool pluginsEnabled;
    bool shrinksStandaloneImagesToFit;
    bool textAreasAreResizable;
    WebKit::WebURL userStyleSheetLocation;
    bool usesPageCache;
    bool webSecurityEnabled;
    bool allowUniversalAccessFromFileURLs;
    WebKit::WebSettings::EditingBehavior editingBehavior;
    bool tabsToLinks;

    WebPreferences() { reset(); }
    void reset();
    void applyTo(WebKit::WebView*);
};

#endif // WebPreferences_h
