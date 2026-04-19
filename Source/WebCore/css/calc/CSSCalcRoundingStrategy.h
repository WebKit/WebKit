/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <cstdint>
#include <wtf/text/ASCIILiteral.h>

namespace WebCore {
namespace CSSCalc {

// Rounding strategy for the CSS round() function.
// https://drafts.csswg.org/css-values-4/#round-func
enum class RoundingStrategy : uint8_t { Nearest, Up, Down, ToZero };

constexpr ASCIILiteral nameLiteral(RoundingStrategy strategy)
{
    switch (strategy) {
    case RoundingStrategy::Nearest: return ASCIILiteral::fromLiteralUnsafe("nearest");
    case RoundingStrategy::Up:      return ASCIILiteral::fromLiteralUnsafe("up");
    case RoundingStrategy::Down:    return ASCIILiteral::fromLiteralUnsafe("down");
    case RoundingStrategy::ToZero:  return ASCIILiteral::fromLiteralUnsafe("to-zero");
    }
    return ASCIILiteral::fromLiteralUnsafe("nearest");
}

} // namespace CSSCalc
} // namespace WebCore
