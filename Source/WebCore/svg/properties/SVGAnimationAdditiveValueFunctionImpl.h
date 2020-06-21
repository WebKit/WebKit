/*
 * Copyright (C) 2018-2019 Apple Inc.  All rights reserved.
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

#include "Color.h"
#include "FloatRect.h"
#include "SVGAngleValue.h"
#include "SVGAnimationAdditiveValueFunction.h"
#include "SVGLengthContext.h"
#include "SVGLengthValue.h"
#include "SVGPathByteStream.h"
#include "SVGPropertyTraits.h"

namespace WebCore {

class SVGAnimationAngleFunction : public SVGAnimationAdditiveValueFunction<SVGAngleValue> {
public:
    using Base = SVGAnimationAdditiveValueFunction<SVGAngleValue>;
    using Base::Base;

    void setFromAndToValues(SVGElement*, const String&, const String&) override
    {
        // Values will be set by SVGAnimatedAngleOrientAnimator.
        ASSERT_NOT_REACHED();
    }

    void animate(SVGElement*, float progress, unsigned repeatCount, SVGAngleValue& animated)
    {
        float number = animated.value();
        number = Base::animate(progress, repeatCount, m_from.value(), m_to.value(), toAtEndOfDuration().value(), number);
        animated.setValue(number);
    }

private:
    friend class SVGAnimatedAngleOrientAnimator;

    void addFromAndToValues(SVGElement*) override
    {
        m_to.setValue(m_to.value() + m_from.value());
    }
};

class SVGAnimationColorFunction : public SVGAnimationAdditiveValueFunction<Color> {
public:
    using Base = SVGAnimationAdditiveValueFunction<Color>;
    using Base::Base;

    void setFromAndToValues(SVGElement* targetElement, const String& from, const String& to) override
    {
        m_from = colorFromString(targetElement, from);
        m_to = colorFromString(targetElement, to);
    }

    void setToAtEndOfDurationValue(const String& toAtEndOfDuration) override
    {
        m_toAtEndOfDuration = SVGPropertyTraits<Color>::fromString(toAtEndOfDuration);
    }

    void animate(SVGElement*, float progress, unsigned repeatCount, Color& animated)
    {
        auto simpleAnimated = animated.toSRGBASimpleColorLossy();
        auto simpleFrom = m_animationMode == AnimationMode::To ? simpleAnimated : m_from.toSRGBASimpleColorLossy();
        auto simpleTo = m_to.toSRGBASimpleColorLossy();
        auto simpleToAtEndOfDuration = toAtEndOfDuration().toSRGBASimpleColorLossy();
        
        float red = Base::animate(progress, repeatCount, simpleFrom.redComponent(), simpleTo.redComponent(), simpleToAtEndOfDuration.redComponent(), simpleAnimated.redComponent());
        float green = Base::animate(progress, repeatCount, simpleFrom.greenComponent(), simpleTo.greenComponent(), simpleToAtEndOfDuration.greenComponent(), simpleAnimated.greenComponent());
        float blue = Base::animate(progress, repeatCount, simpleFrom.blueComponent(), simpleTo.blueComponent(), simpleToAtEndOfDuration.blueComponent(), simpleAnimated.blueComponent());
        float alpha = Base::animate(progress, repeatCount, simpleFrom.alphaComponent(), simpleTo.alphaComponent(), simpleToAtEndOfDuration.alphaComponent(), simpleAnimated.alphaComponent());
        
        animated = makeSimpleColor(roundAndClampColorChannel(red), roundAndClampColorChannel(green), roundAndClampColorChannel(blue), roundAndClampColorChannel(alpha));
    }

    Optional<float> calculateDistance(SVGElement*, const String& from, const String& to) const override
    {
        Color fromColor = CSSParser::parseColor(from.stripWhiteSpace());
        if (!fromColor.isValid())
            return { };
        Color toColor = CSSParser::parseColor(to.stripWhiteSpace());
        if (!toColor.isValid())
            return { };
            
        auto simpleFrom = fromColor.toSRGBASimpleColorLossy();
        auto simpleTo = toColor.toSRGBASimpleColorLossy();

        float red = simpleFrom.redComponent() - simpleTo.redComponent();
        float green = simpleFrom.greenComponent() - simpleTo.greenComponent();
        float blue = simpleFrom.blueComponent() - simpleTo.blueComponent();

        return std::hypot(red, green, blue);
    }

private:
    inline uint8_t roundAndClampColorChannel(int value)
    {
        return std::clamp(value, 0, 255);
    }

    inline uint8_t roundAndClampColorChannel(float value)
    {
        return std::clamp(std::roundf(value), 0.0f, 255.0f);
    }

    void addFromAndToValues(SVGElement*) override
    {
        auto simpleFrom = m_from.toSRGBASimpleColorLossy();
        auto simpleTo = m_to.toSRGBASimpleColorLossy();

        // Ignores any alpha and sets alpha on result to 100% opaque.
        m_to = makeSimpleColor(
            roundAndClampColorChannel(simpleTo.redComponent() + simpleFrom.redComponent()),
            roundAndClampColorChannel(simpleTo.greenComponent() + simpleFrom.greenComponent()),
            roundAndClampColorChannel(simpleTo.blueComponent() + simpleFrom.blueComponent())
        );
    }

    static Color colorFromString(SVGElement*, const String&);
};

class SVGAnimationIntegerFunction : public SVGAnimationAdditiveValueFunction<int> {
    friend class SVGAnimatedIntegerPairAnimator;

public:
    using Base = SVGAnimationAdditiveValueFunction<int>;
    using Base::Base;

    void setFromAndToValues(SVGElement*, const String& from, const String& to) override
    {
        m_from = SVGPropertyTraits<int>::fromString(from);
        m_to = SVGPropertyTraits<int>::fromString(to);
    }

    void setToAtEndOfDurationValue(const String& toAtEndOfDuration) override
    {
        m_toAtEndOfDuration = SVGPropertyTraits<int>::fromString(toAtEndOfDuration);
    }

    void animate(SVGElement*, float progress, unsigned repeatCount, int& animated)
    {
        animated = static_cast<int>(roundf(Base::animate(progress, repeatCount, m_from, m_to, toAtEndOfDuration(), animated)));
    }

    Optional<float> calculateDistance(SVGElement*, const String& from, const String& to) const override
    {
        return std::abs(to.toIntStrict() - from.toIntStrict());
    }

private:
    void addFromAndToValues(SVGElement*) override
    {
        m_to += m_from;
    }
};

class SVGAnimationLengthFunction : public SVGAnimationAdditiveValueFunction<SVGLengthValue> {
    using Base = SVGAnimationAdditiveValueFunction<SVGLengthValue>;

public:
    SVGAnimationLengthFunction(AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive, SVGLengthMode lengthMode)
        : Base(animationMode, calcMode, isAccumulated, isAdditive)
        , m_lengthMode(lengthMode)
    {
    }

    void setFromAndToValues(SVGElement*, const String& from, const String& to) override
    {
        m_from = SVGLengthValue(m_lengthMode, from);
        m_to = SVGLengthValue(m_lengthMode, to);
    }

    void setToAtEndOfDurationValue(const String& toAtEndOfDuration) override
    {
        m_toAtEndOfDuration = SVGLengthValue(m_lengthMode, toAtEndOfDuration);
    }

    void animate(SVGElement* targetElement, float progress, unsigned repeatCount, SVGLengthValue& animated)
    {
        SVGLengthContext lengthContext(targetElement);
        SVGLengthType lengthType = progress < 0.5 ? m_from.lengthType() : m_to.lengthType();

        float from = (m_animationMode == AnimationMode::To ? animated : m_from).value(lengthContext);
        float to = m_to.value(lengthContext);
        float toAtEndOfDuration = this->toAtEndOfDuration().value(lengthContext);
        float value = animated.value(lengthContext);

        value = Base::animate(progress, repeatCount, from, to, toAtEndOfDuration, value);
        animated = { lengthContext, value, lengthType, m_lengthMode };
    }

    Optional<float> calculateDistance(SVGElement* targetElement, const String& from, const String& to) const override
    {
        SVGLengthContext lengthContext(targetElement);
        auto fromLength = SVGLengthValue(m_lengthMode, from);
        auto toLength = SVGLengthValue(m_lengthMode, to);
        return fabsf(toLength.value(lengthContext) - fromLength.value(lengthContext));
    }

private:
    void addFromAndToValues(SVGElement* targetElement) override
    {
        SVGLengthContext lengthContext(targetElement);
        m_to.setValue(lengthContext, m_to.value(lengthContext) + m_from.value(lengthContext));
    }

    SVGLengthMode m_lengthMode;
};

class SVGAnimationNumberFunction : public SVGAnimationAdditiveValueFunction<float> {
    friend class SVGAnimatedNumberPairAnimator;

public:
    using Base = SVGAnimationAdditiveValueFunction<float>;
    using Base::Base;

    void setFromAndToValues(SVGElement*, const String& from, const String& to) override
    {
        m_from = SVGPropertyTraits<float>::fromString(from);
        m_to = SVGPropertyTraits<float>::fromString(to);
    }

    void setToAtEndOfDurationValue(const String& toAtEndOfDuration) override
    {
        m_toAtEndOfDuration = SVGPropertyTraits<float>::fromString(toAtEndOfDuration);
    }

    void animate(SVGElement*, float progress, unsigned repeatCount, float& animated)
    {
        float from = m_animationMode == AnimationMode::To ? animated : m_from;
        animated = Base::animate(progress, repeatCount, from, m_to, toAtEndOfDuration(), animated);
    }

    Optional<float> calculateDistance(SVGElement*, const String& from, const String& to) const override
    {
        return std::abs(parseNumber(to).valueOr(0) - parseNumber(from).valueOr(0));
    }

private:
    void addFromAndToValues(SVGElement*) override
    {
        m_to += m_from;
    }
};

class SVGAnimationPathSegListFunction : public SVGAnimationAdditiveValueFunction<SVGPathByteStream> {
public:
    using Base = SVGAnimationAdditiveValueFunction<SVGPathByteStream>;
    using Base::Base;

    void setFromAndToValues(SVGElement*, const String& from, const String& to) override
    {
        m_from = SVGPathByteStream(from);
        m_to = SVGPathByteStream(to);
    }

    void setToAtEndOfDurationValue(const String& toAtEndOfDuration) override
    {
        m_toAtEndOfDuration = SVGPathByteStream(toAtEndOfDuration);
    }

    void animate(SVGElement*, float progress, unsigned repeatCount, SVGPathByteStream& animated)
    {
        SVGPathByteStream underlyingPath;
        if (m_animationMode == AnimationMode::To)
            underlyingPath = animated;

        const SVGPathByteStream& from = m_animationMode == AnimationMode::To ? underlyingPath : m_from;

        // Cache the current animated value before the buildAnimatedSVGPathByteStream() clears animatedPath.
        SVGPathByteStream lastAnimated;
        if (!from.size() || (m_isAdditive && m_animationMode != AnimationMode::To))
            lastAnimated = animated;

        buildAnimatedSVGPathByteStream(from, m_to, animated, progress);

        // Handle additive='sum'.
        if (!lastAnimated.isEmpty())
            addToSVGPathByteStream(animated, lastAnimated);

        // Handle accumulate='sum'.
        if (m_isAccumulated && repeatCount)
            addToSVGPathByteStream(animated, toAtEndOfDuration(), repeatCount);
    }

private:
    void addFromAndToValues(SVGElement*) override
    {
        if (!m_from.size() || m_from.size() != m_to.size())
            return;
        addToSVGPathByteStream(m_to, m_from);
    }
};

class SVGAnimationRectFunction : public SVGAnimationAdditiveValueFunction<FloatRect> {
public:
    using Base = SVGAnimationAdditiveValueFunction<FloatRect>;
    using Base::Base;

    void setFromAndToValues(SVGElement*, const String& from, const String& to) override
    {
        m_from = SVGPropertyTraits<FloatRect>::fromString(from);
        m_to = SVGPropertyTraits<FloatRect>::fromString(to);
    }

    void setToAtEndOfDurationValue(const String& toAtEndOfDuration) override
    {
        m_toAtEndOfDuration = SVGPropertyTraits<FloatRect>::fromString(toAtEndOfDuration);
    }

    void animate(SVGElement*, float progress, unsigned repeatCount, FloatRect& animated)
    {
        FloatRect from = m_animationMode == AnimationMode::To ? animated : m_from;
        
        float x = Base::animate(progress, repeatCount, from.x(), m_to.x(), toAtEndOfDuration().x(), animated.x());
        float y = Base::animate(progress, repeatCount, from.y(), m_to.y(), toAtEndOfDuration().y(), animated.y());
        float width = Base::animate(progress, repeatCount, from.width(), m_to.width(), toAtEndOfDuration().width(), animated.width());
        float height = Base::animate(progress, repeatCount, from.height(), m_to.height(), toAtEndOfDuration().height(), animated.height());
        
        animated = { x, y, width, height };
    }

private:
    void addFromAndToValues(SVGElement*) override
    {
        m_to += m_from;
    }
};

}
