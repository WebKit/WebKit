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
#include "PolicyContainer.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "SecurityContext.h"
#include "SecurityOrigin.h"
#include <wtf/Assertions.h>
#include <wtf/URL.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

std::optional<WebCore::ResourceError> performLocalNetworkAccessCheckWithContext(const ResourceRequest& request, const IPAddressSpace& connectionSpace, const SecurityContext* securityContext)
{
    if (request.hasHTTPOrigin()) {
        auto originString = request.httpOrigin();
        if (!originString.isEmpty()) {
            auto requestOrigin = WebCore::SecurityOrigin::createFromString(originString);
            if (requestOrigin->isPotentiallyTrustworthy()) {
                auto currentURLOrigin = WebCore::SecurityOrigin::create(request.url());
                if (requestOrigin->isSameOriginAs(currentURLOrigin.get()))
                    return std::nullopt;
            }
        }
    }

    if (!securityContext)
        return std::nullopt;

    auto policyContainer = securityContext->policyContainer();
    WebCore::IPAddressSpace requestorIPAddressSpace = policyContainer.ipAddressSpace;

    if (connectionSpace != request.targetAddressSpace()) {
        return WebCore::ResourceError { "WebKitErrorDomain"_s,
            0,
            request.url(),
            "Local Network Access: Connection IP address space does not match target IP address space"_s,
            WebCore::ResourceError::Type::AccessControl };
    }
    // FIXME: When IPAddressSpace value is returned from connection and targetAddressSpace isn't a necessary field to fetch off local network
    // Return null.
    // return std::nullopt;

    // Step 4: If connection's IP address space is less public than request's policy container's IP address space
    bool connectionIsLessPublic = (connectionSpace == WebCore::IPAddressSpace::Local && requestorIPAddressSpace == WebCore::IPAddressSpace::Public);

    if (connectionIsLessPublic) {
        // Let error be a network error.
        WebCore::ResourceError error("WebKitErrorDomain"_s, 0, request.url(), "Local Network Access: Connection to less public address space blocked"_s, WebCore::ResourceError::Type::AccessControl);

        // FIXME: Set error's IP address space property to connection's IP address space.
        if (!securityContext->isSecureContext())
            return error;

        // TODO: Permission check system (not fully implemented yet)
        // If the initiating origin has been granted the local network access permission, return null.
        // If the initiating origin has been denied the local network access permission, return error.
        // Otherwise, prompt the user:
        //   If the user grants permission, return null.
        //   If the user denies the permission, return error.

        // For now, log that permission prompt is needed and return error
        LOG_ERROR("Local Network Access: Permission required but user prompt not implemented for request to %s", request.url().string().utf8().data());
        return error;
    }
    return std::nullopt;
}

} // namespace WebCore
