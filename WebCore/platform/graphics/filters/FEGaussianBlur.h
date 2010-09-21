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

#ifndef FEGaussianBlur_h
#define FEGaussianBlur_h

#if ENABLE(FILTERS)
#include "FilterEffect.h"
#include "Filter.h"

namespace WebCore {

class FEGaussianBlur : public FilterEffect {
public:
    static PassRefPtr<FEGaussianBlur> create(float, float);

    float stdDeviationX() const;
    void setStdDeviationX(float);

    float stdDeviationY() const;
    void setStdDeviationY(float);

    static float calculateStdDeviation(float);

    virtual void apply(Filter*);
    virtual void dump();

    virtual TextStream& externalRepresentation(TextStream&, int indention) const;

private:
    FEGaussianBlur(float, float);
    static void kernelPosition(int boxBlur, unsigned& std, int& dLeft, int& dRight);

    float m_stdX;
    float m_stdY;
};

} // namespace WebCore

#endif // ENABLE(FILTERS)

#endif // FEGaussianBlur_h
