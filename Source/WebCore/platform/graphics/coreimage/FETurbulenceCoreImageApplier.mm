/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "FETurbulenceCoreImageApplier.h"

#if USE(CORE_IMAGE)

#import "FETurbulence.h"
#import "Filter.h"
#import "Logging.h"
#import <CoreImage/CoreImage.h>
#import <simd/simd.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/FastMalloc.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

// FIXME: Find a way to share these with the shader.
#define TURBULENCE_BLOCK_SIZE 256
#define TURBULENCE_LATTICE_SIZE (2 * TURBULENCE_BLOCK_SIZE + 2)
#define TURBULENCE_GRADIENT_COLUMNS TURBULENCE_BLOCK_SIZE

static CIKernel* turbulenceKernel()
{
    static NeverDestroyed<RetainPtr<CIKernel>> kernel;
    static std::once_flag onceFlag;

    std::call_once(onceFlag, [] {
        NSError *error = nil;
        NSArray<CIKernel *> *kernels = [CIKernel kernelsWithMetalString:@R"( /* NOLINT */
#define TURBULENCE_BLOCK_SIZE 256
#define TURBULENCE_LATTICE_SIZE (2 * TURBULENCE_BLOCK_SIZE + 2)
#define TURBULENCE_GRADIENT_COLUMNS TURBULENCE_BLOCK_SIZE

#define PERLIN_NOISE 4096
#define BLOCK_MASK (TURBULENCE_BLOCK_SIZE - 1)

extern "C" {
namespace coreimage {

enum NoiseType : uint8_t {
    FractalNoise,
    Turbulence
};

struct TurbulenceStitchData {
    vector_int2 size; // How much to subtract to wrap for stitching.
    vector_int2 wrap; // Minimum value to wrap.
};

struct TurbulenceConstants {
    uint8_t type;
    uint8_t stitchTiles;
    int numOctaves;
    float extentBottom;
    float2 origin;
    float2 scale;
    float2 baseFrequency;
    TurbulenceStitchData stitchData;
};

struct LatticeSelector {
    int data[TURBULENCE_LATTICE_SIZE];
};

struct GradientChannel {
    float2 data[TURBULENCE_LATTICE_SIZE];
};

static inline float2 sCurve(float2 t)
{
    return t * t * (3.0 - 2.0 * t);
}

static inline float lerp(float t, float a, float b)
{
    return a + t * (b - a);
}

float noiseForChannel(constant GradientChannel& gradient, int4 indices, float2 fraction, float2 s)
{
    // q = fGradient[nColorChannel][b00];
    // u = rx0 * q[0] + ry0 * q[1];
    float2 q1 = gradient.data[indices[0] % TURBULENCE_BLOCK_SIZE];
    float u1 = fraction.x * q1[0] + fraction.y * q1[1];

    // q = fGradient[nColorChannel][b10];
    // v = rx1 * q[0] + ry0 * q[1];
    float2 q2 = gradient.data[indices[1] % TURBULENCE_BLOCK_SIZE];
    float v1 = (fraction.x - 1) * q2[0] + fraction.y * q2[1];

    // a = lerp(sx, u, v);
    float a = lerp(s.x, u1, v1);

    // q = fGradient[nColorChannel][b01];
    // u = rx0 * q[0] + ry1 * q[1];
    float2 q3 = gradient.data[indices[2] % TURBULENCE_BLOCK_SIZE];
    float u2 = fraction.x * q3[0] + (fraction.y - 1) * q3[1];

    // q = fGradient[nColorChannel][b11];
    // v = rx1 * q[0] + ry1 * q[1];
    float2 q4 = gradient.data[indices[3] % TURBULENCE_BLOCK_SIZE];
    float v2 = (fraction.x - 1) * q4[0] + (fraction.y - 1) * q4[1];

    // b = lerp(sx, u, v);
    float b = lerp(s.x, u2, v2);

    return lerp(s.y, a, b);
}

float4 noise2D(constant TurbulenceConstants& constants,
    constant LatticeSelector& latticeSelector,
    constant GradientChannel& redGradient,
    constant GradientChannel& greenGradient,
    constant GradientChannel& blueGradient,
    constant GradientChannel& alphaGradient,
    float2 noiseVector, TurbulenceStitchData stitchData)
{
    // t = vec[0] + PerlinN;
    float2 position = noiseVector + PERLIN_NOISE;
    // bx0 = (int)t;
    int2 index = (int2)floor(position);
    // bx1 = bx0 + 1;
    int2 nextIndex = index + 1;
    // rx0 = t - (int)t;
    float2 fraction = position - (float2)index;

    if (constants.stitchTiles) {
        if (index.x >= stitchData.wrap.x)
            index.x -= stitchData.size.x;

        if (nextIndex.x >= stitchData.wrap.x)
            nextIndex.x -= stitchData.size.x;

        if (index.y >= stitchData.wrap.y)
            index.y -= stitchData.size.y;

        if (nextIndex.y >= stitchData.wrap.y)
            nextIndex.y -= stitchData.size.y;
    }

    // bx0 &= BM;
    // bx1 &= BM;
    // by0 &= BM;
    // by1 &= BM;
    index &= BLOCK_MASK;
    nextIndex &= BLOCK_MASK;

    // i = uLatticeSelector[bx0];
    // j = uLatticeSelector[bx1];
    int latticeIndex = latticeSelector.data[index.x];
    int nextLatticeIndex = latticeSelector.data[nextIndex.x];

    // sx = double(s_curve(rx0));
    // sy = double(s_curve(ry0));
    float2 s = sCurve(fraction);

    int4 indices = {
        latticeSelector.data[latticeIndex      + index.y],        // b00 = uLatticeSelector[i + by0]
        latticeSelector.data[nextLatticeIndex  + index.y],        // b10 = uLatticeSelector[j + by0]
        latticeSelector.data[latticeIndex      + nextIndex.y],    // b01 = uLatticeSelector[i + by1]
        latticeSelector.data[nextLatticeIndex  + nextIndex.y],    // b11 = uLatticeSelector[j + by1]
    };

    return {
        noiseForChannel(redGradient,   indices, fraction, s),
        noiseForChannel(greenGradient, indices, fraction, s),
        noiseForChannel(blueGradient,  indices, fraction, s),
        noiseForChannel(alphaGradient, indices, fraction, s),
    };
}

[[stitchable]] float4 kernel_turbulence(
    constant TurbulenceConstants* constants,
    constant LatticeSelector* latticeSelector,
    constant GradientChannel* redGradient,
    constant GradientChannel* greenGradient,
    constant GradientChannel* blueGradient,
    constant GradientChannel* alphaGradient,
    destination dest)
{
    // Texture coordinates are y-flipped. 
    float2 coordinate = dest.coord();
    coordinate.y = constants->extentBottom - coordinate.y;
    coordinate += constants->origin;
    coordinate /= constants->scale;
    coordinate.x += 0.5;
    coordinate.y += 0.5;

    auto stitchData = constants->stitchData;

    float ratio = 1;
    float2 noiseVector = coordinate * constants->baseFrequency;

    float4 resultPixel = { };

    for (int octave = 0; octave < constants->numOctaves; ++octave) {
        if (constants->type == NoiseType::FractalNoise)
            resultPixel += noise2D(*constants, *latticeSelector, *redGradient, *greenGradient, *blueGradient, *alphaGradient, noiseVector, stitchData) / ratio;
        else
            resultPixel += fabs(noise2D(*constants, *latticeSelector, *redGradient, *greenGradient, *blueGradient, *alphaGradient, noiseVector, stitchData)) / ratio;

        if (octave == constants->numOctaves - 1)
            break;

        noiseVector *= 2;
        ratio *= 2;

        if (constants->stitchTiles) {
            stitchData.size *= 2;
            stitchData.wrap = 2 * stitchData.wrap - PERLIN_NOISE;
        }
    }

    if (constants->type == NoiseType::FractalNoise)
        resultPixel = resultPixel * 0.5 + 0.5;

    resultPixel = clamp(resultPixel, 0, 1);
    return premultiply(resultPixel);
}

} // extern "C"
} // namespace coreimage
        )" /* NOLINT */ error:&error];

        if (error || !kernels || !kernels.count) {
            LOG(Filters, "Turbulence kernel compilation failed: %@", error);
            return;
        }

        kernel.get() = kernels[0];
    });

    return kernel.get().get();
}


