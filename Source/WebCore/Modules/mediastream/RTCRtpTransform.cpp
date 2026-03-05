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
#include "RTCRtpTransform.h"

#if ENABLE(WEB_RTC)

#include "RTCRtpReceiver.h"
#include "RTCRtpSender.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RTCRtpTransform);

std::unique_ptr<RTCRtpTransform> RTCRtpTransform::from(std::optional<Internal>&& internal)
{
    if (!internal)
        return nullptr;
    return makeUnique<RTCRtpTransform>(WTF::move(*internal));
}

RTCRtpTransform::RTCRtpTransform(Internal&& transform)
    : m_transform(WTF::move(transform))
{
}

RTCRtpTransform::~RTCRtpTransform()
{
    clearBackend();
}

bool RTCRtpTransform::isAttached() const
{
    return WTF::switchOn(m_transform,
        [&](const Ref<RTCRtpSFrameTransform>& sframeTransform) {
            return sframeTransform->isAttached();
        },
        [&](const Ref<RTCRtpScriptTransform>& scriptTransform) {
            return scriptTransform->isAttached();
        }
    );
}

void RTCRtpTransform::attachToReceiver(RTCRtpReceiver& receiver, RTCRtpTransform* previousTransform)
{
    ASSERT(!isAttached());

    if (previousTransform)
        m_backend = previousTransform->takeBackend();
    else if (auto* backend = receiver.backend())
        m_backend = backend->rtcRtpTransformBackend();

    if (!m_backend)
        return;

    switchOn(m_transform,
        [&](Ref<RTCRtpSFrameTransform>& sframeTransform) {
            sframeTransform->initializeBackendForReceiver(*m_backend);
        },
        [&](Ref<RTCRtpScriptTransform>& scriptTransform) {
            scriptTransform->initializeBackendForReceiver(*m_backend);
        }
    );
}

void RTCRtpTransform::attachToSender(RTCRtpSender& sender, RTCRtpTransform* previousTransform)
{
    ASSERT(!isAttached());

    if (previousTransform)
        m_backend = previousTransform->takeBackend();
    else if (RefPtr backend = sender.backend())
        m_backend = backend->rtcRtpTransformBackend();

    if (!m_backend)
        return;

    switchOn(m_transform,
        [&](Ref<RTCRtpSFrameTransform>& sframeTransform) {
            sframeTransform->initializeBackendForSender(*m_backend);
        },
        [&](Ref<RTCRtpScriptTransform>& scriptTransform) {
            scriptTransform->initializeBackendForSender(*m_backend);
            if (previousTransform)
                previousTransform->backendTransferedToNewTransform();
        }
    );
}

void RTCRtpTransform::backendTransferedToNewTransform()
{
    switchOn(m_transform,
        [&](Ref<RTCRtpSFrameTransform>&) { },
        [&](Ref<RTCRtpScriptTransform>& scriptTransform) {
            scriptTransform->backendTransferedToNewTransform();
        }
    );
}

void RTCRtpTransform::clearBackend()
{
    if (!m_backend)
        return;

    switchOn(m_transform,
        [&](Ref<RTCRtpSFrameTransform>& sframeTransform) {
            sframeTransform->willClearBackend(*m_backend);
        },
        [&](Ref<RTCRtpScriptTransform>& scriptTransform) {
            scriptTransform->willClearBackend(*m_backend);
        }
    );

    m_backend = nullptr;
}

void RTCRtpTransform::detachFromReceiver(RTCRtpReceiver&)
{
    clearBackend();
}

void RTCRtpTransform::detachFromSender(RTCRtpSender&)
{
    clearBackend();
}

bool operator==(const RTCRtpTransform& a, const RTCRtpTransform& b)
{
    return WTF::switchOn(a.m_transform,
        [&](const Ref<RTCRtpSFrameTransform>& sframeTransformA) {
            return WTF::switchOn(b.m_transform,
                [&](const Ref<RTCRtpSFrameTransform>& sframeTransformB) {
                    return sframeTransformA.ptr() == sframeTransformB.ptr();
                },
                [&](const Ref<RTCRtpScriptTransform>&) {
                    return false;
                }
            );
        },
        [&](const Ref<RTCRtpScriptTransform>& scriptTransformA) {
            return WTF::switchOn(b.m_transform,
                [&](const Ref<RTCRtpSFrameTransform>&) {
                    return false;
                },
                [&](const Ref<RTCRtpScriptTransform>& scriptTransformB) {
                    return scriptTransformA.ptr() == scriptTransformB.ptr();
                }
            );
        }
    );
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
