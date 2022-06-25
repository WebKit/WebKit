/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "SVGGeometryElement.h"
#include "SVGNames.h"

namespace WebCore {

class SVGEllipseElement final : public SVGGeometryElement {
    WTF_MAKE_ISO_ALLOCATED(SVGEllipseElement);
public:
    static Ref<SVGEllipseElement> create(const QualifiedName&, Document&);

    const SVGLengthValue& cx() const { return m_cx->currentValue(); }
    const SVGLengthValue& cy() const { return m_cy->currentValue(); }
    const SVGLengthValue& rx() const { return m_rx->currentValue(); }
    const SVGLengthValue& ry() const { return m_ry->currentValue(); }

    SVGAnimatedLength& cxAnimated() { return m_cx; }
    SVGAnimatedLength& cyAnimated() { return m_cy; }
    SVGAnimatedLength& rxAnimated() { return m_rx; }
    SVGAnimatedLength& ryAnimated() { return m_ry; }

private:
    SVGEllipseElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGEllipseElement, SVGGeometryElement>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    void parseAttribute(const QualifiedName&, const AtomString&) final;
    void svgAttributeChanged(const QualifiedName&) final;

    bool isValid() const final { return SVGTests::isValid(); }
    bool selfHasRelativeLengths() const final { return true; }

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;

    PropertyRegistry m_propertyRegistry { *this };
    Ref<SVGAnimatedLength> m_cx { SVGAnimatedLength::create(this, SVGLengthMode::Width) };
    Ref<SVGAnimatedLength> m_cy { SVGAnimatedLength::create(this, SVGLengthMode::Height) };
    Ref<SVGAnimatedLength> m_rx { SVGAnimatedLength::create(this, SVGLengthMode::Width) };
    Ref<SVGAnimatedLength> m_ry { SVGAnimatedLength::create(this, SVGLengthMode::Height) };
};

} // namespace WebCore
