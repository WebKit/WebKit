/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleResolveForFont.h"

#include "CSSCalcSymbolTable.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontSelector.h"
#include "CSSFontStyleWithAngleValue.h"
#include "CSSFontVariationValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyParserConsumer+Font.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "FontCascade.h"
#include "FontCascadeDescription.h"
#include "FontSelectionValueInlines.h"
#include "LengthFunctions.h"
#include "RenderStyle.h"
#include "ScriptExecutionContext.h"
#include "Settings.h"
#include "StyleBuilderConverter.h"
#include "StyleFontSizeFunctions.h"
#include "StyleLengthResolution.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "WebKitFontFamilyNames.h"

namespace WebCore {
namespace Style {

using namespace WebKitFontFamilyNames;

// MARK: - 'font-weight'

FontSelectionValue fontWeightFromCSSValueDeprecated(const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    if (primitiveValue.isNumber())
        return FontSelectionValue(clampTo<float>(primitiveValue.resolveAsNumberDeprecated(), 1, 1000));

    ASSERT(primitiveValue.isValueID());
    switch (primitiveValue.valueID()) {
    case CSSValueNormal:
        return normalWeightValue();
    case CSSValueBold:
    case CSSValueBolder:
        return boldWeightValue();
    case CSSValueLighter:
        return lightWeightValue();
    default:
        ASSERT_NOT_REACHED();
        return normalWeightValue();
    }
}

FontSelectionValue fontWeightFromCSSValue(BuilderState& builderState, const CSSValue& value)
{
    RefPtr primitiveValue = BuilderConverter::requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };

    auto& conversionData = builderState.cssToLengthConversionData();

    if (primitiveValue->isNumber())
        return FontSelectionValue(clampTo<float>(primitiveValue->resolveAsNumber(conversionData), 1, 1000));

    ASSERT(primitiveValue->isValueID());
    auto valueID = primitiveValue->valueID();

    if (CSSPropertyParserHelpers::isSystemFontShorthand(valueID))
        return SystemFontDatabase::singleton().systemFontShorthandWeight(CSSPropertyParserHelpers::lowerFontShorthand(valueID));

    switch (valueID) {
    case CSSValueNormal:
        return normalWeightValue();
    case CSSValueBold:
        return boldWeightValue();
    case CSSValueBolder:
        return FontCascadeDescription::bolderWeight(conversionData.parentStyle()->fontDescription().weight());
    case CSSValueLighter:
        return FontCascadeDescription::lighterWeight(conversionData.parentStyle()->fontDescription().weight());
    default:
        ASSERT_NOT_REACHED();
        return normalWeightValue();
    }
}

static FontSelectionValue fontWeightFromUnresolvedFontWeight(const CSSPropertyParserHelpers::UnresolvedFontWeight& unresolvedWeight, const FontCascadeDescription& fontDescription)
{
    return WTF::switchOn(unresolvedWeight,
        [&](CSSValueID ident) {
            switch (ident) {
            case CSSValueNormal:
                return normalWeightValue();
            case CSSValueBold:
                return boldWeightValue();
            case CSSValueBolder:
                return FontCascadeDescription::bolderWeight(fontDescription.weight());
            case CSSValueLighter:
                return FontCascadeDescription::lighterWeight(fontDescription.weight());
            default:
                ASSERT_NOT_REACHED();
                return normalWeightValue();
            }
        },
        [&](const CSSPropertyParserHelpers::UnresolvedFontWeightNumber& weight) {
            // FIXME: Figure out correct behavior when conversion data is required.
            if (requiresConversionData(weight))
                return normalWeightValue();
            return FontSelectionValue::clampFloat(Style::toStyleNoConversionDataRequired(weight).value);
        }
    );
}

// MARK: - 'font-stretch'

FontSelectionValue fontStretchFromCSSValueDeprecated(const CSSValue& value)
{
    const auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    if (primitiveValue.isPercentage())
        return FontSelectionValue::clampFloat(primitiveValue.resolveAsPercentageDeprecated<float>());

    ASSERT(primitiveValue.isValueID());
    if (auto value = fontWidthValue(primitiveValue.valueID()))
        return value.value();

    ASSERT(CSSPropertyParserHelpers::isSystemFontShorthand(primitiveValue.valueID()));
    return normalWidthValue();
}

FontSelectionValue fontStretchFromCSSValue(BuilderState& builderState, const CSSValue& value)
{
    RefPtr primitiveValue = BuilderConverter::requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };

