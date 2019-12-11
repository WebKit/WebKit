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
#include "RenderSelectionInfo.h"

#include "RenderText.h"

namespace WebCore {

RenderSelectionInfoBase::RenderSelectionInfoBase(RenderObject& renderer)
    : m_renderer(renderer)
    , m_repaintContainer(renderer.containerForRepaint())
    , m_state(renderer.selectionState())
{
}

void RenderSelectionInfoBase::repaintRectangle(const LayoutRect& repaintRect)
{
    m_renderer.repaintUsingContainer(m_repaintContainer, enclosingIntRect(repaintRect));
}

RenderSelectionInfo::RenderSelectionInfo(RenderObject& renderer, bool clipToVisibleContent)
    : RenderSelectionInfoBase(renderer)
{
    if (renderer.canUpdateSelectionOnRootLineBoxes()) {
        if (is<RenderText>(renderer))
            m_rect = downcast<RenderText>(renderer).collectSelectionRectsForLineBoxes(m_repaintContainer, clipToVisibleContent, m_collectedSelectionRects);
        else
            m_rect = renderer.selectionRectForRepaint(m_repaintContainer, clipToVisibleContent);
    }
}

void RenderSelectionInfo::repaint()
{
    repaintRectangle(m_rect);
}

RenderBlockSelectionInfo::RenderBlockSelectionInfo(RenderBlock& renderer)
    : RenderSelectionInfoBase(renderer)
    , m_rects(renderer.canUpdateSelectionOnRootLineBoxes() ? renderer.selectionGapRectsForRepaint(m_repaintContainer) : GapRects())
{
}

void RenderBlockSelectionInfo::repaint()
{
    repaintRectangle(m_rects);
}

} // namespace WebCore
