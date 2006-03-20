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

#include "SVGTests.h"
#include "SVGLangSpace.h"
#include "SVGURIReference.h"
#include "SVGStyledLocatableElement.h"
#include "SVGExternalResourcesRequired.h"

#include "KCanvasResources.h"

class KCanvasImage;

namespace WebCore
{
    class Attribute;
    class SVGAnimatedLength;
    class SVGMaskElement : public SVGStyledLocatableElement,
                                  public SVGURIReference,
                                  public SVGTests,
                                  public SVGLangSpace,
                                  public SVGExternalResourcesRequired
    {
    public:
        SVGMaskElement(const QualifiedName& tagName, Document *doc);
        virtual ~SVGMaskElement();
        virtual bool isValid() const { return SVGTests::isValid(); }

        // 'SVGMaskElement' functions
        SVGAnimatedLength *x() const;
        SVGAnimatedLength *y() const;

        SVGAnimatedLength *width() const;
        SVGAnimatedLength *height() const;

        virtual void childrenChanged();
        virtual void attributeChanged(Attribute* attr, bool preserveDecls);
        virtual void parseMappedAttribute(MappedAttribute *attr);

        virtual bool rendererIsNeeded(RenderStyle *style) { return StyledElement::rendererIsNeeded(style); }
        virtual RenderObject *createRenderer(RenderArena *arena, RenderStyle *style);
        virtual KCanvasMasker *canvasResource();

    protected:
        KCanvasImage *drawMaskerContent();

        mutable RefPtr<SVGAnimatedLength> m_x;
        mutable RefPtr<SVGAnimatedLength> m_y;
        mutable RefPtr<SVGAnimatedLength> m_width;
        mutable RefPtr<SVGAnimatedLength> m_height;

    private:
        KCanvasMasker *m_masker;
        bool m_dirty;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
