//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "common/mathutil.h"

#include <string.h>

namespace angle
{

namespace priv
{

template <typename T>
inline T *OffsetDataPointer(uint8_t *data, size_t y, size_t z, size_t rowPitch, size_t depthPitch)
{
    return reinterpret_cast<T*>(data + (y * rowPitch) + (z * depthPitch));
}

template <typename T>
inline const T *OffsetDataPointer(const uint8_t *data, size_t y, size_t z, size_t rowPitch, size_t depthPitch)
{
    return reinterpret_cast<const T*>(data + (y * rowPitch) + (z * depthPitch));
}

}  // namespace priv

template <typename type, size_t componentCount>
inline void LoadToNative(const ImageLoadContext &context, size_t width, size_t height, size_t depth,
                         const uint8_t *input, size_t inputRowPitch, size_t inputDepthPitch,
                         uint8_t *output, size_t outputRowPitch, size_t outputDepthPitch)
{
    const size_t rowSize = width * sizeof(type) * componentCount;
    const size_t layerSize = rowSize * height;
    const size_t imageSize = layerSize * depth;

    if (layerSize == inputDepthPitch && layerSize == outputDepthPitch)
    {
        ASSERT(rowSize == inputRowPitch && rowSize == outputRowPitch);
        memcpy(output, input, imageSize);
    }
    else if (rowSize == inputRowPitch && rowSize == outputRowPitch)
    {
        for (size_t z = 0; z < depth; z++)
        {
            const type *source = priv::OffsetDataPointer<type>(input, 0, z, inputRowPitch, inputDepthPitch);
            type *dest = priv::OffsetDataPointer<type>(output, 0, z, outputRowPitch, outputDepthPitch);

            memcpy(dest, source, layerSize);
        }
    }
    else
    {
        for (size_t z = 0; z < depth; z++)
        {
            for (size_t y = 0; y < height; y++)
            {
                const type *source = priv::OffsetDataPointer<type>(input, y, z, inputRowPitch, inputDepthPitch);
                type *dest = priv::OffsetDataPointer<type>(output, y, z, outputRowPitch, outputDepthPitch);
                memcpy(dest, source, width * sizeof(type) * componentCount);
            }
        }
    }
}

template <typename type, uint32_t fourthComponentBits>
inline void LoadToNative3To4(const ImageLoadContext &context, size_t width, size_t height,
                             size_t depth, const uint8_t *input, size_t inputRowPitch,
                             size_t inputDepthPitch, uint8_t *output, size_t outputRowPitch,
                             size_t outputDepthPitch)
{
    const type fourthValue = gl::bitCast<type>(fourthComponentBits);

    for (size_t z = 0; z < depth; z++)
    {
        for (size_t y = 0; y < height; y++)
        {
            const type *source = priv::OffsetDataPointer<type>(input, y, z, inputRowPitch, inputDepthPitch);
            type *dest = priv::OffsetDataPointer<type>(output, y, z, outputRowPitch, outputDepthPitch);
            for (size_t x = 0; x < width; x++)
            {
                memcpy(&dest[x * 4], &source[x * 3], sizeof(type) * 3);
                dest[x * 4 + 3] = fourthValue;
            }
        }
    }
}

template <size_t componentCount>
inline void Load32FTo16F(const ImageLoadContext &context, size_t width, size_t height, size_t depth,
                         const uint8_t *input, size_t inputRowPitch, size_t inputDepthPitch,
                         uint8_t *output, size_t outputRowPitch, size_t outputDepthPitch)
{
    const size_t elementWidth = componentCount * width;

    for (size_t z = 0; z < depth; z++)
    {
        for (size_t y = 0; y < height; y++)
        {
            const float *source = priv::OffsetDataPointer<float>(input, y, z, inputRowPitch, inputDepthPitch);
            uint16_t *dest = priv::OffsetDataPointer<uint16_t>(output, y, z, outputRowPitch, outputDepthPitch);

            for (size_t x = 0; x < elementWidth; x++)
            {
                dest[x] = gl::float32ToFloat16(source[x]);
            }
        }
    }
}

template <size_t blockWidth, size_t blockHeight, size_t blockDepth, size_t blockSize>
inline void LoadCompressedToNative(const ImageLoadContext &context, size_t width, size_t height,
                                   size_t depth, const uint8_t *input, size_t inputRowPitch,
                                   size_t inputDepthPitch, uint8_t *output, size_t outputRowPitch,
                                   size_t outputDepthPitch)
{
    const size_t columns = (width + (blockWidth - 1)) / blockWidth;
    const size_t rows = (height + (blockHeight - 1)) / blockHeight;
    const size_t layers = (depth + (blockDepth - 1)) / blockDepth;

    for (size_t z = 0; z < layers; ++z)
    {
        for (size_t y = 0; y < rows; ++y)
        {
            const uint8_t *source = priv::OffsetDataPointer<uint8_t>(input, y, z, inputRowPitch, inputDepthPitch);
            uint8_t *dest = priv::OffsetDataPointer<uint8_t>(output, y, z, outputRowPitch, outputDepthPitch);
            memcpy(dest, source, columns * blockSize);
        }
    }
}

template <typename type, uint32_t firstBits, uint32_t secondBits, uint32_t thirdBits, uint32_t fourthBits>
inline void Initialize4ComponentData(size_t width, size_t height, size_t depth,
                                     uint8_t *output, size_t outputRowPitch, size_t outputDepthPitch)
{
    type writeValues[4] =
    {
        gl::bitCast<type>(firstBits),
        gl::bitCast<type>(secondBits),
        gl::bitCast<type>(thirdBits),
        gl::bitCast<type>(fourthBits),
    };

    for (size_t z = 0; z < depth; z++)
    {
        for (size_t y = 0; y < height; y++)
        {
            type *destRow = priv::OffsetDataPointer<type>(output, y, z, outputRowPitch, outputDepthPitch);
            for (size_t x = 0; x < width; x++)
            {
                type* destPixel = destRow + x * 4;

                // This could potentially be optimized by generating an entire row of initialization
                // data and copying row by row instead of pixel by pixel.
                memcpy(destPixel, writeValues, sizeof(type) * 4);
            }
        }
    }
}

template <size_t blockWidth, size_t blockHeight>
inline void LoadASTCToRGBA8(const ImageLoadContext &context,
                            size_t width,
                            size_t height,
                            size_t depth,
                            const uint8_t *input,
                            size_t inputRowPitch,
                            size_t inputDepthPitch,
                            uint8_t *output,
                            size_t outputRowPitch,
                            size_t outputDepthPitch)
{
    LoadASTCToRGBA8Inner(context, width, height, depth, blockWidth, blockHeight, input, inputRowPitch,
                         inputDepthPitch, output, outputRowPitch, outputDepthPitch);
}

template <uint32_t indexBits, uint32_t redBlueBits, uint32_t greenBits, uint32_t alphaBits>
inline void LoadPalettedToRGBA8(const ImageLoadContext &context,
                                size_t width,
                                size_t height,
                                size_t depth,
                                const uint8_t *input,
                                size_t inputRowPitch,
                                size_t inputDepthPitch,
                                uint8_t *output,
                                size_t outputRowPitch,
                                size_t outputDepthPitch)
{
    static_assert(indexBits == 4 || indexBits == 8);
    static_assert(redBlueBits == 4 || redBlueBits == 5 || redBlueBits == 8);
    static_assert(greenBits == 4 || greenBits == 5 || greenBits == 6 || greenBits == 8);
    static_assert(alphaBits == 0 || alphaBits == 1 || alphaBits == 4 || alphaBits == 8);
    constexpr uint32_t colorBits = 2 * redBlueBits + greenBits + alphaBits;
    static_assert(colorBits == 16 || colorBits == 24 || colorBits == 32);

    LoadPalettedToRGBA8Impl(context, width, height, depth,
                            indexBits, redBlueBits, greenBits, alphaBits,
                            input, inputRowPitch, inputDepthPitch,
                            output, outputRowPitch, outputDepthPitch);
}

// Temporary overload functions; need to have no-context overloads of the following functions used
// by Chromium.  A Chromium change will switch to the with-context overloads, and then these can be
// removed.
inline void LoadEACR11ToR8(size_t width,
                           size_t height,
                           size_t depth,
                           const uint8_t *input,
                           size_t inputRowPitch,
                           size_t inputDepthPitch,
                           uint8_t *output,
                           size_t outputRowPitch,
                           size_t outputDepthPitch)
{
    LoadEACR11ToR8({}, width, height, depth, input, inputRowPitch, inputDepthPitch, output,
                   outputRowPitch, outputDepthPitch);
}

inline void LoadEACR11SToR8(size_t width,
                            size_t height,
                            size_t depth,
                            const uint8_t *input,
                            size_t inputRowPitch,
                            size_t inputDepthPitch,
                            uint8_t *output,
                            size_t outputRowPitch,
                            size_t outputDepthPitch)
{
    LoadEACR11SToR8({}, width, height, depth, input, inputRowPitch, inputDepthPitch, output,
                    outputRowPitch, outputDepthPitch);
}

inline void LoadEACRG11ToRG8(size_t width,
                             size_t height,
                             size_t depth,
                             const uint8_t *input,
                             size_t inputRowPitch,
                             size_t inputDepthPitch,
                             uint8_t *output,
                             size_t outputRowPitch,
                             size_t outputDepthPitch)
{
    LoadEACRG11ToRG8({}, width, height, depth, input, inputRowPitch, inputDepthPitch, output,
                     outputRowPitch, outputDepthPitch);
}

inline void LoadEACRG11SToRG8(size_t width,
                              size_t height,
                              size_t depth,
                              const uint8_t *input,
                              size_t inputRowPitch,
                              size_t inputDepthPitch,
                              uint8_t *output,
                              size_t outputRowPitch,
                              size_t outputDepthPitch)
{
    LoadEACRG11SToRG8({}, width, height, depth, input, inputRowPitch, inputDepthPitch, output,
                      outputRowPitch, outputDepthPitch);
}

inline void LoadETC2RGB8ToRGBA8(size_t width,
                                size_t height,
                                size_t depth,
                                const uint8_t *input,
                                size_t inputRowPitch,
                                size_t inputDepthPitch,
                                uint8_t *output,
                                size_t outputRowPitch,
                                size_t outputDepthPitch)
{
    LoadETC2RGB8ToRGBA8({}, width, height, depth, input, inputRowPitch, inputDepthPitch, output,
                        outputRowPitch, outputDepthPitch);
}

inline void LoadETC2SRGB8ToRGBA8(size_t width,
                                 size_t height,
                                 size_t depth,
                                 const uint8_t *input,
                                 size_t inputRowPitch,
                                 size_t inputDepthPitch,
                                 uint8_t *output,
                                 size_t outputRowPitch,
                                 size_t outputDepthPitch)
{
    LoadETC2SRGB8ToRGBA8({}, width, height, depth, input, inputRowPitch, inputDepthPitch, output,
                         outputRowPitch, outputDepthPitch);
}

inline void LoadETC2RGBA8ToRGBA8(size_t width,
                                 size_t height,
                                 size_t depth,
                                 const uint8_t *input,
                                 size_t inputRowPitch,
                                 size_t inputDepthPitch,
                                 uint8_t *output,
                                 size_t outputRowPitch,
                                 size_t outputDepthPitch)
{
    LoadETC2RGBA8ToRGBA8({}, width, height, depth, input, inputRowPitch, inputDepthPitch, output,
                         outputRowPitch, outputDepthPitch);
}

inline void LoadETC2RGB8A1ToRGBA8(size_t width,
                                  size_t height,
                                  size_t depth,
                                  const uint8_t *input,
                                  size_t inputRowPitch,
                                  size_t inputDepthPitch,
                                  uint8_t *output,
                                  size_t outputRowPitch,
                                  size_t outputDepthPitch)
{
    LoadETC2RGB8A1ToRGBA8({}, width, height, depth, input, inputRowPitch, inputDepthPitch, output,
                          outputRowPitch, outputDepthPitch);
}

inline void LoadETC2SRGBA8ToSRGBA8(size_t width,
                                   size_t height,
                                   size_t depth,
                                   const uint8_t *input,
                                   size_t inputRowPitch,
                                   size_t inputDepthPitch,
                                   uint8_t *output,
                                   size_t outputRowPitch,
                                   size_t outputDepthPitch)
{
    LoadETC2SRGBA8ToSRGBA8({}, width, height, depth, input, inputRowPitch, inputDepthPitch, output,
                           outputRowPitch, outputDepthPitch);
}

inline void LoadETC2SRGB8A1ToRGBA8(size_t width,
                                   size_t height,
                                   size_t depth,
                                   const uint8_t *input,
                                   size_t inputRowPitch,
                                   size_t inputDepthPitch,
                                   uint8_t *output,
                                   size_t outputRowPitch,
                                   size_t outputDepthPitch)
{
    LoadETC2SRGB8A1ToRGBA8({}, width, height, depth, input, inputRowPitch, inputDepthPitch, output,
                           outputRowPitch, outputDepthPitch);
}

} // namespace angle
