/*
    Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2012 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef WebFrameNetworkingContext_h
#define WebFrameNetworkingContext_h

#include <WebCore/FrameNetworkingContext.h>
#include <WebCore/ResourceError.h>

class WebFrameNetworkingContext : public WebCore::FrameNetworkingContext {
public:
    static PassRefPtr<WebFrameNetworkingContext> create(WebCore::Frame*, const WTF::String& userAgent);

    static void setPrivateBrowsingStorageSessionIdentifierBase(const String&);
    static void ensurePrivateBrowsingSession();
    static void destroyPrivateBrowsingSession();

#if USE(CFNETWORK)
    static void setCookieAcceptPolicyForAllContexts(CFHTTPCookieStorageAcceptPolicy);
#endif

private:
    WebFrameNetworkingContext(WebCore::Frame* frame, const WTF::String& userAgent)
        : WebCore::FrameNetworkingContext(frame)
        , m_userAgent(userAgent)
    {
    }

    virtual WTF::String userAgent() const;
    virtual WTF::String referrer() const;
#if USE(CFNETWORK)
    virtual WebCore::NetworkStorageSession& storageSession() const;
#endif
    virtual WebCore::ResourceError blockedError(const WebCore::ResourceRequest&) const;

    WTF::String m_userAgent;
};

#endif // WebFrameNetworkingContext_h
