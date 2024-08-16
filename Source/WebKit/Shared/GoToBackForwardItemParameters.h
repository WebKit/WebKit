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

#pragma once

#include "NetworkResourceLoadIdentifier.h"
#include "SandboxExtension.h"
#include "WebsitePoliciesData.h"
#include <WebCore/BackForwardItemIdentifier.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/NavigationIdentifier.h>
#include <WebCore/PublicSuffix.h>
#include <WebCore/ShouldTreatAsContinuingLoad.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

struct GoToBackForwardItemParameters {
    WebCore::NavigationIdentifier navigationID;
    WebCore::BackForwardItemIdentifier backForwardItemID;
    WebCore::FrameLoadType backForwardType;
    WebCore::ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad;
    std::optional<WebsitePoliciesData> websitePolicies;
    bool lastNavigationWasAppInitiated;
    std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume;
    WebCore::PublicSuffix publicSuffix;
    SandboxExtension::Handle sandboxExtensionHandle;
};

} // namespace WebKit
