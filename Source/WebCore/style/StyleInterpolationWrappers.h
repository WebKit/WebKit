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

template<typename T>
class WrapperWithGetter : public WrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    WrapperWithGetter(CSSPropertyID property, T (RenderStyle::*getter)() const)
        : WrapperBase(property)
        , m_getter(getter)
    {
    }

    T value(const RenderStyle& style) const
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
    T (RenderStyle::*m_getter)() const;
};

template<typename T>
class Wrapper : public WrapperWithGetter<T> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    Wrapper(CSSPropertyID property, T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T))
        : WrapperWithGetter<T>(property, getter)
        , m_setter(setter)
    {
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const override
    {
        (destination.*m_setter)(blendFunc(this->value(from), this->value(to), context));
    }

protected:
    void (RenderStyle::*m_setter)(T);
};

template<typename T>
class RefCountedWrapper : public WrapperWithGetter<T*> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    RefCountedWrapper(CSSPropertyID property, T* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(RefPtr<T>&&))
        : WrapperWithGetter<T*>(property, getter)
        , m_setter(setter)
    {
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        (destination.*this->m_setter)(blendFunc(this->value(from), this->value(to), context));
    }

private:
    void (RenderStyle::*m_setter)(RefPtr<T>&&);
};

// MARK: - Typed Wrappers

template<typename StyleType>
class StyleTypeWrapper final : public WrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    StyleTypeWrapper(CSSPropertyID property, const StyleType& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(StyleType&&))
        : WrapperBase(property)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        if (&a == &b)
            return true;
        return this->value(a) == this->value(b);
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return Style::canBlend(this->value(from), this->value(to));
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        (destination.*m_setter)(Style::blend(this->value(from), this->value(to), context));
    }

#if !LOG_DISABLED
    void log(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << property() << " from " << this->value(from) << " to " << this->value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << this->value(destination));
    }
#endif

    const StyleType& value(const RenderStyle& style) const
    {
        return (style.*m_getter)();
    }

    const StyleType& (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(StyleType&&);
};

template<typename T>
class AutoWrapper final : public Wrapper<T> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    AutoWrapper(CSSPropertyID property, T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T), bool (RenderStyle::*autoGetter)() const, void (RenderStyle::*autoSetter)(), std::optional<T> minValue = std::nullopt)
        : Wrapper<T>(property, getter, setter)
        , m_autoGetter(autoGetter)
        , m_autoSetter(autoSetter)
        , m_minValue(minValue)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return !(from.*m_autoGetter)() && !(to.*m_autoGetter)();
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        auto blendedValue = blendFunc(this->value(from), this->value(to), context);
        if (m_minValue)
            blendedValue = blendedValue > *m_minValue ? blendedValue : *m_minValue;
        (destination.*this->m_setter)(blendedValue);

        if (!context.isDiscrete)
            return;

        ASSERT(!context.progress || context.progress == 1.0);
        if (!context.progress) {
            if ((from.*m_autoGetter)())
                (destination.*m_autoSetter)();
        } else {
            if ((to.*m_autoGetter)())
                (destination.*m_autoSetter)();
        }
    }

private:
    bool (RenderStyle::*m_autoGetter)() const;
    void (RenderStyle::*m_autoSetter)();
    std::optional<T> m_minValue;
};

class FloatWrapper : public Wrapper<float> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    enum class ValueRange : uint8_t {
        All,
        NonNegative,
        Positive
    };
    FloatWrapper(CSSPropertyID property, float (RenderStyle::*getter)() const, void (RenderStyle::*setter)(float), ValueRange valueRange = ValueRange::All)
        : Wrapper(property, getter, setter)
        , m_valueRange(valueRange)
    {
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const override
    {
        auto blendedValue = blendFunc(value(from), value(to), context);
        if (m_valueRange == ValueRange::NonNegative && blendedValue <= 0)
            blendedValue = 0;
        else if (m_valueRange == ValueRange::Positive && blendedValue < 0)
            blendedValue = std::numeric_limits<float>::epsilon();
        (destination.*m_setter)(blendedValue);
    }

private:
    ValueRange m_valueRange;
};

template<typename T>
class PositiveWrapper final : public Wrapper<T> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    PositiveWrapper(CSSPropertyID property, T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T))
        : Wrapper<T>(property, getter, setter)
    {
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        auto blendedValue = blendFunc(this->value(from), this->value(to), context);
        (destination.*this->m_setter)(blendedValue > 1 ? blendedValue : 1);
    }
};

class LengthWrapper : public WrapperWithGetter<const WebCore::Length&> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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

class LengthPointWrapper : public WrapperWithGetter<const LengthPoint&> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    LengthPointWrapper(CSSPropertyID property, const LengthPoint& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(LengthPoint))
        : WrapperWithGetter(property, getter)
        , m_setter(setter)
    {
    }

    bool requiresInterpolationForAccumulativeIteration(const RenderStyle& from, const RenderStyle& to) const final
    {
        auto fromLengthPoint = value(from);
        auto toLengthPoint = value(to);
        return lengthsRequireInterpolationForAccumulativeIteration(fromLengthPoint.x, toLengthPoint.x)
            || lengthsRequireInterpolationForAccumulativeIteration(fromLengthPoint.y, toLengthPoint.y);
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        (destination.*m_setter)(blendFunc(value(from), value(to), context));
    }

private:
    void (RenderStyle::*m_setter)(LengthPoint);
};

// This class extends LengthPointWrapper to accommodate `auto` or `normal` values expressed as
// LengthPoint(Length(LengthType::Auto/Normal), Length(LengthType::Auto/Normal)). This is used for
// offset-anchor and offset-position, which allows `auto` and `normal`, and is expressed like so.
class LengthPointOrAutoWrapper final : public LengthPointWrapper {
public:
    LengthPointOrAutoWrapper(CSSPropertyID property, const LengthPoint& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(LengthPoint))
        : LengthPointWrapper(property, getter, setter)
    {
    }

    // Check if it's possible to interpolate between the from and to values. In particular,
    // it's only possible if they're both not auto or normal.
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        auto valueFrom = value(from);
        auto valueTo = value(to);

        return !valueFrom.x.isAuto() && !valueTo.x.isAuto() && !valueFrom.x.isNormal() && !valueTo.x.isNormal();
    }
};

template<typename T>
class LengthVariantWrapper final : public WrapperWithGetter<const T&> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    LengthVariantWrapper(CSSPropertyID property, const T& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T&&))
        : WrapperWithGetter<const T&>(property, getter)
        , m_setter(setter)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return canInterpolateLengthVariants(this->value(from), this->value(to));
    }

    bool requiresInterpolationForAccumulativeIteration(const RenderStyle& from, const RenderStyle& to) const final
    {
        return lengthVariantRequiresInterpolationForAccumulativeIteration(this->value(from), this->value(to));
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        (destination.*m_setter)(blendFunc(this->value(from), this->value(to), context));
    }

private:
    void (RenderStyle::*m_setter)(T&&);
};

class OptionalLengthWrapper : public WrapperWithGetter<std::optional<WebCore::Length>> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    enum class Flags {
        IsLengthPercentage = 1 << 0,
        NegativeLengthsAreInvalid = 1 << 1,
    };
    OptionalLengthWrapper(CSSPropertyID property, std::optional<WebCore::Length> (RenderStyle::*getter)() const, void (RenderStyle::*setter)(std::optional<WebCore::Length>), OptionSet<Flags> flags = { })
        : WrapperWithGetter<std::optional<WebCore::Length>>(property, getter)
        , m_setter(setter)
        , m_flags(flags)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const override
    {
        if (!this->value(from) || !this->value(to))
            return false;

        bool isLengthPercentage = m_flags.contains(Flags::IsLengthPercentage);
        return canInterpolateLengths(*this->value(from), *this->value(to), isLengthPercentage);
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const override
    {
        if (context.isDiscrete) {
            ASSERT(!context.progress || context.progress == 1);
            (destination.*m_setter)(context.progress ? this->value(to) : this->value(from));
            return;
        }

        auto valueRange = m_flags.contains(Flags::NegativeLengthsAreInvalid) ? ValueRange::NonNegative : ValueRange::All;
        (destination.*m_setter)(blendFunc(*this->value(from), *this->value(to), context, valueRange));
    }

private:
    void (RenderStyle::*m_setter)(std::optional<WebCore::Length>);
    OptionSet<Flags> m_flags;
};

// MARK: - Discrete Wrappers

template<typename T>
class DiscreteWrapper : public WrapperWithGetter<T> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    DiscreteWrapper(CSSPropertyID property, T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T))
        : WrapperWithGetter<T>(property, getter)
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
        (destination.*this->m_setter)(this->value(context.progress ? to : from));
    }

