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

#ifndef KSVG_SVGUseElementImpl_H
#define KSVG_SVGUseElementImpl_H
#if SVG_SUPPORT

#include "SVGTests.h"
#include "SVGLangSpace.h"
#include "SVGURIReference.h"
#include "SVGStyledTransformableElement.h"
#include "SVGExternalResourcesRequired.h"

namespace WebCore
{
    class SVGAnimatedLength;
    class SVGUseElement : public SVGStyledTransformableElement,
                              public SVGTests,
                              public SVGLangSpace,
                              public SVGExternalResourcesRequired,
                              public SVGURIReference
    {
    public:
        SVGUseElement(const QualifiedName& tagName, Document *doc);
        virtual ~SVGUseElement();
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        // Derived from: 'Element'
        virtual bool hasChildNodes() const;

        virtual void closeRenderer();

        // 'SVGUseElement' functions
        SVGAnimatedLength *x() const;
        SVGAnimatedLength *y() const;

        SVGAnimatedLength *width() const;
        SVGAnimatedLength *height() const;

        virtual void parseMappedAttribute(MappedAttribute *attr);

        virtual bool rendererIsNeeded(RenderStyle *style) { return StyledElement::rendererIsNeeded(style); }
        virtual RenderObject *createRenderer(RenderArena *arena, RenderStyle *style);

    private:
        mutable RefPtr<SVGAnimatedLength> m_x;
        mutable RefPtr<SVGAnimatedLength> m_y;
        mutable RefPtr<SVGAnimatedLength> m_width;
        mutable RefPtr<SVGAnimatedLength> m_height;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
