//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// copyvertex.inc.h: Implementation of vertex buffer copying and conversion functions

namespace rx
{

// Returns an aligned buffer to read the input from
template <typename T, size_t inputComponentCount>
inline const T *GetAlignedOffsetInput(const T *offsetInput, T *alignedElement)
{
    if (reinterpret_cast<uintptr_t>(offsetInput) % sizeof(T) != 0)
    {
        // Applications may pass in arbitrarily aligned buffers as input.
        // Certain architectures have restrictions regarding unaligned reads. Specifically, we crash
        // on armeabi-v7a devices with a SIGBUS error when performing such operations. arm64 and
        // x86-64 devices do not appear to have such issues.
        //
        // The workaround is to detect if the input buffer is unaligned and if so, perform a
        // byte-wise copy of the unaligned portion and a memcpy of the rest of the buffer.
        uint8_t *alignedBuffer               = reinterpret_cast<uint8_t *>(&alignedElement[0]);
        uintptr_t unalignedInputStartAddress = reinterpret_cast<uintptr_t>(offsetInput);
        constexpr size_t kAlignmentMinusOne  = sizeof(T) - 1;
        uintptr_t alignedInputStartAddress =
            (reinterpret_cast<uintptr_t>(offsetInput) + kAlignmentMinusOne) & ~(kAlignmentMinusOne);
        ASSERT(alignedInputStartAddress >= unalignedInputStartAddress);

        const size_t totalBytesToCopy     = sizeof(T) * inputComponentCount;
        const size_t unalignedBytesToCopy = alignedInputStartAddress - unalignedInputStartAddress;
        ASSERT(totalBytesToCopy >= unalignedBytesToCopy);

        // byte-wise copy of unaligned portion
        for (size_t i = 0; i < unalignedBytesToCopy; i++)
        {
            alignedBuffer[i] = reinterpret_cast<const uint8_t *>(&offsetInput[0])[i];
        }

        // memcpy remaining buffer
        memcpy(&alignedBuffer[unalignedBytesToCopy],
               &reinterpret_cast<const uint8_t *>(&offsetInput[0])[unalignedBytesToCopy],
               totalBytesToCopy - unalignedBytesToCopy);

        return alignedElement;
    }
    else
    {
        return offsetInput;
    }
}

template <typename T,
          size_t inputComponentCount,
          size_t outputComponentCount,
          uint32_t alphaDefaultValueBits>
inline void CopyNativeVertexData(const uint8_t *input, size_t stride, size_t count, uint8_t *output)
{
    const size_t attribSize = sizeof(T) * inputComponentCount;

    if (attribSize == stride && inputComponentCount == outputComponentCount)
    {
        memcpy(output, input, count * attribSize);
        return;
    }

    if (inputComponentCount == outputComponentCount)
    {
        for (size_t i = 0; i < count; i++)
        {
            const T *offsetInput = reinterpret_cast<const T *>(input + (i * stride));
            T offsetInputAligned[inputComponentCount];
            offsetInput =
                GetAlignedOffsetInput<T, inputComponentCount>(offsetInput, &offsetInputAligned[0]);

            T *offsetOutput = reinterpret_cast<T *>(output) + i * outputComponentCount;

            memcpy(offsetOutput, offsetInput, attribSize);
        }
        return;
    }

    const T defaultAlphaValue                = gl::bitCast<T>(alphaDefaultValueBits);
    const size_t lastNonAlphaOutputComponent = std::min<size_t>(outputComponentCount, 3);

    for (size_t i = 0; i < count; i++)
    {
        const T *offsetInput = reinterpret_cast<const T *>(input + (i * stride));
        T offsetInputAligned[inputComponentCount];
        ASSERT(sizeof(offsetInputAligned) == attribSize);
        offsetInput =
            GetAlignedOffsetInput<T, inputComponentCount>(offsetInput, &offsetInputAligned[0]);

        T *offsetOutput = reinterpret_cast<T *>(output) + i * outputComponentCount;

        memcpy(offsetOutput, offsetInput, attribSize);

        if (inputComponentCount < lastNonAlphaOutputComponent)
        {
            // Set the remaining G/B channels to 0.
            size_t numComponents = (lastNonAlphaOutputComponent - inputComponentCount);
            memset(&offsetOutput[inputComponentCount], 0, numComponents * sizeof(T));
        }

        if (inputComponentCount < outputComponentCount && outputComponentCount == 4)
        {
            // Set the remaining alpha channel to the defaultAlphaValue.
            offsetOutput[3] = defaultAlphaValue;
        }
    }
}

template <size_t inputComponentCount, size_t outputComponentCount>
inline void Copy8SintTo16SintVertexData(const uint8_t *input,
                                        size_t stride,
                                        size_t count,
                                        uint8_t *output)
{
    const size_t lastNonAlphaOutputComponent = std::min<size_t>(outputComponentCount, 3);

    for (size_t i = 0; i < count; i++)
    {
        const GLbyte *offsetInput = reinterpret_cast<const GLbyte *>(input + i * stride);
        GLshort *offsetOutput     = reinterpret_cast<GLshort *>(output) + i * outputComponentCount;

        for (size_t j = 0; j < inputComponentCount; j++)
        {
            offsetOutput[j] = static_cast<GLshort>(offsetInput[j]);
        }

        for (size_t j = inputComponentCount; j < lastNonAlphaOutputComponent; j++)
        {
            // Set remaining G/B channels to 0.
            offsetOutput[j] = 0;
        }

        if (inputComponentCount < outputComponentCount && outputComponentCount == 4)
        {
            // On integer formats, we must set the Alpha channel to 1 if it's unused.
            offsetOutput[3] = 1;
        }
    }
}

template <size_t inputComponentCount, size_t outputComponentCount>
inline void Copy8SnormTo16SnormVertexData(const uint8_t *input,
                                          size_t stride,
                                          size_t count,
                                          uint8_t *output)
{
    for (size_t i = 0; i < count; i++)
    {
        const GLbyte *offsetInput = reinterpret_cast<const GLbyte *>(input + i * stride);
        GLshort *offsetOutput     = reinterpret_cast<GLshort *>(output) + i * outputComponentCount;

        for (size_t j = 0; j < inputComponentCount; j++)
        {
            // The original GLbyte value ranges from -128 to +127 (INT8_MAX).
            // When converted to GLshort, the value must be scaled to between -32768 and +32767
            // (INT16_MAX).
            if (offsetInput[j] > 0)
            {
                offsetOutput[j] =
                    offsetInput[j] << 8 | offsetInput[j] << 1 | ((offsetInput[j] & 0x40) >> 6);
            }
            else
            {
                offsetOutput[j] = offsetInput[j] << 8;
            }
        }

        for (size_t j = inputComponentCount; j < std::min<size_t>(outputComponentCount, 3); j++)
        {
            // Set remaining G/B channels to 0.
            offsetOutput[j] = 0;
        }

        if (inputComponentCount < outputComponentCount && outputComponentCount == 4)
        {
            // On normalized formats, we must set the Alpha channel to the max value if it's unused.
            offsetOutput[3] = INT16_MAX;
        }
    }
}

template <size_t inputComponentCount, size_t outputComponentCount>
inline void Copy32FixedTo32FVertexData(const uint8_t *input,
                                       size_t stride,
                                       size_t count,
                                       uint8_t *output)
{
    static const float divisor = 1.0f / (1 << 16);

    for (size_t i = 0; i < count; i++)
    {
        const uint8_t *offsetInput = input + i * stride;
        float *offsetOutput        = reinterpret_cast<float *>(output) + i * outputComponentCount;

        // GLfixed access must be 4-byte aligned on arm32, input and stride sometimes are not
        if (reinterpret_cast<uintptr_t>(offsetInput) % sizeof(GLfixed) == 0)
        {
            for (size_t j = 0; j < inputComponentCount; j++)
            {
                offsetOutput[j] =
                    static_cast<float>(reinterpret_cast<const GLfixed *>(offsetInput)[j]) * divisor;
            }
        }
        else
        {
            for (size_t j = 0; j < inputComponentCount; j++)
            {
                GLfixed alignedInput;
                memcpy(&alignedInput, offsetInput + j * sizeof(GLfixed), sizeof(GLfixed));
                offsetOutput[j] = static_cast<float>(alignedInput) * divisor;
            }
        }

        // 4-component output formats would need special padding in the alpha channel.
        static_assert(!(inputComponentCount < 4 && outputComponentCount == 4),
                      "An inputComponentCount less than 4 and an outputComponentCount equal to 4 "
                      "is not supported.");

        for (size_t j = inputComponentCount; j < outputComponentCount; j++)
        {
            offsetOutput[j] = 0.0f;
        }
    }
}

template <typename T,
          size_t inputComponentCount,
          size_t outputComponentCount,
          bool normalized,
          bool toHalf>
inline void CopyToFloatVertexData(const uint8_t *input,
                                  size_t stride,
                                  size_t count,
                                  uint8_t *output)
{
    typedef std::numeric_limits<T> NL;
    typedef typename std::conditional<toHalf, GLhalf, float>::type outputType;

    for (size_t i = 0; i < count; i++)
    {
        const T *offsetInput = reinterpret_cast<const T *>(input + (stride * i));
        outputType *offsetOutput =
            reinterpret_cast<outputType *>(output) + i * outputComponentCount;

        T offsetInputAligned[inputComponentCount];
        offsetInput =
            GetAlignedOffsetInput<T, inputComponentCount>(offsetInput, &offsetInputAligned[0]);

        for (size_t j = 0; j < inputComponentCount; j++)
        {
            float result = 0;

            if (normalized)
            {
                if (NL::is_signed)
                {
                    result = static_cast<float>(offsetInput[j]) / static_cast<float>(NL::max());
                    result = result >= -1.0f ? result : -1.0f;
                }
                else
                {
                    result = static_cast<float>(offsetInput[j]) / static_cast<float>(NL::max());
                }
            }
            else
            {
                result = static_cast<float>(offsetInput[j]);
            }

            if (toHalf)
            {
                offsetOutput[j] = gl::float32ToFloat16(result);
            }
            else
            {
                offsetOutput[j] = static_cast<outputType>(result);
            }
        }

        for (size_t j = inputComponentCount; j < outputComponentCount; j++)
        {
            offsetOutput[j] = 0;
        }

        if (inputComponentCount < 4 && outputComponentCount == 4)
        {
            if (toHalf)
            {
                offsetOutput[3] = gl::Float16One;
            }
            else
            {
                offsetOutput[3] = static_cast<outputType>(gl::Float32One);
            }
        }
    }
}

template <size_t inputComponentCount, size_t outputComponentCount>
void Copy32FTo16FVertexData(const uint8_t *input, size_t stride, size_t count, uint8_t *output)
{
    const unsigned short kZero = gl::float32ToFloat16(0.0f);
    const unsigned short kOne  = gl::float32ToFloat16(1.0f);

    for (size_t i = 0; i < count; i++)
    {
        const float *offsetInput = reinterpret_cast<const float *>(input + (stride * i));
        unsigned short *offsetOutput =
            reinterpret_cast<unsigned short *>(output) + i * outputComponentCount;

        for (size_t j = 0; j < inputComponentCount; j++)
        {
            offsetOutput[j] = gl::float32ToFloat16(offsetInput[j]);
        }

        for (size_t j = inputComponentCount; j < outputComponentCount; j++)
        {
            offsetOutput[j] = (j == 3) ? kOne : kZero;
        }
    }
}

inline void CopyXYZ32FToXYZ9E5(const uint8_t *input, size_t stride, size_t count, uint8_t *output)
{
    for (size_t i = 0; i < count; i++)
    {
        const float *offsetInput   = reinterpret_cast<const float *>(input + (stride * i));
        unsigned int *offsetOutput = reinterpret_cast<unsigned int *>(output) + i;

        *offsetOutput = gl::convertRGBFloatsTo999E5(offsetInput[0], offsetInput[1], offsetInput[2]);
    }
}

inline void CopyXYZ32FToX11Y11B10F(const uint8_t *input,
                                   size_t stride,
                                   size_t count,
                                   uint8_t *output)
{
    for (size_t i = 0; i < count; i++)
    {
        const float *offsetInput   = reinterpret_cast<const float *>(input + (stride * i));
        unsigned int *offsetOutput = reinterpret_cast<unsigned int *>(output) + i;

        *offsetOutput = gl::float32ToFloat11(offsetInput[0]) << 0 |
                        gl::float32ToFloat11(offsetInput[1]) << 11 |
                        gl::float32ToFloat10(offsetInput[2]) << 22;
    }
}

namespace priv
{

template <bool isSigned, bool normalized, bool toFloat, bool toHalf>
static inline void CopyPackedRGB(uint32_t data, uint8_t *output)
{
    const uint32_t rgbSignMask  = 0x200;       // 1 set at the 9 bit
    const uint32_t negativeMask = 0xFFFFFC00;  // All bits from 10 to 31 set to 1

    if (toFloat || toHalf)
    {
        GLfloat finalValue = static_cast<GLfloat>(data);
        if (isSigned)
        {
            if (data & rgbSignMask)
            {
                int negativeNumber = data | negativeMask;
                finalValue         = static_cast<GLfloat>(negativeNumber);
            }

            if (normalized)
            {
                const int32_t maxValue = 0x1FF;       // 1 set in bits 0 through 8
                const int32_t minValue = 0xFFFFFE01;  // Inverse of maxValue

                // A 10-bit two's complement number has the possibility of being minValue - 1 but
                // OpenGL's normalization rules dictate that it should be clamped to minValue in
                // this case.
                if (finalValue < minValue)
                {
                    finalValue = minValue;
                }

                const int32_t halfRange = (maxValue - minValue) >> 1;
                finalValue              = ((finalValue - minValue) / halfRange) - 1.0f;
            }
        }
        else
        {
            if (normalized)
            {
                const uint32_t maxValue = 0x3FF;  // 1 set in bits 0 through 9
                finalValue /= static_cast<GLfloat>(maxValue);
            }
        }

        if (toHalf)
        {
            *reinterpret_cast<GLhalf *>(output) = gl::float32ToFloat16(finalValue);
        }
        else
        {
            *reinterpret_cast<GLfloat *>(output) = finalValue;
        }
    }
    else
    {
        if (isSigned)
        {
            GLshort *intOutput = reinterpret_cast<GLshort *>(output);

            if (data & rgbSignMask)
            {
                *intOutput = static_cast<GLshort>(data | negativeMask);
            }
            else
            {
                *intOutput = static_cast<GLshort>(data);
            }
        }
        else
        {
            GLushort *uintOutput = reinterpret_cast<GLushort *>(output);
            *uintOutput          = static_cast<GLushort>(data);
        }
    }
}

template <bool isSigned, bool normalized, bool toFloat, bool toHalf>
inline void CopyPackedAlpha(uint32_t data, uint8_t *output)
{
    ASSERT(data >= 0 && data <= 3);

    if (toFloat || toHalf)
    {
        GLfloat finalValue = 0;
        if (isSigned)
        {
            if (normalized)
            {
                switch (data)
                {
                    case 0x0:
                        finalValue = 0.0f;
                        break;
                    case 0x1:
                        finalValue = 1.0f;
                        break;
                    case 0x2:
                        finalValue = -1.0f;
                        break;
                    case 0x3:
                        finalValue = -1.0f;
                        break;
                    default:
                        UNREACHABLE();
                }
            }
            else
            {
                switch (data)
                {
                    case 0x0:
                        finalValue = 0.0f;
                        break;
                    case 0x1:
                        finalValue = 1.0f;
                        break;
                    case 0x2:
                        finalValue = -2.0f;
                        break;
                    case 0x3:
                        finalValue = -1.0f;
                        break;
                    default:
                        UNREACHABLE();
                }
            }
        }
        else
        {
            if (normalized)
            {
                finalValue = data / 3.0f;
            }
            else
            {
                finalValue = static_cast<float>(data);
            }
        }

        if (toHalf)
        {
            *reinterpret_cast<GLhalf *>(output) = gl::float32ToFloat16(finalValue);
        }
        else
        {
            *reinterpret_cast<GLfloat *>(output) = finalValue;
        }
    }
    else
    {
        if (isSigned)
        {
            GLshort *intOutput = reinterpret_cast<GLshort *>(output);
            switch (data)
            {
                case 0x0:
                    *intOutput = 0;
                    break;
                case 0x1:
                    *intOutput = 1;
                    break;
                case 0x2:
                    *intOutput = -2;
                    break;
                case 0x3:
                    *intOutput = -1;
                    break;
                default:
                    UNREACHABLE();
            }
        }
        else
        {
            *reinterpret_cast<GLushort *>(output) = static_cast<GLushort>(data);
        }
    }
}

}  // namespace priv

template <bool isSigned, bool normalized, bool toFloat, bool toHalf>
inline void CopyXYZ10W2ToXYZWFloatVertexData(const uint8_t *input,
                                             size_t stride,
                                             size_t count,
                                             uint8_t *output)
{
    const size_t outputComponentSize = toFloat && !toHalf ? 4 : 2;
    const size_t componentCount      = 4;

    const uint32_t rgbMask  = 0x3FF;  // 1 set in bits 0 through 9
    const size_t redShift   = 0;      // red is bits 0 through 9
    const size_t greenShift = 10;     // green is bits 10 through 19
    const size_t blueShift  = 20;     // blue is bits 20 through 29

    const uint32_t alphaMask = 0x3;  // 1 set in bits 0 and 1
    const size_t alphaShift  = 30;   // Alpha is the 30 and 31 bits

    for (size_t i = 0; i < count; i++)
    {
        GLuint packedValue    = *reinterpret_cast<const GLuint *>(input + (i * stride));
        uint8_t *offsetOutput = output + (i * outputComponentSize * componentCount);

        priv::CopyPackedRGB<isSigned, normalized, toFloat, toHalf>(
            (packedValue >> redShift) & rgbMask, offsetOutput + (0 * outputComponentSize));
        priv::CopyPackedRGB<isSigned, normalized, toFloat, toHalf>(
            (packedValue >> greenShift) & rgbMask, offsetOutput + (1 * outputComponentSize));
        priv::CopyPackedRGB<isSigned, normalized, toFloat, toHalf>(
            (packedValue >> blueShift) & rgbMask, offsetOutput + (2 * outputComponentSize));
        priv::CopyPackedAlpha<isSigned, normalized, toFloat, toHalf>(
            (packedValue >> alphaShift) & alphaMask, offsetOutput + (3 * outputComponentSize));
    }
}

template <bool isSigned, bool normalized, bool toHalf>
inline void CopyXYZ10ToXYZWFloatVertexData(const uint8_t *input,
                                           size_t stride,
                                           size_t count,
                                           uint8_t *output)
{
    const size_t outputComponentSize = toHalf ? 2 : 4;
    const size_t componentCount      = 4;

    const uint32_t rgbMask  = 0x3FF;  // 1 set in bits 0 through 9
    const size_t redShift   = 22;     // red is bits 22 through 31
    const size_t greenShift = 12;     // green is bits 12 through 21
    const size_t blueShift  = 2;      // blue is bits 2 through 11

    const uint32_t alphaDefaultValueBits = normalized ? (isSigned ? 0x1 : 0x3) : 0x1;

    for (size_t i = 0; i < count; i++)
    {
        GLuint packedValue    = *reinterpret_cast<const GLuint *>(input + (i * stride));
        uint8_t *offsetOutput = output + (i * outputComponentSize * componentCount);

        priv::CopyPackedRGB<isSigned, normalized, true, toHalf>(
            (packedValue >> redShift) & rgbMask, offsetOutput + (0 * outputComponentSize));
        priv::CopyPackedRGB<isSigned, normalized, true, toHalf>(
            (packedValue >> greenShift) & rgbMask, offsetOutput + (1 * outputComponentSize));
        priv::CopyPackedRGB<isSigned, normalized, true, toHalf>(
            (packedValue >> blueShift) & rgbMask, offsetOutput + (2 * outputComponentSize));
        priv::CopyPackedAlpha<isSigned, normalized, true, toHalf>(
            alphaDefaultValueBits, offsetOutput + (3 * outputComponentSize));
    }
}

template <bool isSigned, bool normalized, bool toHalf>
inline void CopyW2XYZ10ToXYZWFloatVertexData(const uint8_t *input,
                                             size_t stride,
                                             size_t count,
                                             uint8_t *output)
{
    const size_t outputComponentSize = toHalf ? 2 : 4;
    const size_t componentCount      = 4;

    const uint32_t rgbMask  = 0x3FF;  // 1 set in bits 0 through 9
    const size_t redShift   = 22;     // red is bits 22 through 31
    const size_t greenShift = 12;     // green is bits 12 through 21
    const size_t blueShift  = 2;      // blue is bits 2 through 11

    const uint32_t alphaMask = 0x3;  // 1 set in bits 0 and 1
    const size_t alphaShift  = 0;    // Alpha is the 30 and 31 bits

    for (size_t i = 0; i < count; i++)
    {
        GLuint packedValue    = *reinterpret_cast<const GLuint *>(input + (i * stride));
        uint8_t *offsetOutput = output + (i * outputComponentSize * componentCount);

        priv::CopyPackedRGB<isSigned, normalized, true, toHalf>(
            (packedValue >> redShift) & rgbMask, offsetOutput + (0 * outputComponentSize));
        priv::CopyPackedRGB<isSigned, normalized, true, toHalf>(
            (packedValue >> greenShift) & rgbMask, offsetOutput + (1 * outputComponentSize));
        priv::CopyPackedRGB<isSigned, normalized, true, toHalf>(
            (packedValue >> blueShift) & rgbMask, offsetOutput + (2 * outputComponentSize));
        priv::CopyPackedAlpha<isSigned, normalized, true, toHalf>(
            (packedValue >> alphaShift) & alphaMask, offsetOutput + (3 * outputComponentSize));
    }
}
}  // namespace rx
