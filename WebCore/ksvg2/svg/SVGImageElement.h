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

#ifndef KSVG_SVGImageElementImpl_H
#define KSVG_SVGImageElementImpl_H
#if SVG_SUPPORT

#include "SVGTests.h"
#include "SVGImageLoader.h"
#include "SVGLangSpace.h"
#include "SVGURIReference.h"
#include "SVGStyledTransformableElement.h"
#include "SVGExternalResourcesRequired.h"

namespace WebCore
{
    class SVGAnimatedPreserveAspectRatio;
    class SVGAnimatedLength;
    class SVGDocument;

    class SVGImageElement : public SVGStyledTransformableElement,
                                public SVGTests,
                                public SVGLangSpace,
                                public SVGExternalResourcesRequired,
                                public SVGURIReference
    {
    public:
        SVGImageElement(const QualifiedName&, Document*);
        virtual ~SVGImageElement();
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        // 'SVGImageElement' functions
        SVGAnimatedLength *x() const;
        SVGAnimatedLength *y() const;

        SVGAnimatedLength *width() const;
        SVGAnimatedLength *height() const;

        SVGAnimatedPreserveAspectRatio *preserveAspectRatio() const;

        virtual void parseMappedAttribute(MappedAttribute *attr);
        virtual void attach();

        virtual bool rendererIsNeeded(RenderStyle *style) { return StyledElement::rendererIsNeeded(style); }
        virtual RenderObject *createRenderer(RenderArena *arena, RenderStyle *style);

    private:
        mutable RefPtr<SVGAnimatedLength> m_x;
        mutable RefPtr<SVGAnimatedLength> m_y;
        mutable RefPtr<SVGAnimatedLength> m_width;
        mutable RefPtr<SVGAnimatedLength> m_height;
        mutable RefPtr<SVGAnimatedPreserveAspectRatio> m_preserveAspectRatio;

        SVGImageLoader m_imageLoader;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
