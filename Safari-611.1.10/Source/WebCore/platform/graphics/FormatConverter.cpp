/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Mozilla Corporation. All rights reserved.
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

#include "config.h"

#if ENABLE(WEBGL)

#include "FormatConverter.h"

#if HAVE(ARM_NEON_INTRINSICS)
#include "GraphicsContextGLNEON.h"
#endif

#if USE(ACCELERATE)
#include <Accelerate/Accelerate.h>
#endif

namespace WebCore {

namespace {

const float MaxInt8Value = INT8_MAX;
const float MaxUInt8Value = UINT8_MAX;
const float MaxInt16Value = INT16_MAX;
const float MaxUInt16Value = UINT16_MAX;
const double MaxInt32Value = INT32_MAX;
const double MaxUInt32Value = UINT32_MAX;

int8_t clampMin(int8_t value)
{
    const int8_t MinInt8Value = INT8_MIN + 1;
    return value < MinInt8Value ? MinInt8Value : value;
}

int16_t clampMin(int16_t value)
{
    const int16_t MinInt16Value = INT16_MIN + 1;
    return value < MinInt16Value ? MinInt16Value : value;
}

int32_t clampMin(int32_t value)
{
    const int32_t MinInt32Value = INT32_MIN + 1;
    return value < MinInt32Value ? MinInt32Value : value;
}

} // anonymous namespace

// Following Float to Half-Float converion code is from the implementation of ftp://www.fox-toolkit.org/pub/fasthalffloatconversion.pdf,
// "Fast Half Float Conversions" by Jeroen van der Zijp, November 2008 (Revised September 2010).
// Specially, the basetable[512] and shifttable[512] are generated as follows:
/*
unsigned short basetable[512];
unsigned char shifttable[512];

void generatetables(){
    unsigned int i;
    int e;
    for (i = 0; i < 256; ++i){
        e = i - 127;
        if (e < -24){ // Very small numbers map to zero
            basetable[i | 0x000] = 0x0000;
            basetable[i | 0x100] = 0x8000;
            shifttable[i | 0x000] = 24;
            shifttable[i | 0x100] = 24;
        }
        else if (e < -14) { // Small numbers map to denorms
            basetable[i | 0x000] = (0x0400>>(-e-14));
            basetable[i | 0x100] = (0x0400>>(-e-14)) | 0x8000;
            shifttable[i | 0x000] = -e-1;
            shifttable[i | 0x100] = -e-1;
        }
        else if (e <= 15){ // Normal numbers just lose precision
            basetable[i | 0x000] = ((e+15)<<10);
            basetable[i| 0x100] = ((e+15)<<10) | 0x8000;
            shifttable[i|0x000] = 13;
            shifttable[i|0x100] = 13;
        }
        else if (e<128){ // Large numbers map to Infinity
            basetable[i|0x000] = 0x7C00;
            basetable[i|0x100] = 0xFC00;
            shifttable[i|0x000] = 24;
            shifttable[i|0x100] = 24;
        }
        else { // Infinity and NaN's stay Infinity and NaN's
            basetable[i|0x000] = 0x7C00;
            basetable[i|0x100] = 0xFC00;
            shifttable[i|0x000] = 13;
            shifttable[i|0x100] = 13;
       }
    }
}
*/

static const unsigned short baseTable[512] = {
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      1,      2,      4,      8,      16,     32,     64,     128,    256,
512,    1024,   2048,   3072,   4096,   5120,   6144,   7168,   8192,   9216,   10240,  11264,  12288,  13312,  14336,  15360,
16384,  17408,  18432,  19456,  20480,  21504,  22528,  23552,  24576,  25600,  26624,  27648,  28672,  29696,  30720,  31744,
31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,
31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,
31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,
31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,
31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,
31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,
31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,
32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,
32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,
32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,
32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,
32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,
32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,
32768,  32768,  32768,  32768,  32768,  32768,  32768,  32769,  32770,  32772,  32776,  32784,  32800,  32832,  32896,  33024,
33280,  33792,  34816,  35840,  36864,  37888,  38912,  39936,  40960,  41984,  43008,  44032,  45056,  46080,  47104,  48128,
49152,  50176,  51200,  52224,  53248,  54272,  55296,  56320,  57344,  58368,  59392,  60416,  61440,  62464,  63488,  64512,
64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,
64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,
64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,
64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,
64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,
64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,
64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512
};

static const unsigned char shiftTable[512] = {
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     23,     22,     21,     20,     19,     18,     17,     16,     15,
14,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,
13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     13,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     23,     22,     21,     20,     19,     18,     17,     16,     15,
14,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,
13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     13
};

inline unsigned short convertFloatToHalfFloat(float f)
{
    unsigned temp = *(reinterpret_cast<unsigned *>(&f));
    unsigned signexp = (temp >> 23) & 0x1ff;
    return baseTable[signexp] + ((temp & 0x007fffff) >> shiftTable[signexp]);
}

/* BEGIN CODE SHARED WITH MOZILLA FIREFOX */

// The following packing and unpacking routines are expressed in terms of function templates and inline functions to achieve generality and speedup.
// Explicit template specializations correspond to the cases that would occur.
// Some code are merged back from Mozilla code in http://mxr.mozilla.org/mozilla-central/source/content/canvas/src/WebGLTexelConversions.h

//----------------------------------------------------------------------
// Pixel unpacking routines.
template<GraphicsContextGL::DataFormat format, typename SourceType, typename DstType>
ALWAYS_INLINE void unpack(const SourceType*, DstType*, unsigned)
{
    ASSERT_NOT_REACHED();
}

template<> ALWAYS_INLINE void unpack<GraphicsContextGL::DataFormat::ARGB8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[1];
        destination[1] = source[2];
        destination[2] = source[3];
        destination[3] = source[0];
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContextGL::DataFormat::ABGR8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[3];
        destination[1] = source[2];
        destination[2] = source[1];
        destination[3] = source[0];
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContextGL::DataFormat::BGRA8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    const uint32_t* source32 = reinterpret_cast_ptr<const uint32_t*>(source);
    uint32_t* destination32 = reinterpret_cast_ptr<uint32_t*>(destination);
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        uint32_t bgra = source32[i];
#if CPU(BIG_ENDIAN)
        uint32_t brMask = 0xff00ff00;
        uint32_t gaMask = 0x00ff00ff;
#else
        uint32_t brMask = 0x00ff00ff;
        uint32_t gaMask = 0xff00ff00;
#endif
        uint32_t rgba = (((bgra >> 16) | (bgra << 16)) & brMask) | (bgra & gaMask);
        destination32[i] = rgba;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContextGL::DataFormat::RGBA5551, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::unpackOneRowOfRGBA5551ToRGBA8(source, destination, pixelsPerRow);
#endif
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        uint16_t packedValue = source[0];
        uint8_t r = packedValue >> 11;
        uint8_t g = (packedValue >> 6) & 0x1F;
        uint8_t b = (packedValue >> 1) & 0x1F;
        destination[0] = (r << 3) | (r & 0x7);
        destination[1] = (g << 3) | (g & 0x7);
        destination[2] = (b << 3) | (b & 0x7);
        destination[3] = (packedValue & 0x1) ? 0xFF : 0x0;
        source += 1;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContextGL::DataFormat::RGBA4444, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::unpackOneRowOfRGBA4444ToRGBA8(source, destination, pixelsPerRow);
#endif
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        uint16_t packedValue = source[0];
        uint8_t r = packedValue >> 12;
        uint8_t g = (packedValue >> 8) & 0x0F;
        uint8_t b = (packedValue >> 4) & 0x0F;
        uint8_t a = packedValue & 0x0F;
        destination[0] = r << 4 | r;
        destination[1] = g << 4 | g;
        destination[2] = b << 4 | b;
        destination[3] = a << 4 | a;
        source += 1;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContextGL::DataFormat::RA8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[0];
        destination[2] = source[0];
        destination[3] = source[1];
        source += 2;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContextGL::DataFormat::AR8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[1];
        destination[1] = source[1];
        destination[2] = source[1];
        destination[3] = source[0];
        source += 2;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContextGL::DataFormat::RGBA8, uint8_t, float>(const uint8_t* source, float* destination, unsigned pixelsPerRow)
{
    const float scaleFactor = 1.0f / 255.0f;
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        destination[2] = source[2] * scaleFactor;
        destination[3] = source[3] * scaleFactor;
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContextGL::DataFormat::BGRA8, uint8_t, float>(const uint8_t* source, float* destination, unsigned pixelsPerRow)
{
    const float scaleFactor = 1.0f / 255.0f;
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[2] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        destination[2] = source[0] * scaleFactor;
        destination[3] = source[3] * scaleFactor;
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContextGL::DataFormat::ABGR8, uint8_t, float>(const uint8_t* source, float* destination, unsigned pixelsPerRow)
{
    const float scaleFactor = 1.0f / 255.0f;
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[3] * scaleFactor;
        destination[1] = source[2] * scaleFactor;
        destination[2] = source[1] * scaleFactor;
        destination[3] = source[0] * scaleFactor;
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContextGL::DataFormat::ARGB8, uint8_t, float>(const uint8_t* source, float* destination, unsigned pixelsPerRow)
{
    const float scaleFactor = 1.0f / 255.0f;
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[1] * scaleFactor;
        destination[1] = source[2] * scaleFactor;
        destination[2] = source[3] * scaleFactor;
        destination[3] = source[0] * scaleFactor;
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContextGL::DataFormat::RA32F, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[0];
        destination[2] = source[0];
        destination[3] = source[1];
        source += 2;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContextGL::DataFormat::RGBA2_10_10_10, uint32_t, float>(const uint32_t* source, float* destination, unsigned pixelsPerRow)
{
    const float rgbScaleFactor = 1.0f / 1023.0f;
    const float alphaScaleFactor = 1.0f / 3.0f;
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        uint32_t packedValue = source[0];
        destination[0] = static_cast<float>(packedValue & 0x3FF) * rgbScaleFactor;
        destination[1] = static_cast<float>((packedValue >> 10) & 0x3FF) * rgbScaleFactor;
        destination[2] = static_cast<float>((packedValue >> 20) & 0x3FF) * rgbScaleFactor;
        destination[3] = static_cast<float>(packedValue >> 30) * alphaScaleFactor;
        source += 1;
        destination += 4;
    }
}

// Unpacking routines needed by various WebKit ports, for example to
// handle the internal representations of grayscale PNGs.

template<> ALWAYS_INLINE void unpack<GraphicsContextGL::DataFormat::R8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[0];
        destination[2] = source[0];
        destination[3] = 0xFF;
        source += 1;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContextGL::DataFormat::A8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = 0x0;
        destination[1] = 0x0;
        destination[2] = 0x0;
        destination[3] = source[0];
        source += 1;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContextGL::DataFormat::R32F, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[0];
        destination[2] = source[0];
        destination[3] = 1;
        source += 1;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContextGL::DataFormat::A32F, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = 0;
        destination[1] = 0;
        destination[2] = 0;
        destination[3] = source[0];
        source += 1;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContextGL::DataFormat::RGB8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[1];
        destination[2] = source[2];
        destination[3] = 0xFF;
        source += 3;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContextGL::DataFormat::BGR8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[2];
        destination[1] = source[1];
        destination[2] = source[0];
        destination[3] = 0xFF;
        source += 3;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContextGL::DataFormat::RGB565, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::unpackOneRowOfRGB565ToRGBA8(source, destination, pixelsPerRow);
#endif
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        uint16_t packedValue = source[0];
        uint8_t r = packedValue >> 11;
        uint8_t g = (packedValue >> 5) & 0x3F;
        uint8_t b = packedValue & 0x1F;
        destination[0] = (r << 3) | (r & 0x7);
        destination[1] = (g << 2) | (g & 0x3);
        destination[2] = (b << 3) | (b & 0x7);
        destination[3] = 0xFF;
        source += 1;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContextGL::DataFormat::RGB32F, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[1];
        destination[2] = source[2];
        destination[3] = 1;
        source += 3;
        destination += 4;
    }
}

//----------------------------------------------------------------------
// Pixel packing routines.
//

template<GraphicsContextGL::DataFormat format, int alphaOp, typename SourceType, typename DstType>
ALWAYS_INLINE void pack(const SourceType*, DstType*, unsigned)
{
    ASSERT_NOT_REACHED();
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::A8, GraphicsContextGL::AlphaOp::DoNothing, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[3];
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::R8, GraphicsContextGL::AlphaOp::DoNothing, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::R8, GraphicsContextGL::AlphaOp::DoPremultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] / 255.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        destination[0] = sourceR;
        source += 4;
        destination += 1;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::R8, GraphicsContextGL::AlphaOp::DoUnmultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        destination[0] = sourceR;
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RA8, GraphicsContextGL::AlphaOp::DoNothing, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[3];
        source += 4;
        destination += 2;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RA8, GraphicsContextGL::AlphaOp::DoPremultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] / 255.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        destination[0] = sourceR;
        destination[1] = source[3];
        source += 4;
        destination += 2;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RA8, GraphicsContextGL::AlphaOp::DoUnmultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        destination[0] = sourceR;
        destination[1] = source[3];
        source += 4;
        destination += 2;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGB8, GraphicsContextGL::AlphaOp::DoNothing, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[1];
        destination[2] = source[2];
        source += 4;
        destination += 3;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGB8, GraphicsContextGL::AlphaOp::DoPremultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] / 255.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        destination[0] = sourceR;
        destination[1] = sourceG;
        destination[2] = sourceB;
        source += 4;
        destination += 3;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGB8, GraphicsContextGL::AlphaOp::DoUnmultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        destination[0] = sourceR;
        destination[1] = sourceG;
        destination[2] = sourceB;
        source += 4;
        destination += 3;
    }
}


