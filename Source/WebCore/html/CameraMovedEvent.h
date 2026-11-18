/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(VIDEO)

#include "Event.h"

namespace WebCore {

class CameraMovedEvent final : public Event {
    WTF_MAKE_TZONE_ALLOCATED(CameraMovedEvent);
public:
    virtual ~CameraMovedEvent();

    static Ref<CameraMovedEvent> create(const AtomString& type, double yaw, double pitch, double fieldOfView)
    {
        return adoptRef(*new CameraMovedEvent(type, yaw, pitch, fieldOfView));
    }

    struct Init : public EventInit {
        double yaw { 0 };
        double pitch { 0 };
        double fieldOfView { 0 };
    };

    static Ref<CameraMovedEvent> create(const AtomString& type, Init&& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new CameraMovedEvent(type, WTF::move(initializer), isTrusted));
    }

    double yaw() const { return m_yaw; }
    double pitch() const { return m_pitch; }
    double fieldOfView() const { return m_fieldOfView; }

private:
    CameraMovedEvent(const AtomString& type, double yaw, double pitch, double fieldOfView);
    CameraMovedEvent(const AtomString& type, Init&& initializer, IsTrusted);

    double m_yaw { 0 };
    double m_pitch { 0 };
    double m_fieldOfView { 0 };
};

} // namespace WebCore

#endif // ENABLE(VIDEO)
