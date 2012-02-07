/*
 * Copyright (C) 2011 Igalia S.L.
 * Portions Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef WebViewTest_h
#define WebViewTest_h

#include "TestMain.h"
#include <webkit2/webkit2.h>
#include <wtf/text/CString.h>

class WebViewTest: public Test {
public:
    MAKE_GLIB_TEST_FIXTURE(WebViewTest);
    WebViewTest();
    virtual ~WebViewTest();

    virtual void loadURI(const char* uri);
    virtual void loadHtml(const char* html, const char* baseURI);
    virtual void loadPlainText(const char* plainText);
    virtual void loadRequest(WebKitURIRequest*);
    void replaceContent(const char* html, const char* contentURI, const char* baseURI);
    void goBack();
    void goForward();
    void goToBackForwardListItem(WebKitBackForwardListItem*);

    void wait(double seconds);
    void waitUntilLoadFinished();

    WebKitWebView* m_webView;
    GMainLoop* m_mainLoop;
    CString m_activeURI;
};

#endif // WebViewTest_h
