/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "APIObject.h"
#include <WebCore/FloatPoint.h>
#include <WebCore/PageOverlay.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS DDActionContext;

namespace WebCore {
class IntRect;
}

namespace WebKit {

class WebFrame;
class WebPage;

class WebPageOverlay : public API::ObjectImpl<API::Object::Type::BundlePageOverlay>, private WebCore::PageOverlay::Client {
public:
    struct ActionContext;

    class Client {
    public:
        virtual ~Client() { }

        virtual void willMoveToPage(WebPageOverlay&, WebPage*) = 0;
        virtual void didMoveToPage(WebPageOverlay&, WebPage*) = 0;
        virtual void drawRect(WebPageOverlay&, WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRect) = 0;
        virtual bool mouseEvent(WebPageOverlay&, const WebCore::PlatformMouseEvent&) = 0;
        virtual void didScrollFrame(WebPageOverlay&, WebFrame*) { }

#if PLATFORM(MAC)
        virtual Optional<ActionContext> actionContextForResultAtPoint(WebPageOverlay&, WebCore::FloatPoint) { return WTF::nullopt; }
        virtual void dataDetectorsDidPresentUI(WebPageOverlay&) { }
        virtual void dataDetectorsDidChangeUI(WebPageOverlay&) { }
        virtual void dataDetectorsDidHideUI(WebPageOverlay&) { }
#endif

        virtual bool copyAccessibilityAttributeStringValueForPoint(WebPageOverlay&, String /* attribute */, WebCore::FloatPoint /* parameter */, String& /* value */) { return false; }
        virtual bool copyAccessibilityAttributeBoolValueForPoint(WebPageOverlay&, String /* attribute */, WebCore::FloatPoint /* parameter */, bool& /* value */) { return false; }
        virtual Vector<String> copyAccessibilityAttributeNames(WebPageOverlay&, bool /* parameterizedNames */) { return Vector<String>(); }
    };

    static Ref<WebPageOverlay> create(std::unique_ptr<Client>, WebCore::PageOverlay::OverlayType = WebCore::PageOverlay::OverlayType::View);
    static WebPageOverlay* fromCoreOverlay(WebCore::PageOverlay&);
    virtual ~WebPageOverlay();

    void setNeedsDisplay(const WebCore::IntRect& dirtyRect);
    void setNeedsDisplay();

    void clear();

    WebCore::PageOverlay* coreOverlay() const { return m_overlay.get(); }
    Client& client() const { return *m_client; }

#if PLATFORM(MAC)
    struct ActionContext {
        RetainPtr<DDActionContext> context;
        WebCore::SimpleRange range;
    };
    Optional<ActionContext> actionContextForResultAtPoint(WebCore::FloatPoint);
    void dataDetectorsDidPresentUI();
    void dataDetectorsDidChangeUI();
    void dataDetectorsDidHideUI();
#endif

private:
    WebPageOverlay(std::unique_ptr<Client>, WebCore::PageOverlay::OverlayType);

    // WebCore::PageOverlay::Client
    void willMoveToPage(WebCore::PageOverlay&, WebCore::Page*) override;
    void didMoveToPage(WebCore::PageOverlay&, WebCore::Page*) override;
    void drawRect(WebCore::PageOverlay&, WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRect) override;
    bool mouseEvent(WebCore::PageOverlay&, const WebCore::PlatformMouseEvent&) override;
    void didScrollFrame(WebCore::PageOverlay&, WebCore::Frame&) override;

    bool copyAccessibilityAttributeStringValueForPoint(WebCore::PageOverlay&, String /* attribute */, WebCore::FloatPoint /* parameter */, String& value) override;
    bool copyAccessibilityAttributeBoolValueForPoint(WebCore::PageOverlay&, String /* attribute */, WebCore::FloatPoint /* parameter */, bool& value) override;
    Vector<String> copyAccessibilityAttributeNames(WebCore::PageOverlay&, bool /* parameterizedNames */) override;

    RefPtr<WebCore::PageOverlay> m_overlay;
    std::unique_ptr<Client> m_client;
};

} // namespace WebKit
