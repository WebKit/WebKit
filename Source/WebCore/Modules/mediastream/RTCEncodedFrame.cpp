/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RTCEncodedFrame.h"

#if ENABLE(WEB_RTC)

#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/StrongInlines.h>

namespace WebCore {

RTCEncodedFrame::RTCEncodedFrame(Ref<RTCRtpTransformableFrame>&& frame)
    : m_frame(WTF::move(frame))
{
}

Ref<JSC::ArrayBuffer> RTCEncodedFrame::data() const
{
    if (!m_data)
        m_data = m_isNeutered ? JSC::ArrayBuffer::create(static_cast<size_t>(0U), 1) : JSC::ArrayBuffer::create(m_frame->data());
    return *m_data;
}

void RTCEncodedFrame::setData(JSC::ArrayBuffer& buffer)
{
    m_data = buffer;
}

uint64_t RTCEncodedFrame::timestamp() const
{
    if (!m_timestamp)
        m_timestamp = m_frame->timestamp();
    return *m_timestamp;
}

Ref<RTCRtpTransformableFrame> RTCEncodedFrame::rtcFrame(JSC::VM& vm, ShouldNeuter shouldNeuter)
{
    if (shouldNeuter == ShouldNeuter::Yes && !m_isNeutered) {
        m_isNeutered = true;
        if (m_data) {
            m_frame->setData(m_data->span());

            JSC::ArrayBufferContents emptyBuffer;
            bool result = m_data->transferTo(vm, emptyBuffer);
            ASSERT_UNUSED(result, result);
        }
    }
    return m_frame;
}

Ref<RTCRtpTransformableFrame> RTCEncodedFrame::serialize()
{
    auto clone = m_frame->clone();
    if (m_data)
        clone->setData(m_data->span());
    return clone;
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
