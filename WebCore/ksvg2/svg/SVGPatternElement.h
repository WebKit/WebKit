/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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

#ifndef SVGPatternElement_H
#define SVGPatternElement_H

#ifdef SVG_SUPPORT

#include "SVGPaintServerPattern.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGFitToViewBox.h"
#include "SVGLangSpace.h"
#include "SVGStyledLocatableElement.h"
#include "SVGTests.h"
#include "SVGURIReference.h"


namespace WebCore
{
    class SVGLength;
    class SVGPatternElement;
    class SVGTransformList;
    class SVGPatternElement : public SVGStyledLocatableElement,
                              public SVGURIReference,
                              public SVGTests,
                              public SVGLangSpace,
                              public SVGExternalResourcesRequired,
                              public SVGFitToViewBox,
                              public SVGResourceListener
    {
    public:
        SVGPatternElement(const QualifiedName&, Document*);
        virtual ~SVGPatternElement();
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        // 'SVGPatternElement' functions
        virtual void parseMappedAttribute(MappedAttribute*);

        const SVGStyledElement* pushAttributeContext(const SVGStyledElement*);

        virtual void resourceNotification() const;
        virtual void notifyAttributeChange() const;

        virtual bool rendererIsNeeded(RenderStyle* style) { return StyledElement::rendererIsNeeded(style); }
        virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
        virtual SVGResource* canvasResource();

        // 'virtual SVGLocatable' functions
        virtual AffineTransform getCTM() const;

    protected:
        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGURIReference, String, Href, href)
        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGExternalResourcesRequired, bool, ExternalResourcesRequired, externalResourcesRequired)
        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGFitToViewBox, FloatRect, ViewBox, viewBox)
        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGFitToViewBox, SVGPreserveAspectRatio*, PreserveAspectRatio, preserveAspectRatio)

        ANIMATED_PROPERTY_DECLARATIONS(SVGPatternElement, SVGLength, SVGLength, X, x)
        ANIMATED_PROPERTY_DECLARATIONS(SVGPatternElement, SVGLength, SVGLength, Y, y)
        ANIMATED_PROPERTY_DECLARATIONS(SVGPatternElement, SVGLength, SVGLength, Width, width)
        ANIMATED_PROPERTY_DECLARATIONS(SVGPatternElement, SVGLength, SVGLength, Height, height)
        ANIMATED_PROPERTY_DECLARATIONS(SVGPatternElement, int, int, PatternUnits, patternUnits)
        ANIMATED_PROPERTY_DECLARATIONS(SVGPatternElement, int, int, PatternContentUnits, patternContentUnits)
        ANIMATED_PROPERTY_DECLARATIONS(SVGPatternElement, SVGTransformList*, RefPtr<SVGTransformList>, PatternTransform, patternTransform)

        mutable bool m_ignoreAttributeChanges;
        mutable RefPtr<SVGPaintServerPattern> m_paintServer;

        virtual const SVGElement* contextElement() const { return this; }

    private:
        // notifyAttributeChange helpers:
        void fillAttributesFromReferencePattern(const SVGPatternElement* target, AffineTransform& patternTransformMatrix);
        void drawPatternContentIntoTile(const SVGPatternElement* target, const IntSize& newSize, AffineTransform patternTransformMatrix);
        void notifyClientsToRepaint() const;
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
