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

namespace WebKit {

void HTTPBody::Element::encode(IPC::ArgumentEncoder& encoder) const
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

bool HTTPBody::Element::decode(IPC::ArgumentDecoder& decoder, Element& result)
{
    if (!decoder.decodeEnum(result.type) || !isValidEnum(result.type))
        return false;
    if (!decoder.decode(result.data))
        return false;
    if (!decoder.decode(result.filePath))
        return false;
    if (!decoder.decode(result.fileStart))
        return false;
    if (!decoder.decode(result.fileLength))
        return false;
    if (!decoder.decode(result.expectedFileModificationTime))
        return false;
    if (!decoder.decode(result.blobURLString))
        return false;

    return true;
}

void HTTPBody::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << contentType;
    encoder << elements;
}

bool HTTPBody::decode(IPC::ArgumentDecoder& decoder, HTTPBody& result)
{
    if (!decoder.decode(result.contentType))
        return false;
    if (!decoder.decode(result.elements))
        return false;

    return true;
}

void FrameState::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << urlString;
    encoder << originalURLString;
    encoder << referrer;
    encoder << target;

    encoder << documentState;
    encoder << stateObjectData;

    encoder << documentSequenceNumber;
    encoder << itemSequenceNumber;

    encoder << scrollPoint;
    encoder << pageScaleFactor;

    encoder << httpBody;

#if PLATFORM(IOS)
    encoder << exposedContentRect;
    encoder << unobscuredContentRect;
    encoder << minimumLayoutSizeInScrollViewCoordinates;
    encoder << contentSize;
    encoder << scaleIsInitial;
#endif

    encoder << children;
}

bool FrameState::decode(IPC::ArgumentDecoder& decoder, FrameState& result)
{
    if (!decoder.decode(result.urlString))
        return false;
    if (!decoder.decode(result.originalURLString))
        return false;
    if (!decoder.decode(result.referrer))
        return false;
    if (!decoder.decode(result.target))
        return false;

    if (!decoder.decode(result.documentState))
        return false;
    if (!decoder.decode(result.stateObjectData))
        return false;

    if (!decoder.decode(result.documentSequenceNumber))
        return false;
    if (!decoder.decode(result.itemSequenceNumber))
        return false;

    if (!decoder.decode(result.scrollPoint))
        return false;
    if (!decoder.decode(result.pageScaleFactor))
        return false;

    if (!decoder.decode(result.httpBody))
        return false;

#if PLATFORM(IOS)
    if (!decoder.decode(result.exposedContentRect))
        return false;
    if (!decoder.decode(result.unobscuredContentRect))
        return false;
    if (!decoder.decode(result.minimumLayoutSizeInScrollViewCoordinates))
        return false;
    if (!decoder.decode(result.contentSize))
        return false;
    if (!decoder.decode(result.scaleIsInitial))
        return false;
#endif

    if (!decoder.decode(result.children))
        return false;

    return true;
}

void PageState::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << title;
    encoder << mainFrameState;
}

bool PageState::decode(IPC::ArgumentDecoder& decoder, PageState& result)
{
    if (!decoder.decode(result.title))
        return false;
    if (!decoder.decode(result.mainFrameState))
        return false;

    return true;
}

void BackForwardListItemState::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << identifier;
    encoder << pageState;
}

bool BackForwardListItemState::decode(IPC::ArgumentDecoder& decoder, BackForwardListItemState& result)
{
    if (!decoder.decode(result.identifier))
        return false;

    if (!decoder.decode(result.pageState))
        return false;

    return true;
}

void BackForwardListState::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << items;
    encoder << currentIndex;
}

bool BackForwardListState::decode(IPC::ArgumentDecoder& decoder, BackForwardListState& result)
{
    if (!decoder.decode(result.items))
        return false;
    if (!decoder.decode(result.currentIndex))
        return false;

    return true;
}

} // namespace WebKit
