/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "IPAddressSpace.h"
#include "ResourceRequest.h"
#include "SecurityOrigin.h"
#include "SecurityOriginData.h"

namespace WebCore {

std::optional<ResourceError> performLocalNetworkAccessCheck(const ResourceRequest& request, IPAddressSpace connectionAddressSpace, IPAddressSpace clientPolicyContainerAddressSpace, bool clientIsSecureContext, const SecurityOriginData& clientOrigin, NOESCAPE const LocalNetworkAccessPermissionCheckFunction& permissionCheck)
{
    auto requestOrigin = SecurityOriginData::fromURL(request.url());
    if (requestOrigin == clientOrigin) {
        if (clientOrigin.securityOrigin()->isPotentiallyTrustworthy())
            return std::nullopt;
    }

    if (connectionAddressSpace != IPAddressSpace::Unknown) {
        if (request.targetAddressSpace() != IPAddressSpace::Public && request.targetAddressSpace() != connectionAddressSpace) {
            return ResourceError { "WebKitErrorDomain"_s, 0, request.url(),
                "Local Network Access: connection's resolved address space does not match the request's declared targetAddressSpace"_s,
                ResourceError::Type::AccessControl };
        }

        if (!isLessPublicThan(connectionAddressSpace, clientPolicyContainerAddressSpace))
            return std::nullopt;
    }

    auto error = ResourceError { "WebKitErrorDomain"_s, 0, request.url(),
        "Local Network Access: connection to a less public address space than the requesting document was blocked"_s,
        ResourceError::Type::AccessControl };

    if (!clientIsSecureContext)
        return error;

    switch (permissionCheck(clientOrigin, connectionAddressSpace)) {
    case LocalNetworkAccessPermissionDecision::Granted:
        return std::nullopt;
    case LocalNetworkAccessPermissionDecision::Denied:
    case LocalNetworkAccessPermissionDecision::Prompt:
        return error;
    }
    return error;
}

} // namespace WebCore
