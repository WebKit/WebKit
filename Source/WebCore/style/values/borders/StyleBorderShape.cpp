/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "StyleBorderShape.h"

#include "AnimationUtilities.h"
#include "CSSKeywordValue.h"
#include "CSSValueList.h"
#include "StyleBuilderChecking.h"
#include "StyleBuilderState.h"

namespace WebCore::Style {

// MARK: - Conversion

auto CSSValueConversion<BorderShape>::operator()(BuilderState& state, const CSSValue& value) -> BorderShape
{
    if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        if (keywordValue->valueID() != CSSValueNone)
            state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::None { };
    }

    // The value is an outer list of one or two groups, each group being a
    // `<basic-shape> <geometry-box>?` value (either a bare basic-shape value or a
    // space-separated list of the basic-shape and the geometry-box).
    auto list = requiredListDowncast<CSSValueList, CSSValue>(state, value);
    if (!list)
        return CSS::Keyword::None { };

    auto shapeOperationForGroup = [&](const CSSValue& group) -> RefPtr<ShapePathOperation> {
        RefPtr operation = toStyleFromCSSValue<RefPtr<PathOperation>>(state, group, SupportRayPathOperation::No);
        return dynamicDowncast<ShapePathOperation>(operation.get());
    };

    RefPtr<ShapePathOperation> outer;
    RefPtr<ShapePathOperation> inner;

    unsigned index = 0;
    for (Ref group : *list) {
        if (index >= 2) {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::None { };
        }

        RefPtr shapeOperation = shapeOperationForGroup(group);
        if (!shapeOperation) {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::None { };
        }

        if (!index)
            outer = WTF::move(shapeOperation);
        else
            inner = WTF::move(shapeOperation);
        ++index;
    }

    if (!outer) {
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::None { };
    }

    if (!inner)
        return BorderShape { BasicShapePath { outer.releaseNonNull() } };

    return BorderShape { BasicShapePath { outer.releaseNonNull() }, BasicShapePath { inner.releaseNonNull() } };
}

// MARK: - Blending

auto Blending<BorderShape>::canBlend(const BorderShape& a, const BorderShape& b) -> bool
{
    if (a.isNone() || b.isNone())
        return false;

    if (a.isStroke() && !b.isStroke())
        return false;

    if (a.isFill() && !b.isFill())
        return false;

    if (!a.outerOperation()->canBlend(*b.outerOperation()))
        return false;

    if (a.isFill()) {
        if (!a.innerOperation()->canBlend(*b.innerOperation()))
            return false;
    }

    return true;
}

auto Blending<BorderShape>::blend(const BorderShape& a, const BorderShape& b, const BlendingContext& context) -> BorderShape
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? b : a;
    }

    ASSERT(canBlend(a, b));

    auto blendShapes = [&](const RefPtr<PathOperation>& from, const RefPtr<PathOperation>& to) {
        return BasicShapePath { downcast<ShapePathOperation>(from->blend(to.get(), context).releaseNonNull()) };
    };

    if (a.isStroke())
        return BorderShape { blendShapes(a.outerOperation(), b.outerOperation()) };

    ASSERT(a.isFill() && b.isFill());

    return BorderShape {
        blendShapes(a.outerOperation(), b.outerOperation()),
        blendShapes(a.innerOperation(), b.innerOperation()),
    };
}

} // namespace WebCore::Style
