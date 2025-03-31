/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "FontVariantBuilder.h"

#include "CSSFunctionValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "StyleBuilderState.h"
#include "TextFlags.h"

namespace WebCore {

FontVariantLigaturesValues extractFontVariantLigatures(const CSSValue& value)
{
    FontVariantLigatures common = FontVariantLigatures::Normal;
    FontVariantLigatures discretionary = FontVariantLigatures::Normal;
    FontVariantLigatures historical = FontVariantLigatures::Normal;
    FontVariantLigatures contextualAlternates = FontVariantLigatures::Normal;

    if (auto* valueList = dynamicDowncast<CSSValueList>(value)) {
        for (auto& item : *valueList) {
            switch (item.valueID()) {
            case CSSValueNoCommonLigatures:
                common = FontVariantLigatures::No;
                break;
            case CSSValueCommonLigatures:
                common = FontVariantLigatures::Yes;
                break;
            case CSSValueNoDiscretionaryLigatures:
                discretionary = FontVariantLigatures::No;
                break;
            case CSSValueDiscretionaryLigatures:
                discretionary = FontVariantLigatures::Yes;
                break;
            case CSSValueNoHistoricalLigatures:
                historical = FontVariantLigatures::No;
                break;
            case CSSValueHistoricalLigatures:
                historical = FontVariantLigatures::Yes;
                break;
            case CSSValueContextual:
                contextualAlternates = FontVariantLigatures::Yes;
                break;
            case CSSValueNoContextual:
                contextualAlternates = FontVariantLigatures::No;
                break;
            default:
                ASSERT_NOT_REACHED();
                break;
            }
        }
    } else if (is<CSSPrimitiveValue>(value)) {
        switch (value.valueID()) {
        case CSSValueNormal:
            break;
        case CSSValueNone:
            common = FontVariantLigatures::No;
            discretionary = FontVariantLigatures::No;
            historical = FontVariantLigatures::No;
            contextualAlternates = FontVariantLigatures::No;
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    return FontVariantLigaturesValues(common, discretionary, historical, contextualAlternates);
}

FontVariantNumericValues extractFontVariantNumeric(const CSSValue& value)
{
    FontVariantNumericFigure figure = FontVariantNumericFigure::Normal;
    FontVariantNumericSpacing spacing = FontVariantNumericSpacing::Normal;
    FontVariantNumericFraction fraction = FontVariantNumericFraction::Normal;
    FontVariantNumericOrdinal ordinal = FontVariantNumericOrdinal::Normal;
    FontVariantNumericSlashedZero slashedZero = FontVariantNumericSlashedZero::Normal;

    if (auto* valueList = dynamicDowncast<CSSValueList>(value)) {
        for (auto& item : *valueList) {
            switch (item.valueID()) {
            case CSSValueLiningNums:
                figure = FontVariantNumericFigure::LiningNumbers;
                break;
            case CSSValueOldstyleNums:
                figure = FontVariantNumericFigure::OldStyleNumbers;
                break;
            case CSSValueProportionalNums:
                spacing = FontVariantNumericSpacing::ProportionalNumbers;
                break;
            case CSSValueTabularNums:
                spacing = FontVariantNumericSpacing::TabularNumbers;
                break;
            case CSSValueDiagonalFractions:
                fraction = FontVariantNumericFraction::DiagonalFractions;
                break;
            case CSSValueStackedFractions:
                fraction = FontVariantNumericFraction::StackedFractions;
                break;
            case CSSValueOrdinal:
                ordinal = FontVariantNumericOrdinal::Yes;
                break;
            case CSSValueSlashedZero:
                slashedZero = FontVariantNumericSlashedZero::Yes;
                break;
            default:
                ASSERT_NOT_REACHED();
                break;
            }
        }
    } else if (is<CSSPrimitiveValue>(value))
        ASSERT(value.valueID() == CSSValueNormal);

    return FontVariantNumericValues(figure, spacing, fraction, ordinal, slashedZero);
}

FontVariantEastAsianValues extractFontVariantEastAsian(const CSSValue& value)
{
    FontVariantEastAsianVariant variant = FontVariantEastAsianVariant::Normal;
    FontVariantEastAsianWidth width = FontVariantEastAsianWidth::Normal;
    FontVariantEastAsianRuby ruby = FontVariantEastAsianRuby::Normal;

    if (auto* valueList = dynamicDowncast<CSSValueList>(value)) {
        for (auto& item : *valueList) {
            switch (item.valueID()) {
            case CSSValueJis78:
                variant = FontVariantEastAsianVariant::Jis78;
                break;
            case CSSValueJis83:
                variant = FontVariantEastAsianVariant::Jis83;
                break;
            case CSSValueJis90:
                variant = FontVariantEastAsianVariant::Jis90;
                break;
            case CSSValueJis04:
                variant = FontVariantEastAsianVariant::Jis04;
                break;
            case CSSValueSimplified:
                variant = FontVariantEastAsianVariant::Simplified;
                break;
            case CSSValueTraditional:
                variant = FontVariantEastAsianVariant::Traditional;
                break;
            case CSSValueFullWidth:
                width = FontVariantEastAsianWidth::Full;
                break;
            case CSSValueProportionalWidth:
                width = FontVariantEastAsianWidth::Proportional;
                break;
            case CSSValueRuby:
                ruby = FontVariantEastAsianRuby::Yes;
                break;
            default:
                ASSERT_NOT_REACHED();
                break;
            }
        }
    } else if (is<CSSPrimitiveValue>(value))
        ASSERT(value.valueID() == CSSValueNormal);

    return FontVariantEastAsianValues(variant, width, ruby);
}

FontVariantAlternates extractFontVariantAlternates(const CSSValue& value, Style::BuilderState& builderState)
{
    auto processSingleItemFunction = [&](const CSSFunctionValue& function, String& parameterToSet) -> bool {
        if (function.size() != 1) {
            builderState.setCurrentPropertyInvalidAtComputedValueTime();
            return false;
        }
        RefPtr primitiveArgument = dynamicDowncast<CSSPrimitiveValue>(function[0]);
        if (!primitiveArgument || !primitiveArgument->isCustomIdent()) {
            builderState.setCurrentPropertyInvalidAtComputedValueTime();
            return false;
        }
        parameterToSet = primitiveArgument->customIdent();
        return true;
    };

    auto processListFunction = [&](const CSSFunctionValue& function, Vector<String>& parameterToSet) -> bool {
        if (!function.size()) {
            builderState.setCurrentPropertyInvalidAtComputedValueTime();
            return false;
        }
        for (Ref argument : function) {
            RefPtr primitiveArgument = dynamicDowncast<CSSPrimitiveValue>(argument);
            if (!primitiveArgument || !primitiveArgument->isCustomIdent()) {
                builderState.setCurrentPropertyInvalidAtComputedValueTime();
                return false;
            }
            parameterToSet.append(primitiveArgument->customIdent());
        }
        return true;
    };

    auto result = FontVariantAlternates::Normal();

    if (RefPtr valueList = dynamicDowncast<CSSValueList>(value)) {
        for (Ref item : *valueList) {
            if (RefPtr primitive = dynamicDowncast<CSSPrimitiveValue>(item.get())) {
                switch (primitive->valueID()) {
                case CSSValueHistoricalForms:
                    result.valuesRef().historicalForms = true;
                    break;
                default:
                    builderState.setCurrentPropertyInvalidAtComputedValueTime();
                    return FontVariantAlternates::Normal();
                }
            } else if (RefPtr function = dynamicDowncast<CSSFunctionValue>(item.get())) {
                switch (function->name()) {
                case CSSValueSwash:
                    if (!processSingleItemFunction(*function, result.valuesRef().swash))
                        return FontVariantAlternates::Normal();
                    break;
                case CSSValueStylistic:
                    if (!processSingleItemFunction(*function, result.valuesRef().stylistic))
                        return FontVariantAlternates::Normal();
                    break;
                case CSSValueStyleset:
                    if (!processListFunction(*function, result.valuesRef().styleset))
                        return FontVariantAlternates::Normal();
                    break;
                case CSSValueCharacterVariant:
                    if (!processListFunction(*function, result.valuesRef().characterVariant))
                        return FontVariantAlternates::Normal();
                    break;
                case CSSValueOrnaments:
                    if (!processSingleItemFunction(*function, result.valuesRef().ornaments))
                        return FontVariantAlternates::Normal();
                    break;
                case CSSValueAnnotation:
                    if (!processSingleItemFunction(*function, result.valuesRef().annotation))
                        return FontVariantAlternates::Normal();
                    break;
                default:
                    builderState.setCurrentPropertyInvalidAtComputedValueTime();
                    return FontVariantAlternates::Normal();
                }
            } else {
                builderState.setCurrentPropertyInvalidAtComputedValueTime();
                return FontVariantAlternates::Normal();
            }
        }
    } else if (is<CSSPrimitiveValue>(value)) {
        switch (value.valueID()) {
        case CSSValueNormal:
            break;
        case CSSValueHistoricalForms:
            result.valuesRef().historicalForms = true;
            break;
        default:
            builderState.setCurrentPropertyInvalidAtComputedValueTime();
            return FontVariantAlternates::Normal();
        }
    }

    return result;
}

} // WebCore namespace
