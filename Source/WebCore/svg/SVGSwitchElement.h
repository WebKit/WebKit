/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "SVGExternalResourcesRequired.h"
#include "SVGGraphicsElement.h"

namespace WebCore {

class SVGSwitchElement final : public SVGGraphicsElement, public SVGExternalResourcesRequired {
    WTF_MAKE_ISO_ALLOCATED(SVGSwitchElement);
public:
    static Ref<SVGSwitchElement> create(const QualifiedName&, Document&);

private:
    SVGSwitchElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGSwitchElement, SVGGraphicsElement, SVGExternalResourcesRequired>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    bool isValid() const final { return SVGTests::isValid(); }

    bool childShouldCreateRenderer(const Node&) const final;
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;

    PropertyRegistry m_propertyRegistry { *this };
};

} // namespace WebCore
