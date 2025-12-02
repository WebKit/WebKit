/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "StylePositionTryFallback.h"

#include "StyleBuilderChecking.h"
#include "StylePrimitiveKeyword+CSSValueConversion.h"
#include "StylePrimitiveKeyword+CSSValueCreation.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveKeyword+Serialization.h"
#include "StylePropertiesInlines.h"

namespace WebCore {
namespace Style {

bool PositionTryFallback::PositionArea::operator==(const PositionTryFallback::PositionArea& other) const
{
    if (properties && other.properties) {
        if (properties == other.properties)
            return true;

        Ref strongProperties = *properties;
        Ref strongOtherProperties = *other.properties;

        auto lhsPositionArea = strongProperties->getPropertyCSSValue(CSSPropertyPositionArea);
        ASSERT(lhsPositionArea);

        auto rhsPositionArea = strongOtherProperties->getPropertyCSSValue(CSSPropertyPositionArea);
        ASSERT(rhsPositionArea);

        return *lhsPositionArea == *rhsPositionArea;
    }

    return !properties && !other.properties;
}

bool PositionTryFallback::operator==(const PositionTryFallback& other) const
{
    if (positionArea.properties && other.positionArea.properties)
        return positionArea == other.positionArea;

    if (!positionArea.properties && !other.positionArea.properties)
        return ruleAndTactics == other.ruleAndTactics;

    // If we got here, this and other don't have the same type (e.g comparing position-area with rule + tactics)
    return false;
}

// MARK: - Conversion

auto CSSValueConversion<PositionTryFallback>::operator()(BuilderState& state, const CSSValue& value) -> PositionTryFallback
{
    if (RefPtr valueList = dynamicDowncast<CSSValueList>(value)) {
        if (valueList->separator() != CSSValueList::SpaceSeparator) {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return { };
        }

        auto rule = std::optional<ScopedName> { };
        auto tactics = SpaceSeparatedVector<PositionTryFallbackTactic> { };

        for (Ref item : *valueList) {
            switch (item->valueID()) {
            case CSSValueFlipBlock:
                tactics.value.append(PositionTryFallbackTactic::FlipBlock);
                break;
            case CSSValueFlipInline:
                tactics.value.append(PositionTryFallbackTactic::FlipInline);
                break;
            case CSSValueFlipStart:
                tactics.value.append(PositionTryFallbackTactic::FlipStart);
                break;
            case CSSValueFlipX:
                tactics.value.append(PositionTryFallbackTactic::FlipX);
                break;
            case CSSValueFlipY:
                tactics.value.append(PositionTryFallbackTactic::FlipY);
                break;
            case CSSValueInvalid:
                if (item->isCustomIdent() && !rule) {
                    rule = ScopedName { AtomString { item->customIdent() }, state.styleScopeOrdinal() };
                    break;
                }
                [[fallthrough]];
            default:
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return { };
            }
        }

        if (tactics.isEmpty()) {
            return {
                .ruleAndTactics = {
                    .rule = WTFMove(rule),
                }
            };
        } else {
            return {
                .ruleAndTactics = {
                    .rule = WTFMove(rule),
                    .tactics = WTFMove(tactics),
                }
            };
        }
    }

    // Turn the inlined position-area fallback into properties object that can be applied similarly to @position-try declarations.
    auto property = CSSProperty { CSSPropertyPositionArea, Ref { const_cast<CSSValue&>(value) } };
    return {
        .positionArea = { ImmutableStyleProperties::createDeduplicating(std::span { &property, 1 }, HTMLStandardMode) }
    };
}

auto CSSValueCreation<PositionTryFallback::PositionArea>::operator()(CSSValuePool&, const RenderStyle&, const PositionTryFallback::PositionArea& value) -> Ref<CSSValue>
{
    return RefPtr { value.properties }->getPropertyCSSValue(CSSPropertyPositionArea).releaseNonNull();
}

// MARK: - Serialization

void Serialize<PositionTryFallback::PositionArea>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle&, const PositionTryFallback::PositionArea& value)
{
    builder.append(RefPtr { value.properties }->getPropertyCSSValue(CSSPropertyPositionArea)->cssText(context));
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const PositionTryFallback::PositionArea& value)
{
    return ts << RefPtr { value.properties }->getPropertyValue(CSSPropertyPositionArea);
}

} // namespace Style
} // namespace WebCore
