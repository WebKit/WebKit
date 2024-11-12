/*
 * Copyright (C) 2024 Igalia S.L.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PositionArea.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

bool PositionArea::isAmbiguous(const CSSValueID id)
{
    return (
        id == CSSValueCenter
        || id == CSSValueStart
        || id == CSSValueEnd
        || id == CSSValueSpanStart
        || id == CSSValueSpanEnd
        || id == CSSValueSelfStart
        || id == CSSValueSelfEnd
        || id == CSSValueSpanSelfStart
        || id == CSSValueSpanSelfEnd
        || id == CSSValueSpanAll
    );
}

static TextStream& operator<<(TextStream& ts, const PositionArea::Cols& positionAreaCols)
{
    switch (positionAreaCols) {
    case PositionArea::Cols::Center:
        return ts << "center";
    case PositionArea::Cols::End:
        return ts << "end";
    case PositionArea::Cols::InlineEnd:
        return ts << "inline-end";
    case PositionArea::Cols::InlineStart:
        return ts << "inline-start";
    case PositionArea::Cols::Left:
        return ts << "left";
    case PositionArea::Cols::Right:
        return ts << "right";
    case PositionArea::Cols::SelfEnd:
        return ts << "self-end";
    case PositionArea::Cols::SelfInlineEnd:
        return ts << "self-inline-end";
    case PositionArea::Cols::SelfInlineStart:
        return ts << "self-inline-start";
    case PositionArea::Cols::SelfStart:
        return ts << "self-start";
    case PositionArea::Cols::SpanAll:
        return ts << "span-all";
    case PositionArea::Cols::SpanEnd:
        return ts << "span-end";
    case PositionArea::Cols::SpanInlineEnd:
        return ts << "span-inline-end";
    case PositionArea::Cols::SpanInlineStart:
        return ts << "span-inline-start";
    case PositionArea::Cols::SpanLeft:
        return ts << "span-left";
    case PositionArea::Cols::SpanRight:
        return ts << "span-right";
    case PositionArea::Cols::SpanSelfEnd:
        return ts << "span-self-end";
    case PositionArea::Cols::SpanSelfInlineEnd:
        return ts << "span-self-inline-end";
    case PositionArea::Cols::SpanSelfInlineStart:
        return ts << "span-self-inline-start";
    case PositionArea::Cols::SpanSelfStart:
        return ts << "span-self-start";
    case PositionArea::Cols::SpanStart:
        return ts << "span-start";
    case PositionArea::Cols::SpanXEnd:
        return ts << "span-x-end";
    case PositionArea::Cols::SpanXSelfEnd:
        return ts << "span-x-self-end";
    case PositionArea::Cols::SpanXSelfStart:
        return ts << "span-x-self-start";
    case PositionArea::Cols::SpanXStart:
        return ts << "span-x-start";
    case PositionArea::Cols::Start:
        return ts << "start";
    case PositionArea::Cols::XEnd:
        return ts << "x-end";
    case PositionArea::Cols::XSelfEnd:
        return ts << "x-self-end";
    case PositionArea::Cols::XSelfStart:
        return ts << "x-self-start";
    case PositionArea::Cols::XStart:
        return ts << "x-start";
    }
    ASSERT_NOT_REACHED();
    return ts;
}

static TextStream& operator<<(TextStream& ts, const PositionArea::Rows& positionAreaRows)
{
    switch (positionAreaRows) {
    case PositionArea::Rows::BlockEnd:
        return ts << "block-end";
    case PositionArea::Rows::BlockStart:
        return ts << "block-start";
    case PositionArea::Rows::Bottom:
        return ts << "bottom";
    case PositionArea::Rows::Center:
        return ts << "center";
    case PositionArea::Rows::End:
        return ts << "end";
    case PositionArea::Rows::SelfBlockEnd:
        return ts << "self-block-end";
    case PositionArea::Rows::SelfBlockStart:
        return ts << "self-block-start";
    case PositionArea::Rows::SelfEnd:
        return ts << "self-end";
    case PositionArea::Rows::SelfStart:
        return ts << "self-start";
    case PositionArea::Rows::SpanAll:
        return ts << "span-all";
    case PositionArea::Rows::SpanBlockEnd:
        return ts << "span-block-end";
    case PositionArea::Rows::SpanBlockStart:
        return ts << "span-block-start";
    case PositionArea::Rows::SpanBottom:
        return ts << "span-bottom";
    case PositionArea::Rows::SpanEnd:
        return ts << "span-end";
    case PositionArea::Rows::SpanSelfBlockEnd:
        return ts << "span-self-block-end";
    case PositionArea::Rows::SpanSelfBlockStart:
        return ts << "span-self-block-start";
    case PositionArea::Rows::SpanSelfEnd:
        return ts << "span-self-end";
    case PositionArea::Rows::SpanSelfStart:
        return ts << "span-self-start";
    case PositionArea::Rows::SpanStart:
        return ts << "span-start";
    case PositionArea::Rows::SpanTop:
        return ts << "span-top";
    case PositionArea::Rows::SpanYEnd:
        return ts << "span-y-end";
    case PositionArea::Rows::SpanYSelfEnd:
        return ts << "span-y-self-end";
    case PositionArea::Rows::SpanYSelfStart:
        return ts << "span-y-self-start";
    case PositionArea::Rows::SpanYStart:
        return ts << "span-y-start";
    case PositionArea::Rows::Start:
        return ts << "start";
    case PositionArea::Rows::Top:
        return ts << "top";
    case PositionArea::Rows::YEnd:
        return ts << "y-end";
    case PositionArea::Rows::YSelfEnd:
        return ts << "y-self-end";
    case PositionArea::Rows::YSelfStart:
        return ts << "y-self-start";
    case PositionArea::Rows::YStart:
        return ts << "y-start";
    }
    ASSERT_NOT_REACHED();
    return ts;
}

TextStream& operator<<(TextStream& ts, const PositionArea& postionArea)
{
    return ts << postionArea.cols << ' ' << postionArea.rows;
}

} // namespace WebCore
