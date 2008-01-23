/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
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

#if PLATFORM(QT)
#include <QPainter>
#include <QPixmap>
#elif PLATFORM(CAIRO)
#include <cairo.h>
#endif

namespace WebCore {

using namespace HTMLNames;

// These values come from the WhatWG spec.
static const int defaultWidth = 300;
static const int defaultHeight = 150;

// Firefox limits width/height to 32767 pixels, but slows down dramatically before it 
// reaches that limit. We limit by area instead, giving us larger maximum dimensions,
// in exchange for a smaller maximum canvas size.
static const float maxCanvasArea = 32768 * 8192; // Maximum canvas area in CSS pixels

HTMLCanvasElement::HTMLCanvasElement(Document* doc)
    : HTMLElement(canvasTag, doc)
    , m_size(defaultWidth, defaultHeight)
    , m_createdDrawingContext(false)
    , m_data(0)
#if PLATFORM(QT)
    , m_painter(0)
#elif PLATFORM(CAIRO)
    , m_surface(0)
#endif
    , m_drawingContext(0)
{
}

HTMLCanvasElement::~HTMLCanvasElement()
{
    if (m_2DContext)
        m_2DContext->detachCanvas();
#if PLATFORM(CG)
    fastFree(m_data);
#elif PLATFORM(QT)
    delete m_painter;
    delete m_data;
#elif PLATFORM(CAIRO)
    if (m_surface)
        cairo_surface_destroy(m_surface);
#endif
    delete m_drawingContext;
}

HTMLTagStatus HTMLCanvasElement::endTagRequirement() const 
{
    Settings* settings = document()->settings();
    if (settings && settings->usesDashboardBackwardCompatibilityMode())
        return TagStatusForbidden; 

    return HTMLElement::endTagRequirement();
}

int HTMLCanvasElement::tagPriority() const 
{ 
    Settings* settings = document()->settings();
    if (settings && settings->usesDashboardBackwardCompatibilityMode())
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
    Settings* settings = document()->settings();
    if (settings && settings->isJavaScriptEnabled()) {
        m_rendererIsCanvas = true;
        return new (arena) RenderHTMLCanvas(this);
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

void HTMLCanvasElement::willDraw(const FloatRect& rect)
{
    if (RenderObject* ro = renderer()) {
#ifdef CANVAS_INCREMENTAL_REPAINT
        // Handle CSS triggered scaling
        float widthScale = static_cast<float>(ro->width()) / static_cast<float>(m_size.width());
        float heightScale = static_cast<float>(ro->height()) / static_cast<float>(m_size.height());
        FloatRect r(rect.x() * widthScale, rect.y() * heightScale, rect.width() * widthScale, rect.height() * heightScale);
        ro->repaintRectangle(enclosingIntRect(r));
#else
        ro->repaint();
#endif
    }
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

    IntSize oldSize = m_size;
    m_size = IntSize(w, h);

    bool hadDrawingContext = m_createdDrawingContext;
    m_createdDrawingContext = false;
#if PLATFORM(CG)
    fastFree(m_data);
#elif PLATFORM(QT)
    delete m_painter;
    m_painter = 0;
    delete m_data;
#elif PLATFORM(CAIRO)
    if (m_surface)
        cairo_surface_destroy(m_surface);
    m_surface = 0;
#endif
    m_data = 0;
    delete m_drawingContext;
    m_drawingContext = 0;
    if (m_2DContext)
        m_2DContext->reset();

    if (RenderObject* ro = renderer())
        if (m_rendererIsCanvas) {
            if (oldSize != m_size)
                static_cast<RenderHTMLCanvas*>(ro)->canvasSizeChanged();
            if (hadDrawingContext)
                ro->repaint();
        }
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
#elif PLATFORM(QT)
    if (m_data) {
        QPen currentPen = m_painter->pen();
        qreal currentOpacity = m_painter->opacity();
        QBrush currentBrush = m_painter->brush();
        QBrush currentBackground = m_painter->background();
        if (m_painter->isActive())
            m_painter->end();
        static_cast<QPainter*>(p->platformContext())->drawImage(r, *m_data);
        m_painter->begin(m_data);
        m_painter->setPen(currentPen);
        m_painter->setBrush(currentBrush);
        m_painter->setOpacity(currentOpacity);
        m_painter->setBackground(currentBackground);
    }
#elif PLATFORM(CAIRO)
    if (cairo_surface_t* image = createPlatformImage()) {
        cairo_t* cr = p->platformContext();
        cairo_save(cr);
        cairo_translate(cr, r.x(), r.y());
        cairo_set_source_surface(cr, image, 0, 0);
        cairo_surface_destroy(image);
        cairo_rectangle(cr, 0, 0, r.width(), r.height());
        cairo_fill(cr);
        cairo_restore(cr);
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

#if PLATFORM(CG)
    size_t bytesPerRow = w * 4;
    if (bytesPerRow / 4 != w) // check for overflow
        return;
    m_data = fastCalloc(h, bytesPerRow);
    if (!m_data)
        return;
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef bitmapContext = CGBitmapContextCreate(m_data, w, h, 8, bytesPerRow, colorSpace, kCGImageAlphaPremultipliedLast);
    CGContextScaleCTM(bitmapContext, w / unscaledWidth, h / unscaledHeight);
    CGColorSpaceRelease(colorSpace);
    m_drawingContext = new GraphicsContext(bitmapContext);
    CGContextRelease(bitmapContext);
#elif PLATFORM(QT)
    m_data = new QImage(w, h, QImage::Format_ARGB32_Premultiplied);
    if (!m_data)
        return;
    m_painter = new QPainter(m_data);
    m_painter->setBackground(QBrush(Qt::transparent));
    m_painter->fillRect(0, 0, w, h, QColor(Qt::transparent));
    m_drawingContext = new GraphicsContext(m_painter);
#elif PLATFORM(CAIRO)
    m_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    // m_data is owned by m_surface
    m_data = cairo_image_surface_get_data(m_surface);
    cairo_t* cr = cairo_create(m_surface);
    cairo_scale(cr, w / unscaledWidth, h / unscaledHeight);
    m_drawingContext = new GraphicsContext(cr);
    cairo_destroy(cr);
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

#elif PLATFORM(QT)

QImage HTMLCanvasElement::createPlatformImage() const
{
    if (m_data)
        return *m_data;
    return QImage();
}

#elif PLATFORM(CAIRO)

cairo_surface_t* HTMLCanvasElement::createPlatformImage() const
{
    if (!m_surface)
        return 0;

    // Note that unlike CG, our returned image is not a copy or
    // copy-on-write, but the original. This is fine, since it is only
    // ever used as a source.

    cairo_surface_flush(m_surface);
    cairo_surface_reference(m_surface);
    return m_surface;
}

#endif

}
