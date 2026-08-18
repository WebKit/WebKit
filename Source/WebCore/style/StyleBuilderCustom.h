/*
 * Copyright (C) 2013-2014 Google Inc. All rights reserved.
 * Copyright (C) 2014-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AnchorPositionEvaluator.h"
#include "CSSCounterStyleRegistry.h"
#include "CSSCounterStyleRule.h"
#include "CSSPropertyParserConsumer+Font.h"
#include "CSSRegisteredCustomProperty.h"
#include "CSSStringValue.h"
#include "CSSValuePair.h"
#include "CSSValuePool.h"
#include "DocumentQuirks.h"
#include "DocumentView.h"
#include "ElementAncestorIteratorInlines.h"
#include "FontCascadeInlines.h"
#include "FontSelectionValueInlines.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLElement.h"
#include "LocalFrame.h"
#include "OpenTypeMathData.h"
#include "SVGElement.h"
#include "SVGElementTypeHelpers.h"
#include "SVGPathElement.h"
#include "Settings.h"
#include "StyleBuilderChecking.h"
#include "StyleBuilderStateInlines.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "StyleFontSizeFunctions.h"
#include "StyleKeyword+CSSValueConversion.h"
#include "StylePrimitiveNumericOrKeyword+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StyleResolveForFont.h"
#include "StyleResolver.h"
#include "StyleTextEdge+CSSValueConversion.h"
#include "StyleValueTypes+CSSValueConversion.h"
#include "TextSpacing.h"
#include "ViewTimeline.h"
#include <ranges>

namespace WebCore {
namespace Style {

template<typename T>
decltype(auto) forwardInheritedValue(T&& value)
{
    if constexpr (std::is_lvalue_reference_v<T>)
        return std::remove_cvref_t<T>(value);
    else
        return std::forward<T>(value);
}

class BuilderCustom {
public:
    static void applyInheritFontFamily(BuilderState&);
    static void applyInitialFontFamily(BuilderState&);
    static void applyValueFontFamily(BuilderState&, CSSValue&);

    static void applyInheritFontSize(BuilderState&);
    static void applyInitialFontSize(BuilderState&);
    static void applyValueFontSize(BuilderState&, CSSValue&);

    static void applyInheritLetterSpacing(BuilderState&);
    static void applyInitialLetterSpacing(BuilderState&);
    static void applyValueLetterSpacing(BuilderState&, CSSValue&);

#if ENABLE(TEXT_AUTOSIZING)
    static void applyInheritLineHeight(BuilderState&);
    static void applyInitialLineHeight(BuilderState&);
    static void applyValueLineHeight(BuilderState&, CSSValue&);
#endif

    static void applyInheritWordSpacing(BuilderState&);
    static void applyInitialWordSpacing(BuilderState&);
    static void applyValueWordSpacing(BuilderState&, CSSValue&);

    static void applyInheritZoom(BuilderState&);
    static void applyInitialZoom(BuilderState&);
    static void applyValueZoom(BuilderState&, CSSValue&);

    static void applyInitialColor(BuilderState&);
    static void applyValueColor(BuilderState&, CSSValue&);
    static void applyHighlightInitialColor(BuilderState&);
    static void applyHighlightInheritColor(BuilderState&);
    static void applyHighlightValueColor(BuilderState&, CSSValue&);

    // Custom handling of value setting only.
    static void applyValueWebkitLocale(BuilderState&, CSSValue&);
    static void applyValueTextOrientation(BuilderState&, CSSValue&);
#if ENABLE(TEXT_AUTOSIZING)
    static void applyValueWebkitTextSizeAdjust(BuilderState&, CSSValue&);
#endif
    static void applyValueWebkitTextZoom(BuilderState&, CSSValue&);
    static void applyValueWritingMode(BuilderState&, CSSValue&);
    static void applyValueFontSizeAdjust(BuilderState&, CSSValue&);

private:
    static void resetUsedZoom(BuilderState&);

    static float largerFontSize(float size);
    static float smallerFontSize(float size);
    static float determineRubyTextSizeMultiplier(BuilderState&);
    static float determineMathDepthScale(BuilderState&);
};

// MARK: - CoordinatedValueList Utilities

template<auto propertyID, auto listMutableGetter, typename ListType>
void applyInitialCoordinatedValueListProperty(BuilderState& builderState)
{
    using PropertyAccessor = CoordinatedValueListPropertyAccessor<propertyID>;

    auto& list = (builderState.style().*listMutableGetter)();
    ASSERT(list.computedLength() > 0);

    PropertyAccessor { list[0] }.set(PropertyAccessor::initial());

    for (size_t i = 0; i < list.computedLength(); ++i)
        PropertyAccessor { list[i] }.clear();
}

template<auto propertyID, auto listMutableGetter, auto listGetter, typename ListType>
void applyInheritCoordinatedValueListProperty(BuilderState& builderState)
{
    using PropertyAccessor = CoordinatedValueListPropertyAccessor<propertyID>;
    using ConstPropertyAccessor = CoordinatedValueListPropertyConstAccessor<propertyID>;

    auto& list = (builderState.style().*listMutableGetter)();
    auto& parentList = (builderState.parentStyle().*listGetter)();

    size_t i = 0;
    size_t parentSize = parentList.isInitial() ? 0 : parentList.computedLength();

    for (; i < parentSize && ConstPropertyAccessor { parentList[i] }.isSet(); ++i) {
        if (list.computedLength() <= i)
            list.append(typename ListType::value_type { });
        PropertyAccessor { list[i] }.set(forwardInheritedValue(ConstPropertyAccessor { parentList[i] }.get()));
    }

    for (; i < list.computedLength(); ++i)
        PropertyAccessor { list[i] }.clear();
}

template<auto propertyID, auto listMutableGetter, typename ItemType, typename ListType>
void applyValueCoordinatedValueListProperty(BuilderState& builderState, CSSValue& value)
{
    using PropertyAccessor = CoordinatedValueListPropertyAccessor<propertyID>;

    auto& list = (builderState.style().*listMutableGetter)();

    auto set = [&](auto i, auto& item) {
        if (isValueID(item, CSSValueInitial))
            PropertyAccessor { list[i] }.set(PropertyAccessor::initial());
        else
            PropertyAccessor { list[i] }.set(toStyleFromCSSValue<ItemType>(builderState, item));
    };

    size_t i = 0;
    if (RefPtr valueList = dynamicDowncast<CSSValueList>(value)) {
        for (Ref item : *valueList) {
            if (i >= list.computedLength())
                list.append(typename ListType::value_type { });

            set(i, item.get());
            ++i;
        }
    } else {
        ASSERT(list.computedLength() > 0);
        set(0, value);
        i = 1;
    }

    for (; i < list.computedLength(); ++i)
        PropertyAccessor { list[i] }.clear();
}

// MARK: - Custom conversions

inline void BuilderCustom::resetUsedZoom(BuilderState& builderState)
{
    // Reset the zoom in effect. This allows the setZoom method to accurately compute a new zoom in effect.
    builderState.setUsedZoom(builderState.parentStyle().usedZoom());
}

inline void BuilderCustom::applyInitialZoom(BuilderState& builderState)
{
    resetUsedZoom(builderState);
    builderState.setZoom(ComputedStyle::initialZoom());
}

inline void BuilderCustom::applyInheritZoom(BuilderState& builderState)
{
    resetUsedZoom(builderState);
    builderState.setZoom(forwardInheritedValue(builderState.parentStyle().zoom()));
}

inline void BuilderCustom::applyValueZoom(BuilderState& builderState, CSSValue& value)
{
    if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
        case CSSValueNormal:
            resetUsedZoom(builderState);
            builderState.setZoom(Style::ComputedStyle::initialZoom());
            return;

        default:
            builderState.setCurrentPropertyInvalidAtComputedValueTime();
            return;
        }
    }

    resetUsedZoom(builderState);
    auto zoom = toStyleFromCSSValue<Zoom>(builderState, value);
    builderState.setZoom(isZero(zoom) ? Zoom { 1.0f } : zoom);
}

void maybeUpdateFontForLetterSpacingOrWordSpacing(BuilderState& builderState, CSSValue& value)
{
    // This is unfortunate. It's related to https://github.com/w3c/csswg-drafts/issues/5498.
    //
    // From StyleBuilder's point of view, there's a dependency cycle:
    // letter-spacing accepts an arbitrary <length>, which must be resolved against a font, which must
    // be selected after all the properties that affect font selection are processed, but letter-spacing
    // itself affects font selection because it can disable font features. StyleBuilder has some (valid)
    // ASSERT()s which would fire because of this cycle.
    //
    // There isn't *actually* a dependency cycle, though, as none of the font-relative units are
    // actually sensitive to font features (luckily). The problem is that our StyleBuilder is only
    // smart enough to consider fonts as one indivisible thing, rather than having the deeper
    // understanding that different parts of fonts may or may not depend on each other.
    //
    // So, we update the font early here, so that if there is a font-relative unit inside the CSSValue,
    // its font is updated and ready to go. In the worst case there might be a second call to
    // updateFont() later, but that isn't bad for perf because 1. It only happens twice if there is
    // actually a font-relative unit passed to letter-spacing or word-spacing, and 2. updateFont() internally
    // has logic to only do work if the font is actually dirty.

    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->isFontRelativeLength() || primitiveValue->isCalculated())
            builderState.updateFont();
    }
}

inline void BuilderCustom::applyInheritWordSpacing(BuilderState& builderState)
{
    builderState.style().setWordSpacing(forwardInheritedValue(builderState.parentStyle().computedWordSpacing()));
    builderState.setFontDirty();
}

inline void BuilderCustom::applyInitialWordSpacing(BuilderState& builderState)
{
    builderState.style().setWordSpacing(ComputedStyle::initialWordSpacing());
    builderState.setFontDirty();
}

void BuilderCustom::applyValueWordSpacing(BuilderState& builderState, CSSValue& value)
{
    maybeUpdateFontForLetterSpacingOrWordSpacing(builderState, value);
    builderState.style().setWordSpacing(toStyleFromCSSValue<WordSpacing>(builderState, value));
    builderState.setFontDirty();
}

inline void BuilderCustom::applyInheritLetterSpacing(BuilderState& builderState)
{
    builderState.style().setLetterSpacing(forwardInheritedValue(builderState.parentStyle().computedLetterSpacing()));
    builderState.setFontDirty();
}

inline void BuilderCustom::applyInitialLetterSpacing(BuilderState& builderState)
{
    builderState.style().setLetterSpacing(ComputedStyle::initialLetterSpacing());
    builderState.setFontDirty();
}

inline void BuilderCustom::applyValueLetterSpacing(BuilderState& builderState, CSSValue& value)
{
    maybeUpdateFontForLetterSpacingOrWordSpacing(builderState, value);
    builderState.style().setLetterSpacing(toStyleFromCSSValue<LetterSpacing>(builderState, value));
    builderState.setFontDirty();
}

#if ENABLE(TEXT_AUTOSIZING)

inline void BuilderCustom::applyInheritLineHeight(BuilderState& builderState)
{
    builderState.style().setLineHeight(forwardInheritedValue(builderState.parentStyle().lineHeight()));
    builderState.style().setSpecifiedLineHeight(forwardInheritedValue(builderState.parentStyle().specifiedLineHeight()));
}

inline void BuilderCustom::applyInitialLineHeight(BuilderState& builderState)
{
    builderState.style().setLineHeight(ComputedStyle::initialLineHeight());
    builderState.style().setSpecifiedLineHeight(ComputedStyle::initialSpecifiedLineHeight());
}

static inline float computeBaseSpecifiedFontSize(const Document& document, const ComputedStyle& style)
{
    float result = style.specifiedFontSize();
    auto* frame = document.frame();
    if (frame && style.textZoom() != TextZoom::Reset)
        result *= frame->textZoomFactor();
    result *= style.usedZoom();
    return result;
}

static inline float computeLineHeightMultiplierDueToFontSize(const Document& document, const ComputedStyle& style, const CSSValue& value)
{
    bool percentageAutosizingEnabled = document.settings().textAutosizingEnabled() && style.textSizeAdjust().isPercentage();

    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value); primitiveValue && primitiveValue->isLength()) {
        auto minimumFontSize = document.settings().minimumFontSize();
        if (minimumFontSize > 0) {
            auto specifiedFontSize = computeBaseSpecifiedFontSize(document, style);
            // Small font sizes cause a preposterously large (near infinity) line-height. Add a fuzz-factor of 1px which opts out of
            // boosted line-height.
            if (specifiedFontSize < minimumFontSize && specifiedFontSize >= 1) {
                // FIXME: There are two settings which are relevant here: minimum font size, and minimum logical font size (as
                // well as things like the zoom property, text zoom on the page, and text autosizing). The minimum logical font
                // size is nonzero by default, and already incorporated into the computed font size, so if we just use the ratio
                // of the computed : specified font size, it will be > 1 in the cases where the minimum logical font size kicks
                // in. In general, this is the right thing to do, however, this kind of blanket change is too risky to perform
                // right now. https://bugs.webkit.org/show_bug.cgi?id=174570 tracks turning this on. For now, we can just pretend
                // that the minimum font size is the only thing affecting the computed font size.

                // This calculation matches the line-height computed size calculation in
                // TextAutoSizing::Value::adjustTextNodeSizes().
                auto scaleChange = minimumFontSize / specifiedFontSize;
                return scaleChange;
            }
        }
    }

    if (percentageAutosizingEnabled && !document.settings().textAutosizingUsesIdempotentMode())
        return style.textSizeAdjust().multiplier();
    return 1;
}

inline void BuilderCustom::applyValueLineHeight(BuilderState& builderState, CSSValue& value)
{
    if (CSSPropertyParserHelpers::isSystemFontShorthand(valueID(value))) {
        applyInitialLineHeight(builderState);
        return;
    }

    auto lineHeight = toStyleFromCSSValue<LineHeight>(builderState, value, 1.0f);

    auto computedLineHeight = [&] -> LineHeight {
        if (lineHeight.isNormal())
            return lineHeight;

        auto multiplier = computeLineHeightMultiplierDueToFontSize(builderState.document(), builderState.style(), value);
        if (multiplier == 1)
            return lineHeight;

        return toStyleFromCSSValue<LineHeight>(builderState, value, multiplier);
    }();

    builderState.style().setLineHeight(WTF::move(computedLineHeight));
    builderState.style().setSpecifiedLineHeight(WTF::move(lineHeight));
}

#endif

inline void BuilderCustom::applyValueWebkitLocale(BuilderState& builderState, CSSValue& value)
{
    builderState.setFontDescriptionSpecifiedLocale(toStyleFromCSSValue<WebkitLocale>(builderState, value));
}

inline void BuilderCustom::applyValueWritingMode(BuilderState& builderState, CSSValue& value)
{
    builderState.setWritingMode(fromCSSValue<StyleWritingMode>(value));
    builderState.style().setHasExplicitlySetWritingMode(true);
}

inline void BuilderCustom::applyValueTextOrientation(BuilderState& builderState, CSSValue& value)
{
    builderState.setTextOrientation(fromCSSValue<TextOrientation>(value));
}

#if ENABLE(TEXT_AUTOSIZING)
inline void BuilderCustom::applyValueWebkitTextSizeAdjust(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setTextSizeAdjust(toStyleFromCSSValue<TextSizeAdjust>(builderState, value));
    builderState.setFontDirty();
}
#endif

inline void BuilderCustom::applyValueWebkitTextZoom(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setTextZoom(toStyleFromCSSValue<TextZoom>(builderState, value));
    builderState.setFontDirty();
}

inline void BuilderCustom::applyInitialFontFamily(BuilderState& builderState)
{
    auto& fontDescription = builderState.fontDescription();
    auto initialDesc = FontCascadeDescription();

    // We need to adjust the size to account for the generic family change from monospace to non-monospace.
    if (fontDescription.useFixedDefaultSize()) {
        if (CSSValueID sizeIdentifier = fontDescription.keywordSizeAsIdentifier())
            builderState.setFontDescriptionFontSize(fontSizeForKeyword(sizeIdentifier, false, builderState.document()));
    }

    if (!initialDesc.firstFamily().name.isEmpty())
        builderState.setFontDescriptionFamilies(FontFamilies { initialDesc.families(), fontDescription.hasAuthorSpecifiedNonGenericPrimaryFont() });
}

inline void BuilderCustom::applyInheritFontFamily(BuilderState& builderState)
{
    builderState.setFontDescriptionFamilies(forwardInheritedValue(builderState.parentStyle().fontFamily()));
}

inline void BuilderCustom::applyValueFontFamily(BuilderState& builderState, CSSValue& value)
{
    auto& fontDescription = builderState.fontDescription();

    // Before mapping in a new font-family property, we should reset the generic family.
    bool oldFamilyUsedFixedDefaultSize = fontDescription.useFixedDefaultSize();

    builderState.setFontDescriptionFamilies(toStyleFromCSSValue<FontFamilies>(builderState, value));

    if (fontDescription.useFixedDefaultSize() != oldFamilyUsedFixedDefaultSize) {
        if (CSSValueID sizeIdentifier = fontDescription.keywordSizeAsIdentifier())
            builderState.setFontDescriptionFontSize(fontSizeForKeyword(sizeIdentifier, !oldFamilyUsedFixedDefaultSize, builderState.document()));
    }
}

inline void BuilderCustom::applyInitialFontSize(BuilderState& builderState)
{
    auto fontDescription = builderState.fontDescription();
    float size = fontSizeForKeyword(CSSValueMedium, fontDescription.useFixedDefaultSize(), builderState.document());

    if (size < 0)
        return;

    fontDescription.setKeywordSizeFromIdentifier(CSSValueMedium);
    builderState.setFontSize(fontDescription, size);
    builderState.setFontDescription(WTF::move(fontDescription));
}

inline void BuilderCustom::applyInheritFontSize(BuilderState& builderState)
{
    const auto& parentFontDescription = builderState.parentStyle().fontDescription();
    float size = parentFontDescription.specifiedSize();

    if (size < 0)
        return;

    builderState.setFontDescriptionKeywordSize(parentFontDescription.keywordSize());
    builderState.setFontDescriptionFontSize(size);
}

// When the CSS keyword "larger" is used, this function will attempt to match within the keyword
// table, and failing that, will simply multiply by 1.2.
inline float BuilderCustom::largerFontSize(float size)
{
    // FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large) and scale up to
    // the next size level.
    return size * 1.2f;
}

// Like the previous function, but for the keyword "smaller".
inline float BuilderCustom::smallerFontSize(float size)
{
    // FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large) and scale down to
    // the next size level.
    return size / 1.2f;
}

inline float BuilderCustom::determineRubyTextSizeMultiplier(BuilderState& builderState)
{
    switch (builderState.style().rubyPosition()) {
    case RubyPosition::Over:
    case RubyPosition::Under:
        return 0.5f;

    case RubyPosition::InterCharacter:
        // If the writing mode of the enclosing ruby container is vertical, 'inter-character' value has the same effect as over.
        return !builderState.parentStyle().writingMode().isVerticalTypographic() ? 0.3f : 0.5f;

    case RubyPosition::LegacyInterCharacter:
        // FIXME: This hack is to ensure tone marks are the same size as
        // the bopomofo. This code will go away if we make a special renderer
        // for the tone marks eventually.
        if (auto* element = builderState.element()) {
            for (auto& ancestor : ancestorsOfType<HTMLElement>(*element)) {
                if (ancestor.hasTagName(HTMLNames::rtTag))
                    return 1.0f;
            }
        }
        return 0.25f;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

// https://w3c.github.io/mathml-core/#the-math-script-level-property
inline float BuilderCustom::determineMathDepthScale(BuilderState& builderState)
{
    // Step 1.
    auto inherited = builderState.parentStyle().mathDepth();
    auto computed = builderState.style().mathDepth();
    float scale = 1.0f;
    float scaleDown = 0.71f;

    // Step 2.
    if (inherited == computed)
        return scale;
    bool invertScaleFactor = false;
    if (computed < inherited) {
        std::swap(computed, inherited);
        invertScaleFactor = true;
    }

    // Step 3.
    int exponent = computed.value - inherited.value;

    // Step 4.
#if ENABLE(MATHML)
    Ref primaryFont = builderState.style().fontCascade().primaryFont();
    if (RefPtr mathData = primaryFont->mathData()) {
        float scriptPercentScaleDown = mathData->getMathConstant(primaryFont, OpenTypeMathData::MathConstant::ScriptPercentScaleDown);
        if (!scriptPercentScaleDown)
            scriptPercentScaleDown = 0.71;

        float scriptScriptPercentScaleDown = mathData->getMathConstant(primaryFont, OpenTypeMathData::MathConstant::ScriptScriptPercentScaleDown);
        if (!scriptScriptPercentScaleDown)
            scriptScriptPercentScaleDown = 0.5041;

        if (inherited <= 0 && computed >= 2) {
            scale *= scriptScriptPercentScaleDown;
            exponent -= 2;
        } else if (inherited == 1) {
            scale *= scriptScriptPercentScaleDown / scriptPercentScaleDown;
            exponent--;
        } else if (computed == 1) {
            scale *= scriptPercentScaleDown;
            exponent--;
        }
    }
#endif

    // Step 5.
    scale *= std::pow(scaleDown, exponent);

    // Step 6.
    return invertScaleFactor ? 1.f / scale : scale;
}

inline void BuilderCustom::applyValueFontSize(BuilderState& builderState, CSSValue& value)
{
    auto& fontDescription = builderState.fontDescription();
    builderState.setFontDescriptionKeywordSizeFromIdentifier(CSSValueInvalid);

    float parentSize = builderState.parentStyle().fontDescription().specifiedSize();
    bool parentIsAbsoluteSize = builderState.parentStyle().fontDescription().isAbsoluteSize();

    float size = 0;
    if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        auto ident = keywordValue->valueID();
        builderState.setFontDescriptionIsAbsoluteSize((parentIsAbsoluteSize && (ident == CSSValueLarger || ident == CSSValueSmaller || ident == CSSValueWebkitRubyText || ident == CSSValueMath)) || CSSPropertyParserHelpers::isSystemFontShorthand(ident));

        if (CSSPropertyParserHelpers::isSystemFontShorthand(ident))
            size = SystemFontDatabase::singleton().systemFontShorthandSize(CSSPropertyParserHelpers::lowerFontShorthand(ident));

        switch (ident) {
        case CSSValueXxSmall:
        case CSSValueXSmall:
        case CSSValueSmall:
        case CSSValueMedium:
        case CSSValueLarge:
        case CSSValueXLarge:
        case CSSValueXxLarge:
        case CSSValueXxxLarge:
            size = fontSizeForKeyword(ident, fontDescription.useFixedDefaultSize(), builderState.document());
            builderState.setFontDescriptionKeywordSizeFromIdentifier(ident);
            break;
        case CSSValueLarger:
            size = largerFontSize(parentSize);
            break;
        case CSSValueSmaller:
            size = smallerFontSize(parentSize);
            break;
        case CSSValueMath:
            size = determineMathDepthScale(builderState) * parentSize;
            break;
        case CSSValueWebkitRubyText:
            size = determineRubyTextSizeMultiplier(builderState) * parentSize;
            break;
        default:
            break;
        }
    } else if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        // FIXME: Checking `primitiveValue->isPercentageOrParentFontRelativeLength()` is not sufficient to determine if any parent relative length units have been used, as arbitrary calc() expressions may contain them as well. For example, `font-size: calc(1px + 1em)`.
        builderState.setFontDescriptionIsAbsoluteSize(parentIsAbsoluteSize || !primitiveValue->isPercentageOrParentFontRelativeLength());

        using StyleType = LengthPercentage<CSS::Nonnegative>;

        auto handleLength = [](const auto& length) -> float { return length.resolveZoom(ZoomFactor::none()); };
        auto handlePercentage = [&](const auto& percentage) -> float { return percentage.value * parentSize / 100.0f; };
        auto handleCalc = [&](const auto& calc) -> float { return calc.evaluate(parentSize, ZoomFactor::none()); };

        size =  WTF::switchOn(*primitiveValue,
            [&](const CSSPrimitiveValue::Calc& calc) -> float {
                using CSSRaw = typename StyleType::CSS::Raw;

                auto resolved = toStyle(CSS::UnevaluatedCalc<CSSRaw> { calc }, builderState);
                return WTF::switchOn(resolved,
                    [&](const typename StyleType::Dimension& length) {
                        return handleLength(length);
                    },
                    [&](const typename StyleType::Percentage& percentage) {
                        return handlePercentage(percentage);
                    },
                    [&](const typename StyleType::Calc& calc) {
                        return handleCalc(calc);
                    }
                );
            },
            [&](const CSSPrimitiveValue::Raw& raw) -> float {
                using CSSDimensionRaw = typename StyleType::Dimension::CSS::Raw;
                using CSSPercentageRaw = typename StyleType::Percentage::CSS::Raw;

                if (auto unit = CSSDimensionRaw::UnitTraits::validate(raw.unit))
                    return handleLength(toStyle(CSSDimensionRaw(*unit, raw.value), builderState));
                if (auto unit = CSSPercentageRaw::UnitTraits::validate(raw.unit))
                    return handlePercentage(toStyle(CSSPercentageRaw(*unit, raw.value), builderState));

                builderState.setCurrentPropertyInvalidAtComputedValueTime();
                return 0;
            }
        );
    } else {
        builderState.setCurrentPropertyInvalidAtComputedValueTime();
        return;
    }

    if (size < 0)
        return;

    builderState.setFontDescriptionFontSize(std::min(maximumAllowedFontSize, size));
}

// CanvasText is the initial color according to spec.
// https://www.w3.org/TR/css-color-4/#the-color-property
inline void BuilderCustom::applyInitialColor(BuilderState& builderState)
{
    const CSS::Color initialColor { CSS::KeywordColor { CSSValueCanvastext } };

    if (builderState.applyPropertyToRegularStyle()) {
        auto styleColor = toStyle(initialColor, builderState, ForVisitedLink::No);
        builderState.style().setColor(styleColor.resolveColor(builderState.parentStyle().color()));
    }
    if (builderState.applyPropertyToVisitedLinkStyle()) {
        auto styleColor = toStyle(initialColor, builderState, ForVisitedLink::Yes);
        builderState.style().setVisitedLinkColor(styleColor.resolveColor(builderState.parentStyle().visitedLinkColor()));
    }

    builderState.style().setDisallowsFastPathInheritance();
    builderState.style().setHasExplicitlySetColor(builderState.isAuthorOrigin());
}

// For the color property, "currentcolor" is actually the inherited computed color.
inline void BuilderCustom::applyValueColor(BuilderState& builderState, CSSValue& value)
{
    if (builderState.applyPropertyToRegularStyle()) {
        auto color = toStyleFromCSSValue<Color>(builderState, value, ForVisitedLink::No);
        builderState.style().setColor(color.resolveColor(builderState.parentStyle().color()));
    }
    if (builderState.applyPropertyToVisitedLinkStyle()) {
        auto color = toStyleFromCSSValue<Color>(builderState, value, ForVisitedLink::Yes);
        builderState.style().setVisitedLinkColor(color.resolveColor(builderState.parentStyle().visitedLinkColor()));
    }

    builderState.style().setDisallowsFastPathInheritance();
    builderState.style().setHasExplicitlySetColor(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyHighlightInitialColor(BuilderState& builderState)
{
    applyInitialColor(builderState);
    builderState.style().setColorIsCurrentColorForHighlight(false);
}

// currentcolor in a highlight pseudo-element is the originating element's color, so the chain
// inherits the keyword rather than the color it resolved to. At the start of the chain the inherited
// value is currentColor. https://drafts.csswg.org/css-pseudo-4/#highlight-cascade
// FIXME: A value that only references currentcolor, like color-mix(in oklab, teal, currentcolor),
// still propagates as the color it resolved to. Resolving those per element needs the unresolved
// Style::Color, which the color property doesn't store, and no engine does it today.
inline void BuilderCustom::applyHighlightInheritColor(BuilderState& builderState)
{
    CheckedPtr parentHighlightStyle = builderState.parentHighlightStyle();
    auto isCurrentColor = !parentHighlightStyle || parentHighlightStyle->colorIsCurrentColorForHighlight();
    auto& sourceStyle = isCurrentColor ? builderState.parentStyle() : *parentHighlightStyle;

    if (builderState.applyPropertyToRegularStyle()) {
        builderState.style().setColor(forwardInheritedValue(sourceStyle.color()));
        // FIXME: visitedLinkColor needs its own bit for this.
        builderState.style().setColorIsCurrentColorForHighlight(isCurrentColor);
    }
    if (builderState.applyPropertyToVisitedLinkStyle())
        builderState.style().setVisitedLinkColor(forwardInheritedValue(sourceStyle.color()));

    builderState.style().setDisallowsFastPathInheritance();
    // The seeding pass has no declaration to take the origin from, so it comes from the source.
    // FIXME: When the source is the originating element, its own color counts as one the highlight set.
    builderState.style().setHasExplicitlySetColor(builderState.isAuthorOrigin() || sourceStyle.hasExplicitlySetColor());
}

inline void BuilderCustom::applyHighlightValueColor(BuilderState& builderState, CSSValue& value)
{
    applyValueColor(builderState, value);

    if (builderState.applyPropertyToRegularStyle())
        builderState.style().setColorIsCurrentColorForHighlight(valueID(value) == CSSValueCurrentcolor);
}

} // namespace Style
} // namespace WebCore
