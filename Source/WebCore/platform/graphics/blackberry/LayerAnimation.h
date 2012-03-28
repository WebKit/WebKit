/*
 * Copyright (C) 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef LayerAnimation_h
#define LayerAnimation_h

#include "GraphicsLayer.h"

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

#define DEBUG_LAYER_ANIMATION 0

namespace WebCore {

class Animation;
class LayerCompositingThread;
class TransformationMatrix;

// This class uses non-threadsafe refcounting in the WebCore::Animation and
// WebCore::String members, so using threadsafe refcounting here would be a big
// cover-up. Instead, you must be careful to use ref/deref this class only on
// the WebKit thread, or when the WebKit and compositing threads are in sync.
class LayerAnimation : public RefCounted<LayerAnimation> {
public:
    // WebKit thread only
    // Because the refcounting done in constructor and destructor is not thread safe,
    // the LayerAnimation must be created or destroyed on the WebKit thread, or when
    // the WebKit and compositing threads are in sync.
    // Also, the name property is using a String which has non-threadsafe refcounting.
    // The setStartTime method is not threadsafe and must only be called on a newly
    // created LayerAnimation before sending it off to the compositing thread.
    static PassRefPtr<LayerAnimation> create(const KeyframeValueList& values, const IntSize& boxSize, const Animation* animation, const String& name, double timeOffset)
    {
        return adoptRef(new LayerAnimation(values, boxSize, animation, name, timeOffset));
    }

    PassRefPtr<LayerAnimation> clone(double timeOffset)
    {
        LayerAnimation* animation = new LayerAnimation(*this);
        // The cloned animation should get a different timeOffset if it's paused.
        animation->m_timeOffset = timeOffset;

        return adoptRef(animation);
    }

    ~LayerAnimation()
    {
    }

    const String& name() const { return m_name; }
    void setStartTime(double time) { m_startTime = time; }

    // These functions are thread safe (immutable state).
    static int idFromAnimation(const Animation* animation) { return reinterpret_cast<int>(animation); }
    bool isEqualToAnimation(const Animation* animation) const { return idFromAnimation(animation) == id(); }
    int id() const { return m_id; }
    AnimatedPropertyID property() const { return m_values.property(); }
    IntSize boxSize() const { return m_boxSize; }
    double timeOffset() const { return m_timeOffset; }
    double startTime() const { return m_startTime; }
    size_t valueCount() const { return m_values.size(); }

    // NOTE: Don't try to ref a TimingFunction, that's not a threadsafe operation.
    const TimingFunction* timingFunction() const { return m_timingFunction.get(); }
    double duration() const { return m_duration; }
    int iterationCount() const { return m_iterationCount; }
    Animation::AnimationDirection direction() const { return m_direction; }

    // NOTE: Don't try to clone() an AnimationValue, that's not a threadsafe operation since it mutates refcounts.
    const AnimationValue* valueAt(size_t i) const { return m_values.at(i); }

    TransformationMatrix blendTransform(const TransformOperations* from, const TransformOperations*, double progress) const;
    float blendOpacity(float from, float to, double progress) const;
    void apply(LayerCompositingThread*, double elapsedTime);

private:
    LayerAnimation(const KeyframeValueList& values, const IntSize& boxSize, const Animation* animation, const String& name, double timeOffset)
        : m_id(reinterpret_cast<int>(animation))
        , m_values(values)
        , m_boxSize(boxSize)
        , m_name(name)
        , m_timeOffset(timeOffset)
        , m_startTime(0)
        , m_timingFunction(0)
        , m_duration(animation->duration())
        , m_iterationCount(animation->iterationCount())
        , m_direction(animation->direction())
    {
        if (animation->isTimingFunctionSet())
            m_timingFunction = animation->timingFunction();

        validateTransformLists();
    }

    LayerAnimation(const LayerAnimation& other)
        :  RefCounted<LayerAnimation>()
        , m_id(other.m_id)
        , m_values(other.m_values)
        , m_boxSize(other.m_boxSize)
        , m_name(other.m_name)
        , m_timeOffset(other.m_timeOffset)
        , m_startTime(other.m_startTime)
        , m_transformFunctionListValid(other.m_transformFunctionListValid)
        , m_timingFunction(other.m_timingFunction)
        , m_duration(other.m_duration)
        , m_iterationCount(other.m_iterationCount)
        , m_direction(other.m_direction)
    {
    }

    void validateTransformLists();

    int m_id;

    // NOTE: Don't expose the KeyframeValueList directly, since its copy
    // constructor mutates refcounts and thus is not thread safe.
    KeyframeValueList m_values;
    IntSize m_boxSize;
    String m_name;
    double m_timeOffset;
    double m_startTime;
    bool m_transformFunctionListValid;

    RefPtr<TimingFunction> m_timingFunction;
    double m_duration;
    int m_iterationCount;
    Animation::AnimationDirection m_direction;
};

}

#endif // LayerAnimation_h
