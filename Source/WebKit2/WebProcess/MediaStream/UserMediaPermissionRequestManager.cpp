/*
 * Copyright (C) 2014 Igalia S.L.
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

#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/SecurityOrigin.h>

using namespace WebCore;

namespace WebKit {

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
    Document* document = downcast<Document>(request.scriptExecutionContext());
    Frame* frame = document ? document->frame() : nullptr;

    if (!frame) {
        request.userMediaAccessDenied();
        return;
    }

    uint64_t requestID = generateRequestID();
    m_idToUserMediaRequestMap.add(requestID, &request);
    m_userMediaRequestToIDMap.add(&request, requestID);

    WebFrame* webFrame = WebFrame::fromCoreFrame(*frame);
    ASSERT(webFrame);

    SecurityOrigin* origin = request.securityOrigin();
    m_page.send(Messages::WebPageProxy::RequestUserMediaPermissionForFrame(requestID, webFrame->frameID(), origin->databaseIdentifier(), request.audioDeviceUIDs(), request.videoDeviceUIDs()));
}

void UserMediaPermissionRequestManager::cancelUserMediaRequest(UserMediaRequest& request)
{
    uint64_t requestID = m_userMediaRequestToIDMap.take(&request);
    if (!requestID)
        return;
    m_idToUserMediaRequestMap.remove(requestID);
}

void UserMediaPermissionRequestManager::didReceiveUserMediaPermissionDecision(uint64_t requestID, bool allowed, const String& audioDeviceUID, const String& videoDeviceUID)
{
    RefPtr<UserMediaRequest> request = m_idToUserMediaRequestMap.take(requestID);
    if (!request)
        return;
    m_userMediaRequestToIDMap.remove(request);

    if (allowed)
        request->userMediaAccessGranted(audioDeviceUID, videoDeviceUID);
    else
        request->userMediaAccessDenied();
}

void UserMediaPermissionRequestManager::startUserMediaPermissionCheck(WebCore::UserMediaPermissionCheck& request)
{
    Document* document = downcast<Document>(request.scriptExecutionContext());
    Frame* frame = document ? document->frame() : nullptr;

    if (!frame) {
        request.setHasPersistentPermission(false);
        return;
    }

    uint64_t requestID = generateRequestID();
    m_idToUserMediaPermissionCheckMap.add(requestID, &request);
    m_userMediaPermissionCheckToIDMap.add(&request, requestID);

    WebFrame* webFrame = WebFrame::fromCoreFrame(*frame);
    ASSERT(webFrame);

    SecurityOrigin* origin = request.securityOrigin();
    m_page.send(Messages::WebPageProxy::CheckUserMediaPermissionForFrame(requestID, webFrame->frameID(), origin->databaseIdentifier()));
}

void UserMediaPermissionRequestManager::cancelUserMediaPermissionCheck(WebCore::UserMediaPermissionCheck& request)
{
    uint64_t requestID = m_userMediaPermissionCheckToIDMap.take(&request);
    if (!requestID)
        return;
    m_idToUserMediaPermissionCheckMap.remove(requestID);
}

void UserMediaPermissionRequestManager::didCompleteUserMediaPermissionCheck(uint64_t requestID, bool allowed)
{
    RefPtr<UserMediaPermissionCheck> request = m_idToUserMediaPermissionCheckMap.take(requestID);
    if (!request)
        return;
    m_userMediaPermissionCheckToIDMap.remove(request);
    
    request->setHasPersistentPermission(allowed);
}

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)
