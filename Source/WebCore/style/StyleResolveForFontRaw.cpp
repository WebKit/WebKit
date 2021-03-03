/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L.
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

#include "CSSFontSelector.h"
#include "CSSPropertyParserHelpers.h"
#include "CSSToLengthConversionData.h"
#include "Document.h"
#include "FontCascade.h"
#include "FontCascadeDescription.h"
#include "RenderStyle.h"
#include "Settings.h"
#include "StyleFontSizeFunctions.h"

namespace WebCore {

namespace Style {

using namespace CSSPropertyParserHelpers;

Optional<RenderStyle> resolveForFontRaw(const FontRaw& fontRaw, FontCascadeDescription&& fontDescription, Document& document)
{
    auto fontStyle = RenderStyle::create();

    auto getParentStyle = [&] () -> std::unique_ptr<RenderStyle> {
        auto parentStyle = RenderStyle::clonePtr(fontStyle);
        parentStyle->setFontDescription(FontCascadeDescription(fontDescription));
        parentStyle->fontCascade().update(&document.fontSelector());
        return parentStyle;
    };

    // Map the font property longhands into the style.
    float parentSize = fontDescription.specifiedSize();

    // Font family applied in the same way as StyleBuilderCustom::applyValueFontFamily
    // Before mapping in a new font-family property, we should reset the generic family.
    bool oldFamilyUsedFixedDefaultSize = fontDescription.useFixedDefaultSize();

    Vector<AtomString> families;
    families.reserveInitialCapacity(fontRaw.family.size());

    for (auto& item : fontRaw.family) {
        AtomString family;
        bool isGenericFamily = false;
        switchOn(item, [&] (CSSValueID ident) {
            isGenericFamily = ident != CSSValueWebkitBody;
            family = isGenericFamily ? CSSPropertyParserHelpers::genericFontFamilyFromValueID(ident) : AtomString(document.settings().standardFontFamily());
        }, [&] (const String& familyString) {
            family = familyString;
        });

        if (family.isEmpty())
            continue;
        if (families.isEmpty())
            fontDescription.setIsSpecifiedFont(!isGenericFamily);
        families.uncheckedAppend(family);
    }

    if (families.isEmpty())
        return WTF::nullopt;
    fontDescription.setFamilies(families);

    if (fontDescription.useFixedDefaultSize() != oldFamilyUsedFixedDefaultSize) {
        if (CSSValueID sizeIdentifier = fontDescription.keywordSizeAsIdentifier()) {
            auto size = Style::fontSizeForKeyword(sizeIdentifier, !oldFamilyUsedFixedDefaultSize, document);
            fontDescription.setSpecifiedSize(size);
            fontDescription.setComputedSize(Style::computedFontSizeFromSpecifiedSize(size, fontDescription.isAbsoluteSize(), false, &fontStyle, document));
        }
    }

    // Font style applied in the same way as BuilderConverter::convertFontStyleFromValue
    if (fontRaw.style) {
        switch (fontRaw.style->style) {
        case CSSValueNormal:
            break;

        case CSSValueItalic:
            fontDescription.setItalic(italicValue());
            break;

        case CSSValueOblique: {
            float degrees;
            if (fontRaw.style->angle)
                degrees = static_cast<float>(CSSPrimitiveValue::computeDegrees(fontRaw.style->angle->type, fontRaw.style->angle->value));
            else
                degrees = 0;
            fontDescription.setItalic(FontSelectionValue(degrees));
            break;
        }

        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
    fontDescription.setFontStyleAxis((fontRaw.style && fontRaw.style->style == CSSValueItalic) ? FontStyleAxis::ital : FontStyleAxis::slnt);

    if (fontRaw.variantCaps) {
        switch (*fontRaw.variantCaps) {
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
    }

    if (fontRaw.weight) {
        auto weight = switchOn(*fontRaw.weight, [&] (CSSValueID ident) {
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
        }, [&] (double weight) {
            return FontSelectionValue::clampFloat(weight);
        });
        fontDescription.setWeight(weight);
    }

    fontDescription.setKeywordSizeFromIdentifier(CSSValueInvalid);
    float size = switchOn(fontRaw.size, [&] (CSSValueID ident) {
        switch (ident) {
        case CSSValueXxSmall:
        case CSSValueXSmall:
        case CSSValueSmall:
        case CSSValueMedium:
        case CSSValueLarge:
        case CSSValueXLarge:
        case CSSValueXxLarge:
        case CSSValueWebkitXxxLarge:
            fontDescription.setKeywordSizeFromIdentifier(ident);
            return Style::fontSizeForKeyword(ident, fontDescription.useFixedDefaultSize(), document);
        case CSSValueLarger:
            return parentSize * 1.2f;
        case CSSValueSmaller:
            return parentSize / 1.2f;
        default:
            return 0.f;
        }
    }, [&] (const CSSPropertyParserHelpers::LengthOrPercentRaw& lengthOrPercent) {
        return switchOn(lengthOrPercent, [&] (const CSSPropertyParserHelpers::LengthRaw& length) {
            auto parentStyle = getParentStyle();
            CSSToLengthConversionData conversionData { parentStyle.get(), nullptr, parentStyle.get(), document.renderView(), 1.0f, CSSPropertyFontSize };
            return static_cast<float>(CSSPrimitiveValue::computeNonCalcLengthDouble(conversionData, length.type, length.value));
        }, [&] (double percentage) {
            return static_cast<float>((parentSize * percentage) / 100.0);
        });
    });

    if (size > 0) {
        fontDescription.setSpecifiedSize(size);
        fontDescription.setComputedSize(size);
    }

    if (fontRaw.lineHeight) {
        Optional<Length> lineHeight = switchOn(*fontRaw.lineHeight, [&] (CSSValueID ident) {
            if (ident == CSSValueNormal)
                return Optional<Length>(RenderStyle::initialLineHeight());
            return Optional<Length>(WTF::nullopt);
        }, [&] (double number) {
            return Optional<Length>(Length(number * 100.0, LengthType::Percent));
        }, [&] (const CSSPropertyParserHelpers::LengthOrPercentRaw& lengthOrPercent) {
            return switchOn(lengthOrPercent, [&] (const CSSPropertyParserHelpers::LengthRaw& length) {
                auto parentStyle = getParentStyle();
                CSSToLengthConversionData conversionData { parentStyle.get(), nullptr, parentStyle.get(), document.renderView(), 1.0f, CSSPropertyLineHeight };
                return Optional<Length>(Length(clampTo<float>(CSSPrimitiveValue::computeNonCalcLengthDouble(conversionData, length.type, length.value), minValueForCssLength, maxValueForCssLength), LengthType::Fixed));
            }, [&] (double percentage) {
                return Optional<Length>(Length((fontDescription.computedSize() * static_cast<int>(percentage)) / 100, LengthType::Fixed));
            });
        });

        if (lineHeight)
            fontStyle.setLineHeight(WTFMove(lineHeight.value()));
    }

    fontStyle.setFontDescription(WTFMove(fontDescription));
    fontStyle.fontCascade().update(&document.fontSelector());

    return fontStyle;
}

}
}
