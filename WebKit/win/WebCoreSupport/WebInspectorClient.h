/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebInspectorClient_h
#define WebInspectorClient_h

#include <WebCore/COMPtr.h>
#include <WebCore/InspectorClient.h>
#include <WebCore/PlatformString.h>
#include <WebCore/WindowMessageListener.h>
#include <wtf/OwnPtr.h>
#include <windows.h>

class WebNodeHighlight;
class WebView;

class WebInspectorClient : public WebCore::InspectorClient, WebCore::WindowMessageListener {
public:
    WebInspectorClient(WebView*);

    // InspectorClient
    virtual void inspectorDestroyed();

    virtual WebCore::Page* createPage();

    virtual WebCore::String localizedStringsURL();

    virtual WebCore::String hiddenPanels();

    virtual void showWindow();
    virtual void closeWindow();
    virtual bool windowVisible();

    virtual void attachWindow();
    virtual void detachWindow();

    virtual void setAttachedWindowHeight(unsigned height);

    virtual void highlight(WebCore::Node*);
    virtual void hideHighlight();

    virtual void inspectedURLChanged(const WebCore::String& newURL);

    virtual void populateSetting(const WebCore::String& key, WebCore::String* value);
    virtual void storeSetting(const WebCore::String& key, const WebCore::String& value);

    virtual void inspectorWindowObjectCleared();

private:
    ~WebInspectorClient();

    void closeWindowWithoutNotifications();
    void showWindowWithoutNotifications();

    void updateWindowTitle();

    LRESULT onGetMinMaxInfo(WPARAM, LPARAM);
    LRESULT onSize(WPARAM, LPARAM);
    LRESULT onClose(WPARAM, LPARAM);
    LRESULT onSetFocus();

    virtual void windowReceivedMessage(HWND, UINT message, WPARAM, LPARAM);

    void onWebViewWindowPosChanging(WPARAM, LPARAM);

    WebView* m_inspectedWebView;
    HWND m_inspectedWebViewHwnd;
    HWND m_hwnd;
    COMPtr<WebView> m_webView;
    HWND m_webViewHwnd;

    bool m_shouldAttachWhenShown;
    bool m_attached;

    OwnPtr<WebNodeHighlight> m_highlight;

    WebCore::String m_inspectedURL;

    static friend LRESULT CALLBACK WebInspectorWndProc(HWND, UINT, WPARAM, LPARAM);
};

#endif // !defined(WebInspectorClient_h)
