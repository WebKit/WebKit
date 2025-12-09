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
    WrapperWithGetter(CSSPropertyID property, GetterType (RenderStyleProperties::*getter)() const)
        : WrapperBase(property)
        , m_getter(getter)
    {
    }

    GetterType value(const RenderStyle& style) const
    {
        return (style.*m_getter)();
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
    GetterType (RenderStyleProperties::*m_getter)() const;
};

template<typename T, typename GetterType = T, typename SetterType = T>
class Wrapper : public WrapperWithGetter<T, GetterType> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Wrapper, Animation);
public:
    Wrapper(CSSPropertyID property, GetterType (RenderStyleProperties::*getter)() const, void (RenderStyleProperties::*setter)(SetterType))
        : WrapperWithGetter<T, GetterType>(property, getter)
        , m_setter(setter)
    {
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const override
    {
        (destination.*m_setter)(blendFunc(this->value(from), this->value(to), context));
    }

protected:
    void (RenderStyleProperties::*m_setter)(SetterType);
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
    StyleTypeWrapper(CSSPropertyID property, GetterType (RenderStyleProperties::*getter)() const, void (RenderStyleProperties::*setter)(SetterType))
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
        (destination.*m_setter)(Style::blend(this->value(from), this->value(to), from, to, context));
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
        return (style.*m_getter)();
    }

    GetterType (RenderStyleProperties::*m_getter)() const;
    void (RenderStyleProperties::*m_setter)(SetterType);
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

template<typename T> class VisitedAffectedStyleTypeWrapper : public WrapperBase {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(VisitedAffectedStyleTypeWrapper, Animation);
public:
    VisitedAffectedStyleTypeWrapper(CSSPropertyID property, const T& (RenderStyleProperties::*getter)() const, void (RenderStyleProperties::*setter)(T&&), const T& (RenderStyleProperties::*visitedGetter)() const, void (RenderStyleProperties::*visitedSetter)(T&&))
        : WrapperBase(property)
        , m_wrapper(StyleTypeWrapper<T, const T&, T&&>(property, getter, setter))
        , m_visitedWrapper(StyleTypeWrapper<T, const T&, T&&>(property, visitedGetter, visitedSetter))
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const override
    {
        return m_wrapper.equals(a, b) && m_visitedWrapper.equals(a, b);
    }

    bool requiresInterpolationForAccumulativeIteration(const RenderStyle& a, const RenderStyle& b) const override
    {
        return m_wrapper.requiresInterpolationForAccumulativeIteration(a, b) && m_visitedWrapper.requiresInterpolationForAccumulativeIteration(a, b);
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const override
    {
        m_wrapper.interpolate(destination, from, to, context);
        m_visitedWrapper.interpolate(destination, from, to, context);
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
};

// MARK: - Discrete Wrappers

template<typename T, typename GetterType = T, typename SetterType = T> class DiscreteWrapper : public WrapperWithGetter<T, GetterType> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(DiscreteWrapper, Animation);
public:
    DiscreteWrapper(CSSPropertyID property, GetterType (RenderStyleProperties::*getter)() const, void (RenderStyleProperties::*setter)(SetterType))
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
        (destination.*this->m_setter)(T { this->value(context.progress ? to : from) });
    }

private:
    void (RenderStyleProperties::*m_setter)(SetterType);
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
    NonNormalizedDiscreteWrapper(CSSPropertyID property, T (RenderStyleProperties::*getter)() const, void (RenderStyleProperties::*setter)(T))
        : Wrapper<T>(property, getter, setter)
    {
    }

    bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const final
    {
        return false;
    }
};

// MARK: - Color Property Wrappers

class ColorWrapper final : public WrapperWithGetter<const WebCore::Color&> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(ColorWrapper, Animation);
public:
    ColorWrapper(CSSPropertyID property, const WebCore::Color& (RenderStyleProperties::*getter)() const, void (RenderStyleProperties::*setter)(WebCore::Color&&))
        : WrapperWithGetter<const WebCore::Color&>(property, getter)
        , m_setter(setter)
    {
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const override
    {
        (destination.*m_setter)(blendFunc(value(from), value(to), context));
    }

private:
    void (RenderStyleProperties::*m_setter)(WebCore::Color&&);
};

