/*
 * Copyright (C) 2024 Apple Inc.  All rights reserved.
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
#include "CanvasFilterTargetSwitcher.h"

#include "CanvasRenderingContext2DBase.h"
#include "Filter.h"
#include "FilterTargetSwitcher.h"

namespace WebCore {

std::unique_ptr<CanvasFilterTargetSwitcher> CanvasFilterTargetSwitcher::create(CanvasRenderingContext2DBase& context, const DestinationColorSpace& colorSpace, Function<FloatRect()>&& boundsProvider)
{
    FloatRect bounds;
    auto filter = context.createFilter([&]() {
        bounds = boundsProvider();
        return bounds;
    });

    if (!filter)
        return nullptr;

    // FIXME: Disable GraphicsContext filters for now. The CG coordinates need to be flipped before applying the style.
    filter->setFilterRenderingModes(filter->filterRenderingModes() - FilterRenderingMode::GraphicsContext);

    ASSERT(!bounds.isEmpty());
    auto* destinationContext = context.drawingContext();

    auto filterTargetSwitcher = FilterTargetSwitcher::create(*destinationContext, *filter, bounds, colorSpace);
    if (!filterTargetSwitcher)
        return nullptr;

    filterTargetSwitcher->beginDrawSourceImage(*destinationContext);
    context.setFilterTargetSwitcher(filterTargetSwitcher.get());

    return makeUnique<CanvasFilterTargetSwitcher>(context, bounds, WTFMove(filterTargetSwitcher));
}

std::unique_ptr<CanvasFilterTargetSwitcher> CanvasFilterTargetSwitcher::create(CanvasRenderingContext2DBase& context, const DestinationColorSpace& colorSpace, const FloatRect& bounds)
{
    return create(context, colorSpace, [&]() {
        return bounds;
    });
}

CanvasFilterTargetSwitcher::CanvasFilterTargetSwitcher(CanvasRenderingContext2DBase& context, const FloatRect& bounds, std::unique_ptr<FilterTargetSwitcher>&& filterTargetSwitcher)
    : m_context(context)
    , m_bounds(bounds)
    , m_filterTargetSwitcher(WTFMove(filterTargetSwitcher))
{
    m_context.setFilterTargetSwitcher(m_filterTargetSwitcher.get());
}

CanvasFilterTargetSwitcher::~CanvasFilterTargetSwitcher()
{
    m_context.setFilterTargetSwitcher(nullptr);

    auto* destinationContext = m_context.drawingContext();
    m_filterTargetSwitcher->endDrawSourceImage(*destinationContext, DestinationColorSpace::SRGB());
}

FloatBoxExtent CanvasFilterTargetSwitcher::outsets() const
{
    return m_context.calculateFilterOutsets(m_bounds);
}

} // namespace WebCore
