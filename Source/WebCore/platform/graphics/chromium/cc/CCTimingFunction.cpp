/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "cc/CCTimingFunction.h"

#include <wtf/OwnPtr.h>

namespace {
const double epsilon = 1e-6;
} // namespace

namespace WebCore {

CCTimingFunction::CCTimingFunction()
{
}

CCTimingFunction::~CCTimingFunction()
{
}

double CCTimingFunction::duration() const
{
    return 1.0;
}

PassOwnPtr<CCCubicBezierTimingFunction> CCCubicBezierTimingFunction::create(double x1, double y1, double x2, double y2)
{
    return adoptPtr(new CCCubicBezierTimingFunction(x1, y1, x2, y2));
}

CCCubicBezierTimingFunction::CCCubicBezierTimingFunction(double x1, double y1, double x2, double y2)
    : m_curve(x1, y1, x2, y2)
{
}

CCCubicBezierTimingFunction::~CCCubicBezierTimingFunction()
{
}

float CCCubicBezierTimingFunction::getValue(double x) const
{
    UnitBezier temp(m_curve);
    return static_cast<float>(temp.solve(x, epsilon));
}

PassOwnPtr<CCAnimationCurve> CCCubicBezierTimingFunction::clone() const
{
    return adoptPtr(new CCCubicBezierTimingFunction(*this));
}

// These numbers come from http://www.w3.org/TR/css3-transitions/#transition-timing-function_tag.
PassOwnPtr<CCTimingFunction> CCEaseTimingFunction::create()
{
    return CCCubicBezierTimingFunction::create(0.25, 0.1, 0.25, 1);
}

PassOwnPtr<CCTimingFunction> CCEaseInTimingFunction::create()
{
    return CCCubicBezierTimingFunction::create(0.42, 0, 1.0, 1);
}

PassOwnPtr<CCTimingFunction> CCEaseOutTimingFunction::create()
{
    return CCCubicBezierTimingFunction::create(0, 0, 0.58, 1);
}

PassOwnPtr<CCTimingFunction> CCEaseInOutTimingFunction::create()
{
    return CCCubicBezierTimingFunction::create(0.42, 0, 0.58, 1);
}

} // namespace WebCore
