/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
#include "TransparencyLayerContextSwitcher.h"

#include "Filter.h"
#include "GraphicsContext.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TransparencyLayerContextSwitcher);

TransparencyLayerContextSwitcher::TransparencyLayerContextSwitcher(GraphicsContext& destinationContext, const FloatRect& sourceImageRect, RefPtr<Filter>&& filter)
    : GraphicsContextSwitcher(sourceImageRect, WTFMove(filter))
{
    if (m_filter)
        m_filterStyles = m_filter->createFilterStyles(destinationContext, sourceImageRect);
}

SwitcherState TransparencyLayerContextSwitcher::beginDrawSourceImage(GraphicsContext& destinationContext, std::optional<FloatRect> clipRect, float opacity)
{
    ASSERT(m_state != SwitcherState::PaintingSource);
    m_state = SwitcherState::PaintingSource;

    destinationContext.save();
    destinationContext.beginTransparencyLayer(opacity);

    for (auto& filterStyle : m_filterStyles) {
        destinationContext.save();
        destinationContext.clip(clipRect ? intersection(filterStyle.imageRect, *clipRect) : filterStyle.imageRect);
        destinationContext.setStyle(filterStyle.style);
        destinationContext.beginTransparencyLayer(1);
    }

    return m_state;
}

SwitcherState TransparencyLayerContextSwitcher::endDrawSourceImage(GraphicsContext& destinationContext)
{
    ASSERT(m_state == SwitcherState::PaintingSource);
    m_state = SwitcherState::None;

    for ([[maybe_unused]] auto& filterStyle : makeReversedRange(m_filterStyles)) {
        destinationContext.endTransparencyLayer();
        destinationContext.restore();
    }

    destinationContext.endTransparencyLayer();
    destinationContext.restore();

    return m_state;
}

void TransparencyLayerContextSwitcher::drawOutputImage(GraphicsContext&, const DestinationColorSpace&)
{
    ASSERT_NOT_REACHED();
}

} // namespace WebCore