private:
    void (RenderStyle::*m_setter)(T);
};

class DiscreteFontDescriptionWrapper : public WrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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

template<typename T>
class NonNormalizedDiscreteWrapper final : public Wrapper<T> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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

class ContentWrapper final : public WrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    ContentWrapper()
        : WrapperBase(CSSPropertyContent)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        if (!a.hasContent() && !b.hasContent())
            return true;
        if (a.hasContent() && b.hasContent())
            return *a.contentData() == *b.contentData();
        return false;
    }

    bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const final
    {
        return false;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        ASSERT(context.isDiscrete);
        ASSERT(!context.progress || context.progress == 1);

        auto& style = context.progress ? to : from;
        if (auto* content = style.contentData())
            destination.setContent(content->clone(), false);
        else
            destination.clearContent();
    }

#if !LOG_DISABLED
    void log(const RenderStyle&, const RenderStyle&, const RenderStyle&, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << " blending content at " << TextStream::FormatNumberRespectingIntegers(progress) << ".");
    }
#endif
};

class FontFeatureSettingsWrapper final : public DiscreteFontDescriptionWrapper {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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

class TextEmphasisStyleWrapper final : public DiscreteWrapper<TextEmphasisMark> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    TextEmphasisStyleWrapper()
        : DiscreteWrapper(CSSPropertyTextEmphasisStyle, &RenderStyle::textEmphasisMark, &RenderStyle::setTextEmphasisMark)
    {
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        destination.setTextEmphasisFill((context.progress > 0.5 ? to : from).textEmphasisFill());
        DiscreteWrapper::interpolate(destination, from, to, context);
    }
};

// MARK: - Customized Wrappers

class OffsetRotateWrapper final : public WrapperWithGetter<OffsetRotation> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    OffsetRotateWrapper()
        : WrapperWithGetter(CSSPropertyOffsetRotate, &RenderStyle::offsetRotate)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return value(from).canBlend(value(to));
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        destination.setOffsetRotate(value(from).blend(value(to), context));
    }
};

class GridTemplateWrapper final : public Wrapper<const GridTrackList&> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    GridTemplateWrapper(CSSPropertyID property, const GridTrackList& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const GridTrackList&))
        : Wrapper(property, getter, setter)
    {
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        (destination.*m_setter)(blendFunc(this->value(from), this->value(to), context));
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return WebCore::Style::Interpolation::canInterpolate(this->value(from), this->value(to));
    }
};

class NinePieceImageRepeatWrapper final : public WrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    NinePieceImageRepeatWrapper(CSSPropertyID property, NinePieceImageRule (RenderStyle::*horizontalGetter)() const, void (RenderStyle::*horizontalSetter)(NinePieceImageRule), NinePieceImageRule (RenderStyle::*verticalGetter)() const, void (RenderStyle::*verticalSetter)(NinePieceImageRule))
        : WrapperBase(property)
        , m_horizontalWrapper(DiscreteWrapper<NinePieceImageRule>(property, horizontalGetter, horizontalSetter))
        , m_verticalWrapper(DiscreteWrapper<NinePieceImageRule>(property, verticalGetter, verticalSetter))
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return m_horizontalWrapper.equals(a, b) && m_verticalWrapper.equals(a, b);
    }

    bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const final
    {
        return false;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        m_horizontalWrapper.interpolate(destination, from, to, context);
        m_verticalWrapper.interpolate(destination, from, to, context);
    }

#if !LOG_DISABLED
    void log(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        m_horizontalWrapper.log(from, to, destination, progress);
        m_verticalWrapper.log(from, to, destination, progress);
    }
#endif

    DiscreteWrapper<NinePieceImageRule> m_horizontalWrapper;
    DiscreteWrapper<NinePieceImageRule> m_verticalWrapper;
};

class ContainIntrinsicLengthWrapper final : public OptionalLengthWrapper {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    ContainIntrinsicLengthWrapper(CSSPropertyID property, std::optional<WebCore::Length> (RenderStyle::*getter)() const, void (RenderStyle::*setter)(std::optional<WebCore::Length>), ContainIntrinsicSizeType (RenderStyle::*typeGetter)() const, void (RenderStyle::*typeSetter)(ContainIntrinsicSizeType))
        : OptionalLengthWrapper(property, getter, setter, { Flags::NegativeLengthsAreInvalid })
        , m_containIntrinsicSizeTypeGetter(typeGetter)
        , m_containIntrinsicSizeTypeSetter(typeSetter)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation operation) const final
    {
        if ((from.*m_containIntrinsicSizeTypeGetter)() != (to.*m_containIntrinsicSizeTypeGetter)())
            return false;
        return OptionalLengthWrapper::canInterpolate(from, to, operation);
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        auto type = context.progress < 0.5 ? (from.*m_containIntrinsicSizeTypeGetter)() : (to.*m_containIntrinsicSizeTypeGetter)();
        (destination.*m_containIntrinsicSizeTypeSetter)(type);

        OptionalLengthWrapper::interpolate(destination, from, to, context);
    }

private:
    ContainIntrinsicSizeType (RenderStyle::*m_containIntrinsicSizeTypeGetter)() const;
    void (RenderStyle::*m_containIntrinsicSizeTypeSetter)(ContainIntrinsicSizeType);
};

class LengthBoxWrapper : public WrapperWithGetter<const LengthBox&> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    enum class Flags : uint8_t {
        IsLengthPercentage      = 1 << 0,
        UsesFillKeyword         = 1 << 1,
        AllowsNegativeValues    = 1 << 2,
        MayOverrideBorderWidths = 1 << 3,
    };
    LengthBoxWrapper(CSSPropertyID property, const LengthBox& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(LengthBox&&), OptionSet<Flags> flags = { })
        : WrapperWithGetter(property, getter)
        , m_setter(setter)
        , m_flags(flags)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const override
    {
        if (m_flags.contains(Flags::UsesFillKeyword)) {
            if (property() == CSSPropertyBorderImageSlice && from.borderImage().fill() != to.borderImage().fill())
                return false;
            if (property() == CSSPropertyMaskBorderSlice && from.maskBorder().fill() != to.maskBorder().fill())
                return false;
        }

        bool isLengthPercentage = m_flags.contains(Flags::IsLengthPercentage);

        if (m_flags.contains(Flags::MayOverrideBorderWidths)) {
            bool overridesBorderWidths = from.borderImage().overridesBorderWidths();
            if (overridesBorderWidths != to.borderImage().overridesBorderWidths())
                return false;
            // Even if this property accepts <length-percentage>, border widths can only be a <length>.
            if (overridesBorderWidths)
                isLengthPercentage = false;
        }

        auto& fromLengthBox = value(from);
        auto& toLengthBox = value(to);
        return canInterpolateLengths(fromLengthBox.top(), toLengthBox.top(), isLengthPercentage)
            && canInterpolateLengths(fromLengthBox.right(), toLengthBox.right(), isLengthPercentage)
            && canInterpolateLengths(fromLengthBox.bottom(), toLengthBox.bottom(), isLengthPercentage)
            && canInterpolateLengths(fromLengthBox.left(), toLengthBox.left(), isLengthPercentage);
    }

    bool requiresInterpolationForAccumulativeIteration(const RenderStyle& from, const RenderStyle& to) const final
    {
        auto& fromLengthBox = value(from);
        auto& toLengthBox = value(to);
        return lengthsRequireInterpolationForAccumulativeIteration(fromLengthBox.top(), toLengthBox.top())
            && lengthsRequireInterpolationForAccumulativeIteration(fromLengthBox.right(), toLengthBox.right())
            && lengthsRequireInterpolationForAccumulativeIteration(fromLengthBox.bottom(), toLengthBox.bottom())
            && lengthsRequireInterpolationForAccumulativeIteration(fromLengthBox.left(), toLengthBox.left());
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const override
    {
        if (m_flags.contains(Flags::UsesFillKeyword)) {
            if (property() == CSSPropertyBorderImageSlice)
                destination.setBorderImageSliceFill((!context.progress || !context.isDiscrete ? from : to).borderImage().fill());
            else if (property() == CSSPropertyMaskBorderSlice)
                destination.setMaskBorderSliceFill((!context.progress || !context.isDiscrete ? from : to).maskBorder().fill());
        }
        if (m_flags.contains(Flags::MayOverrideBorderWidths))
            destination.setBorderImageWidthOverridesBorderWidths((!context.progress || !context.isDiscrete ? from : to).borderImage().overridesBorderWidths());
        if (context.isDiscrete) {
            // It is important we have this non-interpolated shortcut because certain CSS properties
            // represented as a LengthBox, such as border-image-slice, don't know how to deal with
            // calculated Length values, see for instance valueForImageSliceSide(const Length&).
            (destination.*m_setter)(context.progress ? LengthBox(value(to)) : LengthBox(value(from)));
            return;
        }
        auto valueRange = m_flags.contains(Flags::AllowsNegativeValues) ? ValueRange::All : ValueRange::NonNegative;
        (destination.*m_setter)(blendFunc(value(from), value(to), context, valueRange));
    }

private:
    void (RenderStyle::*m_setter)(LengthBox&&);
    OptionSet<Flags> m_flags;
};

