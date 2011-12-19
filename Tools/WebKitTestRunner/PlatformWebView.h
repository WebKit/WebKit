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

#include <WebKit2/WKRetainPtr.h>

#if defined(BUILDING_QT__)
class QQuickWebView;
typedef QQuickWebView* PlatformWKView;
class QQuickView;
typedef QQuickView* PlatformWindow;
class QEventLoop;
#elif defined(__APPLE__) && __APPLE__
#if __OBJC__
@class WKView;
@class WebKitTestRunnerWindow;
#else
class WKView;
class WebKitTestRunnerWindow;
#endif
typedef WKView* PlatformWKView;
typedef WebKitTestRunnerWindow* PlatformWindow;
#elif defined(WIN32) || defined(_WIN32)
typedef WKViewRef PlatformWKView;
typedef HWND PlatformWindow;
#elif defined(BUILDING_GTK__)
typedef struct _GtkWidget GtkWidget;
typedef WKViewRef PlatformWKView;
typedef GtkWidget* PlatformWindow;
#endif

namespace WTR {

class PlatformWebView {
public:
    PlatformWebView(WKContextRef, WKPageGroupRef);
    ~PlatformWebView();

    WKPageRef page();
    PlatformWKView platformView() { return m_view; }
    PlatformWindow platformWindow() { return m_window; }
    void resizeTo(unsigned width, unsigned height);
    void focus();

#if PLATFORM(QT)
    bool sendEvent(QEvent*);
    void postEvent(QEvent*);
    void setModalEventLoop(QEventLoop* eventLoop) { m_modalEventLoop = eventLoop; }
#endif

    WKRect windowFrame();
    void setWindowFrame(WKRect);
    
    void addChromeInputField();
    void removeChromeInputField();
    void makeWebViewFirstResponder();
    void setWindowIsKey(bool isKey) { m_windowIsKey = isKey; }
    bool windowIsKey() const { return m_windowIsKey; }
    
    WKRetainPtr<WKImageRef> windowSnapshotImage();

private:
    PlatformWKView m_view;
    PlatformWindow m_window;
    bool m_windowIsKey;
#if PLATFORM(QT)
    QEventLoop* m_modalEventLoop;
#endif
};

} // namespace WTR

#endif // PlatformWebView_h
