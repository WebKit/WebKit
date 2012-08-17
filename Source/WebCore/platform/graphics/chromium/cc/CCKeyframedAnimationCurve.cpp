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

#include "CCKeyframedAnimationCurve.h"

#include <wtf/OwnPtr.h>

using WebKit::WebTransformationMatrix;

namespace WebCore {

namespace {

template <class Keyframe>
void insertKeyframe(PassOwnPtr<Keyframe> popKeyframe, Vector<OwnPtr<Keyframe> >& keyframes)
{
    OwnPtr<Keyframe> keyframe = popKeyframe;

    // Usually, the keyframes will be added in order, so this loop would be unnecessary and
    // we should skip it if possible.
    if (!keyframes.isEmpty() && keyframe->time() < keyframes.last()->time()) {
        for (size_t i = 0; i < keyframes.size(); ++i) {
            if (keyframe->time() < keyframes[i]->time()) {
                keyframes.insert(i, keyframe.release());
                return;
            }
        }
    }

    keyframes.append(keyframe.release());
}

PassOwnPtr<CCTimingFunction> cloneTimingFunction(const CCTimingFunction* timingFunction)
{
    ASSERT(timingFunction);
    OwnPtr<CCAnimationCurve> curve(timingFunction->clone());
    return adoptPtr(static_cast<CCTimingFunction*>(curve.leakPtr()));
}

} // namespace

CCKeyframe::CCKeyframe(double time, PassOwnPtr<CCTimingFunction> timingFunction)
    : m_time(time)
    , m_timingFunction(timingFunction)
{
}

CCKeyframe::~CCKeyframe()
{
}

double CCKeyframe::time() const
{
    return m_time;
}

const CCTimingFunction* CCKeyframe::timingFunction() const
{
    return m_timingFunction.get();
}

PassOwnPtr<CCFloatKeyframe> CCFloatKeyframe::create(double time, float value, PassOwnPtr<CCTimingFunction> timingFunction)
{
    return adoptPtr(new CCFloatKeyframe(time, value, timingFunction));
}

CCFloatKeyframe::CCFloatKeyframe(double time, float value, PassOwnPtr<CCTimingFunction> timingFunction)
    : CCKeyframe(time, timingFunction)
    , m_value(value)
{
}

CCFloatKeyframe::~CCFloatKeyframe()
{
}

float CCFloatKeyframe::value() const
{
    return m_value;
}

PassOwnPtr<CCFloatKeyframe> CCFloatKeyframe::clone() const
{
    return CCFloatKeyframe::create(time(), value(), timingFunction() ? cloneTimingFunction(timingFunction()) : nullptr);
}

PassOwnPtr<CCTransformKeyframe> CCTransformKeyframe::create(double time, const WebKit::WebTransformOperations& value, PassOwnPtr<CCTimingFunction> timingFunction)
{
    return adoptPtr(new CCTransformKeyframe(time, value, timingFunction));
}

CCTransformKeyframe::CCTransformKeyframe(double time, const WebKit::WebTransformOperations& value, PassOwnPtr<CCTimingFunction> timingFunction)
    : CCKeyframe(time, timingFunction)
    , m_value(value)
{
}

CCTransformKeyframe::~CCTransformKeyframe()
{
}

const WebKit::WebTransformOperations& CCTransformKeyframe::value() const
{
    return m_value;
}

PassOwnPtr<CCTransformKeyframe> CCTransformKeyframe::clone() const
{
    return CCTransformKeyframe::create(time(), value(), timingFunction() ? cloneTimingFunction(timingFunction()) : nullptr);
}

PassOwnPtr<CCKeyframedFloatAnimationCurve> CCKeyframedFloatAnimationCurve::create()
{
    return adoptPtr(new CCKeyframedFloatAnimationCurve);
}

CCKeyframedFloatAnimationCurve::CCKeyframedFloatAnimationCurve()
{
}

CCKeyframedFloatAnimationCurve::~CCKeyframedFloatAnimationCurve()
{
}

void CCKeyframedFloatAnimationCurve::addKeyframe(PassOwnPtr<CCFloatKeyframe> keyframe)
{
    insertKeyframe(keyframe, m_keyframes);
}

double CCKeyframedFloatAnimationCurve::duration() const
{
    return m_keyframes.last()->time() - m_keyframes.first()->time();
}

PassOwnPtr<CCAnimationCurve> CCKeyframedFloatAnimationCurve::clone() const
{
    OwnPtr<CCKeyframedFloatAnimationCurve> toReturn(CCKeyframedFloatAnimationCurve::create());
    for (size_t i = 0; i < m_keyframes.size(); ++i)
        toReturn->addKeyframe(m_keyframes[i]->clone());
    return toReturn.release();
}

float CCKeyframedFloatAnimationCurve::getValue(double t) const
{
    if (t <= m_keyframes.first()->time())
        return m_keyframes.first()->value();

    if (t >= m_keyframes.last()->time())
        return m_keyframes.last()->value();

    size_t i = 0;
    for (; i < m_keyframes.size() - 1; ++i) {
        if (t < m_keyframes[i+1]->time())
            break;
    }

    float progress = static_cast<float>((t - m_keyframes[i]->time()) / (m_keyframes[i+1]->time() - m_keyframes[i]->time()));

    if (m_keyframes[i]->timingFunction())
        progress = m_keyframes[i]->timingFunction()->getValue(progress);

    return m_keyframes[i]->value() + (m_keyframes[i+1]->value() - m_keyframes[i]->value()) * progress;
}

PassOwnPtr<CCKeyframedTransformAnimationCurve> CCKeyframedTransformAnimationCurve::create()
{
    return adoptPtr(new CCKeyframedTransformAnimationCurve);
}

CCKeyframedTransformAnimationCurve::CCKeyframedTransformAnimationCurve()
{
}

CCKeyframedTransformAnimationCurve::~CCKeyframedTransformAnimationCurve()
{
}

void CCKeyframedTransformAnimationCurve::addKeyframe(PassOwnPtr<CCTransformKeyframe> keyframe)
{
    insertKeyframe(keyframe, m_keyframes);
}

double CCKeyframedTransformAnimationCurve::duration() const
{
    return m_keyframes.last()->time() - m_keyframes.first()->time();
}

PassOwnPtr<CCAnimationCurve> CCKeyframedTransformAnimationCurve::clone() const
{
    OwnPtr<CCKeyframedTransformAnimationCurve> toReturn(CCKeyframedTransformAnimationCurve::create());
    for (size_t i = 0; i < m_keyframes.size(); ++i)
        toReturn->addKeyframe(m_keyframes[i]->clone());
    return toReturn.release();
}

WebTransformationMatrix CCKeyframedTransformAnimationCurve::getValue(double t) const
{
    if (t <= m_keyframes.first()->time())
        return m_keyframes.first()->value().apply();

    if (t >= m_keyframes.last()->time())
        return m_keyframes.last()->value().apply();

    size_t i = 0;
    for (; i < m_keyframes.size() - 1; ++i) {
        if (t < m_keyframes[i+1]->time())
            break;
    }

    double progress = (t - m_keyframes[i]->time()) / (m_keyframes[i+1]->time() - m_keyframes[i]->time());

    if (m_keyframes[i]->timingFunction())
        progress = m_keyframes[i]->timingFunction()->getValue(progress);

    return m_keyframes[i+1]->value().blend(m_keyframes[i]->value(), progress);
}

} // namespace WebCore
