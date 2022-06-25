/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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
#include "SVGNames.h"
#include "SVGURIReference.h"

namespace WebCore {
    
class SVGPathElement;

class SVGMPathElement final : public SVGElement, public SVGURIReference {
    WTF_MAKE_ISO_ALLOCATED(SVGMPathElement);
public:
    static Ref<SVGMPathElement> create(const QualifiedName&, Document&);

    virtual ~SVGMPathElement();

    RefPtr<SVGPathElement> pathElement();

    void targetPathChanged();

private:
    SVGMPathElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGMPathElement, SVGElement, SVGURIReference>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    void parseAttribute(const QualifiedName&, const AtomString&) final;
    void svgAttributeChanged(const QualifiedName&) final;

    void buildPendingResource() final;
    void clearResourceReferences();
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) final;
    void removedFromAncestor(RemovalType, ContainerNode&) final;

    bool rendererIsNeeded(const RenderStyle&) final { return false; }
    void didFinishInsertingNode() final;

    void notifyParentOfPathChange(ContainerNode*);

    PropertyRegistry m_propertyRegistry { *this };
};

} // namespace WebCore
