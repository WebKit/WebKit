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

#include "SVGAnimatedBoolean.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGGraphicsElement.h"
#include "SVGURIReference.h"

namespace WebCore {

class SVGAElement final : public SVGGraphicsElement,
                          public SVGURIReference,
                          public SVGExternalResourcesRequired {
public:
    static Ref<SVGAElement> create(const QualifiedName&, Document&);

private:
    SVGAElement(const QualifiedName&, Document&);

    bool isValid() const final { return SVGTests::isValid(); }
    
    String title() const final;
    String target() const final { return svgTarget(); }

    void parseAttribute(const QualifiedName&, const AtomicString&) final;
    void svgAttributeChanged(const QualifiedName&) final;

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    bool childShouldCreateRenderer(const Node&) const final;

    void defaultEventHandler(Event&) final;
    
    bool supportsFocus() const final;
    bool isMouseFocusable() const final;
    bool isKeyboardFocusable(KeyboardEvent&) const final;
    bool isFocusable() const final;
    bool isURLAttribute(const Attribute&) const final;
    bool canStartSelection() const final;
    int tabIndex() const final;

    bool willRespondToMouseClickEvents() final;

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGAElement)
        // This declaration used to define a non-virtual "String& target() const" method, that clashes with "virtual String Element::target() const".
        // That's why it has been renamed to "svgTarget", the CodeGenerators take care of calling svgTargetAnimated() instead of targetAnimated(), see CodeGenerator.pm.
        DECLARE_ANIMATED_STRING(SVGTarget, svgTarget)
        DECLARE_ANIMATED_STRING_OVERRIDE(Href, href)
        DECLARE_ANIMATED_BOOLEAN_OVERRIDE(ExternalResourcesRequired, externalResourcesRequired)
    END_DECLARE_ANIMATED_PROPERTIES
};

} // namespace WebCore

#endif // SVGAElement_h
