/*
    Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2007 Eric Seidel <eric@webkit.org>

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

#ifndef SVGAElement_h
#define SVGAElement_h

#if ENABLE(SVG)
#include "SVGExternalResourcesRequired.h"
#include "SVGLangSpace.h"
#include "SVGStyledTransformableElement.h"
#include "SVGTests.h"
#include "SVGURIReference.h"

namespace WebCore {

    class SVGAElement : public SVGStyledTransformableElement,
                        public SVGURIReference,
                        public SVGTests,
                        public SVGLangSpace,
                        public SVGExternalResourcesRequired {
    public:
        SVGAElement(const QualifiedName&, Document*);
        virtual ~SVGAElement();

        virtual bool isValid() const { return SVGTests::isValid(); }
        
        virtual String title() const;

        virtual void parseMappedAttribute(MappedAttribute*);
        virtual void svgAttributeChanged(const QualifiedName&);

        virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

        virtual void defaultEventHandler(Event*);
        
        virtual bool supportsFocus() const;
        virtual bool isMouseFocusable() const;
        virtual bool isKeyboardFocusable(KeyboardEvent*) const;
        virtual bool isFocusable() const;

        virtual bool childShouldCreateRenderer(Node*) const;

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGAElement, SVGNames::aTagString, SVGNames::targetAttrString, String, Target, target)
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGAElement_h
