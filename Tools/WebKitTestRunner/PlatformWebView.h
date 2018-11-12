/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "TestOptions.h"

#if PLATFORM(COCOA) && !defined(BUILDING_GTK__)
#include <WebKit/WKFoundation.h>
#include <wtf/RetainPtr.h>
OBJC_CLASS NSView;
OBJC_CLASS UIView;
OBJC_CLASS TestRunnerWKWebView;
OBJC_CLASS WKWebViewConfiguration;
OBJC_CLASS WebKitTestRunnerWindow;
typedef struct CGImage *CGImageRef;

#if WK_API_ENABLED
typedef TestRunnerWKWebView *PlatformWKView;
#else
typedef NSView *PlatformWKView;
#endif
typedef WebKitTestRunnerWindow *PlatformWindow;
typedef RetainPtr<CGImageRef> PlatformImage;
#elif defined(BUILDING_GTK__)
typedef struct _GtkWidget GtkWidget;
typedef WKViewRef PlatformWKView;
typedef GtkWidget* PlatformWindow;
#elif PLATFORM(WPE)
namespace WPEToolingBackends {
class HeadlessViewBackend;
}
typedef WKViewRef PlatformWKView;
typedef WPEToolingBackends::HeadlessViewBackend* PlatformWindow;
#elif PLATFORM(WIN)
#include <cairo.h>
class TestRunnerWindow;
typedef HWND PlatformWindow;
typedef WKViewRef PlatformWKView;
#endif

#if USE(CAIRO)
typedef cairo_surface_t* PlatformImage;
#endif

namespace WTR {

class PlatformWebView {
public:
#if PLATFORM(COCOA)
    PlatformWebView(WKWebViewConfiguration*, const TestOptions&);
#else
    PlatformWebView(WKPageConfigurationRef, const TestOptions&);
#endif
    ~PlatformWebView();

    WKPageRef page();
    PlatformWKView platformView() { return m_view; }
    PlatformWindow platformWindow() { return m_window; }
    static PlatformWindow keyWindow();

    enum class WebViewSizingMode {
        Default,
        HeightRespectsStatusBar
    };

    void resizeTo(unsigned width, unsigned height, WebViewSizingMode = WebViewSizingMode::Default);
    void focus();

    WKRect windowFrame();
    void setWindowFrame(WKRect, WebViewSizingMode = WebViewSizingMode::Default);

    void didInitializeClients();
    
    void addChromeInputField();
    void removeChromeInputField();
    void makeWebViewFirstResponder();
    void setWindowIsKey(bool);
    bool windowIsKey() const { return m_windowIsKey; }

    bool drawsBackground() const;
    void setDrawsBackground(bool);

    void removeFromWindow();
    void addToWindow();

    bool viewSupportsOptions(const TestOptions& options) const { return !options.runSingly && m_options.hasSameInitializationOptions(options); }

    PlatformImage windowSnapshotImage();
    const TestOptions& options() const { return m_options; }

    void changeWindowScaleIfNeeded(float newScale);
    void setNavigationGesturesEnabled(bool);

#if PLATFORM(GTK)
    void dismissAllPopupMenus();
#endif

private:
    void forceWindowFramesChanged();

    PlatformWKView m_view;
    PlatformWindow m_window;
    bool m_windowIsKey;
    const TestOptions m_options;
#if PLATFORM(GTK)
    GtkWidget* m_otherWindow { nullptr };
#endif
};

} // namespace WTR
