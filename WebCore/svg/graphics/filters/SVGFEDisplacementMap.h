/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric@webkit.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGFEDisplacementMap_h
#define SVGFEDisplacementMap_h

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "PlatformString.h"
#include "FilterEffect.h"

namespace WebCore {

    enum ChannelSelectorType {
        CHANNEL_UNKNOWN = 0,
        CHANNEL_R       = 1,
        CHANNEL_G       = 2,
        CHANNEL_B       = 3,
        CHANNEL_A       = 4
    };
    
    class FEDisplacementMap : public FilterEffect {
    public:
        static PassRefPtr<FEDisplacementMap> create(FilterEffect*, FilterEffect*, ChannelSelectorType,
                ChannelSelectorType, const float&);

        ChannelSelectorType xChannelSelector() const;
        void setXChannelSelector(const ChannelSelectorType);

        ChannelSelectorType yChannelSelector() const;
        void setYChannelSelector(const ChannelSelectorType);

        float scale() const;
        void setScale(float scale);

        virtual void apply();
        virtual void dump();
        TextStream& externalRepresentation(TextStream& ts) const;

    private:
        FEDisplacementMap(FilterEffect*, FilterEffect*, ChannelSelectorType,
            ChannelSelectorType, const float&);

        RefPtr<FilterEffect> m_in;
        RefPtr<FilterEffect> m_in2;
        ChannelSelectorType m_xChannelSelector;
        ChannelSelectorType m_yChannelSelector;
        float m_scale;
    };

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)

#endif // SVGFEDisplacementMap_h
