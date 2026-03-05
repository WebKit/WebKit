/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleFontFamily.h"

#include "CSSPropertyParserConsumer+Font.h"
#include "Document.h"
#include "Settings.h"
#include "StyleBuilderChecking.h"
#include "SystemFontDatabase.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<FontFamilies>::operator()(BuilderState& state, const CSSValue& value) -> FontFamilies
{
    using namespace CSSPropertyParserHelpers;

    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->isFontFamily()) {
            return {
                AtomString { primitiveValue->stringValue() },
                FontFamilyKind::Specified
            };
        }

        auto valueID = primitiveValue->valueID();
        if (valueID == CSSValueWebkitBody) {
            return {
                AtomString { state.document().settings().standardFontFamily() },
                FontFamilyKind::Specified
            };
        }

        if (auto family = genericFontFamily(valueID); !family.isNull()) {
            return {
                family,
                FontFamilyKind::Generic
            };
        }

        if (isSystemFontShorthand(valueID)) {
            auto family = SystemFontDatabase::singleton().systemFontShorthandFamily(lowerFontShorthand(valueID));
            ASSERT(!family.isEmpty());

            return {
                WTF::move(family),
                FontFamilyKind::Generic
            };
        }

        state.setCurrentPropertyInvalidAtComputedValueTime();
        return { nullAtom(), FontFamilyKind::Generic };
    }

    auto valueList = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(state, value);
    if (!valueList)
        return { nullAtom(), FontFamilyKind::Generic };

    std::optional<FontFamilyKind> firstFontKind;
    auto families = WTF::compactMap(*valueList, [&](auto& contentValue) -> std::optional<AtomString> {
        auto [family, kind] = [&] -> std::pair<AtomString, FontFamilyKind> {
            if (contentValue.isFontFamily())
                return { AtomString { contentValue.stringValue() }, FontFamilyKind::Specified };

            auto valueID = contentValue.valueID();
            if (valueID == CSSValueWebkitBody)
                return { AtomString { state.document().settings().standardFontFamily() }, FontFamilyKind::Specified };

            return { genericFontFamily(valueID), FontFamilyKind::Generic };
        }();

        if (family.isNull())
            return std::nullopt;

        if (!firstFontKind)
            firstFontKind = kind;

        return family;
    });

    if (families.isEmpty()) {
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return { nullAtom(), FontFamilyKind::Generic };
    }

    return {
        RefCountedFixedVector<AtomString>::createFromVector(WTF::move(families)),
        *firstFontKind
    };
}

} // namespace Style
} // namespace WebCore
