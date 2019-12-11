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

#include "LinkIconType.h"
#include <wtf/HashMap.h>
#include <wtf/Optional.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct LinkIcon {
    URL url;
    LinkIconType type;
    String mimeType;
    Optional<unsigned> size;
    Vector<std::pair<String, String>> attributes;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, LinkIcon&);
};

template<class Encoder>
void LinkIcon::encode(Encoder& encoder) const
{
    encoder << url << mimeType << size << attributes;
    encoder.encodeEnum(type);
}

template<class Decoder>
bool LinkIcon::decode(Decoder& decoder, LinkIcon& result)
{
    if (!decoder.decode(result.url))
        return false;

    if (!decoder.decode(result.mimeType))
        return false;

    if (!decoder.decode(result.size))
        return false;

    if (!decoder.decode(result.attributes))
        return false;

    if (!decoder.decodeEnum(result.type))
        return false;

    return true;
}

} // namespace WebCore
