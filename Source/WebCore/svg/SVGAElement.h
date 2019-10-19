/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#pragma once

#include "SVGGraphicsElement.h"
#include "SVGURIReference.h"
#include "SharedStringHash.h"

namespace WebCore {

class SVGAElement final : public SVGGraphicsElement, public SVGURIReference {
    WTF_MAKE_ISO_ALLOCATED(SVGAElement);
public:
    static Ref<SVGAElement> create(const QualifiedName&, Document&);

    String target() const final { return m_target->currentValue(); }
    Ref<SVGAnimatedString>& targetAnimated() { return m_target; }

    SharedStringHash visitedLinkHash() const;

private:
    SVGAElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGAElement, SVGGraphicsElement, SVGURIReference>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    void parseAttribute(const QualifiedName&, const AtomString&) final;
    void svgAttributeChanged(const QualifiedName&) final;

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    bool childShouldCreateRenderer(const Node&) const final;

    bool isValid() const final { return SVGTests::isValid(); }
    String title() const final;
    void defaultEventHandler(Event&) final;
    
    bool supportsFocus() const final;
    bool isMouseFocusable() const final;
    bool isKeyboardFocusable(KeyboardEvent*) const final;
    bool isURLAttribute(const Attribute&) const final;
    bool canStartSelection() const final;
    int defaultTabIndex() const final;

    bool willRespondToMouseClickEvents() final;

    PropertyRegistry m_propertyRegistry { *this };
    Ref<SVGAnimatedString> m_target { SVGAnimatedString::create(this) };

    // This is computed only once and must not be affected by subsequent URL changes.
    mutable Optional<SharedStringHash> m_storedVisitedLinkHash;
};

} // namespace WebCore
