// Copyright 2016 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2023 Apple Inc. All rights reserved.
// Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include "CSSFunctionValue.h"
#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSShadowValue.h"
#include "CSSValuePool.h"
#include "GridArea.h"
#include "Length.h"
#include <variant>
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>

namespace WebCore {

class CSSGridLineNamesValue;

// When these functions are successful, they will consume all the relevant
// tokens from the range and also consume any whitespace which follows. When
// the start of the range doesn't match the type we're looking for, the range
// will not be modified.
namespace CSSPropertyParserHelpers {

RefPtr<CSSShadowValue> consumeSingleShadow(CSSParserTokenRange&, const CSSParserContext&, bool allowInset, bool allowSpread, bool isWebkitBoxShadow = false);

RefPtr<CSSPrimitiveValue> consumeSingleContainerName(CSSParserTokenRange&);

RefPtr<CSSValue> consumeAspectRatio(CSSParserTokenRange&);
RefPtr<CSSValue> consumeDisplay(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeWillChange(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeQuotes(CSSParserTokenRange&);
RefPtr<CSSValue> consumeSize(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeTextIndent(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeTextTransform(CSSParserTokenRange&);
RefPtr<CSSValue> consumeMarginSide(CSSParserTokenRange&, CSSPropertyID currentShorthand, CSSParserMode);
RefPtr<CSSValue> consumeMarginTrim(CSSParserTokenRange&);
RefPtr<CSSValue> consumeSide(CSSParserTokenRange&, CSSPropertyID currentShorthand, const CSSParserContext&);
RefPtr<CSSValue> consumeInsetLogicalStartEnd(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeClip(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeTouchAction(CSSParserTokenRange&);
RefPtr<CSSValue> consumeKeyframesName(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeSingleTransitionPropertyOrNone(CSSParserTokenRange&);
RefPtr<CSSValue> consumeSingleTransitionProperty(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeTextShadow(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeBoxShadow(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeWebkitBoxShadow(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeTextDecorationLine(CSSParserTokenRange&);
RefPtr<CSSValue> consumeTextEmphasisStyle(CSSParserTokenRange&);
RefPtr<CSSValue> consumeBorderWidth(CSSParserTokenRange&, CSSPropertyID currentShorthand, const CSSParserContext&);
RefPtr<CSSValue> consumeBorderColor(CSSParserTokenRange&, CSSPropertyID currentShorthand, const CSSParserContext&);
RefPtr<CSSValue> consumeRepeatStyle(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumePaintStroke(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumePaintOrder(CSSParserTokenRange&);
RefPtr<CSSValue> consumeStrokeDasharray(CSSParserTokenRange&);
RefPtr<CSSValue> consumeCursor(CSSParserTokenRange&, const CSSParserContext&, bool inQuirksMode);
RefPtr<CSSValue> consumeAttr(CSSParserTokenRange args, const CSSParserContext&);
RefPtr<CSSValue> consumeContent(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeScrollSnapAlign(CSSParserTokenRange&);
RefPtr<CSSValue> consumeScrollSnapType(CSSParserTokenRange&);
RefPtr<CSSValue> consumeScrollbarColor(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeScrollbarGutter(CSSParserTokenRange&);
RefPtr<CSSValue> consumeTextEdge(CSSPropertyID, CSSParserTokenRange&);
RefPtr<CSSValue> consumeBorderRadiusCorner(CSSParserTokenRange&, CSSParserMode);
bool consumeRadii(std::array<RefPtr<CSSValue>, 4>& horizontalRadii, std::array<RefPtr<CSSValue>, 4>& verticalRadii, CSSParserTokenRange&, CSSParserMode, bool useLegacyParsing);
enum PathParsingOption : uint8_t { RejectRay = 1 << 0, RejectFillRule = 1 << 1 };
RefPtr<CSSValue> consumePathOperation(CSSParserTokenRange&, const CSSParserContext&, OptionSet<PathParsingOption>);
RefPtr<CSSValue> consumePath(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeShapeOutside(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeBorderImageRepeat(CSSParserTokenRange&);
RefPtr<CSSValue> consumeBorderImageSlice(CSSPropertyID, CSSParserTokenRange&);
RefPtr<CSSValue> consumeBorderImageOutset(CSSParserTokenRange&);
RefPtr<CSSValue> consumeBorderImageWidth(CSSPropertyID, CSSParserTokenRange&);
bool consumeBorderImageComponents(CSSPropertyID, CSSParserTokenRange&, const CSSParserContext&, RefPtr<CSSValue>&, RefPtr<CSSValue>&, RefPtr<CSSValue>&, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
RefPtr<CSSValue> consumeWebkitBorderImage(CSSPropertyID, CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeReflect(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeSingleBackgroundClip(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeBackgroundClip(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeBackgroundSize(CSSPropertyID, CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeSingleBackgroundSize(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeSingleMaskSize(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeSingleWebkitBackgroundSize(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeLineBoxContain(CSSParserTokenRange&);
RefPtr<CSSValue> consumeContainerName(CSSParserTokenRange&);
RefPtr<CSSValue> consumeWebkitInitialLetter(CSSParserTokenRange&);
RefPtr<CSSValue> consumeSpeakAs(CSSParserTokenRange&);
RefPtr<CSSValue> consumeHangingPunctuation(CSSParserTokenRange&);
RefPtr<CSSValue> consumeContain(CSSParserTokenRange&);
RefPtr<CSSValue> consumeContainIntrinsicSize(CSSParserTokenRange&);
RefPtr<CSSValue> consumeTextEmphasisPosition(CSSParserTokenRange&);
#if ENABLE(DARK_MODE_CSS)
RefPtr<CSSValue> consumeColorScheme(CSSParserTokenRange&);
#endif
RefPtr<CSSValue> consumeOffsetRotate(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeTextSpacingTrim(CSSParserTokenRange&);
RefPtr<CSSValue> consumeTextAutospace(CSSParserTokenRange&);
RefPtr<CSSValue> consumeTextUnderlinePosition(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSPrimitiveValue> consumeAnchor(CSSParserTokenRange&, CSSParserMode);

RefPtr<CSSValue> consumeWebKitRubyPosition(CSSParserTokenRange&, const CSSParserContext&);

RefPtr<CSSValue> consumeDeclarationValue(CSSParserTokenRange&, const CSSParserContext&);


} // namespace CSSPropertyParserHelpers

} // namespace WebCore
