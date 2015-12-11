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

#ifndef UserMediaPermissionRequestManagerProxy_h
#define UserMediaPermissionRequestManagerProxy_h

#include "UserMediaPermissionCheckProxy.h"
#include "UserMediaPermissionRequestProxy.h"
#include <wtf/HashMap.h>

namespace WebKit {

class WebPageProxy;

class UserMediaPermissionRequestManagerProxy {
public:
    explicit UserMediaPermissionRequestManagerProxy(WebPageProxy&);

    void invalidateRequests();

    Ref<UserMediaPermissionRequestProxy> createRequest(uint64_t userMediaID, const Vector<String>& audioDeviceUIDs, const Vector<String>& videoDeviceUIDs);
    void didReceiveUserMediaPermissionDecision(uint64_t, bool allow, const String& audioDeviceUID, const String& videoDeviceUID);


    Ref<UserMediaPermissionCheckProxy> createUserMediaPermissionCheck(uint64_t userMediaID);
    void didCompleteUserMediaPermissionCheck(uint64_t, bool allow);

private:
    HashMap<uint64_t, RefPtr<UserMediaPermissionRequestProxy>> m_pendingUserMediaRequests;
    HashMap<uint64_t, RefPtr<UserMediaPermissionCheckProxy>> m_pendingDeviceRequests;
    WebPageProxy& m_page;
};

} // namespace WebKit

#endif // UserMediaPermissionRequestManagerProxy_h
