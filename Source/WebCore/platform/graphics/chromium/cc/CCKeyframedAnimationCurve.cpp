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

#include "cc/CCKeyframedAnimationCurve.h"

#include "IdentityTransformOperation.h"
#include "Matrix3DTransformOperation.h"
#include "MatrixTransformOperation.h"
#include "PerspectiveTransformOperation.h"
#include "RotateTransformOperation.h"
#include "ScaleTransformOperation.h"
#include "SkewTransformOperation.h"
#include "TranslateTransformOperation.h"

#include <wtf/OwnPtr.h>

namespace WebCore {

namespace {

template <class Keyframe>
bool keyframesAreSorted(const Vector<Keyframe>& keyframes)
{
    if (!keyframes.size())
        return true;

    for (size_t i = 0; i < keyframes.size() - 1; ++i) {
        if (keyframes[i].time > keyframes[i+1].time)
            return false;
    }

    return true;
}

} // namespace

PassOwnPtr<CCKeyframedFloatAnimationCurve> CCKeyframedFloatAnimationCurve::create(const Vector<CCFloatKeyframe>& keyframes)
{
    if (!keyframes.size() || !keyframesAreSorted(keyframes))
        return nullptr;

    return adoptPtr(new CCKeyframedFloatAnimationCurve(keyframes));
}

CCKeyframedFloatAnimationCurve::CCKeyframedFloatAnimationCurve(const Vector<CCFloatKeyframe>& keyframes)
    : m_keyframes(keyframes)
{
}

CCKeyframedFloatAnimationCurve::~CCKeyframedFloatAnimationCurve()
{
}

double CCKeyframedFloatAnimationCurve::duration() const
{
    return m_keyframes.last().time - m_keyframes.first().time;
}

PassOwnPtr<CCAnimationCurve> CCKeyframedFloatAnimationCurve::clone() const
{
    return adoptPtr(new CCKeyframedFloatAnimationCurve(*this));
}

float CCKeyframedFloatAnimationCurve::getValue(double t) const
{
    if (t <= m_keyframes.first().time)
        return m_keyframes.first().value;

    if (t >= m_keyframes.last().time)
        return m_keyframes.last().value;

    size_t i = 0;
    for (; i < m_keyframes.size() - 1; ++i) {
        if (t < m_keyframes[i+1].time)
            break;
    }

    float progress = static_cast<float>((t - m_keyframes[i].time) / (m_keyframes[i+1].time - m_keyframes[i].time));
    // FIXME: apply timing function here.
    return m_keyframes[i].value + (m_keyframes[i+1].value - m_keyframes[i].value) * progress;
}

PassOwnPtr<CCKeyframedTransformAnimationCurve> CCKeyframedTransformAnimationCurve::create(const Vector<CCTransformKeyframe>& keyframes)
{
    if (!keyframes.size() || !keyframesAreSorted(keyframes))
        return nullptr;

    return adoptPtr(new CCKeyframedTransformAnimationCurve(keyframes));
}

CCKeyframedTransformAnimationCurve::CCKeyframedTransformAnimationCurve(const Vector<CCTransformKeyframe>& keyframes)
    : m_keyframes(keyframes)
{
}

CCKeyframedTransformAnimationCurve::~CCKeyframedTransformAnimationCurve() { }

double CCKeyframedTransformAnimationCurve::duration() const
{
    return m_keyframes.last().time - m_keyframes.first().time;
}

PassOwnPtr<CCAnimationCurve> CCKeyframedTransformAnimationCurve::clone() const
{
    Vector<CCTransformKeyframe> keyframes;
    // We need to do a deep copy of all of the keyframes since they contain ref
    // pointers to TransformOperation objects.
    for (size_t i = 0; i < m_keyframes.size(); ++i) {
        CCTransformKeyframe keyframe(m_keyframes[i].time);
        for (size_t j = 0; j < m_keyframes[i].value.size(); ++j) {
            TransformOperation::OperationType operationType = m_keyframes[i].value.operations()[j]->getOperationType();
            switch (operationType) {
            case TransformOperation::SCALE_X:
            case TransformOperation::SCALE_Y:
            case TransformOperation::SCALE_Z:
            case TransformOperation::SCALE_3D:
            case TransformOperation::SCALE: {
                ScaleTransformOperation* transform = static_cast<ScaleTransformOperation*>(m_keyframes[i].value.operations()[j].get());
                keyframe.value.operations().append(ScaleTransformOperation::create(transform->x(), transform->y(), transform->z(), operationType));
                break;
            }
            case TransformOperation::TRANSLATE_X:
            case TransformOperation::TRANSLATE_Y:
            case TransformOperation::TRANSLATE_Z:
            case TransformOperation::TRANSLATE_3D:
            case TransformOperation::TRANSLATE: {
                TranslateTransformOperation* transform = static_cast<TranslateTransformOperation*>(m_keyframes[i].value.operations()[j].get());
                keyframe.value.operations().append(TranslateTransformOperation::create(transform->x(), transform->y(), transform->z(), operationType));
                break;
            }
            case TransformOperation::ROTATE_X:
            case TransformOperation::ROTATE_Y:
            case TransformOperation::ROTATE_3D:
            case TransformOperation::ROTATE: {
                RotateTransformOperation* transform = static_cast<RotateTransformOperation*>(m_keyframes[i].value.operations()[j].get());
                keyframe.value.operations().append(RotateTransformOperation::create(transform->x(), transform->y(), transform->z(), transform->angle(), operationType));
                break;
            }
            case TransformOperation::SKEW_X:
            case TransformOperation::SKEW_Y:
            case TransformOperation::SKEW: {
                SkewTransformOperation* transform = static_cast<SkewTransformOperation*>(m_keyframes[i].value.operations()[j].get());
                keyframe.value.operations().append(SkewTransformOperation::create(transform->angleX(), transform->angleY(), operationType));
                break;
            }
            case TransformOperation::MATRIX: {
                MatrixTransformOperation* transform = static_cast<MatrixTransformOperation*>(m_keyframes[i].value.operations()[j].get());
                TransformationMatrix m = transform->matrix();
                keyframe.value.operations().append(MatrixTransformOperation::create(m.a(), m.b(), m.c(), m.d(), m.e(), m.f()));
                break;
            }
            case TransformOperation::MATRIX_3D: {
                Matrix3DTransformOperation* transform = static_cast<Matrix3DTransformOperation*>(m_keyframes[i].value.operations()[j].get());
                keyframe.value.operations().append(Matrix3DTransformOperation::create(transform->matrix()));
                break;
            }
            case TransformOperation::PERSPECTIVE: {
                PerspectiveTransformOperation* transform = static_cast<PerspectiveTransformOperation*>(m_keyframes[i].value.operations()[j].get());
                keyframe.value.operations().append(PerspectiveTransformOperation::create(transform->perspective()));
                break;
            }
            case TransformOperation::IDENTITY: {
                keyframe.value.operations().append(IdentityTransformOperation::create());
                break;
            }
            case TransformOperation::NONE:
                // Do nothing.
                break;
            } // switch
        } // for each operation
        keyframes.append(keyframe);
    }
    return CCKeyframedTransformAnimationCurve::create(keyframes);
}

TransformationMatrix CCKeyframedTransformAnimationCurve::getValue(double t, const IntSize& layerSize) const
{
    TransformationMatrix transformMatrix;

    if (t <= m_keyframes.first().time) {
        m_keyframes.first().value.apply(layerSize, transformMatrix);
        return transformMatrix;
    }

    if (t >= m_keyframes.last().time) {
        m_keyframes.last().value.apply(layerSize, transformMatrix);
        return transformMatrix;
    }

    size_t i = 0;
    for (; i < m_keyframes.size() - 1; ++i) {
        if (t < m_keyframes[i+1].time)
            break;
    }

    // FIXME: apply timing function here.
    double progress = (t - m_keyframes[i].time) / (m_keyframes[i+1].time - m_keyframes[i].time);

    if (m_keyframes[i].value.operationsMatch(m_keyframes[i+1].value)) {
        for (size_t j = 0; j < m_keyframes[i+1].value.size(); ++j)
            m_keyframes[i+1].value.operations()[j]->blend(m_keyframes[i].value.at(j), progress)->apply(transformMatrix, layerSize);
    } else {
        TransformationMatrix source;

        m_keyframes[i].value.apply(layerSize, source);
        m_keyframes[i+1].value.apply(layerSize, transformMatrix);

        transformMatrix.blend(source, progress);
    }

    return transformMatrix;
}

} // namespace WebCore
