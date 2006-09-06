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

#ifndef KSVG_SVGRectElementImpl_H
#define KSVG_SVGRectElementImpl_H
#ifdef SVG_SUPPORT

#include "SVGTests.h"
#include "SVGLangSpace.h"
#include "SVGStyledTransformableElement.h"
#include "SVGExternalResourcesRequired.h"

namespace WebCore
{
    class SVGLength;
    class SVGRectElement : public SVGStyledTransformableElement,
                               public SVGTests,
                               public SVGLangSpace,
                               public SVGExternalResourcesRequired
    {
    public:
        SVGRectElement(const QualifiedName&, Document*);
        virtual ~SVGRectElement();
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        // 'SVGRectElement' functions
        virtual void parseMappedAttribute(MappedAttribute *attr);

        virtual bool rendererIsNeeded(RenderStyle *style) { return StyledElement::rendererIsNeeded(style); }
        virtual Path toPathData() const;

        virtual const SVGStyledElement *pushAttributeContext(const SVGStyledElement *context);

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGLength*, RefPtr<SVGLength>, X, x)
        ANIMATED_PROPERTY_DECLARATIONS(SVGLength*, RefPtr<SVGLength>, Y, y)
        ANIMATED_PROPERTY_DECLARATIONS(SVGLength*, RefPtr<SVGLength>, Width, width)
        ANIMATED_PROPERTY_DECLARATIONS(SVGLength*, RefPtr<SVGLength>, Height, height)
        ANIMATED_PROPERTY_DECLARATIONS(SVGLength*, RefPtr<SVGLength>, Rx, rx)
        ANIMATED_PROPERTY_DECLARATIONS(SVGLength*, RefPtr<SVGLength>, Ry, ry)
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