    if (primitiveValue->isPercentage())
        return FontSelectionValue::clampFloat(primitiveValue->resolveAsPercentage<float>(builderState.cssToLengthConversionData()));

    ASSERT(primitiveValue->isValueID());
    if (auto value = fontWidthValue(primitiveValue->valueID()))
        return value.value();

    ASSERT(CSSPropertyParserHelpers::isSystemFontShorthand(primitiveValue->valueID()));
    return normalWidthValue();
}

// MARK: - 'font-style'

FontSelectionValue fontStyleAngleFromCSSValueDeprecated(const CSSValue& value)
{
    return normalizedFontItalicValue(downcast<CSSPrimitiveValue>(value).resolveAsAngleDeprecated<float>());
}

FontSelectionValue fontStyleAngleFromCSSValue(BuilderState& builderState, const CSSValue& value)
{
    RefPtr primitiveValue = BuilderConverter::requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };

    return normalizedFontItalicValue(primitiveValue->resolveAsAngle<float>(builderState.cssToLengthConversionData()));
}

std::optional<FontSelectionValue> fontStyleAngleFromCSSFontStyleWithAngleValueDeprecated(const CSSFontStyleWithAngleValue& value)
{
    if (requiresConversionData(value.obliqueAngle()))
        return { };
    return FontSelectionValue { narrowPrecisionToFloat(Style::toStyle(value.obliqueAngle(), NoConversionDataRequiredToken { }).value) };
}

std::optional<FontSelectionValue> fontStyleAngleFromCSSFontStyleWithAngleValue(BuilderState& builderState, const CSSFontStyleWithAngleValue& value)
{
    return FontSelectionValue { narrowPrecisionToFloat(Style::toStyle(value.obliqueAngle(), builderState.cssToLengthConversionData()).value) };
}

std::optional<FontSelectionValue> fontStyleFromCSSValueDeprecated(const CSSValue& value)
{
    if (RefPtr fontStyleValue = dynamicDowncast<CSSFontStyleWithAngleValue>(value))
        return fontStyleAngleFromCSSFontStyleWithAngleValueDeprecated(*fontStyleValue);

    auto valueID = value.valueID();
    if (valueID == CSSValueNormal)
        return std::nullopt;

    ASSERT(valueID == CSSValueItalic || valueID == CSSValueOblique);
    return italicValue();
}

std::optional<FontSelectionValue> fontStyleFromCSSValue(BuilderState& builderState, const CSSValue& value)
{
    if (RefPtr fontStyleValue = dynamicDowncast<CSSFontStyleWithAngleValue>(value))
        return fontStyleAngleFromCSSFontStyleWithAngleValue(builderState, *fontStyleValue);

    auto valueID = value.valueID();
    if (valueID == CSSValueNormal)
        return std::nullopt;

    ASSERT(valueID == CSSValueItalic || valueID == CSSValueOblique);
    return italicValue();
}

struct ResolvedFontStyle {
    std::optional<FontSelectionValue> italic;
    FontStyleAxis axis;
};