class VisitedAffectedColorWrapper final : public WrapperBase {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(VisitedAffectedColorWrapper, Animation);
public:
    VisitedAffectedColorWrapper(CSSPropertyID property, const WebCore::Color& (RenderStyleProperties::*getter)() const, void (RenderStyleProperties::*setter)(WebCore::Color&&), const WebCore::Color& (RenderStyleProperties::*visitedGetter)() const, void (RenderStyleProperties::*visitedSetter)(WebCore::Color&&))
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

class CaretColorWrapper final : public VisitedAffectedStyleTypeWrapper<Color> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(CaretColorWrapper, Animation);
public:
    CaretColorWrapper()
        : VisitedAffectedStyleTypeWrapper<Color>(CSSPropertyCaretColor, &RenderStyleProperties::caretColor, &RenderStyleProperties::setCaretColor, &RenderStyleProperties::visitedLinkCaretColor, &RenderStyleProperties::setVisitedLinkCaretColor)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return a.hasAutoCaretColor() == b.hasAutoCaretColor()
            && a.hasVisitedLinkAutoCaretColor() == b.hasVisitedLinkAutoCaretColor()
            && VisitedAffectedStyleTypeWrapper<Color>::equals(a, b);
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return canInterpolateCaretColor(from, to, false) || canInterpolateCaretColor(from, to, true);
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        if (canInterpolateCaretColor(from, to, false))
            m_wrapper.interpolate(destination, from, to, context);
        else {
            auto& blendingRenderStyle = context.progress < 0.5 ? from : to;
            if (blendingRenderStyle.hasAutoCaretColor())
                destination.setHasAutoCaretColor();
            else
                destination.setCaretColor(Color { blendingRenderStyle.caretColor() });
        }

        if (canInterpolateCaretColor(from, to, true))
            m_visitedWrapper.interpolate(destination, from, to, context);
        else {
            auto& blendingRenderStyle = context.progress < 0.5 ? from : to;
            if (blendingRenderStyle.hasVisitedLinkAutoCaretColor())
                destination.setHasVisitedLinkAutoCaretColor();
            else
                destination.setVisitedLinkCaretColor(Color { blendingRenderStyle.visitedLinkCaretColor() });
        }
    }

private:
    static bool canInterpolateCaretColor(const RenderStyle& from, const RenderStyle& to, bool visited)
    {
        if (visited)
            return !from.hasVisitedLinkAutoCaretColor() && !to.hasVisitedLinkAutoCaretColor();
        return !from.hasAutoCaretColor() && !to.hasAutoCaretColor();
    }
};

// MARK: - Other Custom Wrappers

class CounterWrapper final : public WrapperBase {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(CounterWrapper, Animation);
public:
    CounterWrapper(CSSPropertyID property)
        : WrapperBase(property)
    {
        ASSERT(property == CSSPropertyCounterIncrement || property == CSSPropertyCounterReset || property == CSSPropertyCounterSet);
    }

    bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const final
    {
        return false;
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        auto& mapA = a.counterDirectives().map;
        auto& mapB = b.counterDirectives().map;
        if (mapA.size() != mapB.size())
            return false;
        for (auto& [key, aDirective] : mapA) {
            auto it = mapB.find(key);
            if (it == mapB.end())
                return false;
            auto& bDirective = it->value;
            if ((property() == CSSPropertyCounterIncrement && aDirective.incrementValue != bDirective.incrementValue)
                || (property() == CSSPropertyCounterReset && aDirective.resetValue != bDirective.resetValue)
                || (property() == CSSPropertyCounterSet && aDirective.setValue != bDirective.setValue))
                return false;
        }
        return true;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        ASSERT(context.isDiscrete);
        ASSERT(!context.progress || context.progress == 1);

        // Clear all existing values in the existing set of directives.
        for (auto& [key, directive] : destination.accessCounterDirectives().map) {
            if (property() == CSSPropertyCounterIncrement)
                directive.incrementValue = std::nullopt;
            else if (property() == CSSPropertyCounterReset)
                directive.resetValue = std::nullopt;
            else
                directive.setValue = std::nullopt;
        }

        auto& style = context.progress ? to : from;
        auto& targetDirectives = destination.accessCounterDirectives().map;
        for (auto& [key, directive] : style.counterDirectives().map) {
            auto updateDirective = [&](CounterDirectives& target, const CounterDirectives& source) {
                if (property() == CSSPropertyCounterIncrement)
                    target.incrementValue = source.incrementValue;
                else if (property() == CSSPropertyCounterReset)
                    target.resetValue = source.resetValue;
                else
                    target.setValue = source.setValue;
            };
            auto it = targetDirectives.find(key);
            if (it == targetDirectives.end())
                updateDirective(targetDirectives.add(key, CounterDirectives { }).iterator->value, directive);
            else
                updateDirective(it->value, directive);
        }
    }

#if !LOG_DISABLED
    void log(const RenderStyle&, const RenderStyle&, const RenderStyle&, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << " blending " << property() << " at " << TextStream::FormatNumberRespectingIntegers(progress) << ".");
    }
#endif
};

