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
#include "DeviceOrientationAndMotionAccessController.h"
#include "JSDOMPromiseDeferred.h"
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

static std::optional<DeviceMotionEvent::Acceleration> convert(const DeviceMotionData::Acceleration* acceleration)
{
    if (!acceleration)
        return std::nullopt;

    return DeviceMotionEvent::Acceleration { acceleration->x(), acceleration->y(), acceleration->z() };
}

static std::optional<DeviceMotionEvent::RotationRate> convert(const DeviceMotionData::RotationRate* rotationRate)
{
    if (!rotationRate)
        return std::nullopt;

    return DeviceMotionEvent::RotationRate { rotationRate->alpha(), rotationRate->beta(), rotationRate->gamma() };
}

static RefPtr<DeviceMotionData::Acceleration> convert(std::optional<DeviceMotionEvent::Acceleration>&& acceleration)
{
    if (!acceleration)
        return nullptr;

    if (!acceleration->x && !acceleration->y && !acceleration->z)
        return nullptr;

    return DeviceMotionData::Acceleration::create(acceleration->x, acceleration->y, acceleration->z);
}

static RefPtr<DeviceMotionData::RotationRate> convert(std::optional<DeviceMotionEvent::RotationRate>&& rotationRate)
{
    if (!rotationRate)
        return nullptr;

    if (!rotationRate->alpha && !rotationRate->beta && !rotationRate->gamma)
        return nullptr;

    return DeviceMotionData::RotationRate::create(rotationRate->alpha, rotationRate->beta, rotationRate->gamma);
}

std::optional<DeviceMotionEvent::Acceleration> DeviceMotionEvent::acceleration() const
{
    RefPtr acceleration = m_deviceMotionData->acceleration();
    return convert(acceleration.get());
}

std::optional<DeviceMotionEvent::Acceleration> DeviceMotionEvent::accelerationIncludingGravity() const
{
    RefPtr accelerationIncludingGravity = m_deviceMotionData->accelerationIncludingGravity();
    return convert(accelerationIncludingGravity.get());
}

std::optional<DeviceMotionEvent::RotationRate> DeviceMotionEvent::rotationRate() const
{
    RefPtr rotationRate = m_deviceMotionData->rotationRate();
    return convert(rotationRate.get());
}

std::optional<double> DeviceMotionEvent::interval() const
{
    return m_deviceMotionData->interval();
}

void DeviceMotionEvent::initDeviceMotionEvent(const AtomString& type, bool bubbles, bool cancelable, std::optional<DeviceMotionEvent::Acceleration>&& acceleration, std::optional<DeviceMotionEvent::Acceleration>&& accelerationIncludingGravity, std::optional<DeviceMotionEvent::RotationRate>&& rotationRate, std::optional<double> interval)
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

#if ENABLE(DEVICE_ORIENTATION)
void DeviceMotionEvent::requestPermission(Document& document, PermissionPromise&& promise)
{
    RefPtr window = document.domWindow();
    if (!window || !document.page())
        return promise.reject(Exception { ExceptionCode::InvalidStateError, "No browsing context"_s });

    String errorMessage;
    if (!window->isAllowedToUseDeviceMotion(errorMessage)) {
        document.addConsoleMessage(MessageSource::JS, MessageLevel::Warning, makeString("Call to requestPermission() failed, reason: ", errorMessage, "."));
        return promise.resolve(PermissionState::Denied);
    }

    document.deviceOrientationAndMotionAccessController().shouldAllowAccess(document, [promise = WTFMove(promise)](auto permissionState) mutable {
        if (permissionState == PermissionState::Prompt)
            return promise.reject(Exception { ExceptionCode::NotAllowedError, "Requesting device motion access requires a user gesture to prompt"_s });
        promise.resolve(permissionState);
    });
}
#endif

} // namespace WebCore
