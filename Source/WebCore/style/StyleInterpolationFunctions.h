/*
 * Copyright (C) 2007-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2012, 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2025 Sam Weinig. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSPropertyNames.h"
#include "CachedImage.h"
#include "CalculationValue.h"
#include "ColorBlending.h"
#include "Document.h"
#include "FloatConversion.h"
#include "FontCascade.h"
#include "FontSelectionAlgorithm.h"
#include "FontSelectionValueInlines.h"
#include "FontTaggedSettings.h"
#include "IdentityTransformOperation.h"
#include "Logging.h"
#include "Matrix3DTransformOperation.h"
#include "MatrixTransformOperation.h"
#include "PathOperation.h"
#include "RenderBox.h"
#include "RenderStyleSetters.h"
#include "ScopedName.h"
#include "Settings.h"
#include "StyleDynamicRangeLimit.h"
#include "StyleImageWrapper.h"
#include "StyleInterpolationClient.h"
#include "StyleInterpolationContext.h"
#include "StyleLengthWrapper+Blending.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StyleResolver.h"
#include <algorithm>
#include <wtf/MathExtras.h>
#include <wtf/PointerComparison.h>

namespace WebCore::Style::Interpolation {

inline int blendFunc(int from, int to, const Context& context)
{
    return WebCore::blend(from, to, context);
}

inline double blendFunc(double from, double to, const Context& context)
{
    return WebCore::blend(from, to, context);
}

inline float blendFunc(float from, float to, const Context& context)
{
    if (context.iterationCompositeOperation == IterationCompositeOperation::Accumulate && context.currentIteration) {
        auto iterationIncrement = context.currentIteration * to;
        from += iterationIncrement;
        to += iterationIncrement;
    }

    if (context.compositeOperation == CompositeOperation::Replace)
        return narrowPrecisionToFloat(from + (to - from) * context.progress);
    return narrowPrecisionToFloat(from + from + (to - from) * context.progress);
}

inline WebCore::Color blendFunc(const WebCore::Color& from, const WebCore::Color& to, const Context& context)
{
    return WebCore::blend(from, to, context);
}

inline ContentVisibility blendFunc(ContentVisibility from, ContentVisibility to, const Context& context)
{
    // https://drafts.csswg.org/css-contain-3/#content-visibility-animation
    // In general, the content-visibility property's animation type is discrete. However, similar to interpolation of
    // visibility, during interpolation between hidden and any other content-visibility value, p values between 0 and 1
    // map to the non-hidden value.
    if (from != ContentVisibility::Hidden && to != ContentVisibility::Hidden)
        return context.progress < 0.5 ? from : to;
    if (context.progress <= 0)
        return from;
    if (context.progress >= 1)
        return to;
    return from == ContentVisibility::Hidden ? to : from;
}

inline Visibility blendFunc(Visibility from, Visibility to, const Context& context)
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? to : from;
    }

    // Any non-zero result means we consider the object to be visible. Only at 0 do we consider the object to be
    // invisible. The invisible value we use (Visibility::Hidden vs. Visibility::Collapse) depends on the specified from/to values.
    double fromVal = from == Visibility::Visible ? 1. : 0.;
    double toVal = to == Visibility::Visible ? 1. : 0.;
    if (fromVal == toVal)
        return to;
    // The composite operation here is irrelevant.
    double result = blendFunc(fromVal, toVal, { context.property, context.progress, false, CompositeOperation::Replace, IterationCompositeOperation::Replace, 0, { }, { }, context.client });
    return result > 0. ? Visibility::Visible : (to != Visibility::Visible ? to : from);
}

inline DisplayType blendFunc(DisplayType from, DisplayType to, const Context& context)
{
    // https://drafts.csswg.org/css-display-4/#display-animation
    // In general, the display property's animation type is discrete. However, similar to interpolation of
    // visibility, during interpolation between none and any other display value, p values between 0 and 1
    // map to the non-none value. Additionally, the element is inert as long as its display value would
    // compute to none when ignoring the Transitions and Animations cascade origins.
    if (from != DisplayType::None && to != DisplayType::None)
        return context.progress < 0.5 ? from : to;
    if (context.progress <= 0)
        return from;
    if (context.progress >= 1)
        return to;
    return from == DisplayType::None ? to : from;
}

} // namespace WebCore::Style::Interpolation
