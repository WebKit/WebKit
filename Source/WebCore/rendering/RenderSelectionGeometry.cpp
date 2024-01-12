/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderSelectionGeometry.h"

#include "RenderText.h"

namespace WebCore {

RenderSelectionGeometryBase::RenderSelectionGeometryBase(RenderObject& renderer)
    : m_renderer(renderer)
    , m_repaintContainer(renderer.containerForRepaint().renderer.get())
    , m_state(renderer.selectionState())
{
}

void RenderSelectionGeometryBase::repaintRectangle(const LayoutRect& repaintRect)
{
    m_renderer.repaintUsingContainer(m_repaintContainer, enclosingIntRect(repaintRect));
}

RenderSelectionGeometry::RenderSelectionGeometry(RenderObject& renderer, bool clipToVisibleContent)
    : RenderSelectionGeometryBase(renderer)
{
    if (renderer.canUpdateSelectionOnRootLineBoxes()) {
        if (CheckedPtr textRenderer = dynamicDowncast<RenderText>(renderer))
            m_rect = textRenderer->collectSelectionGeometriesForLineBoxes(m_repaintContainer, clipToVisibleContent, m_collectedSelectionQuads);
        else
            m_rect = renderer.selectionRectForRepaint(m_repaintContainer, clipToVisibleContent);
    }
}

void RenderSelectionGeometry::repaint()
{
    repaintRectangle(m_rect);
}

RenderBlockSelectionGeometry::RenderBlockSelectionGeometry(RenderBlock& renderer)
    : RenderSelectionGeometryBase(renderer)
    , m_rects(renderer.canUpdateSelectionOnRootLineBoxes() ? renderer.selectionGapRectsForRepaint(m_repaintContainer) : GapRects())
{
}

void RenderBlockSelectionGeometry::repaint()
{
    repaintRectangle(m_rects);
}

} // namespace WebCore
