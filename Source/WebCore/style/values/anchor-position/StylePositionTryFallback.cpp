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
#include "CSSPropertyParserConsumer+Anchor.h"
#include "CSSValuePair.h"
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
                    .rule = WTF::move(rule),
                }
            };
        } else {
            return {
                .ruleAndTactics = {
                    .rule = WTF::move(rule),
                    .tactics = WTF::move(tactics),
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

// Return the computed-value form of a position-area CSS value stored in a
// PositionTryFallback::PositionArea's ImmutableStyleProperties.
//
// When a position-area is inlined in position-try-fallbacks (e.g.
// "position-try-fallbacks: block-start span-inline-end, ..."), the raw
// specified CSS value is stored as-is. At computed-value time the block-/inline-
// axis prefixes must be stripped when the two keywords are on opposite axes.
// For example, "block-start span-inline-end" must become "start span-end".
//
// This mirrors the logic in CSSValueCreation<PositionAreaValue> which calls
// valueForPositionArea(..., ValueType::Computed) via the PositionAreaValue
// style type, but that path is only used for the standalone position-area
// property, not for inlined position-area values inside position-try-fallbacks.
static Ref<CSSValue> computedPositionAreaCSSValue(const PositionTryFallback::PositionArea& positionArea)
{
    using namespace CSSPropertyParserHelpers;

    Ref rawValue = RefPtr { positionArea.properties }->getPropertyCSSValue(CSSPropertyPositionArea).releaseNonNull();

    // Only two-keyword position-area values require computed-value normalization.
    // Single-keyword values (e.g. "center", "start") have no block-/inline- prefix
    // to strip and are already in their canonical computed form.
    if (!rawValue->isPair())
        return rawValue;

    auto dim1 = rawValue->first().valueID();
    auto dim2 = rawValue->second().valueID();

    if (auto computed = valueForPositionArea(dim1, dim2, ValueType::Computed))
        return computed.releaseNonNull();

    // Fallback: return the raw value unchanged if normalization fails.
    return rawValue;
}

auto CSSValueCreation<PositionTryFallback::PositionArea>::operator()(CSSValuePool&, const RenderStyle&, const PositionTryFallback::PositionArea& value) -> Ref<CSSValue>
{
    return computedPositionAreaCSSValue(value);
}

// MARK: - Serialization

void Serialize<PositionTryFallback::PositionArea>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle&, const PositionTryFallback::PositionArea& value)
{
    builder.append(computedPositionAreaCSSValue(value)->cssText(context));
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const PositionTryFallback::PositionArea& value)
{
    return ts << RefPtr { value.properties }->getPropertyValue(CSSPropertyPositionArea);
}

} // namespace Style
} // namespace WebCore
