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

#ifndef UserMediaPermissionRequestProxy_h
#define UserMediaPermissionRequestProxy_h

#include "APIObject.h"
#include <WebCore/RealtimeMediaSource.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class UserMediaPermissionRequestManagerProxy;

class UserMediaPermissionRequestProxy : public API::ObjectImpl<API::Object::Type::UserMediaPermissionRequest> {
public:
    static PassRefPtr<UserMediaPermissionRequestProxy> create(UserMediaPermissionRequestManagerProxy& manager, uint64_t userMediaID, bool requiresAudio, bool requiresVideo, const Vector<String>& deviceUIDsVideo, const Vector<String>& deviceUIDsAudio)
    {
        return adoptRef(new UserMediaPermissionRequestProxy(manager, userMediaID, requiresAudio, requiresVideo, deviceUIDsVideo, deviceUIDsAudio));
    }

    void allow();
    void deny();

    void invalidate();

#if ENABLE(MEDIA_STREAM)
    const String& getDeviceNameForUID(const String&, WebCore::RealtimeMediaSource::Type);
#endif

    bool requiresAudio() const { return m_requiresAudio; }
    bool requiresVideo() const { return m_requiresVideo; }
    
    const Vector<String>& videoDeviceUIDs() const { return m_videoDeviceUIDs; }
    const Vector<String>& audioDeviceUIDs() const { return m_audiodeviceUIDs; }

private:
    UserMediaPermissionRequestProxy(UserMediaPermissionRequestManagerProxy&, uint64_t userMediaID, bool requiresAudio, bool requiresVideo, const Vector<String>& deviceUIDsVideo, const Vector<String>& deviceUIDsAudio);

    UserMediaPermissionRequestManagerProxy& m_manager;
    uint64_t m_userMediaID;
    bool m_requiresAudio;
    bool m_requiresVideo;
    Vector<String> m_videoDeviceUIDs;
    Vector<String> m_audiodeviceUIDs;
};

} // namespace WebKit

#endif // UserMediaPermissionRequestProxy_h
