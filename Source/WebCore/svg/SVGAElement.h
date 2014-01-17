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
#include "SVGAnimatedBoolean.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGGraphicsElement.h"
#include "SVGURIReference.h"

namespace WebCore {

class SVGAElement final : public SVGGraphicsElement,
                          public SVGURIReference,
                          public SVGExternalResourcesRequired {
public:
    static PassRefPtr<SVGAElement> create(const QualifiedName&, Document&);

private:
    SVGAElement(const QualifiedName&, Document&);

    virtual bool isValid() const override { return SVGTests::isValid(); }
    
    virtual String title() const override;
    virtual String target() const override { return svgTarget(); }

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;
    virtual void svgAttributeChanged(const QualifiedName&) override;

    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>) override;
    virtual bool childShouldCreateRenderer(const Node&) const override;

    virtual void defaultEventHandler(Event*) override;
    
    virtual bool supportsFocus() const override;
    virtual bool isMouseFocusable() const override;
    virtual bool isKeyboardFocusable(KeyboardEvent*) const override;
    virtual bool isFocusable() const override;
    virtual bool isURLAttribute(const Attribute&) const override;

    virtual bool willRespondToMouseClickEvents() override;

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGAElement)
        // This declaration used to define a non-virtual "String& target() const" method, that clashes with "virtual String Element::target() const".
        // That's why it has been renamed to "svgTarget", the CodeGenerators take care of calling svgTargetAnimated() instead of targetAnimated(), see CodeGenerator.pm.
        DECLARE_ANIMATED_STRING(SVGTarget, svgTarget)
        DECLARE_ANIMATED_STRING(Href, href)
        DECLARE_ANIMATED_BOOLEAN(ExternalResourcesRequired, externalResourcesRequired)
    END_DECLARE_ANIMATED_PROPERTIES
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGAElement_h
