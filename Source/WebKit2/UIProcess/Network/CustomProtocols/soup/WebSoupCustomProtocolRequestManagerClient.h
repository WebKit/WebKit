/*
 * Copyright (C) 2013 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef WebSoupCustomProtocolRequestManagerClient_h
#define WebSoupCustomProtocolRequestManagerClient_h

#if ENABLE(CUSTOM_PROTOCOLS)

#include "APIClient.h"
#include "WKAPICast.h"
#include "WKSoupCustomProtocolRequestManager.h"

namespace API {

template<> struct ClientTraits<WKSoupCustomProtocolRequestManagerClientBase> {
    typedef std::tuple<WKSoupCustomProtocolRequestManagerClientV0> Versions;
};
}

namespace WebCore {
class ResourceRequest;
}

namespace WebKit {

class WebSoupCustomProtocolRequestManager;

class WebSoupCustomProtocolRequestManagerClient : public API::Client<WKSoupCustomProtocolRequestManagerClientBase> {
public:
    bool startLoading(WebSoupCustomProtocolRequestManager*, uint64_t customProtocolID, const WebCore::ResourceRequest&);
    void stopLoading(WebSoupCustomProtocolRequestManager*, uint64_t customProtocolID);
};

} // namespace WebKit

#endif // ENABLE(CUSTOM_PROTOCOLS)

#endif // WebSoupCustomProtocolRequestManagerClient_h
