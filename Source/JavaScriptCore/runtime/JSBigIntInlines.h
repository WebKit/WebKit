/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/JSBigInt.h>
#include <JavaScriptCore/JSCJSValueBigInt.h>

namespace JSC {

inline JSValue JSBigInt::toNumber(JSValue bigInt)
{
    ASSERT(bigInt.isBigInt());
#if USE(BIGINT32)
    if (bigInt.isBigInt32())
        return jsNumber(bigInt.bigInt32AsInt32());
#endif
    return toNumberHeap(uncheckedDowncast<JSBigInt>(bigInt));
}

uint64_t JSBigInt::toBigUInt64(JSValue bigInt)
{
    ASSERT(bigInt.isBigInt());
#if USE(BIGINT32)
    if (bigInt.isBigInt32())
        return static_cast<uint64_t>(static_cast<int64_t>(bigInt.bigInt32AsInt32()));
#endif
    return toBigUInt64Heap(bigInt.asHeapBigInt());
}

inline int64_t JSBigInt::toBigInt64(JSValue bigInt)
{
    ASSERT(bigInt.isBigInt());
#if USE(BIGINT32)
    if (bigInt.isBigInt32())
        return static_cast<int64_t>(bigInt.bigInt32AsInt32());
#endif
    return static_cast<int64_t>(toBigUInt64Heap(bigInt.asHeapBigInt()));
}

ALWAYS_INLINE std::optional<double> JSBigInt::tryExtractDouble(JSValue value)
{
    if (value.isNumber())
        return value.asNumber();

    if (!value.isBigInt())
        return std::nullopt;

#if USE(BIGINT32)
    if (value.isBigInt32())
        return value.bigInt32AsInt32();
#endif

    ASSERT(value.isHeapBigInt());
    JSBigInt* bigInt = value.asHeapBigInt();
    if (!bigInt->length())
        return 0;

    uint64_t integer = 0;
    if constexpr (sizeof(Digit) == 8) {
        if (bigInt->length() != 1)
            return std::nullopt;
        integer = bigInt->digit(0);
    } else {
        ASSERT(sizeof(Digit) == 4);
        if (bigInt->length() > 2)
            return std::nullopt;
        integer = bigInt->digit(0);
        if (bigInt->length() == 2)
            integer |= (static_cast<uint64_t>(bigInt->digit(1)) << 32);
    }

    if (integer <= maxSafeIntegerAsUInt64())
        return (bigInt->sign()) ? -static_cast<double>(integer) : static_cast<double>(integer);

    return std::nullopt;
}

}
