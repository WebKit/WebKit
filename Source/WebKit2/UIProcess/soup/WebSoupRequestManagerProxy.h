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

#ifndef WebSoupRequestManagerProxy_h
#define WebSoupRequestManagerProxy_h

#include "APIObject.h"
#include "WebSoupRequestManagerClient.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace CoreIPC {
class ArgumentDecoder;
class Connection;
class MessageID;
}

namespace WebKit {

class WebContext;
class WebData;

class WebSoupRequestManagerProxy : public APIObject {
public:
    static const Type APIType = TypeSoupRequestManager;

    static PassRefPtr<WebSoupRequestManagerProxy> create(WebContext*);
    virtual ~WebSoupRequestManagerProxy();

    void invalidate();
    void clearContext() { m_webContext = 0; }

    void initializeClient(const WKSoupRequestManagerClient*);

    void registerURIScheme(const String& scheme);
    void didHandleURIRequest(const WebData*, uint64_t contentLength, const String& mimeType, uint64_t requestID);
    void didReceiveURIRequestData(const WebData*, uint64_t requestID);

    void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);

private:
    WebSoupRequestManagerProxy(WebContext*);

    virtual Type type() const { return APIType; }

    void didReceiveURIRequest(const String& uriString, uint64_t requestID);

    void didReceiveWebSoupRequestManagerProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);

    WebContext* m_webContext;
    WebSoupRequestManagerClient m_client;
};

} // namespace WebKit

#endif // WebSoupRequestManagerProxy_h
