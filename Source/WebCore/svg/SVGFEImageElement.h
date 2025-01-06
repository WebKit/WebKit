/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SVGURIReference.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class SVGFEImageElement final : public SVGFilterPrimitiveStandardAttributes, public SVGURIReference, private CachedImageClient {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(SVGFEImageElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SVGFEImageElement);
public:
    static Ref<SVGFEImageElement> create(const QualifiedName&, Document&);

    virtual ~SVGFEImageElement();

    // CheckedPtr interface
    uint32_t checkedPtrCount() const { return SVGFilterPrimitiveStandardAttributes::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const { return SVGFilterPrimitiveStandardAttributes::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const { SVGFilterPrimitiveStandardAttributes::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const { SVGFilterPrimitiveStandardAttributes::decrementCheckedPtrCount(); }

    bool renderingTaintsOrigin() const;

    const SVGPreserveAspectRatioValue& preserveAspectRatio() const { return m_preserveAspectRatio->currentValue(); }
    SVGAnimatedPreserveAspectRatio& preserveAspectRatioAnimated() { return m_preserveAspectRatio; }

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGFEImageElement, SVGFilterPrimitiveStandardAttributes, SVGURIReference>;

private:
    SVGFEImageElement(const QualifiedName&, Document&);

    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) override;
    void svgAttributeChanged(const QualifiedName&) override;

    void notifyFinished(CachedResource&, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess) final;
    void addSubresourceAttributeURLs(ListHashSet<URL>&) const override;

    void didFinishInsertingNode() override;

    std::tuple<RefPtr<ImageBuffer>, FloatRect> imageBufferForEffect(const GraphicsContext& destinationContext) const;

    RefPtr<FilterEffect> createFilterEffect(const FilterEffectVector&, const GraphicsContext& destinationContext) const override;

    void clearResourceReferences();
    void requestImageResource();

    void buildPendingResource() override;
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) override;
    void removedFromAncestor(RemovalType, ContainerNode&) override;

    Ref<SVGAnimatedPreserveAspectRatio> m_preserveAspectRatio { SVGAnimatedPreserveAspectRatio::create(this) };
    CachedResourceHandle<CachedImage> m_cachedImage;
};

} // namespace WebCore
