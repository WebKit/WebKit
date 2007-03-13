/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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
#include "HTMLCanvasElement.h"

#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasRenderingContext2D.h"
#include "CanvasStyle.h"
#include "Chrome.h"
#include "Document.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "Page.h"
#include "RenderHTMLCanvas.h"
#include "Settings.h"
#include <math.h>

namespace WebCore {

using namespace HTMLNames;

// These values come from the WhatWG spec.
const int defaultWidth = 300;
const int defaultHeight = 150;

// Firefox limits width/height to 32767 pixels, but slows down dramatically before it 
// reaches that limit we limit by area instead, giving us larger max dimensions, in exchange 
// for reduce maximum canvas size.
const float maxCanvasArea = 32768 * 8192; // Maximum canvas area in CSS pixels

HTMLCanvasElement::HTMLCanvasElement(Document* doc)
    : HTMLElement(canvasTag, doc)
    , m_size(defaultWidth, defaultHeight)
    , m_createdDrawingContext(false)
    , m_data(0)
    , m_drawingContext(0)
{
}

HTMLCanvasElement::~HTMLCanvasElement()
{
    if (m_2DContext)
        m_2DContext->detachCanvas();
    fastFree(m_data);
    delete m_drawingContext;
}

HTMLTagStatus HTMLCanvasElement::endTagRequirement() const 
{ 
    if (document()->frame() && document()->frame()->settings()->usesDashboardBackwardCompatibilityMode())
        return TagStatusForbidden; 

    return HTMLElement::endTagRequirement();
}

int HTMLCanvasElement::tagPriority() const 
{ 
    if (document()->frame() && document()->frame()->settings()->usesDashboardBackwardCompatibilityMode())
        return 0; 

    return HTMLElement::tagPriority();
}

void HTMLCanvasElement::parseMappedAttribute(MappedAttribute* attr)
{
    const QualifiedName& attrName = attr->name();
    if (attrName == widthAttr || attrName == heightAttr)
        reset();
    HTMLElement::parseMappedAttribute(attr);
}

RenderObject* HTMLCanvasElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    if (document()->frame() && document()->frame()->settings()->isJavaScriptEnabled()) {
        m_rendererIsCanvas = true;
        RenderHTMLCanvas* r = new (arena) RenderHTMLCanvas(this);
        r->setIntrinsicWidth(width());
        r->setIntrinsicHeight(height());
        return r;
    }

    m_rendererIsCanvas = false;
    return HTMLElement::createRenderer(arena, style);
}

void HTMLCanvasElement::setHeight(int value)
{
    setAttribute(heightAttr, String::number(value));
}

void HTMLCanvasElement::setWidth(int value)
{
    setAttribute(widthAttr, String::number(value));
}

CanvasRenderingContext* HTMLCanvasElement::getContext(const String& type)
{
    if (type == "2d") {
        if (!m_2DContext)
            m_2DContext = new CanvasRenderingContext2D(this);
        return m_2DContext.get();
    }
    return 0;
}

void HTMLCanvasElement::willDraw(const FloatRect&)
{
    // FIXME: Change to repaint just the dirty rect for speed.
    // Until we start doing this, we won't know if the rects passed in are
    // accurate. Also don't forget to take into account the transform
    // on the context when determining what needs to be repainted.
    if (renderer())
        renderer()->repaint();
}

void HTMLCanvasElement::reset()
{
    bool ok;
    int w = getAttribute(widthAttr).toInt(&ok);
    if (!ok)
        w = defaultWidth;
    int h = getAttribute(heightAttr).toInt(&ok);
    if (!ok)
        h = defaultHeight;
    m_size = IntSize(w, h);

    if (RenderObject* ro = renderer())
        if (m_rendererIsCanvas) {
            RenderHTMLCanvas* r = static_cast<RenderHTMLCanvas*>(ro);
            r->setIntrinsicWidth(w);
            r->setIntrinsicHeight(h);
            r->repaint();
        }

    m_createdDrawingContext = false;
    fastFree(m_data);
    m_data = 0;
    delete m_drawingContext;
    m_drawingContext = 0;
}

void HTMLCanvasElement::paint(GraphicsContext* p, const IntRect& r)
{
    if (p->paintingDisabled())
        return;
#if PLATFORM(CG)
    if (CGImageRef image = createPlatformImage()) {
        CGContextDrawImage(p->platformContext(), p->roundToDevicePixels(r), image);
        CGImageRelease(image);
    }
#endif
}

void HTMLCanvasElement::createDrawingContext() const
{
    ASSERT(!m_createdDrawingContext);
    ASSERT(!m_data);

    m_createdDrawingContext = true;

    float unscaledWidth = width();
    float unscaledHeight = height();
    float pageScaleFactor = document()->frame() ? document()->frame()->page()->chrome()->scaleFactor() : 1.0f;
    float wf = ceilf(unscaledWidth * pageScaleFactor);
    float hf = ceilf(unscaledHeight * pageScaleFactor);
    
    if (!(wf >= 1 && hf >= 1 && wf * hf <= maxCanvasArea))
        return;
        
    unsigned w = static_cast<unsigned>(wf);
    unsigned h = static_cast<unsigned>(hf);
    
    size_t bytesPerRow = w * 4;
    if (bytesPerRow / 4 != w) // check for overflow
        return;
    m_data = fastCalloc(h, bytesPerRow);
    if (!m_data)
        return;
#if PLATFORM(CG)
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef bitmapContext = CGBitmapContextCreate(m_data, w, h, 8, bytesPerRow, colorSpace, kCGImageAlphaPremultipliedLast);
    CGContextScaleCTM(bitmapContext, w / unscaledWidth, h / unscaledHeight);
    CGColorSpaceRelease(colorSpace);
    m_drawingContext = new GraphicsContext(bitmapContext);
    CGContextRelease(bitmapContext);
#endif
}

GraphicsContext* HTMLCanvasElement::drawingContext() const
{
    if (!m_createdDrawingContext)
        createDrawingContext();
    return m_drawingContext;
}

#if PLATFORM(CG)

CGImageRef HTMLCanvasElement::createPlatformImage() const
{
    GraphicsContext* context = drawingContext();
    if (!context)
        return 0;
    
    CGContextRef contextRef = context->platformContext();
    if (!contextRef)
        return 0;
    
    CGContextFlush(contextRef);
    
    return CGBitmapContextCreateImage(contextRef);
}

#endif

}
