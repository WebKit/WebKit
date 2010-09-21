/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Igalia, S.L.
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

#include "config.h"

#if ENABLE(FILTERS)
#include "FEGaussianBlur.h"

#include "CanvasPixelArray.h"
#include "Filter.h"
#include "GraphicsContext.h"
#include "ImageData.h"
#include <wtf/MathExtras.h>

using std::max;

static const float gGaussianKernelFactor = (3 * sqrtf(2 * piFloat) / 4.f);

namespace WebCore {

FEGaussianBlur::FEGaussianBlur(float x, float y)
    : FilterEffect()
    , m_stdX(x)
    , m_stdY(y)
{
}

PassRefPtr<FEGaussianBlur> FEGaussianBlur::create(float x, float y)
{
    return adoptRef(new FEGaussianBlur(x, y));
}

float FEGaussianBlur::stdDeviationX() const
{
    return m_stdX;
}

void FEGaussianBlur::setStdDeviationX(float x)
{
    m_stdX = x;
}

float FEGaussianBlur::stdDeviationY() const
{
    return m_stdY;
}

void FEGaussianBlur::setStdDeviationY(float y)
{
    m_stdY = y;
}

static void boxBlur(CanvasPixelArray*& srcPixelArray, CanvasPixelArray*& dstPixelArray,
                    unsigned dx, int dxLeft, int dxRight, int stride, int strideLine, int effectWidth, int effectHeight, bool alphaImage)
{
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

void FEGaussianBlur::kernelPosition(int boxBlur, unsigned& std, int& dLeft, int& dRight)
{
    // check http://www.w3.org/TR/SVG/filters.html#feGaussianBlurElement for details
    switch (boxBlur) {
    case 0:
        if (!(std % 2)) {
            dLeft = std / 2 - 1;
            dRight = std - dLeft;
        } else {
            dLeft = std / 2;
            dRight = std - dLeft;
        }
        break;
    case 1:
        if (!(std % 2)) {
            dLeft++;
            dRight--;
        }
        break;
    case 2:
        if (!(std % 2)) {
            dRight++;
            std++;
        }
        break;
    }
}

void FEGaussianBlur::apply(Filter* filter)
{
    FilterEffect* in = inputEffect(0);
    in->apply(filter);
    if (!in->resultImage())
        return;

    if (!effectContext())
        return;

    setIsAlphaImage(in->isAlphaImage());

    IntRect effectDrawingRect = requestedRegionOfInputImageData(in->repaintRectInLocalCoordinates());
    RefPtr<ImageData> srcImageData(in->resultImage()->getPremultipliedImageData(effectDrawingRect));
    IntRect imageRect(IntPoint(), resultImage()->size());

    if (!m_stdX && !m_stdY) {
        resultImage()->putPremultipliedImageData(srcImageData.get(), imageRect, IntPoint());
        return;
    }

    unsigned kernelSizeX = 0;
    if (m_stdX)
        kernelSizeX = max(2U, static_cast<unsigned>(floor(m_stdX * filter->filterResolution().width() * gGaussianKernelFactor + 0.5f)));

    unsigned kernelSizeY = 0;
    if (m_stdY)
        kernelSizeY = max(2U, static_cast<unsigned>(floor(m_stdY * filter->filterResolution().height() * gGaussianKernelFactor + 0.5f)));

    CanvasPixelArray* srcPixelArray(srcImageData->data());
    RefPtr<ImageData> tmpImageData = ImageData::create(imageRect.width(), imageRect.height());
    CanvasPixelArray* tmpPixelArray(tmpImageData->data());

    int stride = 4 * imageRect.width();
    int dxLeft = 0;
    int dxRight = 0;
    int dyLeft = 0;
    int dyRight = 0;
    for (int i = 0; i < 3; ++i) {
        if (kernelSizeX) {
            kernelPosition(i, kernelSizeX, dxLeft, dxRight);
            boxBlur(srcPixelArray, tmpPixelArray, kernelSizeX, dxLeft, dxRight, 4, stride, imageRect.width(), imageRect.height(), isAlphaImage());
        } else {
            CanvasPixelArray* auxPixelArray = tmpPixelArray;
            tmpPixelArray = srcPixelArray;
            srcPixelArray = auxPixelArray;
        }

        if (kernelSizeY) {
            kernelPosition(i, kernelSizeY, dyLeft, dyRight);
            boxBlur(tmpPixelArray, srcPixelArray, kernelSizeY, dyLeft, dyRight, stride, 4, imageRect.height(), imageRect.width(), isAlphaImage());
        } else {
            CanvasPixelArray* auxPixelArray = tmpPixelArray;
            tmpPixelArray = srcPixelArray;
            srcPixelArray = auxPixelArray;
        }
    }

    resultImage()->putPremultipliedImageData(srcImageData.get(), imageRect, IntPoint());
}

void FEGaussianBlur::dump()
{
}

TextStream& FEGaussianBlur::externalRepresentation(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "[feGaussianBlur";
    FilterEffect::externalRepresentation(ts);
    ts << " stdDeviation=\"" << m_stdX << ", " << m_stdY << "\"]\n";
    inputEffect(0)->externalRepresentation(ts, indent + 1);
    return ts;
}

float FEGaussianBlur::calculateStdDeviation(float radius)
{
    // Blur radius represents 2/3 times the kernel size, the dest pixel is half of the radius applied 3 times
    return max((radius * 2 / 3.f - 0.5f) / gGaussianKernelFactor, 0.f);
}

} // namespace WebCore

#endif // ENABLE(FILTERS)
