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
#include "SVGFilterEffect.h"

namespace WebCore {

enum SVGChannelSelectorType {
    SVG_CHANNEL_UNKNOWN = 0,
    SVG_CHANNEL_R       = 1,
    SVG_CHANNEL_G       = 2,
    SVG_CHANNEL_B       = 3,
    SVG_CHANNEL_A       = 4
};

class SVGFEDisplacementMap : public SVGFilterEffect {
public:
    SVGFEDisplacementMap(SVGResourceFilter*);

    String in2() const;
    void setIn2(const String&);

    SVGChannelSelectorType xChannelSelector() const;
    void setXChannelSelector(const SVGChannelSelectorType);

    SVGChannelSelectorType yChannelSelector() const;
    void setYChannelSelector(const SVGChannelSelectorType);

    float scale() const;
    void setScale(float scale);

    virtual TextStream& externalRepresentation(TextStream&) const;

#if PLATFORM(CI)
    virtual CIFilter* getCIFilter(const FloatRect& bbox) const;
#endif

private:
    SVGChannelSelectorType m_xChannelSelector;
    SVGChannelSelectorType m_yChannelSelector;
    float m_scale;
    String m_in2;
};

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)

#endif // SVGFEDisplacementMap_h
