/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef AnimationTrigger_h
#define AnimationTrigger_h

#if ENABLE(CSS_ANIMATIONS_LEVEL_2)

#include "Length.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

class AnimationTrigger : public RefCounted<AnimationTrigger> {
public:

    virtual PassRefPtr<AnimationTrigger> clone() const = 0;

    enum class AnimationTriggerType {
        AutoAnimationTriggerType, ScrollAnimationTriggerType
    };

    virtual ~AnimationTrigger() { }

    AnimationTriggerType type() const { return m_type; }

    bool isAutoAnimationTrigger() const { return m_type == AnimationTriggerType::AutoAnimationTriggerType; }
    bool isScrollAnimationTrigger() const { return m_type == AnimationTriggerType::ScrollAnimationTriggerType; }

    virtual bool operator==(const AnimationTrigger& other) = 0;

protected:
    AnimationTrigger(AnimationTriggerType type)
        : m_type(type)
    {
    }

    AnimationTriggerType m_type;
};

class AutoAnimationTrigger : public AnimationTrigger {
public:
    static PassRefPtr<AutoAnimationTrigger> create()
    {
        return adoptRef(new AutoAnimationTrigger);
    }

    virtual ~AutoAnimationTrigger() { }

    bool operator==(const AnimationTrigger& other) override
    {
        return other.isAutoAnimationTrigger();
    }

private:
    AutoAnimationTrigger()
        : AnimationTrigger(AnimationTriggerType::AutoAnimationTriggerType)
    {
    }

    PassRefPtr<AnimationTrigger> clone() const override
    {
        return adoptRef(new AutoAnimationTrigger);
    }
};

class ScrollAnimationTrigger : public AnimationTrigger {
public:
    static PassRefPtr<ScrollAnimationTrigger> create(Length startValue, Length endValue)
    {
        return adoptRef(new ScrollAnimationTrigger(startValue, endValue));
    }

    virtual ~ScrollAnimationTrigger() { }

    bool operator==(const AnimationTrigger& other) override
    {
        if (!other.isScrollAnimationTrigger())
            return false;

        const ScrollAnimationTrigger* otherTrigger = static_cast<const ScrollAnimationTrigger*>(&other);
        return m_startValue == otherTrigger->m_startValue
            && m_endValue == otherTrigger->m_endValue
            && m_hasEndValue == otherTrigger->m_hasEndValue;
    }

    Length startValue() const { return m_startValue; }

    void setStartValue(Length value)
    {
        m_startValue = value;
    }

    Length endValue() const { return m_endValue; }

    void setEndValue(Length value)
    {
        m_endValue = value;
    }

    bool hasEndValue() const { return !m_endValue.isAuto() && m_endValue.value() > m_startValue.value(); }

private:
    explicit ScrollAnimationTrigger(Length startValue, Length endValue)
        : AnimationTrigger(AnimationTriggerType::ScrollAnimationTriggerType)
        , m_startValue(startValue)
    {
        if (!endValue.isAuto() && endValue.value() > startValue.value())
            m_endValue = endValue;
    }

    PassRefPtr<AnimationTrigger> clone() const override
    {
        return adoptRef(new ScrollAnimationTrigger(m_startValue, m_endValue));
    }

    Length m_startValue;
    Length m_endValue;
    bool m_hasEndValue;
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_ANIMATION_TRIGGER(ToClassName, TriggerTest) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToClassName) \
static bool isType(const WebCore::AnimationTrigger& trigger) { return trigger.TriggerTest(); } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_ANIMATION_TRIGGER(AutoAnimationTrigger, isAutoAnimationTrigger);
SPECIALIZE_TYPE_TRAITS_ANIMATION_TRIGGER(ScrollAnimationTrigger, isScrollAnimationTrigger);

#endif

#endif // AnimationTrigger_h
