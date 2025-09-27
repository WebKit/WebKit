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
#include "StylePrimitiveNumericTypes+Logging.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/TextStream.h>

namespace WebCore::Style::Interpolation {

// MARK: - Base Wrappers

template<typename T, typename GetterType = T>
class WrapperWithGetter : public WrapperBase {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(WrapperWithGetter, Animation);
public:
    WrapperWithGetter(CSSPropertyID property, GetterType (RenderStyle::*getter)() const)
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
    GetterType (RenderStyle::*m_getter)() const;
};

template<typename T, typename GetterType = T, typename SetterType = T>
class Wrapper : public WrapperWithGetter<T, GetterType> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Wrapper, Animation);
public:
    Wrapper(CSSPropertyID property, GetterType (RenderStyle::*getter)() const, void (RenderStyle::*setter)(SetterType))
        : WrapperWithGetter<T, GetterType>(property, getter)
        , m_setter(setter)
    {
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const override
    {
        (destination.*m_setter)(blendFunc(this->value(from), this->value(to), context));
    }

protected:
    void (RenderStyle::*m_setter)(SetterType);
};

// Deduction guide for getter/setters that return and take values.
template<typename T>
Wrapper(CSSPropertyID, T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T)) -> Wrapper<T, T, T>;

// Deduction guide for getter/setters that return const references and take r-value references.
template<typename T>
Wrapper(CSSPropertyID, const T& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T&&)) -> Wrapper<T, const T&, T&&>;

// MARK: - Typed Wrappers

template<typename T, typename GetterType = T, typename SetterType = T>
class StyleTypeWrapper : public WrapperBase {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StyleTypeWrapper, Animation);
public:
    StyleTypeWrapper(CSSPropertyID property, GetterType (RenderStyle::*getter)() const, void (RenderStyle::*setter)(SetterType))
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

    GetterType (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(SetterType);
};

// Deduction guide for getter/setters that return and take values.
template<typename T>
StyleTypeWrapper(CSSPropertyID, T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T)) -> StyleTypeWrapper<T, T, T>;

// Deduction guide for getter/setters that return const references and take r-value references.
template<typename T>
StyleTypeWrapper(CSSPropertyID, const T& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T&&)) -> StyleTypeWrapper<T, const T&, T&&>;

// Deduction guide for getter/setters that return values and take r-value references.
template<typename T>
StyleTypeWrapper(CSSPropertyID, T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T&&)) -> StyleTypeWrapper<T, T, T&&>;

template<typename T> class VisitedAffectedStyleTypeWrapper : public WrapperBase {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(VisitedAffectedStyleTypeWrapper, Animation);
public:
    VisitedAffectedStyleTypeWrapper(CSSPropertyID property, const T& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T&&), const T& (RenderStyle::*visitedGetter)() const, void (RenderStyle::*visitedSetter)(T&&))
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

class LengthWrapper : public WrapperWithGetter<const WebCore::Length&> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(LengthWrapper, Animation);
public:
    enum class Flags {
        IsLengthPercentage          = 1 << 0,
        NegativeLengthsAreInvalid   = 1 << 1,
    };
    LengthWrapper(CSSPropertyID property, const WebCore::Length& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(WebCore::Length&&), OptionSet<Flags> flags = { })
        : WrapperWithGetter(property, getter)
        , m_setter(setter)
        , m_flags(flags)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const override
    {
        return canInterpolateLengths(value(from), value(to), m_flags.contains(Flags::IsLengthPercentage));
    }

    bool requiresInterpolationForAccumulativeIteration(const RenderStyle& from, const RenderStyle& to) const final
    {
        return lengthsRequireInterpolationForAccumulativeIteration(value(from), value(to));
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const override
    {
        auto valueRange = m_flags.contains(Flags::NegativeLengthsAreInvalid) ? ValueRange::NonNegative : ValueRange::All;
        (destination.*m_setter)(blendFunc(value(from), value(to), context, valueRange));
    }

private:
    void (RenderStyle::*m_setter)(WebCore::Length&&);
    OptionSet<Flags> m_flags;
};