class ClipWrapper final : public LengthBoxWrapper {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    ClipWrapper()
        : LengthBoxWrapper(CSSPropertyClip, &RenderStyle::clip, &RenderStyle::setClip, { LengthBoxWrapper::Flags::AllowsNegativeValues })
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation compositeOperation) const final
    {
        return from.hasClip() && to.hasClip() && LengthBoxWrapper::canInterpolate(from, to, compositeOperation);
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        LengthBoxWrapper::interpolate(destination, from, to, context);
        destination.setHasClip(true);
    }
};

class PathOperationWrapper final : public RefCountedWrapper<PathOperation> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    PathOperationWrapper(CSSPropertyID property, PathOperation* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(RefPtr<PathOperation>&&))
        : RefCountedWrapper(property, getter, setter)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        // If the style pointers are the same, don't bother doing the test.
        if (&a == &b)
            return true;

        auto* clipPathA = value(a);
        auto* clipPathB = value(b);
        if (clipPathA == clipPathB)
            return true;
        if (!clipPathA || !clipPathB)
            return false;
        return *clipPathA == *clipPathB;
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const override
    {
        auto* fromPath = value(from);
        auto* toPath = value(to);
        return fromPath && toPath && fromPath->canBlend(*toPath);
    }
};

#if ENABLE(VARIATION_FONTS)

class FontVariationSettingsWrapper final : public Wrapper<FontVariationSettings> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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

class ShapeWrapper final : public RefCountedWrapper<ShapeValue> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    ShapeWrapper(CSSPropertyID property, ShapeValue* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(RefPtr<ShapeValue>&&))
        : RefCountedWrapper(property, getter, setter)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        // If the style pointers are the same, don't bother doing the test.
        if (&a == &b)
            return true;

        auto* shapeA = value(a);
        auto* shapeB = value(b);
        if (shapeA == shapeB)
            return true;
        if (!shapeA || !shapeB)
            return false;
        return *shapeA == *shapeB;
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        auto* fromShape = value(from);
        auto* toShape = value(to);
        return fromShape && toShape && fromShape->canBlend(*toShape);
    }
};

class StyleImageWrapper final : public RefCountedWrapper<StyleImage> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    StyleImageWrapper(CSSPropertyID property, StyleImage* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(RefPtr<StyleImage>&&))
        : RefCountedWrapper(property, getter, setter)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        if (&a == &b)
            return true;

        auto* imageA = value(a);
        auto* imageB = value(b);
        return arePointingToEqualData(imageA, imageB);
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return value(from) && value(to);
    }
};

class TransformOperationsWrapper final : public WrapperWithGetter<const TransformOperations&> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    TransformOperationsWrapper()
        : WrapperWithGetter<const TransformOperations&>(CSSPropertyTransform, &RenderStyle::transform)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation compositeOperation) const override
    {
        if (compositeOperation == CompositeOperation::Replace)
            return !this->value(to).shouldFallBackToDiscreteAnimation(this->value(from), { });
        return true;
    }

    bool requiresInterpolationForAccumulativeIteration(const RenderStyle&, const RenderStyle&) const final
    {
        return true;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const override
    {
        destination.setTransform(blendFunc(this->value(from), this->value(to), context));
    }
};

template<typename T>
class IndividualTransformWrapper final : public RefCountedWrapper<T> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    IndividualTransformWrapper(CSSPropertyID property, T* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(RefPtr<T>&&))
        : RefCountedWrapper<T>(property, getter, setter)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return arePointingToEqualData(this->value(a), this->value(b));
    }
};

class FilterWrapper final : public WrapperWithGetter<const FilterOperations&> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    FilterWrapper(CSSPropertyID property, const FilterOperations& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(FilterOperations&&))
        : WrapperWithGetter<const FilterOperations&>(property, getter)
        , m_setter(setter)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation compositeOperation) const final
    {
        return value(from).canInterpolate(value(to), compositeOperation);
    }

    bool requiresInterpolationForAccumulativeIteration(const RenderStyle&, const RenderStyle&) const final
    {
        return true;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        (destination.*m_setter)(blendFunc(value(from), value(to), context));
    }

    void (RenderStyle::*m_setter)(FilterOperations&&);
};

inline const ShadowData* shadowForInterpolation(const ShadowData* srcShadow, const ShadowData* otherShadow)
{
    static NeverDestroyed<ShadowData> defaultShadowData {
        BoxShadow {
            .color = { WebCore::Color::transparentBlack },
            .location = { { 0 }, { 0 } },
            .blur = { 0 },
            .spread = { 0 },
            .inset = std::nullopt,
            .isWebkitBoxShadow = false
        }
    };
    static NeverDestroyed<ShadowData> defaultInsetShadowData {
        BoxShadow {
            .color = { WebCore::Color::transparentBlack },
            .location = { { 0 }, { 0 } },
            .blur = { 0 },
            .spread = { 0 },
            .inset = CSS::Keyword::Inset { },
            .isWebkitBoxShadow = false
        }
    };
    static NeverDestroyed<ShadowData> defaultWebKitBoxShadowData {
        BoxShadow {
            .color = { WebCore::Color::transparentBlack },
            .location = { { 0 }, { 0 } },
            .blur = { 0 },
            .spread = { 0 },
            .inset = std::nullopt,
            .isWebkitBoxShadow = true
        }
    };
    static NeverDestroyed<ShadowData> defaultInsetWebKitBoxShadowData {
        BoxShadow {
            .color = { WebCore::Color::transparentBlack },
            .location = { { 0 }, { 0 } },
            .blur = { 0 },
            .spread = { 0 },
            .inset = CSS::Keyword::Inset { },
            .isWebkitBoxShadow = true
        }
    };

    if (srcShadow)
        return srcShadow;

    if (otherShadow->style() == ShadowStyle::Inset)
        return otherShadow->isWebkitBoxShadow() ? &defaultInsetWebKitBoxShadowData.get() : &defaultInsetShadowData.get();

    return otherShadow->isWebkitBoxShadow() ? &defaultWebKitBoxShadowData.get() : &defaultShadowData.get();
}

class ShadowWrapper final : public WrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    ShadowWrapper(CSSPropertyID property, const ShadowData* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(std::unique_ptr<ShadowData>, bool))
        : WrapperBase(property)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        if (&a == &b)
            return true;

        auto* shadowA = (a.*m_getter)();
        auto* shadowB = (b.*m_getter)();

        while (true) {
            if (!shadowA || !shadowB)
                return shadowA == shadowB;

            if (*shadowA != *shadowB)
                return false;

            shadowA = shadowA->next();
            shadowB = shadowB->next();
        }

        return true;
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation compositeOperation) const final
    {
        if (compositeOperation != CompositeOperation::Replace)
            return true;

        auto* fromShadow = (from.*m_getter)();
        auto* toShadow = (to.*m_getter)();

        // The only scenario where we can't interpolate is if specified items don't have the same shadow style.
        while (fromShadow && toShadow) {
            if (fromShadow->style() != toShadow->style())
                return false;
            fromShadow = fromShadow->next();
            toShadow = toShadow->next();
        }

        return true;
    }

    bool requiresInterpolationForAccumulativeIteration(const RenderStyle&, const RenderStyle&) const final
    {
        return true;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        auto* fromShadow = (from.*m_getter)();
        auto* toShadow = (to.*m_getter)();

        if (context.isDiscrete) {
            ASSERT(!context.progress || context.progress == 1.0);
            auto* shadow = context.progress ? toShadow : fromShadow;
            (destination.*m_setter)(shadow ? makeUnique<ShadowData>(*shadow) : nullptr, false);
            return;
        }

        size_t fromLength = shadowListLength(fromShadow);
        size_t toLength = shadowListLength(toShadow);

        if (fromLength == toLength || (fromLength <= 1 && toLength <= 1)) {
            (destination.*m_setter)(blendSimpleOrMatchedShadowLists(fromShadow, toShadow, from, to, context), false);
            return;
        }

        (destination.*m_setter)(blendMismatchedShadowLists(fromShadow, toShadow, fromLength, toLength, from, to, context), false);
    }

