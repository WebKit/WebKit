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
#if ENABLE(3D_CANVAS)    
#include "CanvasRenderingContext3D.h"
#endif
#include "CanvasStyle.h"
#include "Chrome.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "ImageBuffer.h"
#include "MIMETypeRegistry.h"
#include "MappedAttribute.h"
#include "Page.h"
#include "RenderHTMLCanvas.h"
#include "Settings.h"
#include <math.h>
#include <stdio.h>

namespace WebCore {

using namespace HTMLNames;

// These values come from the WhatWG spec.
static const int defaultWidth = 300;
static const int defaultHeight = 150;

// Firefox limits width/height to 32767 pixels, but slows down dramatically before it 
// reaches that limit. We limit by area instead, giving us larger maximum dimensions,
// in exchange for a smaller maximum canvas size.
const float HTMLCanvasElement::MaxCanvasArea = 32768 * 8192; // Maximum canvas area in CSS pixels

HTMLCanvasElement::HTMLCanvasElement(const QualifiedName& tagName, Document* doc)
    : HTMLElement(tagName, doc)
    , m_size(defaultWidth, defaultHeight)
    , m_observer(0)
    , m_originClean(true)
    , m_ignoreReset(false)
    , m_createdImageBuffer(false)
{
    ASSERT(hasTagName(canvasTag));
}

HTMLCanvasElement::~HTMLCanvasElement()
{
    if (m_observer)
        m_observer->canvasDestroyed(this);
}

#if ENABLE(DASHBOARD_SUPPORT)

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

#endif

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

String HTMLCanvasElement::toDataURL(const String& mimeType, ExceptionCode& ec)
{
    if (!m_originClean) {
        ec = SECURITY_ERR;
        return String();
    }

    if (m_size.isEmpty())
        return String("data:,");

    if (mimeType.isNull() || !MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType))
        return buffer()->toDataURL("image/png");

    return buffer()->toDataURL(mimeType);
}

CanvasRenderingContext* HTMLCanvasElement::getContext(const String& type)
{
    // A Canvas can either be "2D" or "3D" never both. If you request a 2D canvas and the existing
    // context is already 2D, just return that. If the existing context is 3D, then destroy it
    // before creating a new 2D context. Vice versa when requesting a 3D canvas. Requesting a
    // context with any other type string will destroy any existing context.
    
    // FIXME - The code depends on the context not going away once created, to prevent JS from
    // seeing a dangling pointer. So for now we will disallow the context from being changed
    // once it is created.
    if (type == "2d") {
        if (m_context && !m_context->is2d())
            return 0;
        if (!m_context)
            m_context = new CanvasRenderingContext2D(this);
        return m_context.get();
    }
#if ENABLE(3D_CANVAS)    
    Settings* settings = document()->settings();
    if (settings && settings->experimentalWebGLEnabled()) {
        if ((type == "webkit-3d") ||
            (type == "GL")) {
            if (m_context && !m_context->is3d())
                return 0;
            if (!m_context) {
                m_context = new CanvasRenderingContext3D(this);

                // Need to make sure a RenderLayer and compositing layer get created for the Canvas
                setNeedsStyleRecalc(SyntheticStyleChange);
            }
            return m_context.get();
        }
    }
#endif
    return 0;
}

void HTMLCanvasElement::willDraw(const FloatRect& rect)
{
    if (m_imageBuffer)
        m_imageBuffer->clearImage();
    
    if (RenderBox* ro = renderBox()) {
        FloatRect destRect = ro->contentBoxRect();
        FloatRect r = mapRect(rect, FloatRect(0, 0, m_size.width(), m_size.height()), destRect);
        r.intersect(destRect);
        if (m_dirtyRect.contains(r))
            return;

        m_dirtyRect.unite(r);
        ro->repaintRectangle(enclosingIntRect(m_dirtyRect));
    }
    
    if (m_observer)
        m_observer->canvasChanged(this, rect);
}

