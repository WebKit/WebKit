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

#include <WebCore/DocumentIsolationPolicy.h>
#include <WebCore/IsSecureContext.h>
#include <WebCore/OriginKeyed.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/URL.h>

namespace WebCore {
class ResourceResponse;
}

namespace WebKit {

class WebPreferences;

enum class IsSubstituteData : bool { No, Yes };

struct AgentClusterRequest {
    enum class Source : uint8_t { NavigationAction, SubstituteData, NavigationResponse };

    static AgentClusterRequest forNavigationAction(const URL&, const URL& topLevelCreationURL, IsSubstituteData);
    static AgentClusterRequest forNavigationResponse(const WebCore::ResourceResponse&, const URL& topLevelCreationURL, WebCore::IsSecureContext topLevelIsSecureContext, const WebPreferences&);

    Source source;
    WebCore::SecurityOriginData origin;
    URL topLevelCreationURL;
    std::optional<WebCore::OriginKeyed> requestedKeying { };
    WebCore::DocumentIsolationPolicy documentIsolationPolicy { WebCore::DocumentIsolationPolicy::None };
};

WebCore::DocumentIsolationPolicy documentIsolationPolicyForNavigationResponse(const URL&, WebCore::DocumentIsolationPolicy headerPolicy, WebCore::IsSecureContext topLevelIsSecureContext, const WebPreferences&);

} // namespace WebKit
