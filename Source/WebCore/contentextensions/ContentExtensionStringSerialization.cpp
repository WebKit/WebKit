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

#include "config.h"
#include "ContentExtensionStringSerialization.h"

#if ENABLE(CONTENT_EXTENSIONS)

namespace WebCore::ContentExtensions {

String deserializeString(std::span<const uint8_t> span)
{
    auto serializedLength = *reinterpret_cast<const uint32_t*>(span.data());
    return String::fromUTF8(span.data() + sizeof(uint32_t), serializedLength - sizeof(uint32_t));
}

void serializeString(Vector<uint8_t>& actions, const String& string)
{
    auto utf8 = string.utf8();
    uint32_t serializedLength = sizeof(uint32_t) + utf8.length();
    actions.reserveCapacity(actions.size() + serializedLength);
    actions.append(std::span<const uint8_t> { reinterpret_cast<const uint8_t*>(&serializedLength), sizeof(serializedLength) });
    actions.append(std::span<const uint8_t> { reinterpret_cast<const uint8_t*>(utf8.data()), utf8.length() });
}

size_t stringSerializedLength(std::span<const uint8_t> span)
{
    return *reinterpret_cast<const uint32_t*>(span.data());
}

} // namespace WebCore::ContentExtensions

#endif // ENABLE(CONTENT_EXTENSIONS)