// MARK: - Discrete Wrappers

template<typename T, typename GetterType = T, typename SetterType = T> class DiscreteWrapper : public WrapperWithGetter<T, GetterType> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(DiscreteWrapper, Animation);
public:
    DiscreteWrapper(CSSPropertyID property, GetterType (RenderStyle::*getter)() const, void (RenderStyle::*setter)(SetterType))
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
    void (RenderStyle::*m_setter)(SetterType);
};

// Deduction guide for getter/setters that return and take values.
template<typename T>
DiscreteWrapper(CSSPropertyID, T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T)) -> DiscreteWrapper<T, T, T>;

// Deduction guide for getter/setters that return const references and take r-value references.
template<typename T>
DiscreteWrapper(CSSPropertyID, const T& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T&&)) -> DiscreteWrapper<T, const T&, T&&>;

// Deduction guide for getter/setters that return values and take r-value references.
template<typename T>
DiscreteWrapper(CSSPropertyID, T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T&&)) -> DiscreteWrapper<T, T, T&&>;

template<typename T>
class NonNormalizedDiscreteWrapper final : public Wrapper<T> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(NonNormalizedDiscreteWrapper, Animation);
public:
    NonNormalizedDiscreteWrapper(CSSPropertyID property, T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T))
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
        : Wrapper<float>(CSSPropertyID::CSSPropertyFontSize, &RenderStyle::computedFontSize, &RenderStyle::setFontSize)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return a.specifiedFontSize() == b.specifiedFontSize();
    }
};

class DiscreteFontDescriptionWrapper : public WrapperBase {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(DiscreteFontDescriptionWrapper, Animation);
public:
    DiscreteFontDescriptionWrapper(CSSPropertyID property)
        : WrapperBase(property)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const override
    {
        return propertiesInFontDescriptionAreEqual(a.fontDescription(), b.fontDescription());
    }

    bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const override
    {
        return false;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const override
    {
        ASSERT(!context.progress || context.progress == 1.0);
        auto destinationDescription = destination.fontDescription();
        auto& sourceDescription = (context.progress ? to : from).fontDescription();
        setPropertiesInFontDescription(sourceDescription, destinationDescription);
        destination.setFontDescription(WTFMove(destinationDescription));
    }

#if !LOG_DISABLED
    void log(const RenderStyle&, const RenderStyle&, const RenderStyle&, double) const override
    {
    }
#endif

protected:
    virtual bool propertiesInFontDescriptionAreEqual(const FontCascadeDescription&, const FontCascadeDescription&) const { return false; }
    virtual void setPropertiesInFontDescription(const FontCascadeDescription&, FontCascadeDescription&) const { }
};

template<typename T>
class DiscreteFontDescriptionTypedWrapper final : public DiscreteFontDescriptionWrapper {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(DiscreteFontDescriptionTypedWrapper, Animation);
public:
    DiscreteFontDescriptionTypedWrapper(CSSPropertyID property, T (FontCascadeDescription::*getter)() const, void (FontCascadeDescription::*setter)(T))
        : DiscreteFontDescriptionWrapper(property)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

private:
    bool propertiesInFontDescriptionAreEqual(const FontCascadeDescription& a, const FontCascadeDescription& b) const override
    {
        return this->value(a) == this->value(b);
    }

    void setPropertiesInFontDescription(const FontCascadeDescription& source, FontCascadeDescription& destination) const override
    {
        (destination.*this->m_setter)(this->value(source));
    }

    T value(const FontCascadeDescription& description) const
    {
        return (description.*this->m_getter)();
    }

    T (FontCascadeDescription::*m_getter)() const;
    void (FontCascadeDescription::*m_setter)(T);
};

class FontFeatureSettingsWrapper final : public DiscreteFontDescriptionWrapper {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(FontFeatureSettingsWrapper, Animation);
public:
    FontFeatureSettingsWrapper()
        : DiscreteFontDescriptionWrapper(CSSPropertyFontFeatureSettings)
    {
    }

private:
    bool propertiesInFontDescriptionAreEqual(const FontCascadeDescription& a, const FontCascadeDescription& b) const override
    {
        return a.featureSettings() == b.featureSettings();
    }

