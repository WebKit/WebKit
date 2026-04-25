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
#include "DisplayListRecorderImpl.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PaintRenderingContext2D);

std::unique_ptr<PaintRenderingContext2D> PaintRenderingContext2D::create(CustomPaintCanvas& canvas)
{
    return std::unique_ptr<PaintRenderingContext2D>(new PaintRenderingContext2D(canvas));
}

// Note: currently there is a structural mismatch where PaintRenderingContext2D : public CanvasRenderingContext2DBase,
// but at the same time CanvasRenderingContext2DBase has the m_buffer backing store and PaintRenderingContext2D has
// the m_recordingContext backing store. PaintRenderingContext2D is not a shipping class, and
// thus we do not want to spend code to make an extra CanvasRenderingContext2DBitmapBase : public CanvasRenderingContext2DBase.
// CanvasRenderingContext2DBase has dynamicCasting related to drawing context accessors.
// CanvasRenderingContext2DBase is written in a way that assumes m_buffer is used.
// PaintRenderingContext2D is written in a way to compensate so that the assumption does not affect the correctness.

PaintRenderingContext2D::PaintRenderingContext2D(CustomPaintCanvas& canvas)
    : CanvasRenderingContext2DBase(canvas, Type::Paint, { }, false)
{
}

PaintRenderingContext2D::~PaintRenderingContext2D()
{
#if ASSERT_ENABLED
    // Call the user-induced restore()s. Restore through the restore() method
    // to let the CanvasRenderingContext2DBase::~CanvasRenderingContext2DBase()
    // code make sense.
    size_t restoreCount = stateStack().size() - 1;
    for (size_t i = 0; i < restoreCount; ++i)
        restore();
#endif
}

CustomPaintCanvas& PaintRenderingContext2D::canvas() const
{
    return downcast<CustomPaintCanvas>(canvasBase());
}

GraphicsContext* PaintRenderingContext2D::drawingContext() const
{
    if (!m_recordingContext)
        m_recordingContext.emplace(canvasBase().size());
    return &*m_recordingContext;
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
    target.drawDisplayList(m_recordingContext->takeDisplayList());
}

void PaintRenderingContext2D::didUpdateCanvasSizeProperties(bool sizeChanged)
{
    size_t restoreCount = stateStack().size() - 1;
    for (size_t i = 0; i < restoreCount; ++i)
        restore();
    m_recordingContext.reset();
    CanvasRenderingContext2DBase::didUpdateCanvasSizeProperties(sizeChanged);
}

} // namespace WebCore
