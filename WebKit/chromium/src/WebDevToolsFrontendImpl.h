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

#ifndef WebDevToolsFrontendImpl_h
#define WebDevToolsFrontendImpl_h

#include "ContextMenu.h"
#include "ContextMenuProvider.h"
#include "DevToolsRPC.h"
#include "WebDevToolsFrontend.h"
#include <v8.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {
class ContextMenuItem;
class Node;
class Page;
class String;
}

namespace WebKit {

class JSDebuggerAgentBoundObj;
class JSProfilerAgentBoundObj;
class JSToolsAgentBoundObj;
class WebDevToolsClientDelegate;
class WebViewImpl;
struct WebDevToolsMessageData;

class WebDevToolsFrontendImpl : public WebKit::WebDevToolsFrontend
                              , public DevToolsRPC::Delegate
                              , public Noncopyable {
public:
    WebDevToolsFrontendImpl(
        WebKit::WebViewImpl* webViewImpl,
        WebKit::WebDevToolsFrontendClient* client,
        const String& applicationLocale);
    virtual ~WebDevToolsFrontendImpl();

    // DevToolsRPC::Delegate implementation.
    virtual void sendRpcMessage(const WebKit::WebDevToolsMessageData& data);

    // WebDevToolsFrontend implementation.
    virtual void dispatchMessageFromAgent(const WebKit::WebDevToolsMessageData& data);

private:
    class MenuProvider : public WebCore::ContextMenuProvider {
    public:
        static PassRefPtr<MenuProvider> create(WebDevToolsFrontendImpl* frontendHost, const Vector<WebCore::ContextMenuItem*>& items)
        {
            return adoptRef(new MenuProvider(frontendHost, items));
        }

        virtual ~MenuProvider()
        {
            contextMenuCleared();
        }

        void disconnect()
        {
            m_frontendHost = 0;
        }

        virtual void populateContextMenu(WebCore::ContextMenu* menu)
        {
            for (size_t i = 0; i < m_items.size(); ++i)
                menu->appendItem(*m_items[i]);
        }

        virtual void contextMenuItemSelected(WebCore::ContextMenuItem* item)
        {
            if (m_frontendHost)
                m_frontendHost->contextMenuItemSelected(item);
        }

        virtual void contextMenuCleared()
        {
            if (m_frontendHost)
                m_frontendHost->contextMenuCleared();
            deleteAllValues(m_items);
            m_items.clear();
        }

    private:
        MenuProvider(WebDevToolsFrontendImpl* frontendHost, const Vector<WebCore::ContextMenuItem*>& items)
            : m_frontendHost(frontendHost)
            , m_items(items) { }
        WebDevToolsFrontendImpl* m_frontendHost;
        Vector<WebCore::ContextMenuItem*> m_items;
    };

    void executeScript(const Vector<String>& v);
    void dispatchOnWebInspector(const String& method, const String& param);

    // friend class MenuSelectionHandler;
    void contextMenuItemSelected(WebCore::ContextMenuItem* menuItem);
    void contextMenuCleared();

    static v8::Handle<v8::Value> jsLoaded(const v8::Arguments& args);
    static v8::Handle<v8::Value> jsPlatform(const v8::Arguments& args);
    static v8::Handle<v8::Value> jsPort(const v8::Arguments& args);
    static v8::Handle<v8::Value> jsCopyText(const v8::Arguments& args);

    static v8::Handle<v8::Value> jsActivateWindow(const v8::Arguments& args);
    static v8::Handle<v8::Value> jsCloseWindow(const v8::Arguments& args);
    static v8::Handle<v8::Value> jsDockWindow(const v8::Arguments& args);
    static v8::Handle<v8::Value> jsUndockWindow(const v8::Arguments& args);
    static v8::Handle<v8::Value> jsLocalizedStringsURL(const v8::Arguments& args);
    static v8::Handle<v8::Value> jsHiddenPanels(const v8::Arguments& args);
    static v8::Handle<v8::Value> jsDebuggerCommand(const v8::Arguments& args);
    static v8::Handle<v8::Value> jsSetting(const v8::Arguments& args);
    static v8::Handle<v8::Value> jsSetSetting(const v8::Arguments& args);
    static v8::Handle<v8::Value> jsDebuggerPauseScript(const v8::Arguments& args);
    static v8::Handle<v8::Value> jsWindowUnloading(const v8::Arguments& args);
    static v8::Handle<v8::Value> jsShowContextMenu(const v8::Arguments& args);

    WebKit::WebViewImpl* m_webViewImpl;
    WebKit::WebDevToolsFrontendClient* m_client;
    String m_applicationLocale;
    OwnPtr<JSDebuggerAgentBoundObj> m_debuggerAgentObj;
    OwnPtr<JSProfilerAgentBoundObj> m_profilerAgentObj;
    OwnPtr<JSToolsAgentBoundObj> m_toolsAgentObj;
    bool m_loaded;
    Vector<Vector<String> > m_pendingIncomingMessages;
    RefPtr<MenuProvider> m_menuProvider;
};

} // namespace WebKit

#endif
