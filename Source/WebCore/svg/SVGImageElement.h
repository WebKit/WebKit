/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "SVGAnimatedLength.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGGraphicsElement.h"
#include "SVGImageLoader.h"
#include "SVGURIReference.h"

namespace WebCore {

class SVGImageElement final : public SVGGraphicsElement, public SVGExternalResourcesRequired, public SVGURIReference {
    WTF_MAKE_ISO_ALLOCATED(SVGImageElement);
public:
    static Ref<SVGImageElement> create(const QualifiedName&, Document&);

    bool hasSingleSecurityOrigin() const;
    const AtomicString& imageSourceURL() const final;

    const SVGLengthValue& x() const { return m_x.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& y() const { return m_y.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& width() const { return m_width.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& height() const { return m_height.currentValue(attributeOwnerProxy()); }
    const SVGPreserveAspectRatioValue& preserveAspectRatio() const { return m_preserveAspectRatio.currentValue(attributeOwnerProxy()); }

    RefPtr<SVGAnimatedLength> xAnimated() { return m_x.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> yAnimated() { return m_y.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> widthAnimated() { return m_width.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> heightAnimated() { return m_height.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedPreserveAspectRatio> preserveAspectRatioAnimated() { return m_preserveAspectRatio.animatedProperty(attributeOwnerProxy()); }

private:
    SVGImageElement(const QualifiedName&, Document&);
    
    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGImageElement, SVGGraphicsElement, SVGExternalResourcesRequired, SVGURIReference>;
    static AttributeOwnerProxy::AttributeRegistry& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }
    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }
    static void registerAttributes();

    const SVGAttributeOwnerProxy& attributeOwnerProxy() const final { return m_attributeOwnerProxy; }
    void parseAttribute(const QualifiedName&, const AtomicString&) final;
    void svgAttributeChanged(const QualifiedName&) final;

    void didAttachRenderers() final;
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) final;
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;

    void addSubresourceAttributeURLs(ListHashSet<URL>&) const final;
    bool haveLoadedRequiredResources() final;

    bool isValid() const final { return SVGTests::isValid(); }
    bool selfHasRelativeLengths() const final { return true; }
    void didMoveToNewDocument(Document& oldDocument, Document& newDocument) final;

    AttributeOwnerProxy m_attributeOwnerProxy { *this };
    SVGAnimatedLengthAttribute m_x { LengthModeWidth };
    SVGAnimatedLengthAttribute m_y { LengthModeHeight };
    SVGAnimatedLengthAttribute m_width { LengthModeWidth };
    SVGAnimatedLengthAttribute m_height { LengthModeHeight };
    SVGAnimatedPreserveAspectRatioAttribute m_preserveAspectRatio;
    SVGImageLoader m_imageLoader;
};

} // namespace WebCore
