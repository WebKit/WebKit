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

#ifndef CCAnimationCurve_h
#define CCAnimationCurve_h

namespace WebCore {

class CCFloatAnimationCurve;
class CCTransformAnimationCurve;
class TransformOperations;

// An animation curve is a function that returns a value given a time.
// There are currently only two types of curve, float and transform.
class CCAnimationCurve {
public:
    enum Type { Float, Transform };

    virtual ~CCAnimationCurve() { }

    virtual double duration() const = 0;
    virtual Type type() const = 0;

    const CCFloatAnimationCurve* toFloatAnimationCurve() const;
    const CCTransformAnimationCurve* toTransformAnimationCurve() const;
};

class CCFloatAnimationCurve : public CCAnimationCurve {
public:
    virtual ~CCFloatAnimationCurve() { }

    virtual float getValue(double t) const = 0;

    // Partial CCAnimation implementation.
    virtual Type type() const { return Float; }
};

class CCTransformAnimationCurve : public CCAnimationCurve {
public:
    virtual ~CCTransformAnimationCurve() { }

    virtual TransformOperations getValue(double t) const = 0;

    // Partial CCAnimation implementation.
    virtual Type type() const { return Transform; }
};

} // namespace WebCore

#endif // CCAnimation_h
