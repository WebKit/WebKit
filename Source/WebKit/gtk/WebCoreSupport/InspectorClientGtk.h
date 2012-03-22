/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#ifndef InspectorClientGtk_h
#define InspectorClientGtk_h

#include "InspectorClient.h"
#include "InspectorFrontendClientLocal.h"
#include "webkitwebview.h"
#include "webkitwebinspector.h"
#include <wtf/Forward.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/gobject/GRefPtr.h>

namespace WebCore {
    class Page;
}

namespace WebKit {

    class InspectorFrontendClient;

    class InspectorClient : public WebCore::InspectorClient {
    public:
        InspectorClient(WebKitWebView* webView);
        ~InspectorClient();

        void disconnectFrontendClient() { m_frontendClient = 0; }

        virtual void inspectorDestroyed();

        virtual void openInspectorFrontend(WebCore::InspectorController*);
        virtual void closeInspectorFrontend();
        virtual void bringFrontendToFront();

        virtual void highlight();
        virtual void hideHighlight();

        virtual bool sendMessageToFrontend(const WTF::String&);

        void releaseFrontendPage();
        const char* inspectorFilesPath();

    private:
        WebKitWebView* m_inspectedWebView;
        WebCore::Page* m_frontendPage;
        InspectorFrontendClient* m_frontendClient;
        GOwnPtr<gchar> m_inspectorFilesPath;
    };

    class InspectorFrontendClient : public WebCore::InspectorFrontendClientLocal {
    public:
        InspectorFrontendClient(WebKitWebView* inspectedWebView, WebKitWebView* inspectorWebView, WebKitWebInspector* webInspector, WebCore::Page* inspectorPage, InspectorClient* inspectorClient);
        virtual ~InspectorFrontendClient();

        void disconnectInspectorClient() { m_inspectorClient = 0; }

        void destroyInspectorWindow(bool notifyInspectorController);

        virtual WTF::String localizedStringsURL();

        virtual WTF::String hiddenPanels();

        virtual void bringToFront();
        virtual void closeWindow();

        virtual void attachWindow();
        virtual void detachWindow();

        virtual void setAttachedWindowHeight(unsigned height);

        virtual void inspectedURLChanged(const WTF::String& newURL);

    private:
        WebKitWebView* m_inspectorWebView;
        WebKitWebView* m_inspectedWebView;
        GRefPtr<WebKitWebInspector> m_webInspector;
        InspectorClient* m_inspectorClient;
    };
}

#endif