WTF_MAKE_TZONE_ALLOCATED_IMPL(FETurbulenceCoreImageApplier);

FETurbulenceCoreImageApplier::FETurbulenceCoreImageApplier(const FETurbulence& effect)
    : Base(effect)
{
}

FETurbulenceCoreImageApplier::~FETurbulenceCoreImageApplier() = default;

bool FETurbulenceCoreImageApplier::apply(const Filter& filter, std::span<const Ref<FilterImage>>, FilterImage& result) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    RetainPtr kernel = turbulenceKernel();
    if (!kernel)
        return false;

    auto tileSize = roundedIntSize(result.primitiveSubregion().size());

    float baseFrequencyX = m_effect->baseFrequencyX();
    float baseFrequencyY = m_effect->baseFrequencyY();
    auto stitchData = FETurbulenceSoftwareApplier::computeStitching(tileSize, baseFrequencyX, baseFrequencyY, m_effect->stitchTiles());
    auto paintingData = FETurbulenceSoftwareApplier::PaintingData(m_effect->type(), baseFrequencyX, baseFrequencyY, m_effect->numOctaves(), m_effect->seed(), m_effect->stitchTiles());

    enum NoiseType {
        FractalNoise,
        Turbulence
    };

    struct TurbulenceStitchData {
        vector_int2 size; // How much to subtract to wrap for stitching.
        vector_int2 wrap; // Minimum value to wrap.
    };

    struct GradientChannel {
        vector_float2 data[TURBULENCE_GRADIENT_COLUMNS];
    };

    struct TurbulenceConstants {
        uint8_t type;
        uint8_t stitchTiles;
        int numOctaves;
        float extentBottom;
        vector_float2 origin;
        vector_float2 scale;
        vector_float2 baseFrequency;
        TurbulenceStitchData stitchData;
    };

    auto origin = FloatPoint { result.absoluteImageRect().location() };
    auto scale = filter.filterScale();
    auto extent = filter.flippedRectRelativeToAbsoluteEnclosingFilterRegion(result.absoluteImageRect());
    // Work around a bug where the extent of the kernel is too large after compositing with other CIImages (TODO: file the radar).
    extent.shiftMaxXEdgeBy(1);

    auto constants = TurbulenceConstants {
        .type = static_cast<uint8_t>(paintingData.type == TurbulenceType::FractalNoise ? NoiseType::FractalNoise : NoiseType::Turbulence),
        .stitchTiles = paintingData.stitchTiles,
        .numOctaves = paintingData.numOctaves,
        .extentBottom = extent.maxY(),
        .origin = { origin.x(), origin.y() },
        .scale = { scale.width(), scale.height() },
        .baseFrequency = { paintingData.baseFrequencyX, paintingData.baseFrequencyY },
        .stitchData = TurbulenceStitchData { { stitchData.width, stitchData.height }, { stitchData.wrapX, stitchData.wrapY } },
    };

    RetainPtr<NSArray> arguments = @[
        [NSData dataWithBytes:&constants length:sizeof(TurbulenceConstants)],
        [NSData dataWithBytes:&paintingData.latticeSelector length:sizeof(FETurbulenceSoftwareApplier::PaintingData::LatticeSelector)],
        [NSData dataWithBytes:&paintingData.gradient[0] length:sizeof(FETurbulenceSoftwareApplier::PaintingData::ChannelGradient)],
        [NSData dataWithBytes:&paintingData.gradient[1] length:sizeof(FETurbulenceSoftwareApplier::PaintingData::ChannelGradient)],
        [NSData dataWithBytes:&paintingData.gradient[2] length:sizeof(FETurbulenceSoftwareApplier::PaintingData::ChannelGradient)],
        [NSData dataWithBytes:&paintingData.gradient[3] length:sizeof(FETurbulenceSoftwareApplier::PaintingData::ChannelGradient)],
    ];

    RetainPtr outputImage = [kernel applyWithExtent:extent
        roiCallback:^CGRect(int, CGRect destRect) {
            return destRect;
        }
        arguments:arguments.get()];

    if (!outputImage)
        return false;

    // Part 2 of the workaround; this extent modification is necessary to avoid this -imageByCroppingToRect: being a no-op.
    extent.shiftMaxXEdgeBy(-1);
    outputImage = [outputImage imageByCroppingToRect:extent];

    result.setCIImage(WTF::move(outputImage));
    return true;

    END_BLOCK_OBJC_EXCEPTIONS
    return false;
}

} // namespace WebCore

#endif // USE(CORE_IMAGE)
