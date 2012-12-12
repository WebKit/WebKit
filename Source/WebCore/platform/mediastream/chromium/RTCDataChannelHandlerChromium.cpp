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

#if ENABLE(MEDIA_STREAM)

#include "RTCDataChannelHandlerChromium.h"

#include "RTCDataChannelHandlerClient.h"
#include <public/WebRTCDataChannelHandler.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

PassOwnPtr<RTCDataChannelHandler> RTCDataChannelHandlerChromium::create(WebKit::WebRTCDataChannelHandler* webHandler)
{
    return adoptPtr(new RTCDataChannelHandlerChromium(webHandler));
}

RTCDataChannelHandlerChromium::RTCDataChannelHandlerChromium(WebKit::WebRTCDataChannelHandler* webHandler)
    : m_webHandler(adoptPtr(webHandler))
    , m_client(0)
{
}

RTCDataChannelHandlerChromium::~RTCDataChannelHandlerChromium()
{
}

void RTCDataChannelHandlerChromium::setClient(RTCDataChannelHandlerClient* client)
{
    m_client = client;
    m_webHandler->setClient(m_client ? this : 0);
}

String RTCDataChannelHandlerChromium::label()
{
    return m_webHandler->label();
}

bool RTCDataChannelHandlerChromium::isReliable()
{
    return m_webHandler->isReliable();
}

unsigned long RTCDataChannelHandlerChromium::bufferedAmount()
{
    return m_webHandler->bufferedAmount();
}

bool RTCDataChannelHandlerChromium::sendStringData(const String& data)
{
    return m_webHandler->sendStringData(data);
}

bool RTCDataChannelHandlerChromium::sendRawData(const char* data, size_t size)
{
    return m_webHandler->sendRawData(data, size);
}

void RTCDataChannelHandlerChromium::close()
{
    m_webHandler->close();
}

void RTCDataChannelHandlerChromium::didChangeReadyState(WebRTCDataChannelHandlerClient::ReadyState state) const
{
    if (m_client)
        m_client->didChangeReadyState(static_cast<RTCDataChannelHandlerClient::ReadyState>(state));
}

void RTCDataChannelHandlerChromium::didReceiveStringData(const WebKit::WebString& data) const
{
    if (m_client)
        m_client->didReceiveStringData(data);
}

void RTCDataChannelHandlerChromium::didReceiveRawData(const char* data, size_t size) const
{
    if (m_client)
        m_client->didReceiveRawData(data, size);
}

void RTCDataChannelHandlerChromium::didDetectError() const
{
    if (m_client)
        m_client->didDetectError();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
