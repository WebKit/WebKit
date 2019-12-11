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

struct RadialGradientAttributes;

class SVGRadialGradientElement final : public SVGGradientElement {
    WTF_MAKE_ISO_ALLOCATED(SVGRadialGradientElement);
public:
    static Ref<SVGRadialGradientElement> create(const QualifiedName&, Document&);

    bool collectGradientAttributes(RadialGradientAttributes&);

    const SVGLengthValue& cx() const { return m_cx->currentValue(); }
    const SVGLengthValue& cy() const { return m_cy->currentValue(); }
    const SVGLengthValue& r() const { return m_r->currentValue(); }
    const SVGLengthValue& fx() const { return m_fx->currentValue(); }
    const SVGLengthValue& fy() const { return m_fy->currentValue(); }
    const SVGLengthValue& fr() const { return m_fr->currentValue(); }

    SVGAnimatedLength& cxAnimated() { return m_cx; }
    SVGAnimatedLength& cyAnimated() { return m_cy; }
    SVGAnimatedLength& rAnimated() { return m_r; }
    SVGAnimatedLength& fxAnimated() { return m_fx; }
    SVGAnimatedLength& fyAnimated() { return m_fy; }
    SVGAnimatedLength& frAnimated() { return m_fr; }

private:
    SVGRadialGradientElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGRadialGradientElement, SVGGradientElement>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    void parseAttribute(const QualifiedName&, const AtomString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;

    bool selfHasRelativeLengths() const override;

    PropertyRegistry m_propertyRegistry { *this };
    Ref<SVGAnimatedLength> m_cx { SVGAnimatedLength::create(this, SVGLengthMode::Width, "50%") };
    Ref<SVGAnimatedLength> m_cy { SVGAnimatedLength::create(this, SVGLengthMode::Height, "50%") };
    Ref<SVGAnimatedLength> m_r { SVGAnimatedLength::create(this, SVGLengthMode::Other, "50%") };
    Ref<SVGAnimatedLength> m_fx { SVGAnimatedLength::create(this, SVGLengthMode::Width) };
    Ref<SVGAnimatedLength> m_fy { SVGAnimatedLength::create(this, SVGLengthMode::Height) };
    Ref<SVGAnimatedLength> m_fr { SVGAnimatedLength::create(this, SVGLengthMode::Other, "0%") };
};

} // namespace WebCore
