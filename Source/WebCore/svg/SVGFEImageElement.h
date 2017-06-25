/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

class SVGFEImageElement final : public SVGFilterPrimitiveStandardAttributes,
                                public SVGURIReference,
                                public SVGExternalResourcesRequired,
                                public CachedImageClient {
public:
    static Ref<SVGFEImageElement> create(const QualifiedName&, Document&);

    virtual ~SVGFEImageElement();

    bool hasSingleSecurityOrigin() const;

private:
    SVGFEImageElement(const QualifiedName&, Document&);

    void finishedInsertingSubtree() override;

    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void svgAttributeChanged(const QualifiedName&) override;
    void notifyFinished(CachedResource&) final;

    void addSubresourceAttributeURLs(ListHashSet<URL>&) const override;
    RefPtr<FilterEffect> build(SVGFilterBuilder*, Filter&) override;

    void clearResourceReferences();
    void requestImageResource();

    void buildPendingResource() override;
    InsertionNotificationRequest insertedInto(ContainerNode&) override;
    void removedFrom(ContainerNode&) override;

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGFEImageElement)
        DECLARE_ANIMATED_PRESERVEASPECTRATIO(PreserveAspectRatio, preserveAspectRatio)
        DECLARE_ANIMATED_STRING_OVERRIDE(Href, href)
        DECLARE_ANIMATED_BOOLEAN_OVERRIDE(ExternalResourcesRequired, externalResourcesRequired)
    END_DECLARE_ANIMATED_PROPERTIES

    CachedResourceHandle<CachedImage> m_cachedImage;
};

} // namespace WebCore
