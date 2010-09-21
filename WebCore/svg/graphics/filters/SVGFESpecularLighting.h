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

#ifndef SVGFESpecularLighting_h
#define SVGFESpecularLighting_h

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFELighting.h"

namespace WebCore {

class FESpecularLighting : public FELighting {
public:
    static PassRefPtr<FESpecularLighting> create(const Color&, float, float,
        float, float, float, PassRefPtr<LightSource>);
    virtual ~FESpecularLighting();

    Color lightingColor() const;
    void setLightingColor(const Color&);

    float surfaceScale() const;
    void setSurfaceScale(float);

    float specularConstant() const;
    void setSpecularConstant(float);

    float specularExponent() const;
    void setSpecularExponent(float);

    float kernelUnitLengthX() const;
    void setKernelUnitLengthX(float);

    float kernelUnitLengthY() const;
    void setKernelUnitLengthY(float);

    const LightSource* lightSource() const;
    void setLightSource(PassRefPtr<LightSource>);

    virtual void dump();

    virtual TextStream& externalRepresentation(TextStream&, int indention) const;

private:
    FESpecularLighting(const Color&, float, float, float, float, float, PassRefPtr<LightSource>);
};

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(FILTERS)

#endif // SVGFESpecularLighting_h
