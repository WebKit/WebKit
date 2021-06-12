/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(INLINE_PATH_DATA)

#include "FloatPoint.h"
#include <wtf/Variant.h>

namespace WebCore {

struct LineData {
    FloatPoint start;
    FloatPoint end;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<LineData> decode(Decoder&);
};

struct ArcData {
    enum class Type : uint8_t {
        ArcOnly,
        LineAndArc,
        ClosedLineAndArc
    };

    FloatPoint start;
    FloatPoint center;
    float radius { 0 };
    float startAngle { 0 };
    float endAngle { 0 };
    bool clockwise { false };
    Type type { Type::ArcOnly };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<ArcData> decode(Decoder&);
};

struct MoveData {
    FloatPoint location;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<MoveData> decode(Decoder&);
};

struct QuadCurveData {
    FloatPoint startPoint;
    FloatPoint controlPoint;
    FloatPoint endPoint;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<QuadCurveData> decode(Decoder&);
};

struct BezierCurveData {
    FloatPoint startPoint;
    FloatPoint controlPoint1;
    FloatPoint controlPoint2;
    FloatPoint endPoint;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<BezierCurveData> decode(Decoder&);
};

template<class Encoder> void MoveData::encode(Encoder& encoder) const
{
    encoder << location;
}

template<class Decoder> std::optional<MoveData> MoveData::decode(Decoder& decoder)
{
    MoveData data;
    if (!decoder.decode(data.location))
        return std::nullopt;
    return data;
}

template<class Encoder> void LineData::encode(Encoder& encoder) const
{
    encoder << start;
    encoder << end;
}

template<class Decoder> std::optional<LineData> LineData::decode(Decoder& decoder)
{
    LineData data;
    if (!decoder.decode(data.start))
        return std::nullopt;

    if (!decoder.decode(data.end))
        return std::nullopt;

    return data;
}

template<class Encoder> void ArcData::encode(Encoder& encoder) const
{
    encoder << start;
    encoder << center;
    encoder << radius;
    encoder << startAngle;
    encoder << endAngle;
    encoder << clockwise;
    encoder << type;
}

template<class Decoder> std::optional<ArcData> ArcData::decode(Decoder& decoder)
{
    ArcData data;
    if (!decoder.decode(data.start))
        return std::nullopt;

    if (!decoder.decode(data.center))
        return std::nullopt;

    if (!decoder.decode(data.radius))
        return std::nullopt;

    if (!decoder.decode(data.startAngle))
        return std::nullopt;

    if (!decoder.decode(data.endAngle))
        return std::nullopt;

    if (!decoder.decode(data.clockwise))
        return std::nullopt;

    if (!decoder.decode(data.type))
        return std::nullopt;

    return data;
}

template<class Encoder> void QuadCurveData::encode(Encoder& encoder) const
{
    encoder << startPoint;
    encoder << controlPoint;
    encoder << endPoint;
}

template<class Decoder> std::optional<QuadCurveData> QuadCurveData::decode(Decoder& decoder)
{
    QuadCurveData data;
    if (!decoder.decode(data.startPoint))
        return std::nullopt;

    if (!decoder.decode(data.controlPoint))
        return std::nullopt;

    if (!decoder.decode(data.endPoint))
        return std::nullopt;

    return data;
}

template<class Encoder> void BezierCurveData::encode(Encoder& encoder) const
{
    encoder << startPoint;
    encoder << controlPoint1;
    encoder << controlPoint2;
    encoder << endPoint;
}

template<class Decoder> std::optional<BezierCurveData> BezierCurveData::decode(Decoder& decoder)
{
    BezierCurveData data;
    if (!decoder.decode(data.startPoint))
        return std::nullopt;

    if (!decoder.decode(data.controlPoint1))
        return std::nullopt;

    if (!decoder.decode(data.controlPoint2))
        return std::nullopt;

    if (!decoder.decode(data.endPoint))
        return std::nullopt;

    return data;
}

using InlinePathData = Variant<Monostate, MoveData, LineData, ArcData, QuadCurveData, BezierCurveData>;

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::ArcData::Type> {
    using values = EnumValues<
        WebCore::ArcData::Type,
        WebCore::ArcData::Type::ArcOnly,
        WebCore::ArcData::Type::LineAndArc,
        WebCore::ArcData::Type::ClosedLineAndArc
    >;
};

} // namespace WTF

#endif // ENABLE(INLINE_PATH_DATA)
