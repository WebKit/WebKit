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

#include "SVGAnimationAdditiveListFunction.h"
#include "SVGLengthList.h"
#include "SVGNumberList.h"
#include "SVGPointList.h"
#include "SVGTransformDistance.h"
#include "SVGTransformList.h"

namespace WebCore {

class SVGElement;

class SVGAnimationLengthListFunction : public SVGAnimationAdditiveListFunction<SVGLengthList> {
    using Base = SVGAnimationAdditiveListFunction<SVGLengthList>;

public:
    SVGAnimationLengthListFunction(AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive, SVGLengthMode lengthMode)
        : Base(animationMode, calcMode, isAccumulated, isAdditive, lengthMode)
    {
    }

    void setFromAndToValues(SVGElement*, const String& from, const String& to) override
    {
        m_from->parse(from);
        m_to->parse(to);
    }

    void setToAtEndOfDurationValue(const String& toAtEndOfDuration) override
    {
        m_toAtEndOfDuration->parse(toAtEndOfDuration);
    }

    void animate(SVGElement* targetElement, float progress, unsigned repeatCount, RefPtr<SVGLengthList>& animated)
    {
        if (!adjustAnimatedList(m_animationMode, progress, animated))
            return;

        const Vector<Ref<SVGLength>>& fromItems = m_animationMode == AnimationMode::To ? animated->items() : m_from->items();
        const Vector<Ref<SVGLength>>& toItems = m_to->items();
        const Vector<Ref<SVGLength>>& toAtEndOfDurationItems = toAtEndOfDuration()->items();
        Vector<Ref<SVGLength>>& animatedItems = animated->items();
        SVGLengthMode lengthMode = animated->lengthMode();

        SVGLengthContext lengthContext(targetElement);
        for (unsigned i = 0; i < toItems.size(); ++i) {
            SVGLengthType unitType = (i < fromItems.size() && progress < 0.5 ? fromItems : toItems)[i]->value().unitType();

            float from = i < fromItems.size() ? fromItems[i]->value().value(lengthContext) : 0;
            float to = toItems[i]->value().value(lengthContext);
            float toAtEndOfDuration = i < toAtEndOfDurationItems.size() ? toAtEndOfDurationItems[i]->value().value(lengthContext) : 0;
            float value = animatedItems[i]->value().value(lengthContext);

            value = Base::animate(progress, repeatCount, from, to, toAtEndOfDuration, value);
            animatedItems[i]->value().setValue(lengthContext, value, lengthMode, unitType);
        }
    }

private:
    void addFromAndToValues(SVGElement* targetElement) override
    {
        const Vector<Ref<SVGLength>>& fromItems = m_from->items();
        const Vector<Ref<SVGLength>>& toItems = m_to->items();

        if (!fromItems.size() || fromItems.size() != toItems.size())
            return;

        SVGLengthContext lengthContext(targetElement);
        for (unsigned i = 0; i < fromItems.size(); ++i) {
            const SVGLengthValue& fromValue = fromItems[i]->value();
            SVGLengthValue& toValue = toItems[i]->value();
            toValue.setValue(toValue.value(lengthContext) + fromValue.value(lengthContext), lengthContext);
        }
    }
};

class SVGAnimationNumberListFunction : public SVGAnimationAdditiveListFunction<SVGNumberList> {
public:
    using Base = SVGAnimationAdditiveListFunction<SVGNumberList>;
    using Base::Base;

    void setFromAndToValues(SVGElement*, const String& from, const String& to) override
    {
        m_from->parse(from);
        m_to->parse(to);
    }

    void setToAtEndOfDurationValue(const String& toAtEndOfDuration) override
    {
        m_toAtEndOfDuration->parse(toAtEndOfDuration);
    }

    void animate(SVGElement*, float progress, unsigned repeatCount, RefPtr<SVGNumberList>& animated)
    {
        if (!adjustAnimatedList(m_animationMode, progress, animated))
            return;

        auto& fromItems = m_animationMode == AnimationMode::To ? animated->items() : m_from->items();
        auto& toItems = m_to->items();
        auto& toAtEndOfDurationItems = toAtEndOfDuration()->items();
        auto& animatedItems = animated->items();

        for (unsigned i = 0; i < toItems.size(); ++i) {
            float from = i < fromItems.size() ? fromItems[i]->value() : 0;
            float to = toItems[i]->value();
            float toAtEndOfDuration = i < toAtEndOfDurationItems.size() ? toAtEndOfDurationItems[i]->value() : 0;

            float& value = animatedItems[i]->value();
            value = Base::animate(progress, repeatCount, from, to, toAtEndOfDuration, value);
        }
    }

private:
    void addFromAndToValues(SVGElement*) override
    {
        const Vector<Ref<SVGNumber>>& fromItems = m_from->items();
        Vector<Ref<SVGNumber>>& toItems = m_to->items();

        if (!fromItems.size() || fromItems.size() != toItems.size())
            return;

        for (unsigned i = 0; i < fromItems.size(); ++i)
            toItems[i]->setValue(fromItems[i]->value() + toItems[i]->value());
    }
};

class SVGAnimationPointListFunction : public SVGAnimationAdditiveListFunction<SVGPointList> {
public:
    using Base = SVGAnimationAdditiveListFunction<SVGPointList>;
    using Base::Base;

