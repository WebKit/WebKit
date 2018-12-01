/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "Pasteboard.h"

#include "PasteboardStrategy.h"
#include "PlatformStrategies.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include <wtf/URLParser.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

// Making this non-inline so that WebKit 2's decoding doesn't have to include Image.h.
PasteboardImage::PasteboardImage() = default;
PasteboardImage::~PasteboardImage() = default;

bool Pasteboard::isSafeTypeForDOMToReadAndWrite(const String& type)
{
    return type == "text/plain" || type == "text/html" || type == "text/uri-list";
}

bool Pasteboard::canExposeURLToDOMWhenPasteboardContainsFiles(const String& urlString)
{
    URL url({ }, urlString);
    return url.protocolIsInHTTPFamily() || url.protocolIsBlob() || url.protocolIsData();
}

Ref<SharedBuffer> PasteboardCustomData::createSharedBuffer() const
{
    const static unsigned currentCustomDataSerializationVersion = 1;

    WTF::Persistence::Encoder encoder;
    encoder << currentCustomDataSerializationVersion;
    encoder << origin;
    encoder << sameOriginCustomData;
    encoder << orderedTypes;
    return SharedBuffer::create(encoder.buffer(), encoder.bufferSize());
}

PasteboardCustomData PasteboardCustomData::fromSharedBuffer(const SharedBuffer& buffer)
{
    const static unsigned maxSupportedDataSerializationVersionNumber = 1;

    PasteboardCustomData result;
    WTF::Persistence::Decoder decoder { reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size() };
    unsigned version;
    if (!decoder.decode(version) || version > maxSupportedDataSerializationVersionNumber)
        return { };

    if (!decoder.decode(result.origin))
        return { };

    if (!decoder.decode(result.sameOriginCustomData))
        return { };

    if (!decoder.decode(result.orderedTypes))
        return { };

    return result;
}

#if !PLATFORM(COCOA)

Vector<String> Pasteboard::readAllStrings(const String& type)
{
    auto result = readString(type);
    if (result.isEmpty())
        return { };

    return { result };
}

#endif

};
