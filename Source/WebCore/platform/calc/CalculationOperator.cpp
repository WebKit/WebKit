/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
#include "CalculationOperator.h"

#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Calculation {

TextStream& operator<<(TextStream& ts, Operator op)
{
    switch (op) {
    case Operator::Sum: ts << "+"; break;
    case Operator::Negate: ts << "-"; break;
    case Operator::Product: ts << "*"; break;
    case Operator::Invert: ts << "/"; break;
    case Operator::Min: ts << "min"; break;
    case Operator::Max: ts << "max"; break;
    case Operator::Clamp: ts << "clamp"; break;
    case Operator::Pow: ts << "pow"; break;
    case Operator::Sqrt: ts << "sqrt"; break;
    case Operator::Hypot: ts << "hypot"; break;
    case Operator::Sin: ts << "sin"; break;
    case Operator::Cos: ts << "cos"; break;
    case Operator::Tan: ts << "tan"; break;
    case Operator::Exp: ts << "exp"; break;
    case Operator::Log: ts << "log"; break;
    case Operator::Asin: ts << "asin"; break;
    case Operator::Acos: ts << "acos"; break;
    case Operator::Atan: ts << "atan"; break;
    case Operator::Atan2: ts << "atan2"; break;
    case Operator::Abs: ts << "abs"; break;
    case Operator::Sign: ts << "sign"; break;
    case Operator::Mod: ts << "mod"; break;
    case Operator::Rem: ts << "rem"; break;
    case Operator::Round: ts << "round"; break;
    case Operator::Up: ts << "up"; break;
    case Operator::Down: ts << "down"; break;
    case Operator::ToZero: ts << "to-zero"; break;
    case Operator::Nearest: ts << "nearest"; break;
    case Operator::Progress: ts << "progress"; break;
    case Operator::Random: ts << "random"; break;
    case Operator::Blend: ts << "blend"; break;
    }
    return ts;
}

} // namespace Calculation
} // namespace WebCore
