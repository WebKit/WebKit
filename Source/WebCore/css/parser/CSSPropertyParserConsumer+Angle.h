/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSParserToken.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include <optional>
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSCalcSymbolTable;
class CSSPrimitiveValue;
class CSSParserTokenRange;

namespace CSSPropertyParserHelpers {

// MARK: - Consumer functions

// MARK: - Angle
std::optional<AngleRaw> consumeAngleRaw(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
RefPtr<CSSPrimitiveValue> consumeAngle(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk = UnitlessQuirk::Forbid, UnitlessZeroQuirk = UnitlessZeroQuirk::Forbid);

// MARK: - Angle or Percent
RefPtr<CSSPrimitiveValue> consumeAngleOrPercent(CSSParserTokenRange&, CSSParserMode);

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
