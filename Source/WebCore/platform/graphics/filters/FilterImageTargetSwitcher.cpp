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
#include "FilterImageTargetSwitcher.h"

#include "Filter.h"
#include "FilterResults.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"

namespace WebCore {

FilterImageTargetSwitcher::FilterImageTargetSwitcher(GraphicsContext& destinationContext, Filter& filter, const FloatRect &sourceImageRect, const DestinationColorSpace& colorSpace, FilterResults* results)
    : FilterTargetSwitcher(filter)
    , m_sourceImageRect(sourceImageRect)
    , m_results(results)
{
    if (sourceImageRect.isEmpty())
        return;

    m_sourceImage = destinationContext.createScaledImageBuffer(m_sourceImageRect, filter.filterScale(), colorSpace, filter.renderingMode());
    if (!m_sourceImage) {
        m_filter = nullptr;
        return;
    }

    auto state = destinationContext.state();
    m_sourceImage->context().mergeAllChanges(state);
}

GraphicsContext* FilterImageTargetSwitcher::drawingContext(GraphicsContext& context) const
{
    return m_sourceImage ? &m_sourceImage->context() : &context;
}

void FilterImageTargetSwitcher::beginClipAndDrawSourceImage(GraphicsContext& destinationContext, const FloatRect& repaintRect)
{
    if (auto* context = drawingContext(destinationContext)) {
        context->save();
        context->clearRect(repaintRect);
        context->clip(repaintRect);
    }
}

void FilterImageTargetSwitcher::endClipAndDrawSourceImage(GraphicsContext& destinationContext)
{
    if (auto* context = drawingContext(destinationContext))
        context->restore();

    endDrawSourceImage(destinationContext);
}

void FilterImageTargetSwitcher::endDrawSourceImage(GraphicsContext& destinationContext)
{
    if (!m_filter)
        return;

    FilterResults results;
    destinationContext.drawFilteredImageBuffer(m_sourceImage.get(), m_sourceImageRect, *m_filter, m_results ? *m_results : results);
}

} // namespace WebCore
