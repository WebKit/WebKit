/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "SessionState.h"

#include "WebCoreArgumentCoders.h"
#include <WebCore/BackForwardItemIdentifier.h>

namespace WebKit {
using namespace WebCore;

bool isValidEnum(WebCore::ShouldOpenExternalURLsPolicy policy)
{
    switch (policy) {
    case WebCore::ShouldOpenExternalURLsPolicy::ShouldAllow:
    case WebCore::ShouldOpenExternalURLsPolicy::ShouldAllowExternalSchemes:
    case WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow:
        return true;
    }
    return false;
}

void HTTPBody::Element::encode(IPC::Encoder& encoder) const
{
    encoder.encodeEnum(type);
    encoder << data;
    encoder << filePath;
    encoder << fileStart;
    encoder << fileLength;
    encoder << expectedFileModificationTime;
    encoder << blobURLString;
}

static bool isValidEnum(HTTPBody::Element::Type type)
{
    switch (type) {
    case HTTPBody::Element::Type::Data:
    case HTTPBody::Element::Type::File:
    case HTTPBody::Element::Type::Blob:
        return true;
    }

    return false;
}

auto HTTPBody::Element::decode(IPC::Decoder& decoder) -> Optional<Element>
{
    Element result;
    if (!decoder.decodeEnum(result.type) || !isValidEnum(result.type))
        return WTF::nullopt;
    if (!decoder.decode(result.data))
        return WTF::nullopt;
    if (!decoder.decode(result.filePath))
        return WTF::nullopt;
    if (!decoder.decode(result.fileStart))
        return WTF::nullopt;
    if (!decoder.decode(result.fileLength))
        return WTF::nullopt;
    if (!decoder.decode(result.expectedFileModificationTime))
        return WTF::nullopt;
    if (!decoder.decode(result.blobURLString))
        return WTF::nullopt;

    return WTFMove(result);
}

void HTTPBody::encode(IPC::Encoder& encoder) const
{
    encoder << contentType;
    encoder << elements;
}

bool HTTPBody::decode(IPC::Decoder& decoder, HTTPBody& result)
{
    if (!decoder.decode(result.contentType))
        return false;
    if (!decoder.decode(result.elements))
        return false;

    return true;
}

void FrameState::encode(IPC::Encoder& encoder) const
{
    encoder << urlString;
    encoder << originalURLString;
    encoder << referrer;
    encoder << target;

    encoder << documentState;
    encoder << stateObjectData;

    encoder << documentSequenceNumber;
    encoder << itemSequenceNumber;

    encoder << scrollPosition;
    encoder << shouldRestoreScrollPosition;
    encoder << pageScaleFactor;

    encoder << httpBody;

#if PLATFORM(IOS_FAMILY)
    encoder << exposedContentRect;
    encoder << unobscuredContentRect;
    encoder << minimumLayoutSizeInScrollViewCoordinates;
    encoder << contentSize;
    encoder << scaleIsInitial;
    encoder << obscuredInsets;
#endif

    encoder << children;
}

Optional<FrameState> FrameState::decode(IPC::Decoder& decoder)
{
    FrameState result;
    if (!decoder.decode(result.urlString))
        return WTF::nullopt;
    if (!decoder.decode(result.originalURLString))
        return WTF::nullopt;
    if (!decoder.decode(result.referrer))
        return WTF::nullopt;
    if (!decoder.decode(result.target))
        return WTF::nullopt;

    if (!decoder.decode(result.documentState))
        return WTF::nullopt;
    if (!decoder.decode(result.stateObjectData))
        return WTF::nullopt;

    if (!decoder.decode(result.documentSequenceNumber))
        return WTF::nullopt;
    if (!decoder.decode(result.itemSequenceNumber))
        return WTF::nullopt;

    if (!decoder.decode(result.scrollPosition))
        return WTF::nullopt;
    if (!decoder.decode(result.shouldRestoreScrollPosition))
        return WTF::nullopt;
    if (!decoder.decode(result.pageScaleFactor))
        return WTF::nullopt;

    if (!decoder.decode(result.httpBody))
        return WTF::nullopt;

#if PLATFORM(IOS_FAMILY)
    if (!decoder.decode(result.exposedContentRect))
        return WTF::nullopt;
    if (!decoder.decode(result.unobscuredContentRect))
        return WTF::nullopt;
    if (!decoder.decode(result.minimumLayoutSizeInScrollViewCoordinates))
        return WTF::nullopt;
    if (!decoder.decode(result.contentSize))
        return WTF::nullopt;
    if (!decoder.decode(result.scaleIsInitial))
        return WTF::nullopt;
    if (!decoder.decode(result.obscuredInsets))
        return WTF::nullopt;
#endif

    if (!decoder.decode(result.children))
        return WTF::nullopt;

    return WTFMove(result);
}

void PageState::encode(IPC::Encoder& encoder) const
{
    encoder << title << mainFrameState << !!sessionStateObject;

    if (sessionStateObject)
        encoder << sessionStateObject->toWireBytes();

    encoder.encodeEnum(shouldOpenExternalURLsPolicy);
}

bool PageState::decode(IPC::Decoder& decoder, PageState& result)
{
    if (!decoder.decode(result.title))
        return false;
    Optional<FrameState> mainFrameState;
    decoder >> mainFrameState;
    if (!mainFrameState)
        return false;
    result.mainFrameState = WTFMove(*mainFrameState);

    bool hasSessionState;
    if (!decoder.decode(hasSessionState))
        return false;

    if (hasSessionState) {
        Vector<uint8_t> wireBytes;
        if (!decoder.decode(wireBytes))
            return false;

        result.sessionStateObject = SerializedScriptValue::createFromWireBytes(WTFMove(wireBytes));
    }

    if (!decoder.decodeEnum(result.shouldOpenExternalURLsPolicy) || !isValidEnum(result.shouldOpenExternalURLsPolicy))
        return false;

    return true;
}

void BackForwardListItemState::encode(IPC::Encoder& encoder) const
{
    encoder << identifier;
    encoder << pageState;
}

Optional<BackForwardListItemState> BackForwardListItemState::decode(IPC::Decoder& decoder)
{
    BackForwardListItemState result;

    auto identifier = BackForwardItemIdentifier::decode(decoder);
    if (!identifier)
        return WTF::nullopt;
    result.identifier = *identifier;

    if (!decoder.decode(result.pageState))
        return WTF::nullopt;

    return WTFMove(result);
}

void BackForwardListState::encode(IPC::Encoder& encoder) const
{
    encoder << items;
    encoder << currentIndex;
}

Optional<BackForwardListState> BackForwardListState::decode(IPC::Decoder& decoder)
{
    Optional<Vector<BackForwardListItemState>> items;
    decoder >> items;
    if (!items)
        return WTF::nullopt;

    Optional<uint32_t> currentIndex;
    if (!decoder.decode(currentIndex))
        return WTF::nullopt;

    return {{ WTFMove(*items), WTFMove(currentIndex) }};
}

} // namespace WebKit
