/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(APP_HIGHLIGHTS)

#include "SharedBuffer.h"
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum class CreateNewGroupForHighlight : bool { No, Yes };

enum class HighlightRequestOriginatedInApp : bool { No, Yes };

struct AppHighlight {
    Ref<WebCore::SharedBuffer> highlight;
    std::optional<String> text;
    CreateNewGroupForHighlight isNewGroup;
    HighlightRequestOriginatedInApp requestOriginatedInApp;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<AppHighlight> decode(Decoder&);
};


template<class Encoder>
void AppHighlight::encode(Encoder& encoder) const
{
    encoder << static_cast<size_t>(highlight->size());
    encoder.encodeFixedLengthData(highlight->data(), highlight->size(), 1);

    encoder << text;

    encoder << isNewGroup;

    encoder << requestOriginatedInApp;
}

template<class Decoder>
std::optional<AppHighlight> AppHighlight::decode(Decoder& decoder)
{

    std::optional<size_t> length;
    decoder >> length;
    if (!length)
        return std::nullopt;

    if (!decoder.template bufferIsLargeEnoughToContain<uint8_t>(length.value()))
        return std::nullopt;

    Vector<uint8_t> highlight;
    highlight.grow(*length);
    if (!decoder.decodeFixedLengthData(highlight.data(), highlight.size(), 1))
        return std::nullopt;

    std::optional<std::optional<String>> text;
    decoder >> text;
    if (!text)
        return std::nullopt;

    CreateNewGroupForHighlight isNewGroup;
    if (!decoder.decode(isNewGroup))
        return std::nullopt;

    HighlightRequestOriginatedInApp requestOriginatedInApp;
    if (!decoder.decode(requestOriginatedInApp))
        return std::nullopt;

    return {{ SharedBuffer::create(WTFMove(highlight)), WTFMove(*text), isNewGroup, requestOriginatedInApp }};
}

}

#endif // ENABLE(APP_HIGHLIGHTS)