#if !LOG_DISABLED
    void log(const RenderStyle&, const RenderStyle&, const RenderStyle&, double progress) const final
    {
        // FIXME: better logging.
        LOG_WITH_STREAM(Animations, stream << "  blending ShadowData at " << TextStream::FormatNumberRespectingIntegers(progress));
    }
#endif

    std::unique_ptr<ShadowData> addShadowLists(const ShadowData* shadowA, const ShadowData* shadowB) const
    {
        std::unique_ptr<ShadowData> newShadowData;
        ShadowData* lastShadow = nullptr;
        auto addShadows = [&](const ShadowData* shadow) {
            while (shadow) {
                auto blendedShadow = makeUnique<ShadowData>(*shadow);
                auto* blendedShadowPtr = blendedShadow.get();
                if (!lastShadow)
                    newShadowData = WTFMove(blendedShadow);
                else
                    lastShadow->setNext(WTFMove(blendedShadow));

                lastShadow = blendedShadowPtr;
                shadow = shadow ? shadow->next() : nullptr;
            }
        };
        addShadows(shadowB);
        addShadows(shadowA);
        return newShadowData;
    }

    std::unique_ptr<ShadowData> blendSimpleOrMatchedShadowLists(const ShadowData* shadowA, const ShadowData* shadowB, const RenderStyle& styleA, const RenderStyle& styleB, const Context& context) const
    {
        // from or to might be null in which case we don't want to do additivity, but do replace instead.
        if (shadowA && shadowB && context.compositeOperation == CompositeOperation::Add)
            return addShadowLists(shadowA, shadowB);

        std::unique_ptr<ShadowData> newShadowData;
        ShadowData* lastShadow = nullptr;

        while (shadowA || shadowB) {
            auto* srcShadow = shadowForInterpolation(shadowA, shadowB);
            auto* dstShadow = shadowForInterpolation(shadowB, shadowA);

            auto blendedShadow = blendFunc(srcShadow, dstShadow, styleA, styleB, context);
            auto* blendedShadowPtr = blendedShadow.get();

            if (!lastShadow)
                newShadowData = WTFMove(blendedShadow);
            else
                lastShadow->setNext(WTFMove(blendedShadow));

            lastShadow = blendedShadowPtr;

            shadowA = shadowA ? shadowA->next() : 0;
            shadowB = shadowB ? shadowB->next() : 0;
        }

        return newShadowData;
    }

    std::unique_ptr<ShadowData> blendMismatchedShadowLists(const ShadowData* shadowA, const ShadowData* shadowB, int fromLength, int toLength, const RenderStyle& styleA, const RenderStyle& styleB, const Context& context) const
    {
        if (shadowA && shadowB && context.compositeOperation != CompositeOperation::Replace)
            return addShadowLists(shadowA, shadowB);

        // The shadows in ShadowData are stored in reverse order, so when animating mismatched lists,
        // reverse them and match from the end.
        Vector<const ShadowData*, 4> fromShadows(fromLength);
        for (int i = fromLength - 1; i >= 0; --i) {
            fromShadows[i] = shadowA;
            shadowA = shadowA->next();
        }

        Vector<const ShadowData*, 4> toShadows(toLength);
        for (int i = toLength - 1; i >= 0; --i) {
            toShadows[i] = shadowB;
            shadowB = shadowB->next();
        }

        std::unique_ptr<ShadowData> newShadowData;

        int maxLength = std::max(fromLength, toLength);
        for (int i = 0; i < maxLength; ++i) {
            auto* fromShadow = i < fromLength ? fromShadows[i] : nullptr;
            auto* toShadow = i < toLength ? toShadows[i] : nullptr;

            auto* srcShadow = shadowForInterpolation(fromShadow, toShadow);
            auto* dstShadow = shadowForInterpolation(toShadow, fromShadow);

            auto blendedShadow = blendFunc(srcShadow, dstShadow, styleA, styleB, context);
            // Insert at the start of the list to preserve the order.
            blendedShadow->setNext(WTFMove(newShadowData));
            newShadowData = WTFMove(blendedShadow);
        }

        return newShadowData;
    }

private:
    const ShadowData* (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(std::unique_ptr<ShadowData>, bool);
};

class StyleColorWrapper : public WrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    StyleColorWrapper(CSSPropertyID property, const Style::Color& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Style::Color&))
        : WrapperBase(property)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const override
    {
        if (&a == &b)
            return true;

        auto& fromStyleColor = value(a);
        auto& toStyleColor = value(b);

        if (fromStyleColor.isCurrentColor() && toStyleColor.isCurrentColor())
            return true;

        if (fromStyleColor.isResolvedColor() && toStyleColor.isResolvedColor())
            return fromStyleColor.resolvedColor() == toStyleColor.resolvedColor();

        return a.colorResolvingCurrentColor(fromStyleColor) == b.colorResolvingCurrentColor(toStyleColor);
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const override
    {
        auto& fromStyleColor = value(from);
        auto& toStyleColor = value(to);

        // We don't animate on currentcolor-only transition.
        // https://github.com/WebKit/WebKit/blob/main/LayoutTests/imported/w3c/web-platform-tests/css/css-transitions/currentcolor-animation-001.html#L27
        if (fromStyleColor.isCurrentColor() && toStyleColor.isCurrentColor())
            return;

        auto fromColor = from.colorResolvingCurrentColor(fromStyleColor);
        auto toColor = to.colorResolvingCurrentColor(toStyleColor);

        auto result = blendFunc(fromColor, toColor, context);
        (destination.*m_setter)(WTFMove(result));
    }

#if !LOG_DISABLED
    void log(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        // FIXME: better logging.
        LOG_WITH_STREAM(Animations, stream << "  blending " << property() << " from " << value(from) << " to " << value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(destination));
    }
#endif

private:
    const Style::Color& value(const RenderStyle& style) const
    {
        return (style.*m_getter)();
    }

    const Style::Color& (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(const Style::Color&);
};

class ColorWrapper final : public WrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    ColorWrapper(CSSPropertyID property, const WebCore::Color& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const WebCore::Color&))
        : WrapperBase(property)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const override
    {
        if (&a == &b)
            return true;

        return value(a) == value(b);
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const override
    {
        auto result = blendFunc(value(from), value(to), context);
        (destination.*m_setter)(WTFMove(result));
    }

#if !LOG_DISABLED
    void log(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        // FIXME: better logging.
        LOG_WITH_STREAM(Animations, stream << "  blending " << property() << " from " << value(from) << " to " << value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(destination));
    }
#endif

private:
    const WebCore::Color& value(const RenderStyle& style) const
    {
        return (style.*m_getter)();
    }

    const WebCore::Color& (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(const WebCore::Color&);
};

class ScrollbarColorWrapper final : public WrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    ScrollbarColorWrapper()
        : WrapperBase(CSSPropertyScrollbarColor)
        , m_thumbWrapper(StyleColorWrapper(CSSPropertyScrollbarColor, &RenderStyle::scrollbarThumbColor, &RenderStyle::setScrollbarThumbColor))
        , m_trackWrapper(StyleColorWrapper(CSSPropertyScrollbarColor, &RenderStyle::scrollbarTrackColor, &RenderStyle::setScrollbarTrackColor))
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        bool aAuto = !a.scrollbarColor().has_value();
        bool bAuto = !b.scrollbarColor().has_value();

        if (aAuto || bAuto)
            return aAuto == bAuto;

        return m_thumbWrapper.equals(a, b) && m_trackWrapper.equals(a, b);
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return from.scrollbarColor().has_value() && to.scrollbarColor().has_value();
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        if (canInterpolate(from, to, context.compositeOperation)) {
            destination.setScrollbarColor(from.scrollbarColor().value());
            m_thumbWrapper.interpolate(destination, from, to, context);
            m_trackWrapper.interpolate(destination, from, to, context);
            return;
        }

        ASSERT(!context.progress || context.progress == 1.0);
        auto& blendingRenderStyle = context.progress ? to : from;
        destination.setScrollbarColor(blendingRenderStyle.scrollbarColor());
    }

#if !LOG_DISABLED
    void log(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        m_thumbWrapper.log(from, to, destination, progress);
        m_trackWrapper.log(from, to, destination, progress);
    }
#endif

private:
    StyleColorWrapper m_thumbWrapper;
    StyleColorWrapper m_trackWrapper;
};

