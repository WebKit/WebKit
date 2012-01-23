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

#ifndef InspectorFrontendClientImpl_h
#define InspectorFrontendClientImpl_h

#include "InspectorFrontendClient.h"
#include <wtf/Noncopyable.h>

namespace WebCore {
class InspectorFrontendHost;
class Page;
}

namespace WebKit {

class WebDevToolsFrontendClient;
class WebDevToolsFrontendImpl;

class InspectorFrontendClientImpl : public WebCore::InspectorFrontendClient {
    WTF_MAKE_NONCOPYABLE(InspectorFrontendClientImpl);
public:
    InspectorFrontendClientImpl(WebCore::Page*, WebDevToolsFrontendClient*, WebDevToolsFrontendImpl*);
    virtual ~InspectorFrontendClientImpl();

    // InspectorFrontendClient methods:
    virtual void windowObjectCleared();
    virtual void frontendLoaded();

    virtual void moveWindowBy(float x, float y);

    virtual WTF::String localizedStringsURL();
    virtual WTF::String hiddenPanels();

    virtual void bringToFront();
    virtual void closeWindow();

    virtual void requestAttachWindow();
    virtual void requestDetachWindow();
    virtual void requestSetDockSide(const String&);
    virtual void changeAttachedWindowHeight(unsigned);

    virtual void openInNewTab(const String& url);

    virtual bool canSaveAs();
    virtual void saveAs(const WTF::String& fileName, const WTF::String& content);

    virtual void inspectedURLChanged(const WTF::String&);

    virtual void sendMessageToBackend(const WTF::String&);
private:
    WebCore::Page* m_frontendPage;
    WebDevToolsFrontendClient* m_client;
    WebDevToolsFrontendImpl* m_frontend;
    RefPtr<WebCore::InspectorFrontendHost> m_frontendHost;
};

} // namespace WebKit

#endif
