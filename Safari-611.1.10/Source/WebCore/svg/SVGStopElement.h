/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "SVGElement.h"

namespace WebCore {

class SVGStopElement final : public SVGElement {
    WTF_MAKE_ISO_ALLOCATED(SVGStopElement);
public:
    static Ref<SVGStopElement> create(const QualifiedName&, Document&);

    Color stopColorIncludingOpacity() const;

    float offset() const { return m_offset->currentValue(); }
    SVGAnimatedNumber& offsetAnimated() { return m_offset; }

private:
    SVGStopElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGStopElement, SVGElement>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    void parseAttribute(const QualifiedName&, const AtomString&) final;
    void svgAttributeChanged(const QualifiedName&) final;

    bool isGradientStop() const final { return true; }

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    bool rendererIsNeeded(const RenderStyle&) final;

    PropertyRegistry m_propertyRegistry { *this };
    Ref<SVGAnimatedNumber> m_offset { SVGAnimatedNumber::create(0) };
};

} // namespace WebCore
