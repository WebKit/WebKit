/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "TransformList.h"

#include "AnimationUtilities.h"
#include <algorithm>
#include <ranges>
#include <wtf/text/TextStream.h>

namespace WebCore {

void TransformList::apply(TransformationMatrix& matrix, unsigned start) const
{
    for (const auto& operation : list) {
        if (start != 0) {
            --start;
            continue;
        }
        WTF::switchOn(operation, [&](const auto& op) { return op.applyTransform(matrix); });
    }
}

bool TransformList::isRepresentableIn2D() const
{
    return std::ranges::all_of(list, [](auto& op) {
        return WTF::switchOn(op, [](const auto& op) { return op.isRepresentableIn2D(); });
    });
}

// MARK: - Blending

enum class UseDiscrete : bool { No, Yes };

static bool isInvertible(const TransformList& list)
{
    TransformationMatrix transform;
    list.apply(transform);
    return transform.isInvertible();
}

static bool containsNonInvertibleMatrix(const TransformList& list)
{
    return list.hasTransformOfType<Matrix, Matrix3D>() && !isInvertible(list);
}

static bool shouldFallBackToDiscreteAnimation(const TransformList& from, const TransformList& to)
{
    return containsNonInvertibleMatrix(from) || containsNonInvertibleMatrix(to);
}

static TransformFunction createSuffixMatrixBlend(const TransformList& from, const TransformList& to, const BlendingContext& context, unsigned start, UseDiscrete useDiscreteAnimation)
{
    TransformationMatrix fromTransform;
    from.apply(fromTransform, start);

    TransformationMatrix toTransform;
    to.apply(toTransform, start);

    if (useDiscreteAnimation == UseDiscrete::Yes)
        toTransform.blend(fromTransform, context.progress < 0.5 ? 0 : 1, CompositeOperation::Replace);
    else
        toTransform.blend(fromTransform, context.progress, context.compositeOperation);

    return Matrix3D { toTransform };
}

static bool shouldPerformSuffixMatrixBlend(const std::optional<TransformFunction>& from, const std::optional<TransformFunction>& to, unsigned start, std::optional<unsigned> prefixLength)
{
    return (prefixLength && start >= *prefixLength)
        || (from && to && !haveSharedPrimitiveType(*from, *to));
}

TransformList blend(const TransformList& from, const TransformList& to, const BlendingContext& context, std::optional<unsigned> prefixLength)
{
    if (shouldFallBackToDiscreteAnimation(from, to))
        return { { createSuffixMatrixBlend(from, to, context, 0, UseDiscrete::Yes) } };

    Vector<TransformFunction> blendedFunctions;

    auto fromCurrent = from.begin();
    auto fromEnd = from.end();

    auto toCurrent = to.begin();
    auto toEnd = to.end();

    // If either of the transform list is empty, then we should not attempt to do a matrix blend.
    bool mayAttemptSuffixMatrixBlend = !from.isEmpty() && !to.isEmpty();

    // The number of transform functions already blended.
    unsigned count = 0;

    while (fromCurrent != fromEnd || toCurrent != toEnd) {
        std::optional<TransformFunction> fromFunction = fromCurrent != fromEnd ? std::make_optional(*fromCurrent) : std::nullopt;
        std::optional<TransformFunction> toFunction = toCurrent != toEnd ? std::make_optional(*toCurrent) : std::nullopt;

        if (mayAttemptSuffixMatrixBlend && shouldPerformSuffixMatrixBlend(fromFunction, toFunction, count, prefixLength)) {
            blendedFunctions.append(createSuffixMatrixBlend(from, to, context, count, UseDiscrete::No));
            return { WTFMove(blendedFunctions) };
        }

        auto blendedFunction = [&] {
            if (fromFunction && toFunction)
                return blend(*fromFunction, *toFunction, context);
            if (toFunction)
                return blend(identityTransformFunction(*toFunction), *toFunction, context);
            return blend(*fromFunction, identityTransformFunction(*fromFunction), context);
        };
        blendedFunctions.append(blendedFunction());

        ++count;
        if (fromCurrent != fromEnd)
            fromCurrent = std::next(fromCurrent);
        if (toCurrent != toEnd)
            toCurrent = std::next(toCurrent);
    }

    return { WTFMove(blendedFunctions) };
}

} // namespace WebCore
