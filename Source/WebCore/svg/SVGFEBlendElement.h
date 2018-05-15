/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
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

#include "GraphicsTypes.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {

template<>
struct SVGPropertyTraits<BlendMode> {
    static unsigned highestEnumValue() { return BlendModeLuminosity; }

    static String toString(BlendMode type)
    {
        if (type < BlendModePlusDarker)
            return blendModeName(type);

        return emptyString();
    }
};

class SVGFEBlendElement final : public SVGFilterPrimitiveStandardAttributes {
    WTF_MAKE_ISO_ALLOCATED(SVGFEBlendElement);
public:
    static Ref<SVGFEBlendElement> create(const QualifiedName&, Document&);

private:
    SVGFEBlendElement(const QualifiedName&, Document&);

    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    bool setFilterEffectAttribute(FilterEffect*, const QualifiedName& attrName) override;
    void svgAttributeChanged(const QualifiedName&) override;
    RefPtr<FilterEffect> build(SVGFilterBuilder*, Filter&) override;

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGFEBlendElement)
        DECLARE_ANIMATED_STRING(In1, in1)
        DECLARE_ANIMATED_STRING(In2, in2)
        DECLARE_ANIMATED_ENUMERATION(Mode, mode, BlendMode)
    END_DECLARE_ANIMATED_PROPERTIES
};

} // namespace WebCore