class VisitedAffectedStyleColorWrapper : public WrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    VisitedAffectedStyleColorWrapper(CSSPropertyID property, const Style::Color& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Style::Color&), const Style::Color& (RenderStyle::*visitedGetter)() const, void (RenderStyle::*visitedSetter)(const Style::Color&))
        : WrapperBase(property)
        , m_wrapper(StyleColorWrapper(property, getter, setter))
        , m_visitedWrapper(StyleColorWrapper(property, visitedGetter, visitedSetter))
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const override
    {
        return m_wrapper.equals(a, b) && m_visitedWrapper.equals(a, b);
    }

    bool requiresInterpolationForAccumulativeIteration(const RenderStyle&, const RenderStyle&) const final
    {
        return true;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const override
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

    StyleColorWrapper m_wrapper;
    StyleColorWrapper m_visitedWrapper;
};

class VisitedAffectedColorWrapper final : public WrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    VisitedAffectedColorWrapper(CSSPropertyID property, const WebCore::Color& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const WebCore::Color&), const WebCore::Color& (RenderStyle::*visitedGetter)() const, void (RenderStyle::*visitedSetter)(const WebCore::Color&))
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

class AccentColorWrapper final : public StyleColorWrapper {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    AccentColorWrapper()
        : StyleColorWrapper(CSSPropertyAccentColor, &RenderStyle::accentColor, &RenderStyle::setAccentColor)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return a.hasAutoAccentColor() == b.hasAutoAccentColor()
            && StyleColorWrapper::equals(a, b);
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return !from.hasAutoAccentColor() && !to.hasAutoAccentColor();
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        if (canInterpolate(from, to, context.compositeOperation)) {
            StyleColorWrapper::interpolate(destination, from, to, context);
            return;
        }

        ASSERT(!context.progress || context.progress == 1.0);
        auto& blendingRenderStyle = context.progress ? to : from;
        if (blendingRenderStyle.hasAutoAccentColor())
            destination.setHasAutoAccentColor();
        else
            destination.setAccentColor(blendingRenderStyle.accentColor());
    }
};

class CaretColorWrapper final : public VisitedAffectedStyleColorWrapper {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    CaretColorWrapper()
        : VisitedAffectedStyleColorWrapper(CSSPropertyCaretColor, &RenderStyle::caretColor, &RenderStyle::setCaretColor, &RenderStyle::visitedLinkCaretColor, &RenderStyle::setVisitedLinkCaretColor)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return a.hasAutoCaretColor() == b.hasAutoCaretColor()
            && a.hasVisitedLinkAutoCaretColor() == b.hasVisitedLinkAutoCaretColor()
            && VisitedAffectedStyleColorWrapper::equals(a, b);
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
                destination.setCaretColor(blendingRenderStyle.caretColor());
        }

        if (canInterpolateCaretColor(from, to, true))
            m_visitedWrapper.interpolate(destination, from, to, context);
        else {
            auto& blendingRenderStyle = context.progress < 0.5 ? from : to;
            if (blendingRenderStyle.hasVisitedLinkAutoCaretColor())
                destination.setHasVisitedLinkAutoCaretColor();
            else
                destination.setVisitedLinkCaretColor(blendingRenderStyle.visitedLinkCaretColor());
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

class SVGPaintWrapper final : public WrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    SVGPaintWrapper(CSSPropertyID property, SVGPaintType (RenderStyle::*paintTypeGetter)() const, const Style::Color& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Style::Color&))
        : WrapperBase(property)
        , m_paintTypeGetter(paintTypeGetter)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        if (&a == &b)
            return true;

        if ((a.*m_paintTypeGetter)() != (b.*m_paintTypeGetter)())
            return false;

        // We only support animations between SVGPaints that are pure Color values.
        // For everything else we must return true for this method, otherwise
        // we will try to animate between values forever.
        if ((a.*m_paintTypeGetter)() == SVGPaintType::RGBColor) {
            auto fromStyleColor = (a.*m_getter)();
            auto toStyleColor = (b.*m_getter)();

            // We don't animate when both are currentcolor
            auto fromColor = a.colorResolvingCurrentColor(fromStyleColor);
            auto toColor = b.colorResolvingCurrentColor(toStyleColor);

            return (fromStyleColor.isCurrentColor() && toStyleColor.isCurrentColor()) || fromColor == toColor;
        }
        return true;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        auto isValidPaintType = [](SVGPaintType paintType) {
            return paintType == SVGPaintType::RGBColor || paintType == SVGPaintType::CurrentColor;
        };

        if (!isValidPaintType((from.*m_paintTypeGetter)()) || !isValidPaintType((to.*m_paintTypeGetter)()))
            return;

        auto fromStyleColor = (from.*m_getter)();
        auto toStyleColor = (to.*m_getter)();

        // We don't animate when both are currentcolor
        if (fromStyleColor.isCurrentColor() && toStyleColor.isCurrentColor())
            return;

        auto fromColor = from.colorResolvingCurrentColor(fromStyleColor);
        auto toColor = to.colorResolvingCurrentColor(toStyleColor);

        (destination.*m_setter)(blendFunc(fromColor, toColor, context));
    }

#if !LOG_DISABLED
    void log(const RenderStyle&, const RenderStyle&, const RenderStyle&, double progress) const final
    {
        // FIXME: better logging.
        LOG_WITH_STREAM(Animations, stream << "  blending SVGPaint at " << TextStream::FormatNumberRespectingIntegers(progress));
    }
#endif

private:
    SVGPaintType (RenderStyle::*m_paintTypeGetter)() const;
    const Style::Color& (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(const Style::Color&);
};

class VisitedAffectedSVGPaintWrapper final : public WrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    VisitedAffectedSVGPaintWrapper(CSSPropertyID property, SVGPaintType (RenderStyle::*paintTypeGetter)() const, const Style::Color& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Style::Color&), SVGPaintType (RenderStyle::*visitedPaintTypeGetter)() const, const Style::Color& (RenderStyle::*visitedGetter)() const, void (RenderStyle::*visitedSetter)(const Style::Color&))
        : WrapperBase(property)
        , m_wrapper(SVGPaintWrapper(property, paintTypeGetter, getter, setter))
        , m_visitedWrapper(SVGPaintWrapper(property, visitedPaintTypeGetter, visitedGetter, visitedSetter))
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return m_wrapper.equals(a, b) && m_visitedWrapper.equals(a, b);
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

private:
    SVGPaintWrapper m_wrapper;
    SVGPaintWrapper m_visitedWrapper;
};

class FontWeightWrapper final : public Wrapper<FontSelectionValue> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    FontWeightWrapper()
        : Wrapper(CSSPropertyFontWeight, &RenderStyle::fontWeight, &RenderStyle::setFontWeight)
    {
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        (destination.*m_setter)(FontSelectionValue(std::clamp(blendFunc(static_cast<float>(this->value(from)), static_cast<float>(this->value(to)), context), 1.0f, 1000.0f)));
    }
};

class FontStyleWrapper final : public Wrapper<std::optional<FontSelectionValue>> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    FontStyleWrapper()
        : Wrapper(CSSPropertyFontStyle, &RenderStyle::fontItalic, &RenderStyle::setFontItalic)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return from.fontDescription().fontStyleAxis() == FontStyleAxis::slnt && to.fontDescription().fontStyleAxis() == FontStyleAxis::slnt;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        auto blendedStyleAxis = FontStyleAxis::slnt;
        if (context.isDiscrete)
            blendedStyleAxis = (context.progress < 0.5 ? from : to).fontDescription().fontStyleAxis();

        auto fromFontItalic = from.fontItalic();
        auto toFontItalic = to.fontItalic();
        auto blendedFontItalic = context.progress < 0.5 ? fromFontItalic : toFontItalic;
        if (!context.isDiscrete)
            blendedFontItalic = blendFunc(fromFontItalic, toFontItalic, context);

        auto description = destination.fontDescription();
        description.setItalic(blendedFontItalic);
        description.setFontStyleAxis(blendedStyleAxis);
        destination.setFontDescription(WTFMove(description));
    }
};

class FontSizeAdjustWrapper final : public WrapperWithGetter<FontSizeAdjust> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    FontSizeAdjustWrapper()
        : WrapperWithGetter(CSSPropertyFontSizeAdjust, &RenderStyle::fontSizeAdjust)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        auto fromFontSizeAdjust = from.fontSizeAdjust();
        auto toFontSizeAdjust = to.fontSizeAdjust();
        return fromFontSizeAdjust.metric == toFontSizeAdjust.metric
            && fromFontSizeAdjust.value && toFontSizeAdjust.value;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        auto blendedFontSizeAdjust = [&]() -> FontSizeAdjust {
            if (context.isDiscrete)
                return (!context.progress ? from : to).fontSizeAdjust();

            ASSERT(from.fontSizeAdjust().value && to.fontSizeAdjust().value);
            auto blendedAdjust = blendFunc(*from.fontSizeAdjust().value, *to.fontSizeAdjust().value, context);

            ASSERT(from.fontSizeAdjust().metric == to.fontSizeAdjust().metric);
            return { to.fontSizeAdjust().metric, FontSizeAdjust::ValueType::Number, std::max(blendedAdjust, 0.0f) };
        };

        destination.setFontSizeAdjust(blendedFontSizeAdjust());
    }
};

