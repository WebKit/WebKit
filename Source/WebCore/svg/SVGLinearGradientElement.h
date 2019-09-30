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

#include "SVGGradientElement.h"
#include "SVGNames.h"

namespace WebCore {

struct LinearGradientAttributes;

class SVGLinearGradientElement final : public SVGGradientElement {
    WTF_MAKE_ISO_ALLOCATED(SVGLinearGradientElement);
public:
    static Ref<SVGLinearGradientElement> create(const QualifiedName&, Document&);

    bool collectGradientAttributes(LinearGradientAttributes&);

    const SVGLengthValue& x1() const { return m_x1->currentValue(); }
    const SVGLengthValue& y1() const { return m_y1->currentValue(); }
    const SVGLengthValue& x2() const { return m_x2->currentValue(); }
    const SVGLengthValue& y2() const { return m_y2->currentValue(); }

    SVGAnimatedLength& x1Animated() { return m_x1; }
    SVGAnimatedLength& y1Animated() { return m_y1; }
    SVGAnimatedLength& x2Animated() { return m_x2; }
    SVGAnimatedLength& y2Animated() { return m_y2; }

private:
    SVGLinearGradientElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGLinearGradientElement, SVGGradientElement>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    void parseAttribute(const QualifiedName&, const AtomString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;

    bool selfHasRelativeLengths() const override;

    PropertyRegistry m_propertyRegistry { *this };
    Ref<SVGAnimatedLength> m_x1 { SVGAnimatedLength::create(this, SVGLengthMode::Width) };
    Ref<SVGAnimatedLength> m_y1 { SVGAnimatedLength::create(this, SVGLengthMode::Height) };
    Ref<SVGAnimatedLength> m_x2 { SVGAnimatedLength::create(this, SVGLengthMode::Width, "100%") };
    Ref<SVGAnimatedLength> m_y2 { SVGAnimatedLength::create(this, SVGLengthMode::Height) };
};

} // namespace WebCore
