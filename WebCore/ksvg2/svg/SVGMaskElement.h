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
#ifdef SVG_SUPPORT

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
    class SVGLength;
    class SVGMaskElement : public SVGStyledLocatableElement,
                                  public SVGURIReference,
                                  public SVGTests,
                                  public SVGLangSpace,
                                  public SVGExternalResourcesRequired
    {
    public:
        SVGMaskElement(const QualifiedName&, Document*);
        virtual ~SVGMaskElement();
        virtual bool isValid() const { return SVGTests::isValid(); }

        // 'SVGMaskElement' functions
        virtual void childrenChanged();
        virtual void attributeChanged(Attribute* attr, bool preserveDecls);
        virtual void parseMappedAttribute(MappedAttribute *attr);

        virtual bool rendererIsNeeded(RenderStyle *style) { return StyledElement::rendererIsNeeded(style); }
        virtual RenderObject *createRenderer(RenderArena *arena, RenderStyle *style);
        virtual KCanvasMasker *canvasResource();

    protected:
        KCanvasImage *drawMaskerContent();

        ANIMATED_PROPERTY_DECLARATIONS(SVGLength*, RefPtr<SVGLength>, X, x)
        ANIMATED_PROPERTY_DECLARATIONS(SVGLength*, RefPtr<SVGLength>, Y, y)
        ANIMATED_PROPERTY_DECLARATIONS(SVGLength*, RefPtr<SVGLength>, Width, width)
        ANIMATED_PROPERTY_DECLARATIONS(SVGLength*, RefPtr<SVGLength>, Height, height)

        virtual const SVGElement* contextElement() const { return this; }

    private:
        KCanvasMasker *m_masker;
        bool m_dirty;
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
