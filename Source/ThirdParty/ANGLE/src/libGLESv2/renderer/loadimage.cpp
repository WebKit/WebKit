#include "precompiled.h"
//
// Copyright (c) 0013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// loadimage.cpp: Defines image loading functions.

#include "libGLESv2/renderer/loadimage.h"

namespace rx
{

void loadAlphaDataToBGRA(int width, int height, int depth,
                         const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                         void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned char>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned char>(output, y, z, outputRowPitch, outputDepthPitch);
            for (int x = 0; x < width; x++)
            {
                dest[4 * x + 0] = 0;
                dest[4 * x + 1] = 0;
                dest[4 * x + 2] = 0;
                dest[4 * x + 3] = source[x];
            }
        }
    }
}

void loadAlphaDataToNative(int width, int height, int depth,
                           const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                           void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned char>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned char>(output, y, z, outputRowPitch, outputDepthPitch);
            memcpy(dest, source, width);
        }
    }
}

void loadAlphaFloatDataToRGBA(int width, int height, int depth,
                              const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                              void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const float *source = NULL;
    float *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<float>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<float>(output, y, z, outputRowPitch, outputDepthPitch);
            for (int x = 0; x < width; x++)
            {
                dest[4 * x + 0] = 0;
                dest[4 * x + 1] = 0;
                dest[4 * x + 2] = 0;
                dest[4 * x + 3] = source[x];
            }
        }
    }
}

void loadAlphaHalfFloatDataToRGBA(int width, int height, int depth,
                                  const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                                  void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned short *source = NULL;
    unsigned short *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned short>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned short>(output, y, z, outputRowPitch, outputDepthPitch);
            for (int x = 0; x < width; x++)
            {
                dest[4 * x + 0] = 0;
                dest[4 * x + 1] = 0;
                dest[4 * x + 2] = 0;
                dest[4 * x + 3] = source[x];
            }
        }
    }
}

void loadLuminanceDataToNative(int width, int height, int depth,
                               const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                               void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned char>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned char>(output, y, z, outputRowPitch, outputDepthPitch);
            memcpy(dest, source, width);
        }
    }
}

void loadLuminanceDataToBGRA(int width, int height, int depth,
                             const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                             void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned char>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned char>(output, y, z, outputRowPitch, outputDepthPitch);
            for (int x = 0; x < width; x++)
            {
                dest[4 * x + 0] = source[x];
                dest[4 * x + 1] = source[x];
                dest[4 * x + 2] = source[x];
                dest[4 * x + 3] = 0xFF;
            }
        }
    }
}

void loadLuminanceFloatDataToRGBA(int width, int height, int depth,
                                  const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                                  void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const float *source = NULL;
    float *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<float>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<float>(output, y, z, outputRowPitch, outputDepthPitch);
            for (int x = 0; x < width; x++)
            {
                dest[4 * x + 0] = source[x];
                dest[4 * x + 1] = source[x];
                dest[4 * x + 2] = source[x];
                dest[4 * x + 3] = 1.0f;
            }
        }
    }
}

void loadLuminanceFloatDataToRGB(int width, int height, int depth,
                                 const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                                 void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const float *source = NULL;
    float *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<float>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<float>(output, y, z, outputRowPitch, outputDepthPitch);
            for (int x = 0; x < width; x++)
            {
                dest[3 * x + 0] = source[x];
                dest[3 * x + 1] = source[x];
                dest[3 * x + 2] = source[x];
            }
        }
    }
}

void loadLuminanceHalfFloatDataToRGBA(int width, int height, int depth,
                                      const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                                      void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned short *source = NULL;
    unsigned short *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned short>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned short>(output, y, z, outputRowPitch, outputDepthPitch);
            for (int x = 0; x < width; x++)
            {
                dest[4 * x + 0] = source[x];
                dest[4 * x + 1] = source[x];
                dest[4 * x + 2] = source[x];
                dest[4 * x + 3] = 0x3C00; // SEEEEEMMMMMMMMMM, S = 0, E = 15, M = 0: 16bit flpt representation of 1
            }
        }
    }
}