class BaselineShiftWrapper final : public Wrapper<SVGLengthValue> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    BaselineShiftWrapper()
        : Wrapper(CSSPropertyBaselineShift, &RenderStyle::baselineShiftValue, &RenderStyle::setBaselineShiftValue)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return a.svgStyle().baselineShift() == b.svgStyle().baselineShift() && Wrapper::equals(a, b);
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation compositeOperation) const final
    {
        return from.svgStyle().baselineShift() == to.svgStyle().baselineShift() && Wrapper::canInterpolate(from, to, compositeOperation);
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        auto& srcSVGStyle = !context.progress ? from.svgStyle() : to.svgStyle();
        destination.accessSVGStyle().setBaselineShift(srcSVGStyle.baselineShift());
        Wrapper::interpolate(destination, from, to, context);
    }
};

class TextUnderlineOffsetWrapper final : public WrapperWithGetter<TextUnderlineOffset> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    TextUnderlineOffsetWrapper()
        : WrapperWithGetter(CSSPropertyTextUnderlineOffset, &RenderStyle::textUnderlineOffset)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        auto fromTextUnderlineOffset = from.textUnderlineOffset();
        auto toTextUnderlineOffset = to.textUnderlineOffset();
        if (fromTextUnderlineOffset.isAuto() || toTextUnderlineOffset.isAuto())
            return false;

        auto fromValue = fromTextUnderlineOffset.resolve(from.computedFontSize());
        auto toValue = toTextUnderlineOffset.resolve(to.computedFontSize());
        return fromValue != toValue;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        auto blendedTextUnderlineOffset = [&]() -> TextUnderlineOffset {
            if (context.isDiscrete)
                return (!context.progress ? from : to).textUnderlineOffset();

            auto fromTextUnderlineOffset = from.textUnderlineOffset();
            auto toTextUnderlineOffset = to.textUnderlineOffset();

            auto fromValue = fromTextUnderlineOffset.resolve(from.computedFontSize());
            auto toValue = toTextUnderlineOffset.resolve(to.computedFontSize());

            auto blendedValue = blendFunc(fromValue, toValue, context);
            return TextUnderlineOffset::createWithLength(WebCore::Length(clampTo<float>(blendedValue, minValueForCssLength, maxValueForCssLength), LengthType::Fixed));
        };

        destination.setTextUnderlineOffset(blendedTextUnderlineOffset());
    }
};

class TextDecorationThicknessWrapper final : public WrapperWithGetter<TextDecorationThickness> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    TextDecorationThicknessWrapper()
        : WrapperWithGetter(CSSPropertyTextDecorationThickness, &RenderStyle::textDecorationThickness)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        auto fromTextDecorationThickness = from.textDecorationThickness();
        auto toTextDecorationThickness = to.textDecorationThickness();
        if (fromTextDecorationThickness.isAuto() || toTextDecorationThickness.isAuto())
            return false;

        auto fromValue = fromTextDecorationThickness.resolve(from.computedFontSize(), from.metricsOfPrimaryFont());
        auto toValue = toTextDecorationThickness.resolve(to.computedFontSize(), to.metricsOfPrimaryFont());
        return fromValue != toValue;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        auto blendedTextDecorationThickness = [&]() -> TextDecorationThickness {
            if (context.isDiscrete)
                return (!context.progress ? from : to).textDecorationThickness();

            auto fromTextDecorationThickness = from.textDecorationThickness();
            auto toTextDecorationThickness = to.textDecorationThickness();

            auto fromValue = fromTextDecorationThickness.resolve(from.computedFontSize(), from.metricsOfPrimaryFont());
            auto toValue = toTextDecorationThickness.resolve(to.computedFontSize(), to.metricsOfPrimaryFont());

            auto blendedValue = blendFunc(fromValue, toValue, context);
            return TextDecorationThickness::createWithLength(WebCore::Length(clampTo<float>(blendedValue, minValueForCssLength, maxValueForCssLength), LengthType::Fixed));
        };

        destination.setTextDecorationThickness(blendedTextDecorationThickness());
    }
};

class LineHeightWrapper final : public LengthWrapper {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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

class VerticalAlignWrapper final : public LengthWrapper {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    VerticalAlignWrapper()
        : LengthWrapper(CSSPropertyVerticalAlign, &RenderStyle::verticalAlignLength, &RenderStyle::setVerticalAlignLength, LengthWrapper::Flags::IsLengthPercentage)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation compositeOperation) const final
    {
        return from.verticalAlign() == VerticalAlign::Length && to.verticalAlign() == VerticalAlign::Length && LengthWrapper::canInterpolate(from, to, compositeOperation);
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        LengthWrapper::interpolate(destination, from, to, context);
        auto& blendingStyle = context.isDiscrete && context.progress ? to : from;
        destination.setVerticalAlign(blendingStyle.verticalAlign());
    }
};

class TextIndentWrapper final : public LengthWrapper {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    TextIndentWrapper()
        : LengthWrapper(CSSPropertyTextIndent, &RenderStyle::textIndent, &RenderStyle::setTextIndent, LengthWrapper::Flags::IsLengthPercentage)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        if (a.textIndentLine() != b.textIndentLine())
            return false;
        if (a.textIndentType() != b.textIndentType())
            return false;
        return LengthWrapper::equals(a, b);
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation compositeOperation) const final
    {
        if (from.textIndentLine() != to.textIndentLine())
            return false;
        if (from.textIndentType() != to.textIndentType())
            return false;
        return LengthWrapper::canInterpolate(from, to, compositeOperation);
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        auto& blendingStyle = context.isDiscrete && context.progress ? to : from;
        destination.setTextIndentLine(blendingStyle.textIndentLine());
        destination.setTextIndentType(blendingStyle.textIndentType());
        LengthWrapper::interpolate(destination, from, to, context);
    }
};

class PerspectiveWrapper final : public FloatWrapper {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    PerspectiveWrapper()
        : FloatWrapper(CSSPropertyPerspective, &RenderStyle::perspective, &RenderStyle::setPerspective, FloatWrapper::ValueRange::NonNegative)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation compositeOperation) const final
    {
        if (!from.hasPerspective() || !to.hasPerspective())
            return false;
        return FloatWrapper::canInterpolate(from, to, compositeOperation);
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        if (context.isDiscrete)
            (destination.*m_setter)(context.progress ? value(to) : value(from));
        else
            FloatWrapper::interpolate(destination, from, to, context);
    }
};

class TabSizeWrapper final : public Wrapper<const TabSize&> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    TabSizeWrapper()
        : Wrapper(CSSPropertyTabSize, &RenderStyle::tabSize, &RenderStyle::setTabSize)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return value(from).isSpaces() == value(to).isSpaces();
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        if (context.isDiscrete)
            (destination.*m_setter)(context.progress ? value(to) : value(from));
        else
            Wrapper::interpolate(destination, from, to, context);
    }
};

class AspectRatioWrapper final : public WrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    AspectRatioWrapper()
        : WrapperBase(CSSPropertyAspectRatio)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        if (&a == &b)
            return true;

        return a.aspectRatioType() == b.aspectRatioType() && a.aspectRatioWidth() == b.aspectRatioWidth() && a.aspectRatioHeight() == b.aspectRatioHeight();
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return (from.aspectRatioType() == AspectRatioType::Ratio && to.aspectRatioType() == AspectRatioType::Ratio) || (from.aspectRatioType() == AspectRatioType::AutoAndRatio && to.aspectRatioType() == AspectRatioType::AutoAndRatio);
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        destination.setAspectRatioType(context.progress < 0.5 ? from.aspectRatioType() : to.aspectRatioType());
        if (!context.isDiscrete) {
            auto aspectRatioDst = WebCore::blend(std::log(from.logicalAspectRatio()), std::log(to.logicalAspectRatio()), context);
            destination.setAspectRatio(std::exp(aspectRatioDst), 1);
            return;
        }
        // For auto/auto-zero aspect-ratio we use discrete values, we can't use general
        // logic since logicalAspectRatio asserts on aspect-ratio type.
        ASSERT(!context.progress || context.progress == 1);
        auto& applicableStyle = context.progress ? to : from;
        destination.setAspectRatio(applicableStyle.aspectRatioWidth(), applicableStyle.aspectRatioHeight());
    }

