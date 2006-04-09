/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "RenderHTMLCanvas.h"

namespace WebCore {

using namespace HTMLNames;

// These value come from the specification.
const int defaultWidth = 300;
const int defaultHeight = 150;

HTMLCanvasElement::HTMLCanvasElement(Document* doc)
    : HTMLElement(canvasTag, doc), m_size(defaultWidth, defaultHeight)
    , m_createdDrawingContext(false), m_data(0)
#if __APPLE__
    , m_drawingContext(0)
#endif
{
}

HTMLCanvasElement::~HTMLCanvasElement()
{
    if (m_2DContext)
        m_2DContext->detachCanvas();
    fastFree(m_data);
#if __APPLE__
    CGContextRelease(m_drawingContext);
#endif
}

void HTMLCanvasElement::parseMappedAttribute(MappedAttribute* attr)
{
    const QualifiedName& attrName = attr->name();
    if (attrName == widthAttr || attrName == heightAttr)
        reset();
    HTMLElement::parseMappedAttribute(attr);
}

RenderObject* HTMLCanvasElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
    RenderHTMLCanvas* r = new (arena) RenderHTMLCanvas(this);
    r->setIntrinsicWidth(width());
    r->setIntrinsicHeight(height());
    return r;
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
    // FIXME: Web Applications 1.0 says "2d" only, but the code here matches historical behavior of WebKit.
    if (type.isNull() || type == "2d" || type == "2D") {
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

    RenderHTMLCanvas* r = static_cast<RenderHTMLCanvas*>(renderer());
    if (r) {
        r->setIntrinsicWidth(w);
        r->setIntrinsicHeight(h);
        r->repaint();
    }

    m_createdDrawingContext = false;
    CGContextRelease(m_drawingContext);
    m_drawingContext = 0;
    fastFree(m_data);
    m_data = 0;
}

void HTMLCanvasElement::paint(GraphicsContext* p, const IntRect& r)
{
    if (p->paintingDisabled())
        return;
#if __APPLE__
    if (CGImageRef image = createPlatformImage()) {
        CGContextDrawImage(p->currentCGContext(), r, image);
        CGImageRelease(image);
    }
#endif
}

void HTMLCanvasElement::createDrawingContext() const
{
    ASSERT(!m_createdDrawingContext);
    ASSERT(!m_data);

    m_createdDrawingContext = true;

    if (width() <= 0 || height() <= 0)
        return;
    unsigned w = width();
    size_t bytesPerRow = w * 4;
    if (bytesPerRow / 4 != w) // check for overflow
        return;
    m_data = fastCalloc(height(), bytesPerRow);
    if (!m_data)
        return;
#if __APPLE__
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    m_drawingContext = CGBitmapContextCreate(m_data, w, height(), 8, bytesPerRow, colorSpace, kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(colorSpace);
#endif
}

#if __APPLE__

CGContextRef HTMLCanvasElement::drawingContext() const
{
    if (!m_createdDrawingContext)
        createDrawingContext();
    return m_drawingContext;
}

CGImageRef HTMLCanvasElement::createPlatformImage() const
{
    CGContextRef context = drawingContext();
    if (!context)
        return 0;
    CGContextFlush(context);
    return CGBitmapContextCreateImage(context);
}

#endif

}