void loadLuminanceAlphaDataToNative(int width, int height, int depth,
                                    const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                                    void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned char>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned char>(output, y, z, outputRowPitch, outputDepthPitch);

            memcpy(dest, source, width * 2);
        }
    }
}

void loadLuminanceAlphaDataToBGRA(int width, int height, int depth,
                                  const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                                  void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned char>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned char>(output, y, z, outputRowPitch, outputDepthPitch);

            for (int x = 0; x < width; x++)
            {
                dest[4 * x + 0] = source[2*x+0];
                dest[4 * x + 1] = source[2*x+0];
                dest[4 * x + 2] = source[2*x+0];
                dest[4 * x + 3] = source[2*x+1];
            }
        }
    }
}

void loadLuminanceAlphaFloatDataToRGBA(int width, int height, int depth,
                                       const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                                       void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const float *source = NULL;
    float *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<float>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<float>(output, y, z, outputRowPitch, outputDepthPitch);
            for (int x = 0; x < width; x++)
            {
                dest[4 * x + 0] = source[2*x+0];
                dest[4 * x + 1] = source[2*x+0];
                dest[4 * x + 2] = source[2*x+0];
                dest[4 * x + 3] = source[2*x+1];
            }
        }
    }
}

void loadLuminanceAlphaHalfFloatDataToRGBA(int width, int height, int depth,
                                           const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                                           void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned short *source = NULL;
    unsigned short *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned short>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned short>(output, y, z, outputRowPitch, outputDepthPitch);
            for (int x = 0; x < width; x++)
            {
                dest[4 * x + 0] = source[2*x+0];
                dest[4 * x + 1] = source[2*x+0];
                dest[4 * x + 2] = source[2*x+0];
                dest[4 * x + 3] = source[2*x+1];
            }
        }
    }
}

void loadRGBUByteDataToBGRX(int width, int height, int depth,
                            const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                            void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned char>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned char>(output, y, z, outputRowPitch, outputDepthPitch);
            for (int x = 0; x < width; x++)
            {
                dest[4 * x + 0] = source[x * 3 + 2];
                dest[4 * x + 1] = source[x * 3 + 1];
                dest[4 * x + 2] = source[x * 3 + 0];
                dest[4 * x + 3] = 0xFF;
            }
        }
    }
}

void loadRGUByteDataToBGRX(int width, int height, int depth,
                           const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                           void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned char>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned char>(output, y, z, outputRowPitch, outputDepthPitch);
            for (int x = 0; x < width; x++)
            {
                dest[4 * x + 0] = 0x00;
                dest[4 * x + 1] = source[x * 2 + 1];
                dest[4 * x + 2] = source[x * 2 + 0];
                dest[4 * x + 3] = 0xFF;
            }
        }
    }
}

void loadRUByteDataToBGRX(int width, int height, int depth,
                          const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                          void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned char>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned char>(output, y, z, outputRowPitch, outputDepthPitch);
            for (int x = 0; x < width; x++)
            {
                dest[4 * x + 0] = 0x00;
                dest[4 * x + 1] = 0x00;
                dest[4 * x + 2] = source[x];
                dest[4 * x + 3] = 0xFF;
            }
        }
    }
}

void loadRGBUByteDataToRGBA(int width, int height, int depth,
                            const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                            void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned char>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned char>(output, y, z, outputRowPitch, outputDepthPitch);
            for (int x = 0; x < width; x++)
            {
                dest[4 * x + 0] = source[x * 3 + 0];
                dest[4 * x + 1] = source[x * 3 + 1];
                dest[4 * x + 2] = source[x * 3 + 2];
                dest[4 * x + 3] = 0xFF;
            }
        }
    }
}

void loadRGBSByteDataToRGBA(int width, int height, int depth,
                            const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                            void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const char *source = NULL;
    char *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<char>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<char>(output, y, z, outputRowPitch, outputDepthPitch);
            for (int x = 0; x < width; x++)
            {
                dest[4 * x + 0] = source[x * 3 + 0];
                dest[4 * x + 1] = source[x * 3 + 1];
                dest[4 * x + 2] = source[x * 3 + 2];
                dest[4 * x + 3] = 0x7F;
            }
        }
    }
}

