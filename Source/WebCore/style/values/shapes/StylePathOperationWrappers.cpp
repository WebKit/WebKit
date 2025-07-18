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
#include "StylePathOperationWrappers.h"

#include "CSSBasicShapeValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSRayValue.h"
#include "CSSURLValue.h"
#include "SVGURIReference.h"
#include "CSSValueList.h"
#include "StyleBuilderState.h"
#include "StylePrimitiveKeyword+CSSValueCreation.h"
#include "StylePrimitiveKeyword+Serialization.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

RefPtr<PathOperation> CSSValueConversion<RefPtr<PathOperation>>::operator()(BuilderState& state, const CSSValue& value, SupportRayPathOperation supportRayPathOperation)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        ASSERT_UNUSED(primitiveValue, primitiveValue->valueID() == CSSValueNone);
        return nullptr;
    }

    if (RefPtr url = dynamicDowncast<CSSURLValue>(value)) {
        auto styleURL = toStyle(url->url(), state);

        // FIXME: ReferencePathOperation are not hooked up to support remote URLs yet, so only works with document local references. To see an example of how this should work, see ReferenceFilterOperation which supports both document local and remote URLs.

        auto fragment = SVGURIReference::fragmentIdentifierFromIRIString(styleURL, state.document());

        Ref treeScope = [&] -> Ref<const TreeScope> {
            if (RefPtr element = state.element())
                return element->treeScopeForSVGReferences();
            return state.document();
        }();
        auto target = SVGURIReference::targetElementFromIRIString(styleURL, treeScope);

        return ReferencePathOperation::create(WTFMove(styleURL), fragment, dynamicDowncast<SVGElement>(target.element.get()));
    }

    if (RefPtr ray = dynamicDowncast<CSSRayValue>(value)) {
        if (supportRayPathOperation == SupportRayPathOperation::No) {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return nullptr;
        }
        return RayPathOperation::create(toStyle(ray->ray(), state));
    }

    RefPtr<PathOperation> operation;
    auto referenceBox = CSSBoxType::BoxMissing;
    auto processSingleValue = [&](const CSSValue& singleValue) -> bool {
        ASSERT(!is<CSSValueList>(singleValue));
        if (RefPtr ray = dynamicDowncast<CSSRayValue>(singleValue)) {
            if (supportRayPathOperation == SupportRayPathOperation::No) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return false;
            }
            operation = RayPathOperation::create(toStyle(ray->ray(), state));
        } else if (RefPtr shape = dynamicDowncast<CSSBasicShapeValue>(singleValue))
            operation = ShapePathOperation::create(toStyle(shape->shape(), state, std::nullopt));
        else
            referenceBox = fromCSSValue<CSSBoxType>(singleValue);
        return true;
    };

    if (RefPtr list = dynamicDowncast<CSSValueList>(value)) {
        for (Ref currentValue : *list) {
            if (!processSingleValue(currentValue))
                return nullptr;
        }
    } else {
        if (!processSingleValue(value))
            return nullptr;
    }

    if (operation)
        operation->setReferenceBox(referenceBox);
    else {
        ASSERT(referenceBox != CSSBoxType::BoxMissing);
        operation = BoxPathOperation::create(referenceBox);
    }

    return operation;
}

Ref<CSSValue> CSSValueCreation<RayPath>::operator()(CSSValuePool&, const RenderStyle& style, const RayPath& path)
{
    return CSSRayValue::create(toCSS(path.ray(), style), path.referenceBox());
}

Ref<CSSValue> CSSValueCreation<BasicShapePath>::operator()(CSSValuePool& pool, const RenderStyle& style, const BasicShapePath& path, PathConversion conversion)
{
    if (path.referenceBox() == CSSBoxType::BoxMissing)
        return CSSValueList::createSpaceSeparated(createCSSValue(pool, style, path.shape(), conversion));
    return CSSValueList::createSpaceSeparated(createCSSValue(pool, style, path.shape(), conversion), createCSSValue(pool, style, path.referenceBox()));
}

// MARK: - Serialization

void Serialize<BasicShapePath>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const BasicShapePath& shape, PathConversion conversion)
{
    if (shape.referenceBox() == CSSBoxType::BoxMissing) {
        serializationForCSS(builder, context, style, shape.shape(), conversion);
        return;
    }

    serializationForCSS(builder, context, style, shape.shape(), conversion);
    builder.append(' ');
    serializationForCSS(builder, context, style, shape.referenceBox());
}

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream& ts, const RayPath& value)
{
    logForCSSOnVariantLike(ts, value);
    return ts;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const ReferencePath& value)
{
    logForCSSOnVariantLike(ts, value);
    return ts;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const BasicShapePath& value)
{
    logForCSSOnVariantLike(ts, value);
    return ts;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const BoxPath& value)
{
    logForCSSOnVariantLike(ts, value);
    return ts;
}

} // namespace Style
} // namespace WebCore
