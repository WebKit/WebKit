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

#include "SVGPathSeg.h"

namespace WebCore {

template<class... Arguments>
class SVGPathSegValue : public SVGPathSeg {
public:
    template<typename PathSegType>
    static Ref<PathSegType> create(Arguments... arguments)
    {
        return adoptRef(*new PathSegType(std::forward<Arguments>(arguments)...));
    }

    template<typename PathSegType>
    Ref<PathSegType> clone() const
    {
        return adoptRef(*new PathSegType(m_arguments));
    }

    SVGPathSegValue(Arguments... arguments)
        : m_arguments(std::forward<Arguments>(arguments)...)
    {
    }

    SVGPathSegValue(const std::tuple<Arguments...>& arguments)
        : m_arguments(arguments)
    {
    }

protected:
    template<size_t I>
    const auto& argument() const
    {
        return std::get<I>(m_arguments);
    }

    template<size_t I, typename ArgumentValue>
    void setArgument(ArgumentValue value)
    {
        std::get<I>(m_arguments) = value;
        commitChange();
    }

    std::tuple<Arguments...> m_arguments;
};

class SVGPathSegLinetoHorizontal : public SVGPathSegValue<float> {
public:
    float x() const { return argument<0>(); }
    void setX(float x) { setArgument<0>(x); }

private:
    using SVGPathSegValue::SVGPathSegValue;
};

class SVGPathSegLinetoVertical : public SVGPathSegValue<float> {
public:
    float y() const { return argument<0>(); }
    void setY(float x) { setArgument<0>(x); }

private:
    using SVGPathSegValue::SVGPathSegValue;
};

class SVGPathSegSingleCoordinate : public SVGPathSegValue<float, float> {
public:
    float x() const { return argument<0>(); }
    void setX(float x) { setArgument<0>(x); }

    float y() const { return argument<1>(); }
    void setY(float y) { setArgument<1>(y); }

private:
    using SVGPathSegValue::SVGPathSegValue;
};

class SVGPathSegCurvetoQuadratic : public SVGPathSegValue<float, float, float, float> {
public:
    float x() const { return argument<0>(); }
    void setX(float x) { setArgument<0>(x); }

    float y() const { return argument<1>(); }
    void setY(float y) { setArgument<1>(y); }

    float x1() const { return argument<2>(); }
    void setX1(float x) { setArgument<2>(x); }

    float y1() const { return argument<3>(); }
    void setY1(float y) { setArgument<3>(y); }

private:
    using SVGPathSegValue::SVGPathSegValue;
};

class SVGPathSegCurvetoCubicSmooth : public SVGPathSegValue<float, float, float, float> {
public:
    float x() const { return argument<0>(); }
    void setX(float x) { setArgument<0>(x); }

    float y() const { return argument<1>(); }
    void setY(float y) { setArgument<1>(y); }

    float x2() const { return argument<2>(); }
    void setX2(float x) { setArgument<2>(x); }

    float y2() const { return argument<3>(); }
    void setY2(float y) { setArgument<3>(y); }

private:
    using SVGPathSegValue::SVGPathSegValue;
};

class SVGPathSegCurvetoCubic : public SVGPathSegValue<float, float, float, float, float, float> {
public:
    float x() const { return argument<0>(); }
    void setX(float x) { setArgument<0>(x); }

    float y() const { return argument<1>(); }
    void setY(float y) { setArgument<1>(y); }

    float x1() const { return argument<2>(); }
    void setX1(float x) { setArgument<2>(x); }

    float y1() const { return argument<3>(); }
    void setY1(float y) { setArgument<3>(y); }

    float x2() const { return argument<4>(); }
    void setX2(float x) { setArgument<4>(x); }

    float y2() const { return argument<5>(); }
    void setY2(float y) { setArgument<5>(y); }

private:
    using SVGPathSegValue::SVGPathSegValue;
};

class SVGPathSegArc : public SVGPathSegValue<float, float, float, float, float, bool, bool> {
public:
    float x() const { return argument<0>(); }
    void setX(float x) { setArgument<0>(x); }

    float y() const { return argument<1>(); }
    void setY(float y) { setArgument<1>(y); }

    float r1() const { return argument<2>(); }
    void setR1(float r1) { setArgument<2>(r1); }

    float r2() const { return argument<3>(); }
    void setR2(float r2) { setArgument<3>(r2); }

    float angle() const { return argument<4>(); }
    void setAngle(float angle) { setArgument<4>(angle); }

    bool largeArcFlag() const { return argument<5>(); }
    void setLargeArcFlag(bool largeArcFlag) { setArgument<5>(largeArcFlag); }

    bool sweepFlag() const { return argument<6>(); }
    void setSweepFlag(bool sweepFlag) { setArgument<6>(sweepFlag); }

private:
    using SVGPathSegValue::SVGPathSegValue;
};

}
