/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM)

#include "RTCDTMFSenderHandlerChromium.h"

#include "RTCDTMFSenderHandlerClient.h"
#include <public/WebRTCDTMFSenderHandler.h>
#include <wtf/PassOwnPtr.h>

using namespace WebKit;

namespace WebCore {

PassOwnPtr<RTCDTMFSenderHandler> RTCDTMFSenderHandlerChromium::create(WebRTCDTMFSenderHandler* webHandler)
{
    return adoptPtr(new RTCDTMFSenderHandlerChromium(webHandler));
}

RTCDTMFSenderHandlerChromium::RTCDTMFSenderHandlerChromium(WebRTCDTMFSenderHandler* webHandler)
    : m_webHandler(adoptPtr(webHandler))
    , m_client(0)
{
}

RTCDTMFSenderHandlerChromium::~RTCDTMFSenderHandlerChromium()
{
}

void RTCDTMFSenderHandlerChromium::setClient(RTCDTMFSenderHandlerClient* client)
{
    m_client = client;
    m_webHandler->setClient(m_client ? this : 0);
}

String RTCDTMFSenderHandlerChromium::currentToneBuffer()
{
    return m_webHandler->currentToneBuffer();
}

bool RTCDTMFSenderHandlerChromium::canInsertDTMF()
{
    return m_webHandler->canInsertDTMF();
}

bool RTCDTMFSenderHandlerChromium::insertDTMF(const String& tones, long duration, long interToneGap)
{
    return m_webHandler->insertDTMF(tones, duration, interToneGap);
}

void RTCDTMFSenderHandlerChromium::didPlayTone(const WebString& tone) const
{
    if (m_client)
        m_client->didPlayTone(tone);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
