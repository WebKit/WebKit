/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef HTMLCanvasElement_h
#define HTMLCanvasElement_h

#include "TransformationMatrix.h"
#include "FloatRect.h"
#include "HTMLElement.h"
#include "IntSize.h"

namespace WebCore {

class CanvasRenderingContext2D;
typedef CanvasRenderingContext2D CanvasRenderingContext;
class FloatPoint;
class FloatRect;
class FloatSize;
class GraphicsContext;
class HTMLCanvasElement;
class ImageBuffer;
class IntPoint;
class IntSize;

class CanvasObserver {
public:
    virtual ~CanvasObserver() {};

    virtual void canvasChanged(HTMLCanvasElement*, const FloatRect& changedRect) = 0;
    virtual void canvasResized(HTMLCanvasElement*) = 0;
    virtual void canvasDestroyed(HTMLCanvasElement*) = 0;
};

class HTMLCanvasElement : public HTMLElement {
public:
    HTMLCanvasElement(const QualifiedName&, Document*);
    virtual ~HTMLCanvasElement();

#if ENABLE(DASHBOARD_SUPPORT)
    virtual HTMLTagStatus endTagRequirement() const;
    virtual int tagPriority() const;
#endif

    int width() const { return m_size.width(); }
    int height() const { return m_size.height(); }
    void setWidth(int);
    void setHeight(int);

    String toDataURL(const String& mimeType, ExceptionCode&);

    CanvasRenderingContext* getContext(const String&);

    virtual void parseMappedAttribute(MappedAttribute*);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

    IntSize size() const { return m_size; }
    void setSize(const IntSize& size)
    { 
        if (size == m_size)
            return;
        m_ignoreReset = true; 
        setWidth(size.width());
        setHeight(size.height());
        m_ignoreReset = false;
        reset();
    }

    void willDraw(const FloatRect&);

    void paint(GraphicsContext*, const IntRect&);

    GraphicsContext* drawingContext() const;

    ImageBuffer* buffer() const;

    IntRect convertLogicalToDevice(const FloatRect&) const;
    IntSize convertLogicalToDevice(const FloatSize&) const;
    IntPoint convertLogicalToDevice(const FloatPoint&) const;

    void setOriginTainted() { m_originClean = false; } 
    bool originClean() const { return m_originClean; }

    static const float MaxCanvasArea;

    void setObserver(CanvasObserver* o) { m_observer = o; }

    TransformationMatrix baseTransform() const;
private:
    void createImageBuffer() const;
    void reset();

    bool m_rendererIsCanvas;

    OwnPtr<CanvasRenderingContext2D> m_2DContext;
    IntSize m_size;    
    CanvasObserver* m_observer;

    bool m_originClean;
    bool m_ignoreReset;
    FloatRect m_dirtyRect;

    // m_createdImageBuffer means we tried to malloc the buffer.  We didn't necessarily get it.
    mutable bool m_createdImageBuffer;
    mutable OwnPtr<ImageBuffer> m_imageBuffer;
};

} //namespace

#endif
