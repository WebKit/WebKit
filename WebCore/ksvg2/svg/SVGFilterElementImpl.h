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

#include "SVGLangSpaceImpl.h"
#include "SVGURIReferenceImpl.h"
#include "SVGStyledElementImpl.h"
#include "SVGExternalResourcesRequiredImpl.h"

#include "KCanvasFilters.h"

namespace KSVG
{
    class SVGAnimatedEnumerationImpl;
    class SVGAnimatedIntegerImpl;
    class SVGAnimatedLengthImpl;

    class SVGFilterElementImpl : public SVGStyledElementImpl,
                                 public SVGURIReferenceImpl,
                                 public SVGLangSpaceImpl,
                                 public SVGExternalResourcesRequiredImpl
    {
    public:
        SVGFilterElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc);
        virtual ~SVGFilterElementImpl();

        virtual KCanvasFilter *canvasResource();

        // 'SVGFilterElement' functions
        SVGAnimatedEnumerationImpl *filterUnits() const;
        SVGAnimatedEnumerationImpl *primitiveUnits() const;

        SVGAnimatedLengthImpl *x() const;
        SVGAnimatedLengthImpl *y() const;

        SVGAnimatedLengthImpl *width() const;
        SVGAnimatedLengthImpl *height() const;

        SVGAnimatedIntegerImpl *filterResX() const;
        SVGAnimatedIntegerImpl *filterResY() const;

        void setFilterRes(unsigned long filterResX, unsigned long filterResY) const;

        virtual void parseMappedAttribute(KDOM::MappedAttributeImpl *attr);

    private:
        mutable SVGAnimatedEnumerationImpl *m_filterUnits;
        mutable SVGAnimatedEnumerationImpl *m_primitiveUnits;
        mutable SVGAnimatedLengthImpl *m_x;
        mutable SVGAnimatedLengthImpl *m_y;
        mutable SVGAnimatedLengthImpl *m_width;
        mutable SVGAnimatedLengthImpl *m_height;
        mutable SVGAnimatedIntegerImpl *m_filterResX;
        mutable SVGAnimatedIntegerImpl *m_filterResY;
        KCanvasFilter *m_filter;
    };
};

#endif

// vim:ts=4:noet
