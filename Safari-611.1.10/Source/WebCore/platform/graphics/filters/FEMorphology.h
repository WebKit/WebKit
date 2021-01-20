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

namespace WebCore {

enum MorphologyOperatorType {
    FEMORPHOLOGY_OPERATOR_UNKNOWN = 0,
    FEMORPHOLOGY_OPERATOR_ERODE = 1,
    FEMORPHOLOGY_OPERATOR_DILATE = 2
};

class FEMorphology : public FilterEffect {
public:
    static Ref<FEMorphology> create(Filter&, MorphologyOperatorType, float radiusX, float radiusY);

    MorphologyOperatorType morphologyOperator() const { return m_type; }
    bool setMorphologyOperator(MorphologyOperatorType);

    float radiusX() const { return m_radiusX; }
    bool setRadiusX(float);

    float radiusY() const { return m_radiusY; }
    bool setRadiusY(float);

private:
    FEMorphology(Filter&, MorphologyOperatorType, float radiusX, float radiusY);

    const char* filterName() const final { return "FEMorphology"; }

    void platformApplySoftware() override;

    void determineAbsolutePaintRect() override;

    WTF::TextStream& externalRepresentation(WTF::TextStream&, RepresentationType) const override;

    bool platformApplyDegenerate(Uint8ClampedArray& dstPixelArray, const IntRect& imageRect, int radiusX, int radiusY);

    struct PaintingData {
        const Uint8ClampedArray* srcPixelArray;
        Uint8ClampedArray* dstPixelArray;
        int width;
        int height;
        int radiusX;
        int radiusY;
    };

    struct PlatformApplyParameters {
        FEMorphology* filter;
        int startY;
        int endY;
        const PaintingData* paintingData;
    };

    static void platformApplyWorker(PlatformApplyParameters*);

    void platformApply(const PaintingData&);
    void platformApplyGeneric(const PaintingData&, int startY, int endY);

    MorphologyOperatorType m_type;
    float m_radiusX;
    float m_radiusY;
};

} // namespace WebCore

