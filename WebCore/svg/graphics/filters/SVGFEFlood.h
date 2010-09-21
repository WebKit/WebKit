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

#ifndef SVGFEFlood_h
#define SVGFEFlood_h

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "Color.h"
#include "Filter.h"
#include "FilterEffect.h"

namespace WebCore {

class FEFlood : public FilterEffect {
public:
    static PassRefPtr<FEFlood> create(const Color&, float);

    Color floodColor() const;
    void setFloodColor(const Color &);

    float floodOpacity() const;
    void setFloodOpacity(float);

    virtual void apply(Filter*);
    virtual void dump();

    virtual TextStream& externalRepresentation(TextStream&, int indention) const;

private:
    FEFlood(const Color&, float);

    Color m_floodColor;
    float m_floodOpacity;
};

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(FILTERS)

#endif // SVGFEFlood_h