void loadRGB565DataToBGRA(int width, int height, int depth,
                          const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                          void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned short *source = NULL;
    unsigned char *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned short>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned char>(output, y, z, outputRowPitch, outputDepthPitch);
            for (int x = 0; x < width; x++)
            {
                unsigned short rgba = source[x];
                dest[4 * x + 0] = ((rgba & 0x001F) << 3) | ((rgba & 0x001F) >> 2);
                dest[4 * x + 1] = ((rgba & 0x07E0) >> 3) | ((rgba & 0x07E0) >> 9);
                dest[4 * x + 2] = ((rgba & 0xF800) >> 8) | ((rgba & 0xF800) >> 13);
                dest[4 * x + 3] = 0xFF;
            }
        }
    }
}

void loadRGB565DataToRGBA(int width, int height, int depth,
                          const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                          void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned short *source = NULL;
    unsigned char *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned short>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned char>(output, y, z, outputRowPitch, outputDepthPitch);
            for (int x = 0; x < width; x++)
            {
                unsigned short rgba = source[x];
                dest[4 * x + 0] = ((rgba & 0xF800) >> 8) | ((rgba & 0xF800) >> 13);
                dest[4 * x + 1] = ((rgba & 0x07E0) >> 3) | ((rgba & 0x07E0) >> 9);
                dest[4 * x + 2] = ((rgba & 0x001F) << 3) | ((rgba & 0x001F) >> 2);
                dest[4 * x + 3] = 0xFF;
            }
        }
    }
}

void loadRGBFloatDataToRGBA(int width, int height, int depth,
                            const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                            void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const float *source = NULL;
    float *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<float>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<float>(output, y, z, outputRowPitch, outputDepthPitch);
            for (int x = 0; x < width; x++)
            {
                dest[4 * x + 0] = source[x * 3 + 0];
                dest[4 * x + 1] = source[x * 3 + 1];
                dest[4 * x + 2] = source[x * 3 + 2];
                dest[4 * x + 3] = 1.0f;
            }
        }
    }
}

void loadRGBFloatDataToNative(int width, int height, int depth,
                              const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                              void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const float *source = NULL;
    float *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<float>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<float>(output, y, z, outputRowPitch, outputDepthPitch);
            memcpy(dest, source, width * 12);
        }
    }
}

void loadRGBHalfFloatDataToRGBA(int width, int height, int depth,
                                const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                                void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned short *source = NULL;
    unsigned short *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned short>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned short>(output, y, z, outputRowPitch, outputDepthPitch);
            for (int x = 0; x < width; x++)
            {
                dest[4 * x + 0] = source[x * 3 + 0];
                dest[4 * x + 1] = source[x * 3 + 1];
                dest[4 * x + 2] = source[x * 3 + 2];
                dest[4 * x + 3] = 0x3C00; // SEEEEEMMMMMMMMMM, S = 0, E = 15, M = 0: 16bit flpt representation of 1
            }
        }
    }
}

void loadRGBAUByteDataToBGRA(int width, int height, int depth,
                             const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                             void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned int *source = NULL;
    unsigned int *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned int>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned int>(output, y, z, outputRowPitch, outputDepthPitch);

            for (int x = 0; x < width; x++)
            {
                unsigned int rgba = source[x];
                dest[x] = (_rotl(rgba, 16) & 0x00ff00ff) | (rgba & 0xff00ff00);
            }
        }
    }
}

void loadRGBAUByteDataToNative(int width, int height, int depth,
                               const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                               void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned int *source = NULL;
    unsigned int *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned int>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned int>(output, y, z, outputRowPitch, outputDepthPitch);

            memcpy(dest, source, width * 4);
        }
    }
}

void loadRGBA4444DataToBGRA(int width, int height, int depth,
                            const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                            void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned short *source = NULL;
    unsigned char *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned short>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned char>(output, y, z, outputRowPitch, outputDepthPitch);
            for (int x = 0; x < width; x++)
            {
                unsigned short rgba = source[x];
                dest[4 * x + 0] = ((rgba & 0x00F0) << 0) | ((rgba & 0x00F0) >> 4);
                dest[4 * x + 1] = ((rgba & 0x0F00) >> 4) | ((rgba & 0x0F00) >> 8);
                dest[4 * x + 2] = ((rgba & 0xF000) >> 8) | ((rgba & 0xF000) >> 12);
                dest[4 * x + 3] = ((rgba & 0x000F) << 4) | ((rgba & 0x000F) >> 0);
            }
        }
    }
}

