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

#ifndef WebInspectorProxy_h
#define WebInspectorProxy_h

#if ENABLE(INSPECTOR)

#include "APIObject.h"
#include "Connection.h"
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>

OBJC_CLASS NSWindow;
OBJC_CLASS WebInspectorProxyObjCAdapter;
OBJC_CLASS WebInspectorWKView;
#endif

#if PLATFORM(WIN)
#include <WebCore/WindowMessageListener.h>
#endif

namespace WebKit {

class WebPageGroup;
class WebPageProxy;
struct WebPageCreationParameters;

#if PLATFORM(WIN)
class WebView;
#endif

class WebInspectorProxy : public APIObject
#if PLATFORM(WIN)
    , public WebCore::WindowMessageListener
#endif
{
public:
    static const Type APIType = TypeInspector;

    static PassRefPtr<WebInspectorProxy> create(WebPageProxy* page)
    {
        return adoptRef(new WebInspectorProxy(page));
    }

    ~WebInspectorProxy();

    void invalidate();

    // Public APIs
    WebPageProxy* page() const { return m_page; }

    bool isVisible() const { return m_isVisible; }
    void show();
    void close();
    
#if PLATFORM(MAC)
    void inspectedViewFrameDidChange();
#endif

    void showConsole();

    bool isAttached() const { return m_isAttached; }
    void attach();
    void detach();
    void setAttachedWindowHeight(unsigned);

    bool isDebuggingJavaScript() const { return m_isDebuggingJavaScript; }
    void toggleJavaScriptDebugging();

    bool isProfilingJavaScript() const { return m_isProfilingJavaScript; }
    void toggleJavaScriptProfiling();

    bool isProfilingPage() const { return m_isProfilingPage; }
    void togglePageProfiling();

#if ENABLE(INSPECTOR)
    // Implemented in generated WebInspectorProxyMessageReceiver.cpp
    void didReceiveWebInspectorProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    CoreIPC::SyncReplyMode didReceiveSyncWebInspectorProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, CoreIPC::ArgumentEncoder*);
#endif

    static bool isInspectorPage(WebPageProxy*);

private:
    WebInspectorProxy(WebPageProxy* page);

    virtual Type type() const { return APIType; }

    WebPageProxy* platformCreateInspectorPage();
    void platformOpen(bool willOpenAttached);
    void platformDidClose();
    void platformBringToFront();
    void platformInspectedURLChanged(const String&);
    void platformAttach();
    void platformDetach();
    void platformSetAttachedWindowHeight(unsigned);

    // Implemented the platform WebInspectorProxy file
    String inspectorPageURL() const;

    // Called by WebInspectorProxy messages
    void createInspectorPage(uint64_t& inspectorPageID, WebPageCreationParameters&);
    void didLoadInspectorPage(bool canStartAttached);
    void didClose();
    void bringToFront();
    void inspectedURLChanged(const String&);

    static WebPageGroup* inspectorPageGroup();

#if PLATFORM(WIN)
    static bool registerInspectorViewWindowClass();
    static LRESULT CALLBACK InspectorViewWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT onSizeEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onMinMaxInfoEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onSetFocusEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onCloseEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);

    void onWebViewWindowPosChangingEvent(WPARAM, LPARAM);
    virtual void windowReceivedMessage(HWND, UINT message, WPARAM, LPARAM);
#endif

    static const unsigned minimumWindowWidth = 500;
    static const unsigned minimumWindowHeight = 400;

    static const unsigned initialWindowWidth = 750;
    static const unsigned initialWindowHeight = 650;

    WebPageProxy* m_page;

    bool m_isVisible;
    bool m_isAttached;
    bool m_isDebuggingJavaScript;
    bool m_isProfilingJavaScript;
    bool m_isProfilingPage;

#if PLATFORM(MAC)
    RetainPtr<WebInspectorWKView> m_inspectorView;
    RetainPtr<NSWindow> m_inspectorWindow;
    RetainPtr<WebInspectorProxyObjCAdapter> m_inspectorProxyObjCAdapter;
#elif PLATFORM(WIN)
    HWND m_inspectorWindow;
    RefPtr<WebView> m_inspectorView;
#endif
};

} // namespace WebKit

#endif // ENABLE(INSPECTOR)

#endif // WebInspectorProxy_h
