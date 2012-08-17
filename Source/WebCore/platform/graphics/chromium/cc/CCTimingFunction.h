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

#ifndef CCTimingFunction_h
#define CCTimingFunction_h

#include "CCAnimationCurve.h"
#include "UnitBezier.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

// See http://www.w3.org/TR/css3-transitions/.
class CCTimingFunction : public CCFloatAnimationCurve {
public:
    virtual ~CCTimingFunction();

    // Partial implementation of CCFloatAnimationCurve.
    virtual double duration() const OVERRIDE;

protected:
    CCTimingFunction();
};

class CCCubicBezierTimingFunction : public CCTimingFunction {
public:
    static PassOwnPtr<CCCubicBezierTimingFunction> create(double x1, double y1, double x2, double y2);
    virtual ~CCCubicBezierTimingFunction();

    // Partial implementation of CCFloatAnimationCurve.
    virtual float getValue(double time) const OVERRIDE;
    virtual PassOwnPtr<CCAnimationCurve> clone() const OVERRIDE;

protected:
    CCCubicBezierTimingFunction(double x1, double y1, double x2, double y2);

    UnitBezier m_curve;
};

class CCEaseTimingFunction {
public:
    static PassOwnPtr<CCTimingFunction> create();
};

class CCEaseInTimingFunction {
public:
    static PassOwnPtr<CCTimingFunction> create();
};

class CCEaseOutTimingFunction {
public:
    static PassOwnPtr<CCTimingFunction> create();
};

class CCEaseInOutTimingFunction {
public:
    static PassOwnPtr<CCTimingFunction> create();
};

} // namespace WebCore

#endif // CCTimingFunction_h

