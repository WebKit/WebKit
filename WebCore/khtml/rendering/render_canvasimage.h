/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#ifndef RENDER_CANVASIMAGE_H
#define RENDER_CANVASIMAGE_H

#if __APPLE__

#include "html/html_elementimpl.h"
#include "rendering/render_image.h"
#include "dom/dom_string.h"

#include <qmap.h>
#include <qpixmap.h>

#include <ApplicationServices/ApplicationServices.h>

namespace khtml {

class DocLoader;

class RenderCanvasImage : public RenderImage
{
public:
    RenderCanvasImage(DOM::NodeImpl*);
    virtual ~RenderCanvasImage();

    virtual const char *renderName() const { return "RenderCanvasImage"; }
    
    virtual void paint(PaintInfo& i, int tx, int ty);

    virtual void layout();

    void setNeedsImageUpdate();
    
    // don't even think about making this method virtual!
    DOM::HTMLElementImpl* element() const
    { return static_cast<DOM::HTMLElementImpl*>(RenderObject::element()); }
    
    void updateDrawnImage();
    CGContextRef drawingContext();
    
private:
    void createDrawingContext();
    CGImageRef drawnImage();

    CGContextRef _drawingContext;
    void *_drawingContextData;
    CGImageRef _drawnImage;
    
    unsigned _needsImageUpdate:1;
};


}; //namespace

#endif

#endif