    void setFromAndToValues(SVGElement*, const String& from, const String& to) override
    {
        m_from->parse(from);
        m_to->parse(to);
    }

    void setToAtEndOfDurationValue(const String& toAtEndOfDuration) override
    {
        m_toAtEndOfDuration->parse(toAtEndOfDuration);
    }

    void animate(SVGElement*, float progress, unsigned repeatCount, RefPtr<SVGPointList>& animated)
    {
        if (!adjustAnimatedList(m_animationMode, progress, animated))
            return;

        auto& fromItems = m_animationMode == AnimationMode::To ? animated->items() : m_from->items();
        auto& toItems = m_to->items();
        auto& toAtEndOfDurationItems = toAtEndOfDuration()->items();
        auto& animatedItems = animated->items();

        for (unsigned i = 0; i < toItems.size(); ++i) {
            FloatPoint from = i < fromItems.size() ? fromItems[i]->value() : FloatPoint();
            FloatPoint to = toItems[i]->value();
            FloatPoint toAtEndOfDuration = i < toAtEndOfDurationItems.size() ? toAtEndOfDurationItems[i]->value() : FloatPoint();
            FloatPoint& animated = animatedItems[i]->value();

            float animatedX = Base::animate(progress, repeatCount, from.x(), to.x(), toAtEndOfDuration.x(), animated.x());
            float animatedY = Base::animate(progress, repeatCount, from.y(), to.y(), toAtEndOfDuration.y(), animated.y());

            animated = { animatedX, animatedY };
        }
    }

private:
    void addFromAndToValues(SVGElement*) override
    {
        const Vector<Ref<SVGPoint>>& fromItems = m_from->items();
        Vector<Ref<SVGPoint>>& toItems = m_to->items();

        if (!fromItems.size() || fromItems.size() != toItems.size())
            return;

        for (unsigned i = 0; i < fromItems.size(); ++i)
            toItems[i]->setValue(fromItems[i]->value() + toItems[i]->value());
    }
};

class SVGAnimationTransformListFunction : public SVGAnimationAdditiveListFunction<SVGTransformList> {
public:
    using Base = SVGAnimationAdditiveListFunction<SVGTransformList>;
    using Base::Base;

    void setFromAndToValues(SVGElement*, const String& from, const String& to) override
    {
        m_from->parse(from);
        m_to->parse(to);
    }

    void setToAtEndOfDurationValue(const String& toAtEndOfDuration) override
    {
        m_toAtEndOfDuration->parse(toAtEndOfDuration);
    }

    void animate(SVGElement*, float progress, unsigned repeatCount, RefPtr<SVGTransformList>& animated)
    {
        // Pass false to 'resizeAnimatedIfNeeded', as the special post-multiplication behavior of <animateTransform> needs to be respected below.
        if (!adjustAnimatedList(m_animationMode, progress, animated, false))
            return;

        // Spec: To animations provide specific functionality to get a smooth change from the underlying
        // value to the âtoâ attribute value, which conflicts mathematically with the requirement for
        // additive transform animations to be post-multiplied. As a consequence, in SVG 1.1 the behavior
        // of to animations for âanimateTransformâ is undefined.
        const Vector<Ref<SVGTransform>>& fromItems = m_from->items();
        const Vector<Ref<SVGTransform>>& toItems = m_to->items();
        const Vector<Ref<SVGTransform>>& toAtEndOfDurationItems = toAtEndOfDuration()->items();
        Vector<Ref<SVGTransform>>& animatedItems = animated->items();

        // Never resize the animatedList to the m_to size, instead either clear the list
        // or append to it.
        if (!animatedItems.isEmpty() && (!m_isAdditive || m_animationMode == AnimationMode::To))
            animatedItems.clear();

        auto fromItemsSize = fromItems.size();

        static const AffineTransform zerosAffineTransform = { 0, 0, 0, 0, 0, 0 };
        const SVGTransformValue& to = toItems[0]->value();
        const SVGTransformValue zerosTransform = SVGTransformValue(to.type(), zerosAffineTransform);

        const SVGTransformValue& from = fromItemsSize ? fromItems[0]->value() : zerosTransform;
        SVGTransformValue current = SVGTransformDistance(from, to).scaledDistance(progress).addToSVGTransform(from);

        if (m_isAccumulated && repeatCount) {
            const SVGTransformValue& toAtEndOfDuration = toAtEndOfDurationItems.size() ? toAtEndOfDurationItems[0]->value() : zerosTransform;
            animatedItems.append(SVGTransform::create(SVGTransformDistance::addSVGTransforms(current, toAtEndOfDuration, repeatCount)));
        } else
            animatedItems.append(SVGTransform::create(current));
    }

private:
    void addFromAndToValues(SVGElement*) override
    {
        const Vector<Ref<SVGTransform>>& fromItems = m_from->items();
        Vector<Ref<SVGTransform>>& toItems = m_to->items();

        if (!fromItems.size() || fromItems.size() != toItems.size())
            return;

        ASSERT(fromItems.size() == 1);
        const Ref<SVGTransform>& from = fromItems[0];
        Ref<SVGTransform>& to = toItems[0];

        to->setValue(SVGTransformDistance::addSVGTransforms(from->value(), to->value()));
    }
};

}
