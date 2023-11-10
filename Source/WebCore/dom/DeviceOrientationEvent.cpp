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

#include "config.h"
#include "DeviceOrientationEvent.h"

#include "DeviceOrientationAndMotionAccessController.h"
#include "DeviceOrientationData.h"
#include "Document.h"
#include "JSDOMPromiseDeferred.h"
#include "LocalDOMWindow.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DeviceOrientationEvent);

DeviceOrientationEvent::~DeviceOrientationEvent() = default;

DeviceOrientationEvent::DeviceOrientationEvent()
    : m_orientation(DeviceOrientationData::create())
{
}

DeviceOrientationEvent::DeviceOrientationEvent(const AtomString& eventType, DeviceOrientationData* orientation)
    : Event(eventType, CanBubble::No, IsCancelable::No)
    , m_orientation(orientation)
{
}

std::optional<double> DeviceOrientationEvent::alpha() const
{
    return m_orientation->alpha();
}

std::optional<double> DeviceOrientationEvent::beta() const
{
    return m_orientation->beta();
}

std::optional<double> DeviceOrientationEvent::gamma() const
{
    return m_orientation->gamma();
}

#if PLATFORM(IOS_FAMILY)

std::optional<double> DeviceOrientationEvent::compassHeading() const
{
    return m_orientation->compassHeading();
}

std::optional<double> DeviceOrientationEvent::compassAccuracy() const
{
    return m_orientation->compassAccuracy();
}

void DeviceOrientationEvent::initDeviceOrientationEvent(const AtomString& type, bool bubbles, bool cancelable, std::optional<double> alpha, std::optional<double> beta, std::optional<double> gamma, std::optional<double> compassHeading, std::optional<double> compassAccuracy)
{
    if (isBeingDispatched())
        return;

    initEvent(type, bubbles, cancelable);
    m_orientation = DeviceOrientationData::create(alpha, beta, gamma, compassHeading, compassAccuracy);
}

#else

std::optional<bool> DeviceOrientationEvent::absolute() const
{
    return m_orientation->absolute();
}

void DeviceOrientationEvent::initDeviceOrientationEvent(const AtomString& type, bool bubbles, bool cancelable, std::optional<double> alpha, std::optional<double> beta, std::optional<double> gamma, std::optional<bool> absolute)
{
    if (isBeingDispatched())
        return;

    initEvent(type, bubbles, cancelable);
    m_orientation = DeviceOrientationData::create(alpha, beta, gamma, absolute);
}

#endif

EventInterface DeviceOrientationEvent::eventInterface() const
{
#if ENABLE(DEVICE_ORIENTATION)
    return DeviceOrientationEventInterfaceType;
#else
    // FIXME: ENABLE(DEVICE_ORIENTATION) seems to be in a strange state where
    // it is half-guarded by #ifdefs. DeviceOrientationEvent.idl is guarded
    // but DeviceOrientationEvent.cpp itself is required by ungarded code.
    return EventInterfaceType;
#endif
}

#if ENABLE(DEVICE_ORIENTATION)
void DeviceOrientationEvent::requestPermission(Document& document, PermissionPromise&& promise)
{
    RefPtr window = document.domWindow();
    if (!window || !document.page())
        return promise.reject(Exception { ExceptionCode::InvalidStateError, "No browsing context"_s });

    String errorMessage;
    if (!window->isAllowedToUseDeviceOrientation(errorMessage)) {
        document.addConsoleMessage(MessageSource::JS, MessageLevel::Warning, makeString("Call to requestPermission() failed, reason: ", errorMessage, "."));
        return promise.resolve(PermissionState::Denied);
    }

    document.deviceOrientationAndMotionAccessController().shouldAllowAccess(document, [promise = WTFMove(promise)](PermissionState permissionState) mutable {
        if (permissionState == PermissionState::Prompt)
            return promise.reject(Exception { ExceptionCode::NotAllowedError, "Requesting device orientation access requires a user gesture to prompt"_s });
        promise.resolve(permissionState);
    });
}
#endif

} // namespace WebCore
