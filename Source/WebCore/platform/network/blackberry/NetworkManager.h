/*
 * Copyright (C) 2009, 2010 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef NetworkManager_h
#define NetworkManager_h

#include "KURL.h"
#include "ResourceHandle.h"

#include <wtf/Vector.h>

namespace BlackBerry {
namespace Platform {
class FilterStream;
class NetworkStreamFactory;
}
}

namespace WebCore {

class Frame;
class NetworkJob;

class NetworkManager {
public:
    static NetworkManager* instance();
    void setInitialURL(const KURL& url) { m_initialURL = url; }
    KURL initialURL() { return m_initialURL; }
    bool startJob(int playerId, PassRefPtr<ResourceHandle> job, const Frame&, bool defersLoading);
    bool startJob(int playerId, PassRefPtr<ResourceHandle> job, const ResourceRequest&, const Frame&, bool defersLoading);
    bool stopJob(PassRefPtr<ResourceHandle>);
    void setDefersLoading(PassRefPtr<ResourceHandle> job, bool defersLoading);
    void pauseLoad(PassRefPtr<ResourceHandle> job, bool pause);
    BlackBerry::Platform::FilterStream* streamForHandle(PassRefPtr<ResourceHandle>);

private:
    friend class NetworkJob;

    NetworkJob* findJobForHandle(PassRefPtr<ResourceHandle>);
    void deleteJob(NetworkJob*);
    bool startJob(int playerId, const String& pageGroupName, PassRefPtr<ResourceHandle>, const ResourceRequest&, BlackBerry::Platform::NetworkStreamFactory*, const Frame&, int deferLoadingCount = 0, int redirectCount = 0);

    Vector<NetworkJob*> m_jobs;
    KURL m_initialURL;
};

} // namespace WebCore

#endif // NetworkManager_h
