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

#include "config.h"
#include "WKBundlePageOverlay.h"

#include "APIClient.h"
#include "InjectedBundleRangeHandle.h"
#include "WKAPICast.h"
#include "WKArray.h"
#include "WKBundleAPICast.h"
#include "WKNumber.h"
#include "WKRetainPtr.h"
#include "WKSharedAPICast.h"
#include "WKStringPrivate.h"
#include "WebPage.h"
#include "WebPageOverlay.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/PageOverlay.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/Range.h>

namespace API {

template<> struct ClientTraits<WKBundlePageOverlayClientBase> {
    typedef std::tuple<WKBundlePageOverlayClientV0, WKBundlePageOverlayClientV1> Versions;
};

template<> struct ClientTraits<WKBundlePageOverlayAccessibilityClientBase> {
    typedef std::tuple<WKBundlePageOverlayAccessibilityClientV0> Versions;
};

}

class PageOverlayClientImpl : API::Client<WKBundlePageOverlayClientBase>, public WebKit::WebPageOverlay::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit PageOverlayClientImpl(WKBundlePageOverlayClientBase* client)
    {
        initialize(client);
    }

    virtual void setAccessibilityClient(WKBundlePageOverlayAccessibilityClientBase* client)
    {
        m_accessibilityClient.initialize(client);
    }

private:
    // WebKit::WebPageOverlay::Client.
    void willMoveToPage(WebKit::WebPageOverlay& pageOverlay, WebKit::WebPage* page) override
    {
        if (!m_client.willMoveToPage)
            return;

        m_client.willMoveToPage(toAPI(&pageOverlay), toAPI(page), m_client.base.clientInfo);
    }
    
    void didMoveToPage(WebKit::WebPageOverlay& pageOverlay, WebKit::WebPage* page) override
    {
        if (!m_client.didMoveToPage)
            return;

        m_client.didMoveToPage(toAPI(&pageOverlay), toAPI(page), m_client.base.clientInfo);
    }

    void drawRect(WebKit::WebPageOverlay& pageOverlay, WebCore::GraphicsContext& graphicsContext, const WebCore::IntRect& dirtyRect) override
    {
        if (!m_client.drawRect)
            return;

        m_client.drawRect(toAPI(&pageOverlay), graphicsContext.platformContext(), WebKit::toAPI(dirtyRect), m_client.base.clientInfo);
    }
    
    bool mouseEvent(WebKit::WebPageOverlay& pageOverlay, const WebCore::PlatformMouseEvent& event) override
    {
        switch (event.type()) {
        case WebCore::PlatformMouseEvent::Type::MousePressed: {
            if (!m_client.mouseDown)
                return false;

            return m_client.mouseDown(toAPI(&pageOverlay), WebKit::toAPI(event.position()), WebKit::toAPI(event.button()), m_client.base.clientInfo);
        }
        case WebCore::PlatformMouseEvent::Type::MouseReleased: {
            if (!m_client.mouseUp)
                return false;

            return m_client.mouseUp(toAPI(&pageOverlay), WebKit::toAPI(event.position()), WebKit::toAPI(event.button()), m_client.base.clientInfo);
        }
        case WebCore::PlatformMouseEvent::Type::MouseMoved: {
            if (event.button() == WebCore::MouseButton::NoButton) {
                if (!m_client.mouseMoved)
                    return false;

                return m_client.mouseMoved(toAPI(&pageOverlay), WebKit::toAPI(event.position()), m_client.base.clientInfo);
            }

            // This is a MouseMove event with a mouse button pressed. Call mouseDragged.
            if (!m_client.mouseDragged)
                return false;

            return m_client.mouseDragged(toAPI(&pageOverlay), WebKit::toAPI(event.position()), WebKit::toAPI(event.button()), m_client.base.clientInfo);
        }

        default:
            return false;
        }
    }

#if PLATFORM(MAC)
    Optional<WebKit::WebPageOverlay::ActionContext> actionContextForResultAtPoint(WebKit::WebPageOverlay& pageOverlay, WebCore::FloatPoint location) final
    {
        if (!m_client.actionContextForResultAtPoint)
            return WTF::nullopt;

        WKBundleRangeHandleRef apiRange = nullptr;
        auto actionContext = (DDActionContext *)m_client.actionContextForResultAtPoint(toAPI(&pageOverlay), WKPointMake(location.x(), location.y()), &apiRange, m_client.base.clientInfo);
        if (!actionContext || !apiRange)
            return WTF::nullopt;

        return { { actionContext, makeSimpleRange(WebKit::toImpl(apiRange)->coreRange()) } };
    }

    void dataDetectorsDidPresentUI(WebKit::WebPageOverlay& pageOverlay) override
    {
        if (!m_client.dataDetectorsDidPresentUI)
            return;

        m_client.dataDetectorsDidPresentUI(toAPI(&pageOverlay), m_client.base.clientInfo);
    }

    void dataDetectorsDidChangeUI(WebKit::WebPageOverlay& pageOverlay) override
    {
        if (!m_client.dataDetectorsDidChangeUI)
            return;

        m_client.dataDetectorsDidChangeUI(toAPI(&pageOverlay), m_client.base.clientInfo);
    }

    void dataDetectorsDidHideUI(WebKit::WebPageOverlay& pageOverlay) override
    {
        if (!m_client.dataDetectorsDidHideUI)
            return;

        m_client.dataDetectorsDidHideUI(toAPI(&pageOverlay), m_client.base.clientInfo);
    }
