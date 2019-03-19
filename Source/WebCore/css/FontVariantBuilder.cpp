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

#include "CSSPrimitiveValue.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "TextFlags.h"

namespace WebCore {

FontVariantLigaturesValues extractFontVariantLigatures(const CSSValue& value)
{
    FontVariantLigatures common = FontVariantLigatures::Normal;
    FontVariantLigatures discretionary = FontVariantLigatures::Normal;
    FontVariantLigatures historical = FontVariantLigatures::Normal;
    FontVariantLigatures contextualAlternates = FontVariantLigatures::Normal;

    if (is<CSSValueList>(value)) {
        for (auto& item : downcast<CSSValueList>(value)) {
            switch (downcast<CSSPrimitiveValue>(item.get()).valueID()) {
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
        switch (downcast<CSSPrimitiveValue>(value).valueID()) {
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

    if (is<CSSValueList>(value)) {
        for (auto& item : downcast<CSSValueList>(value)) {
            switch (downcast<CSSPrimitiveValue>(item.get()).valueID()) {
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
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNormal);

    return FontVariantNumericValues(figure, spacing, fraction, ordinal, slashedZero);
}

FontVariantEastAsianValues extractFontVariantEastAsian(const CSSValue& value)
{
    FontVariantEastAsianVariant variant = FontVariantEastAsianVariant::Normal;
    FontVariantEastAsianWidth width = FontVariantEastAsianWidth::Normal;
    FontVariantEastAsianRuby ruby = FontVariantEastAsianRuby::Normal;

    if (is<CSSValueList>(value)) {
        for (auto& item : downcast<CSSValueList>(value)) {
            switch (downcast<CSSPrimitiveValue>(item.get()).valueID()) {
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
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNormal);

    return FontVariantEastAsianValues(variant, width, ruby);
}

Ref<CSSValue> computeFontVariant(const FontVariantSettings& variantSettings)
{
    if (variantSettings.isAllNormal())
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNormal);

    auto list = CSSValueList::createSpaceSeparated();

    switch (variantSettings.commonLigatures) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueCommonLigatures));
        break;
    case FontVariantLigatures::No:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueNoCommonLigatures));
        break;
    }

    switch (variantSettings.discretionaryLigatures) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueDiscretionaryLigatures));
        break;
    case FontVariantLigatures::No:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueNoDiscretionaryLigatures));
        break;
    }

    switch (variantSettings.historicalLigatures) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueHistoricalLigatures));
        break;
    case FontVariantLigatures::No:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueNoHistoricalLigatures));
        break;
    }

    switch (variantSettings.contextualAlternates) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueContextual));
        break;
    case FontVariantLigatures::No:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueNoContextual));
        break;
    }

    switch (variantSettings.position) {
    case FontVariantPosition::Normal:
        break;
    case FontVariantPosition::Subscript:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueSub));
        break;
    case FontVariantPosition::Superscript:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueSuper));
        break;
    }

    switch (variantSettings.caps) {
    case FontVariantCaps::Normal:
        break;
    case FontVariantCaps::Small:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueSmallCaps));
        break;
    case FontVariantCaps::AllSmall:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueAllSmallCaps));
        break;
    case FontVariantCaps::Petite:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValuePetiteCaps));
        break;
    case FontVariantCaps::AllPetite:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueAllPetiteCaps));
        break;
    case FontVariantCaps::Unicase:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueUnicase));
        break;
    case FontVariantCaps::Titling:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueTitlingCaps));
        break;
    }

    switch (variantSettings.numericFigure) {
    case FontVariantNumericFigure::Normal:
        break;
    case FontVariantNumericFigure::LiningNumbers:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueLiningNums));
        break;
    case FontVariantNumericFigure::OldStyleNumbers:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueOldstyleNums));
        break;
    }

    switch (variantSettings.numericSpacing) {
    case FontVariantNumericSpacing::Normal:
        break;
    case FontVariantNumericSpacing::ProportionalNumbers:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueProportionalNums));
        break;
    case FontVariantNumericSpacing::TabularNumbers:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueTabularNums));
        break;
    }

    switch (variantSettings.numericFraction) {
    case FontVariantNumericFraction::Normal:
        break;
    case FontVariantNumericFraction::DiagonalFractions:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueDiagonalFractions));
        break;
    case FontVariantNumericFraction::StackedFractions:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueStackedFractions));
        break;
    }

    switch (variantSettings.numericOrdinal) {
    case FontVariantNumericOrdinal::Normal:
        break;
    case FontVariantNumericOrdinal::Yes:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueOrdinal));
        break;
    }

    switch (variantSettings.numericSlashedZero) {
    case FontVariantNumericSlashedZero::Normal:
        break;
    case FontVariantNumericSlashedZero::Yes:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueSlashedZero));
        break;
    }

    switch (variantSettings.alternates) {
    case FontVariantAlternates::Normal:
        break;
    case FontVariantAlternates::HistoricalForms:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueHistoricalForms));
        break;
    }

    switch (variantSettings.eastAsianVariant) {
    case FontVariantEastAsianVariant::Normal:
        break;
    case FontVariantEastAsianVariant::Jis78:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueJis78));
        break;
    case FontVariantEastAsianVariant::Jis83:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueJis83));
        break;
    case FontVariantEastAsianVariant::Jis90:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueJis90));
        break;
    case FontVariantEastAsianVariant::Jis04:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueJis04));
        break;
    case FontVariantEastAsianVariant::Simplified:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueSimplified));
        break;
    case FontVariantEastAsianVariant::Traditional:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueTraditional));
        break;
    }

    switch (variantSettings.eastAsianWidth) {
    case FontVariantEastAsianWidth::Normal:
        break;
    case FontVariantEastAsianWidth::Full:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueFullWidth));
        break;
    case FontVariantEastAsianWidth::Proportional:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueProportionalWidth));
        break;
    }

    switch (variantSettings.eastAsianRuby) {
    case FontVariantEastAsianRuby::Normal:
        break;
    case FontVariantEastAsianRuby::Yes:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueRuby));
        break;
    }

    return list;
}

}

