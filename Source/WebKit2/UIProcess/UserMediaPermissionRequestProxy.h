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
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class UserMediaPermissionRequestManagerProxy;

class UserMediaPermissionRequestProxy : public API::ObjectImpl<API::Object::Type::UserMediaPermissionRequest> {
public:
    static Ref<UserMediaPermissionRequestProxy> create(UserMediaPermissionRequestManagerProxy& manager, uint64_t userMediaID, const Vector<String>& videoDeviceUIDs, const Vector<String>& audioDeviceUIDs)
    {
        return adoptRef(*new UserMediaPermissionRequestProxy(manager, userMediaID, videoDeviceUIDs, audioDeviceUIDs));
    }

    void allow(const String& videoDeviceUID, const String& audioDeviceUID);
    void deny();

    void invalidate();

    bool requiresAudio() const { return m_audioDeviceUIDs.size(); }
    bool requiresVideo() const { return m_videoDeviceUIDs.size(); }

    const Vector<String>& videoDeviceUIDs() const { return m_videoDeviceUIDs; }
    const Vector<String>& audioDeviceUIDs() const { return m_audioDeviceUIDs; }
    
    const String& firstVideoDeviceUID() const { return !videoDeviceUIDs().isEmpty() ? videoDeviceUIDs().at(0) : emptyString(); }
    const String& firstAudioDeviceUID() const { return !audioDeviceUIDs().isEmpty() ? audioDeviceUIDs().at(0) : emptyString(); }

private:
    UserMediaPermissionRequestProxy(UserMediaPermissionRequestManagerProxy&, uint64_t userMediaID, const Vector<String>& videoDeviceUIDs, const Vector<String>& audioDeviceUIDs);

    UserMediaPermissionRequestManagerProxy* m_manager;
    uint64_t m_userMediaID;
    Vector<String> m_videoDeviceUIDs;
    Vector<String> m_audioDeviceUIDs;
};

} // namespace WebKit

#endif // UserMediaPermissionRequestProxy_h