static ResolvedFontStyle fontStyleFromUnresolvedFontStyle(const CSSPropertyParserHelpers::UnresolvedFontStyle& unresolvedStyle)
{
    // Font style applied in the same way as BuilderConverter::convertFontStyleFromValue
    return WTF::switchOn(unresolvedStyle,
        [](CSSValueID ident) -> ResolvedFontStyle {
            switch (ident) {
            case CSSValueNormal:
                return {
                    .italic = std::nullopt,
                    .axis = FontStyleAxis::slnt
                };

            case CSSValueItalic:
                return {
                    .italic = italicValue(),
                    .axis = FontStyleAxis::ital
                };

            case CSSValueOblique:
                return {
                    .italic = FontSelectionValue(0.0f),
                    .axis = FontStyleAxis::slnt
                };

            default:
                break;
            }

            ASSERT_NOT_REACHED();
            return { .italic = std::nullopt, .axis = FontStyleAxis::slnt };
        },
        [](const CSSPropertyParserHelpers::UnresolvedFontStyleObliqueAngle& angle) -> ResolvedFontStyle {
            // FIXME: Figure out correct behavior when conversion data is required.
            if (requiresConversionData(angle))
                return { .italic = std::nullopt, .axis = FontStyleAxis::slnt };

            return {
                .italic = FontSelectionValue::clampFloat(Style::toStyleNoConversionDataRequired(angle).value),
                .axis = FontStyleAxis::slnt
            };
        }
    );
}

// MARK: - 'font-size'

struct ResolvedFontSize {
    float size;
    CSSValueID keyword;
};

static ResolvedFontSize fontSizeFromUnresolvedFontSize(const CSSPropertyParserHelpers::UnresolvedFontSize& unresolvedSize, float parentSize, const FontCascadeDescription& fontDescription, Ref<ScriptExecutionContext> context)
{
    return WTF::switchOn(unresolvedSize,
        [&](CSSValueID ident) -> ResolvedFontSize {
            switch (ident) {
            case CSSValueXxSmall:
            case CSSValueXSmall:
            case CSSValueSmall:
            case CSSValueMedium:
            case CSSValueLarge:
            case CSSValueXLarge:
            case CSSValueXxLarge:
            case CSSValueXxxLarge:
                return {
                    .size = Style::fontSizeForKeyword(ident, fontDescription.useFixedDefaultSize(), context->settingsValues()),
                    .keyword = ident
                };

            case CSSValueLarger:
                return {
                    .size = parentSize * 1.02f,
                    .keyword = CSSValueInvalid
                };

            case CSSValueSmaller:
                return {
                    .size = parentSize / 1.02f,
                    .keyword = CSSValueInvalid
                };

            default:
                break;
            }

            ASSERT_NOT_REACHED();
            return { .size = 0.0f, .keyword = CSSValueInvalid };
        },
        [&](const CSS::LengthPercentage<CSS::Nonnegative>& lengthPercentage) -> ResolvedFontSize {
            return WTF::switchOn(lengthPercentage,
                [&](const CSS::LengthPercentage<CSS::Nonnegative>::Raw& lengthPercentage) -> ResolvedFontSize {
                    return CSS::switchOnUnitType(lengthPercentage.unit,
                        [&](CSS::PercentageUnit) -> ResolvedFontSize {
                            return {
                                .size = Style::evaluate(Style::Percentage<> { narrowPrecisionToFloat(lengthPercentage.value) }, parentSize),
                                .keyword = CSSValueInvalid
                            };
                        },
                        [&](CSS::LengthUnit lengthUnit) -> ResolvedFontSize {
                            auto fontCascade = FontCascade(FontCascadeDescription(fontDescription));
                            fontCascade.update(context->cssFontSelector());

                            // FIXME: Passing null for the RenderView parameter means that vw and vh units will evaluate to
                            //        zero and vmin and vmax units will evaluate as if they were px units.
                            //        It's unclear in the specification if they're expected to work on OffscreenCanvas, given
                            //        that it's off-screen and therefore doesn't strictly have an associated viewport.
                            //        This needs clarification and possibly fixing.
                            // FIXME: How should root font units work in OffscreenCanvas?

                            RefPtr document = dynamicDowncast<Document>(context);
                            return {
                                .size = static_cast<float>(Style::computeUnzoomedNonCalcLengthDouble(lengthPercentage.value, lengthUnit, CSSPropertyFontSize, &fontCascade, document ? document->renderView() : nullptr)),
                                .keyword = CSSValueInvalid
                            };
                        }
                    );
                },
                [&](const CSS::LengthPercentage<CSS::Nonnegative>::Calc& calc) -> ResolvedFontSize {
                    // FIXME: Figure out correct behavior when conversion data is required.
                    if (requiresConversionData(calc))
                        return { .size = 0.0f, .keyword = CSSValueInvalid };

                    return {
                        .size = Style::evaluate(Style::toStyleNoConversionDataRequired(calc), parentSize),
                        .keyword = CSSValueInvalid
                    };
                }
            );
        }
    );
}

