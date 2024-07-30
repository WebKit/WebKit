/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

// https://drafts.csswg.org/css-images-3/#default-sizing
LayoutSize defaultSizingAlgorithm(NaturalDimensions naturalDimensions, SpecifiedSize specifiedSize, LayoutSize defaultObjectSize)
{
    // If the specified size is a definite width and height, the concrete object
    // size is given that width and height.
    if (specifiedSize.definiteWidth && specifiedSize.definiteHeight)
        return { *specifiedSize.definiteWidth, *specifiedSize.definiteHeight };

    // If the specified size is only a width or height (but not both) then the
    // concrete object size is given that specified width or height. The other
    // dimension is calculated as follows:
    if (specifiedSize.definiteWidth) {
        auto width = *specifiedSize.definiteWidth;

        // If the object has a natural aspect ratio, the missing dimension of
        // the concrete object size is calculated using that aspect ratio and
        // the present dimension.
        if (naturalDimensions.aspectRatio) {
            auto height = width / naturalDimensions.aspectRatio->width() * naturalDimensions.aspectRatio->height();
            return { width, height };
        }

        // Otherwise, if the missing dimension is present in the object’s
        // natural dimensions, the missing dimension is taken from the object’s
        // natural dimensions.
        if (naturalDimensions.height)
            return { width, *naturalDimensions.height };

        // Otherwise, the missing dimension of the concrete object size is
        // taken from the default object size.
        return { width, defaultObjectSize.height() };
    }
    if (specifiedSize.definiteHeight) {
        auto height = *specifiedSize.definiteHeight;

        // If the object has a natural aspect ratio, the missing dimension of
        // the concrete object size is calculated using that aspect ratio and
        // the present dimension.
        if (naturalDimensions.aspectRatio) {
            auto width = height / naturalDimensions.aspectRatio->height() * naturalDimensions.aspectRatio->width();
            return { width, height };
        }

        // Otherwise, if the missing dimension is present in the object’s
        // natural dimensions, the missing dimension is taken from the object’s
        // natural dimensions.
        if (naturalDimensions.width)
            return { *naturalDimensions.width, height };

        // Otherwise, the missing dimension of the concrete object size is
        // taken from the default object size.
        return { defaultObjectSize.width(), height };
    }

    // If the specified size has no constraints:

    // If the object has a natural height or width, its size is resolved
    // as if its natural dimensions were given as the specified size.
    if (naturalDimensions.width || naturalDimensions.height)
        return defaultSizingAlgorithm(naturalDimensions, { naturalDimensions.width, naturalDimensions.height }, defaultObjectSize);

    // Otherwise, its size is resolved as a contain constraint against
    // the default object size.
    return resolveContainConstraint(naturalDimensions, defaultObjectSize);
}

// https://drafts.csswg.org/css-images-3/#contain-constraint
LayoutSize resolveContainConstraint(NaturalDimensions naturalDimensions, LayoutSize constraintRectangle)
{
    if (!naturalDimensions.aspectRatio) {
        // In both cases, if the object doesn’t have a natural aspect ratio,
        // the concrete object size is the specified constraint rectangle.
        return constraintRectangle;
    }

    // A contain constraint is resolved by setting the concrete object size
    // to the largest rectangle that has the object’s natural aspect ratio
    // and additionally has neither width nor height larger than the constraint
    // rectangle’s width and height, respectively.
    return constraintRectangle.fitToAspectRatio(*naturalDimensions.aspectRatio, AspectRatioFitShrink);
}

// https://drafts.csswg.org/css-images-3/#cover-constraint
LayoutSize resolveCoverConstraint(NaturalDimensions naturalDimensions, LayoutSize constraintRectangle)
{
    if (!naturalDimensions.aspectRatio) {
        // In both cases, if the object doesn’t have a natural aspect ratio,
        // the concrete object size is the specified constraint rectangle.
        return constraintRectangle;
    }

    // A cover constraint is resolved by setting the concrete object size to
    // the smallest rectangle that has the object’s natural aspect ratio and
    // additionally has neither width nor height smaller than the constraint
    // rectangle’s width and height, respectively.
    return constraintRectangle.fitToAspectRatio(*naturalDimensions.aspectRatio, AspectRatioFitGrow);
}

} // namespace WebCore::ObjectSizeNegotiation