template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGBA8, GraphicsContextGL::AlphaOp::DoPremultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] / 255.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        destination[0] = sourceR;
        destination[1] = sourceG;
        destination[2] = sourceB;
        destination[3] = source[3];
        source += 4;
        destination += 4;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGBA8, GraphicsContextGL::AlphaOp::DoUnmultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        destination[0] = sourceR;
        destination[1] = sourceG;
        destination[2] = sourceB;
        destination[3] = source[3];
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGBA4444, GraphicsContextGL::AlphaOp::DoNothing, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::packOneRowOfRGBA8ToUnsignedShort4444(source, destination, pixelsPerRow);
#endif
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        *destination = (((source[0] & 0xF0) << 8)
                        | ((source[1] & 0xF0) << 4)
                        | (source[2] & 0xF0)
                        | (source[3] >> 4));
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGBA4444, GraphicsContextGL::AlphaOp::DoPremultiply, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] / 255.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        *destination = (((sourceR & 0xF0) << 8)
                        | ((sourceG & 0xF0) << 4)
                        | (sourceB & 0xF0)
                        | (source[3] >> 4));
        source += 4;
        destination += 1;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGBA4444, GraphicsContextGL::AlphaOp::DoUnmultiply, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        *destination = (((sourceR & 0xF0) << 8)
                        | ((sourceG & 0xF0) << 4)
                        | (sourceB & 0xF0)
                        | (source[3] >> 4));
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGBA5551, GraphicsContextGL::AlphaOp::DoNothing, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::packOneRowOfRGBA8ToUnsignedShort5551(source, destination, pixelsPerRow);
#endif
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        *destination = (((source[0] & 0xF8) << 8)
                        | ((source[1] & 0xF8) << 3)
                        | ((source[2] & 0xF8) >> 2)
                        | (source[3] >> 7));
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGBA5551, GraphicsContextGL::AlphaOp::DoPremultiply, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] / 255.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        *destination = (((sourceR & 0xF8) << 8)
                        | ((sourceG & 0xF8) << 3)
                        | ((sourceB & 0xF8) >> 2)
                        | (source[3] >> 7));
        source += 4;
        destination += 1;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGBA5551, GraphicsContextGL::AlphaOp::DoUnmultiply, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        *destination = (((sourceR & 0xF8) << 8)
                        | ((sourceG & 0xF8) << 3)
                        | ((sourceB & 0xF8) >> 2)
                        | (source[3] >> 7));
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGB565, GraphicsContextGL::AlphaOp::DoNothing, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::packOneRowOfRGBA8ToUnsignedShort565(source, destination, pixelsPerRow);
#endif
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        *destination = (((source[0] & 0xF8) << 8)
                        | ((source[1] & 0xFC) << 3)
                        | ((source[2] & 0xF8) >> 3));
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGB565, GraphicsContextGL::AlphaOp::DoPremultiply, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] / 255.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        *destination = (((sourceR & 0xF8) << 8)
                        | ((sourceG & 0xFC) << 3)
                        | ((sourceB & 0xF8) >> 3));
        source += 4;
        destination += 1;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGB565, GraphicsContextGL::AlphaOp::DoUnmultiply, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        *destination = (((sourceR & 0xF8) << 8)
                        | ((sourceG & 0xFC) << 3)
                        | ((sourceB & 0xF8) >> 3));
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGB32F, GraphicsContextGL::AlphaOp::DoNothing, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[1];
        destination[2] = source[2];
        source += 4;
        destination += 3;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGB32F, GraphicsContextGL::AlphaOp::DoPremultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        destination[2] = source[2] * scaleFactor;
        source += 4;
        destination += 3;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGB32F, GraphicsContextGL::AlphaOp::DoUnmultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        destination[2] = source[2] * scaleFactor;
        source += 4;
        destination += 3;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGBA32F, GraphicsContextGL::AlphaOp::DoPremultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        destination[2] = source[2] * scaleFactor;
        destination[3] = source[3];
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGBA32F, GraphicsContextGL::AlphaOp::DoUnmultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        destination[2] = source[2] * scaleFactor;
        destination[3] = source[3];
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::A32F, GraphicsContextGL::AlphaOp::DoNothing, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[3];
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::R32F, GraphicsContextGL::AlphaOp::DoNothing, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::R32F, GraphicsContextGL::AlphaOp::DoPremultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = source[0] * scaleFactor;
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::R32F, GraphicsContextGL::AlphaOp::DoUnmultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = source[0] * scaleFactor;
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RA32F, GraphicsContextGL::AlphaOp::DoNothing, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[3];
        source += 4;
        destination += 2;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RA32F, GraphicsContextGL::AlphaOp::DoPremultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[3];
        source += 4;
        destination += 2;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RA32F, GraphicsContextGL::AlphaOp::DoUnmultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[3];
        source += 4;
        destination += 2;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGBA16F, GraphicsContextGL::AlphaOp::DoNothing, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertFloatToHalfFloat(source[0]);
        destination[1] = convertFloatToHalfFloat(source[1]);
        destination[2] = convertFloatToHalfFloat(source[2]);
        destination[3] = convertFloatToHalfFloat(source[3]);
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGBA16F, GraphicsContextGL::AlphaOp::DoPremultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        destination[1] = convertFloatToHalfFloat(source[1] * scaleFactor);
        destination[2] = convertFloatToHalfFloat(source[2] * scaleFactor);
        destination[3] = convertFloatToHalfFloat(source[3]);
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGBA16F, GraphicsContextGL::AlphaOp::DoUnmultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        destination[1] = convertFloatToHalfFloat(source[1] * scaleFactor);
        destination[2] = convertFloatToHalfFloat(source[2] * scaleFactor);
        destination[3] = convertFloatToHalfFloat(source[3]);
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGB16F, GraphicsContextGL::AlphaOp::DoNothing, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertFloatToHalfFloat(source[0]);
        destination[1] = convertFloatToHalfFloat(source[1]);
        destination[2] = convertFloatToHalfFloat(source[2]);
        source += 4;
        destination += 3;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGB16F, GraphicsContextGL::AlphaOp::DoPremultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        destination[1] = convertFloatToHalfFloat(source[1] * scaleFactor);
        destination[2] = convertFloatToHalfFloat(source[2] * scaleFactor);
        source += 4;
        destination += 3;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RGB16F, GraphicsContextGL::AlphaOp::DoUnmultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        destination[1] = convertFloatToHalfFloat(source[1] * scaleFactor);
        destination[2] = convertFloatToHalfFloat(source[2] * scaleFactor);
        source += 4;
        destination += 3;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RA16F, GraphicsContextGL::AlphaOp::DoNothing, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertFloatToHalfFloat(source[0]);
        destination[1] = convertFloatToHalfFloat(source[3]);
        source += 4;
        destination += 2;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RA16F, GraphicsContextGL::AlphaOp::DoPremultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        destination[1] = convertFloatToHalfFloat(source[3]);
        source += 4;
        destination += 2;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::RA16F, GraphicsContextGL::AlphaOp::DoUnmultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        destination[1] = convertFloatToHalfFloat(source[3]);
        source += 4;
        destination += 2;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::R16F, GraphicsContextGL::AlphaOp::DoNothing, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertFloatToHalfFloat(source[0]);
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::R16F, GraphicsContextGL::AlphaOp::DoPremultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::R16F, GraphicsContextGL::AlphaOp::DoUnmultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContextGL::DataFormat::A16F, GraphicsContextGL::AlphaOp::DoNothing, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertFloatToHalfFloat(source[3]);
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE
void pack<GraphicsContextGL::DataFormat::RGBA8_S, GraphicsContextGL::AlphaOp::DoPremultiply, int8_t, int8_t>(const int8_t* source, int8_t* destination, unsigned pixels_per_row)
{
    for (unsigned i = 0; i < pixels_per_row; ++i) {
        destination[3] = clampMin(source[3]);
        float scaleFactor = static_cast<float>(destination[3]) / MaxInt8Value;
        destination[0] = static_cast<int8_t>(static_cast<float>(clampMin(source[0])) * scaleFactor);
        destination[1] = static_cast<int8_t>(static_cast<float>(clampMin(source[1])) * scaleFactor);
        destination[2] = static_cast<int8_t>(static_cast<float>(clampMin(source[2])) * scaleFactor);
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE
void pack<GraphicsContextGL::DataFormat::RGBA16, GraphicsContextGL::AlphaOp::DoPremultiply, uint16_t, uint16_t>(const uint16_t* source, uint16_t* destination, unsigned pixels_per_row)
{
    for (unsigned i = 0; i < pixels_per_row; ++i) {
        float scaleFactor = static_cast<float>(source[3]) / MaxUInt16Value;
        destination[0] = static_cast<uint16_t>(static_cast<float>(source[0]) * scaleFactor);
        destination[1] = static_cast<uint16_t>(static_cast<float>(source[1]) * scaleFactor);
        destination[2] = static_cast<uint16_t>(static_cast<float>(source[2]) * scaleFactor);
        destination[3] = source[3];
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE
void pack<GraphicsContextGL::DataFormat::RGBA16_S, GraphicsContextGL::AlphaOp::DoPremultiply, int16_t, int16_t>(const int16_t* source, int16_t* destination, unsigned pixels_per_row)
{
    for (unsigned i = 0; i < pixels_per_row; ++i) {
        destination[3] = clampMin(source[3]);
        float scaleFactor = static_cast<float>(destination[3]) / MaxInt16Value;
        destination[0] = static_cast<int16_t>(static_cast<float>(clampMin(source[0])) * scaleFactor);
        destination[1] = static_cast<int16_t>(static_cast<float>(clampMin(source[1])) * scaleFactor);
        destination[2] = static_cast<int16_t>(static_cast<float>(clampMin(source[2])) * scaleFactor);
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE
void pack<GraphicsContextGL::DataFormat::RGBA32, GraphicsContextGL::AlphaOp::DoPremultiply, uint32_t, uint32_t>(const uint32_t* source, uint32_t* destination, unsigned pixels_per_row)
{
    for (unsigned i = 0; i < pixels_per_row; ++i) {
        double scaleFactor = static_cast<double>(source[3]) / MaxUInt32Value;
        destination[0] = static_cast<uint32_t>(static_cast<double>(source[0]) * scaleFactor);
        destination[1] = static_cast<uint32_t>(static_cast<double>(source[1]) * scaleFactor);
        destination[2] = static_cast<uint32_t>(static_cast<double>(source[2]) * scaleFactor);
        destination[3] = source[3];
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE
void pack<GraphicsContextGL::DataFormat::RGBA32_S, GraphicsContextGL::AlphaOp::DoPremultiply, int32_t, int32_t>(const int32_t* source, int32_t* destination, unsigned pixels_per_row)
{
    for (unsigned i = 0; i < pixels_per_row; ++i) {
        destination[3] = clampMin(source[3]);
        double scaleFactor = static_cast<double>(destination[3]) / MaxInt32Value;
        destination[0] = static_cast<int32_t>(static_cast<double>(clampMin(source[0])) * scaleFactor);
        destination[1] = static_cast<int32_t>(static_cast<double>(clampMin(source[1])) * scaleFactor);
        destination[2] = static_cast<int32_t>(static_cast<double>(clampMin(source[2])) * scaleFactor);
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE
void pack<GraphicsContextGL::DataFormat::RGBA2_10_10_10, GraphicsContextGL::AlphaOp::DoNothing, float, uint32_t>(const float* source, uint32_t* destination, unsigned pixels_per_row)
{
    for (unsigned i = 0; i < pixels_per_row; ++i) {
        uint32_t r = static_cast<uint32_t>(source[0] * 1023.0f);
        uint32_t g = static_cast<uint32_t>(source[1] * 1023.0f);
        uint32_t b = static_cast<uint32_t>(source[2] * 1023.0f);
        uint32_t a = static_cast<uint32_t>(source[3] * 3.0f);
        destination[0] = (a << 30) | (b << 20) | (g << 10) | r;
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE
void pack<GraphicsContextGL::DataFormat::RGBA2_10_10_10, GraphicsContextGL::AlphaOp::DoPremultiply, float, uint32_t>(const float* source, uint32_t* destination, unsigned pixels_per_row)
{
    for (unsigned i = 0; i < pixels_per_row; ++i) {
        uint32_t r = static_cast<uint32_t>(source[0] * source[3] * 1023.0f);
        uint32_t g = static_cast<uint32_t>(source[1] * source[3] * 1023.0f);
        uint32_t b = static_cast<uint32_t>(source[2] * source[3] * 1023.0f);
        uint32_t a = static_cast<uint32_t>(source[3] * 3.0f);
        destination[0] = (a << 30) | (b << 20) | (g << 10) | r;
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE
void pack<GraphicsContextGL::DataFormat::RGBA2_10_10_10, GraphicsContextGL::AlphaOp::DoUnmultiply, float, uint32_t>(const float* source, uint32_t* destination, unsigned pixels_per_row)
{
    for (unsigned i = 0; i < pixels_per_row; ++i) {
        float scaleFactor = source[3] ? 1023.0f / source[3] : 1023.0f;
        uint32_t r = static_cast<uint32_t>(source[0] * scaleFactor);
        uint32_t g = static_cast<uint32_t>(source[1] * scaleFactor);
        uint32_t b = static_cast<uint32_t>(source[2] * scaleFactor);
        uint32_t a = static_cast<uint32_t>(source[3] * 3.0f);
        destination[0] = (a << 30) | (b << 20) | (g << 10) | r;
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE
void pack<GraphicsContextGL::DataFormat::RG8, GraphicsContextGL::AlphaOp::DoNothing, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixels_per_row)
{
    for (unsigned i = 0; i < pixels_per_row; ++i) {
        destination[0] = source[0];
        destination[1] = source[1];
        source += 4;
        destination += 2;
    }
}

template<> ALWAYS_INLINE
void pack<GraphicsContextGL::DataFormat::RG8, GraphicsContextGL::AlphaOp::DoPremultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixels_per_row)
{
    for (unsigned i = 0; i < pixels_per_row; ++i) {
        float scaleFactor = static_cast<float>(source[3]) / MaxUInt8Value;
        destination[0] = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        destination[1] = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        source += 4;
        destination += 2;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> ALWAYS_INLINE
void pack<GraphicsContextGL::DataFormat::RG8, GraphicsContextGL::AlphaOp::DoUnmultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixels_per_row)
{
    for (unsigned i = 0; i < pixels_per_row; ++i) {
        float scaleFactor = source[3] ? MaxUInt8Value / static_cast<float>(source[3]) : 1.0f;
        destination[0] = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        destination[1] = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        source += 4;
        destination += 2;
    }
}

template<> ALWAYS_INLINE
void pack<GraphicsContextGL::DataFormat::RG16F, GraphicsContextGL::AlphaOp::DoNothing, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixels_per_row)
{
    for (unsigned i = 0; i < pixels_per_row; ++i) {
        destination[0] = convertFloatToHalfFloat(source[0]);
        destination[1] = convertFloatToHalfFloat(source[1]);
        source += 4;
        destination += 2;
    }
}

template<> ALWAYS_INLINE
void pack<GraphicsContextGL::DataFormat::RG16F, GraphicsContextGL::AlphaOp::DoPremultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixels_per_row)
{
    for (unsigned i = 0; i < pixels_per_row; ++i) {
        float scaleFactor = source[3];
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        destination[1] = convertFloatToHalfFloat(source[1] * scaleFactor);
        source += 4;
        destination += 2;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> ALWAYS_INLINE
void pack<GraphicsContextGL::DataFormat::RG16F, GraphicsContextGL::AlphaOp::DoUnmultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixels_per_row)
{
    for (unsigned i = 0; i < pixels_per_row; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        destination[1] = convertFloatToHalfFloat(source[1] * scaleFactor);
        source += 4;
        destination += 2;
    }
}

template<> ALWAYS_INLINE
void pack<GraphicsContextGL::DataFormat::RG32F, GraphicsContextGL::AlphaOp::DoNothing, float, float>(const float* source, float* destination, unsigned pixels_per_row)
{
    for (unsigned i = 0; i < pixels_per_row; ++i) {
        destination[0] = source[0];
        destination[1] = source[1];
        source += 4;
        destination += 2;
    }
}

template<> ALWAYS_INLINE
void pack<GraphicsContextGL::DataFormat::RG32F, GraphicsContextGL::AlphaOp::DoPremultiply, float, float>(const float* source, float* destination, unsigned pixels_per_row)
{
    for (unsigned i = 0; i < pixels_per_row; ++i) {
        float scaleFactor = source[3];
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        source += 4;
        destination += 2;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> ALWAYS_INLINE
void pack<GraphicsContextGL::DataFormat::RG32F, GraphicsContextGL::AlphaOp::DoUnmultiply, float, float>(const float* source, float* destination, unsigned pixels_per_row)
{
    for (unsigned i = 0; i < pixels_per_row; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        source += 4;
        destination += 2;
    }
}

template <GraphicsContextGL::DataFormat Format>
struct IsInt8Format {
    static const bool Value =
        Format == GraphicsContextGL::DataFormat::RGBA8_S
        || Format == GraphicsContextGL::DataFormat::RGB8_S
        || Format == GraphicsContextGL::DataFormat::RG8_S
        || Format == GraphicsContextGL::DataFormat::R8_S;
};

template <GraphicsContextGL::DataFormat Format>
struct IsInt16Format {
    static const bool Value =
        Format == GraphicsContextGL::DataFormat::RGBA16_S
        || Format == GraphicsContextGL::DataFormat::RGB16_S
        || Format == GraphicsContextGL::DataFormat::RG16_S
        || Format == GraphicsContextGL::DataFormat::R16_S;
};

template <GraphicsContextGL::DataFormat Format>
struct IsInt32Format {
    static const bool Value =
        Format == GraphicsContextGL::DataFormat::RGBA32_S
        || Format == GraphicsContextGL::DataFormat::RGB32_S
        || Format == GraphicsContextGL::DataFormat::RG32_S
        || Format == GraphicsContextGL::DataFormat::R32_S;
};

template <GraphicsContextGL::DataFormat Format>
struct IsUInt8Format {
    static const bool Value =
        Format == GraphicsContextGL::DataFormat::RGBA8
        || Format == GraphicsContextGL::DataFormat::RGB8
        || Format == GraphicsContextGL::DataFormat::RG8
        || Format == GraphicsContextGL::DataFormat::R8
        || Format == GraphicsContextGL::DataFormat::BGRA8
        || Format == GraphicsContextGL::DataFormat::BGR8
        || Format == GraphicsContextGL::DataFormat::ARGB8
        || Format == GraphicsContextGL::DataFormat::ABGR8
        || Format == GraphicsContextGL::DataFormat::RA8
        || Format == GraphicsContextGL::DataFormat::AR8
        || Format == GraphicsContextGL::DataFormat::A8;
};

template <GraphicsContextGL::DataFormat Format>
struct IsUInt16Format {
    static const bool Value = Format == GraphicsContextGL::DataFormat::RGBA16
        || Format == GraphicsContextGL::DataFormat::RGB16
        || Format == GraphicsContextGL::DataFormat::RG16
        || Format == GraphicsContextGL::DataFormat::R16;
};

template <GraphicsContextGL::DataFormat Format>
struct IsUInt32Format {
    static const bool Value =
        Format == GraphicsContextGL::DataFormat::RGBA32
        || Format == GraphicsContextGL::DataFormat::RGB32
        || Format == GraphicsContextGL::DataFormat::RG32
        || Format == GraphicsContextGL::DataFormat::R32;
};

template<GraphicsContextGL::DataFormat Format>
struct IsFloatFormat {
    static const bool Value =
        Format == GraphicsContextGL::DataFormat::RGBA32F
        || Format == GraphicsContextGL::DataFormat::RGB32F
        || Format == GraphicsContextGL::DataFormat::RA32F
        || Format == GraphicsContextGL::DataFormat::R32F
        || Format == GraphicsContextGL::DataFormat::A32F
        || Format == GraphicsContextGL::DataFormat::RG32F;
};

template<GraphicsContextGL::DataFormat Format>
struct IsHalfFloatFormat {
    static const bool Value =
        Format == GraphicsContextGL::DataFormat::RGBA16F
        || Format == GraphicsContextGL::DataFormat::RGB16F
        || Format == GraphicsContextGL::DataFormat::RA16F
        || Format == GraphicsContextGL::DataFormat::R16F
        || Format == GraphicsContextGL::DataFormat::A16F
        || Format == GraphicsContextGL::DataFormat::RG16F;
};

template<GraphicsContextGL::DataFormat Format>
struct Is32bppFormat {
    static const bool Value =
        Format == GraphicsContextGL::DataFormat::RGBA2_10_10_10
        || Format == GraphicsContextGL::DataFormat::RGB5999
        || Format == GraphicsContextGL::DataFormat::RGB10F11F11F;
};

template<GraphicsContextGL::DataFormat Format>
struct Is16bppFormat {
    static const bool Value =
        Format == GraphicsContextGL::DataFormat::RGBA5551
        || Format == GraphicsContextGL::DataFormat::RGBA4444
        || Format == GraphicsContextGL::DataFormat::RGB565;
};

template<GraphicsContextGL::DataFormat Format,
    bool IsInt8Format = IsInt8Format<Format>::Value,
    bool IsUInt8Format = IsUInt8Format<Format>::Value,
    bool IsInt16Format = IsInt16Format<Format>::Value,
    bool IsUInt16Format = IsUInt16Format<Format>::Value,
    bool IsInt32Format = IsInt32Format<Format>::Value,
    bool IsUInt32Format = IsUInt32Format<Format>::Value,
    bool IsFloat = IsFloatFormat<Format>::Value,
    bool IsHalfFloat = IsHalfFloatFormat<Format>::Value,
    bool Is16bpp = Is16bppFormat<Format>::Value,
    bool Is32bpp = Is32bppFormat<Format>::Value>
struct DataTypeForFormat {
    typedef double Type; // Use a type that's not used in unpack/pack.
};

template <GraphicsContextGL::DataFormat Format>
struct DataTypeForFormat<Format,
    true,
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false> {
    typedef int8_t Type;
};

template <GraphicsContextGL::DataFormat Format>
struct DataTypeForFormat<Format,
    false,
    true,
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false> {
    typedef uint8_t Type;
};

template <GraphicsContextGL::DataFormat Format>
struct DataTypeForFormat<Format,
    false,
    false,
    true,
    false,
    false,
    false,
    false,
    false,
    false,
    false> {
    typedef int16_t Type;
};

template <GraphicsContextGL::DataFormat Format>
struct DataTypeForFormat<Format,
    false,
    false,
    false,
    true,
    false,
    false,
    false,
    false,
    false,
    false> {
    typedef uint16_t Type;
};

template <GraphicsContextGL::DataFormat Format>
struct DataTypeForFormat<Format,
    false,
    false,
    false,
    false,
    true,
    false,
    false,
    false,
    false,
    false> {
    typedef int32_t Type;
};

template <GraphicsContextGL::DataFormat Format>
struct DataTypeForFormat<Format,
    false,
    false,
    false,
    false,
    false,
    true,
    false,
    false,
    false,
    false> {
    typedef uint32_t Type;
};

template <GraphicsContextGL::DataFormat Format>
struct DataTypeForFormat<Format,
    false,
    false,
    false,
    false,
    false,
    false,
    true,
    false,
    false,
    false> {
    typedef float Type;
};

template <GraphicsContextGL::DataFormat Format>
struct DataTypeForFormat<Format,
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    true,
    false,
    false> {
    typedef uint16_t Type;
};

template <GraphicsContextGL::DataFormat Format>
struct DataTypeForFormat<Format,
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    true,
    false> {
    typedef uint16_t Type;
};

template <GraphicsContextGL::DataFormat Format>
struct DataTypeForFormat<Format,
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    true> {
    typedef uint32_t Type;
};

template <GraphicsContextGL::DataFormat Format>
struct UsesFloatIntermediateFormat {
    static const bool Value =
        IsFloatFormat<Format>::Value || IsHalfFloatFormat<Format>::Value
        || Format == GraphicsContextGL::DataFormat::RGBA2_10_10_10
        || Format == GraphicsContextGL::DataFormat::RGB10F11F11F
        || Format == GraphicsContextGL::DataFormat::RGB5999;
};

template <GraphicsContextGL::DataFormat Format>
struct IntermediateFormat {
    static const GraphicsContextGL::DataFormat Value =
        UsesFloatIntermediateFormat<Format>::Value
            ? GraphicsContextGL::DataFormat::RGBA32F
            : IsInt32Format<Format>::Value
                ? GraphicsContextGL::DataFormat::RGBA32_S
                : IsUInt32Format<Format>::Value
                    ? GraphicsContextGL::DataFormat::RGBA32
                    : IsInt16Format<Format>::Value
                        ? GraphicsContextGL::DataFormat::RGBA16_S
                        : (IsUInt16Format<Format>::Value || Is32bppFormat<Format>::Value)
                            ? GraphicsContextGL::DataFormat::RGBA16
                            : IsInt8Format<Format>::Value
                                ? GraphicsContextGL::DataFormat::RGBA8_S
                                : GraphicsContextGL::DataFormat::RGBA8;
};

/* END CODE SHARED WITH MOZILLA FIREFOX */

void FormatConverter::convert(GraphicsContextGL::DataFormat srcFormat, GraphicsContextGL::DataFormat dstFormat, GraphicsContextGL::AlphaOp alphaOp)
{
#define FORMATCONVERTER_CASE_SRCFORMAT(SrcFormat) \
    case SrcFormat: \
        return convert<SrcFormat>(dstFormat, alphaOp);

        switch (srcFormat) {
            // These are all of the input formats that potentially have to apply a non-trivial AlphaOp.
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContextGL::DataFormat::RA8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContextGL::DataFormat::RA32F)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContextGL::DataFormat::RGBA8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContextGL::DataFormat::ARGB8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContextGL::DataFormat::ABGR8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContextGL::DataFormat::AR8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContextGL::DataFormat::BGRA8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContextGL::DataFormat::RGBA5551)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContextGL::DataFormat::RGBA4444)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContextGL::DataFormat::RGBA32F)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContextGL::DataFormat::RGBA2_10_10_10)
            // Additional input formats needed by various WebKit ports.
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContextGL::DataFormat::R8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContextGL::DataFormat::A8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContextGL::DataFormat::R32F)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContextGL::DataFormat::A32F)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContextGL::DataFormat::RGB8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContextGL::DataFormat::BGR8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContextGL::DataFormat::RGB565)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContextGL::DataFormat::RGB32F)
        default:
            ASSERT_NOT_REACHED();
        }
#undef FORMATCONVERTER_CASE_SRCFORMAT
}

template<GraphicsContextGL::DataFormat SrcFormat>
ALWAYS_INLINE void FormatConverter::convert(GraphicsContextGL::DataFormat dstFormat, GraphicsContextGL::AlphaOp alphaOp)
{
#define FORMATCONVERTER_CASE_DSTFORMAT(DstFormat) \
    case DstFormat: \
        return convert<SrcFormat, DstFormat>(alphaOp);

        switch (dstFormat) {
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::R8)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::R16F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::R32F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::A8)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::A16F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::A32F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::RA8)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::RA16F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::RA32F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::RGB8)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::RGB565)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::RGB16F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::RGB32F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::RGBA8)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::RGBA5551)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::RGBA4444)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::RGBA16F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::RGBA32F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::RGBA8_S)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::RGBA16)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::RGBA16_S)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::RGBA32)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::RGBA32_S)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::RGBA2_10_10_10)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::RG8)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::RG16F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContextGL::DataFormat::RG32F)
        default:
            ASSERT_NOT_REACHED();
        }