// MARK: - 'font-variant-caps'

static FontVariantCaps fontVariantCapsFromUnresolvedFontVariantCaps(const CSSPropertyParserHelpers::UnresolvedFontVariantCaps& unresolvedVariantCaps)
{
    return fromCSSValueID<FontVariantCaps>(unresolvedVariantCaps);
}

// MARK: - 'font-family'

struct ResolvedFontFamily {
    Vector<AtomString> family;
    bool isSpecifiedFont;
};

static ResolvedFontFamily fontFamilyFromUnresolvedFontFamily(const CSSPropertyParserHelpers::UnresolvedFontFamily& unresolvedFamily, Ref<ScriptExecutionContext> context)
{
    bool isFirstFont = true;
    bool isSpecifiedFont = false;

    auto family = WTF::compactMap(unresolvedFamily, [&](auto& item) -> std::optional<AtomString> {
        auto [family, isGenericFamily] = switchOn(item,
            [&](CSSValueID ident) -> std::pair<AtomString, bool> {
                if (ident != CSSValueWebkitBody) {
                    // FIXME: Treat system-ui like other generic font families
                    if (ident == CSSValueSystemUi)
                        return { nameString(CSSValueSystemUi), true };
                    return { familyNamesData->at(CSSPropertyParserHelpers::genericFontFamilyIndex(ident)), true };
                }
                return { AtomString(context->settingsValues().fontGenericFamilies.standardFontFamily()), false };
            },
            [&](const AtomString& familyString) -> std::pair<AtomString, bool> {
                return { familyString, false };
            }
        );

        if (family.isEmpty())
            return std::nullopt;

        if (isFirstFont) {
            isSpecifiedFont = !isGenericFamily;
            isFirstFont = false;
        }
        return family;
    });

    return {
        .family = WTFMove(family),
        .isSpecifiedFont = isSpecifiedFont
    };
}

// MARK: 'font-feature-settings'

FontFeatureSettings fontFeatureSettingsFromCSSValue(BuilderState& builderState, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(value.valueID() == CSSValueNormal || CSSPropertyParserHelpers::isSystemFontShorthand(value.valueID()));
        return { };
    }

    auto list = BuilderConverter::requiredListDowncast<CSSValueList, CSSFontFeatureValue>(builderState, value);
    if (!list)
        return { };

    FontFeatureSettings settings;
    for (Ref feature : *list)
        settings.insert(FontFeature(feature->tag(), feature->value().resolveAsNumber<int>(builderState.cssToLengthConversionData())));
    return settings;
}

// MARK: 'font-variation-settings'

FontVariationSettings fontVariationSettingsFromCSSValue(BuilderState& builderState, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(value.valueID() == CSSValueNormal || CSSPropertyParserHelpers::isSystemFontShorthand(value.valueID()));
        return { };
    }

    auto list = BuilderConverter::requiredListDowncast<CSSValueList, CSSFontVariationValue>(builderState, value);
    if (!list)
        return { };

    FontVariationSettings settings;
    for (Ref feature : *list)
        settings.insert({ feature->tag(), feature->value().resolveAsNumber<float>(builderState.cssToLengthConversionData()) });
    return settings;
}

// MARK: 'font-size-adjust'

