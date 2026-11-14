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

#pragma once

#include <WebCore/StylePathOperationWrappers.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore::Style {

// <'border-shape'> = none | [ <basic-shape> <geometry-box>? ]{1,2}
//
// A single <basic-shape> selects "stroke mode": the border is stroked along the
// shape's path. Two <basic-shape>s select "fill mode": the border is the area
// between the outer (first) and inner (second) paths.
//
// https://drafts.csswg.org/css-borders-4/#border-shape-func
struct BorderShape {
    BorderShape(CSS::Keyword::None) { }

    explicit BorderShape(BasicShapePath&& outer)
        : outerPathOperation { outer.operation }
    {
    }

    BorderShape(BasicShapePath&& outer, BasicShapePath&& inner)
        : outerPathOperation { outer.operation }
        , innerPathOperation { inner.operation }
    {
    }

    bool isNone() const { return !outerPathOperation; }
    bool isStroke() const { return outerPathOperation && !innerPathOperation; }
    bool isFill() const { return outerPathOperation && innerPathOperation; }

    // The outer (or only) shape. Null when `none`.
    RefPtr<PathOperation> outerOperation() const { return outerPathOperation; }
    // The inner shape. Null unless in fill mode.
    RefPtr<PathOperation> innerOperation() const { return innerPathOperation; }

    template<typename... F> decltype(auto) switchOn(F&&...) const;

    bool operator==(const BorderShape& other) const
    {
        return arePointingToEqualData(outerPathOperation, other.outerPathOperation)
            && arePointingToEqualData(innerPathOperation, other.innerPathOperation);
    }

private:
    friend struct Blending<BorderShape>;

    RefPtr<PathOperation> outerPathOperation;
    RefPtr<PathOperation> innerPathOperation;
};

template<typename... F> decltype(auto) BorderShape::switchOn(F&&... f) const
{
    auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

    if (!outerPathOperation)
        return visitor(CSS::Keyword::None { });

    if (!innerPathOperation)
        return visitor(BasicShapePath { downcast<ShapePathOperation>(*outerPathOperation) });

    return visitor(SpaceSeparatedTuple {
        BasicShapePath { downcast<ShapePathOperation>(*outerPathOperation) },
        BasicShapePath { downcast<ShapePathOperation>(*innerPathOperation) }
    });
}

// MARK: - Conversion

template<> struct CSSValueConversion<BorderShape> { auto operator()(BuilderState&, const CSSValue&) -> BorderShape; };

// MARK: - Blending

template<> struct Blending<BorderShape> {
    auto canBlend(const BorderShape&, const BorderShape&) -> bool;
    auto blend(const BorderShape&, const BorderShape&, const BlendingContext&) -> BorderShape;
};

} // namespace WebCore::Style

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::BorderShape)
