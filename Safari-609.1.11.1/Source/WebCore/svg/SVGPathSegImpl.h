/*
 * Copyright (C) 2019 Apple Inc.  All rights reserved.
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

#include "SVGPathSegValue.h"

namespace WebCore {

class SVGPathSegClosePath final : public SVGPathSeg {
public:
    static Ref<SVGPathSegClosePath> create() { return adoptRef(*new SVGPathSegClosePath()); }
private:
    using SVGPathSeg::SVGPathSeg;
    unsigned short pathSegType() const final { return PATHSEG_CLOSEPATH; }
    String pathSegTypeAsLetter() const final { return "Z"; }
    Ref<SVGPathSeg> clone() const final { return adoptRef(*new SVGPathSegClosePath()); }
};

class SVGPathSegLinetoHorizontalAbs final : public SVGPathSegLinetoHorizontal {
public:
    constexpr static auto create = SVGPathSegValue::create<SVGPathSegLinetoHorizontalAbs>;
private:
    using SVGPathSegLinetoHorizontal::SVGPathSegLinetoHorizontal;
    unsigned short pathSegType() const final { return PATHSEG_LINETO_HORIZONTAL_ABS; }
    String pathSegTypeAsLetter() const final { return "H"; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::clone<SVGPathSegLinetoHorizontalAbs>(); }
};

class SVGPathSegLinetoHorizontalRel final : public SVGPathSegLinetoHorizontal {
public:
    constexpr static auto create = SVGPathSegValue::create<SVGPathSegLinetoHorizontalRel>;
private:
    using SVGPathSegLinetoHorizontal::SVGPathSegLinetoHorizontal;
    unsigned short pathSegType() const final { return PATHSEG_LINETO_HORIZONTAL_REL; }
    String pathSegTypeAsLetter() const final { return "h"; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::clone<SVGPathSegLinetoHorizontalRel>(); }
};

class SVGPathSegLinetoVerticalAbs final : public SVGPathSegLinetoVertical {
public:
    constexpr static auto create = SVGPathSegValue::create<SVGPathSegLinetoVerticalAbs>;
private:
    using SVGPathSegLinetoVertical::SVGPathSegLinetoVertical;
    unsigned short pathSegType() const final { return PATHSEG_LINETO_VERTICAL_ABS; }
    String pathSegTypeAsLetter() const final { return "V"; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::clone<SVGPathSegLinetoVerticalAbs>(); }
};

class SVGPathSegLinetoVerticalRel final : public SVGPathSegLinetoVertical {
public:
    constexpr static auto create = SVGPathSegValue::create<SVGPathSegLinetoVerticalRel>;
private:
    using SVGPathSegLinetoVertical::SVGPathSegLinetoVertical;
    unsigned short pathSegType() const final { return PATHSEG_LINETO_VERTICAL_REL; }
    String pathSegTypeAsLetter() const final { return "v"; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::clone<SVGPathSegLinetoVerticalRel>(); }
};

class SVGPathSegMovetoAbs final : public SVGPathSegSingleCoordinate {
public:
    constexpr static auto create = SVGPathSegValue::create<SVGPathSegMovetoAbs>;
private:
    using SVGPathSegSingleCoordinate::SVGPathSegSingleCoordinate;
    unsigned short pathSegType() const final { return PATHSEG_MOVETO_ABS; }
    String pathSegTypeAsLetter() const final { return "M"; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::clone<SVGPathSegMovetoAbs>(); }
};

class SVGPathSegMovetoRel final : public SVGPathSegSingleCoordinate {
public:
    constexpr static auto create = SVGPathSegValue::create<SVGPathSegMovetoRel>;
private:
    using SVGPathSegSingleCoordinate::SVGPathSegSingleCoordinate;
    unsigned short pathSegType() const final { return PATHSEG_MOVETO_REL; }
    String pathSegTypeAsLetter() const final { return "m"; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::clone<SVGPathSegMovetoRel>(); }
};

class SVGPathSegLinetoAbs final : public SVGPathSegSingleCoordinate {
public:
    constexpr static auto create = SVGPathSegValue::create<SVGPathSegLinetoAbs>;
private:
    using SVGPathSegSingleCoordinate::SVGPathSegSingleCoordinate;
    unsigned short pathSegType() const final { return PATHSEG_LINETO_ABS; }
    String pathSegTypeAsLetter() const final { return "L"; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::clone<SVGPathSegLinetoAbs>(); }
};

class SVGPathSegLinetoRel final : public SVGPathSegSingleCoordinate {
public:
    constexpr static auto create = SVGPathSegValue::create<SVGPathSegLinetoRel>;
private:
    using SVGPathSegSingleCoordinate::SVGPathSegSingleCoordinate;
    unsigned short pathSegType() const final { return PATHSEG_LINETO_REL; }
    String pathSegTypeAsLetter() const final { return "l"; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::clone<SVGPathSegLinetoRel>(); }
};

class SVGPathSegCurvetoQuadraticAbs final : public SVGPathSegCurvetoQuadratic {
public:
    constexpr static auto create = SVGPathSegValue::create<SVGPathSegCurvetoQuadraticAbs>;
private:
    using SVGPathSegCurvetoQuadratic::SVGPathSegCurvetoQuadratic;
    unsigned short pathSegType() const final { return PATHSEG_CURVETO_QUADRATIC_ABS; }
    String pathSegTypeAsLetter() const final { return "Q"; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::clone<SVGPathSegCurvetoQuadraticAbs>(); }
};

class SVGPathSegCurvetoQuadraticRel final : public SVGPathSegCurvetoQuadratic {
public:
    constexpr static auto create = SVGPathSegValue::create<SVGPathSegCurvetoQuadraticRel>;
private:
    using SVGPathSegCurvetoQuadratic::SVGPathSegCurvetoQuadratic;
    unsigned short pathSegType() const final { return PATHSEG_CURVETO_QUADRATIC_REL; }
    String pathSegTypeAsLetter() const final { return "q"; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::clone<SVGPathSegCurvetoQuadraticRel>(); }
};

class SVGPathSegCurvetoCubicAbs final : public SVGPathSegCurvetoCubic {
public:
    constexpr static auto create = SVGPathSegValue::create<SVGPathSegCurvetoCubicAbs>;
private:
    using SVGPathSegCurvetoCubic::SVGPathSegCurvetoCubic;
    unsigned short pathSegType() const final { return PATHSEG_CURVETO_CUBIC_ABS; }
    String pathSegTypeAsLetter() const final { return "C"; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::clone<SVGPathSegCurvetoCubicAbs>(); }
};

class SVGPathSegCurvetoCubicRel final : public SVGPathSegCurvetoCubic {
public:
    constexpr static auto create = SVGPathSegValue::create<SVGPathSegCurvetoCubicRel>;
private:
    using SVGPathSegCurvetoCubic::SVGPathSegCurvetoCubic;
    unsigned short pathSegType() const final { return PATHSEG_CURVETO_CUBIC_REL; }
    String pathSegTypeAsLetter() const final { return "c"; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::clone<SVGPathSegCurvetoCubicRel>(); }
};

class SVGPathSegArcAbs final : public SVGPathSegArc {
public:
    constexpr static auto create = SVGPathSegValue::create<SVGPathSegArcAbs>;
private:
    using SVGPathSegArc::SVGPathSegArc;
    unsigned short pathSegType() const final { return PATHSEG_ARC_ABS; }
    String pathSegTypeAsLetter() const final { return "A"; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::clone<SVGPathSegArcAbs>(); }
};

class SVGPathSegArcRel final : public SVGPathSegArc {
public:
    constexpr static auto create = SVGPathSegValue::create<SVGPathSegArcRel>;
private:
    using SVGPathSegArc::SVGPathSegArc;
    unsigned short pathSegType() const final { return PATHSEG_ARC_REL; }
    String pathSegTypeAsLetter() const final { return "a"; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::clone<SVGPathSegArcRel>(); }
};

class SVGPathSegCurvetoQuadraticSmoothAbs final : public SVGPathSegSingleCoordinate {
public:
    constexpr static auto create = SVGPathSegValue::create<SVGPathSegCurvetoQuadraticSmoothAbs>;
private:
    using SVGPathSegSingleCoordinate::SVGPathSegSingleCoordinate;
    unsigned short pathSegType() const final { return PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS; }
    String pathSegTypeAsLetter() const final { return "T"; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::clone<SVGPathSegCurvetoQuadraticSmoothAbs>(); }
};

class SVGPathSegCurvetoQuadraticSmoothRel final : public SVGPathSegSingleCoordinate {
public:
    constexpr static auto create = SVGPathSegValue::create<SVGPathSegCurvetoQuadraticSmoothRel>;
private:
    using SVGPathSegSingleCoordinate::SVGPathSegSingleCoordinate;
    unsigned short pathSegType() const final { return PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL; }
    String pathSegTypeAsLetter() const final { return "t"; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::clone<SVGPathSegCurvetoQuadraticSmoothRel>(); }
};

class SVGPathSegCurvetoCubicSmoothAbs final : public SVGPathSegCurvetoCubicSmooth {
public:
    constexpr static auto create = SVGPathSegValue::create<SVGPathSegCurvetoCubicSmoothAbs>;
private:
    using SVGPathSegCurvetoCubicSmooth::SVGPathSegCurvetoCubicSmooth;
    unsigned short pathSegType() const final { return PATHSEG_CURVETO_CUBIC_SMOOTH_ABS; }
    String pathSegTypeAsLetter() const final { return "S"; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::clone<SVGPathSegCurvetoCubicSmoothAbs>(); }
};

class SVGPathSegCurvetoCubicSmoothRel final : public SVGPathSegCurvetoCubicSmooth {
public:
    constexpr static auto create = SVGPathSegValue::create<SVGPathSegCurvetoCubicSmoothRel>;
private:
    using SVGPathSegCurvetoCubicSmooth::SVGPathSegCurvetoCubicSmooth;
    unsigned short pathSegType() const final { return PATHSEG_CURVETO_CUBIC_SMOOTH_REL; }
    String pathSegTypeAsLetter() const final { return "s"; }
    Ref<SVGPathSeg> clone() const final { return SVGPathSegValue::clone<SVGPathSegCurvetoCubicSmoothRel>(); }
};

}
