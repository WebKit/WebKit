/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef RenderForeignObject_h
#define RenderForeignObject_h
#if ENABLE(SVG) && ENABLE(SVG_FOREIGN_OBJECT)

#include "AffineTransform.h"
#include "RenderSVGBlock.h"

namespace WebCore {

class SVGForeignObjectElement;

class RenderForeignObject : public RenderSVGBlock {
public:
    RenderForeignObject(SVGForeignObjectElement*);

    virtual const char* renderName() const { return "RenderForeignObject"; }

    virtual void paint(PaintInfo&, int parentX, int parentY);

    virtual AffineTransform localTransform() const { return m_localTransform; }
    virtual bool calculateLocalTransform();

    virtual void computeAbsoluteRepaintRect(IntRect&, bool fixed);
    virtual bool requiresLayer();
    virtual void layout();

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);

 private:
    AffineTransform translationForAttributes();

    AffineTransform m_localTransform;
    IntRect m_absoluteBounds;
};

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FOREIGN_OBJECT)
#endif // RenderForeignObject_h
