/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "SVGAnimatedLengthList.h"
#include "SVGAnimatedNumberList.h"
#include "SVGTextContentElement.h"

namespace WebCore {

class SVGTextPositioningElement : public SVGTextContentElement {
    WTF_MAKE_ISO_ALLOCATED(SVGTextPositioningElement);
public:
    static SVGTextPositioningElement* elementFromRenderer(RenderBoxModelObject&);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGTextPositioningElement, SVGTextContentElement>;
    static AttributeOwnerProxy::AttributeRegistry& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }

    const SVGLengthListValues& x() const { return m_x.currentValue(attributeOwnerProxy()); }
    const SVGLengthListValues& y() const { return m_y.currentValue(attributeOwnerProxy()); }
    const SVGLengthListValues& dx() const { return m_dx.currentValue(attributeOwnerProxy()); }
    const SVGLengthListValues& dy() const { return m_dy.currentValue(attributeOwnerProxy()); }
    const SVGNumberListValues& rotate() const { return m_rotate.currentValue(attributeOwnerProxy()); }

    RefPtr<SVGAnimatedLengthList> xAnimated() { return m_x.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLengthList> yAnimated() { return m_y.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLengthList> dxAnimated() { return m_dx.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLengthList> dyAnimated() { return m_dy.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumberList> rotateAnimated() { return m_rotate.animatedProperty(attributeOwnerProxy()); }

protected:
    SVGTextPositioningElement(const QualifiedName&, Document&);

    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

private:
    bool isPresentationAttribute(const QualifiedName&) const final;
    void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStyleProperties&) final;

    const SVGAttributeOwnerProxy& attributeOwnerProxy() const override { return m_attributeOwnerProxy; }

    static void registerAttributes();
    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }

    AttributeOwnerProxy m_attributeOwnerProxy { *this };
    SVGAnimatedLengthListAttribute m_x;
    SVGAnimatedLengthListAttribute m_y;
    SVGAnimatedLengthListAttribute m_dx;
    SVGAnimatedLengthListAttribute m_dy;
    SVGAnimatedNumberListAttribute m_rotate;
};

} // namespace WebCore
