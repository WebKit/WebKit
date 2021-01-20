/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2008 Rob Buis <buis@kde.org>
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

#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {

class SVGFEFloodElement final : public SVGFilterPrimitiveStandardAttributes {
    WTF_MAKE_ISO_ALLOCATED(SVGFEFloodElement);
public:
    static Ref<SVGFEFloodElement> create(const QualifiedName&, Document&);

private:
    SVGFEFloodElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGFEFloodElement, SVGFilterPrimitiveStandardAttributes>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    bool setFilterEffectAttribute(FilterEffect*, const QualifiedName& attrName) override;
    RefPtr<FilterEffect> build(SVGFilterBuilder*, Filter&) const override;

    PropertyRegistry m_propertyRegistry { *this };
};

} // namespace WebCore
