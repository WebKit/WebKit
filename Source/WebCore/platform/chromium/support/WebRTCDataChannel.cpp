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

#include <public/WebRTCDataChannel.h>

#include "RTCDataChannelDescriptor.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

using namespace WebCore;

namespace WebKit {

class ExtraDataContainer : public WebCore::RTCDataChannelDescriptor::ExtraData {
public:
    ExtraDataContainer(WebRTCDataChannel::ExtraData* extraData) : m_extraData(WTF::adoptPtr(extraData)) { }

    WebRTCDataChannel::ExtraData* extraData() { return m_extraData.get(); }

private:
    OwnPtr<WebRTCDataChannel::ExtraData> m_extraData;
};

WebRTCDataChannel::WebRTCDataChannel(const PassRefPtr<RTCDataChannelDescriptor>& dataChannel)
    : m_private(dataChannel)
{
}

WebRTCDataChannel::WebRTCDataChannel(RTCDataChannelDescriptor* dataChannel)
    : m_private(dataChannel)
{
}

void WebRTCDataChannel::initialize(const WebString& label, bool reliable)
{
    m_private = RTCDataChannelDescriptor::create(label, reliable);
}

void WebRTCDataChannel::assign(const WebRTCDataChannel& other)
{
    m_private = other.m_private;
}

void WebRTCDataChannel::reset()
{
    m_private.reset();
}

WebRTCDataChannel::operator PassRefPtr<WebCore::RTCDataChannelDescriptor>() const
{
    return m_private.get();
}

WebRTCDataChannel::operator WebCore::RTCDataChannelDescriptor*() const
{
    return m_private.get();
}

WebRTCDataChannel::ExtraData* WebRTCDataChannel::extraData() const
{
    RefPtr<RTCDataChannelDescriptor::ExtraData> data = m_private->extraData();
    if (!data)
        return 0;
    return static_cast<ExtraDataContainer*>(data.get())->extraData();
}

void WebRTCDataChannel::setExtraData(ExtraData* extraData)
{
    m_private->setExtraData(adoptRef(new ExtraDataContainer(extraData)));
}

WebString WebRTCDataChannel::label() const
{
    ASSERT(!isNull());
    return m_private->label();
}

bool WebRTCDataChannel::reliable() const
{
    ASSERT(!isNull());
    return m_private->reliable();
}

void WebRTCDataChannel::setBufferedAmount(unsigned long bufferedAmount) const
{
    ASSERT(!isNull());
    m_private->setBufferedAmount(bufferedAmount);
}

void WebRTCDataChannel::readyStateChanged(ReadyState state) const
{
    ASSERT(!isNull());
    m_private->readyStateChanged(static_cast<RTCDataChannelDescriptor::ReadyState>(state));
}

void WebRTCDataChannel::dataArrived(const WebString& data) const
{
    ASSERT(!isNull());
    m_private->dataArrived(data);
}

void WebRTCDataChannel::dataArrived(const char* data, size_t dataLength) const
{
    ASSERT(!isNull());
    m_private->dataArrived(data, dataLength);
}

void WebRTCDataChannel::error() const
{
    ASSERT(!isNull());
    m_private->error();
}

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)