void HTMLCanvasElement::reset()
{
    if (m_ignoreReset)
        return;

    bool ok;
    int w = getAttribute(widthAttr).toInt(&ok);
    if (!ok)
        w = defaultWidth;
    int h = getAttribute(heightAttr).toInt(&ok);
    if (!ok)
        h = defaultHeight;

    IntSize oldSize = m_size;
    m_size = IntSize(w, h);

    bool hadImageBuffer = m_createdImageBuffer;
    m_createdImageBuffer = false;
    m_imageBuffer.clear();
    if (m_context && m_context->is2d())
        static_cast<CanvasRenderingContext2D*>(m_context.get())->reset();

    if (RenderObject* renderer = this->renderer()) {
        if (m_rendererIsCanvas) {
            if (oldSize != m_size)
                toRenderHTMLCanvas(renderer)->canvasSizeChanged();
            if (hadImageBuffer)
                renderer->repaint();
        }
    }

    if (m_observer)
        m_observer->canvasResized(this);
}

void HTMLCanvasElement::paint(GraphicsContext* context, const IntRect& r)
{
    // Clear the dirty rect
    m_dirtyRect = FloatRect();

    if (context->paintingDisabled())
        return;
    
#if ENABLE(3D_CANVAS)
    CanvasRenderingContext3D* context3D = NULL;
    if (m_context && m_context->is3d()) {
        context3D = static_cast<CanvasRenderingContext3D*>(m_context.get());
        context3D->beginPaint();
    }
#endif

    if (m_imageBuffer) {
        Image* image = m_imageBuffer->image();
        if (image)
            context->drawImage(image, r);
    }

#if ENABLE(3D_CANVAS)
    if (context3D != NULL) {
        context3D->reshape(r.width(), r.height());
        context3D->endPaint();
    }
#endif
}

IntRect HTMLCanvasElement::convertLogicalToDevice(const FloatRect& logicalRect) const
{
    return IntRect(convertLogicalToDevice(logicalRect.location()), convertLogicalToDevice(logicalRect.size()));
}

IntSize HTMLCanvasElement::convertLogicalToDevice(const FloatSize& logicalSize) const
{
    float pageScaleFactor = document()->frame() ? document()->frame()->page()->chrome()->scaleFactor() : 1.0f;
    float wf = ceilf(logicalSize.width() * pageScaleFactor);
    float hf = ceilf(logicalSize.height() * pageScaleFactor);
    
    if (!(wf >= 1 && hf >= 1 && wf * hf <= MaxCanvasArea))
        return IntSize();

    return IntSize(static_cast<unsigned>(wf), static_cast<unsigned>(hf));
}

IntPoint HTMLCanvasElement::convertLogicalToDevice(const FloatPoint& logicalPos) const
{
    float pageScaleFactor = document()->frame() ? document()->frame()->page()->chrome()->scaleFactor() : 1.0f;
    float xf = logicalPos.x() * pageScaleFactor;
    float yf = logicalPos.y() * pageScaleFactor;
    
    return IntPoint(static_cast<unsigned>(xf), static_cast<unsigned>(yf));
}

void HTMLCanvasElement::createImageBuffer() const
{
    ASSERT(!m_imageBuffer);

    m_createdImageBuffer = true;
    
    FloatSize unscaledSize(width(), height());
    IntSize size = convertLogicalToDevice(unscaledSize);
    if (!size.width() || !size.height())
        return;

    m_imageBuffer = ImageBuffer::create(size);
    // The convertLogicalToDevice MaxCanvasArea check should prevent common cases
    // where ImageBuffer::create() returns NULL, however we could still be low on memory.
    if (!m_imageBuffer)
        return;
    m_imageBuffer->context()->scale(FloatSize(size.width() / unscaledSize.width(), size.height() / unscaledSize.height()));
    m_imageBuffer->context()->setShadowsIgnoreTransforms(true);
}

GraphicsContext* HTMLCanvasElement::drawingContext() const
{
    return buffer() ? m_imageBuffer->context() : 0;
}

ImageBuffer* HTMLCanvasElement::buffer() const
{
    if (!m_createdImageBuffer)
        createImageBuffer();
    return m_imageBuffer.get();
}
    
TransformationMatrix HTMLCanvasElement::baseTransform() const
{
    ASSERT(m_createdImageBuffer);
    FloatSize unscaledSize(width(), height());
    IntSize size = convertLogicalToDevice(unscaledSize);
    TransformationMatrix transform;
    if (size.width() && size.height())
        transform.scaleNonUniform(size.width() / unscaledSize.width(), size.height() / unscaledSize.height());
    transform.multiply(m_imageBuffer->baseTransform());
    return transform;
}

#if ENABLE(3D_CANVAS)    
bool HTMLCanvasElement::is3D() const
{
    return m_context && m_context->is3d();
}
#endif

}
