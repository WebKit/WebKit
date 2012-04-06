/*
 * Copyright (C) 2011 University of Szeged
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RenderSVGRect_h
#define RenderSVGRect_h

#if ENABLE(SVG)
#include "RenderSVGPath.h"
#include "SVGRectElement.h"

namespace WebCore {

class RenderSVGRect : public RenderSVGShape {
public:
    explicit RenderSVGRect(SVGRectElement*);
    virtual ~RenderSVGRect();

private:
    virtual bool isSVGRect() const { return true; }
    virtual const char* renderName() const { return "RenderSVGRect"; }

    virtual void createShape();
    virtual bool isEmpty() const { return hasPath() ? RenderSVGShape::isEmpty() : m_boundingBox.isEmpty(); };
    virtual void fillShape(GraphicsContext*) const;
    virtual void strokeShape(GraphicsContext*) const;
    virtual FloatRect objectBoundingBox() const;
    virtual FloatRect strokeBoundingBox() const;
    virtual bool shapeDependentStrokeContains(const FloatPoint&) const;
    virtual bool shapeDependentFillContains(const FloatPoint&, const WindRule) const;

private:
    FloatRect m_boundingBox;
    FloatRect m_innerStrokeRect;
    FloatRect m_outerStrokeRect;
    FloatRect m_strokeBoundingRect;
};

}

#endif // ENABLE(SVG)
#endif
