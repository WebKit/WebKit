/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "ClientOrigin.h"
#include "IPAddressSpace.h"
#include "ResourceRequest.h"
#include "SecurityOrigin.h"
#include "SecurityOriginData.h"

namespace WebCore {

LocalNetworkAccessPermissionRequestOutcome localNetworkAccessPermissionRequestOutcome(IPAddressSpace connectionAddressSpace, bool hasRecordedDecision, bool canPrompt)
{
    if (connectionAddressSpace == IPAddressSpace::Unknown)
        return LocalNetworkAccessPermissionRequestOutcome::RefuseAsUndetermined;

    if (hasRecordedDecision)
        return LocalNetworkAccessPermissionRequestOutcome::UseRecordedDecision;

    if (!canPrompt)
        return LocalNetworkAccessPermissionRequestOutcome::RefuseAsUnpromptable;

    return LocalNetworkAccessPermissionRequestOutcome::Prompt;
}

void performLocalNetworkAccessCheck(const ResourceRequest& request, const URL& currentURL, IPAddressSpace connectionAddressSpace, IPAddressSpace clientPolicyContainerAddressSpace, bool clientIsSecureContext, const ClientOrigin& clientOrigin, bool localNetworkAllowedByPermissionsPolicy, bool loopbackNetworkAllowedByPermissionsPolicy, const LocalNetworkAccessPermissionCheckFunction& permissionCheck, CompletionHandler<void(std::optional<ResourceError>)>&& completionHandler)
{
    if (shouldTreatAsPotentiallyTrustworthy(currentURL) && SecurityOriginData::fromURL(currentURL) == clientOrigin.clientOrigin)
        return completionHandler(std::nullopt);

    // publicnessRank() ranks Unknown alongside Loopback, so leaving it would make every target
    // not-less-public and skip the check entirely.
    if (clientPolicyContainerAddressSpace == IPAddressSpace::Unknown)
        clientPolicyContainerAddressSpace = IPAddressSpace::Public;

    if (connectionAddressSpace != IPAddressSpace::Unknown) {
        if (request.targetAddressSpace() != IPAddressSpace::Public && request.targetAddressSpace() != connectionAddressSpace) {
            return completionHandler(ResourceError { "WebKitErrorDomain"_s, 0, request.url(),
                "Local Network Access: connection's resolved address space does not match the request's declared targetAddressSpace"_s,
                ResourceError::Type::AccessControl });
        }

        if (!isLessPublicThan(connectionAddressSpace, clientPolicyContainerAddressSpace))
            return completionHandler(std::nullopt);
    }

    auto refusalWithReason = [url = request.url()](ASCIILiteral reason) {
        return ResourceError { "WebKitErrorDomain"_s, 0, url, reason, ResourceError::Type::AccessControl };
    };

    if (!clientIsSecureContext)
        return completionHandler(refusalWithReason("the requesting document is not a secure context, which Local Network Access requires"_s));

    if (!(connectionAddressSpace == IPAddressSpace::Loopback ? loopbackNetworkAllowedByPermissionsPolicy : localNetworkAllowedByPermissionsPolicy))
        return completionHandler(refusalWithReason("the requesting frame is not allowed to use the \"local-network\" or \"loopback-network\" feature"_s));

    permissionCheck(clientOrigin, connectionAddressSpace, [refusalWithReason, completionHandler = WTF::move(completionHandler)](PermissionState state) mutable {
        switch (state) {
        case PermissionState::Granted:
            return completionHandler(std::nullopt);
        case PermissionState::Prompt:
            return completionHandler(refusalWithReason("no permission to reach the local network has been granted, and there is no document to ask in. Issue the request from a page, or let the fetch handler pass it through"_s));
        case PermissionState::Denied:
            break;
        }
        completionHandler(refusalWithReason("permission to reach the local network was denied"_s));
    });
}

} // namespace WebCore
