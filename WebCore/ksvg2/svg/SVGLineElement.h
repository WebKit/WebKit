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

#ifndef KSVG_SVGLineElementImpl_H
#define KSVG_SVGLineElementImpl_H
#if SVG_SUPPORT

#include "SVGTests.h"
#include "SVGLangSpace.h"
#include "SVGStyledTransformableElement.h"
#include "SVGExternalResourcesRequired.h"

namespace WebCore
{
    class SVGAnimatedLength;
    class SVGLineElement : public SVGStyledTransformableElement,
                               public SVGTests,
                               public SVGLangSpace,
                               public SVGExternalResourcesRequired
    {
    public:
        SVGLineElement(const QualifiedName&, Document*);
        virtual ~SVGLineElement();
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        // 'SVGLineElement' functions
        SVGAnimatedLength *x1() const;
        SVGAnimatedLength *y1() const;
        SVGAnimatedLength *x2() const;
        SVGAnimatedLength *y2() const;

        virtual void parseMappedAttribute(MappedAttribute *attr);

        virtual bool rendererIsNeeded(RenderStyle *style) { return StyledElement::rendererIsNeeded(style); }
        virtual KCanvasPath* toPathData() const;

        virtual const SVGStyledElement *pushAttributeContext(const SVGStyledElement *context);

    private:
        mutable RefPtr<SVGAnimatedLength> m_x1;
        mutable RefPtr<SVGAnimatedLength> m_y1;
        mutable RefPtr<SVGAnimatedLength> m_x2;
        mutable RefPtr<SVGAnimatedLength> m_y2;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
