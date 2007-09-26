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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGMaskElement_h
#define SVGMaskElement_h

#if ENABLE(SVG)

#include "SVGResourceMasker.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGLangSpace.h"
#include "SVGStyledLocatableElement.h"
#include "SVGTests.h"
#include "SVGURIReference.h"

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
        virtual void parseMappedAttribute(MappedAttribute*);
        virtual void notifyAttributeChange() const;

        virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
        virtual SVGResource* canvasResource();

    protected:
        std::auto_ptr<ImageBuffer> drawMaskerContent();

        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGURIReference, String, Href, href)
        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGExternalResourcesRequired, bool, ExternalResourcesRequired, externalResourcesRequired)
 
        ANIMATED_PROPERTY_DECLARATIONS(SVGMaskElement, SVGLength, SVGLength, X, x)
        ANIMATED_PROPERTY_DECLARATIONS(SVGMaskElement, SVGLength, SVGLength, Y, y)
        ANIMATED_PROPERTY_DECLARATIONS(SVGMaskElement, SVGLength, SVGLength, Width, width)
        ANIMATED_PROPERTY_DECLARATIONS(SVGMaskElement, SVGLength, SVGLength, Height, height)

        virtual const SVGElement* contextElement() const { return this; }

    private:
        RefPtr<SVGResourceMasker> m_masker;
        mutable bool m_dirty;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif

// vim:ts=4:noet
