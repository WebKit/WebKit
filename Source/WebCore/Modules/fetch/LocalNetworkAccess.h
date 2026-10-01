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

#pragma once

#include <WebCore/PermissionState.h>
#include <WebCore/ResourceError.h>
#include <expected>
#include <optional>
#include <wtf/Forward.h>

namespace WebCore {

enum class IPAddressSpace : uint8_t;
class ResourceRequest;
struct ClientOrigin;

// Where no backend can report the peer's address space it is always Unknown, which is fail-closed and
// would block nearly every cross-origin subresource, so the check is skipped instead.
constexpr bool canDetermineConnectionAddressSpace()
{
#if PLATFORM(COCOA)
    return true;
#else
    return false;
#endif
}

// The order these are consulted in is security-critical: an undetermined address space must lose even
// to a recorded grant.
enum class LocalNetworkAccessPermissionRequestOutcome : uint8_t {
    RefuseAsUndetermined,
    UseRecordedDecision,
    Prompt,
};
WEBCORE_EXPORT LocalNetworkAccessPermissionRequestOutcome localNetworkAccessPermissionRequestOutcome(IPAddressSpace connectionAddressSpace, bool hasRecordedDecision);

enum class LocalNetworkAccessRequirement : bool { None, Permission };

// currentURL is the URL the connection was made to, which after a redirect differs from the request's
// URL, and is what the same-origin exemption has to be judged against.
WEBCORE_EXPORT std::expected<LocalNetworkAccessRequirement, ResourceError> checkLocalNetworkAccess(const ResourceRequest&, const URL& currentURL, IPAddressSpace connectionAddressSpace, IPAddressSpace clientPolicyContainerAddressSpace, bool clientIsSecureContext, const ClientOrigin&, bool localNetworkAllowedByPermissionsPolicy, bool loopbackNetworkAllowedByPermissionsPolicy);
WEBCORE_EXPORT std::optional<ResourceError> localNetworkAccessPermissionError(const URL&, PermissionState);

} // namespace WebCore
