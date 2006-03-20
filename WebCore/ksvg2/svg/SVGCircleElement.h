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

#ifndef KSVG_SVGCircleElementImpl_H
#define KSVG_SVGCircleElementImpl_H
#if SVG_SUPPORT

#include "SVGTests.h"
#include "SVGLangSpace.h"
#include "SVGStyledTransformableElement.h"
#include "SVGExternalResourcesRequired.h"

namespace WebCore
{
    class SVGAnimatedLength;
    class SVGCircleElement : public SVGStyledTransformableElement,
                                 public SVGTests,
                                 public SVGLangSpace,
                                 public SVGExternalResourcesRequired
    {
    public:
        SVGCircleElement(const QualifiedName& tagName, Document *doc);
        virtual ~SVGCircleElement();
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        // 'SVGCircleElement' functions
        SVGAnimatedLength *cx() const;
        SVGAnimatedLength *cy() const;
        SVGAnimatedLength *r() const;

        virtual void parseMappedAttribute(MappedAttribute *attr);

        virtual bool rendererIsNeeded(RenderStyle *style) { return StyledElement::rendererIsNeeded(style); }
        virtual KCanvasPath* toPathData() const;

        virtual const SVGStyledElement *pushAttributeContext(const SVGStyledElement *context);

    private:
        mutable RefPtr<SVGAnimatedLength> m_cx;
        mutable RefPtr<SVGAnimatedLength> m_cy;
        mutable RefPtr<SVGAnimatedLength> m_r;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
