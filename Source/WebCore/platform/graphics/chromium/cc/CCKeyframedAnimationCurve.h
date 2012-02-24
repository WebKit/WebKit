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

#ifndef CCKeyframedAnimationCurve_h
#define CCKeyframedAnimationCurve_h

#include "TransformOperations.h"
#include "cc/CCAnimationCurve.h"

#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

struct CCFloatKeyframe {
    CCFloatKeyframe(double time, float value)
        : time(time)
        , value(value)
    {
    }

    double time;
    float value;
};

struct CCTransformKeyframe {
    explicit CCTransformKeyframe(double time)
        : time(time)
    {
    }

    CCTransformKeyframe(double time, const TransformOperations& value)
        : time(time)
        , value(value)
    {
    }

    double time;
    TransformOperations value;
};

class CCKeyframedFloatAnimationCurve : public CCFloatAnimationCurve {
public:
    // It is required that the keyframes be sorted by time.
    static PassOwnPtr<CCKeyframedFloatAnimationCurve> create(const Vector<CCFloatKeyframe>& keyframes);

    virtual ~CCKeyframedFloatAnimationCurve();

    // CCAnimationCurve implementation
    virtual double duration() const;
    virtual PassOwnPtr<CCAnimationCurve> clone() const;

    // CCFloatAnimationCurve implementation
    virtual float getValue(double t) const;

private:
    explicit CCKeyframedFloatAnimationCurve(const Vector<CCFloatKeyframe>&);

    // Always sorted in order of increasing time. No two keyframes have the
    // same time.
    Vector<CCFloatKeyframe> m_keyframes;
};

class CCKeyframedTransformAnimationCurve : public CCTransformAnimationCurve {
public:
    // It is required that the keyframes be sorted by time.
    static PassOwnPtr<CCKeyframedTransformAnimationCurve> create(const Vector<CCTransformKeyframe>& keyframes);

    virtual ~CCKeyframedTransformAnimationCurve();

    // CCAnimationCurve implementation
    virtual double duration() const;
    virtual PassOwnPtr<CCAnimationCurve> clone() const;

    // CCTransformAnimationCurve implementation
    virtual TransformationMatrix getValue(double t, const IntSize&) const;

private:
    explicit CCKeyframedTransformAnimationCurve(const Vector<CCTransformKeyframe>&);

    // Always sorted in order of increasing time. No two keyframes have the
    // same time.
    Vector<CCTransformKeyframe> m_keyframes;
};

} // namespace WebCore

#endif // CCKeyframedAnimationCurve_h
