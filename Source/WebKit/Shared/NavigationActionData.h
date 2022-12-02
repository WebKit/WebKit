/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#include "WebHitTestResultData.h"
#include "WebMouseEvent.h"
#include <WebCore/BackForwardItemIdentifier.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/PrivateClickMeasurement.h>
#include <WebCore/SecurityOriginData.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebCore {
typedef int SandboxFlags;
}

namespace WebKit {

struct NavigationActionData {
    void encode(IPC::Encoder&) const;
    static std::optional<NavigationActionData> decode(IPC::Decoder&);

    WebCore::NavigationType navigationType { WebCore::NavigationType::Other };
    OptionSet<WebEventModifier> modifiers;
    WebMouseEventButton mouseButton { WebMouseEventButton::NoButton };
    WebMouseEventSyntheticClickType syntheticClickType { WebMouseEventSyntheticClickType::NoTap };
    uint64_t userGestureTokenIdentifier { 0 };
    bool canHandleRequest { false };
    WebCore::ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy { WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow };
    WTF::String downloadAttribute;
    WebCore::FloatPoint clickLocationInRootViewCoordinates;
    bool isRedirect { false };
    bool treatAsSameOriginNavigation { false };
    bool hasOpenedFrames { false };
    bool openedByDOMWithOpener { false };
    WebCore::SecurityOriginData requesterOrigin;
    std::optional<WebCore::BackForwardItemIdentifier> targetBackForwardItemIdentifier;
    std::optional<WebCore::BackForwardItemIdentifier> sourceBackForwardItemIdentifier;
    WebCore::LockHistory lockHistory { WebCore::LockHistory::No };
    WebCore::LockBackForwardList lockBackForwardList { WebCore::LockBackForwardList::No };
    WTF::String clientRedirectSourceForHistory;
    WebCore::SandboxFlags effectiveSandboxFlags { 0 };
    std::optional<WebCore::PrivateClickMeasurement> privateClickMeasurement;
#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    std::optional<WebKit::WebHitTestResultData> webHitTestResultData;
#endif
};

}