    void setPropertiesInFontDescription(const FontCascadeDescription& source, FontCascadeDescription& destination) const override
    {
        destination.setFeatureSettings(FontFeatureSettings(source.featureSettings()));
    }
};

class FontVariantEastAsianWrapper final : public DiscreteFontDescriptionWrapper {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(FontVariantEastAsianWrapper, Animation);
public:
    FontVariantEastAsianWrapper()
        : DiscreteFontDescriptionWrapper(CSSPropertyFontVariantEastAsian)
    {
    }

private:
    bool propertiesInFontDescriptionAreEqual(const FontCascadeDescription& a, const FontCascadeDescription& b) const override
    {
        return a.variantEastAsianVariant() == b.variantEastAsianVariant()
            && a.variantEastAsianWidth() == b.variantEastAsianWidth()
            && a.variantEastAsianRuby() == b.variantEastAsianRuby();
    }

    void setPropertiesInFontDescription(const FontCascadeDescription& source, FontCascadeDescription& destination) const override
    {
        destination.setVariantEastAsianVariant(source.variantEastAsianVariant());
        destination.setVariantEastAsianWidth(source.variantEastAsianWidth());
        destination.setVariantEastAsianRuby(source.variantEastAsianRuby());
    }
};

class FontVariantLigaturesWrapper final : public DiscreteFontDescriptionWrapper {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(FontVariantLigaturesWrapper, Animation);
public:
    FontVariantLigaturesWrapper()
        : DiscreteFontDescriptionWrapper(CSSPropertyFontVariantLigatures)
    {
    }

private:
    bool propertiesInFontDescriptionAreEqual(const FontCascadeDescription& a, const FontCascadeDescription& b) const override
    {
        return a.variantCommonLigatures() == b.variantCommonLigatures()
            && a.variantDiscretionaryLigatures() == b.variantDiscretionaryLigatures()
            && a.variantHistoricalLigatures() == b.variantHistoricalLigatures()
            && a.variantContextualAlternates() == b.variantContextualAlternates();
    }

    void setPropertiesInFontDescription(const FontCascadeDescription& source, FontCascadeDescription& destination) const override
    {
        destination.setVariantCommonLigatures(source.variantCommonLigatures());
        destination.setVariantDiscretionaryLigatures(source.variantDiscretionaryLigatures());
        destination.setVariantHistoricalLigatures(source.variantHistoricalLigatures());
        destination.setVariantContextualAlternates(source.variantContextualAlternates());
    }
};

class FontFamilyWrapper final : public DiscreteFontDescriptionWrapper {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(FontFamilyWrapper, Animation);
public:
    FontFamilyWrapper()
        : DiscreteFontDescriptionWrapper(CSSPropertyFontFamily)
    {
    }

private:
    bool propertiesInFontDescriptionAreEqual(const FontCascadeDescription& a, const FontCascadeDescription& b) const override
    {
        return a.families() == b.families();
    }

    void setPropertiesInFontDescription(const FontCascadeDescription& source, FontCascadeDescription& destination) const override
    {
        destination.setFamilies(source.families());
    }
};

class FontVariantNumericWrapper final : public DiscreteFontDescriptionWrapper {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(FontVariantNumericWrapper, Animation);
public:
    FontVariantNumericWrapper()
        : DiscreteFontDescriptionWrapper(CSSPropertyFontVariantNumeric)
    {
    }

private:
    bool propertiesInFontDescriptionAreEqual(const FontCascadeDescription& a, const FontCascadeDescription& b) const override
    {
        return a.variantNumericFigure() == b.variantNumericFigure()
            && a.variantNumericSpacing() == b.variantNumericSpacing()
            && a.variantNumericFraction() == b.variantNumericFraction()
            && a.variantNumericOrdinal() == b.variantNumericOrdinal()
            && a.variantNumericSlashedZero() == b.variantNumericSlashedZero();
    }

