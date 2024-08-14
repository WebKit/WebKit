/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "PaintRenderingContext2D.h"

#include "CustomPaintCanvas.h"
#include "DisplayListDrawingContext.h"
#include "DisplayListRecorder.h"
#include "DisplayListReplayer.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(PaintRenderingContext2D);

std::unique_ptr<PaintRenderingContext2D> PaintRenderingContext2D::create(CustomPaintCanvas& canvas)
{
    return std::unique_ptr<PaintRenderingContext2D>(new PaintRenderingContext2D(canvas));
}

PaintRenderingContext2D::PaintRenderingContext2D(CustomPaintCanvas& canvas)
    : CanvasRenderingContext2DBase(canvas, { }, false)
{
}

PaintRenderingContext2D::~PaintRenderingContext2D() = default;

CustomPaintCanvas& PaintRenderingContext2D::canvas() const
{
    return downcast<CustomPaintCanvas>(canvasBase());
}

GraphicsContext* PaintRenderingContext2D::drawingContext() const
{
    if (!m_recordingContext)
        m_recordingContext = makeUnique<DisplayList::DrawingContext>(canvasBase().size());
    return &m_recordingContext->context();
}

GraphicsContext* PaintRenderingContext2D::existingDrawingContext() const
{
    return m_recordingContext ? &m_recordingContext->context() : nullptr;
}

AffineTransform PaintRenderingContext2D::baseTransform() const
{
    // The base transform of the display list.
    // FIXME: this is actually correct, but the display list will not behave correctly with respect to
    // playback. The GraphicsContext should be fixed to start at identity transform, and the
    // device transform should be a separate concept that the display list or context2d cannot reset.
    return { };
}

void PaintRenderingContext2D::replayDisplayList(GraphicsContext& target) const
{
    if (!m_recordingContext)
        return;
    auto& displayList = m_recordingContext->displayList();
    if (displayList.isEmpty())
        return;
    DisplayList::Replayer replayer(target, displayList);
    replayer.replay(FloatRect { { }, canvasBase().size() });
    displayList.clear();
}

} // namespace WebCore
