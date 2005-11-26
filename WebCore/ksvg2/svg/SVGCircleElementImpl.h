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

#include "SVGTestsImpl.h"
#include "SVGLangSpaceImpl.h"
#include "SVGStyledTransformableElementImpl.h"
#include "SVGExternalResourcesRequiredImpl.h"

namespace KSVG
{
    class SVGAnimatedLengthImpl;
    class SVGCircleElementImpl : public SVGStyledTransformableElementImpl,
                                 public SVGTestsImpl,
                                 public SVGLangSpaceImpl,
                                 public SVGExternalResourcesRequiredImpl
    {
    public:
        SVGCircleElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc);
        virtual ~SVGCircleElementImpl();
        
        virtual bool isValid() const { return SVGTestsImpl::isValid(); }

        // 'SVGCircleElement' functions
        SVGAnimatedLengthImpl *cx() const;
        SVGAnimatedLengthImpl *cy() const;
        SVGAnimatedLengthImpl *r() const;

        virtual void parseMappedAttribute(KDOM::MappedAttributeImpl *attr);

        virtual bool rendererIsNeeded(khtml::RenderStyle *) { return true; }
        virtual KCPathDataList toPathData() const;

        virtual const SVGStyledElementImpl *pushAttributeContext(const SVGStyledElementImpl *context);

    private:
        mutable SharedPtr<SVGAnimatedLengthImpl> m_cx;
        mutable SharedPtr<SVGAnimatedLengthImpl> m_cy;
        mutable SharedPtr<SVGAnimatedLengthImpl> m_r;
    };
};

#endif

// vim:ts=4:noet
