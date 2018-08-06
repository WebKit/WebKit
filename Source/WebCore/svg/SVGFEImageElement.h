/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

#include "CachedImageClient.h"
#include "CachedResourceHandle.h"
#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGFEImage.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SVGURIReference.h"

namespace WebCore {

class SVGFEImageElement final : public SVGFilterPrimitiveStandardAttributes, public SVGExternalResourcesRequired, public SVGURIReference, public CachedImageClient {
    WTF_MAKE_ISO_ALLOCATED(SVGFEImageElement);
public:
    static Ref<SVGFEImageElement> create(const QualifiedName&, Document&);

    virtual ~SVGFEImageElement();

    bool hasSingleSecurityOrigin() const;

    const SVGPreserveAspectRatioValue& preserveAspectRatio() const { return m_preserveAspectRatio.currentValue(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedPreserveAspectRatio> preserveAspectRatioAnimated() { return m_preserveAspectRatio.animatedProperty(attributeOwnerProxy()); }

private:
    SVGFEImageElement(const QualifiedName&, Document&);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGFEImageElement, SVGFilterPrimitiveStandardAttributes, SVGExternalResourcesRequired, SVGURIReference>;
    static AttributeOwnerProxy::AttributeRegistry& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }
    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }
    static void registerAttributes();

    const SVGAttributeOwnerProxy& attributeOwnerProxy() const final { return m_attributeOwnerProxy; }
    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    void notifyFinished(CachedResource&) final;
    void addSubresourceAttributeURLs(ListHashSet<URL>&) const override;

    void didFinishInsertingNode() override;

    RefPtr<FilterEffect> build(SVGFilterBuilder*, Filter&) override;

    void clearResourceReferences();
    void requestImageResource();

    void buildPendingResource() override;
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) override;
    void removedFromAncestor(RemovalType, ContainerNode&) override;

    AttributeOwnerProxy m_attributeOwnerProxy { *this };
    SVGAnimatedPreserveAspectRatioAttribute m_preserveAspectRatio;
    CachedResourceHandle<CachedImage> m_cachedImage;
};

} // namespace WebCore
