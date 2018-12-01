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
    encoder.encodeEnum(navigationType);
    encoder.encodeEnum(modifiers);
    encoder.encodeEnum(mouseButton);
    encoder.encodeEnum(syntheticClickType);
    encoder << userGestureTokenIdentifier;
    encoder << canHandleRequest;
    encoder.encodeEnum(shouldOpenExternalURLsPolicy);
    encoder << downloadAttribute;
    encoder << clickLocationInRootViewCoordinates;
    encoder << isRedirect;
    encoder << treatAsSameOriginNavigation;
    encoder << hasOpenedFrames;
    encoder << openedByDOMWithOpener;
    encoder << requesterOrigin;
    encoder << targetBackForwardItemIdentifier;
    encoder.encodeEnum(lockHistory);
    encoder.encodeEnum(lockBackForwardList);
    encoder << clientRedirectSourceForHistory;
}

std::optional<NavigationActionData> NavigationActionData::decode(IPC::Decoder& decoder)
{
    WebCore::NavigationType navigationType;
    if (!decoder.decodeEnum(navigationType))
        return std::nullopt;
    
    WebEvent::Modifiers modifiers;
    if (!decoder.decodeEnum(modifiers))
        return std::nullopt;
    
    WebMouseEvent::Button mouseButton;
    if (!decoder.decodeEnum(mouseButton))
        return std::nullopt;
    
    WebMouseEvent::SyntheticClickType syntheticClickType;
    if (!decoder.decodeEnum(syntheticClickType))
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
    if (!decoder.decodeEnum(shouldOpenExternalURLsPolicy))
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

    WebCore::LockHistory lockHistory;
    if (!decoder.decodeEnum(lockHistory))
        return std::nullopt;

    WebCore::LockBackForwardList lockBackForwardList;
    if (!decoder.decodeEnum(lockBackForwardList))
        return std::nullopt;

    std::optional<String> clientRedirectSourceForHistory;
    decoder >> clientRedirectSourceForHistory;
    if (!clientRedirectSourceForHistory)
        return std::nullopt;

    return {{ WTFMove(navigationType), WTFMove(modifiers), WTFMove(mouseButton), WTFMove(syntheticClickType), WTFMove(*userGestureTokenIdentifier),
        WTFMove(*canHandleRequest), WTFMove(shouldOpenExternalURLsPolicy), WTFMove(*downloadAttribute), WTFMove(clickLocationInRootViewCoordinates),
        WTFMove(*isRedirect), *treatAsSameOriginNavigation, *hasOpenedFrames, *openedByDOMWithOpener, WTFMove(*requesterOrigin),
        WTFMove(*targetBackForwardItemIdentifier), lockHistory, lockBackForwardList, WTFMove(*clientRedirectSourceForHistory) }};
}

} // namespace WebKit
