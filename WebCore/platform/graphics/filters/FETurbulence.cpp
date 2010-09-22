/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Renata Hodovan <reni@inf.u-szeged.hu>
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
#include "FETurbulence.h"

#include "CanvasPixelArray.h"
#include "Filter.h"
#include "ImageData.h"

#include <wtf/MathExtras.h>

namespace WebCore {

/*
    Produces results in the range [1, 2**31 - 2]. Algorithm is:
    r = (a * r) mod m where a = randAmplitude = 16807 and
    m = randMaximum = 2**31 - 1 = 2147483647, r = seed.
    See [Park & Miller], CACM vol. 31 no. 10 p. 1195, Oct. 1988
    To test: the algorithm should produce the result 1043618065
    as the 10,000th generated number if the original seed is 1.
*/
static const int s_perlinNoise = 4096;
static const long s_randMaximum = 2147483647; // 2**31 - 1
static const int s_randAmplitude = 16807; // 7**5; primitive root of m
static const int s_randQ = 127773; // m / a
static const int s_randR = 2836; // m % a

FETurbulence::FETurbulence(TurbulanceType type, float baseFrequencyX, float baseFrequencyY, int numOctaves, float seed, bool stitchTiles)
    : FilterEffect()
    , m_type(type)
    , m_baseFrequencyX(baseFrequencyX)
    , m_baseFrequencyY(baseFrequencyY)
    , m_numOctaves(numOctaves)
    , m_seed(seed)
    , m_stitchTiles(stitchTiles)
{
}

PassRefPtr<FETurbulence> FETurbulence::create(TurbulanceType type, float baseFrequencyX, float baseFrequencyY, int numOctaves, float seed, bool stitchTiles)
{
    return adoptRef(new FETurbulence(type, baseFrequencyX, baseFrequencyY, numOctaves, seed, stitchTiles));
}

TurbulanceType FETurbulence::type() const
{
    return m_type;
}

void FETurbulence::setType(TurbulanceType type)
{
    m_type = type;
}

float FETurbulence::baseFrequencyY() const
{
    return m_baseFrequencyY;
}

void FETurbulence::setBaseFrequencyY(float baseFrequencyY)
{
    m_baseFrequencyY = baseFrequencyY;
}

float FETurbulence::baseFrequencyX() const
{
    return m_baseFrequencyX;
}

void FETurbulence::setBaseFrequencyX(float baseFrequencyX)
{
       m_baseFrequencyX = baseFrequencyX;
}

float FETurbulence::seed() const
{
    return m_seed; 
}

void FETurbulence::setSeed(float seed)
{
    m_seed = seed;
}

int FETurbulence::numOctaves() const
{
    return m_numOctaves;
}

void FETurbulence::setNumOctaves(bool numOctaves)
{
    m_numOctaves = numOctaves;
}

bool FETurbulence::stitchTiles() const
{
    return m_stitchTiles;
}

void FETurbulence::setStitchTiles(bool stitch)
{
    m_stitchTiles = stitch;
}

// The turbulence calculation code is an adapted version of what appears in the SVG 1.1 specification:
// http://www.w3.org/TR/SVG11/filters.html#feTurbulence

FETurbulence::PaintingData::PaintingData(long paintingSeed, const IntSize& paintingSize)
    : seed(paintingSeed)
    , width(0)
    , height(0)
    , wrapX(0)
    , wrapY(0)
    , channel(0)
    , filterSize(paintingSize)
{
}

// Compute pseudo random number.
inline long FETurbulence::PaintingData::random()
{
    long result = s_randAmplitude * (seed % s_randQ) - s_randR * (seed / s_randQ);
    if (result <= 0)
        result += s_randMaximum;
    seed = result;
    return result;
}

inline float smoothCurve(float t)
{
    return t * t * (3 - 2 * t);
}

inline float linearInterpolation(float t, float a, float b)
{
    return a + t * (b - a);
}

inline void FETurbulence::initPaint(PaintingData& paintingData)
{
    float normalizationFactor;

    // The seed value clamp to the range [1, s_randMaximum - 1].
    if (paintingData.seed <= 0)
        paintingData.seed = -(paintingData.seed % (s_randMaximum - 1)) + 1;
    if (paintingData.seed > s_randMaximum - 1)
        paintingData.seed = s_randMaximum - 1;

    float* gradient;
    for (int channel = 0; channel < 4; ++channel) {
        for (int i = 0; i < s_blockSize; ++i) {
            paintingData.latticeSelector[i] = i;
            gradient = paintingData.gradient[channel][i];
            gradient[0] = static_cast<float>((paintingData.random() % (2 * s_blockSize)) - s_blockSize) / s_blockSize;
            gradient[1] = static_cast<float>((paintingData.random() % (2 * s_blockSize)) - s_blockSize) / s_blockSize;
            normalizationFactor = sqrtf(gradient[0] * gradient[0] + gradient[1] * gradient[1]);
            gradient[0] /= normalizationFactor;
            gradient[1] /= normalizationFactor;
        }
    }
    for (int i = s_blockSize - 1; i > 0; --i) {
        int k = paintingData.latticeSelector[i];
        int j = paintingData.random() % s_blockSize;
        ASSERT(j >= 0);
        ASSERT(j < 2 * s_blockSize + 2);
        paintingData.latticeSelector[i] = paintingData.latticeSelector[j];
        paintingData.latticeSelector[j] = k;
    }
    for (int i = 0; i < s_blockSize + 2; ++i) {
        paintingData.latticeSelector[s_blockSize + i] = paintingData.latticeSelector[i];
        for (int channel = 0; channel < 4; ++channel) {
            paintingData.gradient[channel][s_blockSize + i][0] = paintingData.gradient[channel][i][0];
            paintingData.gradient[channel][s_blockSize + i][1] = paintingData.gradient[channel][i][1];
        }
    }
}

inline void checkNoise(int& noiseValue, int limitValue, int newValue)
{
    if (noiseValue >= limitValue)
        noiseValue -= newValue;
    if (noiseValue >= limitValue - 1)
        noiseValue -= newValue - 1;
}

float FETurbulence::noise2D(PaintingData& paintingData, const FloatPoint& noiseVector)
{
    struct Noise {
        int noisePositionIntegerValue;
        float noisePositionFractionValue;

        Noise(float component)
        {
            float position = component + s_perlinNoise;
            noisePositionIntegerValue = static_cast<int>(position);
            noisePositionFractionValue = position - noisePositionIntegerValue;
        }
    };

    Noise noiseX(noiseVector.x());
    Noise noiseY(noiseVector.y());
    float* q;
    float sx, sy, a, b, u, v;

    // If stitching, adjust lattice points accordingly.
    if (m_stitchTiles) {
        checkNoise(noiseX.noisePositionIntegerValue, paintingData.wrapX, paintingData.width);
        checkNoise(noiseY.noisePositionIntegerValue, paintingData.wrapY, paintingData.height);
    }

    noiseX.noisePositionIntegerValue &= s_blockMask;
    noiseY.noisePositionIntegerValue &= s_blockMask;
    int latticeIndex = paintingData.latticeSelector[noiseX.noisePositionIntegerValue];
    int nextLatticeIndex = paintingData.latticeSelector[(noiseX.noisePositionIntegerValue + 1) & s_blockMask];

    sx = smoothCurve(noiseX.noisePositionFractionValue);
    sy = smoothCurve(noiseY.noisePositionFractionValue);

    // This is taken 1:1 from SVG spec: http://www.w3.org/TR/SVG11/filters.html#feTurbulenceElement.
    int temp = paintingData.latticeSelector[latticeIndex + noiseY.noisePositionIntegerValue];
    q = paintingData.gradient[paintingData.channel][temp];
    u = noiseX.noisePositionFractionValue * q[0] + noiseY.noisePositionFractionValue * q[1];
    temp = paintingData.latticeSelector[nextLatticeIndex + noiseY.noisePositionIntegerValue];
    q = paintingData.gradient[paintingData.channel][temp];
    v = (noiseX.noisePositionFractionValue - 1) * q[0] + noiseY.noisePositionFractionValue * q[1];
    a = linearInterpolation(sx, u, v);
    temp = paintingData.latticeSelector[latticeIndex + noiseY.noisePositionIntegerValue + 1];
    q = paintingData.gradient[paintingData.channel][temp];
    u = noiseX.noisePositionFractionValue * q[0] + (noiseY.noisePositionFractionValue - 1) * q[1];
    temp = paintingData.latticeSelector[nextLatticeIndex + noiseY.noisePositionIntegerValue + 1];
    q = paintingData.gradient[paintingData.channel][temp];
    v = (noiseX.noisePositionFractionValue - 1) * q[0] + (noiseY.noisePositionFractionValue - 1) * q[1];
    b = linearInterpolation(sx, u, v);
    return linearInterpolation(sy, a, b);
}

unsigned char FETurbulence::calculateTurbulenceValueForPoint(PaintingData& paintingData, const FloatPoint& point)
{
    float tileWidth = paintingData.filterSize.width();
    ASSERT(tileWidth > 0);
    float tileHeight = paintingData.filterSize.height();
    ASSERT(tileHeight > 0);
    // Adjust the base frequencies if necessary for stitching.
    if (m_stitchTiles) {
        // When stitching tiled turbulence, the frequencies must be adjusted
        // so that the tile borders will be continuous.
        if (m_baseFrequencyX) {
            float lowFrequency = floorf(tileWidth * m_baseFrequencyX) / tileWidth;
            float highFrequency = ceilf(tileWidth * m_baseFrequencyX) / tileWidth;
            // BaseFrequency should be non-negative according to the standard.
            if (m_baseFrequencyX / lowFrequency < highFrequency / m_baseFrequencyX)
                m_baseFrequencyX = lowFrequency;
            else
                m_baseFrequencyX = highFrequency;
        }
        if (m_baseFrequencyY) {
            float lowFrequency = floorf(tileHeight * m_baseFrequencyY) / tileHeight;
            float highFrequency = ceilf(tileHeight * m_baseFrequencyY) / tileHeight;
            if (m_baseFrequencyY / lowFrequency < highFrequency / m_baseFrequencyY)
                m_baseFrequencyY = lowFrequency;
            else
                m_baseFrequencyY = highFrequency;
        }
        // Set up TurbulenceInitial stitch values.
        paintingData.width = roundf(tileWidth * m_baseFrequencyX);
        paintingData.wrapX = s_perlinNoise + paintingData.width;
        paintingData.height = roundf(tileHeight * m_baseFrequencyY);
        paintingData.wrapY = s_perlinNoise + paintingData.height;
    }
    float turbulenceFunctionResult = 0;
    FloatPoint noiseVector(point.x() * m_baseFrequencyX, point.y() * m_baseFrequencyY);
    float ratio = 1;
    for (int octave = 0; octave < m_numOctaves; ++octave) {
        if (m_type == FETURBULENCE_TYPE_FRACTALNOISE)
            turbulenceFunctionResult += noise2D(paintingData, noiseVector) / ratio;
        else
            turbulenceFunctionResult += fabsf(noise2D(paintingData, noiseVector)) / ratio;
        noiseVector.setX(noiseVector.x() * 2);
        noiseVector.setY(noiseVector.y() * 2);
        ratio *= 2;
        if (m_stitchTiles) {
            // Update stitch values. Subtracting s_perlinNoiseoise before the multiplication and
            // adding it afterward simplifies to subtracting it once.
            paintingData.width *= 2;
            paintingData.wrapX = 2 * paintingData.wrapX - s_perlinNoise;
            paintingData.height *= 2;
            paintingData.wrapY = 2 * paintingData.wrapY - s_perlinNoise;
        }
    }

    // The value of turbulenceFunctionResult comes from ((turbulenceFunctionResult * 255) + 255) / 2 by fractalNoise
    // and (turbulenceFunctionResult * 255) by turbulence.
    if (m_type == FETURBULENCE_TYPE_FRACTALNOISE)
        turbulenceFunctionResult = turbulenceFunctionResult * 0.5f + 0.5f;
    // Clamp result
    turbulenceFunctionResult = std::max(std::min(turbulenceFunctionResult, 1.f), 0.f);
    return static_cast<unsigned char>(turbulenceFunctionResult * 255);
}

void FETurbulence::apply(Filter* filter)
{
    if (!effectContext())
        return;

    IntRect imageRect(IntPoint(), resultImage()->size());
    if (!imageRect.size().width() || !imageRect.size().height())
        return;

    RefPtr<ImageData> imageData = ImageData::create(imageRect.width(), imageRect.height());
    PaintingData paintingData(m_seed, imageRect.size());
    initPaint(paintingData);

    FloatRect filterRegion = filter->filterRegion();
    FloatPoint point;
    point.setY(filterRegion.y());
    int indexOfPixelChannel = 0;
    for (int y = 0; y < imageRect.height(); ++y) {
        point.setY(point.y() + 1);
        point.setX(filterRegion.x());
        for (int x = 0; x < imageRect.width(); ++x) {
            point.setX(point.x() + 1);
            for (paintingData.channel = 0; paintingData.channel < 4; ++paintingData.channel, ++indexOfPixelChannel)
                imageData->data()->set(indexOfPixelChannel, calculateTurbulenceValueForPoint(paintingData, point));
        }
    }
    resultImage()->putUnmultipliedImageData(imageData.get(), imageRect, IntPoint());
}

void FETurbulence::dump()
{
}

static TextStream& operator<<(TextStream& ts, const TurbulanceType& type)
{
    switch (type) {
    case FETURBULENCE_TYPE_UNKNOWN:
        ts << "UNKNOWN";
        break;
    case FETURBULENCE_TYPE_TURBULENCE:
        ts << "TURBULANCE";
        break;
    case FETURBULENCE_TYPE_FRACTALNOISE:
        ts << "NOISE";
        break;
    }
    return ts;
}

TextStream& FETurbulence::externalRepresentation(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "[feTurbulence";
    FilterEffect::externalRepresentation(ts);
    ts << " type=\"" << type() << "\" "
       << "baseFrequency=\"" << baseFrequencyX() << ", " << baseFrequencyY() << "\" "
       << "seed=\"" << seed() << "\" "
       << "numOctaves=\"" << numOctaves() << "\" "
       << "stitchTiles=\"" << stitchTiles() << "\"]\n";
    return ts;
}

} // namespace WebCore

#endif // ENABLE(FILTERS)