#endif // PLATFORM(MAC)

    bool copyAccessibilityAttributeStringValueForPoint(WebKit::WebPageOverlay& pageOverlay, String attribute, WebCore::FloatPoint parameter, String& value) override
    {
        if (!m_accessibilityClient.client().copyAccessibilityAttributeValue)
            return false;
        auto wkType = m_accessibilityClient.client().copyAccessibilityAttributeValue(toAPI(&pageOverlay), WebKit::toCopiedAPI(attribute), WKPointCreate(WKPointMake(parameter.x(), parameter.y())), m_accessibilityClient.client().base.clientInfo);
        if (WebKit::toImpl(wkType)->type() != API::String::APIType)
            return false;
        value = WebKit::toWTFString(static_cast<WKStringRef>(wkType));
        return true;
    }

    bool copyAccessibilityAttributeBoolValueForPoint(WebKit::WebPageOverlay& pageOverlay, String attribute, WebCore::FloatPoint parameter, bool& value) override
    {
        if (!m_accessibilityClient.client().copyAccessibilityAttributeValue)
            return false;
        auto wkType = m_accessibilityClient.client().copyAccessibilityAttributeValue(toAPI(&pageOverlay), WebKit::toCopiedAPI(attribute), WKPointCreate(WKPointMake(parameter.x(), parameter.y())), m_accessibilityClient.client().base.clientInfo);
        if (WebKit::toImpl(wkType)->type() != API::Boolean::APIType)
            return false;
        value = WKBooleanGetValue(static_cast<WKBooleanRef>(wkType));
        return true;
    }

    Vector<String> copyAccessibilityAttributeNames(WebKit::WebPageOverlay& pageOverlay, bool paramerizedNames) override
    {
        Vector<String> names;
        if (!m_accessibilityClient.client().copyAccessibilityAttributeNames)
            return names;
        auto wkNames = m_accessibilityClient.client().copyAccessibilityAttributeNames(toAPI(&pageOverlay), paramerizedNames, m_accessibilityClient.client().base.clientInfo);

        size_t count = WKArrayGetSize(wkNames);
        for (size_t k = 0; k < count; k++) {
            WKTypeRef item = WKArrayGetItemAtIndex(wkNames, k);
            if (WebKit::toImpl(item)->type() == API::String::APIType)
                names.append(WebKit::toWTFString(static_cast<WKStringRef>(item)));
        }

        return names;
    }
    
    API::Client<WKBundlePageOverlayAccessibilityClientBase> m_accessibilityClient;
};

WKTypeID WKBundlePageOverlayGetTypeID()
{
    return WebKit::toAPI(WebKit::WebPageOverlay::APIType);
}

WKBundlePageOverlayRef WKBundlePageOverlayCreate(WKBundlePageOverlayClientBase* wkClient)
{
    auto clientImpl = makeUnique<PageOverlayClientImpl>(wkClient);
    return toAPI(&WebKit::WebPageOverlay::create(WTFMove(clientImpl)).leakRef());
}

void WKBundlePageOverlaySetAccessibilityClient(WKBundlePageOverlayRef bundlePageOverlayRef, WKBundlePageOverlayAccessibilityClientBase* client)
{
    static_cast<PageOverlayClientImpl&>(WebKit::toImpl(bundlePageOverlayRef)->client()).setAccessibilityClient(client);
}

void WKBundlePageOverlaySetNeedsDisplay(WKBundlePageOverlayRef bundlePageOverlayRef, WKRect rect)
{
    WebKit::toImpl(bundlePageOverlayRef)->setNeedsDisplay(enclosingIntRect(WebKit::toFloatRect(rect)));
}

float WKBundlePageOverlayFractionFadedIn(WKBundlePageOverlayRef)
{
    // Clients who include the fade opacity during painting interfere
    // with composited fade, so we'll pretend we're opaque and do the
    // fade on our own.

    return 1;
}

void WKBundlePageOverlayClear(WKBundlePageOverlayRef bundlePageOverlayRef)
{
    WebKit::toImpl(bundlePageOverlayRef)->clear();
}