void loadRGBA4444DataToRGBA(int width, int height, int depth,
                            const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                            void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned short *source = NULL;
    unsigned char *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned short>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned char>(output, y, z, outputRowPitch, outputDepthPitch);
            for (int x = 0; x < width; x++)
            {
                unsigned short rgba = source[x];
                dest[4 * x + 0] = ((rgba & 0xF000) >> 8) | ((rgba & 0xF000) >> 12);
                dest[4 * x + 1] = ((rgba & 0x0F00) >> 4) | ((rgba & 0x0F00) >> 8);
                dest[4 * x + 2] = ((rgba & 0x00F0) << 0) | ((rgba & 0x00F0) >> 4);
                dest[4 * x + 3] = ((rgba & 0x000F) << 4) | ((rgba & 0x000F) >> 0);
            }
        }
    }
}

void loadRGBA5551DataToBGRA(int width, int height, int depth,
                            const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                            void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned short *source = NULL;
    unsigned char *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned short>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned char>(output, y, z, outputRowPitch, outputDepthPitch);
            for (int x = 0; x < width; x++)
            {
                unsigned short rgba = source[x];
                dest[4 * x + 0] = ((rgba & 0x003E) << 2) | ((rgba & 0x003E) >> 3);
                dest[4 * x + 1] = ((rgba & 0x07C0) >> 3) | ((rgba & 0x07C0) >> 8);
                dest[4 * x + 2] = ((rgba & 0xF800) >> 8) | ((rgba & 0xF800) >> 13);
                dest[4 * x + 3] = (rgba & 0x0001) ? 0xFF : 0;
            }
        }
    }
}
void loadRGBA5551DataToRGBA(int width, int height, int depth,
                            const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                            void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned short *source = NULL;
    unsigned char *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned short>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned char>(output, y, z, outputRowPitch, outputDepthPitch);
            for (int x = 0; x < width; x++)
            {
                unsigned short rgba = source[x];
                dest[4 * x + 0] = ((rgba & 0xF800) >> 8) | ((rgba & 0xF800) >> 13);
                dest[4 * x + 1] = ((rgba & 0x07C0) >> 3) | ((rgba & 0x07C0) >> 8);
                dest[4 * x + 2] = ((rgba & 0x003E) << 2) | ((rgba & 0x003E) >> 3);
                dest[4 * x + 3] = (rgba & 0x0001) ? 0xFF : 0;
            }
        }
    }
}

void loadRGBAFloatDataToRGBA(int width, int height, int depth,
                             const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                             void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const float *source = NULL;
    float *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<float>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<float>(output, y, z, outputRowPitch, outputDepthPitch);
            memcpy(dest, source, width * 16);
        }
    }
}

void loadRGBAHalfFloatDataToRGBA(int width, int height, int depth,
                                 const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                                 void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned char>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned char>(output, y, z, outputRowPitch, outputDepthPitch);
            memcpy(dest, source, width * 8);
        }
    }
}

void loadBGRADataToBGRA(int width, int height, int depth,
                        const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                        void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned char>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned char>(output, y, z, outputRowPitch, outputDepthPitch);
            memcpy(dest, source, width*4);
        }
    }
}

void loadRGBA2101010ToNative(int width, int height, int depth,
                             const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                             void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned int *source = NULL;
    unsigned int *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned int>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned int>(output, y, z, outputRowPitch, outputDepthPitch);
            memcpy(dest, source, width * sizeof(unsigned int));
        }
    }
}

void loadRGBA2101010ToRGBA(int width, int height, int depth,
                           const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                           void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned int *source = NULL;
    unsigned char *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned int>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned char>(output, y, z, outputRowPitch, outputDepthPitch);

            for (int x = 0; x < width; x++)
            {
                unsigned int rgba = source[x];
                dest[4 * x + 0] = (rgba & 0x000003FF) >>  2;
                dest[4 * x + 1] = (rgba & 0x000FFC00) >> 12;
                dest[4 * x + 2] = (rgba & 0x3FF00000) >> 22;
                dest[4 * x + 3] = ((rgba & 0xC0000000) >> 30) * 0x55;
            }
        }
    }
}

