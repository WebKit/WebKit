/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "MediaDevicesEnumerationRequest.h"

#if ENABLE(MEDIA_STREAM)

#include "Document.h"
#include "JSMediaDeviceInfo.h"
#include "RealtimeMediaSourceCenter.h"
#include "UserMediaController.h"

#if PLATFORM(COCOA)
#define REMOVE_ATYPICAL_DEVICES 1
#else
#define REMOVE_ATYPICAL_DEVICES 0
#endif

#if PLATFORM(COCOA)
const int typicalMicrophoneCount = 1;
#endif

#if PLATFORM(IOS)
const int typicalCameraCount = 2;
#endif

#if PLATFORM(MAC)
const int typicalCameraCount = 1;
#endif

namespace WebCore {

inline MediaDevicesEnumerationRequest::MediaDevicesEnumerationRequest(Document& document, Promise&& promise)
    : ContextDestructionObserver(&document)
    , m_promise(WTFMove(promise))
{
}

void MediaDevicesEnumerationRequest::start(Document& document, Promise&& promise)
{
    auto* page = document.page();
    if (!page) {
        // FIXME: Should we resolve or reject the promise here instead of leaving the website waiting?
        return;
    }

    UserMediaController::from(*page).enumerateMediaDevices(adoptRef(*new MediaDevicesEnumerationRequest(document, WTFMove(promise))));
}

MediaDevicesEnumerationRequest::~MediaDevicesEnumerationRequest()
{
    // We will get here with m_isActive true if the client drops the request without ever calling setDeviceInfo.
    // FIXME: Should we resolve or reject the promise in that case instead of leaving the website waiting?
}

Document* MediaDevicesEnumerationRequest::document()
{
    return m_promise ? downcast<Document>(scriptExecutionContext()) : nullptr;
}

Frame* MediaDevicesEnumerationRequest::frame()
{
    auto* document = this->document();
    return document ? document->frame() : nullptr;
}

SecurityOrigin* MediaDevicesEnumerationRequest::userMediaDocumentOrigin()
{
    auto* document = this->document();
    return document ? &document->securityOrigin() : nullptr;
}

SecurityOrigin* MediaDevicesEnumerationRequest::topLevelDocumentOrigin()
{
    auto* document = this->document();
    return document ? &document->topOrigin() : nullptr;
}

void MediaDevicesEnumerationRequest::contextDestroyed()
{
    // FIXME: Should we be calling UserMediaController::cancelMediaDevicesEnumerationRequest here?
    // If not, then why does that function exist? If we decide that we want to call that cancel
    // function here, then there may be a problem, because it's hard to get from the document to
    // the page to the user media controller while the document is being destroyed.

    m_promise = std::nullopt;
    ContextDestructionObserver::contextDestroyed();
}

#if REMOVE_ATYPICAL_DEVICES

// To reduce the value of media devices for fingerprinting, filter devices that go beyond the typical number.
static inline void removeAtypicalDevices(Vector<RefPtr<MediaDeviceInfo>>& devices)
{
    int cameraCount = 0;
    int microphoneCount = 0;
    devices.removeAllMatching([&cameraCount, &microphoneCount] (const RefPtr<MediaDeviceInfo>& device) {
        if (device->kind() == MediaDeviceInfo::Kind::Videoinput && ++cameraCount > typicalCameraCount)
            return true;
        if (device->kind() == MediaDeviceInfo::Kind::Audioinput && ++microphoneCount > typicalMicrophoneCount)
            return true;
        return false;
    });
}

#endif

void MediaDevicesEnumerationRequest::setDeviceInfo(const Vector<CaptureDevice>& captureDevices, const String& deviceIdentifierHashSalt, bool originHasPersistentAccess)
{
    if (!m_promise)
        return;
    auto promise = WTFMove(m_promise.value());
    ASSERT(!m_promise);

    auto& document = downcast<Document>(*scriptExecutionContext());

    // Policy about including some of the more sensitive information about capture devices.
    bool includeLabels = originHasPersistentAccess || document.hasHadActiveMediaStreamTrack();
#if REMOVE_ATYPICAL_DEVICES
    bool includeAtypicalDevices = includeLabels;
#endif

    Vector<RefPtr<MediaDeviceInfo>> devices;

    document.setDeviceIDHashSalt(deviceIdentifierHashSalt);

    for (auto& captureDevice : captureDevices) {
        auto id = RealtimeMediaSourceCenter::singleton().hashStringWithSalt(captureDevice.persistentId(), deviceIdentifierHashSalt);
        if (id.isEmpty())
            continue;

        auto label = includeLabels ? captureDevice.label() : emptyString();
        auto groupId = RealtimeMediaSourceCenter::singleton().hashStringWithSalt(captureDevice.groupId(), deviceIdentifierHashSalt);
        auto deviceType = captureDevice.type() == CaptureDevice::DeviceType::Audio ? MediaDeviceInfo::Kind::Audioinput : MediaDeviceInfo::Kind::Videoinput;

        devices.append(MediaDeviceInfo::create(label, id, groupId, deviceType));
    }

#if REMOVE_ATYPICAL_DEVICES
    if (!includeAtypicalDevices)
        removeAtypicalDevices(devices);
#endif

    promise.resolve(devices);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
