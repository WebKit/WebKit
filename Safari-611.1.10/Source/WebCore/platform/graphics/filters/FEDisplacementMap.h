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

#include "FilterEffect.h"
#include "Filter.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

enum ChannelSelectorType {
    CHANNEL_UNKNOWN = 0,
    CHANNEL_R = 1,
    CHANNEL_G = 2,
    CHANNEL_B = 3,
    CHANNEL_A = 4
};

class FEDisplacementMap : public FilterEffect {
public:
    static Ref<FEDisplacementMap> create(Filter&, ChannelSelectorType xChannelSelector, ChannelSelectorType yChannelSelector, float scale);

    ChannelSelectorType xChannelSelector() const { return m_xChannelSelector; }
    bool setXChannelSelector(const ChannelSelectorType);

    ChannelSelectorType yChannelSelector() const { return m_yChannelSelector; }
    bool setYChannelSelector(const ChannelSelectorType);

    float scale() const { return m_scale; }
    bool setScale(float);

    void setResultColorSpace(ColorSpace) override;
    void transformResultColorSpace(FilterEffect*, const int) override;

private:
    FEDisplacementMap(Filter&, ChannelSelectorType xChannelSelector, ChannelSelectorType yChannelSelector, float);

    const char* filterName() const final { return "FEDisplacementMap"; }

    void platformApplySoftware() override;

    void determineAbsolutePaintRect() override { setAbsolutePaintRect(enclosingIntRect(maxEffectRect())); }

    int xChannelIndex() const { return m_xChannelSelector - 1; }
    int yChannelIndex() const { return m_yChannelSelector - 1; }

    WTF::TextStream& externalRepresentation(WTF::TextStream&, RepresentationType) const override;

    ChannelSelectorType m_xChannelSelector;
    ChannelSelectorType m_yChannelSelector;
    float m_scale;
};

} // namespace WebCore

