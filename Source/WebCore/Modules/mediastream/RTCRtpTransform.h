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

#pragma once

#if ENABLE(WEB_RTC)

#include "RTCRtpSFrameTransform.h"
#include "RTCRtpScriptTransform.h"
#include "RTCRtpTransformBackend.h"
#include <wtf/FastMalloc.h>

namespace WebCore {

class RTCRtpReceiver;
class RTCRtpSender;

class RTCRtpTransform  {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using Internal = Variant<RefPtr<RTCRtpSFrameTransform>, RefPtr<RTCRtpScriptTransform>>;
    static std::unique_ptr<RTCRtpTransform> from(std::optional<Internal>&&);

    explicit RTCRtpTransform(Internal&&);
    ~RTCRtpTransform();

    bool isAttached() const;
    void attachToReceiver(RTCRtpReceiver&, RTCRtpTransform*);
    void attachToSender(RTCRtpSender&, RTCRtpTransform*);
    void detachFromReceiver(RTCRtpReceiver&);
    void detachFromSender(RTCRtpSender&);

    RefPtr<RTCRtpTransformBackend> takeBackend() { return WTFMove(m_backend); }
    Internal internalTransform() { return m_transform; }

    friend bool operator==(const RTCRtpTransform&, const RTCRtpTransform&);

private:
    void clearBackend();
    void backendTransferedToNewTransform();

    RefPtr<RTCRtpTransformBackend> m_backend;
    Internal m_transform;
};

bool operator==(const RTCRtpTransform&, const RTCRtpTransform&);
inline bool operator!=(const RTCRtpTransform& a, const RTCRtpTransform& b)
{
    return !(a == b);
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