#undef FORMATCONVERTER_CASE_DSTFORMAT
}

template<GraphicsContextGL::DataFormat SrcFormat, GraphicsContextGL::DataFormat DstFormat>
ALWAYS_INLINE void FormatConverter::convert(GraphicsContextGL::AlphaOp alphaOp)
{
#define FORMATCONVERTER_CASE_ALPHAOP(alphaOp) \
    case alphaOp: \
        return convert<SrcFormat, DstFormat, alphaOp>();

        switch (alphaOp) {
            FORMATCONVERTER_CASE_ALPHAOP(GraphicsContextGL::AlphaOp::DoNothing)
            FORMATCONVERTER_CASE_ALPHAOP(GraphicsContextGL::AlphaOp::DoPremultiply)
            FORMATCONVERTER_CASE_ALPHAOP(GraphicsContextGL::AlphaOp::DoUnmultiply)
        default:
            ASSERT_NOT_REACHED();
        }
#undef FORMATCONVERTER_CASE_ALPHAOP
}

template <GraphicsContextGL::DataFormat Format>
struct SupportsConversionFromDomElements {
    static const bool Value = (Format == GraphicsContextGL::DataFormat::RGBA8
        || Format == GraphicsContextGL::DataFormat::RGB8
        || Format == GraphicsContextGL::DataFormat::RG8
        || Format == GraphicsContextGL::DataFormat::RA8
        || Format == GraphicsContextGL::DataFormat::R8
        || Format == GraphicsContextGL::DataFormat::RGBA32F
        || Format == GraphicsContextGL::DataFormat::RGB32F
        || Format == GraphicsContextGL::DataFormat::RG32F
        || Format == GraphicsContextGL::DataFormat::RA32F
        || Format == GraphicsContextGL::DataFormat::R32F
        || Format == GraphicsContextGL::DataFormat::RGBA16F
        || Format == GraphicsContextGL::DataFormat::RGB16F
        || Format == GraphicsContextGL::DataFormat::RG16F
        || Format == GraphicsContextGL::DataFormat::RA16F
        || Format == GraphicsContextGL::DataFormat::R16F
        || Format == GraphicsContextGL::DataFormat::RGBA5551
        || Format == GraphicsContextGL::DataFormat::RGBA4444
        || Format == GraphicsContextGL::DataFormat::RGB565
        || Format == GraphicsContextGL::DataFormat::RGBA2_10_10_10);
};

