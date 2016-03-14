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

#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "JSMediaDeviceInfo.h"
#include "RealtimeMediaSourceCenter.h"
#include "SecurityOrigin.h"
#include "UserMediaController.h"
#include <wtf/MainThread.h>
#include <wtf/SHA1.h>

namespace WebCore {

RefPtr<MediaDevicesRequest> MediaDevicesRequest::create(Document* document, MediaDevices::EnumerateDevicesPromise&& promise, ExceptionCode&)
{
    return adoptRef(*new MediaDevicesRequest(document, WTFMove(promise)));
}

MediaDevicesRequest::MediaDevicesRequest(ScriptExecutionContext* context, MediaDevices::EnumerateDevicesPromise&& promise)
    : ContextDestructionObserver(context)
    , m_promise(WTFMove(promise))
{
}

MediaDevicesRequest::~MediaDevicesRequest()
{
    if (m_permissionCheck)
        m_permissionCheck->setClient(nullptr);
}

SecurityOrigin* MediaDevicesRequest::securityOrigin() const
{
    if (scriptExecutionContext())
        return scriptExecutionContext()->securityOrigin();

    return nullptr;
}

void MediaDevicesRequest::contextDestroyed()
{
    ContextDestructionObserver::contextDestroyed();
    if (m_permissionCheck) {
        m_permissionCheck->setClient(nullptr);
        m_permissionCheck = nullptr;
    }
    m_protector = nullptr;
}

void MediaDevicesRequest::start()
{
    m_protector = this;
    m_permissionCheck = UserMediaPermissionCheck::create(*downcast<Document>(scriptExecutionContext()), *this);
    m_permissionCheck->start();
}

void MediaDevicesRequest::didCompletePermissionCheck(const String& salt, bool canAccess)
{
    RefPtr<UserMediaPermissionCheck> permissionCheckProtector = m_permissionCheck;
    m_permissionCheck->setClient(nullptr);
    m_permissionCheck = nullptr;

    m_idHashSalt = salt;
    m_havePersistentPermission = canAccess;

    callOnMainThread([this, permissionCheckProtector] {
        RealtimeMediaSourceCenter::singleton().getMediaStreamTrackSources(this);
    });
}

static void hashString(SHA1& sha1, const String& string)
{
    if (string.isEmpty())
        return;

    if (string.is8Bit() && string.containsOnlyASCII()) {
        const uint8_t nullByte = 0;
        sha1.addBytes(string.characters8(), string.length());
        sha1.addBytes(&nullByte, 1);
        return;
    }

    auto utf8 = string.utf8();
    sha1.addBytes(reinterpret_cast<const uint8_t*>(utf8.data()), utf8.length() + 1); // Include terminating null byte.
}

String MediaDevicesRequest::hashID(const String& id)
{
    if (id.isEmpty() || m_idHashSalt.isEmpty())
        return emptyString();

    SHA1 sha1;

    hashString(sha1, id);
    hashString(sha1, m_idHashSalt);

    SHA1::Digest digest;
    sha1.computeHash(digest);

    return SHA1::hexDigest(digest).data();
}

void MediaDevicesRequest::didCompleteTrackSourceInfoRequest(const TrackSourceInfoVector& captureDevices)
{
    if (!scriptExecutionContext()) {
        m_protector = nullptr;
        return;
    }

    Document& document = downcast<Document>(*scriptExecutionContext());
    UserMediaController* controller = UserMediaController::from(document.page());
    if (!controller) {
        m_protector = nullptr;
        return;
    }

    Vector<RefPtr<MediaDeviceInfo>> devices;
    for (auto deviceInfo : captureDevices) {
        String deviceType = deviceInfo->kind() == TrackSourceInfo::SourceKind::Audio ? MediaDeviceInfo::audioInputType() : MediaDeviceInfo::videoInputType();
        AtomicString label = emptyAtom;
        if (m_havePersistentPermission || document.hasHadActiveMediaStreamTrack())
            label = deviceInfo->label();

        String id = hashID(deviceInfo->persistentId());
        if (id.isEmpty())
            continue;

        String groupId = hashID(deviceInfo->groupId());

        devices.append(MediaDeviceInfo::create(scriptExecutionContext(), label, id, groupId, deviceType));
    }

    RefPtr<MediaDevicesRequest> protectedThis(this);
    callOnMainThread([protectedThis, devices] {
        protectedThis->m_promise.resolve(devices);
    });
    m_protector = nullptr;

}

const String& MediaDevicesRequest::requestOrigin() const
{
    if (scriptExecutionContext()) {
        Document* document = downcast<Document>(scriptExecutionContext());
        if (document)
            return document->url();
    }

    return emptyString();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
