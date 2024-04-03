/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "DownloadID.h"
#include "NavigatingToAppBoundDomain.h"
#include "SandboxExtension.h"
#include "WebsitePoliciesData.h"

namespace JSC {
enum class MessageLevel : uint8_t;
enum class MessageSource : uint8_t;
}

namespace WebKit {

struct PolicyDecisionConsoleMessage {
    JSC::MessageLevel messageLevel;
    JSC::MessageSource messageSource;
    String message;
};

struct PolicyDecision {
    std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain { std::nullopt };
    WebCore::PolicyAction policyAction { WebCore::PolicyAction::Ignore };
    uint64_t navigationID { 0 };
    std::optional<DownloadID> downloadID { std::nullopt };
    std::optional<WebsitePoliciesData> websitePoliciesData { std::nullopt };
    std::optional<SandboxExtension::Handle> sandboxExtensionHandle { std::nullopt };
    std::optional<PolicyDecisionConsoleMessage> consoleMessage { std::nullopt };
};

} // namespace WebKit
