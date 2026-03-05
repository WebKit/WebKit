/*
 * Copyright (C) 2007-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2012, 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2025 Sam Weinig. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#ifndef STYLE_INTERPOLATION_GENERATED_INCLUDE_TRAP
#error "Please do not include this file anywhere except from generated code."
#endif

#include "AnimationMalloc.h"
#include "StyleInterpolationFunctions.h"
#include "StyleInterpolationWrapperBase.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/TextStream.h>

namespace WebCore::Style::Interpolation {

// MARK: - Base Wrappers

template<typename T, typename GetterType = T>
class WrapperWithGetter : public WrapperBase {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(WrapperWithGetter, Animation);
public:
    WrapperWithGetter(CSSPropertyID property, GetterType (ComputedStyleProperties::*getter)() const)
        : WrapperBase(property)
        , m_getter(getter)
    {
    }

    GetterType value(const RenderStyle& style) const
    {
        return (style.computedStyle().*m_getter)();
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const override
    {
        if (&a == &b)
            return true;
        return value(a) == value(b);
    }

#if !LOG_DISABLED
    void log(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << property() << " from " << value(from) << " to " << value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(destination));
    }
#endif

private:
    GetterType (ComputedStyleProperties::*m_getter)() const;
};

template<typename T, typename GetterType = T, typename SetterType = T>
class Wrapper : public WrapperWithGetter<T, GetterType> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Wrapper, Animation);
public:
    Wrapper(CSSPropertyID property, GetterType (ComputedStyleProperties::*getter)() const, void (ComputedStyleProperties::*setter)(SetterType))
        : WrapperWithGetter<T, GetterType>(property, getter)
        , m_setter(setter)
    {
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const override
    {
        (destination.computedStyle().*m_setter)(blendFunc(this->value(from), this->value(to), context));
    }

protected:
    void (ComputedStyleProperties::*m_setter)(SetterType);
};

// Deduction guide for getter/setters that return and take values.
template<typename T, typename GetterRenderStyle, typename SetterRenderStyle>
Wrapper(CSSPropertyID, T (GetterRenderStyle::*getter)() const, void (SetterRenderStyle::*setter)(T)) -> Wrapper<T, T, T>;

// Deduction guide for getter/setters that return const references and take r-value references.
template<typename T, typename GetterRenderStyle, typename SetterRenderStyle>
Wrapper(CSSPropertyID, const T& (GetterRenderStyle::*getter)() const, void (SetterRenderStyle::*setter)(T&&)) -> Wrapper<T, const T&, T&&>;

// MARK: - Typed Wrappers

template<typename T, typename GetterType = T, typename SetterType = T>
class StyleTypeWrapper : public WrapperBase {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StyleTypeWrapper, Animation);
public:
    StyleTypeWrapper(CSSPropertyID property, GetterType (ComputedStyleProperties::*getter)() const, void (ComputedStyleProperties::*setter)(SetterType))
        : WrapperBase(property)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

    bool equals(const RenderStyle& from, const RenderStyle& to) const override
    {
        if (&from == &to)
            return true;
        return Style::equalsForBlending(this->value(from), this->value(to), from, to);
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation operation) const override
    {
        return Style::canBlend(this->value(from), this->value(to), from, to, operation);
    }

    bool requiresInterpolationForAccumulativeIteration(const RenderStyle& from, const RenderStyle& to) const override
    {
        return Style::requiresInterpolationForAccumulativeIteration(this->value(from), this->value(to), from, to);
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const override
    {
        (destination.computedStyle().*m_setter)(Style::blend(this->value(from), this->value(to), from, to, context));
    }

#if !LOG_DISABLED
    void log(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const override
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << property() << " from " << this->value(from) << " to " << this->value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << this->value(destination));
    }
#endif

private:
    GetterType value(const RenderStyle& style) const
    {
        return (style.computedStyle().*m_getter)();
    }

    GetterType (ComputedStyleProperties::*m_getter)() const;
    void (ComputedStyleProperties::*m_setter)(SetterType);
};

// Deduction guide for getter/setters that return and take values.
template<typename T, typename GetterRenderStyle, typename SetterRenderStyle>
StyleTypeWrapper(CSSPropertyID, T (GetterRenderStyle::*getter)() const, void (SetterRenderStyle::*setter)(T)) -> StyleTypeWrapper<T, T, T>;

// Deduction guide for getter/setters that return const references and take r-value references.
template<typename T, typename GetterRenderStyle, typename SetterRenderStyle>
StyleTypeWrapper(CSSPropertyID, const T& (GetterRenderStyle::*getter)() const, void (SetterRenderStyle::*setter)(T&&)) -> StyleTypeWrapper<T, const T&, T&&>;

// Deduction guide for getter/setters that return values and take r-value references.
template<typename T, typename GetterRenderStyle, typename SetterRenderStyle>
StyleTypeWrapper(CSSPropertyID, T (GetterRenderStyle::*getter)() const, void (SetterRenderStyle::*setter)(T&&)) -> StyleTypeWrapper<T, T, T&&>;

template<typename T> class VisitedAffectedStyleTypeWrapper final : public WrapperBase {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(VisitedAffectedStyleTypeWrapper, Animation);
public:
    VisitedAffectedStyleTypeWrapper(CSSPropertyID property, const T& (ComputedStyleProperties::*getter)() const, void (ComputedStyleProperties::*setter)(T&&), const T& (ComputedStyleProperties::*visitedGetter)() const, void (ComputedStyleProperties::*visitedSetter)(T&&))
        : WrapperBase(property)
        , m_wrapper(StyleTypeWrapper<T, const T&, T&&>(property, getter, setter))
        , m_visitedWrapper(StyleTypeWrapper<T, const T&, T&&>(property, visitedGetter, visitedSetter))
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const override
    {
        return m_wrapper.equals(a, b) && m_visitedWrapper.equals(a, b);
    }

    bool canInterpolate(const RenderStyle& a, const RenderStyle& b, CompositeOperation operation) const override
    {
        const_cast<VisitedAffectedStyleTypeWrapper&>(*this).m_wrapperCanInterpolate = m_wrapper.canInterpolate(a, b, operation);
        const_cast<VisitedAffectedStyleTypeWrapper&>(*this).m_visitedWrapperCanInterpolate = m_visitedWrapper.canInterpolate(a, b, operation);
        return m_wrapperCanInterpolate || m_visitedWrapperCanInterpolate;
    }

    bool requiresInterpolationForAccumulativeIteration(const RenderStyle& a, const RenderStyle& b) const override
    {
        return m_wrapper.requiresInterpolationForAccumulativeIteration(a, b) && m_visitedWrapper.requiresInterpolationForAccumulativeIteration(a, b);
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const override
    {
        bool usesNonNormalizeDiscreteInterpolation = CSSProperty::animationUsesNonNormalizedDiscreteInterpolation(property());

        auto wrapperContext = context;
        wrapperContext.isDiscrete = !m_wrapperCanInterpolate;
        if (!usesNonNormalizeDiscreteInterpolation)
            wrapperContext.normalizeProgress();
        m_wrapper.interpolate(destination, from, to, wrapperContext);

        auto visitedWrapperContext = context;
        visitedWrapperContext.isDiscrete = !m_visitedWrapperCanInterpolate;
        if (!usesNonNormalizeDiscreteInterpolation)
            visitedWrapperContext.normalizeProgress();
        m_visitedWrapper.interpolate(destination, from, to, visitedWrapperContext);
    }

#if !LOG_DISABLED
    void log(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const override
    {
        m_wrapper.log(from, to, destination, progress);
        m_visitedWrapper.log(from, to, destination, progress);
    }
#endif

    StyleTypeWrapper<T, const T&, T&&> m_wrapper;
    StyleTypeWrapper<T, const T&, T&&> m_visitedWrapper;
    bool m_wrapperCanInterpolate { false };
    bool m_visitedWrapperCanInterpolate { false };
};

// MARK: - Discrete Wrappers

template<typename T, typename GetterType = T, typename SetterType = T> class DiscreteWrapper : public WrapperWithGetter<T, GetterType> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(DiscreteWrapper, Animation);
public:
    DiscreteWrapper(CSSPropertyID property, GetterType (ComputedStyleProperties::*getter)() const, void (ComputedStyleProperties::*setter)(SetterType))
        : WrapperWithGetter<T, GetterType>(property, getter)
        , m_setter(setter)
    {
    }

    bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const final
    {
        return false;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const override
    {
        ASSERT(!context.progress || context.progress == 1.0);
        (destination.computedStyle().*this->m_setter)(T { this->value(context.progress ? to : from) });
    }

private:
    void (ComputedStyleProperties::*m_setter)(SetterType);
};

// Deduction guide for getter/setters that return and take values.
template<typename T, typename GetterRenderStyle, typename SetterRenderStyle>
DiscreteWrapper(CSSPropertyID, T (GetterRenderStyle::*getter)() const, void (SetterRenderStyle::*setter)(T)) -> DiscreteWrapper<T, T, T>;

// Deduction guide for getter/setters that return const references and take r-value references.
template<typename T, typename GetterRenderStyle, typename SetterRenderStyle>
DiscreteWrapper(CSSPropertyID, const T& (GetterRenderStyle::*getter)() const, void (SetterRenderStyle::*setter)(T&&)) -> DiscreteWrapper<T, const T&, T&&>;

// Deduction guide for getter/setters that return values and take r-value references.
template<typename T, typename GetterRenderStyle, typename SetterRenderStyle>
DiscreteWrapper(CSSPropertyID, T (GetterRenderStyle::*getter)() const, void (SetterRenderStyle::*setter)(T&&)) -> DiscreteWrapper<T, T, T&&>;

template<typename T>
class NonNormalizedDiscreteWrapper final : public Wrapper<T> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(NonNormalizedDiscreteWrapper, Animation);
public:
    NonNormalizedDiscreteWrapper(CSSPropertyID property, T (ComputedStyleProperties::*getter)() const, void (ComputedStyleProperties::*setter)(T))
        : Wrapper<T>(property, getter, setter)
    {
    }

    bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const final
    {
        return false;
    }
};

// MARK: - Font Property Wrappers

class FontSizeWrapper final : public Wrapper<float> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(FontSizeWrapper, Animation);
public:
    FontSizeWrapper()
        : Wrapper<float>(CSSPropertyID::CSSPropertyFontSize, &ComputedStyleProperties::computedFontSize, &ComputedStyleProperties::setFontSize)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return a.specifiedFontSize() == b.specifiedFontSize();
    }
};

// MARK: - Color Property Wrappers

class ColorWrapper final : public WrapperWithGetter<const WebCore::Color&> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(ColorWrapper, Animation);
public:
    ColorWrapper(CSSPropertyID property, const WebCore::Color& (ComputedStyleProperties::*getter)() const, void (ComputedStyleProperties::*setter)(WebCore::Color&&))
        : WrapperWithGetter<const WebCore::Color&>(property, getter)
        , m_setter(setter)
    {
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const override
    {
        (destination.computedStyle().*m_setter)(blendFunc(value(from), value(to), context));
    }

private:
    void (ComputedStyleProperties::*m_setter)(WebCore::Color&&);
};

class VisitedAffectedColorWrapper final : public WrapperBase {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(VisitedAffectedColorWrapper, Animation);
public:
    VisitedAffectedColorWrapper(CSSPropertyID property, const WebCore::Color& (ComputedStyleProperties::*getter)() const, void (ComputedStyleProperties::*setter)(WebCore::Color&&), const WebCore::Color& (ComputedStyleProperties::*visitedGetter)() const, void (ComputedStyleProperties::*visitedSetter)(WebCore::Color&&))
        : WrapperBase(property)
        , m_wrapper(ColorWrapper(property, getter, setter))
        , m_visitedWrapper(ColorWrapper(property, visitedGetter, visitedSetter))
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return m_wrapper.equals(a, b) && m_visitedWrapper.equals(a, b);
    }

    bool requiresInterpolationForAccumulativeIteration(const RenderStyle&, const RenderStyle&) const final
    {
        return true;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        m_wrapper.interpolate(destination, from, to, context);
        m_visitedWrapper.interpolate(destination, from, to, context);
    }

#if !LOG_DISABLED
    void log(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        m_wrapper.log(from, to, destination, progress);
        m_visitedWrapper.log(from, to, destination, progress);
    }
#endif

    ColorWrapper m_wrapper;
    ColorWrapper m_visitedWrapper;
};

// MARK: - Other Custom Wrappers

class VisibilityWrapper final : public Wrapper<Visibility> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(VisibilityWrapper, Animation);
public:
    VisibilityWrapper()
        : Wrapper(CSSPropertyVisibility, &ComputedStyleProperties::visibility, &ComputedStyleProperties::setVisibility)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        // https://drafts.csswg.org/web-animations-1/#animating-visibility
        // If neither value is visible, then discrete animation is used.
        return value(from) == Visibility::Visible || value(to) == Visibility::Visible;
    }
};

// MARK: - CoordinatedValueList Wrappers

// Wrapper base class for an animatable property in a CoordinatedValueList
template<typename CoordinatedValueListValueType>
class CoordinatedValueListPropertyWrapperBase {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(CoordinatedValueListPropertyWrapperBase, Animation);
public:
    CoordinatedValueListPropertyWrapperBase(CSSPropertyID property)
        : m_property(property)
    {
    }
    virtual ~CoordinatedValueListPropertyWrapperBase() = default;

    CSSPropertyID property() const { return m_property; }

    virtual bool equals(const CoordinatedValueListValueType&, const CoordinatedValueListValueType&, const RenderStyle&, const RenderStyle&) const = 0;
    virtual void interpolate(CoordinatedValueListValueType&, const CoordinatedValueListValueType&, const CoordinatedValueListValueType&, const RenderStyle&, const RenderStyle&, const Context&) const = 0;
    virtual bool canInterpolate(const CoordinatedValueListValueType&, const CoordinatedValueListValueType&, const RenderStyle&, const RenderStyle&, CompositeOperation) const { return true; }
#if !LOG_DISABLED
    virtual void log(const CoordinatedValueListValueType& destination, const CoordinatedValueListValueType&, const CoordinatedValueListValueType&, const RenderStyle&, const RenderStyle&, double) const = 0;
#endif

private:
    CSSPropertyID m_property;
};

template<typename StyleType, typename CoordinatedValueListValueType>
class CoordinatedValueListPropertyStyleTypeWrapper final : public CoordinatedValueListPropertyWrapperBase<CoordinatedValueListValueType> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(CoordinatedValueListPropertyStyleTypeWrapper, Animation);
public:
    CoordinatedValueListPropertyStyleTypeWrapper(CSSPropertyID property, const StyleType& (CoordinatedValueListValueType::*getter)() const, void (CoordinatedValueListValueType::*setter)(StyleType&&))
        : CoordinatedValueListPropertyWrapperBase<CoordinatedValueListValueType>(property)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

    bool equals(const CoordinatedValueListValueType& from, const CoordinatedValueListValueType& to, const RenderStyle& fromStyle, const RenderStyle& toStyle) const override
    {
        if (&from == &to)
            return true;
        return Style::equalsForBlending(value(from), value(to), fromStyle, toStyle);
    }

    bool canInterpolate(const CoordinatedValueListValueType& from, const CoordinatedValueListValueType& to, const RenderStyle& fromStyle, const RenderStyle& toStyle, CompositeOperation operation) const override final
    {
        return Style::canBlend(value(from), value(to), fromStyle, toStyle, operation);
    }

    void interpolate(CoordinatedValueListValueType& destination, const CoordinatedValueListValueType& from, const CoordinatedValueListValueType& to, const RenderStyle& fromStyle, const RenderStyle& toStyle, const Context& context) const override final
    {
        (destination.*m_setter)(Style::blend(value(from), value(to), fromStyle, toStyle, context));
    }

#if !LOG_DISABLED
    void log(const CoordinatedValueListValueType& destination, const CoordinatedValueListValueType& from, const CoordinatedValueListValueType& to, const RenderStyle&, const RenderStyle&, double progress) const override final
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << this->property() << " from " << value(from) << " to " << value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(destination));
    }
#endif

private:
    const StyleType& value(const CoordinatedValueListValueType& value) const
    {
        return (value.*m_getter)();
    }

    const StyleType& (CoordinatedValueListValueType::*m_getter)() const;
    void (CoordinatedValueListValueType::*m_setter)(StyleType&&);
};

template<typename T, typename CoordinatedValueListValueType, typename GetterType = T, typename SetterType = T>
class DiscreteCoordinatedValueListPropertyWrapper final : public CoordinatedValueListPropertyWrapperBase<CoordinatedValueListValueType> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(DiscreteCoordinatedValueListPropertyWrapper, Animation);
public:
    DiscreteCoordinatedValueListPropertyWrapper(CSSPropertyID property, GetterType (CoordinatedValueListValueType::*getter)() const, void (CoordinatedValueListValueType::*setter)(SetterType))
        : CoordinatedValueListPropertyWrapperBase<CoordinatedValueListValueType>(property)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

    bool equals(const CoordinatedValueListValueType& a, const CoordinatedValueListValueType& b, const RenderStyle&, const RenderStyle&) const final
    {
        return value(a) == value(b);
    }

    bool canInterpolate(const CoordinatedValueListValueType&, const CoordinatedValueListValueType&, const RenderStyle&, const RenderStyle&, CompositeOperation) const final
    {
        return false;
    }

    void interpolate(CoordinatedValueListValueType& destination, const CoordinatedValueListValueType& from, const CoordinatedValueListValueType& to, const RenderStyle&, const RenderStyle&, const Context& context) const final
    {
        ASSERT(!context.progress || context.progress == 1.0);
        (destination.*m_setter)(T { context.progress ? value(to) : value(from) });
    }

#if !LOG_DISABLED
    void log(const CoordinatedValueListValueType& destination, const CoordinatedValueListValueType& from, const CoordinatedValueListValueType& to, const RenderStyle&, const RenderStyle&, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << this->property() << " from " << value(from) << " to " << value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(destination));
    }
#endif

private:
    GetterType value(const CoordinatedValueListValueType& list) const
    {
        return (list.*m_getter)();
    }

    GetterType (CoordinatedValueListValueType::*m_getter)() const;
    void (CoordinatedValueListValueType::*m_setter)(SetterType);
};

// Deduction guide for getter/setters that return and take values.
template<typename T, typename CoordinatedValueListValueType>
DiscreteCoordinatedValueListPropertyWrapper(CSSPropertyID, T (CoordinatedValueListValueType::*getter)() const, void (CoordinatedValueListValueType::*setter)(T)) -> DiscreteCoordinatedValueListPropertyWrapper<T, CoordinatedValueListValueType, T, T>;

// Deduction guide for getter/setters that return const references and take r-value references.
template<typename T, typename CoordinatedValueListValueType>
DiscreteCoordinatedValueListPropertyWrapper(CSSPropertyID, const T& (CoordinatedValueListValueType::*getter)() const, void (CoordinatedValueListValueType::*setter)(T&&)) -> DiscreteCoordinatedValueListPropertyWrapper<T, CoordinatedValueListValueType, const T&, T&&>;

// Deduction guide for getter/setters that return values and take r-value references.
template<typename T, typename CoordinatedValueListValueType>
DiscreteCoordinatedValueListPropertyWrapper(CSSPropertyID, T (CoordinatedValueListValueType::*getter)() const, void (CoordinatedValueListValueType::*setter)(T&&)) -> DiscreteCoordinatedValueListPropertyWrapper<T, CoordinatedValueListValueType, T, T&&>;

template<typename T, typename RepeatedValueWrapper>
class CoordinatedValueListPropertyWrapper final : public WrapperBase {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(CoordinatedValueListPropertyWrapper, Animation);
public:
    using List = T;
    using CoordinatedValueListValueType = typename List::value_type;

    using ListGetter = const List& (ComputedStyleBase::*)() const;
    using ListAccessor = List& (ComputedStyleBase::*)();
    using ListSetter = void (ComputedStyleBase::*)(List&&);

    CoordinatedValueListPropertyWrapper(CSSPropertyID property, ListGetter getter, ListAccessor accessor, ListSetter setter, RepeatedValueWrapper repeatedValueWrapper)
        : WrapperBase(property)
        , m_listGetter(getter)
        , m_listAccessor(accessor)
        , m_listSetter(setter)
        , m_repeatedValueWrapper(repeatedValueWrapper)
    {
    }

    bool equals(const RenderStyle& from, const RenderStyle& to) const final
    {
        if (&from == &to)
            return true;

        auto& fromList = (from.computedStyle().*m_listGetter)();
        auto& toList = (to.computedStyle().*m_listGetter)();

        auto numberOfFromValues = fromList.computedLength();
        auto numberOfToValues = toList.computedLength();
        auto numberOfValues = std::min(numberOfFromValues, numberOfToValues);

        for (size_t i = 0; i < numberOfValues; ++i) {
            auto& fromValue = fromList[i];
            auto& toValue = toList[i];

            if (!m_repeatedValueWrapper.equals(fromValue, toValue, from, to))
                return false;
        }

        return true;
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation operation) const final
    {
        auto& fromList = (from.computedStyle().*m_listGetter)();
        auto& toList = (to.computedStyle().*m_listGetter)();

        auto numberOfFromValues = fromList.computedLength();
        auto numberOfToValues = toList.computedLength();
        auto numberOfValues = std::min(numberOfFromValues, numberOfToValues);

        for (size_t i = 0; i < numberOfValues; ++i) {
            auto& fromValue = fromList[i];
            auto& toValue = toList[i];

            // First check if the owner values allow interpolation.
            if (!Style::canBlend(fromValue, toValue, from, to, operation))
                return false;

            // Then check if the individual property values allow interpolation.
            if (!m_repeatedValueWrapper.canInterpolate(fromValue, toValue, from, to, operation))
                return false;
        }

        return true;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        auto* fromList = &(from.computedStyle().*m_listGetter)();
        auto* toList = &(to.computedStyle().*m_listGetter)();
        auto& destinationList = (destination.computedStyle().*m_listAccessor)();

        if (context.isDiscrete) {
            ASSERT(!context.progress || context.progress == 1.0);
            auto* list = context.progress ? toList : fromList;
            fromList = list;
            toList = list;
        }

        auto numberOfFromValues = fromList->computedLength();
        auto numberOfToValues = toList->computedLength();
        auto numberOfDestinationValues = destinationList.computedLength();
        auto numberOfValues = std::min(numberOfFromValues, numberOfToValues);

        for (size_t i = 0; i < numberOfValues; ++i) {
            if (i >= numberOfDestinationValues)
                destinationList.append(typename List::value_type { });

            m_repeatedValueWrapper.interpolate(destinationList[i], (*fromList)[i], (*toList)[i], from, to, context);
        }

        destinationList.prepareForUse();
    }

#if !LOG_DISABLED
    void log(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        auto& fromList = (from.computedStyle().*m_listGetter)();
        auto& toList = (to.computedStyle().*m_listGetter)();
        auto& destinationList = (destination.computedStyle().*m_listGetter)();

        auto numberOfFromValues = fromList.computedLength();
        auto numberOfToValues = toList.computedLength();
        auto numberOfDestinationValues = destinationList.computedLength();
        auto numberOfValues = std::min({numberOfFromValues, numberOfToValues, numberOfDestinationValues});

        for (size_t i = 0; i < numberOfValues; ++i)
            m_repeatedValueWrapper.log(destinationList[i], fromList[i], toList[i], from, to, progress);
    }
#endif

private:
    ListGetter m_listGetter;
    ListAccessor m_listAccessor;
    ListSetter m_listSetter;
    RepeatedValueWrapper m_repeatedValueWrapper;
};

// MARK: - Shorthand Wrapper

class ShorthandWrapper final : public WrapperBase {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(ShorthandWrapper, Animation);
public:
    ShorthandWrapper(CSSPropertyID property, Vector<WrapperBase*> longhandWrappers)
        : WrapperBase(property)
        , m_longhandWrappers(WTF::move(longhandWrappers))
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        if (&a == &b)
            return true;

        for (auto& wrapper : m_longhandWrappers) {
            if (!wrapper->equals(a, b))
                return false;
        }

        return true;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        for (auto& wrapper : m_longhandWrappers)
            wrapper->interpolate(destination, from, to, context);
    }

#if !LOG_DISABLED
    void log(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        for (auto& wrapper : m_longhandWrappers)
            wrapper->log(from, to, destination, progress);
    }
#endif

private:
    Vector<WrapperBase*> m_longhandWrappers;
};

} // namespace WebCore::Style::Interpolation