    void setPropertiesInFontDescription(const FontCascadeDescription& source, FontCascadeDescription& destination) const override
    {
        destination.setVariantNumericFigure(source.variantNumericFigure());
        destination.setVariantNumericSpacing(source.variantNumericSpacing());
        destination.setVariantNumericFraction(source.variantNumericFraction());
        destination.setVariantNumericOrdinal(source.variantNumericOrdinal());
        destination.setVariantNumericSlashedZero(source.variantNumericSlashedZero());
    }
};

#if ENABLE(VARIATION_FONTS)

class FontVariationSettingsWrapper final : public Wrapper<FontVariationSettings> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(FontVariationSettingsWrapper, Animation);
public:
    FontVariationSettingsWrapper()
        : Wrapper(CSSPropertyFontVariationSettings, &RenderStyle::fontVariationSettings, &RenderStyle::setFontVariationSettings)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        // If the style pointers are the same, don't bother doing the test.
        if (&a == &b)
            return true;
        return value(a) == value(b);
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        auto fromVariationSettings = value(from);
        auto toVariationSettings = value(to);

        if (fromVariationSettings.size() != toVariationSettings.size())
            return false;

        auto size = fromVariationSettings.size();
        for (unsigned i = 0; i < size; ++i) {
            if (fromVariationSettings.at(i).tag() != toVariationSettings.at(i).tag())
                return false;
        }

        return true;
    }
};

#endif

class LineHeightWrapper final : public LengthWrapper {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(LineHeightWrapper, Animation);
public:
    LineHeightWrapper()
        : LengthWrapper(CSSPropertyLineHeight, &RenderStyle::specifiedLineHeight, &RenderStyle::setLineHeight)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation compositeOperation) const final
    {
        // We must account for how BuilderConverter::convertLineHeight() deals with line-height values:
        // - "normal" is converted to LengthType::Percent with a -100 value
        // - <number> values are converted to LengthType::Percent
        // - <length-percentage> values are converted to LengthType::Fixed
        // This means that animating between "normal" and a "<number>" would work with LengthWrapper::canInterpolate()
        // since it would see two LengthType::Percent values. So if either value is "normal" we cannot interpolate since those
        // values are either equal or of incompatible types.
        auto normalLineHeight = RenderStyle::initialLineHeight();
        if (value(from) == normalLineHeight || value(to) == normalLineHeight)
            return false;

        // The default logic will now apply since <number> and <length-percentage> values
        // are converted to different LengthType values.
        return LengthWrapper::canInterpolate(from, to, compositeOperation);
    }
};

// MARK: - Color Property Wrappers

class ColorWrapper final : public WrapperWithGetter<const WebCore::Color&> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(ColorWrapper, Animation);
public:
    ColorWrapper(CSSPropertyID property, const WebCore::Color& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(WebCore::Color&&))
        : WrapperWithGetter<const WebCore::Color&>(property, getter)
        , m_setter(setter)
    {
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const override
    {
        (destination.*m_setter)(blendFunc(value(from), value(to), context));
    }

private:
    void (RenderStyle::*m_setter)(WebCore::Color&&);
};

class VisitedAffectedColorWrapper final : public WrapperBase {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(VisitedAffectedColorWrapper, Animation);
public:
    VisitedAffectedColorWrapper(CSSPropertyID property, const WebCore::Color& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(WebCore::Color&&), const WebCore::Color& (RenderStyle::*visitedGetter)() const, void (RenderStyle::*visitedSetter)(WebCore::Color&&))
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

class AccentColorWrapper final : public StyleTypeWrapper<Color, const Color&, Color&&> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(AccentColorWrapper, Animation);
public:
    AccentColorWrapper()
        : StyleTypeWrapper(CSSPropertyAccentColor, &RenderStyle::accentColor, &RenderStyle::setAccentColor)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return a.hasAutoAccentColor() == b.hasAutoAccentColor()
            && StyleTypeWrapper::equals(a, b);
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return !from.hasAutoAccentColor() && !to.hasAutoAccentColor();
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        if (canInterpolate(from, to, context.compositeOperation)) {
            StyleTypeWrapper::interpolate(destination, from, to, context);
            return;
        }

