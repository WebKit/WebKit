/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SVGAElement_h
#define SVGAElement_h

#if ENABLE(SVG)
#include "SVGAnimatedPropertyMacros.h"
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
        static PassRefPtr<SVGAElement> create(const QualifiedName&, Document*);

    private:
        SVGAElement(const QualifiedName&, Document*);

        virtual bool isValid() const { return SVGTests::isValid(); }
        
        virtual String title() const;
        virtual String target() const { return svgTarget(); }

        virtual void parseMappedAttribute(Attribute*);
        virtual void svgAttributeChanged(const QualifiedName&);
        virtual void synchronizeProperty(const QualifiedName&);

        virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

        virtual void defaultEventHandler(Event*);
        
        virtual bool supportsFocus() const;
        virtual bool isMouseFocusable() const;
        virtual bool isKeyboardFocusable(KeyboardEvent*) const;
        virtual bool isFocusable() const;

        virtual bool childShouldCreateRenderer(Node*) const;

        // This defines a non-virtual "String& target() const" method before, that would clash with "virtual String target() const"
        // in Element. That's why it's now named "String& svgTarget() const", to avoid the clash. The CodeGenerators take care
        // of calling svgTargetAnimated() instead of targetAnimated(), see CodeGenerator.pm.
        DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGAElement, SVGNames::targetAttr, String, SVGTarget, svgTarget)

        // SVGURIReference
        DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGAElement, XLinkNames::hrefAttr, String, Href, href)

        // SVGExternalResourcesRequired
        DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGAElement, SVGNames::externalResourcesRequiredAttr, bool, ExternalResourcesRequired, externalResourcesRequired)
    };

} // namespace WebCore

#endif // ENABLE(SVG)

#endif // SVGAElement_h
