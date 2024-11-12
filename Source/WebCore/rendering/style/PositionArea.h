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

#pragma once

namespace WebCore {

enum CSSValueID : uint16_t;

struct PositionArea {

    enum class Cols : uint8_t {
        Center,
        End,
        InlineEnd,
        InlineStart,
        Left,
        Right,
        SelfEnd,
        SelfInlineEnd,
        SelfInlineStart,
        SelfStart,
        SpanAll,
        SpanEnd,
        SpanInlineEnd,
        SpanInlineStart,
        SpanLeft,
        SpanRight,
        SpanSelfEnd,
        SpanSelfInlineEnd,
        SpanSelfInlineStart,
        SpanSelfStart,
        SpanStart,
        SpanXEnd,
        SpanXSelfEnd,
        SpanXSelfStart,
        SpanXStart,
        Start,
        XEnd,
        XSelfEnd,
        XSelfStart,
        XStart,
    };

    enum class Rows : uint8_t {
        BlockEnd,
        BlockStart,
        Bottom,
        Center,
        End,
        SelfBlockEnd,
        SelfBlockStart,
        SelfEnd,
        SelfStart,
        SpanAll,
        SpanBlockEnd,
        SpanBlockStart,
        SpanBottom,
        SpanEnd,
        SpanSelfBlockEnd,
        SpanSelfBlockStart,
        SpanSelfEnd,
        SpanSelfStart,
        SpanStart,
        SpanTop,
        SpanYEnd,
        SpanYSelfEnd,
        SpanYSelfStart,
        SpanYStart,
        Start,
        Top,
        YEnd,
        YSelfEnd,
        YSelfStart,
        YStart,
    };

    Cols cols;
    Rows rows;

    static bool isAmbiguous(const CSSValueID);

    friend bool operator==(const PositionArea&, const PositionArea&) = default;
};

WTF::TextStream& operator<<(WTF::TextStream&, const PositionArea&);

} // namespace WebCore