        ASSERT(!context.progress || context.progress == 1.0);
        auto& blendingRenderStyle = context.progress ? to : from;
        if (blendingRenderStyle.hasAutoAccentColor())
            destination.setHasAutoAccentColor();
        else
            destination.setAccentColor(Color { blendingRenderStyle.accentColor() });
    }
};

class CaretColorWrapper final : public VisitedAffectedStyleTypeWrapper<Color> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(CaretColorWrapper, Animation);
public:
    CaretColorWrapper()
        : VisitedAffectedStyleTypeWrapper<Color>(CSSPropertyCaretColor, &RenderStyle::caretColor, &RenderStyle::setCaretColor, &RenderStyle::visitedLinkCaretColor, &RenderStyle::setVisitedLinkCaretColor)
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
        : Wrapper(CSSPropertyVisibility, &RenderStyle::visibility, &RenderStyle::setVisibility)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        // https://drafts.csswg.org/web-animations-1/#animating-visibility
        // If neither value is visible, then discrete animation is used.
        return value(from) == Visibility::Visible || value(to) == Visibility::Visible;
    }
};

template<typename T, typename GetterType = T, typename SetterType = T>
class DiscreteSVGWrapper final : public WrapperBase {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(DiscreteSVGWrapper, Animation);
public:
    DiscreteSVGWrapper(CSSPropertyID property, GetterType (SVGRenderStyle::*getter)() const, void (SVGRenderStyle::*setter)(SetterType))
        : WrapperBase(property)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return this->value(a) == this->value(b);
    }

    bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const final
    {
        return false;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        ASSERT(!context.progress || context.progress == 1.0);
        (destination.accessSVGStyle().*this->m_setter)(T { this->value(context.progress ? to : from) });
    }

#if !LOG_DISABLED
    void log(const RenderStyle&, const RenderStyle&, const RenderStyle&, double) const final
    {
    }
#endif

private:
    T value(const RenderStyle& style) const
    {
        return (style.svgStyle().*this->m_getter)();
    }

    GetterType (SVGRenderStyle::*m_getter)() const;
    void (SVGRenderStyle::*m_setter)(SetterType);
};

// Deduction guide for getter/setters that return and take values.
template<typename T>
DiscreteSVGWrapper(CSSPropertyID, T (SVGRenderStyle::*getter)() const, void (SVGRenderStyle::*setter)(T)) -> DiscreteSVGWrapper<T, T, T>;

// Deduction guide for getter/setters that return const references and take r-value references.
template<typename T>
DiscreteSVGWrapper(CSSPropertyID, const T& (SVGRenderStyle::*getter)() const, void (SVGRenderStyle::*setter)(T&&)) -> DiscreteSVGWrapper<T, const T&, T&&>;

// MARK: - FillLayer Wrappers

// Wrapper base class for an animatable property in a FillLayer
template<typename FillLayerType>
class FillLayerWrapperBase {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(FillLayerWrapperBase, Animation);
public:
    FillLayerWrapperBase(CSSPropertyID property)
        : m_property(property)
    {
    }
    virtual ~FillLayerWrapperBase() = default;

    CSSPropertyID property() const { return m_property; }

    virtual bool equals(const FillLayerType&, const FillLayerType&) const = 0;
    virtual void interpolate(FillLayerType&, const FillLayerType&, const FillLayerType&, const Context&) const = 0;
    virtual bool canInterpolate(const FillLayerType&, const FillLayerType&) const { return true; }
#if !LOG_DISABLED
    virtual void log(const FillLayerType& destination, const FillLayerType&, const FillLayerType&, double) const = 0;
#endif

private:
    CSSPropertyID m_property;
};

