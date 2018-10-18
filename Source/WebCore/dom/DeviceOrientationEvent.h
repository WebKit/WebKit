/*
 * Copyright 2010, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Event.h"

namespace WebCore {

class DeviceOrientationData;

class DeviceOrientationEvent final : public Event {
public:
    static Ref<DeviceOrientationEvent> create(const AtomicString& eventType, DeviceOrientationData* orientation)
    {
        return adoptRef(*new DeviceOrientationEvent(eventType, orientation));
    }

    static Ref<DeviceOrientationEvent> createForBindings()
    {
        return adoptRef(*new DeviceOrientationEvent);
    }

    virtual ~DeviceOrientationEvent();

    std::optional<double> alpha() const;
    std::optional<double> beta() const;
    std::optional<double> gamma() const;

#if PLATFORM(IOS_FAMILY)
    std::optional<double> compassHeading() const;
    std::optional<double> compassAccuracy() const;

    void initDeviceOrientationEvent(const AtomicString& type, bool bubbles, bool cancelable, std::optional<double> alpha, std::optional<double> beta, std::optional<double> gamma, std::optional<double> compassHeading, std::optional<double> compassAccuracy);
#else
    std::optional<bool> absolute() const;

    void initDeviceOrientationEvent(const AtomicString& type, bool bubbles, bool cancelable, std::optional<double> alpha, std::optional<double> beta, std::optional<double> gamma, std::optional<bool> absolute);
#endif

private:
    DeviceOrientationEvent();
    DeviceOrientationEvent(const AtomicString& eventType, DeviceOrientationData*);

    EventInterface eventInterface() const override;

    RefPtr<DeviceOrientationData> m_orientation;
};

} // namespace WebCore
