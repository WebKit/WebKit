/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
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

#include "Color.h"
#include "Filter.h"
#include "FilterEffect.h"

namespace WebCore {

class FEFlood : public FilterEffect {
public:
    static Ref<FEFlood> create(Filter&, const Color&, float);

    const Color& floodColor() const { return m_floodColor; }
    bool setFloodColor(const Color&);

    float floodOpacity() const { return m_floodOpacity; }
    bool setFloodOpacity(float);

#if !USE(CG)
    // feFlood does not perform color interpolation of any kind, so the result is always in the current
    // color space regardless of the value of color-interpolation-filters.
    void setOperatingColorSpace(ColorSpace) override { FilterEffect::setResultColorSpace(ColorSpace::SRGB); }
    void setResultColorSpace(ColorSpace) override { FilterEffect::setResultColorSpace(ColorSpace::SRGB); }
#endif

private:
    FEFlood(Filter&, const Color&, float);

    const char* filterName() const final { return "FEFlood"; }

    void platformApplySoftware() override;

    void determineAbsolutePaintRect() override { setAbsolutePaintRect(enclosingIntRect(maxEffectRect())); }

    WTF::TextStream& externalRepresentation(WTF::TextStream&, RepresentationType) const override;

    Color m_floodColor;
    float m_floodOpacity;
};

} // namespace WebCore

