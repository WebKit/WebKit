/*
    Copyright (C) 2005 Alexander Kellett <lypanov@kde.org>

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

#ifndef KSVG_SVGMaskElementImpl_H
#define KSVG_SVGMaskElementImpl_H
#if SVG_SUPPORT

#include "SVGTestsImpl.h"
#include "SVGLangSpaceImpl.h"
#include "SVGURIReferenceImpl.h"
#include "SVGStyledLocatableElementImpl.h"
#include "SVGExternalResourcesRequiredImpl.h"

#include "KCanvasResources.h"

class KCanvasImage;

namespace WebCore
{
    class AttributeImpl;
    class SVGAnimatedLengthImpl;
    class SVGMaskElementImpl : public SVGStyledLocatableElementImpl,
                                  public SVGURIReferenceImpl,
                                  public SVGTestsImpl,
                                  public SVGLangSpaceImpl,
                                  public SVGExternalResourcesRequiredImpl
    {
    public:
        SVGMaskElementImpl(const QualifiedName& tagName, DocumentImpl *doc);
        virtual ~SVGMaskElementImpl();
        virtual bool isValid() const { return SVGTestsImpl::isValid(); }

        // 'SVGMaskElement' functions
        SVGAnimatedLengthImpl *x() const;
        SVGAnimatedLengthImpl *y() const;

        SVGAnimatedLengthImpl *width() const;
        SVGAnimatedLengthImpl *height() const;

        virtual void childrenChanged();
        virtual void attributeChanged(AttributeImpl* attr, bool preserveDecls);
        virtual void parseMappedAttribute(MappedAttributeImpl *attr);

        virtual bool rendererIsNeeded(RenderStyle *style) { return StyledElementImpl::rendererIsNeeded(style); }
        virtual RenderObject *createRenderer(RenderArena *arena, RenderStyle *style);
        virtual KCanvasMasker *canvasResource();

    protected:
        KCanvasImage *drawMaskerContent();

        mutable RefPtr<SVGAnimatedLengthImpl> m_x;
        mutable RefPtr<SVGAnimatedLengthImpl> m_y;
        mutable RefPtr<SVGAnimatedLengthImpl> m_width;
        mutable RefPtr<SVGAnimatedLengthImpl> m_height;

    private:
        KCanvasMasker *m_masker;
        bool m_dirty;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
