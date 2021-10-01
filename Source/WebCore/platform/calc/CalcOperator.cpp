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
#include "CalcOperator.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

TextStream& operator<<(TextStream& ts, CalcOperator op)
{
    switch (op) {
    case CalcOperator::Add: ts << "+"; break;
    case CalcOperator::Subtract: ts << "-"; break;
    case CalcOperator::Multiply: ts << "*"; break;
    case CalcOperator::Divide: ts << "/"; break;
    case CalcOperator::Min: ts << "min"; break;
    case CalcOperator::Max: ts << "max"; break;
    case CalcOperator::Clamp: ts << "clamp"; break;
    case CalcOperator::Pow: ts << "pow"; break;
    case CalcOperator::Sqrt: ts << "sqrt"; break;
    case CalcOperator::Hypot: ts << "hypot"; break;
    case CalcOperator::Sin: ts << "sin"; break;
    case CalcOperator::Cos: ts << "cos"; break;
    case CalcOperator::Tan: ts << "tan"; break;
    case CalcOperator::Exp: ts << "exp"; break;
    case CalcOperator::Log: ts << "log"; break;
    case CalcOperator::Asin: ts << "asin"; break;
    case CalcOperator::Acos: ts << "acos"; break;
    case CalcOperator::Atan: ts << "atan"; break;
    case CalcOperator::Atan2: ts << "atan2"; break;
    case CalcOperator::Abs: ts << "abs"; break;
    case CalcOperator::Sign: ts << "sign"; break;
    case CalcOperator::Mod: ts << "mod"; break;
    case CalcOperator::Rem: ts << "rem"; break;
    case CalcOperator::Round: ts << "round"; break;
    case CalcOperator::Up: ts << "up"; break;
    case CalcOperator::Down: ts << "down"; break;
    case CalcOperator::ToZero: ts << "to-zero"; break;
    case CalcOperator::Nearest: ts << "nearest"; break;
    }
    return ts;
}

}