// Visual Studio crashes with a C1063 Fatal Error if everything is inlined.
template<GraphicsContextGL::DataFormat SrcFormat, GraphicsContextGL::DataFormat DstFormat, GraphicsContextGL::AlphaOp alphaOp>
ALWAYS_INLINE_EXCEPT_MSVC void FormatConverter::convert()
{
    // Many instantiations of this template function will never be entered, so we
    // try to return immediately in these cases to avoid generating useless code.
    if (SrcFormat == DstFormat && alphaOp == GraphicsContextGL::AlphaOp::DoNothing) {
        ASSERT_NOT_REACHED();
        return;
    }
    if (!IsFloatFormat<DstFormat>::Value && IsFloatFormat<SrcFormat>::Value) {
        ASSERT_NOT_REACHED();
        return;
    }

    // Only textures uploaded from DOM elements or ImageData can allow DstFormat != SrcFormat.
    const bool srcFormatComesFromDOMElementOrImageData = GraphicsContextGL::srcFormatComesFromDOMElementOrImageData(SrcFormat);
    if (!srcFormatComesFromDOMElementOrImageData && SrcFormat != DstFormat) {
        ASSERT_NOT_REACHED();
        return;
    }
    // Likewise, only textures uploaded from DOM elements or ImageData can possibly have to be unpremultiplied.
    if (!srcFormatComesFromDOMElementOrImageData && alphaOp == GraphicsContextGL::AlphaOp::DoUnmultiply) {
        ASSERT_NOT_REACHED();
        return;
    }
    if (srcFormatComesFromDOMElementOrImageData && alphaOp == GraphicsContextGL::AlphaOp::DoUnmultiply && !SupportsConversionFromDomElements<DstFormat>::Value) {
        ASSERT_NOT_REACHED();
        return;
    }
    if ((!GraphicsContextGL::hasAlpha(SrcFormat) || !GraphicsContextGL::hasColor(SrcFormat) || !GraphicsContextGL::hasColor(DstFormat)) && alphaOp != GraphicsContextGL::AlphaOp::DoNothing) {
        ASSERT_NOT_REACHED();
        return;
    }
    // If converting DOM element data to UNSIGNED_INT_5_9_9_9_REV or
    // UNSIGNED_INT_10F_11F_11F_REV, we should always switch to FLOAT instead to
    // avoid unpacking/packing these two types.
    if (srcFormatComesFromDOMElementOrImageData && SrcFormat != DstFormat
        && (DstFormat == GraphicsContextGL::DataFormat::RGB5999 || DstFormat == GraphicsContextGL::DataFormat::RGB10F11F11F)) {
        ASSERT_NOT_REACHED();
        return;
    }

    typedef typename DataTypeForFormat<SrcFormat>::Type SrcType;
    typedef typename DataTypeForFormat<DstFormat>::Type DstType;
    const GraphicsContextGL::DataFormat IntermediateSrcFormat = IntermediateFormat<DstFormat>::Value;
    typedef typename DataTypeForFormat<IntermediateSrcFormat>::Type IntermediateSrcType;
    // Stride here could be negative.
    const ptrdiff_t srcStrideInElements = m_srcStride / sizeof(SrcType);
    const ptrdiff_t dstStrideInElements = m_dstStride / sizeof(DstType);
    const bool trivialUnpack = SrcFormat == IntermediateSrcFormat;
    const bool trivialPack = DstFormat == IntermediateSrcFormat && alphaOp == GraphicsContextGL::AlphaOp::DoNothing;
    ASSERT(!trivialUnpack || !trivialPack);

    const SrcType* srcRowStart =
        static_cast<const SrcType*>(static_cast<const void*>(
        static_cast<const uint8_t*>(m_srcStart) +
        ((m_srcStride * m_srcSubRectangle.y()) + m_srcRowOffset)));

    // If packing multiple images into a 3D texture, and flipY is true,
    // then the sub-rectangle is pointing at the start of the
    // "bottommost" of those images. Since the source pointer strides in
    // the positive direction, we need to back it up to point at the
    // last, or "topmost", of these images.
    if (m_dstStride < 0 && m_depth > 1)
        srcRowStart -= (m_depth - 1) * srcStrideInElements * m_unpackImageHeight;

    DstType* dstRowStart = static_cast<DstType*>(m_dstStart);
    if (trivialUnpack) {
        for (int d = 0; d < m_depth; ++d) {
            for (int i = 0; i < m_srcSubRectangle.height(); ++i) {
                pack<DstFormat, alphaOp>(srcRowStart, dstRowStart,
                    m_srcSubRectangle.width());
                srcRowStart += srcStrideInElements;
                dstRowStart += dstStrideInElements;
            }
            srcRowStart += srcStrideInElements * (m_unpackImageHeight - m_srcSubRectangle.height());
        }
    } else if (trivialPack) {
        for (int d = 0; d < m_depth; ++d) {
            for (int i = 0; i < m_srcSubRectangle.height(); ++i) {
                unpack<SrcFormat>(srcRowStart, dstRowStart,
                    m_srcSubRectangle.width());
                srcRowStart += srcStrideInElements;
                dstRowStart += dstStrideInElements;
            }
            srcRowStart += srcStrideInElements * (m_unpackImageHeight - m_srcSubRectangle.height());
        }
    } else {
        for (int d = 0; d < m_depth; ++d) {
            for (int i = 0; i < m_srcSubRectangle.height(); ++i) {
                unpack<SrcFormat>(srcRowStart,
                    reinterpret_cast<IntermediateSrcType*>(m_unpackedIntermediateSrcData.get()),
                    m_srcSubRectangle.width());
                pack<DstFormat, alphaOp>(reinterpret_cast<IntermediateSrcType*>(m_unpackedIntermediateSrcData.get()),
                    dstRowStart, m_srcSubRectangle.width());
                srcRowStart += srcStrideInElements;
                dstRowStart += dstStrideInElements;
            }
            srcRowStart += srcStrideInElements * (m_unpackImageHeight - m_srcSubRectangle.height());
        }
    }
    m_success = true;
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
