/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
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
 *
 */

#include "config.h"
#include "MediaDevicesRequest.h"

#if ENABLE(MEDIA_STREAM)

#include "CaptureDevice.h"
#include "Document.h"
#include "Frame.h"
#include "JSMediaDeviceInfo.h"
#include "MediaDevicesEnumerationRequest.h"
#include "RealtimeMediaSourceCenter.h"
#include "SecurityOrigin.h"
#include "UserMediaController.h"
#include <wtf/MainThread.h>
#include <wtf/SHA1.h>

namespace WebCore {

inline MediaDevicesRequest::MediaDevicesRequest(Document& document, MediaDevices::EnumerateDevicesPromise&& promise)
    : ContextDestructionObserver(&document)
    , m_promise(WTFMove(promise))
{
}

Ref<MediaDevicesRequest> MediaDevicesRequest::create(Document& document, MediaDevices::EnumerateDevicesPromise&& promise)
{
    return adoptRef(*new MediaDevicesRequest(document, WTFMove(promise)));
}

MediaDevicesRequest::~MediaDevicesRequest()
{
    // This should only get destroyed after the enumeration request has completed or has been canceled.
    ASSERT(!m_enumerationRequest || m_enumerationRequest->wasCanceled());
}

SecurityOrigin* MediaDevicesRequest::securityOrigin() const
{
    if (scriptExecutionContext())
        return scriptExecutionContext()->securityOrigin();

    return nullptr;
}

void MediaDevicesRequest::contextDestroyed()
{
    // The call to m_enumerationRequest->cancel() might delete this.
    auto protectedThis = makeRef(*this);

    if (m_enumerationRequest) {
        m_enumerationRequest->cancel();
        m_enumerationRequest = nullptr;
    }
    ContextDestructionObserver::contextDestroyed();
}

void MediaDevicesRequest::filterDeviceList(Vector<Ref<MediaDeviceInfo>>& devices)
{
#if PLATFORM(IOS_FAMILY)
    static const int defaultCameraCount = 2;
#else
    static const int defaultCameraCount = 1;
#endif

    static const int defaultMicrophoneCount = 1;

    int cameraCount = 0;
    int microphoneCount = 0;
    devices.removeAllMatching([&](const Ref<MediaDeviceInfo>& device) -> bool {
        if (device->kind() == MediaDeviceInfo::Kind::Videoinput && ++cameraCount > defaultCameraCount)
            return true;
        if (device->kind() == MediaDeviceInfo::Kind::Audioinput && ++microphoneCount > defaultMicrophoneCount)
            return true;

        return false;
    });
}

void MediaDevicesRequest::start()
{
    auto& document = downcast<Document>(*scriptExecutionContext());
    auto* controller = UserMediaController::from(document.page());
    if (!controller) {
        callOnMainThread([protectedThis = makeRef(*this)]() {
            protectedThis->m_promise.resolve({ });
        });

        return;
    }

    auto microphoneAccess = controller->canCallGetUserMedia(document, true, false);
    auto cameraAccess = controller->canCallGetUserMedia(document, false, true);
    bool canAccessMicrophone = microphoneAccess == UserMediaController::GetUserMediaAccess::CanCall;
    bool canAccessCamera = cameraAccess == UserMediaController::GetUserMediaAccess::CanCall;
    if (!canAccessMicrophone && !canAccessCamera) {
        controller->logGetUserMediaDenial(document, !canAccessMicrophone ? microphoneAccess : cameraAccess, UserMediaController::BlockedCaller::EnumerateDevices);
        callOnMainThread([protectedThis = makeRef(*this)]() {
            protectedThis->m_promise.resolve({ });
        });

        return;
    }

    // This lambda keeps |this| alive until the request completes or is canceled.
    auto completion = [this, protectedThis = makeRef(*this), canAccessMicrophone, canAccessCamera] (const Vector<CaptureDevice>& captureDevices, const String& deviceIdentifierHashSalt, bool originHasPersistentAccess) mutable {

        m_enumerationRequest = nullptr;

        if (!scriptExecutionContext())
            return;

        auto& document = downcast<Document>(*scriptExecutionContext());
        document.setDeviceIDHashSalt(deviceIdentifierHashSalt);

        Vector<Ref<MediaDeviceInfo>> devices;
        bool revealIdsAndLabels = originHasPersistentAccess || document.hasHadCaptureMediaStreamTrack();
        for (auto& deviceInfo : captureDevices) {
            if (!canAccessMicrophone && deviceInfo.type() == CaptureDevice::DeviceType::Microphone)
                continue;
            if (!canAccessCamera && deviceInfo.type() == CaptureDevice::DeviceType::Camera)
                continue;

            auto label = emptyString();
            auto id = emptyString();
            auto groupId = emptyString();
            if (revealIdsAndLabels) {
                label = deviceInfo.label();
                id = RealtimeMediaSourceCenter::singleton().hashStringWithSalt(deviceInfo.persistentId(), deviceIdentifierHashSalt);
                if (id.isEmpty())
                    continue;
                groupId = RealtimeMediaSourceCenter::singleton().hashStringWithSalt(deviceInfo.groupId(), deviceIdentifierHashSalt);
            }

            auto deviceType = deviceInfo.type() == CaptureDevice::DeviceType::Microphone ? MediaDeviceInfo::Kind::Audioinput : MediaDeviceInfo::Kind::Videoinput;
            devices.append(MediaDeviceInfo::create(scriptExecutionContext(), label, id, groupId, deviceType));
        }

        if (!revealIdsAndLabels)
            filterDeviceList(devices);

        callOnMainThread([protectedThis = makeRef(*this), devices = WTFMove(devices)]() mutable {
            protectedThis->m_promise.resolve(devices);
        });
    };

    m_enumerationRequest = MediaDevicesEnumerationRequest::create(document, WTFMove(completion));
    m_enumerationRequest->start();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
