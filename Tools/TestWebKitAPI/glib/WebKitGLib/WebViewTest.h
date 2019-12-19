/*
 * Copyright (C) 2011, 2017 Igalia S.L.
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

#pragma once

#include "TestMain.h"
#include <wtf/text/CString.h>

class WebViewTest: public Test {
public:
    MAKE_GLIB_TEST_FIXTURE(WebViewTest);
    WebViewTest();
    virtual ~WebViewTest();

    static bool shouldInitializeWebViewInConstructor;
    static bool shouldCreateEphemeralWebView;
    void initializeWebView();

    void platformInitializeWebView();
    void platformDestroy();

    virtual void loadURI(const char* uri);
    virtual void loadHtml(const char* html, const char* baseURI);
    virtual void loadPlainText(const char* plainText);
    virtual void loadRequest(WebKitURIRequest*);
    virtual void loadBytes(GBytes*, const char* mimeType, const char* encoding, const char* baseURI);
    void loadAlternateHTML(const char* html, const char* contentURI, const char* baseURI);
    void goBack();
    void goForward();
    void goToBackForwardListItem(WebKitBackForwardListItem*);

    void quitMainLoop();
    void quitMainLoopAfterProcessingPendingEvents();
    void wait(double seconds);
    void waitUntilLoadFinished();
    void waitUntilTitleChangedTo(const char* expectedTitle);
    void waitUntilTitleChanged();
    void resizeView(int width, int height);
    void hideView();
    void selectAll();
    const char* mainResourceData(size_t& mainResourceDataSize);

    bool isEditable();
    void setEditable(bool);

    void mouseMoveTo(int x, int y, unsigned mouseModifiers = 0);
    void clickMouseButton(int x, int y, unsigned button = 1, unsigned mouseModifiers = 0);
    void keyStroke(unsigned keyVal, unsigned keyModifiers = 0);

#if PLATFORM(GTK)
    void showInWindow(GtkWindowType = GTK_WINDOW_POPUP);
    void showInWindowAndWaitUntilMapped(GtkWindowType = GTK_WINDOW_POPUP, int width = 0, int height = 0);
    void emitPopupMenuSignal();
#endif

#if PLATFORM(WPE)
    void showInWindowAndWaitUntilMapped() { showInWindow(); };
    void showInWindow();
#endif

    WebKitJavascriptResult* runJavaScriptAndWaitUntilFinished(const char* javascript, GError**);
    WebKitJavascriptResult* runJavaScriptFromGResourceAndWaitUntilFinished(const char* resource, GError**);
    WebKitJavascriptResult* runJavaScriptInWorldAndWaitUntilFinished(const char* javascript, const char* world, GError**);

    // Javascript result helpers.
    static char* javascriptResultToCString(WebKitJavascriptResult*);
    static double javascriptResultToNumber(WebKitJavascriptResult*);
    static bool javascriptResultToBoolean(WebKitJavascriptResult*);
    static bool javascriptResultIsNull(WebKitJavascriptResult*);
    static bool javascriptResultIsUndefined(WebKitJavascriptResult*);

#if PLATFORM(GTK)
    cairo_surface_t* getSnapshotAndWaitUntilReady(WebKitSnapshotRegion, WebKitSnapshotOptions);
#endif

    bool runWebProcessTest(const char* suiteName, const char* testName, const char* contents = nullptr, const char* contentType = nullptr);

    // Prohibit overrides because this is called when the web view is created
    // in our constructor, before a derived class's vtable is ready.
    void initializeWebExtensions() final { Test::initializeWebExtensions(); }

    static gboolean webProcessTerminated(WebKitWebView*, WebKitWebProcessTerminationReason, WebViewTest*);

    GRefPtr<GDBusProxy> extensionProxy();

    GRefPtr<WebKitUserContentManager> m_userContentManager;
    WebKitWebView* m_webView { nullptr };
    GMainLoop* m_mainLoop;
    CString m_activeURI;
    CString m_expectedTitle;
    WebKitJavascriptResult* m_javascriptResult { nullptr };
    GError** m_javascriptError { nullptr };
    GUniquePtr<char> m_resourceData { nullptr };
    size_t m_resourceDataSize { 0 };
    cairo_surface_t* m_surface { nullptr };
    bool m_expectedWebProcessCrash { false };

#if PLATFORM(GTK)
    GtkWidget* m_parentWindow { nullptr };
#endif

private:
#if PLATFORM(GTK)
    void doMouseButtonEvent(GdkEventType, int, int, unsigned, unsigned);
#endif
};
