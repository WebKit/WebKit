/*
    Copyright (C) 2006 Apple Computer, Inc.

    This file is part of the WebKit project

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

#ifndef KSVG_SVGForeignObjectElementImpl_H
#define KSVG_SVGForeignObjectElementImpl_H
#if SVG_SUPPORT

#include "SVGTestsImpl.h"
#include "SVGLangSpaceImpl.h"
#include "SVGURIReferenceImpl.h"
#include "SVGStyledTransformableElementImpl.h"
#include "SVGExternalResourcesRequiredImpl.h"

namespace WebCore
{
    class SVGAnimatedLengthImpl;
    class SVGDocumentImpl;

    class SVGForeignObjectElementImpl : public SVGStyledTransformableElementImpl,
                                public SVGTestsImpl,
                                public SVGLangSpaceImpl,
                                public SVGExternalResourcesRequiredImpl,
                                public SVGURIReferenceImpl
    {
    public:
        SVGForeignObjectElementImpl(const QualifiedName& tagName, DocumentImpl *doc);
        virtual ~SVGForeignObjectElementImpl();
        
        virtual bool isValid() const { return SVGTestsImpl::isValid(); }

        SVGAnimatedLengthImpl *x() const;
        SVGAnimatedLengthImpl *y() const;

        SVGAnimatedLengthImpl *width() const;
        SVGAnimatedLengthImpl *height() const;

        virtual void parseMappedAttribute(MappedAttributeImpl *attr);

        virtual bool rendererIsNeeded(RenderStyle *style) { return StyledElementImpl::rendererIsNeeded(style); }
        bool childShouldCreateRenderer(DOM::NodeImpl *child) const;
        virtual RenderObject *createRenderer(RenderArena *arena, RenderStyle *style);

    private:
        mutable RefPtr<SVGAnimatedLengthImpl> m_x;
        mutable RefPtr<SVGAnimatedLengthImpl> m_y;
        mutable RefPtr<SVGAnimatedLengthImpl> m_width;
        mutable RefPtr<SVGAnimatedLengthImpl> m_height;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
