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
#include "UserMediaPermissionRequestProxy.h"

#include "UserMediaPermissionRequestManagerProxy.h"
#include <WebCore/MediaStreamTrackSourcesRequestClient.h>
#include <WebCore/RealtimeMediaSourceCenter.h>
#include <wtf/text/StringHash.h>

namespace WebKit {

UserMediaPermissionRequestProxy::UserMediaPermissionRequestProxy(UserMediaPermissionRequestManagerProxy& manager, uint64_t userMediaID, bool requiresAudio, bool requiresVideo, const Vector<String>& deviceUIDsVideo, const Vector<String>& deviceUIDsAudio)
    : m_manager(manager)
    , m_userMediaID(userMediaID)
    , m_requiresAudio(requiresAudio)
    , m_requiresVideo(requiresVideo)
    , m_videoDeviceUIDs(deviceUIDsVideo)
    , m_audiodeviceUIDs(deviceUIDsAudio)
{
}

void UserMediaPermissionRequestProxy::allow(const String& videoDeviceUID, const String& audioDeviceUID)
{
    m_manager.didReceiveUserMediaPermissionDecision(m_userMediaID, true, videoDeviceUID, audioDeviceUID);
}

void UserMediaPermissionRequestProxy::deny()
{
    m_manager.didReceiveUserMediaPermissionDecision(m_userMediaID, false, emptyString(), emptyString());
}

void UserMediaPermissionRequestProxy::invalidate()
{
    m_manager.invalidateRequests();
}

#if ENABLE(MEDIA_STREAM)
const String& UserMediaPermissionRequestProxy::getDeviceNameForUID(const String& UID, WebCore::RealtimeMediaSource::Type type)
{
    return WebCore::RealtimeMediaSourceCenter::singleton().sourceWithUID(UID, type, nullptr)->label();
}
#endif

} // namespace WebKit

