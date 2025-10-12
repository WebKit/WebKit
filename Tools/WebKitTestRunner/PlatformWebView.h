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
#include <wtf/FastMalloc.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA) && !defined(BUILDING_GTK__)
#include <WebKit/WKFoundation.h>
#include <wtf/RetainPtr.h>
OBJC_CLASS NSView;
OBJC_CLASS UIView;
OBJC_CLASS UIWindow;
OBJC_CLASS TestRunnerWKWebView;
OBJC_CLASS WKWebViewConfiguration;
OBJC_CLASS WebKitTestRunnerWindow;
typedef struct CGImage *CGImageRef;

using PlatformWKView = TestRunnerWKWebView*;
using PlatformWindow = WebKitTestRunnerWindow*;
using PlatformImage = RetainPtr<CGImageRef>;
#elif defined(BUILDING_GTK__)
typedef struct _GtkWidget GtkWidget;
typedef WKViewRef PlatformWKView;
typedef GtkWidget* PlatformWindow;
#elif USE(LIBWPE)
namespace WTR {
class PlatformWebViewClient;
}
using PlatformWKView = WKViewRef;
using PlatformWindow = WTR::PlatformWebViewClient*;
#elif PLATFORM(WIN)
using PlatformWKView = WKViewRef;
using PlatformWindow = HWND;
#endif

#if USE(CAIRO)
#include <cairo.h>
using PlatformImage = cairo_surface_t*;
#elif USE(SKIA)
#include <skia/core/SkImage.h>
using PlatformImage = SkImage*;
#endif

namespace WTR {

class PlatformWebView {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(PlatformWebView);
public:
    PlatformWebView(WKPageConfigurationRef, const TestOptions&);
    ~PlatformWebView();

    WKPageRef page();
    PlatformWKView platformView() const { return m_view; }
    PlatformWindow platformWindow() const { return m_window; }
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

    void setTextInChromeInputField(const String&);
    void selectChromeInputField();
    String getSelectedTextInChromeInputField();

    bool isSecureEventInputEnabled() const;

    bool drawsBackground() const;
    void setDrawsBackground(bool);

    void setEditable(bool);

    void removeFromWindow();
    void addToWindow();

    bool viewSupportsOptions(const TestOptions& options) const { return !options.runSingly() && m_options.hasSameInitializationOptions(options); }

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
#if PLATFORM(IOS_FAMILY)
    RetainPtr<UIWindow> m_otherWindow;
#endif
#if PLATFORM(GTK)
    GtkWidget* m_otherWindow { nullptr };
#endif
};

} // namespace WTR