class VisibilityWrapper final : public Wrapper<Visibility> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(VisibilityWrapper, Animation);
public:
    VisibilityWrapper()
        : Wrapper(CSSPropertyVisibility, &RenderStyleProperties::visibility, &RenderStyleProperties::setVisibility)
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

    virtual bool equals(const CoordinatedValueListValueType&, const CoordinatedValueListValueType&) const = 0;
    virtual void interpolate(CoordinatedValueListValueType&, const CoordinatedValueListValueType&, const CoordinatedValueListValueType&, const Context&) const = 0;
    virtual bool canInterpolate(const CoordinatedValueListValueType&, const CoordinatedValueListValueType&) const { return true; }
#if !LOG_DISABLED
    virtual void log(const CoordinatedValueListValueType& destination, const CoordinatedValueListValueType&, const CoordinatedValueListValueType&, double) const = 0;
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

    bool equals(const CoordinatedValueListValueType& from, const CoordinatedValueListValueType& to) const override
    {
        if (&from == &to)
            return true;
        return Style::equalsForBlending(value(from), value(to));
    }

    bool canInterpolate(const CoordinatedValueListValueType& from, const CoordinatedValueListValueType& to) const override final
    {
        return Style::canBlend(value(from), value(to));
    }

    void interpolate(CoordinatedValueListValueType& destination, const CoordinatedValueListValueType& from, const CoordinatedValueListValueType& to, const Context& context) const override final
    {
        (destination.*m_setter)(Style::blend(value(from), value(to), context));
    }

#if !LOG_DISABLED
    void log(const CoordinatedValueListValueType& destination, const CoordinatedValueListValueType& from, const CoordinatedValueListValueType& to, double progress) const override final
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

    bool equals(const CoordinatedValueListValueType& a, const CoordinatedValueListValueType& b) const final
    {
        return value(a) == value(b);
    }

    bool canInterpolate(const CoordinatedValueListValueType&, const CoordinatedValueListValueType&) const final
    {
        return false;
    }

    void interpolate(CoordinatedValueListValueType& destination, const CoordinatedValueListValueType& from, const CoordinatedValueListValueType& to, const Context& context) const final
    {
        ASSERT(!context.progress || context.progress == 1.0);
        (destination.*m_setter)(T { context.progress ? value(to) : value(from) });
    }

#if !LOG_DISABLED
    void log(const CoordinatedValueListValueType& destination, const CoordinatedValueListValueType& from, const CoordinatedValueListValueType& to, double progress) const final
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

    using ListGetter = const List& (RenderStyleBase::*)() const;
    using ListAccessor = List& (RenderStyleBase::*)();
    using ListSetter = void (RenderStyleBase::*)(List&&);

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

        auto& fromList = (from.*m_listGetter)();
        auto& toList = (to.*m_listGetter)();

        auto numberOfFromValues = fromList.computedLength();
        auto numberOfToValues = toList.computedLength();
        auto numberOfValues = std::min(numberOfFromValues, numberOfToValues);

        for (size_t i = 0; i < numberOfValues; ++i) {
            auto& fromValue = fromList[i];
            auto& toValue = toList[i];

            if (!m_repeatedValueWrapper.equals(fromValue, toValue))
                return false;
        }

        return true;
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        auto& fromList = (from.*m_listGetter)();
        auto& toList = (to.*m_listGetter)();

        auto numberOfFromValues = fromList.computedLength();
        auto numberOfToValues = toList.computedLength();
        auto numberOfValues = std::min(numberOfFromValues, numberOfToValues);

        for (size_t i = 0; i < numberOfValues; ++i) {
            auto& fromValue = fromList[i];
            auto& toValue = toList[i];

            // First check if the owner values allow interpolation.
            if (!Style::canBlend(fromValue, toValue))
                return false;

            // Then check if the individual property values allow interpolation.
            if (!m_repeatedValueWrapper.canInterpolate(fromValue, toValue))
                return false;
        }

        return true;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        auto* fromList = &(from.*m_listGetter)();
        auto* toList = &(to.*m_listGetter)();
        auto& destinationList = (destination.*m_listAccessor)();

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

            m_repeatedValueWrapper.interpolate(destinationList[i], (*fromList)[i], (*toList)[i], context);
        }

        destinationList.prepareForUse();
    }

#if !LOG_DISABLED
    void log(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        auto& fromList = (from.*m_listGetter)();
        auto& toList = (to.*m_listGetter)();
        auto& destinationList = (destination.*m_listGetter)();

        auto numberOfFromValues = fromList.computedLength();
        auto numberOfToValues = toList.computedLength();
        auto numberOfDestinationValues = destinationList.computedLength();
        auto numberOfValues = std::min({numberOfFromValues, numberOfToValues, numberOfDestinationValues});

        for (size_t i = 0; i < numberOfValues; ++i)
            m_repeatedValueWrapper.log(destinationList[i], fromList[i], toList[i], progress);
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
        , m_longhandWrappers(WTFMove(longhandWrappers))
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