template<typename StyleType, typename FillLayerType>
class FillLayerStyleTypeWrapper final : public FillLayerWrapperBase<FillLayerType> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(FillLayerStyleTypeWrapper, Animation);
public:
    FillLayerStyleTypeWrapper(CSSPropertyID property, const StyleType& (FillLayerType::*getter)() const, void (FillLayerType::*setter)(StyleType&&))
        : FillLayerWrapperBase<FillLayerType>(property)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

    bool equals(const FillLayerType& from, const FillLayerType& to) const override
    {
        if (&from == &to)
            return true;
        return Style::equalsForBlending(value(from), value(to));
    }

    bool canInterpolate(const FillLayerType& from, const FillLayerType& to) const override final
    {
        return Style::canBlend(value(from), value(to));
    }

    void interpolate(FillLayerType& destination, const FillLayerType& from, const FillLayerType& to, const Context& context) const override final
    {
        (destination.*m_setter)(Style::blend(value(from), value(to), context));
    }

#if !LOG_DISABLED
    void log(const FillLayerType& destination, const FillLayerType& from, const FillLayerType& to, double progress) const override final
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << this->property() << " from " << value(from) << " to " << value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(destination));
    }
#endif

private:
    const StyleType& value(const FillLayerType& layer) const
    {
        return (layer.*m_getter)();
    }

    const StyleType& (FillLayerType::*m_getter)() const;
    void (FillLayerType::*m_setter)(StyleType&&);
};

template<typename T, typename FillLayerType, typename GetterType = T, typename SetterType = T>
class DiscreteFillLayerWrapper final : public FillLayerWrapperBase<FillLayerType> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(DiscreteFillLayerWrapper, Animation);
public:
    DiscreteFillLayerWrapper(CSSPropertyID property, GetterType (FillLayerType::*getter)() const, void (FillLayerType::*setter)(SetterType))
        : FillLayerWrapperBase<FillLayerType>(property)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

    bool equals(const FillLayerType& a, const FillLayerType& b) const final
    {
        return value(a) == value(b);
    }

    bool canInterpolate(const FillLayerType&, const FillLayerType&) const final
    {
        return false;
    }

    void interpolate(FillLayerType& destination, const FillLayerType& from, const FillLayerType& to, const Context& context) const final
    {
        ASSERT(!context.progress || context.progress == 1.0);
        (destination.*m_setter)(T { context.progress ? value(to) : value(from) });
    }

#if !LOG_DISABLED
    void log(const FillLayerType& destination, const FillLayerType& from, const FillLayerType& to, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << this->property() << " from " << value(from) << " to " << value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(destination));
    }
#endif

private:
    GetterType value(const FillLayerType& fillLayer) const
    {
        return (fillLayer.*m_getter)();
    }

    GetterType (FillLayerType::*m_getter)() const;
    void (FillLayerType::*m_setter)(SetterType);
};

// Deduction guide for getter/setters that return and take values.
template<typename T, typename FillLayerType>
DiscreteFillLayerWrapper(CSSPropertyID, T (FillLayerType::*getter)() const, void (FillLayerType::*setter)(T)) -> DiscreteFillLayerWrapper<T, FillLayerType, T, T>;

// Deduction guide for getter/setters that return const references and take r-value references.
template<typename T, typename FillLayerType>
DiscreteFillLayerWrapper(CSSPropertyID, const T& (FillLayerType::*getter)() const, void (FillLayerType::*setter)(T&&)) -> DiscreteFillLayerWrapper<T, FillLayerType, const T&, T&&>;

// Deduction guide for getter/setters that return values and take r-value references.
template<typename T, typename FillLayerType>
DiscreteFillLayerWrapper(CSSPropertyID, T (FillLayerType::*getter)() const, void (FillLayerType::*setter)(T&&)) -> DiscreteFillLayerWrapper<T, FillLayerType, T, T&&>;

template<typename T, typename RepeatedValueWrapper>
class FillLayersWrapper final : public WrapperBase {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(FillLayersWrapper, Animation);
public:
    using Layers = T;
    using Layer = typename Layers::Layer;

    using LayersGetter = const Layers& (RenderStyle::*)() const;
    using LayersAccessor = Layers& (RenderStyle::*)();
    using LayersSetter = void (RenderStyle::*)(Layers&&);

