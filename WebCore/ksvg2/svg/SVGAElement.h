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

#ifndef KSVG_SVGAElementImpl_H
#define KSVG_SVGAElementImpl_H
#if SVG_SUPPORT

#include "SVGTests.h"
#include "SVGLangSpace.h"
#include "SVGURIReference.h"
#include "SVGStyledTransformableElement.h"
#include "SVGExternalResourcesRequired.h"

namespace WebCore
{
    class SVGAnimatedString;
    class SVGAElement : public SVGStyledTransformableElement,
                            public SVGURIReference,
                            public SVGTests,
                            public SVGLangSpace,
                            public SVGExternalResourcesRequired
    {
    public:
        SVGAElement(const QualifiedName& tagName, Document *doc);
        virtual ~SVGAElement();

        // 'SVGAElement' functions
        SVGAnimatedString *target() const;
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        virtual void parseMappedAttribute(MappedAttribute *attr);

        // Derived from: 'SVGStyledElement'
        virtual bool rendererIsNeeded(RenderStyle *style) { return StyledElement::rendererIsNeeded(style); }
        virtual RenderObject *createRenderer(RenderArena *arena, RenderStyle *style);

        virtual void defaultEventHandler(Event *evt);

    private:
        mutable RefPtr<SVGAnimatedString> m_target;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
