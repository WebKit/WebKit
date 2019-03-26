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

#include "SVGAnimationAdditiveValueFunction.h"
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

    void progress(SVGElement*, float percentage, unsigned repeatCount, SVGAngleValue& animated)
    {
        float number = animated.value();
        number = Base::progress(percentage, repeatCount, m_from.value(), m_to.value(), toAtEndOfDuration().value(), number);
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

    void progress(SVGElement*, float percentage, unsigned repeatCount, Color& animated)
    {
        Color from = m_animationMode == AnimationMode::To ? animated : m_from;
        
        float red = Base::progress(percentage, repeatCount, from.red(), m_to.red(), toAtEndOfDuration().red(), animated.red());
        float green = Base::progress(percentage, repeatCount, from.green(), m_to.green(), toAtEndOfDuration().green(), animated.green());
        float blue = Base::progress(percentage, repeatCount, from.blue(), m_to.blue(), toAtEndOfDuration().blue(), animated.blue());
        float alpha = Base::progress(percentage, repeatCount, from.alpha(), m_to.alpha(), toAtEndOfDuration().alpha(), animated.alpha());
        
        animated = { roundAndClampColorChannel(red), roundAndClampColorChannel(green), roundAndClampColorChannel(blue), roundAndClampColorChannel(alpha) };
    }

    float calculateDistance(SVGElement*, const String& from, const String& to) const override
    {
        Color fromColor = CSSParser::parseColor(from.stripWhiteSpace());
        if (!fromColor.isValid())
            return -1;
        Color toColor = CSSParser::parseColor(to.stripWhiteSpace());
        if (!toColor.isValid())
            return -1;
        float red = fromColor.red() - toColor.red();
        float green = fromColor.green() - toColor.green();
        float blue = fromColor.blue() - toColor.blue();
        return sqrtf(red * red + green * green + blue * blue);
    }

private:
    void addFromAndToValues(SVGElement*) override
    {
        // Ignores any alpha and sets alpha on result to 100% opaque.
        m_to = {
            roundAndClampColorChannel(m_to.red() + m_from.red()),
            roundAndClampColorChannel(m_to.green() + m_from.green()),
            roundAndClampColorChannel(m_to.blue() + m_from.blue())
        };
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

    void progress(SVGElement*, float percentage, unsigned repeatCount, int& animated)
    {
        animated = static_cast<int>(roundf(Base::progress(percentage, repeatCount, m_from, m_to, toAtEndOfDuration(), animated)));
    }

    float calculateDistance(SVGElement*, const String& from, const String& to) const override
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

    void progress(SVGElement* targetElement, float percentage, unsigned repeatCount, SVGLengthValue& animated)
    {
        SVGLengthContext lengthContext(targetElement);
        SVGLengthType unitType = percentage < 0.5 ? m_from.unitType() : m_to.unitType();

        float from = (m_animationMode == AnimationMode::To ? animated : m_from).value(lengthContext);
        float to = m_to.value(lengthContext);
        float toAtEndOfDuration = this->toAtEndOfDuration().value(lengthContext);
        float value = animated.value(lengthContext);

        value = Base::progress(percentage, repeatCount, from, to, toAtEndOfDuration, value);
        animated = { lengthContext, value, m_lengthMode, unitType };
    }

    float calculateDistance(SVGElement* targetElement, const String& from, const String& to) const override
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
        m_to.setValue(m_to.value(lengthContext) + m_from.value(lengthContext), lengthContext);
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

    void progress(SVGElement*, float percentage, unsigned repeatCount, float& animated)
    {
        float from = m_animationMode == AnimationMode::To ? animated : m_from;
        animated = Base::progress(percentage, repeatCount, from, m_to, toAtEndOfDuration(), animated);
    }

    float calculateDistance(SVGElement*, const String& from, const String& to) const override
    {
        float fromNumber = 0;
        float toNumber = 0;
        parseNumberFromString(from, fromNumber);
        parseNumberFromString(to, toNumber);
        return fabsf(toNumber - fromNumber);
    }

private:
    void addFromAndToValues(SVGElement*) override
    {
        m_to += m_from;
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

    void progress(SVGElement*, float percentage, unsigned repeatCount, FloatRect& animated)
    {
        FloatRect from = m_animationMode == AnimationMode::To ? animated : m_from;
        
        float x = Base::progress(percentage, repeatCount, from.x(), m_to.x(), toAtEndOfDuration().x(), animated.x());
        float y = Base::progress(percentage, repeatCount, from.y(), m_to.y(), toAtEndOfDuration().y(), animated.y());
        float width = Base::progress(percentage, repeatCount, from.width(), m_to.width(), toAtEndOfDuration().width(), animated.width());
        float height = Base::progress(percentage, repeatCount, from.height(), m_to.height(), toAtEndOfDuration().height(), animated.height());
        
        animated = { x, y, width, height };
    }

private:
    void addFromAndToValues(SVGElement*) override
    {
        m_to += m_from;
    }
};

}
