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

#include "DeviceOrientationOrMotionPermissionState.h"
#include "Document.h"
#include "Event.h"
#include "IDLTypes.h"

namespace WebCore {

class DeviceMotionData;
template<typename IDLType> class DOMPromiseDeferred;

class DeviceMotionEvent final : public Event {
    WTF_MAKE_ISO_ALLOCATED(DeviceMotionEvent);
public:
    virtual ~DeviceMotionEvent();

    // FIXME: Merge this with DeviceMotionData::Acceleration
    struct Acceleration {
        std::optional<double> x;
        std::optional<double> y;
        std::optional<double> z;
    };

    // FIXME: Merge this with DeviceMotionData::RotationRate
    struct RotationRate {
        std::optional<double> alpha;
        std::optional<double> beta;
        std::optional<double> gamma;
    };

    static Ref<DeviceMotionEvent> create(const AtomString& eventType, DeviceMotionData* deviceMotionData)
    {
        return adoptRef(*new DeviceMotionEvent(eventType, deviceMotionData));
    }

    static Ref<DeviceMotionEvent> createForBindings()
    {
        return adoptRef(*new DeviceMotionEvent);
    }

    std::optional<Acceleration> acceleration() const;
    std::optional<Acceleration> accelerationIncludingGravity() const;
    std::optional<RotationRate> rotationRate() const;
    std::optional<double> interval() const;

    void initDeviceMotionEvent(const AtomString& type, bool bubbles, bool cancelable, std::optional<Acceleration>&&, std::optional<Acceleration>&&, std::optional<RotationRate>&&, std::optional<double>);

#if ENABLE(DEVICE_ORIENTATION)
    using PermissionState = DeviceOrientationOrMotionPermissionState;
    using PermissionPromise = DOMPromiseDeferred<IDLEnumeration<PermissionState>>;
    static void requestPermission(Document&, PermissionPromise&&);
#endif

private:
    DeviceMotionEvent();
    DeviceMotionEvent(const AtomString& eventType, DeviceMotionData*);

    EventInterface eventInterface() const override;

    RefPtr<DeviceMotionData> m_deviceMotionData;
};

} // namespace WebCore
