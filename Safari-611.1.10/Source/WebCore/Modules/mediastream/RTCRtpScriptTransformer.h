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

#include "ActiveDOMObject.h"
#include "ExceptionOr.h"
#include "JSCallbackData.h"
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class MessagePort;
class ScriptExecutionContext;
class RTCRtpTransformBackend;
class SimpleReadableStreamSource;

class RTCRtpScriptTransformer
    : public RefCounted<RTCRtpScriptTransformer>
    , public ActiveDOMObject
    , public CanMakeWeakPtr<RTCRtpScriptTransformer> {
public:
    static ExceptionOr<Ref<RTCRtpScriptTransformer>> create(ScriptExecutionContext&);
    ~RTCRtpScriptTransformer();

    void setCallback(std::unique_ptr<JSCallbackDataStrong>&& callback) { m_callback = WTFMove(callback); }
    MessagePort& port() { return m_port.get(); }

    void start(Ref<RTCRtpTransformBackend>&&);
    void clear();

    void startPendingActivity() { m_pendingActivity = makePendingActivity(*this); }

private:
    RTCRtpScriptTransformer(ScriptExecutionContext&, Ref<MessagePort>&&);

    RefPtr<SimpleReadableStreamSource> startStreams(RTCRtpTransformBackend&);

    // ActiveDOMObject
    const char* activeDOMObjectName() const { return "RTCRtpScriptTransformer"; }
    void stop() final { stopPendingActivity(); }
    void stopPendingActivity() { auto pendingActivity = WTFMove(m_pendingActivity); }

    Ref<MessagePort> m_port;
    std::unique_ptr<JSCallbackDataStrong> m_callback;
    RefPtr<RTCRtpTransformBackend> m_backend;
    RefPtr<PendingActivity<RTCRtpScriptTransformer>> m_pendingActivity;

};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
