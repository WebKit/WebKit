/*
 * Copyright (C) 2012 Igalia S.L.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "MessageReceiver.h"
#include "WebContextSupplement.h"
#include "WebSoupRequestManagerClient.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace API {
class Data;
}

namespace WebKit {

class WebContext;

class WebSoupRequestManagerProxy : public API::ObjectImpl<API::Object::Type::SoupRequestManager>, public WebContextSupplement, private IPC::MessageReceiver {
public:
    static const char* supplementName();

    static PassRefPtr<WebSoupRequestManagerProxy> create(WebContext*);
    virtual ~WebSoupRequestManagerProxy();

    void initializeClient(const WKSoupRequestManagerClientBase*);

    void registerURIScheme(const String& scheme);
    void didHandleURIRequest(const API::Data*, uint64_t contentLength, const String& mimeType, uint64_t requestID);
    void didReceiveURIRequestData(const API::Data*, uint64_t requestID);
    void didReceiveURIRequest(const String& uriString, WebPageProxy*, uint64_t requestID);
    void didFailURIRequest(const WebCore::ResourceError&, uint64_t requestID);

    const Vector<String>& registeredURISchemes() const { return m_registeredURISchemes; }

    using API::Object::ref;
    using API::Object::deref;

private:
    WebSoupRequestManagerProxy(WebContext*);

    // WebContextSupplement
    virtual void contextDestroyed() override;
    virtual void processDidClose(WebProcessProxy*) override;
    virtual void refWebContextSupplement() override;
    virtual void derefWebContextSupplement() override;

    // IPC::MessageReceiver
    virtual void didReceiveMessage(IPC::Connection*, IPC::MessageDecoder&) override;

    void didFailToLoadURIRequest(uint64_t requestID);

    WebSoupRequestManagerClient m_client;
    bool m_loadFailed;
    Vector<String> m_registeredURISchemes;
};

} // namespace WebKit

#endif // WebSoupRequestManagerProxy_h
