//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// copyvertex.h: Defines vertex buffer copying and conversion functions

#ifndef LIBGLESV2_RENDERER_COPYVERTEX_H_
#define LIBGLESV2_RENDERER_COPYVERTEX_H_

#include "common/mathutil.h"

// 'widenDefaultValueBits' gives the default value for the alpha channel (4th component)
//  the sentinel value 0 means we do not want to widen the input or add an alpha channel
template <typename T, unsigned int componentCount, unsigned int widenDefaultValueBits>
inline void copyVertexData(const void *input, size_t stride, size_t count, void *output)
{
    const unsigned int attribSize = sizeof(T) * componentCount;
    const T defaultValue = gl::bitCast<T>(widenDefaultValueBits);
    const bool widen = (widenDefaultValueBits != 0);

    if (attribSize == stride && !widen)
    {
        memcpy(output, input, count * attribSize);
    }
    else
    {
        unsigned int outputStride = widen ? 4 : componentCount;

        for (unsigned int i = 0; i < count; i++)
        {
            const T *offsetInput = reinterpret_cast<const T*>(reinterpret_cast<const char*>(input) + i * stride);
            T *offsetOutput = reinterpret_cast<T*>(output) + i * outputStride;

            for (unsigned int j = 0; j < componentCount; j++)
            {
                offsetOutput[j] = offsetInput[j];
            }

            if (widen)
            {
                offsetOutput[3] = defaultValue;
            }
        }
    }
}

template <unsigned int componentCount>
inline void copyFixedVertexData(const void* input, size_t stride, size_t count, void* output)
{
    static const float divisor = 1.0f / (1 << 16);

    for (unsigned int i = 0; i < count; i++)
    {
        const GLfixed* offsetInput = reinterpret_cast<const GLfixed*>(reinterpret_cast<const char*>(input) + stride * i);
        float* offsetOutput = reinterpret_cast<float*>(output) + i * componentCount;

        for (unsigned int j = 0; j < componentCount; j++)
        {
            offsetOutput[j] = static_cast<float>(offsetInput[j]) * divisor;
        }
    }
}

template <typename T, unsigned int componentCount, bool normalized>
inline void copyToFloatVertexData(const void* input, size_t stride, size_t count, void* output)
{
    typedef std::numeric_limits<T> NL;

    for (unsigned int i = 0; i < count; i++)
    {
        const T *offsetInput = reinterpret_cast<const T*>(reinterpret_cast<const char*>(input) + stride * i);
        float *offsetOutput = reinterpret_cast<float*>(output) + i * componentCount;

        for (unsigned int j = 0; j < componentCount; j++)
        {
            if (normalized)
            {
                if (NL::is_signed)
                {
                    const float divisor = 1.0f / (2 * static_cast<float>(NL::max()) + 1);
                    offsetOutput[j] = (2 * static_cast<float>(offsetInput[j]) + 1) * divisor;
                }
                else
                {
                    offsetOutput[j] =  static_cast<float>(offsetInput[j]) / NL::max();
                }
            }
            else
            {
                offsetOutput[j] =  static_cast<float>(offsetInput[j]);
            }
        }
    }
}

inline void copyPackedUnsignedVertexData(const void* input, size_t stride, size_t count, void* output)
{
    const unsigned int attribSize = 4;

    if (attribSize == stride)
    {
        memcpy(output, input, count * attribSize);
    }
    else
    {
        for (unsigned int i = 0; i < count; i++)
        {
            const GLuint *offsetInput = reinterpret_cast<const GLuint*>(reinterpret_cast<const char*>(input) + (i * stride));
            GLuint *offsetOutput = reinterpret_cast<GLuint*>(output) + (i * attribSize);

            offsetOutput[i] = offsetInput[i];
        }
    }
}

template <bool isSigned, bool normalized, bool toFloat>
static inline void copyPackedRGB(unsigned int data, void *output)
{
    const unsigned int rgbSignMask = 0x200;       // 1 set at the 9 bit
    const unsigned int negativeMask = 0xFFFFFC00; // All bits from 10 to 31 set to 1

    if (toFloat)
    {
        GLfloat *floatOutput = reinterpret_cast<GLfloat*>(output);
        if (isSigned)
        {
            GLfloat finalValue = 0;
            if (data & rgbSignMask)
            {
                int negativeNumber = data | negativeMask;
                finalValue = static_cast<GLfloat>(negativeNumber);
            }
            else
            {
                finalValue = static_cast<GLfloat>(data);
            }

            if (normalized)
            {
                const int maxValue = 0x1FF;      // 1 set in bits 0 through 8
                const int minValue = 0xFFFFFE01; // Inverse of maxValue

                // A 10-bit two's complement number has the possibility of being minValue - 1 but
                // OpenGL's normalization rules dictate that it should be clamped to minValue in this
                // case.
                if (finalValue < minValue)
                {
                    finalValue = minValue;
                }

                const int halfRange = (maxValue - minValue) >> 1;
                *floatOutput = ((finalValue - minValue) / halfRange) - 1.0f;
            }
            else
            {
                *floatOutput = finalValue;
            }
        }
        else
        {
            if (normalized)
            {
                const unsigned int maxValue = 0x3FF; // 1 set in bits 0 through 9
                *floatOutput = static_cast<GLfloat>(data) / static_cast<GLfloat>(maxValue);
            }
            else
            {
                *floatOutput = static_cast<GLfloat>(data);
            }
        }
    }
    else
    {
        if (isSigned)
        {
            GLshort *intOutput = reinterpret_cast<GLshort*>(output);

            if (data & rgbSignMask)
            {
                *intOutput = data | negativeMask;
            }
            else
            {
                *intOutput = data;
            }
        }
        else
        {
            GLushort *uintOutput = reinterpret_cast<GLushort*>(output);
            *uintOutput = data;
        }
    }
}

