/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "WebsiteAutoplayPolicy.h"
#include "WebsiteAutoplayQuirk.h"
#include "WebsiteDataStoreParameters.h"
#include "WebsiteLegacyOverflowScrollingTouchPolicy.h"
#include "WebsiteMediaSourcePolicy.h"
#include "WebsiteMetaViewportPolicy.h"
#include "WebsitePopUpPolicy.h"
#include "WebsiteSimulatedMouseEventsDispatchPolicy.h"
#include <WebCore/CustomHeaderFields.h>
#include <WebCore/DeviceOrientationOrMotionPermissionState.h>
#include <wtf/OptionSet.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebCore {
class DocumentLoader;
}

namespace WebKit {

struct WebsitePoliciesData {
    static void applyToDocumentLoader(WebsitePoliciesData&&, WebCore::DocumentLoader&);

    bool contentBlockersEnabled { true };
    OptionSet<WebsiteAutoplayQuirk> allowedAutoplayQuirks;
    WebsiteAutoplayPolicy autoplayPolicy { WebsiteAutoplayPolicy::Default };
#if ENABLE(DEVICE_ORIENTATION)
    WebCore::DeviceOrientationOrMotionPermissionState deviceOrientationAndMotionAccessState;
#endif
    Vector<WebCore::CustomHeaderFields> customHeaderFields;
    WebsitePopUpPolicy popUpPolicy { WebsitePopUpPolicy::Default };
    Optional<WebsiteDataStoreParameters> websiteDataStoreParameters;
    String customUserAgent;
    String customJavaScriptUserAgentAsSiteSpecificQuirks;
    String customNavigatorPlatform;
    WebsiteMetaViewportPolicy metaViewportPolicy { WebsiteMetaViewportPolicy::Default };
    WebsiteMediaSourcePolicy mediaSourcePolicy { WebsiteMediaSourcePolicy::Default };
    WebsiteSimulatedMouseEventsDispatchPolicy simulatedMouseEventsDispatchPolicy { WebsiteSimulatedMouseEventsDispatchPolicy::Default };
    WebsiteLegacyOverflowScrollingTouchPolicy legacyOverflowScrollingTouchPolicy { WebsiteLegacyOverflowScrollingTouchPolicy::Default };
    bool allowContentChangeObserverQuirk { false };

    void encode(IPC::Encoder&) const;
    static Optional<WebsitePoliciesData> decode(IPC::Decoder&);
};

} // namespace WebKit
