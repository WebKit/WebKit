/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextEncoder.h"

#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSGenericTypedArrayViewInlines.h>
#include <wtf/StdLibExtras.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {

RefPtr<Uint8Array> TextEncoder::encode(String&& input) const
{
    auto result = input.tryGetUTF8([&](std::span<const char8_t> span) -> RefPtr<Uint8Array> {
        return Uint8Array::tryCreate(byteCast<uint8_t>(span));
    });
    if (result)
        return result.value();
    return Uint8Array::tryCreate(nullptr, 0);
}

auto TextEncoder::encodeInto(String&& input, Ref<Uint8Array>&& array) -> EncodeIntoResult
{
    auto* destinationBytes = static_cast<uint8_t*>(array->baseAddress());
    auto capacity = array->byteLength();

    uint64_t read = 0;
    uint64_t written = 0;

    for (auto token : StringView(input).codePoints()) {
        if (written >= capacity) {
            ASSERT(written == capacity);
            break;
        }
        UBool sawError = false;
        U8_APPEND(destinationBytes, written, capacity, token, sawError);
        if (sawError)
            break;
        if (U_IS_BMP(token))
            read++;
        else
            read += 2;
    }

    return { read, written };
}

}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
