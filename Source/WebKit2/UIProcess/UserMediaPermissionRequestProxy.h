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
#include "ImmutableDictionary.h"
#include <wtf/PassRefPtr.h>

namespace WebKit {

class UserMediaPermissionRequestManagerProxy;

class UserMediaPermissionRequestProxy : public API::ObjectImpl<API::Object::Type::UserMediaPermissionRequest> {
public:
    static PassRefPtr<UserMediaPermissionRequestProxy> create(UserMediaPermissionRequestManagerProxy& manager, uint64_t userMediaID, bool audio, bool video)
    {
        return adoptRef(new UserMediaPermissionRequestProxy(manager, userMediaID, audio, video));
    }

    void allow();
    void deny();

    void invalidate();

    PassRefPtr<ImmutableDictionary> mediaParameters() const { return m_mediaParameters; }

private:
    UserMediaPermissionRequestProxy(UserMediaPermissionRequestManagerProxy&, uint64_t userMediaID, bool audio, bool video);

    UserMediaPermissionRequestManagerProxy& m_manager;
    uint64_t m_userMediaID;
    RefPtr<ImmutableDictionary> m_mediaParameters;
};

} // namespace WebKit

#endif // UserMediaPermissionRequestProxy_h
