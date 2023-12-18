/*
 * Copyright (C) 2019-2023 Apple Inc.  All rights reserved.
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

#include "SVGPathSeg.h"
#include "SVGPathSegValue.h"

namespace WebCore {

class SVGPathSegClosePath final : public SVGPathSeg {
public:
    static Ref<SVGPathSegClosePath> create() { return adoptRef(*new SVGPathSegClosePath()); }
private:
    using SVGPathSeg::SVGPathSeg;
    SVGPathSegType pathSegType() const final { return SVGPathSegType::ClosePath; }
    String pathSegTypeAsLetter() const final { return "Z"_s; }
    Ref<SVGPathSeg> clone() const final { return adoptRef(*new SVGPathSegClosePath()); }
};

class SVGPathSegLinetoHorizontalAbs final : public SVGPathSegLinetoHorizontal {
public:
    static constexpr auto create = SVGPathSegValue::create<SVGPathSegLinetoHorizontalAbs>;
private:
    using SVGPathSegLinetoHorizontal::SVGPathSegLinetoHorizontal;
    SVGPathSegType pathSegType() const final { return SVGPathSegType::LineToHorizontalAbs; }
    String pathSegTypeAsLetter() const final { return "H"_s; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::cloneInternal<SVGPathSegLinetoHorizontalAbs>(); }
};

class SVGPathSegLinetoHorizontalRel final : public SVGPathSegLinetoHorizontal {
public:
    static constexpr auto create = SVGPathSegValue::create<SVGPathSegLinetoHorizontalRel>;
private:
    using SVGPathSegLinetoHorizontal::SVGPathSegLinetoHorizontal;
    SVGPathSegType pathSegType() const final { return SVGPathSegType::LineToHorizontalRel; }
    String pathSegTypeAsLetter() const final { return "h"_s; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::cloneInternal<SVGPathSegLinetoHorizontalRel>(); }
};

class SVGPathSegLinetoVerticalAbs final : public SVGPathSegLinetoVertical {
public:
    static constexpr auto create = SVGPathSegValue::create<SVGPathSegLinetoVerticalAbs>;
private:
    using SVGPathSegLinetoVertical::SVGPathSegLinetoVertical;
    SVGPathSegType pathSegType() const final { return SVGPathSegType::LineToVerticalAbs; }
    String pathSegTypeAsLetter() const final { return "V"_s; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::cloneInternal<SVGPathSegLinetoVerticalAbs>(); }
};

class SVGPathSegLinetoVerticalRel final : public SVGPathSegLinetoVertical {
public:
    static constexpr auto create = SVGPathSegValue::create<SVGPathSegLinetoVerticalRel>;
private:
    using SVGPathSegLinetoVertical::SVGPathSegLinetoVertical;
    SVGPathSegType pathSegType() const final { return SVGPathSegType::LineToVerticalRel; }
    String pathSegTypeAsLetter() const final { return "v"_s; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::cloneInternal<SVGPathSegLinetoVerticalRel>(); }
};

class SVGPathSegMovetoAbs final : public SVGPathSegSingleCoordinate {
public:
    static constexpr auto create = SVGPathSegValue::create<SVGPathSegMovetoAbs>;
private:
    using SVGPathSegSingleCoordinate::SVGPathSegSingleCoordinate;
    SVGPathSegType pathSegType() const final { return SVGPathSegType::MoveToAbs; }
    String pathSegTypeAsLetter() const final { return "M"_s; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::cloneInternal<SVGPathSegMovetoAbs>(); }
};

class SVGPathSegMovetoRel final : public SVGPathSegSingleCoordinate {
public:
    static constexpr auto create = SVGPathSegValue::create<SVGPathSegMovetoRel>;
private:
    using SVGPathSegSingleCoordinate::SVGPathSegSingleCoordinate;
    SVGPathSegType pathSegType() const final { return SVGPathSegType::MoveToRel; }
    String pathSegTypeAsLetter() const final { return "m"_s; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::cloneInternal<SVGPathSegMovetoRel>(); }
};

class SVGPathSegLinetoAbs final : public SVGPathSegSingleCoordinate {
public:
    static constexpr auto create = SVGPathSegValue::create<SVGPathSegLinetoAbs>;
private:
    using SVGPathSegSingleCoordinate::SVGPathSegSingleCoordinate;
    SVGPathSegType pathSegType() const final { return SVGPathSegType::LineToAbs; }
    String pathSegTypeAsLetter() const final { return "L"_s; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::cloneInternal<SVGPathSegLinetoAbs>(); }
};

class SVGPathSegLinetoRel final : public SVGPathSegSingleCoordinate {
public:
    static constexpr auto create = SVGPathSegValue::create<SVGPathSegLinetoRel>;
private:
    using SVGPathSegSingleCoordinate::SVGPathSegSingleCoordinate;
    SVGPathSegType pathSegType() const final { return SVGPathSegType::LineToRel; }
    String pathSegTypeAsLetter() const final { return "l"_s; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::cloneInternal<SVGPathSegLinetoRel>(); }
};

class SVGPathSegCurvetoQuadraticAbs final : public SVGPathSegCurvetoQuadratic {
public:
    static constexpr auto create = SVGPathSegValue::create<SVGPathSegCurvetoQuadraticAbs>;
private:
    using SVGPathSegCurvetoQuadratic::SVGPathSegCurvetoQuadratic;
    SVGPathSegType pathSegType() const final { return SVGPathSegType::CurveToQuadraticAbs; }
    String pathSegTypeAsLetter() const final { return "Q"_s; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::cloneInternal<SVGPathSegCurvetoQuadraticAbs>(); }
};

class SVGPathSegCurvetoQuadraticRel final : public SVGPathSegCurvetoQuadratic {
public:
    static constexpr auto create = SVGPathSegValue::create<SVGPathSegCurvetoQuadraticRel>;
private:
    using SVGPathSegCurvetoQuadratic::SVGPathSegCurvetoQuadratic;
    SVGPathSegType pathSegType() const final { return SVGPathSegType::CurveToQuadraticRel; }
    String pathSegTypeAsLetter() const final { return "q"_s; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::cloneInternal<SVGPathSegCurvetoQuadraticRel>(); }
};

class SVGPathSegCurvetoCubicAbs final : public SVGPathSegCurvetoCubic {
public:
    static constexpr auto create = SVGPathSegValue::create<SVGPathSegCurvetoCubicAbs>;
private:
    using SVGPathSegCurvetoCubic::SVGPathSegCurvetoCubic;
    SVGPathSegType pathSegType() const final { return SVGPathSegType::CurveToCubicAbs; }
    String pathSegTypeAsLetter() const final { return "C"_s; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::cloneInternal<SVGPathSegCurvetoCubicAbs>(); }
};

class SVGPathSegCurvetoCubicRel final : public SVGPathSegCurvetoCubic {
public:
    static constexpr auto create = SVGPathSegValue::create<SVGPathSegCurvetoCubicRel>;
private:
    using SVGPathSegCurvetoCubic::SVGPathSegCurvetoCubic;
    SVGPathSegType pathSegType() const final { return SVGPathSegType::CurveToCubicRel; }
    String pathSegTypeAsLetter() const final { return "c"_s; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::cloneInternal<SVGPathSegCurvetoCubicRel>(); }
};

class SVGPathSegArcAbs final : public SVGPathSegArc {
public:
    static constexpr auto create = SVGPathSegValue::create<SVGPathSegArcAbs>;
private:
    using SVGPathSegArc::SVGPathSegArc;
    SVGPathSegType pathSegType() const final { return SVGPathSegType::ArcAbs; }
    String pathSegTypeAsLetter() const final { return "A"_s; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::cloneInternal<SVGPathSegArcAbs>(); }
};

class SVGPathSegArcRel final : public SVGPathSegArc {
public:
    static constexpr auto create = SVGPathSegValue::create<SVGPathSegArcRel>;
private:
    using SVGPathSegArc::SVGPathSegArc;
    SVGPathSegType pathSegType() const final { return SVGPathSegType::ArcRel; }
    String pathSegTypeAsLetter() const final { return "a"_s; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::cloneInternal<SVGPathSegArcRel>(); }
};

class SVGPathSegCurvetoQuadraticSmoothAbs final : public SVGPathSegSingleCoordinate {
public:
    static constexpr auto create = SVGPathSegValue::create<SVGPathSegCurvetoQuadraticSmoothAbs>;
private:
    using SVGPathSegSingleCoordinate::SVGPathSegSingleCoordinate;
    SVGPathSegType pathSegType() const final { return SVGPathSegType::CurveToQuadraticSmoothAbs; }
    String pathSegTypeAsLetter() const final { return "T"_s; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::cloneInternal<SVGPathSegCurvetoQuadraticSmoothAbs>(); }
};

class SVGPathSegCurvetoQuadraticSmoothRel final : public SVGPathSegSingleCoordinate {
public:
    static constexpr auto create = SVGPathSegValue::create<SVGPathSegCurvetoQuadraticSmoothRel>;
private:
    using SVGPathSegSingleCoordinate::SVGPathSegSingleCoordinate;
    SVGPathSegType pathSegType() const final { return SVGPathSegType::CurveToQuadraticSmoothRel; }
    String pathSegTypeAsLetter() const final { return "t"_s; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::cloneInternal<SVGPathSegCurvetoQuadraticSmoothRel>(); }
};

class SVGPathSegCurvetoCubicSmoothAbs final : public SVGPathSegCurvetoCubicSmooth {
public:
    static constexpr auto create = SVGPathSegValue::create<SVGPathSegCurvetoCubicSmoothAbs>;
private:
    using SVGPathSegCurvetoCubicSmooth::SVGPathSegCurvetoCubicSmooth;
    SVGPathSegType pathSegType() const final { return SVGPathSegType::CurveToCubicSmoothAbs; }
    String pathSegTypeAsLetter() const final { return "S"_s; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::cloneInternal<SVGPathSegCurvetoCubicSmoothAbs>(); }
};

class SVGPathSegCurvetoCubicSmoothRel final : public SVGPathSegCurvetoCubicSmooth {
public:
    static constexpr auto create = SVGPathSegValue::create<SVGPathSegCurvetoCubicSmoothRel>;
private:
    using SVGPathSegCurvetoCubicSmooth::SVGPathSegCurvetoCubicSmooth;
    SVGPathSegType pathSegType() const final { return SVGPathSegType::CurveToCubicSmoothRel; }
    String pathSegTypeAsLetter() const final { return "s"_s; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::cloneInternal<SVGPathSegCurvetoCubicSmoothRel>(); }
};

} // namespace WebCore
