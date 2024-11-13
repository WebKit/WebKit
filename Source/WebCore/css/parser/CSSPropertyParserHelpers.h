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

#include <array>
#include <wtf/Forward.h>

namespace WebCore {

class CSSParserTokenRange;
class CSSPrimitiveValue;
class CSSShadowValue;
class CSSValue;

struct CSSParserContext;

enum CSSPropertyID : uint16_t;

// When these functions are successful, they will consume all the relevant
// tokens from the range and also consume any whitespace which follows. When
// the start of the range doesn't match the type we're looking for, the range
// will not be modified.
namespace CSSPropertyParserHelpers {

RefPtr<CSSPrimitiveValue> consumeSingleContainerName(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeAspectRatio(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeDisplay(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeWillChange(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeQuotes(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeSize(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeTextIndent(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeTextTransform(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeMarginSide(CSSParserTokenRange&, const CSSParserContext&, CSSPropertyID currentShorthand);
RefPtr<CSSValue> consumeMarginTrim(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeSide(CSSParserTokenRange&, const CSSParserContext&, CSSPropertyID currentShorthand);
RefPtr<CSSValue> consumeInsetLogicalStartEnd(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeTouchAction(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeKeyframesName(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeSingleTransitionPropertyOrNone(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeSingleTransitionProperty(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeTextShadow(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeTextDecorationLine(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeTextEmphasisStyle(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumePaintStroke(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumePaintOrder(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeStrokeDasharray(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeCursor(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeAttr(CSSParserTokenRange args, const CSSParserContext&);
RefPtr<CSSValue> consumeContent(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeScrollSnapAlign(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeScrollSnapType(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeScrollbarColor(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeScrollbarGutter(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeLineFitEdge(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeTextBoxEdge(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeLineBoxContain(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeContainerName(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeWebkitInitialLetter(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeSpeakAs(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeHangingPunctuation(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeContain(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeContainIntrinsicSize(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeTextEmphasisPosition(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeTextAutospace(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeTextUnderlinePosition(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeWebKitRubyPosition(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeDeclarationValue(CSSParserTokenRange&, const CSSParserContext&);

} // namespace CSSPropertyParserHelpers

} // namespace WebCore
