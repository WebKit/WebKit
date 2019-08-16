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

#ifndef PlatformWebView_h
#define PlatformWebView_h

#include <wtf/FastMalloc.h>

#if USE(CG)
#include <CoreGraphics/CGGeometry.h>
#endif

#if PLATFORM(MAC)
#include <objc/objc.h>
#endif

#if defined(__APPLE__) && !PLATFORM(GTK)
#ifdef __OBJC__
@class WKView;
@class NSWindow;
#else
class WKView;
class NSWindow;
#endif
typedef WKView *PlatformWKView;
typedef NSWindow *PlatformWindow;
#elif PLATFORM(GTK)
typedef WKViewRef PlatformWKView;
typedef GtkWidget *PlatformWindow;
#elif PLATFORM(WPE)
namespace WPEToolingBackends {
class HeadlessViewBackend;
}
struct wpe_view_backend;
typedef WKViewRef PlatformWKView;
typedef WPEToolingBackends::HeadlessViewBackend *PlatformWindow;
#elif PLATFORM(WIN)
typedef WKViewRef PlatformWKView;
typedef HWND PlatformWindow;
#endif
typedef uint32_t WKEventModifiers;

namespace TestWebKitAPI {

class PlatformWebView {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit PlatformWebView(WKPageConfigurationRef);
    explicit PlatformWebView(WKContextRef, WKPageGroupRef = 0);
    explicit PlatformWebView(WKPageRef relatedPage);
#if PLATFORM(MAC)
    explicit PlatformWebView(WKContextRef, WKPageGroupRef, Class wkViewSubclass);
#endif
    ~PlatformWebView();

    WKPageRef page() const;
    PlatformWKView platformView() const { return m_view; }
    void resizeTo(unsigned width, unsigned height);
    void focus();

    void simulateSpacebarKeyPress();
    void simulateAltKeyPress();
    void simulateRightClick(unsigned x, unsigned y);
    void simulateMouseMove(unsigned x, unsigned y, WKEventModifiers = 0);
#if PLATFORM(MAC)
    void simulateButtonClick(WKEventMouseButton, unsigned x, unsigned y, WKEventModifiers);
#endif

private:
#if PLATFORM(MAC)
    void initialize(WKPageConfigurationRef, Class wkViewSubclass);
#elif PLATFORM(GTK) || PLATFORM(WPE) || PLATFORM(WIN)
    void initialize(WKPageConfigurationRef);
#endif
#if PLATFORM(WIN)
    static void registerWindowClass();
    static LRESULT CALLBACK wndProc(HWND, UINT message, WPARAM, LPARAM);
#endif

    PlatformWKView m_view;
    PlatformWindow m_window;
};

} // namespace TestWebKitAPI

#endif // PlatformWebView_h