FontSizeAdjust fontSizeAdjustFromCSSValue(BuilderState& builderState, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->valueID() == CSSValueNone
            || CSSPropertyParserHelpers::isSystemFontShorthand(primitiveValue->valueID()))
            return FontCascadeDescription::initialFontSizeAdjust();

        auto defaultMetric = FontSizeAdjust::Metric::ExHeight;
        if (primitiveValue->isNumber())
            return { defaultMetric, FontSizeAdjust::ValueType::Number, primitiveValue->resolveAsNumber(builderState.cssToLengthConversionData()) };

        ASSERT(primitiveValue->valueID() == CSSValueFromFont);
        // We cannot determine the primary font here, so we defer resolving the
        // aspect value for from-font to when the primary font is created.
        // See FontCascadeFonts::primaryFont().
        return { defaultMetric, FontSizeAdjust::ValueType::FromFont, std::nullopt };
    }

    auto pair = BuilderConverter::requiredPairDowncast<CSSPrimitiveValue>(builderState, value);
    if (!pair)
        return { };

    auto metric = fromCSSValueID<FontSizeAdjust::Metric>(pair->first.valueID());
    if (pair->second.isNumber())
        return { metric, FontSizeAdjust::ValueType::Number, pair->second.resolveAsNumber(builderState.cssToLengthConversionData()) };

    ASSERT(pair->second.valueID() == CSSValueFromFont);
    return { metric, FontSizeAdjust::ValueType::FromFont, std::nullopt };
}


// MARK: - Unresolved Font Shorthand Resolution

std::optional<FontCascade> resolveForUnresolvedFont(const CSSPropertyParserHelpers::UnresolvedFont& unresolvedFont, FontCascadeDescription&& fontDescription, ScriptExecutionContext& context)
{
    Ref protectedContext = context;

    ASSERT(protectedContext->cssFontSelector());

    // Map the font property longhands into the style.
    float parentSize = fontDescription.specifiedSize();

    auto useFixedDefaultSize = [](const FontCascadeDescription& fontDescription) {
        return fontDescription.familyCount() == 1
            && fontDescription.firstFamily() == familyNamesData->at(FamilyNamesIndex::MonospaceFamily);
    };

    // Font family applied in the same way as StyleBuilderCustom::applyValueFontFamily
    // Before mapping in a new font-family property, we should reset the generic family.
    bool oldFamilyUsedFixedDefaultSize = useFixedDefaultSize(fontDescription);

    auto resolvedFamily = fontFamilyFromUnresolvedFontFamily(unresolvedFont.family, protectedContext);
    if (resolvedFamily.family.isEmpty())
        return std::nullopt;
    fontDescription.setFamilies(resolvedFamily.family);
    fontDescription.setIsSpecifiedFont(resolvedFamily.isSpecifiedFont);

    if (useFixedDefaultSize(fontDescription) != oldFamilyUsedFixedDefaultSize) {
        if (auto sizeIdentifier = fontDescription.keywordSizeAsIdentifier()) {
            auto size = Style::fontSizeForKeyword(sizeIdentifier, !oldFamilyUsedFixedDefaultSize, protectedContext->settingsValues());
            fontDescription.setSpecifiedSize(size);
            fontDescription.setComputedSize(Style::computedFontSizeFromSpecifiedSize(size, fontDescription.isAbsoluteSize(), 1.0, MinimumFontSizeRule::None, protectedContext->settingsValues()));
        }
    }

    auto resolvedFontStyle = fontStyleFromUnresolvedFontStyle(unresolvedFont.style);
    fontDescription.setItalic(resolvedFontStyle.italic);
    fontDescription.setFontStyleAxis(resolvedFontStyle.axis);

    auto resolvedFontVariantCaps = fontVariantCapsFromUnresolvedFontVariantCaps(unresolvedFont.variantCaps);
    fontDescription.setVariantCaps(resolvedFontVariantCaps);

    auto resolvedWeight = fontWeightFromUnresolvedFontWeight(unresolvedFont.weight, fontDescription);
    fontDescription.setWeight(resolvedWeight);

    auto resolvedSize = fontSizeFromUnresolvedFontSize(unresolvedFont.size, parentSize, fontDescription, protectedContext);
    fontDescription.setKeywordSizeFromIdentifier(resolvedSize.keyword);
    if (resolvedSize.size > 0) {
        fontDescription.setSpecifiedSize(resolvedSize.size);
        fontDescription.setComputedSize(resolvedSize.size);
    }

    // As there is no line-height on FontCascade, there's no need to resolve it, even
    // though there is line-height information on CSSPropertyParserHelpers::UnresolvedFont.

    auto fontCascade = FontCascade(WTFMove(fontDescription));
    fontCascade.update(protectedContext->cssFontSelector());
    return fontCascade;
}

}
}
