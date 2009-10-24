/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric@webkit.org>
                  2009 Dirk Schulze <krit@webkit.org>

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

#include "config.h"

#if ENABLE(FILTERS)
#include "FEGaussianBlur.h"

#include "CanvasPixelArray.h"
#include "Filter.h"
#include "GraphicsContext.h"
#include "ImageData.h"
#include <math.h>

namespace WebCore {

FEGaussianBlur::FEGaussianBlur(FilterEffect* in, const float& x, const float& y)
    : FilterEffect()
    , m_in(in)
    , m_x(x)
    , m_y(y)
{
}

PassRefPtr<FEGaussianBlur> FEGaussianBlur::create(FilterEffect* in, const float& x, const float& y)
{
    return adoptRef(new FEGaussianBlur(in, x, y));
}

float FEGaussianBlur::stdDeviationX() const
{
    return m_x;
}

void FEGaussianBlur::setStdDeviationX(float x)
{
    m_x = x;
}

float FEGaussianBlur::stdDeviationY() const
{
    return m_y;
}

void FEGaussianBlur::setStdDeviationY(float y)
{
    m_y = y;
}

static void boxBlur(CanvasPixelArray*& srcPixelArray, CanvasPixelArray*& dstPixelArray,
                 unsigned dx, int stride, int strideLine, int effectWidth, int effectHeight, bool alphaImage)
{
    int dxLeft = static_cast<int>(floor(dx / 2));
    int dxRight = dx - dxLeft;

    for (int y = 0; y < effectHeight; ++y) {
        int line = y * strideLine;
        for (int channel = 3; channel >= 0; --channel) {
            int sum = 0;
            // Fill the kernel
            int maxKernelSize = std::min(dxRight, effectWidth);
            for (int i = 0; i < maxKernelSize; ++i)
                sum += srcPixelArray->get(line + i * stride + channel);

            // Blurring
            for (int x = 0; x < effectWidth; ++x) {
                int pixelByteOffset = line + x * stride + channel;
                dstPixelArray->set(pixelByteOffset, static_cast<unsigned char>(sum / dx));
                if (x >= dxLeft)
                    sum -= srcPixelArray->get(pixelByteOffset - dxLeft * stride);
                if (x + dxRight < effectWidth)
                    sum += srcPixelArray->get(pixelByteOffset + dxRight * stride);
            }
            if (alphaImage) // Source image is black, it just has different alpha values
                break;
        }
    }
}

void FEGaussianBlur::apply(Filter* filter)
{
    m_in->apply(filter);
    if (!m_in->resultImage())
        return;

    if (!getEffectContext())
        return;

    setIsAlphaImage(m_in->isAlphaImage());

    if (m_x == 0 || m_y == 0)
        return;

    unsigned sdx = static_cast<unsigned>(floor(m_x * 3 * sqrt(2 * M_PI) / 4.f + 0.5f));
    unsigned sdy = static_cast<unsigned>(floor(m_y * 3 * sqrt(2 * M_PI) / 4.f + 0.5f));

    IntRect effectDrawingRect = calculateDrawingIntRect(m_in->subRegion());
    RefPtr<ImageData> srcImageData(m_in->resultImage()->getPremultipliedImageData(effectDrawingRect));
    CanvasPixelArray* srcPixelArray(srcImageData->data());

    IntRect imageRect(IntPoint(), resultImage()->size());
    RefPtr<ImageData> tmpImageData = ImageData::create(imageRect.width(), imageRect.height());
    CanvasPixelArray* tmpPixelArray(tmpImageData->data());

    int stride = 4 * imageRect.width();
    for (int i = 0; i < 3; ++i) {
        boxBlur(srcPixelArray, tmpPixelArray, sdx, 4, stride, imageRect.width(), imageRect.height(), isAlphaImage());
        boxBlur(tmpPixelArray, srcPixelArray, sdy, stride, 4, imageRect.height(), imageRect.width(), isAlphaImage());
    }

    resultImage()->putPremultipliedImageData(srcImageData.get(), imageRect, IntPoint());
}

void FEGaussianBlur::dump()
{
}

} // namespace WebCore

#endif // ENABLE(FILTERS)
