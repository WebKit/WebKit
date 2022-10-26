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

#include "config.h"
#include "NavigationActionData.h"

#include "ArgumentCoders.h"
#include "Decoder.h"
#include "Encoder.h"
#include "WebCoreArgumentCoders.h"

namespace WebKit {

void NavigationActionData::encode(IPC::Encoder& encoder) const
{
    encoder << navigationType;
    encoder << modifiers;
    encoder << mouseButton;
    encoder << syntheticClickType;
    encoder << userGestureTokenIdentifier;
    encoder << canHandleRequest;
    encoder << shouldOpenExternalURLsPolicy;
    encoder << downloadAttribute;
    encoder << clickLocationInRootViewCoordinates;
    encoder << isRedirect;
    encoder << treatAsSameOriginNavigation;
    encoder << hasOpenedFrames;
    encoder << openedByDOMWithOpener;
    encoder << requesterOrigin;
    encoder << targetBackForwardItemIdentifier;
    encoder << sourceBackForwardItemIdentifier;
    encoder << lockHistory;
    encoder << lockBackForwardList;
    encoder << clientRedirectSourceForHistory;
    encoder << effectiveSandboxFlags;
    encoder << privateClickMeasurement;
#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    encoder << webHitTestResultData;
#endif
}

std::optional<NavigationActionData> NavigationActionData::decode(IPC::Decoder& decoder)
{
    WebCore::NavigationType navigationType;
    if (!decoder.decode(navigationType))
        return std::nullopt;
    
    OptionSet<WebEventModifier> modifiers;
    if (!decoder.decode(modifiers))
        return std::nullopt;

    std::optional<WebMouseEventButton> mouseButton;
    decoder >> mouseButton;
    if (!mouseButton)
        return std::nullopt;
    
    std::optional<WebMouseEventSyntheticClickType> syntheticClickType;
    decoder >> syntheticClickType;
    if (!syntheticClickType)
        return std::nullopt;
    
    std::optional<uint64_t> userGestureTokenIdentifier;
    decoder >> userGestureTokenIdentifier;
    if (!userGestureTokenIdentifier)
        return std::nullopt;
    
    std::optional<bool> canHandleRequest;
    decoder >> canHandleRequest;
    if (!canHandleRequest)
        return std::nullopt;
    
    WebCore::ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy;
    if (!decoder.decode(shouldOpenExternalURLsPolicy))
        return std::nullopt;
    
    std::optional<String> downloadAttribute;
    decoder >> downloadAttribute;
    if (!downloadAttribute)
        return std::nullopt;
    
    WebCore::FloatPoint clickLocationInRootViewCoordinates;
    if (!decoder.decode(clickLocationInRootViewCoordinates))
        return std::nullopt;
    
    std::optional<bool> isRedirect;
    decoder >> isRedirect;
    if (!isRedirect)
        return std::nullopt;

    std::optional<bool> treatAsSameOriginNavigation;
    decoder >> treatAsSameOriginNavigation;
    if (!treatAsSameOriginNavigation)
        return std::nullopt;

    std::optional<bool> hasOpenedFrames;
    decoder >> hasOpenedFrames;
    if (!hasOpenedFrames)
        return std::nullopt;

    std::optional<bool> openedByDOMWithOpener;
    decoder >> openedByDOMWithOpener;
    if (!openedByDOMWithOpener)
        return std::nullopt;

    std::optional<WebCore::SecurityOriginData> requesterOrigin;
    decoder >> requesterOrigin;
    if (!requesterOrigin)
        return std::nullopt;

    std::optional<std::optional<WebCore::BackForwardItemIdentifier>> targetBackForwardItemIdentifier;
    decoder >> targetBackForwardItemIdentifier;
    if (!targetBackForwardItemIdentifier)
        return std::nullopt;

    std::optional<std::optional<WebCore::BackForwardItemIdentifier>> sourceBackForwardItemIdentifier;
    decoder >> sourceBackForwardItemIdentifier;
    if (!sourceBackForwardItemIdentifier)
        return std::nullopt;

    WebCore::LockHistory lockHistory;
    if (!decoder.decode(lockHistory))
        return std::nullopt;

    WebCore::LockBackForwardList lockBackForwardList;
    if (!decoder.decode(lockBackForwardList))
        return std::nullopt;

    std::optional<String> clientRedirectSourceForHistory;
    decoder >> clientRedirectSourceForHistory;
    if (!clientRedirectSourceForHistory)
        return std::nullopt;

    std::optional<WebCore::SandboxFlags> effectiveSandboxFlags;
    decoder >> effectiveSandboxFlags;
    if (!effectiveSandboxFlags)
        return std::nullopt;

    std::optional<std::optional<WebCore::PrivateClickMeasurement>> privateClickMeasurement;
    decoder >> privateClickMeasurement;
    if (!privateClickMeasurement)
        return std::nullopt;

#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    std::optional<std::optional<WebKit::WebHitTestResultData>> webHitTestResultData;
    decoder >> webHitTestResultData;
    if (!webHitTestResultData)
        return std::nullopt;
#endif

    return { { WTFMove(navigationType), modifiers, WTFMove(*mouseButton), WTFMove(*syntheticClickType), WTFMove(*userGestureTokenIdentifier),
        WTFMove(*canHandleRequest), WTFMove(shouldOpenExternalURLsPolicy), WTFMove(*downloadAttribute), WTFMove(clickLocationInRootViewCoordinates),
        WTFMove(*isRedirect), *treatAsSameOriginNavigation, *hasOpenedFrames, *openedByDOMWithOpener, WTFMove(*requesterOrigin),
        WTFMove(*targetBackForwardItemIdentifier), WTFMove(*sourceBackForwardItemIdentifier), lockHistory, lockBackForwardList, WTFMove(*clientRedirectSourceForHistory), *effectiveSandboxFlags, WTFMove(*privateClickMeasurement),
#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
        WTFMove(*webHitTestResultData),
#endif
    } };
}

} // namespace WebKit
