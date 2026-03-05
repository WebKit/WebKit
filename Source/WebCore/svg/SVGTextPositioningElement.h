/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
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

#include "SVGTextContentElement.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class SVGTextPositioningElement : public SVGTextContentElement {
    WTF_MAKE_TZONE_ALLOCATED(SVGTextPositioningElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SVGTextPositioningElement);
public:
    static RefPtr<SVGTextPositioningElement> elementFromRenderer(RenderBoxModelObject&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGTextPositioningElement, SVGTextContentElement>;

    const SVGLengthList& x() const LIFETIME_BOUND { return m_x->currentValue(); }
    const SVGLengthList& y() const LIFETIME_BOUND { return m_y->currentValue(); }
    const SVGLengthList& dx() const LIFETIME_BOUND { return m_dx->currentValue(); }
    const SVGLengthList& dy() const LIFETIME_BOUND { return m_dy->currentValue(); }
    const SVGNumberList& rotate() const LIFETIME_BOUND { return m_rotate->currentValue(); }

    SVGAnimatedLengthList& xAnimated() { return m_x; }
    SVGAnimatedLengthList& yAnimated() { return m_y; }
    SVGAnimatedLengthList& dxAnimated() { return m_dx; }
    SVGAnimatedLengthList& dyAnimated() { return m_dy; }
    SVGAnimatedNumberList& rotateAnimated() { return m_rotate; }

protected:
    SVGTextPositioningElement(const QualifiedName&, Document&, UniqueRef<SVGPropertyRegistry>&&);

    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) override;
    void svgAttributeChanged(const QualifiedName&) override;

private:
    bool hasPresentationalHintsForAttribute(const QualifiedName&) const final;
    void collectPresentationalHintsForAttribute(const QualifiedName&, const AtomString&, MutableStyleProperties&) final;

    bool isSVGTextPositioningElement() const override { return true; }

    const Ref<SVGAnimatedLengthList> m_x { SVGAnimatedLengthList::create(this, SVGLengthMode::Width) };
    const Ref<SVGAnimatedLengthList> m_y { SVGAnimatedLengthList::create(this, SVGLengthMode::Height) };
    const Ref<SVGAnimatedLengthList> m_dx { SVGAnimatedLengthList::create(this, SVGLengthMode::Width) };
    const Ref<SVGAnimatedLengthList> m_dy { SVGAnimatedLengthList::create(this, SVGLengthMode::Height) };
    const Ref<SVGAnimatedNumberList> m_rotate { SVGAnimatedNumberList::create(this) };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SVGTextPositioningElement)
    static bool isType(const WebCore::SVGElement& element) { return element.isSVGTextPositioningElement(); }
    static bool isType(const WebCore::Node& node)
    {
        auto* svgElement = dynamicDowncast<WebCore::SVGElement>(node);
        return svgElement && isType(*svgElement);
    }
SPECIALIZE_TYPE_TRAITS_END()