    FillLayersWrapper(CSSPropertyID property, LayersGetter getter, LayersAccessor accessor, LayersSetter setter, RepeatedValueWrapper repeatedValueWrapper)
        : WrapperBase(property)
        , m_layersGetter(getter)
        , m_layersAccessor(accessor)
        , m_layersSetter(setter)
        , m_repeatedValueWrapper(repeatedValueWrapper)
    {
    }

    bool equals(const RenderStyle& from, const RenderStyle& to) const final
    {
        if (&from == &to)
            return true;

        auto& fromLayers = (from.*m_layersGetter)();
        auto& toLayers = (to.*m_layersGetter)();

        auto numberOfFromLayers = fromLayers.size();
        auto numberOfToLayers = toLayers.size();
        auto numberOfLayers = std::min(numberOfFromLayers, numberOfToLayers);

        for (size_t i = 0; i < numberOfLayers; ++i) {
            auto& fromLayer = fromLayers[i];
            auto& toLayer = toLayers[i];

            if (!m_repeatedValueWrapper.equals(fromLayer, toLayer))
                return false;
        }

        return true;
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        auto& fromLayers = (from.*m_layersGetter)();
        auto& toLayers = (to.*m_layersGetter)();

        auto numberOfFromLayers = fromLayers.size();
        auto numberOfToLayers = toLayers.size();
        auto numberOfLayers = std::min(numberOfFromLayers, numberOfToLayers);

        for (size_t i = 0; i < numberOfLayers; ++i) {
            auto& fromLayer = fromLayers[i];
            auto& toLayer = toLayers[i];

            if (!fromLayer.size().hasSameType(toLayer.size()))
                return false;

            if (!m_repeatedValueWrapper.canInterpolate(fromLayer, toLayer))
                return false;
        }

        return true;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        auto* fromLayers = &(from.*m_layersGetter)();
        auto* toLayers = &(to.*m_layersGetter)();
        auto& destinationLayers = (destination.*m_layersAccessor)();

        if (context.isDiscrete) {
            ASSERT(!context.progress || context.progress == 1.0);
            auto* layers = context.progress ? toLayers : fromLayers;
            fromLayers = layers;
            toLayers = layers;
        }

        auto numberOfFromLayers = fromLayers->size();
        auto numberOfToLayers = toLayers->size();
        auto numberOfDestinationLayers = destinationLayers.size();
        auto numberOfLayers = std::min(numberOfFromLayers, numberOfToLayers);

        if (numberOfLayers > numberOfDestinationLayers) {
            (destination.*m_layersSetter)(
                Layers {
                    Layers::Container::createWithSizeFromGenerator(numberOfLayers, [&](const auto& i) {
                        auto destinationLayer = destinationLayers[i % numberOfDestinationLayers];
                        m_repeatedValueWrapper.interpolate(destinationLayer, (*fromLayers)[i], (*toLayers)[i], context);
                        return destinationLayer;
                    })
                }
            );
        } else {
            for (size_t i = 0; i < numberOfLayers; ++i)
                m_repeatedValueWrapper.interpolate(destinationLayers[i], (*fromLayers)[i], (*toLayers)[i], context);
        }
    }

#if !LOG_DISABLED
    void log(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        auto& fromLayers = (from.*m_layersGetter)();
        auto& toLayers = (to.*m_layersGetter)();
        auto& destinationLayers = (destination.*m_layersGetter)();

        auto numberOfFromLayers = fromLayers.size();
        auto numberOfToLayers = toLayers.size();
        auto numberOfDestinationLayers = destinationLayers.size();
        auto numberOfLayers = std::min({numberOfFromLayers, numberOfToLayers, numberOfDestinationLayers});

        for (size_t i = 0; i < numberOfLayers; ++i)
            m_repeatedValueWrapper.log(destinationLayers[i], fromLayers[i], toLayers[i], progress);
    }
#endif

private:
    LayersGetter m_layersGetter;
    LayersAccessor m_layersAccessor;
    LayersSetter m_layersSetter;
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
