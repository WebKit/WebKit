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

#include "config.h"
#include "DeviceMotionEvent.h"

#include "DeviceMotionData.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DeviceMotionEvent);

DeviceMotionEvent::~DeviceMotionEvent() = default;

DeviceMotionEvent::DeviceMotionEvent()
    : m_deviceMotionData(DeviceMotionData::create())
{
}

DeviceMotionEvent::DeviceMotionEvent(const AtomString& eventType, DeviceMotionData* deviceMotionData)
    : Event(eventType, CanBubble::No, IsCancelable::No)
    , m_deviceMotionData(deviceMotionData)
{
}

static Optional<DeviceMotionEvent::Acceleration> convert(const DeviceMotionData::Acceleration* acceleration)
{
    if (!acceleration)
        return WTF::nullopt;

    return DeviceMotionEvent::Acceleration { acceleration->x(), acceleration->y(), acceleration->z() };
}

static Optional<DeviceMotionEvent::RotationRate> convert(const DeviceMotionData::RotationRate* rotationRate)
{
    if (!rotationRate)
        return WTF::nullopt;

    return DeviceMotionEvent::RotationRate { rotationRate->alpha(), rotationRate->beta(), rotationRate->gamma() };
}

static RefPtr<DeviceMotionData::Acceleration> convert(Optional<DeviceMotionEvent::Acceleration>&& acceleration)
{
    if (!acceleration)
        return nullptr;

    if (!acceleration->x && !acceleration->y && !acceleration->z)
        return nullptr;

    return DeviceMotionData::Acceleration::create(acceleration->x, acceleration->y, acceleration->z);
}

static RefPtr<DeviceMotionData::RotationRate> convert(Optional<DeviceMotionEvent::RotationRate>&& rotationRate)
{
    if (!rotationRate)
        return nullptr;

    if (!rotationRate->alpha && !rotationRate->beta && !rotationRate->gamma)
        return nullptr;

    return DeviceMotionData::RotationRate::create(rotationRate->alpha, rotationRate->beta, rotationRate->gamma);
}

Optional<DeviceMotionEvent::Acceleration> DeviceMotionEvent::acceleration() const
{
    return convert(m_deviceMotionData->acceleration());
}

Optional<DeviceMotionEvent::Acceleration> DeviceMotionEvent::accelerationIncludingGravity() const
{
    return convert(m_deviceMotionData->accelerationIncludingGravity());
}

Optional<DeviceMotionEvent::RotationRate> DeviceMotionEvent::rotationRate() const
{
    return convert(m_deviceMotionData->rotationRate());
}

Optional<double> DeviceMotionEvent::interval() const
{
    return m_deviceMotionData->interval();
}

void DeviceMotionEvent::initDeviceMotionEvent(const AtomString& type, bool bubbles, bool cancelable, Optional<DeviceMotionEvent::Acceleration>&& acceleration, Optional<DeviceMotionEvent::Acceleration>&& accelerationIncludingGravity, Optional<DeviceMotionEvent::RotationRate>&& rotationRate, Optional<double> interval)
{
    if (isBeingDispatched())
        return;

    initEvent(type, bubbles, cancelable);
    m_deviceMotionData = DeviceMotionData::create(convert(WTFMove(acceleration)), convert(WTFMove(accelerationIncludingGravity)), convert(WTFMove(rotationRate)), interval);
}

EventInterface DeviceMotionEvent::eventInterface() const
{
#if ENABLE(DEVICE_ORIENTATION)
    return DeviceMotionEventInterfaceType;
#else
    // FIXME: ENABLE(DEVICE_ORIENTATION) seems to be in a strange state where
    // it is half-guarded by #ifdefs. DeviceMotionEvent.idl is guarded
    // but DeviceMotionEvent.cpp itself is required by ungarded code.
    return EventInterfaceType;
#endif
}

} // namespace WebCore
