/*
 * Copyright (C) 2012 Igalia S.L.
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

#ifndef WebSoupRequestManagerClient_h
#define WebSoupRequestManagerClient_h

#include "APIClient.h"
#include "WKAPICast.h"
#include "WKSoupRequestManager.h"

namespace API {
class URL;

template<> struct ClientTraits<WKSoupRequestManagerClientBase> {
    typedef std::tuple<WKSoupRequestManagerClientV0> Versions;
};
}

namespace WebKit {

class WebSoupRequestManagerProxy;

class WebSoupRequestManagerClient : public API::Client<WKSoupRequestManagerClientBase> {
public:
    bool didReceiveURIRequest(WebSoupRequestManagerProxy*, API::URL*, WebPageProxy*, uint64_t requestID);
    void didFailToLoadURIRequest(WebSoupRequestManagerProxy*, uint64_t requestID);
};

} // namespace WebKit

#endif // WebSoupRequestManagerClient_h
