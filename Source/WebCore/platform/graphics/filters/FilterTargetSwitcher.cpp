/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FilterTargetSwitcher.h"

#include "Filter.h"
#include "FilterImageTargetSwitcher.h"
#include "GraphicsContext.h"

namespace WebCore {

std::unique_ptr<FilterTargetSwitcher> FilterTargetSwitcher::create(GraphicsContext& destinationContext, Filter& filter, const FloatRect &sourceImageRect, const DestinationColorSpace& colorSpace, FilterResults* results)
{
    return makeUnique<FilterImageTargetSwitcher>(destinationContext, filter, sourceImageRect, colorSpace, results);
}

FilterTargetSwitcher::FilterTargetSwitcher(Filter& filter)
    : m_filter(&filter)
{
}

void FilterTargetSwitcher::willDrawSourceImage(GraphicsContext& destinationContext, const FloatRect& repaintRect)
{
    if (auto* context = drawingContext(destinationContext)) {
        context->save();
        context->clearRect(repaintRect);
        context->clip(repaintRect);
    }
}

void FilterTargetSwitcher::didDrawSourceImage(GraphicsContext& destinationContext)
{
    if (auto* context = drawingContext(destinationContext))
        context->restore();
}

} // namespace WebCore
