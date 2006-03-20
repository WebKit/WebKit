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
#include "HTMLNames.h"
#include "RenderHTMLCanvas.h"

namespace WebCore {

using namespace HTMLNames;

HTMLCanvasElement::HTMLCanvasElement(Document *doc)
    : HTMLImageElement(canvasTag, doc), m_2DContext(0)
{
}

HTMLCanvasElement::~HTMLCanvasElement()
{
    if (m_2DContext)
        m_2DContext->detachCanvas();
}

bool HTMLCanvasElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName != srcAttr) // Ignore the src attribute
        return HTMLImageElement::mapToEntry(attrName, result);
    return false;
}

void HTMLCanvasElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() != srcAttr) // Canvas ignores the src attribute
        HTMLImageElement::parseMappedAttribute(attr);
}

RenderObject *HTMLCanvasElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
#if __APPLE__
    return new (arena) RenderHTMLCanvas(this);
#else
    return 0;
#endif
}

void HTMLCanvasElement::attach()
{
    // Don't want to call image's attach().
    HTMLElement::attach();
}

void HTMLCanvasElement::detach()
{
    // Don't want to call image's detach().
    HTMLElement::detach();

    if (m_2DContext)
        m_2DContext->reset();
}

bool HTMLCanvasElement::isURLAttribute(Attribute *attr) const
{
    return ((attr->name() == usemapAttr && attr->value().domString()[0] != '#'));
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

IntSize HTMLCanvasElement::size() const
{
    RenderHTMLCanvas* canvasRenderer = static_cast<RenderHTMLCanvas*>(renderer());
    if (!canvasRenderer)
        return IntSize();
#if __APPLE__
    if (CGContextRef context = canvasRenderer->drawingContext())
        return IntSize(CGBitmapContextGetWidth(context), CGBitmapContextGetHeight(context));
#endif
    return IntSize();
}

#if __APPLE__

CGImageRef HTMLCanvasElement::createPlatformImage() const
{
    RenderHTMLCanvas* canvasRenderer = static_cast<RenderHTMLCanvas*>(renderer());
    if (!canvasRenderer)
        return 0;
    CGContextRef context = canvasRenderer->drawingContext();
    if (!context)
        return 0;
    return CGBitmapContextCreateImage(context);
}

#endif

}
