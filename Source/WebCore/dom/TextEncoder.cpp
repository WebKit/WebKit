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
#include <wtf/unicode/UTF8Conversion.h>

namespace WebCore {

RefPtr<Uint8Array> TextEncoder::encode(String&& input) const
{
    auto result = tryGetUTF8([&](std::span<const char8_t> span) -> RefPtr<Uint8Array> {
        return Uint8Array::tryCreate(byteCast<uint8_t>(span));
    }, input);
    if (result)
        return result.value();
    return Uint8Array::tryCreate(nullptr, 0);
}

auto TextEncoder::encodeInto(String&& input, Ref<Uint8Array>&& array) -> EncodeIntoResult
{
    auto destination = byteCast<char8_t>(array->mutableSpan());
    auto result = input.is8Bit()
        ? WTF::Unicode::convert(input.span8(), destination)
        : WTF::Unicode::convertReplacingUnpairedSurrogates(input.span16(), destination);
    return { result.sourceLength, result.buffer.size() };
}

}
