/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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
#include "CSSCalcOperator.h"

#include <wtf/text/TextStream.h>

namespace WebCore {
namespace CSSCalc {

TextStream& operator<<(TextStream& ts, Operator op)
{
    switch (op) {
    case Operator::Sum: ts << '+'; break;
    case Operator::Negate: ts << '-'; break;
    case Operator::Product: ts << '*'; break;
    case Operator::Invert: ts << '/'; break;
    case Operator::Min: ts << "min"_s; break;
    case Operator::Max: ts << "max"_s; break;
    case Operator::Clamp: ts << "clamp"_s; break;
    case Operator::Pow: ts << "pow"_s; break;
    case Operator::Sqrt: ts << "sqrt"_s; break;
    case Operator::Hypot: ts << "hypot"_s; break;
    case Operator::Sin: ts << "sin"_s; break;
    case Operator::Cos: ts << "cos"_s; break;
    case Operator::Tan: ts << "tan"_s; break;
    case Operator::Exp: ts << "exp"_s; break;
    case Operator::Log: ts << "log"_s; break;
    case Operator::Asin: ts << "asin"_s; break;
    case Operator::Acos: ts << "acos"_s; break;
    case Operator::Atan: ts << "atan"_s; break;
    case Operator::Atan2: ts << "atan2"_s; break;
    case Operator::Abs: ts << "abs"_s; break;
    case Operator::Sign: ts << "sign"_s; break;
    case Operator::Mod: ts << "mod"_s; break;
    case Operator::Rem: ts << "rem"_s; break;
    case Operator::RoundUp: ts << "up"_s; break;
    case Operator::RoundDown: ts << "down"_s; break;
    case Operator::RoundToZero: ts << "to-zero"_s; break;
    case Operator::RoundNearest: ts << "nearest"_s; break;
    case Operator::Progress: ts << "progress"_s; break;
    case Operator::Random: ts << "random"_s; break;
    case Operator::Blend: ts << "blend"_s; break;
    }
    return ts;
}

} // namespace CSSCalc
} // namespace WebCore
