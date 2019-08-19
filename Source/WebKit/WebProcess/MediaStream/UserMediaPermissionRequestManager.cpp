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
        document->addMediaCanStartListener(*this);
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
            document.addMediaCanStartListener(*this);
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
            request.document()->removeMediaCanStartListener(*this);
        else
            m_blockedUserMediaRequests.add(document, pendingRequests);
        break;
    }

    m_userMediaRequestToIDMap.remove(&request);
}

void UserMediaPermissionRequestManager::userMediaAccessWasGranted(uint64_t requestID, CaptureDevice&& audioDevice, CaptureDevice&& videoDevice, String&& deviceIdentifierHashSalt, CompletionHandler<void()>&& completionHandler)
{
    auto request = m_idToUserMediaRequestMap.take(requestID);
    if (!request) {
        completionHandler();
        return;
    }
    removeMediaRequestFromMaps(*request);

    request->allow(WTFMove(audioDevice), WTFMove(videoDevice), WTFMove(deviceIdentifierHashSalt), WTFMove(completionHandler));
}

void UserMediaPermissionRequestManager::userMediaAccessWasDenied(uint64_t requestID, WebCore::UserMediaRequest::MediaAccessDenialReason reason, String&& invalidConstraint)
{
    auto request = m_idToUserMediaRequestMap.take(requestID);
    if (!request)
        return;
    removeMediaRequestFromMaps(*request);

    request->deny(reason, WTFMove(invalidConstraint));
}

void UserMediaPermissionRequestManager::enumerateMediaDevices(Document& document, CompletionHandler<void(const Vector<CaptureDevice>&, const String&)>&& completionHandler)
{
    auto* frame = document.frame();
    if (!frame) {
        completionHandler({ }, emptyString());
        return;
    }

    m_page.sendWithAsyncReply(Messages::WebPageProxy::EnumerateMediaDevicesForFrame { WebFrame::fromCoreFrame(*frame)->frameID(), document.securityOrigin().data(), document.topOrigin().data() }, WTFMove(completionHandler));
}

UserMediaClient::DeviceChangeObserverToken UserMediaPermissionRequestManager::addDeviceChangeObserver(WTF::Function<void()>&& observer)
{
    auto identifier = WebCore::UserMediaClient::DeviceChangeObserverToken::generate();
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
