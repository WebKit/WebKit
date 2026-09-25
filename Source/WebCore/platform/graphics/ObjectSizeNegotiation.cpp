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

static NaturalDimensions adjustedNaturalDimensions(NaturalDimensions dimensions, const Inputs& inputs)
{
    if (!dimensions.hasUsableAspectRatio())
        dimensions.aspectRatio = std::nullopt;

    if (auto scale = inputs.density; scale != 1) {
        if (dimensions.width)
            dimensions.width = *dimensions.width * scale;
        if (dimensions.height)
            dimensions.height = *dimensions.height * scale;
    }

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
    if (specifiedSize.definiteWidth && specifiedSize.definiteHeight)
        return { *specifiedSize.definiteWidth, *specifiedSize.definiteHeight };

    if (specifiedSize.definiteWidth) {
        auto width = *specifiedSize.definiteWidth;

        if (naturalDimensions.aspectRatio)
            return { width, width * naturalDimensions.aspectRatio->height() / naturalDimensions.aspectRatio->width() };

        if (naturalDimensions.height)
            return { width, *naturalDimensions.height };

        return { width, defaultObjectSize.height() };
    }

    if (specifiedSize.definiteHeight) {
        auto height = *specifiedSize.definiteHeight;

        if (naturalDimensions.aspectRatio)
            return { height * naturalDimensions.aspectRatio->width() / naturalDimensions.aspectRatio->height(), height };

        if (naturalDimensions.width)
            return { *naturalDimensions.width, height };

        return { defaultObjectSize.width(), height };
    }

    if (naturalDimensions.width || naturalDimensions.height)
        return resolveSizingAlgorithm(naturalDimensions, { naturalDimensions.width, naturalDimensions.height }, defaultObjectSize);

    return resolveConstraint(naturalDimensions, defaultObjectSize, AspectRatioFitShrink);
}

ConcreteObjectSize defaultSizingAlgorithm(NaturalDimensions naturalDimensions, SpecifiedSize specifiedSize, const Inputs& inputs)
{
    return { resolveSizingAlgorithm(adjustedNaturalDimensions(naturalDimensions, inputs), specifiedSize, inputs.defaultObjectSize), 1 };
}

ConcreteObjectSize resolveContainConstraint(NaturalDimensions naturalDimensions, FloatSize constraintRectangle, const Inputs& inputs)
{
    return { resolveConstraint(adjustedNaturalDimensions(naturalDimensions, inputs), constraintRectangle, AspectRatioFitShrink), 1 };
}

ConcreteObjectSize resolveCoverConstraint(NaturalDimensions naturalDimensions, FloatSize constraintRectangle, const Inputs& inputs)
{
    return { resolveConstraint(adjustedNaturalDimensions(naturalDimensions, inputs), constraintRectangle, AspectRatioFitGrow), 1 };
}

} // namespace WebCore::ObjectSizeNegotiation
