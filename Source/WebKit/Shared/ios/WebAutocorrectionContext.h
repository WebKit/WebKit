/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "Decoder.h"
#include "Encoder.h"
#include <wtf/NotFound.h>
#include <wtf/Optional.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

struct WebAutocorrectionContext {
    String contextBefore;
    String markedText;
    String selectedText;
    String contextAfter;
    uint64_t location { notFound };
    uint64_t length { 0 };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<WebAutocorrectionContext> decode(Decoder&);
};

template<class Encoder> inline void WebAutocorrectionContext::encode(Encoder& encoder) const
{
    encoder << contextBefore << markedText << selectedText << contextAfter << location << length;
}

template<class Decoder> inline Optional<WebAutocorrectionContext> WebAutocorrectionContext::decode(Decoder& decoder)
{
    Optional<String> contextBefore;
    decoder >> contextBefore;
    if (!contextBefore)
        return WTF::nullopt;

    Optional<String> markedText;
    decoder >> markedText;
    if (!markedText)
        return WTF::nullopt;

    Optional<String> selectedText;
    decoder >> selectedText;
    if (!selectedText)
        return WTF::nullopt;

    Optional<String> contextAfter;
    decoder >> contextAfter;
    if (!contextAfter)
        return WTF::nullopt;

    Optional<uint64_t> location;
    decoder >> location;
    if (!location)
        return WTF::nullopt;

    Optional<uint64_t> length;
    decoder >> length;
    if (!length)
        return WTF::nullopt;

    return {{ WTFMove(*contextBefore), WTFMove(*markedText), WTFMove(*selectedText), WTFMove(*contextAfter), WTFMove(*location), WTFMove(*length) }};
}

} // namespace WebKit
