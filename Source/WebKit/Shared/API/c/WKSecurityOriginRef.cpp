/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WKSecurityOriginRef.h"

#include "APISecurityOrigin.h"
#include "WKAPICast.h"
#include <WebCore/SecurityOriginData.h>

WKTypeID WKSecurityOriginGetTypeID()
{
    return WebKit::toAPI(API::SecurityOrigin::APIType);
}

WKSecurityOriginRef WKSecurityOriginCreateFromString(WKStringRef string)
{
    return WebKit::toAPI(&API::SecurityOrigin::create(WebCore::SecurityOrigin::createFromString(WebKit::toImpl(string)->string())).leakRef());
}

WKSecurityOriginRef WKSecurityOriginCreateFromDatabaseIdentifier(WKStringRef identifier)
{
    auto origin = WebCore::SecurityOriginData::fromDatabaseIdentifier(WebKit::toImpl(identifier)->string());
    if (!origin)
        return nullptr;
    return WebKit::toAPI(&API::SecurityOrigin::create(origin.value().securityOrigin()).leakRef());
}

WKSecurityOriginRef WKSecurityOriginCreate(WKStringRef protocol, WKStringRef host, int port)
{
    auto securityOrigin = API::SecurityOrigin::create(WebKit::toImpl(protocol)->string(), WebKit::toImpl(host)->string(), port);
    return WebKit::toAPI(&securityOrigin.leakRef());
}

WKStringRef WKSecurityOriginCopyDatabaseIdentifier(WKSecurityOriginRef securityOrigin)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(securityOrigin)->securityOrigin().data().databaseIdentifier());
}

WKStringRef WKSecurityOriginCopyToString(WKSecurityOriginRef securityOrigin)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(securityOrigin)->securityOrigin().toString());
}

WKStringRef WKSecurityOriginCopyProtocol(WKSecurityOriginRef securityOrigin)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(securityOrigin)->securityOrigin().protocol());
}

WKStringRef WKSecurityOriginCopyHost(WKSecurityOriginRef securityOrigin)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(securityOrigin)->securityOrigin().host());
}

unsigned short WKSecurityOriginGetPort(WKSecurityOriginRef securityOrigin)
{
    return WebKit::toImpl(securityOrigin)->securityOrigin().port().valueOr(0);
}
