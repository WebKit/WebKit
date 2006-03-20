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

#ifndef KSVG_SVGFilterElementImpl_H
#define KSVG_SVGFilterElementImpl_H
#if SVG_SUPPORT

#include "SVGLangSpace.h"
#include "SVGURIReference.h"
#include "SVGStyledElement.h"
#include "SVGExternalResourcesRequired.h"

#include "KCanvasFilters.h"

namespace WebCore {
    class SVGAnimatedEnumeration;
    class SVGAnimatedInteger;
    class SVGAnimatedLength;

    class SVGFilterElement : public SVGStyledElement,
                                 public SVGURIReference,
                                 public SVGLangSpace,
                                 public SVGExternalResourcesRequired
    {
    public:
        SVGFilterElement(const QualifiedName& tagName, Document *doc);
        virtual ~SVGFilterElement();

        virtual KCanvasFilter *canvasResource();

        // 'SVGFilterElement' functions
        SVGAnimatedEnumeration *filterUnits() const;
        SVGAnimatedEnumeration *primitiveUnits() const;

        SVGAnimatedLength *x() const;
        SVGAnimatedLength *y() const;

        SVGAnimatedLength *width() const;
        SVGAnimatedLength *height() const;

        SVGAnimatedInteger *filterResX() const;
        SVGAnimatedInteger *filterResY() const;

        void setFilterRes(unsigned long filterResX, unsigned long filterResY) const;

        virtual void parseMappedAttribute(MappedAttribute *attr);

    private:
        mutable RefPtr<SVGAnimatedEnumeration> m_filterUnits;
        mutable RefPtr<SVGAnimatedEnumeration> m_primitiveUnits;
        mutable RefPtr<SVGAnimatedLength> m_x;
        mutable RefPtr<SVGAnimatedLength> m_y;
        mutable RefPtr<SVGAnimatedLength> m_width;
        mutable RefPtr<SVGAnimatedLength> m_height;
        mutable RefPtr<SVGAnimatedInteger> m_filterResX;
        mutable RefPtr<SVGAnimatedInteger> m_filterResY;
        KCanvasFilter *m_filter;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
