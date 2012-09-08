/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "WebToCCInputHandlerAdapter.h"

#include <public/WebInputHandlerClient.h>

#define COMPILE_ASSERT_MATCHING_ENUM(webkit_name, webcore_name) \
    COMPILE_ASSERT(int(WebKit::webkit_name) == int(WebCore::webcore_name), mismatching_enums)

COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollStatusOnMainThread, CCInputHandlerClient::ScrollOnMainThread);
COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollStatusStarted, CCInputHandlerClient::ScrollStarted);
COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollStatusIgnored, CCInputHandlerClient::ScrollIgnored);
COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollInputTypeGesture, CCInputHandlerClient::Gesture);
COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollInputTypeWheel, CCInputHandlerClient::Wheel);

namespace WebKit {

PassOwnPtr<WebToCCInputHandlerAdapter> WebToCCInputHandlerAdapter::create(PassOwnPtr<WebInputHandler> handler)
{
    return adoptPtr(new WebToCCInputHandlerAdapter(handler));
}

WebToCCInputHandlerAdapter::WebToCCInputHandlerAdapter(PassOwnPtr<WebInputHandler> handler)
    : m_handler(handler)
{
}

WebToCCInputHandlerAdapter::~WebToCCInputHandlerAdapter()
{
}

class WebToCCInputHandlerAdapter::ClientAdapter : public WebInputHandlerClient {
public:
    ClientAdapter(WebCore::CCInputHandlerClient* client)
        : m_client(client)
    {
    }

    virtual ~ClientAdapter()
    {
    }

    virtual ScrollStatus scrollBegin(WebPoint point, ScrollInputType type) OVERRIDE
    {
        return static_cast<WebInputHandlerClient::ScrollStatus>(m_client->scrollBegin(point, static_cast<WebCore::CCInputHandlerClient::ScrollInputType>(type)));
    }

    virtual void scrollBy(WebPoint point, WebSize offset) OVERRIDE
    {
        m_client->scrollBy(point, offset);
    }

    virtual void scrollEnd() OVERRIDE
    {
        m_client->scrollEnd();
    }

    virtual void pinchGestureBegin() OVERRIDE
    {
        m_client->pinchGestureBegin();
    }

    virtual void pinchGestureUpdate(float magnifyDelta, WebPoint anchor) OVERRIDE
    {
        m_client->pinchGestureUpdate(magnifyDelta, anchor);
    }

    virtual void pinchGestureEnd() OVERRIDE
    {
        m_client->pinchGestureEnd();
    }

    virtual void startPageScaleAnimation(WebSize targetPosition,
                                         bool anchorPoint,
                                         float pageScale,
                                         double startTime,
                                         double duration) OVERRIDE
    {
        m_client->startPageScaleAnimation(targetPosition, anchorPoint, pageScale, startTime, duration);
    }

    virtual void scheduleAnimation() OVERRIDE
    {
        m_client->scheduleAnimation();
    }

private:
    WebCore::CCInputHandlerClient* m_client;
};


void WebToCCInputHandlerAdapter::bindToClient(WebCore::CCInputHandlerClient* client)
{
    m_clientAdapter = adoptPtr(new ClientAdapter(client));
    m_handler->bindToClient(m_clientAdapter.get());
}

void WebToCCInputHandlerAdapter::animate(double monotonicTime)
{
    m_handler->animate(monotonicTime);
}

}

