/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "UserMediaPermissionRequestProxy.h"

#include "UserMediaPermissionRequestManagerProxy.h"
#include <WebCore/RealtimeMediaSourceCenter.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/text/StringHash.h>

using namespace WebCore;

namespace WebKit {

UserMediaPermissionRequestProxy::UserMediaPermissionRequestProxy(UserMediaPermissionRequestManagerProxy& manager, uint64_t userMediaID, uint64_t mainFrameID, uint64_t frameID, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, Vector<String>&& audioDeviceUIDs, Vector<String>&& videoDeviceUIDs, String&& deviceIDHashSalt)
    : m_manager(&manager)
    , m_userMediaID(userMediaID)
    , m_mainFrameID(mainFrameID)
    , m_frameID(frameID)
    , m_userMediaDocumentSecurityOrigin(WTFMove(userMediaDocumentOrigin))
    , m_topLevelDocumentSecurityOrigin(WTFMove(topLevelDocumentOrigin))
    , m_videoDeviceUIDs(WTFMove(videoDeviceUIDs))
    , m_audioDeviceUIDs(WTFMove(audioDeviceUIDs))
    , m_deviceIdentifierHashSalt(WTFMove(deviceIDHashSalt))
{
}

void UserMediaPermissionRequestProxy::allow(const String& audioDeviceUID, const String& videoDeviceUID)
{
    ASSERT(m_manager);
    if (!m_manager)
        return;

    m_manager->userMediaAccessWasGranted(m_userMediaID, audioDeviceUID, videoDeviceUID);
    invalidate();
}

void UserMediaPermissionRequestProxy::deny(UserMediaAccessDenialReason reason)
{
    if (!m_manager)
        return;

    m_manager->userMediaAccessWasDenied(m_userMediaID, reason);
    invalidate();
}

void UserMediaPermissionRequestProxy::invalidate()
{
    m_manager = nullptr;
}

} // namespace WebKit