#if !LOG_DISABLED
    void log(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << property() << " from " << from.logicalAspectRatio() << " to " << to.logicalAspectRatio() << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << destination.logicalAspectRatio());
    }
#endif
};

class CounterWrapper final : public WrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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

class GridTemplateAreasWrapper final : public WrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    GridTemplateAreasWrapper()
        : WrapperBase(CSSPropertyGridTemplateAreas)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return a.implicitNamedGridColumnLines().map == b.implicitNamedGridColumnLines().map
            && a.implicitNamedGridRowLines().map == b.implicitNamedGridRowLines().map
            && a.namedGridArea().map == b.namedGridArea().map
            && a.namedGridAreaRowCount() == b.namedGridAreaRowCount()
            && a.namedGridAreaColumnCount() == b.namedGridAreaColumnCount();
    }

    bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const final
    {
        return false;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        ASSERT(context.isDiscrete);
        ASSERT(!context.progress || context.progress == 1);

        auto& source = context.progress ? to : from;
        destination.setImplicitNamedGridColumnLines(source.implicitNamedGridColumnLines());
        destination.setImplicitNamedGridRowLines(source.implicitNamedGridRowLines());
        destination.setNamedGridArea(source.namedGridArea());
        destination.setNamedGridAreaRowCount(source.namedGridAreaRowCount());
        destination.setNamedGridAreaColumnCount(source.namedGridAreaColumnCount());
    }

#if !LOG_DISABLED
    void log(const RenderStyle&, const RenderStyle&, const RenderStyle&, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << " blending " << property() << " at " << TextStream::FormatNumberRespectingIntegers(progress) << ".");
    }
#endif
};


class QuotesWrapper final : public WrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    QuotesWrapper()
        : WrapperBase(CSSPropertyQuotes)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const override
    {
        return a.quotes() == b.quotes();
    }

    bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const override
    {
        return false;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const override
    {
        ASSERT(!context.progress || context.progress == 1.0);
        destination.setQuotes((context.progress ? to : from).quotes());
    }

#if !LOG_DISABLED
    void log(const RenderStyle&, const RenderStyle&, const RenderStyle&, double) const override
    {
    }
#endif
};

class VisibilityWrapper final : public Wrapper<Visibility> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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

template<typename T>
class DiscreteSVGWrapper final : public WrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    DiscreteSVGWrapper(CSSPropertyID property, T (SVGRenderStyle::*getter)() const, void (SVGRenderStyle::*setter)(T))
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
        (destination.accessSVGStyle().*this->m_setter)(this->value(context.progress ? to : from));
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

    T (SVGRenderStyle::*m_getter)() const;
    void (SVGRenderStyle::*m_setter)(T);
};

class DWrapper final : public RefCountedWrapper<StylePathData> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    DWrapper()
        : RefCountedWrapper(CSSPropertyD, &RenderStyle::d, &RenderStyle::setD)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        auto* fromValue = value(from);
        auto* toValue = value(to);
        return fromValue && toValue && fromValue->canBlend(*toValue);
    }
};

// MARK: - FillLayer Wrappers

// Wrapper base class for an animatable property in a FillLayer
class FillLayerWrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    FillLayerWrapperBase(CSSPropertyID property)
        : m_property(property)
    {
    }
    virtual ~FillLayerWrapperBase() = default;

    CSSPropertyID property() const { return m_property; }

    virtual bool equals(const FillLayer*, const FillLayer*) const = 0;
    virtual void interpolate(FillLayer*, const FillLayer*, const FillLayer*, const Context&) const = 0;
    virtual bool canInterpolate(const FillLayer*, const FillLayer*) const { return true; }
#if !LOG_DISABLED
    virtual void log(const FillLayer* destination, const FillLayer*, const FillLayer*, double) const = 0;
#endif

private:
    CSSPropertyID m_property;
};

template<typename T>
class FillLayerWrapperWithGetter : public FillLayerWrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
    WTF_MAKE_NONCOPYABLE(FillLayerWrapperWithGetter);
public:
    FillLayerWrapperWithGetter(CSSPropertyID property, T (FillLayer::*getter)() const)
        : FillLayerWrapperBase(property)
        , m_getter(getter)
    {
    }

protected:
    bool equals(const FillLayer* a, const FillLayer* b) const override
    {
        if (a == b)
            return true;
        if (!a || !b)
            return false;
        return value(a) == value(b);
    }

    T value(const FillLayer* layer) const
    {
        return (layer->*m_getter)();
    }

#if !LOG_DISABLED
    void log(const FillLayer* destination, const FillLayer* from, const FillLayer* to, double progress) const override
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << property() << " from " << value(from) << " to " << value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(destination));
    }
#endif

private:
    T (FillLayer::*m_getter)() const;
};

template<typename T>
class FillLayerWrapper final : public FillLayerWrapperWithGetter<const T&> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    FillLayerWrapper(CSSPropertyID property, const T& (FillLayer::*getter)() const, void (FillLayer::*setter)(T))
        : FillLayerWrapperWithGetter<const T&>(property, getter)
        , m_setter(setter)
    {
    }

private:
    void interpolate(FillLayer* destination, const FillLayer* from, const FillLayer* to, const Context& context) const final
    {
        (destination->*this->m_setter)(blendFunc(this->value(from), this->value(to), context));
    }

    bool canInterpolate(const FillLayer* from, const FillLayer* to) const final
    {
        return canInterpolateLengthVariants(this->value(from), this->value(to));
    }

#if !LOG_DISABLED
    void log(const FillLayer* destination, const FillLayer* from, const FillLayer* to, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << FillLayerWrapperWithGetter<const T&>::property()
            << " from " << FillLayerWrapperWithGetter<const T&>::value(from)
            << " to " << FillLayerWrapperWithGetter<const T&>::value(to)
            << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << FillLayerWrapperWithGetter<const T&>::value(destination));
    }
#endif

    void (FillLayer::*m_setter)(T);
};

class FillLayerPositionWrapper final : public FillLayerWrapperWithGetter<const WebCore::Length&> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    FillLayerPositionWrapper(CSSPropertyID property, const WebCore::Length& (FillLayer::*lengthGetter)() const, void (FillLayer::*lengthSetter)(WebCore::Length), Edge (FillLayer::*originGetter)() const, void (FillLayer::*originSetter)(Edge), Edge farEdge)
        : FillLayerWrapperWithGetter(property, lengthGetter)
        , m_lengthSetter(lengthSetter)
        , m_originGetter(originGetter)
        , m_originSetter(originSetter)
        , m_farEdge(farEdge)
    {
    }

private:
    bool equals(const FillLayer* a, const FillLayer* b) const final
    {
        if (a == b)
            return true;
        if (!a || !b)
            return false;

        auto fromLength = value(a);
        auto toLength = value(b);

        Edge fromEdge = (a->*m_originGetter)();
        Edge toEdge = (b->*m_originGetter)();

        return fromLength == toLength && fromEdge == toEdge;
    }

    void interpolate(FillLayer* destination, const FillLayer* from, const FillLayer* to, const Context& context) const final
    {
        auto fromLength = value(from);
        auto toLength = value(to);

        Edge fromEdge = (from->*m_originGetter)();
        Edge toEdge = (to->*m_originGetter)();

        Edge destinationEdge = toEdge;
        if (fromEdge != toEdge) {
            // Convert the right/bottom into a calc expression,
            if (fromEdge == m_farEdge)
                fromLength = convertTo100PercentMinusLength(fromLength);
            else if (toEdge == m_farEdge) {
                toLength = convertTo100PercentMinusLength(toLength);
                destinationEdge = fromEdge; // Now we have a calc(100% - l), it's relative to the left/top edge.
            }
        }

        (destination->*m_originSetter)(destinationEdge);
        (destination->*m_lengthSetter)(blendFunc(fromLength, toLength, context));
    }

#if !LOG_DISABLED
    void log(const FillLayer* destination, const FillLayer* from, const FillLayer* to, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << property() << " from " << value(from) << " to " << value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(destination));
    }
#endif

    void (FillLayer::*m_lengthSetter)(WebCore::Length);
    Edge (FillLayer::*m_originGetter)() const;
    void (FillLayer::*m_originSetter)(Edge);
    Edge m_farEdge;
};

