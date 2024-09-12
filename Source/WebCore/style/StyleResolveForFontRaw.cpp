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
#include "StyleResolveForFontRaw.h"

#include "CSSCalcSymbolTable.h"
#include "CSSFontSelector.h"
#include "CSSPropertyParserConsumer+Font.h"
#include "CSSPropertyParserConsumer+UnevaluatedCalc.h"
#include "CSSPropertyParserHelpers.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "FontCascade.h"
#include "FontCascadeDescription.h"
#include "FontSelectionValueInlines.h"
#include "RenderStyle.h"
#include "ScriptExecutionContext.h"
#include "Settings.h"
#include "StyleFontSizeFunctions.h"
#include "WebKitFontFamilyNames.h"

namespace WebCore {

namespace Style {

using namespace WebKitFontFamilyNames;

static bool useFixedDefaultSize(const FontCascadeDescription& fontDescription)
{
    return fontDescription.familyCount() == 1 && fontDescription.firstFamily() == familyNamesData->at(FamilyNamesIndex::MonospaceFamily);
}

std::optional<FontCascade> resolveForUnresolvedFont(const CSSPropertyParserHelpers::UnresolvedFont& fontRaw, FontCascadeDescription&& fontDescription, ScriptExecutionContext& context)
{
    ASSERT(context.cssFontSelector());

    // Map the font property longhands into the style.
    float parentSize = fontDescription.specifiedSize();

    // Font family applied in the same way as StyleBuilderCustom::applyValueFontFamily
    // Before mapping in a new font-family property, we should reset the generic family.
    bool oldFamilyUsedFixedDefaultSize = useFixedDefaultSize(fontDescription);

    bool isFirstFont = true;
    auto families = WTF::compactMap(fontRaw.family, [&](auto& item) -> std::optional<AtomString> {
        auto [family, isGenericFamily] = switchOn(item,
            [&](CSSValueID ident) -> std::pair<AtomString, bool> {
                if (ident != CSSValueWebkitBody) {
                    // FIXME: Treat system-ui like other generic font families
                    if (ident == CSSValueSystemUi)
                        return { nameString(CSSValueSystemUi), true };
                    else
                        return { familyNamesData->at(CSSPropertyParserHelpers::genericFontFamilyIndex(ident)), true };
                } else
                    return { AtomString(context.settingsValues().fontGenericFamilies.standardFontFamily()), false };
            },
            [&](const AtomString& familyString) -> std::pair<AtomString, bool> {
                return { familyString, false };
            }
        );

        if (family.isEmpty())
            return std::nullopt;

        if (isFirstFont) {
            fontDescription.setIsSpecifiedFont(!isGenericFamily);
            isFirstFont = false;
        }
        return family;
    });

    if (families.isEmpty())
        return std::nullopt;
    fontDescription.setFamilies(families);

    if (useFixedDefaultSize(fontDescription) != oldFamilyUsedFixedDefaultSize) {
        if (CSSValueID sizeIdentifier = fontDescription.keywordSizeAsIdentifier()) {
            auto size = Style::fontSizeForKeyword(sizeIdentifier, !oldFamilyUsedFixedDefaultSize, context.settingsValues());
            fontDescription.setSpecifiedSize(size);
            fontDescription.setComputedSize(Style::computedFontSizeFromSpecifiedSize(size, fontDescription.isAbsoluteSize(), 1.0, MinimumFontSizeRule::None, context.settingsValues()));
        }
    }

    // Font style applied in the same way as BuilderConverter::convertFontStyleFromValue
    WTF::switchOn(fontRaw.style,
        [&](CSSValueID ident) {
            switch (ident) {
            case CSSValueNormal:
                fontDescription.setFontStyleAxis(FontStyleAxis::slnt);
                break;

            case CSSValueItalic:
                fontDescription.setFontStyleAxis(FontStyleAxis::ital);
                fontDescription.setItalic(italicValue());
                break;

            case CSSValueOblique:
                fontDescription.setFontStyleAxis(FontStyleAxis::slnt);
                fontDescription.setItalic(FontSelectionValue(0.0f)); // FIXME: Spec says this should be 14deg.
                break;

            default:
                ASSERT_NOT_REACHED();
                break;
            }
        },
        [&](AngleRaw angle) {
            fontDescription.setFontStyleAxis(FontStyleAxis::slnt);
            fontDescription.setItalic(FontSelectionValue::clampFloat(CSSPrimitiveValue::computeDegrees(angle.type, angle.value)));
        },
        [&](const UnevaluatedCalc<AngleRaw>& calc) {
            fontDescription.setFontStyleAxis(FontStyleAxis::slnt);

            // FIXME: Figure out correct behavior when conversion data is required.
            if (requiresConversionData(calc))
                return;

            fontDescription.setItalic(normalizedFontItalicValue(static_cast<float>(evaluateCalcNoConversionDataRequired(calc, { }).value)));
        }
    );

    switch (fontRaw.variantCaps) {
    case CSSValueNormal:
        fontDescription.setVariantCaps(FontVariantCaps::Normal);
        break;
    case CSSValueSmallCaps:
        fontDescription.setVariantCaps(FontVariantCaps::Small);
        break;
    case CSSValueAllSmallCaps:
        fontDescription.setVariantCaps(FontVariantCaps::AllSmall);
        break;
    case CSSValuePetiteCaps:
        fontDescription.setVariantCaps(FontVariantCaps::Petite);
        break;
    case CSSValueAllPetiteCaps:
        fontDescription.setVariantCaps(FontVariantCaps::AllPetite);
        break;
    case CSSValueUnicase:
        fontDescription.setVariantCaps(FontVariantCaps::Unicase);
        break;
    case CSSValueTitlingCaps:
        fontDescription.setVariantCaps(FontVariantCaps::Titling);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    auto weight = WTF::switchOn(fontRaw.weight,
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
        [&](NumberRaw weight) {
            return FontSelectionValue::clampFloat(weight.value);
        },
        [&](const UnevaluatedCalc<NumberRaw>& calc) {
            // FIXME: Figure out correct behavior when conversion data is required.
            if (requiresConversionData(calc))
                return normalWeightValue();

            return FontSelectionValue(clampTo<float>(evaluateCalcNoConversionDataRequired(calc, { }).value, 1, 1000));
        }
    );
    fontDescription.setWeight(weight);

    fontDescription.setKeywordSizeFromIdentifier(CSSValueInvalid);

    float size = WTF::switchOn(fontRaw.size,
        [&](CSSValueID ident) {
            switch (ident) {
            case CSSValueXxSmall:
            case CSSValueXSmall:
            case CSSValueSmall:
            case CSSValueMedium:
            case CSSValueLarge:
            case CSSValueXLarge:
            case CSSValueXxLarge:
            case CSSValueXxxLarge:
                fontDescription.setKeywordSizeFromIdentifier(ident);
                return Style::fontSizeForKeyword(ident, fontDescription.useFixedDefaultSize(), context.settingsValues());
            case CSSValueLarger:
                return parentSize * 1.02f;
            case CSSValueSmaller:
                return parentSize / 1.02f;
            default:
                return 0.0f;
            }
        },
        [&](const LengthPercentageRaw& lengthPercentage) {
            if (lengthPercentage.type == CSSUnitType::CSS_PERCENTAGE)
                return static_cast<float>((parentSize * lengthPercentage.value) / 100.0);
            else {
                auto fontCascade = FontCascade(FontCascadeDescription(fontDescription));
                fontCascade.update(context.cssFontSelector());

                // FIXME: Passing null for the RenderView parameter means that vw and vh units will evaluate to
                //        zero and vmin and vmax units will evaluate as if they were px units.
                //        It's unclear in the specification if they're expected to work on OffscreenCanvas, given
                //        that it's off-screen and therefore doesn't strictly have an associated viewport.
                //        This needs clarification and possibly fixing.
                // FIXME: How should root font units work in OffscreenCanvas?

                auto* document = dynamicDowncast<Document>(context);
                return static_cast<float>(CSSPrimitiveValue::computeUnzoomedNonCalcLengthDouble(lengthPercentage.type, lengthPercentage.value, CSSPropertyFontSize, &fontCascade, document ? document->renderView() : nullptr));
            }
        },
        [&](const UnevaluatedCalc<LengthPercentageRaw>& calc) {
            // FIXME: Figure out correct behavior when conversion data is required.
            if (requiresConversionData(calc))
                return 0.0f;

            auto length = calc.calc->lengthPercentageValueNoConversionDataRequired({ });
            return floatValueForLength(length, parentSize);
        }
    );

    if (size > 0) {
        fontDescription.setSpecifiedSize(size);
        fontDescription.setComputedSize(size);
    }

    // As there is no line-height on FontCascade, there's no need to resolve it, even
    // though there is line-height information on CSSPropertyParserHelpers::UnresolvedFont.

    auto fontCascade = FontCascade(WTFMove(fontDescription));
    fontCascade.update(context.cssFontSelector());
    return fontCascade;
}

}
}
