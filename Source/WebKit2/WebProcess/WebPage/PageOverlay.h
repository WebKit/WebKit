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

#ifndef PageOverlay_h
#define PageOverlay_h

#include "APIObject.h"
#include "WKBase.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RunLoop.h>

namespace WebCore {
    class GraphicsContext;
    class IntPoint;
    class IntRect;
}

namespace WebKit {

class WebMouseEvent;
class WebPage;

class PageOverlay : public API::ObjectImpl<API::Object::Type::BundlePageOverlay> {
public:
    class Client {
    protected:
        virtual ~Client() { }
    
    public:
        virtual void pageOverlayDestroyed(PageOverlay*) = 0;
        virtual void willMoveToWebPage(PageOverlay*, WebPage*) = 0;
        virtual void didMoveToWebPage(PageOverlay*, WebPage*) = 0;
        virtual void drawRect(PageOverlay*, WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRect) = 0;
        virtual bool mouseEvent(PageOverlay*, const WebMouseEvent&) = 0;

        virtual WKTypeRef copyAccessibilityAttributeValue(PageOverlay*, WKStringRef /* attribute */, WKTypeRef /* parameter */) { return 0; }
        virtual WKArrayRef copyAccessibilityAttributeNames(PageOverlay*, bool /* parameterizedNames */) { return 0; }
    };

    static PassRefPtr<PageOverlay> create(Client*);
    virtual ~PageOverlay();

    void setPage(WebPage*);
    void setNeedsDisplay(const WebCore::IntRect& dirtyRect);
    void setNeedsDisplay();

    void drawRect(WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRect);
    bool mouseEvent(const WebMouseEvent&);

    WKTypeRef copyAccessibilityAttributeValue(WKStringRef attribute, WKTypeRef parameter);
    WKArrayRef copyAccessibilityAttributeNames(bool parameterizedNames);
    
    void startFadeInAnimation();
    void startFadeOutAnimation();
    void stopFadeOutAnimation();

    void clear();

    Client* client() const { return m_client; }
    
protected:
    explicit PageOverlay(Client*);

private:
    WebCore::IntRect bounds() const;

    void startFadeAnimation();
    void fadeAnimationTimerFired();

    Client* m_client;
    WebPage* m_webPage;

    RunLoop::Timer<PageOverlay> m_fadeAnimationTimer;
    double m_fadeAnimationStartTime;
    double m_fadeAnimationDuration;

    enum FadeAnimationType {
        NoAnimation,
        FadeInAnimation,
        FadeOutAnimation,
    };

    FadeAnimationType m_fadeAnimationType;
    float m_fractionFadedIn;
};

} // namespace WebKit

#endif // PageOverlay_h