void loadRGBHalfFloatDataTo999E5(int width, int height, int depth,
                                 const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                                 void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned short *source = NULL;
    unsigned int *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned short>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned int>(output, y, z, outputRowPitch, outputDepthPitch);

            for (int x = 0; x < width; x++)
            {
                dest[x] = gl::convertRGBFloatsTo999E5(gl::float16ToFloat32(source[x * 3 + 0]),
                                                      gl::float16ToFloat32(source[x * 3 + 1]),
                                                      gl::float16ToFloat32(source[x * 3 + 2]));
            }
        }
    }
}

void loadRGBFloatDataTo999E5(int width, int height, int depth,
                             const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                             void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const float *source = NULL;
    unsigned int *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<float>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned int>(output, y, z, outputRowPitch, outputDepthPitch);

            for (int x = 0; x < width; x++)
            {
                dest[x] = gl::convertRGBFloatsTo999E5(source[x * 3 + 0], source[x * 3 + 1], source[x * 3 + 2]);
            }
        }
    }
}

void loadRGBHalfFloatDataTo111110Float(int width, int height, int depth,
                                       const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                                       void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned short *source = NULL;
    unsigned int *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned short>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned int>(output, y, z, outputRowPitch, outputDepthPitch);

            for (int x = 0; x < width; x++)
            {
                dest[x] = (gl::float32ToFloat11(gl::float16ToFloat32(source[x * 3 + 0])) <<  0) |
                          (gl::float32ToFloat11(gl::float16ToFloat32(source[x * 3 + 1])) << 11) |
                          (gl::float32ToFloat10(gl::float16ToFloat32(source[x * 3 + 2])) << 22);
            }
        }
    }
}

void loadRGBFloatDataTo111110Float(int width, int height, int depth,
                                   const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                                   void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const float *source = NULL;
    unsigned int *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<float>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned int>(output, y, z, outputRowPitch, outputDepthPitch);

            for (int x = 0; x < width; x++)
            {
                dest[x] = (gl::float32ToFloat11(source[x * 3 + 0]) <<  0) |
                          (gl::float32ToFloat11(source[x * 3 + 1]) << 11) |
                          (gl::float32ToFloat10(source[x * 3 + 2]) << 22);
            }
        }
    }
}


void loadG8R24DataToR24G8(int width, int height, int depth,
                          const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                          void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned int *source = NULL;
    unsigned int *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<const unsigned int>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned int>(output, y, z, outputRowPitch, outputDepthPitch);

            for (int x = 0; x < width; x++)
            {
                unsigned int d = source[x] >> 8;
                unsigned int s = source[x] & 0xFF;
                dest[x] = d | (s << 24);
            }
        }
    }
}

void loadFloatRGBDataToHalfFloatRGBA(int width, int height, int depth,
                                     const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                                     void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const float *source = NULL;
    unsigned short *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<float>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned short>(output, y, z, outputRowPitch, outputDepthPitch);

            for (int x = 0; x < width; x++)
            {
                dest[x * 4 + 0] = gl::float32ToFloat16(source[x * 3 + 0]);
                dest[x * 4 + 1] = gl::float32ToFloat16(source[x * 3 + 1]);
                dest[x * 4 + 2] = gl::float32ToFloat16(source[x * 3 + 2]);
                dest[x * 4 + 3] = gl::Float16One;
            }
        }
    }
}

void loadUintDataToUshort(int width, int height, int depth,
                          const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                          void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    const unsigned int *source = NULL;
    unsigned short *dest = NULL;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            source = offsetDataPointer<unsigned int>(input, y, z, inputRowPitch, inputDepthPitch);
            dest = offsetDataPointer<unsigned short>(output, y, z, outputRowPitch, outputDepthPitch);

            for (int x = 0; x < width; x++)
            {
                dest[x] = source[x] >> 16;
            }
        }
    }
}

}
