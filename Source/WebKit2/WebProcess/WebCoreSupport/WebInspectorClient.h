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

#ifndef WebInspectorClient_h
#define WebInspectorClient_h

#if ENABLE(INSPECTOR)

#include <WebCore/InspectorClient.h>
#include <WebCore/InspectorForwarding.h>
#include <WebCore/PageOverlay.h>

namespace WebCore {
class GraphicsContext;
class IntRect;
class PageOverlay;
}

namespace WebKit {

class WebPage;

class WebInspectorClient : public WebCore::InspectorClient, public WebCore::InspectorFrontendChannel, private WebCore::PageOverlay::Client {
public:
    WebInspectorClient(WebPage* page)
        : m_page(page)
        , m_highlightOverlay(0)
    {
    }

private:
    virtual void inspectorDestroyed() override;

    virtual InspectorFrontendChannel* openInspectorFrontend(WebCore::InspectorController*) override;
    virtual void closeInspectorFrontend() override;
    virtual void bringFrontendToFront() override;
    virtual void didResizeMainFrame(WebCore::Frame*) override;

    virtual void highlight() override;
    virtual void hideHighlight() override;

#if PLATFORM(IOS)
    virtual void showInspectorIndication() override;
    virtual void hideInspectorIndication() override;

    virtual void didSetSearchingForNode(bool) override;
#endif

    virtual bool sendMessageToFrontend(const String&) override;

    virtual bool supportsFrameInstrumentation();

    // PageOverlay::Client
    virtual void pageOverlayDestroyed(WebCore::PageOverlay&) override;
    virtual void willMoveToPage(WebCore::PageOverlay&, WebCore::Page*) override;
    virtual void didMoveToPage(WebCore::PageOverlay&, WebCore::Page*) override;
    virtual void drawRect(WebCore::PageOverlay&, WebCore::GraphicsContext&, const WebCore::IntRect&) override;
    virtual bool mouseEvent(WebCore::PageOverlay&, const WebCore::PlatformMouseEvent&) override;

    WebPage* m_page;
    WebCore::PageOverlay* m_highlightOverlay;
};

} // namespace WebKit

#endif // ENABLE(INSPECTOR)

#endif // WebInspectorClient_h
