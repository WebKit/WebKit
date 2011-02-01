/*
 * Copyright (C) 2004, 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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

#ifndef HTMLCanvasElement_h
#define HTMLCanvasElement_h

#include "CanvasSurface.h"
#include "FloatRect.h"
#include "HTMLElement.h"
#if ENABLE(3D_CANVAS)    
#include "GraphicsContext3D.h"
#endif
#include "IntSize.h"

namespace WebCore {

class CanvasContextAttributes;
class CanvasRenderingContext;
class GraphicsContext;
class HTMLCanvasElement;
class IntSize;

class CanvasObserver {
public:
    virtual ~CanvasObserver() { }

    virtual void canvasChanged(HTMLCanvasElement*, const FloatRect& changedRect) = 0;
    virtual void canvasResized(HTMLCanvasElement*) = 0;
    virtual void canvasDestroyed(HTMLCanvasElement*) = 0;
};

class HTMLCanvasElement : public HTMLElement, public CanvasSurface {
public:
    HTMLCanvasElement(const QualifiedName&, Document*);
    virtual ~HTMLCanvasElement();

    void setWidth(int);
    void setHeight(int);

    CanvasRenderingContext* getContext(const String&, CanvasContextAttributes* attributes = 0);

    void setSize(const IntSize& newSize)
    { 
        if (newSize == size())
            return;
        m_ignoreReset = true; 
        setWidth(newSize.width());
        setHeight(newSize.height());
        m_ignoreReset = false;
        reset();
    }

    virtual void willDraw(const FloatRect&);

    void paint(GraphicsContext*, const IntRect&);

    void addObserver(CanvasObserver* observer);
    void removeObserver(CanvasObserver* observer);

    CanvasRenderingContext* renderingContext() const { return m_context.get(); }

    RenderBox* renderBox() const { return HTMLElement::renderBox(); }
    RenderStyle* computedStyle() { return HTMLElement::computedStyle(); }

#if ENABLE(3D_CANVAS)    
    bool is3D() const;
#endif

private:
#if ENABLE(DASHBOARD_SUPPORT)
    virtual HTMLTagStatus endTagRequirement() const;
    virtual int tagPriority() const;
#endif

    virtual void parseMappedAttribute(MappedAttribute*);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

    virtual void attach();

    virtual void recalcStyle(StyleChange);

    void reset();

    bool m_rendererIsCanvas;

    OwnPtr<CanvasRenderingContext> m_context;
    HashSet<CanvasObserver*> m_observers;

    bool m_ignoreReset;
    FloatRect m_dirtyRect;
};

} //namespace

#endif
