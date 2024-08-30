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
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class SVGStopElement final : public SVGElement {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(SVGStopElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SVGStopElement);
public:
    static Ref<SVGStopElement> create(const QualifiedName&, Document&);

    Color stopColorIncludingOpacity() const;

    float offset() const { return m_offset->currentValue(); }
    SVGAnimatedNumber& offsetAnimated() { return m_offset; }

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGStopElement, SVGElement>;

private:
    SVGStopElement(const QualifiedName&, Document&);

    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) final;
    void svgAttributeChanged(const QualifiedName&) final;

    bool isGradientStop() const final { return true; }

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    bool rendererIsNeeded(const RenderStyle&) final;

    Ref<SVGAnimatedNumber> m_offset { SVGAnimatedNumber::create(this, 0) };
};

} // namespace WebCore
