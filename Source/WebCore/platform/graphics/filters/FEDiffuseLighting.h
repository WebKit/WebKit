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

#ifndef FEDiffuseLighting_h
#define FEDiffuseLighting_h

#include "FELighting.h"

namespace WebCore {

class LightSource;

class FEDiffuseLighting : public FELighting {
public:
    static Ref<FEDiffuseLighting> create(Filter&, const Color&, float, float, float, float, Ref<LightSource>&&);
    virtual ~FEDiffuseLighting();

    const Color& lightingColor() const;
    bool setLightingColor(const Color&);

    float surfaceScale() const;
    bool setSurfaceScale(float);

    float diffuseConstant() const;
    bool setDiffuseConstant(float);

    float kernelUnitLengthX() const;
    bool setKernelUnitLengthX(float);

    float kernelUnitLengthY() const;
    bool setKernelUnitLengthY(float);

    const LightSource& lightSource() const;

    void dump() override;

    WTF::TextStream& externalRepresentation(WTF::TextStream&, int indention) const override;

private:
    FEDiffuseLighting(Filter&, const Color&, float, float, float, float, Ref<LightSource>&&);
};

} // namespace WebCore

#endif // FEDiffuseLighting_h
