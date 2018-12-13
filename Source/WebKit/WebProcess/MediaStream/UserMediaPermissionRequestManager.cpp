/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "UserMediaPermissionRequestManager.h"

#if ENABLE(MEDIA_STREAM)

#include "Logging.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/CaptureDevice.h>
#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/MediaConstraints.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>

namespace WebKit {
using namespace WebCore;

static constexpr OptionSet<WebCore::ActivityState::Flag> focusedActiveWindow = { WebCore::ActivityState::IsFocused, WebCore::ActivityState::WindowIsActive };

static uint64_t generateRequestID()
{
    static uint64_t uniqueRequestID = 1;
    return uniqueRequestID++;
}

UserMediaPermissionRequestManager::UserMediaPermissionRequestManager(WebPage& page)
    : m_page(page)
{
}

UserMediaPermissionRequestManager::~UserMediaPermissionRequestManager()
{
    clear();
}

void UserMediaPermissionRequestManager::clear()
{
    for (auto& sandboxExtension : m_userMediaDeviceSandboxExtensions)
        sandboxExtension.value->revoke();
}

void UserMediaPermissionRequestManager::startUserMediaRequest(UserMediaRequest& request)
{
    Document* document = request.document();
    Frame* frame = document ? document->frame() : nullptr;

    if (!frame || !document->page()) {
        request.deny(UserMediaRequest::OtherFailure, emptyString());
        return;
    }

    if (document->page()->canStartMedia()) {
        sendUserMediaRequest(request);
        return;
    }

    auto& pendingRequests = m_blockedUserMediaRequests.add(document, Vector<RefPtr<UserMediaRequest>>()).iterator->value;
    if (pendingRequests.isEmpty())
        document->addMediaCanStartListener(this);
    pendingRequests.append(&request);
}

void UserMediaPermissionRequestManager::sendUserMediaRequest(UserMediaRequest& userRequest)
{
    auto* frame = userRequest.document() ? userRequest.document()->frame() : nullptr;
    if (!frame) {
        userRequest.deny(UserMediaRequest::OtherFailure, emptyString());
        return;
    }

    uint64_t requestID = generateRequestID();
    m_idToUserMediaRequestMap.add(requestID, &userRequest);
    m_userMediaRequestToIDMap.add(&userRequest, requestID);

    WebFrame* webFrame = WebFrame::fromCoreFrame(*frame);
    ASSERT(webFrame);

    auto* topLevelDocumentOrigin = userRequest.topLevelDocumentOrigin();
    m_page.send(Messages::WebPageProxy::RequestUserMediaPermissionForFrame(requestID, webFrame->frameID(), userRequest.userMediaDocumentOrigin()->data(), topLevelDocumentOrigin->data(), userRequest.request()));
}

void UserMediaPermissionRequestManager::cancelUserMediaRequest(UserMediaRequest& request)
{
    uint64_t requestID = m_userMediaRequestToIDMap.take(&request);
    if (!requestID)
        return;

    request.deny(UserMediaRequest::OtherFailure, emptyString());
    m_idToUserMediaRequestMap.remove(requestID);
    removeMediaRequestFromMaps(request);
}

void UserMediaPermissionRequestManager::mediaCanStart(Document& document)
{
    auto pendingRequests = m_blockedUserMediaRequests.take(&document);
    while (!pendingRequests.isEmpty()) {
        if (!document.page()->canStartMedia()) {
            m_blockedUserMediaRequests.add(&document, pendingRequests);
            document.addMediaCanStartListener(this);
            break;
        }

        sendUserMediaRequest(*pendingRequests.takeLast());
    }
}

void UserMediaPermissionRequestManager::removeMediaRequestFromMaps(UserMediaRequest& request)
{
    Document* document = request.document();
    if (!document)
        return;

    auto pendingRequests = m_blockedUserMediaRequests.take(document);
    for (auto& pendingRequest : pendingRequests) {
        if (&request != pendingRequest.get())
            continue;

        if (pendingRequests.isEmpty())
            request.document()->removeMediaCanStartListener(this);
        else
            m_blockedUserMediaRequests.add(document, pendingRequests);
        break;
    }

    m_userMediaRequestToIDMap.remove(&request);
}

void UserMediaPermissionRequestManager::userMediaAccessWasGranted(uint64_t requestID, CaptureDevice&& audioDevice, CaptureDevice&& videoDevice, String&& deviceIdentifierHashSalt)
{
    auto request = m_idToUserMediaRequestMap.take(requestID);
    if (!request)
        return;
    removeMediaRequestFromMaps(*request);

    request->allow(WTFMove(audioDevice), WTFMove(videoDevice), WTFMove(deviceIdentifierHashSalt));
}

void UserMediaPermissionRequestManager::userMediaAccessWasDenied(uint64_t requestID, WebCore::UserMediaRequest::MediaAccessDenialReason reason, String&& invalidConstraint)
{
    auto request = m_idToUserMediaRequestMap.take(requestID);
    if (!request)
        return;
    removeMediaRequestFromMaps(*request);

    request->deny(reason, WTFMove(invalidConstraint));
}

void UserMediaPermissionRequestManager::enumerateMediaDevices(MediaDevicesEnumerationRequest& request)
{
    auto* document = downcast<Document>(request.scriptExecutionContext());
    auto* frame = document ? document->frame() : nullptr;

    if (!frame) {
        request.setDeviceInfo(Vector<CaptureDevice>(), emptyString(), false);
        return;
    }

    uint64_t requestID = generateRequestID();
    m_idToMediaDevicesEnumerationRequestMap.add(requestID, &request);
    m_mediaDevicesEnumerationRequestToIDMap.add(&request, requestID);

    WebFrame* webFrame = WebFrame::fromCoreFrame(*frame);
    ASSERT(webFrame);

    SecurityOrigin* topLevelDocumentOrigin = request.topLevelDocumentOrigin();
    ASSERT(topLevelDocumentOrigin);
    m_page.send(Messages::WebPageProxy::EnumerateMediaDevicesForFrame(requestID, webFrame->frameID(), request.userMediaDocumentOrigin()->data(), topLevelDocumentOrigin->data()));
}

void UserMediaPermissionRequestManager::cancelMediaDevicesEnumeration(WebCore::MediaDevicesEnumerationRequest& request)
{
    uint64_t requestID = m_mediaDevicesEnumerationRequestToIDMap.take(&request);
    if (!requestID)
        return;
    request.setDeviceInfo(Vector<CaptureDevice>(), emptyString(), false);
    m_idToMediaDevicesEnumerationRequestMap.remove(requestID);
}

void UserMediaPermissionRequestManager::didCompleteMediaDeviceEnumeration(uint64_t requestID, const Vector<CaptureDevice>& deviceList, String&& mediaDeviceIdentifierHashSalt, bool hasPersistentAccess)
{
    RefPtr<MediaDevicesEnumerationRequest> request = m_idToMediaDevicesEnumerationRequestMap.take(requestID);
    if (!request)
        return;
    m_mediaDevicesEnumerationRequestToIDMap.remove(request);
    
    request->setDeviceInfo(deviceList, WTFMove(mediaDeviceIdentifierHashSalt), hasPersistentAccess);
}

void UserMediaPermissionRequestManager::grantUserMediaDeviceSandboxExtensions(MediaDeviceSandboxExtensions&& extensions)
{
    for (size_t i = 0; i < extensions.size(); i++) {
        const auto& extension = extensions[i];
        extension.second->consume();
        RELEASE_LOG(WebRTC, "UserMediaPermissionRequestManager::grantUserMediaDeviceSandboxExtensions - granted extension %s", extension.first.utf8().data());
        m_userMediaDeviceSandboxExtensions.add(extension.first, extension.second.copyRef());
    }
}

void UserMediaPermissionRequestManager::revokeUserMediaDeviceSandboxExtensions(const Vector<String>& extensionIDs)
{
    for (const auto& extensionID : extensionIDs) {
        auto extension = m_userMediaDeviceSandboxExtensions.take(extensionID);
        if (extension) {
            extension->revoke();
            RELEASE_LOG(WebRTC, "UserMediaPermissionRequestManager::revokeUserMediaDeviceSandboxExtensions - revoked extension %s", extensionID.utf8().data());
        }
    }
}

UserMediaClient::DeviceChangeObserverToken UserMediaPermissionRequestManager::addDeviceChangeObserver(WTF::Function<void()>&& observer)
{
    auto identifier = generateObjectIdentifier<WebCore::UserMediaClient::DeviceChangeObserverTokenType>();
    m_deviceChangeObserverMap.add(identifier, WTFMove(observer));

    if (!m_monitoringDeviceChange) {
        m_monitoringDeviceChange = true;
        m_page.send(Messages::WebPageProxy::BeginMonitoringCaptureDevices());
    }
    return identifier;
}

void UserMediaPermissionRequestManager::removeDeviceChangeObserver(UserMediaClient::DeviceChangeObserverToken token)
{
    bool wasRemoved = m_deviceChangeObserverMap.remove(token);
    ASSERT_UNUSED(wasRemoved, wasRemoved);
}

void UserMediaPermissionRequestManager::captureDevicesChanged()
{
    // When new media input and/or output devices are made available, or any available input and/or
    // output device becomes unavailable, the User Agent MUST run the following steps in browsing
    // contexts where at least one of the following criteria are met, but in no other contexts:

    // * The permission state of the "device-info" permission is "granted",
    // * any of the input devices are attached to an active MediaStream in the browsing context, or
    // * the active document is fully active and has focus.

    auto identifiers = m_deviceChangeObserverMap.keys();
    for (auto& identifier : identifiers) {
        auto iterator = m_deviceChangeObserverMap.find(identifier);
        if (iterator != m_deviceChangeObserverMap.end())
            (iterator->value)();
    }
}

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)
