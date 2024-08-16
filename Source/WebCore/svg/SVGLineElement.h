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

class SVGLineElement final : public SVGGeometryElement {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(SVGLineElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SVGLineElement);
public:
    static Ref<SVGLineElement> create(const QualifiedName&, Document&);

    const SVGLengthValue& x1() const { return m_x1->currentValue(); }
    const SVGLengthValue& y1() const { return m_y1->currentValue(); }
    const SVGLengthValue& x2() const { return m_x2->currentValue(); }
    const SVGLengthValue& y2() const { return m_y2->currentValue(); }

    SVGAnimatedLength& x1Animated() { return m_x1; }
    SVGAnimatedLength& y1Animated() { return m_y1; }
    SVGAnimatedLength& x2Animated() { return m_x2; }
    SVGAnimatedLength& y2Animated() { return m_y2; }

private:
    SVGLineElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGLineElement, SVGGeometryElement>;

    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) final;
    void svgAttributeChanged(const QualifiedName&) final;

    bool isValid() const final { return SVGTests::isValid(); }
    bool supportsMarkers() const final { return true; }
    bool selfHasRelativeLengths() const final;

    Ref<SVGAnimatedLength> m_x1 { SVGAnimatedLength::create(this, SVGLengthMode::Width) };
    Ref<SVGAnimatedLength> m_y1 { SVGAnimatedLength::create(this, SVGLengthMode::Height) };
    Ref<SVGAnimatedLength> m_x2 { SVGAnimatedLength::create(this, SVGLengthMode::Width) };
    Ref<SVGAnimatedLength> m_y2 { SVGAnimatedLength::create(this, SVGLengthMode::Height) };
};

} // namespace WebCore
