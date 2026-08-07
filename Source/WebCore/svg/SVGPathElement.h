/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#include "Path.h"
#include "SVGGeometryElement.h"
#include "SVGNames.h"
#include "SVGPathByteStream.h"
#include "SVGPathSegList.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class SVGPoint;

class SVGPathElement final : public SVGGeometryElement {
    WTF_MAKE_TZONE_ALLOCATED(SVGPathElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SVGPathElement);
public:
    static Ref<SVGPathElement> create(const QualifiedName&, Document&);

    float getTotalLength() const final;
    ExceptionOr<Ref<SVGPoint>> getPointAtLength(float distance) const final;
    unsigned getPathSegAtLength(float distance) const;

    FloatRect getBBox(StyleUpdateStrategy = StyleUpdateStrategy::Allow) final;

    const SVGPathByteStream& pathByteStream() const;
    Path path() const;
    size_t approximateMemoryCost() const final { return m_pathSegList->approximateMemoryCost(); }

    void pathDidChange();

    static void clearCache();

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGPathElement, SVGGeometryElement>;

private:
    SVGPathElement(const QualifiedName&, Document&);

    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) final;
    void svgAttributeChanged(const QualifiedName&) final;

    bool isValid() const final { return SVGTests::isValid(); }
    bool supportsMarkers() const final { return true; }

    RenderPtr<RenderElement> createElementRenderer(Style::ComputedStyle&&, const RenderTreePosition&) final;

    Node::NeedsPostConnectionSteps insertionSteps(InsertionType, ContainerNode&) final;
    void removingSteps(RemovalType, ContainerNode&) final;

    void invalidateMPathDependencies();

    void collectPresentationalHintsForAttribute(const QualifiedName&, const AtomString&, MutableStyleProperties&) final;
    void collectExtraStyleForPresentationalHints(MutableStyleProperties&) override;
    void collectDPresentationalHint(MutableStyleProperties&);

    const Ref<SVGAnimatedPathSegList> m_pathSegList { SVGAnimatedPathSegList::create(this) };
};

} // namespace WebCore
