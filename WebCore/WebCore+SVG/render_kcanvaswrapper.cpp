/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "render_kcanvaswrapper.h"

#include "kcanvas/KCanvas.h"
#include "kcanvas/KCanvasContainer.h"
#include "kcanvas/device/KRenderingDevice.h"

using namespace khtml;

RenderKCanvasWrapper::RenderKCanvasWrapper(DOM::KDOMNodeTreeWrapperImpl *node) : RenderReplaced(node), m_canvas(0)
{
}

RenderKCanvasWrapper::~RenderKCanvasWrapper()
{
}

void RenderKCanvasWrapper::layout()
{
    QSize canvasSize = m_canvas->canvasSize();
    
    // this should go somewhere in ksvg2
    if ((canvasSize == QSize(-1,-1)) && m_canvas->rootContainer()) {
        QRect canvasBounds = m_canvas->rootContainer()->bbox();
        canvasSize = QSize(abs(canvasBounds.x()) + canvasBounds.width(), abs(canvasBounds.y()) + canvasBounds.height());
        m_canvas->setCanvasSize(canvasSize);
    }

    setWidth(canvasSize.width());
    setHeight(canvasSize.height());
    
    setNeedsLayout(false);
}

void RenderKCanvasWrapper::paint(PaintInfo &paintInfo, int parentX, int parentY)
{
    if (paintInfo.p->paintingDisabled())
        return;
    if (paintInfo.phase == PaintActionBlockBackground) {
        paintRootBoxDecorations(paintInfo, parentX, parentY);
        return;
    }
    if (!RenderReplaced::shouldPaint(paintInfo, parentX, parentY) || (paintInfo.phase != PaintActionForeground))
        return;
    
    if (!m_canvas || !m_canvas->rootContainer() || !m_canvas->renderingDevice())
        return;
    
    KRenderingDevice *renderingDevice = m_canvas->renderingDevice();
    renderingDevice->pushContext(paintInfo.p->renderingDeviceContext());
    paintInfo.p->save();

    QRect dirtyRect = paintInfo.r; //QRect([drawView mapViewRectToCanvas:dirtyViewRect]);
    m_canvas->rootContainer()->draw(dirtyRect);
    
    // restore drawing state
    paintInfo.p->restore();
    renderingDevice->popContext();
}

int RenderKCanvasWrapper::intrinsicWidth() const
{
    return 150;
}

int RenderKCanvasWrapper::intrinsicHeight() const
{
    return 150;
}
