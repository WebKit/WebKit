/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KSVG_SVGPatternElementImpl_H
#define KSVG_SVGPatternElementImpl_H
#if SVG_SUPPORT

#include "SVGTests.h"
#include "SVGLangSpace.h"
#include "SVGURIReference.h"
#include "SVGFitToViewBox.h"
#include "SVGStyledLocatableElement.h"
#include "SVGExternalResourcesRequired.h"

#include <kcanvas/KCanvasResourceListener.h>
#include <kcanvas/device/KRenderingPaintServer.h>
#include <kcanvas/device/KRenderingPaintServerPattern.h>

class KCanvasImage;

namespace WebCore
{
    class SVGAnimatedLength;
    class SVGPatternElement;
    class SVGAnimatedEnumeration;
    class SVGAnimatedTransformList;
    class SVGPatternElement : public SVGStyledLocatableElement,
                                  public SVGURIReference,
                                  public SVGTests,
                                  public SVGLangSpace,
                                  public SVGExternalResourcesRequired,
                                  public SVGFitToViewBox,
                                  public KCanvasResourceListener
    {
    public:
        SVGPatternElement(const QualifiedName& tagName, Document *doc);
        virtual ~SVGPatternElement();
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        // 'SVGPatternElement' functions
        SVGAnimatedEnumeration *patternUnits() const;
        SVGAnimatedEnumeration *patternContentUnits() const;
        SVGAnimatedTransformList *patternTransform() const;

        SVGAnimatedLength *x() const;
        SVGAnimatedLength *y() const;

        SVGAnimatedLength *width() const;
        SVGAnimatedLength *height() const;

        virtual void parseMappedAttribute(MappedAttribute *attr);

        const SVGStyledElement *pushAttributeContext(const SVGStyledElement *context);

        virtual void resourceNotification() const;
        virtual void notifyAttributeChange() const;

        virtual bool rendererIsNeeded(RenderStyle *style) { return StyledElement::rendererIsNeeded(style); }
        virtual RenderObject *createRenderer(RenderArena *arena, RenderStyle *style);
        virtual KRenderingPaintServerPattern *canvasResource();

        // 'virtual SVGLocatable' functions
        virtual SVGMatrix *getCTM() const;

    protected:
        mutable RefPtr<SVGAnimatedLength> m_x;
        mutable RefPtr<SVGAnimatedLength> m_y;
        mutable RefPtr<SVGAnimatedLength> m_width;
        mutable RefPtr<SVGAnimatedLength> m_height;
        
        mutable RefPtr<SVGAnimatedEnumeration> m_patternUnits;
        mutable RefPtr<SVGAnimatedEnumeration> m_patternContentUnits;

        mutable RefPtr<SVGAnimatedTransformList> m_patternTransform;

        mutable KCanvasImage *m_tile;
        mutable bool m_ignoreAttributeChanges;
        mutable KRenderingPaintServerPattern *m_paintServer;
        
    private:
        // notifyAttributeChange helpers:
        void fillAttributesFromReferencePattern(const SVGPatternElement *target, KCanvasMatrix &patternTransformMatrix) const;
        void drawPatternContentIntoTile(const SVGPatternElement *target, const IntSize &newSize, KCanvasMatrix patternTransformMatrix) const;
        void notifyClientsToRepaint() const;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