template <bool isSigned, bool normalized, bool toFloat>
inline void copyPackedAlpha(unsigned int data, void *output)
{
    if (toFloat)
    {
        GLfloat *floatOutput = reinterpret_cast<GLfloat*>(output);
        if (isSigned)
        {
            if (normalized)
            {
                switch (data)
                {
                  case 0x0: *floatOutput =  0.0f; break;
                  case 0x1: *floatOutput =  1.0f; break;
                  case 0x2: *floatOutput = -1.0f; break;
                  case 0x3: *floatOutput = -1.0f; break;
                  default: UNREACHABLE();
                }
            }
            else
            {
                switch (data)
                {
                  case 0x0: *floatOutput =  0.0f; break;
                  case 0x1: *floatOutput =  1.0f; break;
                  case 0x2: *floatOutput = -2.0f; break;
                  case 0x3: *floatOutput = -1.0f; break;
                  default: UNREACHABLE();
                }
            }
        }
        else
        {
            if (normalized)
            {
                switch (data)
                {
                  case 0x0: *floatOutput = 0.0f / 3.0f; break;
                  case 0x1: *floatOutput = 1.0f / 3.0f; break;
                  case 0x2: *floatOutput = 2.0f / 3.0f; break;
                  case 0x3: *floatOutput = 3.0f / 3.0f; break;
                  default: UNREACHABLE();
                }
            }
            else
            {
                switch (data)
                {
                  case 0x0: *floatOutput = 0.0f; break;
                  case 0x1: *floatOutput = 1.0f; break;
                  case 0x2: *floatOutput = 2.0f; break;
                  case 0x3: *floatOutput = 3.0f; break;
                  default: UNREACHABLE();
                }
            }
        }
    }
    else
    {
        if (isSigned)
        {
            GLshort *intOutput = reinterpret_cast<GLshort*>(output);
            switch (data)
            {
              case 0x0: *intOutput =  0; break;
              case 0x1: *intOutput =  1; break;
              case 0x2: *intOutput = -2; break;
              case 0x3: *intOutput = -1; break;
              default: UNREACHABLE();
            }
        }
        else
        {
            GLushort *uintOutput = reinterpret_cast<GLushort*>(output);
            switch (data)
            {
              case 0x0: *uintOutput = 0; break;
              case 0x1: *uintOutput = 1; break;
              case 0x2: *uintOutput = 2; break;
              case 0x3: *uintOutput = 3; break;
              default: UNREACHABLE();
            }
        }
    }
}

template <bool isSigned, bool normalized, bool toFloat>
inline void copyPackedVertexData(const void* input, size_t stride, size_t count, void* output)
{
    const unsigned int outputComponentSize = toFloat ? 4 : 2;
    const unsigned int componentCount = 4;

    const unsigned int rgbMask = 0x3FF; // 1 set in bits 0 through 9
    const unsigned int redShift = 0;    // red is bits 0 through 9
    const unsigned int greenShift = 10; // green is bits 10 through 19
    const unsigned int blueShift = 20;  // blue is bits 20 through 29

    const unsigned int alphaMask = 0x3; // 1 set in bits 0 and 1
    const unsigned int alphaShift = 30; // Alpha is the 30 and 31 bits

    for (unsigned int i = 0; i < count; i++)
    {
        GLuint packedValue = *reinterpret_cast<const GLuint*>(reinterpret_cast<const char*>(input) + (i * stride));
        GLbyte *offsetOutput = reinterpret_cast<GLbyte*>(output) + (i * outputComponentSize * componentCount);

        copyPackedRGB<isSigned, normalized, toFloat>(  (packedValue >> redShift) & rgbMask,     offsetOutput + (0 * outputComponentSize));
        copyPackedRGB<isSigned, normalized, toFloat>(  (packedValue >> greenShift) & rgbMask,   offsetOutput + (1 * outputComponentSize));
        copyPackedRGB<isSigned, normalized, toFloat>(  (packedValue >> blueShift) & rgbMask,    offsetOutput + (2 * outputComponentSize));
        copyPackedAlpha<isSigned, normalized, toFloat>((packedValue >> alphaShift) & alphaMask, offsetOutput + (3* outputComponentSize));
    }
}

#endif // LIBGLESV2_RENDERER_COPYVERTEX_H_
