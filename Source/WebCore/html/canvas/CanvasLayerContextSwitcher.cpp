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
#include "CanvasLayerContextSwitcher.h"

#include "CanvasRenderingContext2DBase.h"
#include "Filter.h"
#include "GraphicsContextSwitcher.h"

namespace WebCore {

RefPtr<CanvasLayerContextSwitcher> CanvasLayerContextSwitcher::create(CanvasRenderingContext2DBase& context, const FloatRect& bounds, RefPtr<Filter>&& filter)
{
    ASSERT(!bounds.isEmpty());
    auto* effectiveDrawingContext = context.effectiveDrawingContext();
    if (!effectiveDrawingContext)
        return nullptr;

    auto targetSwitcher = GraphicsContextSwitcher::create(*effectiveDrawingContext, bounds, context.colorSpace(), WTFMove(filter));
    if (!targetSwitcher)
        return nullptr;

    return adoptRef(*new CanvasLayerContextSwitcher(context, bounds, WTFMove(targetSwitcher)));
}

CanvasLayerContextSwitcher::CanvasLayerContextSwitcher(CanvasRenderingContext2DBase& context, const FloatRect& bounds, std::unique_ptr<GraphicsContextSwitcher>&& targetSwitcher)
    : m_context(context)
    , m_effectiveDrawingContext(m_context.effectiveDrawingContext())
    , m_bounds(bounds)
    , m_targetSwitcher(WTFMove(targetSwitcher))
{
    ASSERT(m_targetSwitcher);
    ASSERT(m_effectiveDrawingContext);
    m_targetSwitcher->beginDrawSourceImage(*m_effectiveDrawingContext, m_context.globalAlpha());
}

CanvasLayerContextSwitcher::~CanvasLayerContextSwitcher()
{
    m_targetSwitcher->endDrawSourceImage(*m_effectiveDrawingContext, DestinationColorSpace::SRGB());
}

GraphicsContext* CanvasLayerContextSwitcher::drawingContext() const
{
    if (!m_effectiveDrawingContext)
        return nullptr;
    return m_targetSwitcher->drawingContext(*m_effectiveDrawingContext);
}

FloatBoxExtent CanvasLayerContextSwitcher::outsets() const
{
    return m_context.calculateFilterOutsets(m_bounds);
}

} // namespace WebCore