template<typename T>
class FillLayerRefCountedWrapper : public FillLayerWrapperWithGetter<T*> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    FillLayerRefCountedWrapper(CSSPropertyID property, T* (FillLayer::*getter)() const, void (FillLayer::*setter)(RefPtr<T>&&))
        : FillLayerWrapperWithGetter<T*>(property, getter)
        , m_setter(setter)
    {
    }

private:
    void interpolate(FillLayer* destination, const FillLayer* from, const FillLayer* to, const Context& context) const final
    {
        (destination->*this->m_setter)(blendFunc(this->value(from), this->value(to), context));
    }

#if !LOG_DISABLED
    void log(const FillLayer* destination, const FillLayer* from, const FillLayer* to, double progress) const override
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << FillLayerWrapperWithGetter<T*>::property()
            << " from " << FillLayerWrapperWithGetter<T*>::value(from)
            << " to " << FillLayerWrapperWithGetter<T*>::value(to)
            << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << FillLayerWrapperWithGetter<T*>::value(destination));
    }
#endif

    void (FillLayer::*m_setter)(RefPtr<T>&&);
};

class FillLayerStyleImageWrapper final : public FillLayerRefCountedWrapper<StyleImage> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    FillLayerStyleImageWrapper(CSSPropertyID property, StyleImage* (FillLayer::*getter)() const, void (FillLayer::*setter)(RefPtr<StyleImage>&&))
        : FillLayerRefCountedWrapper(property, getter, setter)
    {
    }

private:
    bool equals(const FillLayer* a, const FillLayer* b) const final
    {
        if (a == b)
            return true;
        if (!a || !b)
            return false;
        return arePointingToEqualData(value(a), value(b));
    }

    bool canInterpolate(const FillLayer* from, const FillLayer* to) const final
    {
        if (property() == CSSPropertyMaskImage)
            return false;
        return value(from) && value(to);
    }

#if !LOG_DISABLED
    void log(const FillLayer* destination, const FillLayer* from, const FillLayer* to, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << property() << " from " << this->value(from) << " to " << this->value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(destination));
    }
#endif
};

template<typename T>
class DiscreteFillLayerWrapper final : public FillLayerWrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    DiscreteFillLayerWrapper(CSSPropertyID property, T (FillLayer::*getter)() const, void (FillLayer::*setter)(T))
        : FillLayerWrapperBase(property)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

private:
    bool equals(const FillLayer* a, const FillLayer* b) const final
    {
        return (a->*m_getter)() == (b->*m_getter)();
    }

    bool canInterpolate(const FillLayer*, const FillLayer*) const final
    {
        return false;
    }

    void interpolate(FillLayer* destination, const FillLayer* from, const FillLayer* to, const Context& context) const final
    {
        ASSERT(!context.progress || context.progress == 1.0);
        (destination->*m_setter)(((context.progress ? to : from)->*m_getter)());
    }

#if !LOG_DISABLED
    void log(const FillLayer* destination, const FillLayer* from, const FillLayer* to, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << property() << " from " << (from->*m_getter)() << " to " << (to->*m_getter)() << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << (destination->*m_getter)());
    }
#endif

    T (FillLayer::*m_getter)() const;
    void (FillLayer::*m_setter)(T);
};

class FillLayersWrapper final : public WrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    typedef const FillLayer& (RenderStyle::*LayersGetter)() const;
    typedef FillLayer& (RenderStyle::*LayersAccessor)();

    FillLayersWrapper(CSSPropertyID property, LayersGetter getter, LayersAccessor accessor)
        : WrapperBase(property)
        , m_layersGetter(getter)
        , m_layersAccessor(accessor)
    {
        switch (property) {
        case CSSPropertyBackgroundPositionX:
        case CSSPropertyWebkitMaskPositionX:
            m_fillLayerWrapper = makeUnique<FillLayerPositionWrapper>(property, &FillLayer::xPosition, &FillLayer::setXPosition, &FillLayer::backgroundXOrigin, &FillLayer::setBackgroundXOrigin, Edge::Right);
            break;
        case CSSPropertyBackgroundPositionY:
        case CSSPropertyWebkitMaskPositionY:
            m_fillLayerWrapper = makeUnique<FillLayerPositionWrapper>(property, &FillLayer::yPosition, &FillLayer::setYPosition, &FillLayer::backgroundYOrigin, &FillLayer::setBackgroundYOrigin, Edge::Bottom);
            break;
        case CSSPropertyBackgroundSize:
        case CSSPropertyWebkitBackgroundSize:
        case CSSPropertyMaskSize:
            m_fillLayerWrapper = makeUnique<FillLayerWrapper<LengthSize>>(property, &FillLayer::sizeLength, &FillLayer::setSizeLength);
            break;
        case CSSPropertyBackgroundImage:
        case CSSPropertyMaskImage:
            m_fillLayerWrapper = makeUnique<FillLayerStyleImageWrapper>(property, &FillLayer::image, &FillLayer::setImage);
            break;
        case CSSPropertyMaskClip:
            m_fillLayerWrapper = makeUnique<DiscreteFillLayerWrapper<FillBox>>(property, &FillLayer::clip, &FillLayer::setClip);
            break;
        case CSSPropertyMaskOrigin:
            m_fillLayerWrapper = makeUnique<DiscreteFillLayerWrapper<FillBox>>(property, &FillLayer::origin, &FillLayer::setOrigin);
            break;
        case CSSPropertyMaskComposite:
            m_fillLayerWrapper = makeUnique<DiscreteFillLayerWrapper<CompositeOperator>>(property, &FillLayer::composite, &FillLayer::setComposite);
            break;
        case CSSPropertyMaskMode:
            m_fillLayerWrapper = makeUnique<DiscreteFillLayerWrapper<MaskMode>>(property, &FillLayer::maskMode, &FillLayer::setMaskMode);
            break;
        default:
            break;
        }
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        if (&a == &b)
            return true;

        auto* fromLayer = &(a.*m_layersGetter)();
        auto* toLayer = &(b.*m_layersGetter)();

        while (fromLayer && toLayer) {
            if (!m_fillLayerWrapper->equals(fromLayer, toLayer))
                return false;

            fromLayer = fromLayer->next();
            toLayer = toLayer->next();
        }

        return true;
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        auto* fromLayer = &(from.*m_layersGetter)();
        auto* toLayer = &(to.*m_layersGetter)();

        while (fromLayer && toLayer) {
            if (fromLayer->sizeType() != toLayer->sizeType())
                return false;

            if (!m_fillLayerWrapper->canInterpolate(fromLayer, toLayer))
                return false;

            fromLayer = fromLayer->next();
            toLayer = toLayer->next();
        }

        return true;
    }

    void interpolate(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const Context& context) const final
    {
        auto* fromLayer = &(from.*m_layersGetter)();
        auto* toLayer = &(to.*m_layersGetter)();
        auto* dstLayer = &(destination.*m_layersAccessor)();

        if (context.isDiscrete) {
            ASSERT(!context.progress || context.progress == 1.0);
            auto* layer = context.progress ? toLayer : fromLayer;
            fromLayer = layer;
            toLayer = layer;
        }

        size_t layerCount = 0;
        Vector<FillLayer*> previousDstLayers;
        FillLayer* previousDstLayer = nullptr;
        while (fromLayer && toLayer) {
            if (dstLayer)
                previousDstLayers.append(dstLayer);
            else {
                ASSERT(!previousDstLayers.isEmpty());
                auto* layerToCopy = previousDstLayers[layerCount % previousDstLayers.size()];
                previousDstLayer->setNext(layerToCopy->copy());
                dstLayer = previousDstLayer->next();
            }

            dstLayer->setSizeType((context.progress ? toLayer : fromLayer)->sizeType());
            m_fillLayerWrapper->interpolate(dstLayer, fromLayer, toLayer, context);
            fromLayer = fromLayer->next();
            toLayer = toLayer->next();

            previousDstLayer = dstLayer;
            dstLayer = dstLayer->next();
            layerCount++;
        }
    }

#if !LOG_DISABLED
    void log(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        auto* fromLayer = &(from.*m_layersGetter)();
        auto* toLayer = &(to.*m_layersGetter)();
        auto* dstLayer = &(destination.*m_layersGetter)();

        while (fromLayer && toLayer && dstLayer) {
            m_fillLayerWrapper->log(dstLayer, fromLayer, toLayer, progress);
            fromLayer = fromLayer->next();
            toLayer = toLayer->next();
            dstLayer = dstLayer->next();
        }
    }
#endif

private:
    std::unique_ptr<FillLayerWrapperBase> m_fillLayerWrapper;
    LayersGetter m_layersGetter;
    LayersAccessor m_layersAccessor;
};

// MARK: - Shorthand Wrapper

class ShorthandWrapper final : public WrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
