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

#ifndef KSVG_SVGClipPathElementImpl_H
#define KSVG_SVGClipPathElementImpl_H
#if SVG_SUPPORT

#include "SVGTests.h"
#include "SVGLangSpace.h"
#include "SVGStyledTransformableElement.h"
#include "SVGExternalResourcesRequired.h"

#include "KCanvasResources.h"

namespace WebCore
{
    class SVGAnimatedEnumeration;
    class SVGClipPathElement : public SVGStyledTransformableElement,
                                   public SVGTests,
                                   public SVGLangSpace,
                                   public SVGExternalResourcesRequired
    {
    public:
        SVGClipPathElement(const QualifiedName&, Document*);
        virtual ~SVGClipPathElement();
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        virtual KCanvasClipper *canvasResource();

        // 'SVGClipPathElement' functions
        SVGAnimatedEnumeration *clipPathUnits() const;

        virtual void parseMappedAttribute(MappedAttribute *attr);

    private:
        mutable RefPtr<SVGAnimatedEnumeration> m_clipPathUnits;
        KCanvasClipper *m_clipper;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
