/*
 * Copyright (C) 2009-2023 Apple Inc. All rights reserved.
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

#include "FilterOperations.h"
#include "GraphicsLayerAnimationProperty.h"
#include "TimingFunction.h"
#include "TransformList.h"
#include <wtf/IndexedRange.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

// Shared base type for animation values.
class AnimationValueBase {
public:
    AnimationValueBase(double keyTime, RefPtr<TimingFunction> timingFunction = nullptr)
        : m_keyTime(keyTime)
        , m_timingFunction(WTFMove(timingFunction))
    {
    }

    AnimationValueBase(const AnimationValueBase& other)
    {
        *this = other;
    }

    AnimationValueBase& operator=(const AnimationValueBase& other)
    {
        m_keyTime = other.m_keyTime;
        m_timingFunction = other.m_timingFunction ? RefPtr<TimingFunction> { other.timingFunction()->clone() } : nullptr;

        return *this;
    }


    double keyTime() const { return m_keyTime; }
    const RefPtr<TimingFunction> timingFunction() const { return m_timingFunction; }

protected:
    double m_keyTime;
    RefPtr<TimingFunction> m_timingFunction;
};

// Used to store one float value of an animation.
class FloatAnimationValue final : public AnimationValueBase {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(FloatAnimationValue, WEBCORE_EXPORT);
public:
    constexpr static bool supportsProperty(GraphicsLayerAnimationProperty property)
    {
        return property == GraphicsLayerAnimationProperty::Opacity;
    }

    FloatAnimationValue(double keyTime, float value, RefPtr<TimingFunction> timingFunction = nullptr)
        : AnimationValueBase(keyTime, WTFMove(timingFunction))
        , m_value(value)
    {
    }

    FloatAnimationValue(const FloatAnimationValue&) = default;
    FloatAnimationValue& operator=(const FloatAnimationValue&) = default;
    FloatAnimationValue(FloatAnimationValue&&) = default;
    FloatAnimationValue& operator=(FloatAnimationValue&&) = default;

    FloatAnimationValue clone() const { return *this; }

    float value() const { return m_value; }

private:
    float m_value;
};

// Used to store one transform value in a keyframe list.
class TransformAnimationValue final : public AnimationValueBase {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(TransformAnimationValue, WEBCORE_EXPORT);
public:
    constexpr static bool supportsProperty(GraphicsLayerAnimationProperty property)
    {
        return animatedPropertyIsTransformOrRelated(property);
    }

    TransformAnimationValue(double keyTime, const TransformList& value, RefPtr<TimingFunction> timingFunction = nullptr)
        : AnimationValueBase(keyTime, WTFMove(timingFunction))
        , m_value(value)
    {
    }

    TransformAnimationValue(const TransformAnimationValue&) = default;
    TransformAnimationValue& operator=(const TransformAnimationValue&) = default;
    TransformAnimationValue(TransformAnimationValue&&) = default;
    TransformAnimationValue& operator=(TransformAnimationValue&&) = default;

    const TransformList& value() const { return m_value; }

private:
    TransformList m_value;
};

// Used to store one filter value in a keyframe list.
class FilterAnimationValue final : public AnimationValueBase {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(FilterAnimationValue, WEBCORE_EXPORT);
public:
    constexpr static bool supportsProperty(GraphicsLayerAnimationProperty property)
    {
        return animatedPropertyIsFilterOrRelated(property);
    }

    FilterAnimationValue(double keyTime, const FilterOperations& value, RefPtr<TimingFunction> timingFunction = nullptr)
        : AnimationValueBase(keyTime, WTFMove(timingFunction))
        , m_value(value)
    {
    }

    FilterAnimationValue(const FilterAnimationValue& other)
        : AnimationValueBase(other)
        , m_value(other.m_value.clone())
    {
    }

    FilterAnimationValue& operator=(const FilterAnimationValue& other)
    {
        AnimationValueBase::operator=(other);
        m_value = other.m_value.clone();
        return *this;
    }

    FilterAnimationValue(FilterAnimationValue&&) = default;
    FilterAnimationValue& operator=(FilterAnimationValue&&) = default;

    const FilterOperations& value() const { return m_value; }

private:
    FilterOperations m_value;
};

// Used to store a series of values in a keyframe list.
// Values will all be of the same type, which can be inferred from the property.

template<typename T> class KeyframeValueList {
public:
    using const_iterator = typename Vector<T>::const_iterator;
    using const_reverse_iterator = typename Vector<T>::const_reverse_iterator;
    using value_type = typename Vector<T>::value_type;

    explicit KeyframeValueList(GraphicsLayerAnimationProperty property)
        : m_property(property)
    {
        ASSERT(T::supportsProperty(property));
    }

    KeyframeValueList(const KeyframeValueList<T>&) = default;
    KeyframeValueList& operator=(const KeyframeValueList<T>&) = default;
    KeyframeValueList(KeyframeValueList<T>&&) = default;
    KeyframeValueList& operator=(KeyframeValueList<T>&&) = default;

    void swap(KeyframeValueList<T>& other)
    {
        std::swap(m_property, other.m_property);
        m_values.swap(other.m_values);
    }

    GraphicsLayerAnimationProperty property() const { return m_property; }

    size_t size() const { return m_values.size(); }
    const T& operator[](size_t i) const { return m_values[i]; }

    const_iterator begin() const { return m_values.begin(); }
    const_iterator end() const { return m_values.end(); }
    const_reverse_iterator rbegin() const { return m_values.rbegin(); }
    const_reverse_iterator rend() const { return m_values.rend(); }

    // Insert, sorted by keyTime.
    void insert(T);

protected:
    Vector<T> m_values;
    GraphicsLayerAnimationProperty m_property;
};

template<typename T> void KeyframeValueList<T>::insert(T value)
{
    for (auto [i, currentValue] : indexedRange(m_values)) {
        if (currentValue.keyTime() == value.keyTime()) {
            ASSERT_NOT_REACHED();
            // insert after
            m_values.insert(i + 1, WTFMove(value));
            return;
        }
        if (currentValue.keyTime() > value.keyTime()) {
            // insert before
            m_values.insert(i, WTFMove(value));
            return;
        }
    }
    m_values.append(WTFMove(value));
}

// Given a KeyframeValueList containing filterOperations, return -1 if the operations are invalid, otherwise, returns the index of the first non-empty value.
int validateFilterOperations(const KeyframeValueList<FilterAnimationValue>&);

} // namespace WebCore
