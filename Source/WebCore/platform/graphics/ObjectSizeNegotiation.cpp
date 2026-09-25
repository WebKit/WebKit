/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ObjectSizeNegotiation.h"

namespace WebCore::ObjectSizeNegotiation {

static NaturalDimensions usableNaturalDimensions(NaturalDimensions dimensions)
{
    if (!dimensions.hasUsableAspectRatio())
        dimensions.aspectRatio = std::nullopt;
    return dimensions;
}

static FloatSize resolveConstraint(const NaturalDimensions& naturalDimensions, FloatSize constraintRectangle, AspectRatioFit fit)
{
    if (!naturalDimensions.aspectRatio)
        return constraintRectangle;
    return constraintRectangle.fitToAspectRatio(*naturalDimensions.aspectRatio, fit);
}

static FloatSize resolveSizingAlgorithm(const NaturalDimensions& naturalDimensions, SpecifiedSize specifiedSize, FloatSize defaultObjectSize)
{
    // If the specified size is a definite width and height, the concrete object size is
    // given that width and height.
    if (specifiedSize.definiteWidth && specifiedSize.definiteHeight)
        return { *specifiedSize.definiteWidth, *specifiedSize.definiteHeight };

    // If the specified size is only a width or height (but not both) then the concrete
    // object size is given that specified width or height. The other dimension is
    // calculated as follows:
    if (specifiedSize.definiteWidth) {
        auto width = *specifiedSize.definiteWidth;

        // If the object has a natural aspect ratio, the missing dimension of the concrete
        // object size is calculated using that aspect ratio and the present dimension.
        if (naturalDimensions.aspectRatio)
            return { width, width * naturalDimensions.aspectRatio->height() / naturalDimensions.aspectRatio->width() };

        // Otherwise, if the missing dimension is present in the object's natural
        // dimensions, the missing dimension is taken from the object's natural dimensions.
        if (naturalDimensions.height)
            return { width, *naturalDimensions.height };

        // Otherwise, the missing dimension of the concrete object size is taken from the
        // default object size.
        return { width, defaultObjectSize.height() };
    }

    if (specifiedSize.definiteHeight) {
        auto height = *specifiedSize.definiteHeight;

        // If the object has a natural aspect ratio, the missing dimension of the concrete
        // object size is calculated using that aspect ratio and the present dimension.
        if (naturalDimensions.aspectRatio)
            return { height * naturalDimensions.aspectRatio->width() / naturalDimensions.aspectRatio->height(), height };

        // Otherwise, if the missing dimension is present in the object's natural
        // dimensions, the missing dimension is taken from the object's natural dimensions.
        if (naturalDimensions.width)
            return { *naturalDimensions.width, height };

        // Otherwise, the missing dimension of the concrete object size is taken from the
        // default object size.
        return { defaultObjectSize.width(), height };
    }

    // If the specified size has no constraints:

    // If the object has a natural height or width, its size is resolved as if its natural
    // dimensions were given as the specified size.
    if (naturalDimensions.width || naturalDimensions.height)
        return resolveSizingAlgorithm(naturalDimensions, { naturalDimensions.width, naturalDimensions.height }, defaultObjectSize);

    // Otherwise, its size is resolved as a contain constraint against the default object size.
    return resolveConstraint(naturalDimensions, defaultObjectSize, AspectRatioFit::Shrink);
}

ConcreteObjectSize defaultSizingAlgorithm(NaturalDimensions naturalDimensions, SpecifiedSize specifiedSize, FloatSize defaultObjectSize)
{
    // https://drafts.csswg.org/css-images-3/#default-sizing-algorithm

    return ConcreteObjectSize::fixed(resolveSizingAlgorithm(usableNaturalDimensions(naturalDimensions), specifiedSize, defaultObjectSize));
}

ConcreteObjectSize resolveContainConstraint(NaturalDimensions naturalDimensions, FloatSize constraintRectangle)
{
    // https://drafts.csswg.org/css-images-3/#contain-constraint

    return ConcreteObjectSize::fixed(resolveConstraint(usableNaturalDimensions(naturalDimensions), constraintRectangle, AspectRatioFit::Shrink));
}

ConcreteObjectSize resolveCoverConstraint(NaturalDimensions naturalDimensions, FloatSize constraintRectangle)
{
    // https://drafts.csswg.org/css-images-3/#cover-constraint

    return ConcreteObjectSize::fixed(resolveConstraint(usableNaturalDimensions(naturalDimensions), constraintRectangle, AspectRatioFit::Grow));
}

} // namespace WebCore::ObjectSizeNegotiation
