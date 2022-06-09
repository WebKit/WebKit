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

void HTTPBody::Element::encode(IPC::Encoder& encoder) const
{
    encoder << type;
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

auto HTTPBody::Element::decode(IPC::Decoder& decoder) -> std::optional<Element>
{
    Element result;
    if (!decoder.decode(result.type) || !isValidEnum(result.type))
        return std::nullopt;
    if (!decoder.decode(result.data))
        return std::nullopt;
    if (!decoder.decode(result.filePath))
        return std::nullopt;
    if (!decoder.decode(result.fileStart))
        return std::nullopt;
    if (!decoder.decode(result.fileLength))
        return std::nullopt;
    if (!decoder.decode(result.expectedFileModificationTime))
        return std::nullopt;
    if (!decoder.decode(result.blobURLString))
        return std::nullopt;

    return result;
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

    encoder << m_documentState;
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

std::optional<FrameState> FrameState::decode(IPC::Decoder& decoder)
{
    FrameState result;
    if (!decoder.decode(result.urlString))
        return std::nullopt;
    if (!decoder.decode(result.originalURLString))
        return std::nullopt;
    if (!decoder.decode(result.referrer))
        return std::nullopt;
    if (!decoder.decode(result.target))
        return std::nullopt;

    if (!decoder.decode(result.m_documentState))
        return std::nullopt;
    result.validateDocumentState();

    if (!decoder.decode(result.stateObjectData))
        return std::nullopt;

    if (!decoder.decode(result.documentSequenceNumber))
        return std::nullopt;
    if (!decoder.decode(result.itemSequenceNumber))
        return std::nullopt;

    if (!decoder.decode(result.scrollPosition))
        return std::nullopt;
    if (!decoder.decode(result.shouldRestoreScrollPosition))
        return std::nullopt;
    if (!decoder.decode(result.pageScaleFactor))
        return std::nullopt;

    if (!decoder.decode(result.httpBody))
        return std::nullopt;

#if PLATFORM(IOS_FAMILY)
    if (!decoder.decode(result.exposedContentRect))
        return std::nullopt;
    if (!decoder.decode(result.unobscuredContentRect))
        return std::nullopt;
    if (!decoder.decode(result.minimumLayoutSizeInScrollViewCoordinates))
        return std::nullopt;
    if (!decoder.decode(result.contentSize))
        return std::nullopt;
    if (!decoder.decode(result.scaleIsInitial))
        return std::nullopt;
    if (!decoder.decode(result.obscuredInsets))
        return std::nullopt;
#endif

    if (!decoder.decode(result.children))
        return std::nullopt;

    return result;
}

void PageState::encode(IPC::Encoder& encoder) const
{
    encoder << title << mainFrameState << !!sessionStateObject;

    if (sessionStateObject)
        encoder << sessionStateObject->toWireBytes();

    encoder << shouldOpenExternalURLsPolicy;
}

bool PageState::decode(IPC::Decoder& decoder, PageState& result)
{
    if (!decoder.decode(result.title))
        return false;
    std::optional<FrameState> mainFrameState;
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

    std::optional<ShouldOpenExternalURLsPolicy> shouldOpenExternalURLsPolicy;
    decoder >> shouldOpenExternalURLsPolicy;
    if (!shouldOpenExternalURLsPolicy)
        return false;

    result.shouldOpenExternalURLsPolicy = *shouldOpenExternalURLsPolicy;
    return true;
}

void BackForwardListItemState::encode(IPC::Encoder& encoder) const
{
    encoder << identifier;
    encoder << pageState;
    encoder << hasCachedPage;
}

std::optional<BackForwardListItemState> BackForwardListItemState::decode(IPC::Decoder& decoder)
{
    BackForwardListItemState result;

    auto identifier = BackForwardItemIdentifier::decode(decoder);
    if (!identifier)
        return std::nullopt;
    result.identifier = *identifier;

    if (!decoder.decode(result.pageState))
        return std::nullopt;

    if (!decoder.decode(result.hasCachedPage))
        return std::nullopt;

    return result;
}

void BackForwardListState::encode(IPC::Encoder& encoder) const
{
    encoder << items;
    encoder << currentIndex;
}

std::optional<BackForwardListState> BackForwardListState::decode(IPC::Decoder& decoder)
{
    std::optional<Vector<BackForwardListItemState>> items;
    decoder >> items;
    if (!items)
        return std::nullopt;

    std::optional<uint32_t> currentIndex;
    if (!decoder.decode(currentIndex))
        return std::nullopt;

    return {{ WTFMove(*items), WTFMove(currentIndex) }};
}

void FrameState::validateDocumentState() const
{
    for (auto& stateString : m_documentState) {
        if (stateString.isNull())
            continue;

        if (!stateString.is8Bit())
            continue;

        // rdar://48634553 indicates 8-bit string can be invalid.
        const LChar* characters8 = stateString.characters8();
        for (unsigned i = 0; i < stateString.length(); ++i) {
            auto character = characters8[i];
            RELEASE_ASSERT(isLatin1(character));
        }
    }
}

void FrameState::setDocumentState(const Vector<String>& documentState, ShouldValidate shouldValidate)
{
    m_documentState = documentState;

    if (shouldValidate == ShouldValidate::Yes)
        validateDocumentState();
}

} // namespace WebKit
