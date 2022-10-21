/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WebFoundTextRange.h"

#include "WebCoreArgumentCoders.h"

namespace WebKit {

bool WebFoundTextRange::operator==(const WebFoundTextRange& other) const
{
    if (frameIdentifier.isHashTableDeletedValue())
        return other.frameIdentifier.isHashTableDeletedValue();
    if (other.frameIdentifier.isHashTableDeletedValue())
        return false;

    return location == other.location
        && length == other.length
        && frameIdentifier == other.frameIdentifier
        && order == other.order;
}

void WebFoundTextRange::encode(IPC::Encoder& encoder) const
{
    encoder << location;
    encoder << length;
    encoder << frameIdentifier;
    encoder << order;
}

std::optional<WebFoundTextRange> WebFoundTextRange::decode(IPC::Decoder& decoder)
{
    WebFoundTextRange result;

    if (!decoder.decode(result.location))
        return std::nullopt;

    if (!decoder.decode(result.length))
        return std::nullopt;

    if (!decoder.decode(result.frameIdentifier))
        return std::nullopt;

    if (!decoder.decode(result.order))
        return std::nullopt;

    return result;
}

} // namespace WebKit
