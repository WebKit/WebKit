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


#include "config.h"
#import "KCanvasViewQuartz.h"
#import "KCanvasMatrix.h"

#import "render_kcanvaswrapper.h"
#import "DrawViewPrivate.h"

KCanvasViewQuartz::KCanvasViewQuartz() : m_view(0), m_renderObject(0)
{     
}

KCanvasViewQuartz::~KCanvasViewQuartz()
{
    [m_view release];
}

DrawView *KCanvasViewQuartz::view()
{
    return m_view;
}

void KCanvasViewQuartz::setView(DrawView *view)
{
    id oldView = m_view;
    m_view = [view retain];
    [oldView release];
}

khtml::RenderKCanvasWrapper *KCanvasViewQuartz::renderObject()
{
    return m_renderObject;
}

void KCanvasViewQuartz::setRenderObject(khtml::RenderKCanvasWrapper *renderObject)
{
    m_renderObject = renderObject;
}

void KCanvasViewQuartz::invalidateCanvasRect(const QRect &dirtyRect) const
{
    if (m_renderObject)
        m_renderObject->repaint();
    else {
        // transform logic should be in this class.
        if (dirtyRect.isValid()) {
            NSRect dirtyCanvasRect = NSRect(dirtyRect);
            dirtyCanvasRect.size.width += 1.0f; // FIXME: rounding? qrect error?
            dirtyCanvasRect.size.height += 1.0f;
            NSRect viewDirtyRect = [m_view mapCanvasRectToView:dirtyCanvasRect];
            viewDirtyRect.size.width += 1.0f; // FIXME: rounding? qrect error?
            viewDirtyRect.size.height += 1.0f;
            [m_view setNeedsDisplayInRect:viewDirtyRect];
        } else
            [m_view setNeedsDisplay:YES];
    }
}

KCanvasMatrix KCanvasViewQuartz::viewToCanvasMatrix() const
{
    return KCanvasMatrix([m_view transformFromViewToCanvas]);
}

void KCanvasViewQuartz::canvasSizeChanged(int width, int height) {
    //NSLog(@"canvas changed size: %f, %f", width, height);
}

int KCanvasViewQuartz::viewHeight() const
{
    NSLog(@"viewHeight not sure yet.");
    return 100;
}

int KCanvasViewQuartz::viewWidth() const
{
    NSLog(@"viewWidth not sure yet.");
    return 100;
}

