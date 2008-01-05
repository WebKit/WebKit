/*
    Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
    Copyright (C) 2006 Apple Computer, Inc.
    Copyright (C) 2007 Rob Buis <buis@kde.org>

    This file is part of the WebKit project.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef RenderSVGImage_h
#define RenderSVGImage_h

#if ENABLE(SVG)

#include "AffineTransform.h"
#include "FloatRect.h"
#include "RenderImage.h"

namespace WebCore {

    class SVGImageElement;
    class SVGPreserveAspectRatio;

    class RenderSVGImage : public RenderImage {
    public:
        RenderSVGImage(SVGImageElement*);
        virtual ~RenderSVGImage();
        
        virtual AffineTransform localTransform() const { return m_localTransform; }
        
        virtual FloatRect relativeBBox(bool includeStroke = true) const;
        virtual IntRect absoluteClippedOverflowRect();
        virtual void absoluteRects(Vector<IntRect>&, int tx, int ty, bool topLevel = true);
        virtual void addFocusRingRects(GraphicsContext*, int tx, int ty);

        virtual void imageChanged(CachedImage*);
        void adjustRectsForAspectRatio(FloatRect& destRect, FloatRect& srcRect, SVGPreserveAspectRatio*);
        
        virtual void layout();
        virtual void paint(PaintInfo&, int parentX, int parentY);

        bool requiresLayer();

        virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int _x, int _y, int _tx, int _ty, HitTestAction);

        bool calculateLocalTransform();

    private:
        void calculateAbsoluteBounds();
        AffineTransform m_localTransform;
        FloatRect m_localBounds;
        IntRect m_absoluteBounds;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // RenderSVGImage_h

// vim:ts=4:noet
