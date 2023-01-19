/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "LocalNetworkAccess.h"

#include "SecurityContext.h"
#include "SecurityOrigin.h"

namespace WebCore {

AddressSpace addressSpaceFromURL(const URL& url)
{
    if (SecurityOrigin::isLocalHostOrLoopbackIPAddress(url.host()))
        return AddressSpace::Local;

    // FIXME (https://webkit.org/b/250339): add support for checking for IPv6 addresses in private address space.
    if (SecurityOrigin::isLocalNetworkIPv4Address(url.host()))
        return AddressSpace::Private;

    return AddressSpace::Public;
}

bool isLocalNetworkAccessSecureContextRestricted(const SecurityContext& securityContext, AddressSpace requestAddressSpace)
{
    if (requestAddressSpace == AddressSpace::Public)
        return false;

    if (securityContext.addressSpace() != AddressSpace::Public)
        return false;

    // FIXME (https://webkit.org/b/250418): We create documents with a top-level data URL as non-secure
    // contexts contrary to the HTML spec. The SecurityOrigin::isSecure check exists until that is resolved.
    if (securityContext.isSecureContext() || SecurityOrigin::isSecure(securityContext.url()))
        return false;

    if (securityContext.securityOrigin() && securityContext.securityOrigin()->isLocal())
        return false;

    return true;
}

}
