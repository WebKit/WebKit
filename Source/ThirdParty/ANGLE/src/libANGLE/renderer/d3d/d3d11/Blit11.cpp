//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Blit11.cpp: Texture copy utility class.

#include "libANGLE/renderer/d3d/d3d11/Blit11.h"

#include <float.h>

#include "common/utilities.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/d3d/d3d11/RenderTarget11.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/formatutils11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"
#include "libANGLE/renderer/d3d/d3d11/texture_format_table.h"
#include "third_party/trace_event/trace_event.h"

namespace rx
{

namespace
{

// Include inline shaders in the anonymous namespace to make sure no symbols are exported
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthrough2d11vs.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthrougha2d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughdepth2d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughlum2d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughlumalpha2d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughr2d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughr2di11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughr2dui11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrg2d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrg2di11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrg2dui11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgb2d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgb2di11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgb2dui11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgba2d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgba2di11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgba2dui11ps.h"

#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/multiplyalpha_ftof_pm_luma_ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/multiplyalpha_ftof_pm_lumaalpha_ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/multiplyalpha_ftof_pm_rgb_ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/multiplyalpha_ftof_pm_rgba_ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/multiplyalpha_ftof_um_luma_ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/multiplyalpha_ftof_um_lumaalpha_ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/multiplyalpha_ftof_um_rgb_ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/multiplyalpha_ftof_um_rgba_ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/multiplyalpha_ftou_pm_rgb_ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/multiplyalpha_ftou_pm_rgba_ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/multiplyalpha_ftou_pt_rgb_ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/multiplyalpha_ftou_pt_rgba_ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/multiplyalpha_ftou_um_rgb_ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/multiplyalpha_ftou_um_rgba_ps.h"

#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthrough3d11gs.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthrough3d11vs.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughlum3d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughlumalpha3d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughr3d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughr3di11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughr3dui11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrg3d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrg3di11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrg3dui11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgb3d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgb3di11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgb3dui11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgba3d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgba3di11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgba3dui11ps.h"

#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/resolvedepth11_ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/resolvedepthstencil11_ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/resolvedepthstencil11_vs.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/resolvestencil11_ps.h"

#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzlef2darrayps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzlef2dps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzlef3dps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzlei2darrayps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzlei2dps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzlei3dps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzleui2darrayps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzleui2dps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzleui3dps.h"

void StretchedBlitNearest_RowByRow(const gl::Box &sourceArea,
                                   const gl::Box &destArea,
                                   const gl::Rectangle &clippedDestArea,
                                   const gl::Extents &sourceSize,
                                   unsigned int sourceRowPitch,
                                   unsigned int destRowPitch,
                                   size_t pixelSize,
                                   const uint8_t *sourceData,
                                   uint8_t *destData)
{
    int srcHeightSubOne = (sourceArea.height - 1);
    size_t copySize     = pixelSize * destArea.width;
    size_t srcOffset    = sourceArea.x * pixelSize;
    size_t destOffset   = destArea.x * pixelSize;

    for (int y = clippedDestArea.y; y < clippedDestArea.y + clippedDestArea.height; y++)
    {
        float yPerc = static_cast<float>(y - destArea.y) / (destArea.height - 1);

        // Interpolate using the original source rectangle to determine which row to sample from
        // while clamping to the edges
        unsigned int readRow = static_cast<unsigned int>(
            gl::clamp(sourceArea.y + floor(yPerc * srcHeightSubOne + 0.5f), 0, srcHeightSubOne));
        unsigned int writeRow = y;

        const uint8_t *sourceRow = sourceData + readRow * sourceRowPitch + srcOffset;
        uint8_t *destRow         = destData + writeRow * destRowPitch + destOffset;
        memcpy(destRow, sourceRow, copySize);
    }
}

void StretchedBlitNearest_PixelByPixel(const gl::Box &sourceArea,
                                       const gl::Box &destArea,
                                       const gl::Rectangle &clippedDestArea,
                                       const gl::Extents &sourceSize,
                                       unsigned int sourceRowPitch,
                                       unsigned int destRowPitch,
                                       ptrdiff_t readOffset,
                                       ptrdiff_t writeOffset,
                                       size_t copySize,
                                       size_t srcPixelStride,
                                       size_t destPixelStride,
                                       const uint8_t *sourceData,
                                       uint8_t *destData)
{
    auto xMax = clippedDestArea.x + clippedDestArea.width;
    auto yMax = clippedDestArea.y + clippedDestArea.height;

    for (int writeRow = clippedDestArea.y; writeRow < yMax; writeRow++)
    {
        // Interpolate using the original source rectangle to determine which row to sample from
        // while clamping to the edges
        float yPerc    = static_cast<float>(writeRow - destArea.y) / (destArea.height - 1);
        float yRounded = floor(yPerc * (sourceArea.height - 1) + 0.5f);
        unsigned int readRow =
            static_cast<unsigned int>(gl::clamp(sourceArea.y + yRounded, 0, sourceSize.height - 1));

        for (int writeColumn = clippedDestArea.x; writeColumn < xMax; writeColumn++)
        {
            // Interpolate the original source rectangle to determine which column to sample
            // from while clamping to the edges
            float xPerc    = static_cast<float>(writeColumn - destArea.x) / (destArea.width - 1);
            float xRounded = floor(xPerc * (sourceArea.width - 1) + 0.5f);
            unsigned int readColumn = static_cast<unsigned int>(
                gl::clamp(sourceArea.x + xRounded, 0, sourceSize.height - 1));

            const uint8_t *sourcePixel =
                sourceData + readRow * sourceRowPitch + readColumn * srcPixelStride + readOffset;

            uint8_t *destPixel =
                destData + writeRow * destRowPitch + writeColumn * destPixelStride + writeOffset;

            memcpy(destPixel, sourcePixel, copySize);
        }
    }
}

void StretchedBlitNearest(const gl::Box &sourceArea,
                          const gl::Box &destArea,
                          const gl::Rectangle &clipRect,
                          const gl::Extents &sourceSize,
                          unsigned int sourceRowPitch,
                          unsigned int destRowPitch,
                          ptrdiff_t readOffset,
                          ptrdiff_t writeOffset,
                          size_t copySize,
                          size_t srcPixelStride,
                          size_t destPixelStride,
                          const uint8_t *sourceData,
                          uint8_t *destData)
{
    gl::Rectangle clippedDestArea(destArea.x, destArea.y, destArea.width, destArea.height);
    gl::ClipRectangle(clippedDestArea, clipRect, &clippedDestArea);

    // Determine if entire rows can be copied at once instead of each individual pixel. There
    // must be no out of bounds lookups, whole rows copies, and no scale.
    if (sourceArea.width == clippedDestArea.width && sourceArea.x >= 0 &&
        sourceArea.x + sourceArea.width <= sourceSize.width && copySize == srcPixelStride &&
        copySize == destPixelStride)
    {
        StretchedBlitNearest_RowByRow(sourceArea, destArea, clippedDestArea, sourceSize,
                                      sourceRowPitch, destRowPitch, srcPixelStride, sourceData,
                                      destData);
    }
    else
    {
        StretchedBlitNearest_PixelByPixel(sourceArea, destArea, clippedDestArea, sourceSize,
                                          sourceRowPitch, destRowPitch, readOffset, writeOffset,
                                          copySize, srcPixelStride, destPixelStride, sourceData,
                                          destData);
    }
}

using DepthStencilLoader = void(const float *, uint8_t *);

void LoadDepth16(const float *source, uint8_t *dest)
{
    uint32_t convertedDepth = gl::floatToNormalized<16, uint32_t>(source[0]);
    memcpy(dest, &convertedDepth, 2u);
}

void LoadDepth24(const float *source, uint8_t *dest)
{
    uint32_t convertedDepth = gl::floatToNormalized<24, uint32_t>(source[0]);
    memcpy(dest, &convertedDepth, 3u);
}

void LoadStencilHelper(const float *source, uint8_t *dest)
{
    uint32_t convertedStencil = gl::getShiftedData<8, 0>(static_cast<uint32_t>(source[1]));
    memcpy(dest, &convertedStencil, 1u);
}

void LoadStencil8(const float *source, uint8_t *dest)
{
    // STENCIL_INDEX8 is implemented with D24S8, with the depth bits unused. Writes zero for safety.
    float zero = 0.0f;
    LoadDepth24(&zero, &dest[0]);
    LoadStencilHelper(source, &dest[3]);
}

void LoadDepth24Stencil8(const float *source, uint8_t *dest)
{
    LoadDepth24(source, &dest[0]);
    LoadStencilHelper(source, &dest[3]);
}

void LoadDepth32F(const float *source, uint8_t *dest)
{
    memcpy(dest, source, sizeof(float));
}

void LoadDepth32FStencil8(const float *source, uint8_t *dest)
{
    LoadDepth32F(source, &dest[0]);
    LoadStencilHelper(source, &dest[4]);
}

template <DepthStencilLoader loader>
void CopyDepthStencil(const gl::Box &sourceArea,
                      const gl::Box &destArea,
                      const gl::Rectangle &clippedDestArea,
                      const gl::Extents &sourceSize,
                      unsigned int sourceRowPitch,
                      unsigned int destRowPitch,
                      ptrdiff_t readOffset,
                      ptrdiff_t writeOffset,
                      size_t copySize,
                      size_t srcPixelStride,
                      size_t destPixelStride,
                      const uint8_t *sourceData,
                      uint8_t *destData)
{
    // No stretching or subregions are supported, only full blits.
    ASSERT(sourceArea == destArea);
    ASSERT(sourceSize.width == sourceArea.width && sourceSize.height == sourceArea.height &&
           sourceSize.depth == 1);
    ASSERT(clippedDestArea.width == sourceSize.width &&
           clippedDestArea.height == sourceSize.height);
    ASSERT(readOffset == 0 && writeOffset == 0);
    ASSERT(destArea.x == 0 && destArea.y == 0);

    for (int row = 0; row < destArea.height; ++row)
    {
        for (int column = 0; column < destArea.width; ++column)
        {
            ptrdiff_t offset         = row * sourceRowPitch + column * srcPixelStride;
            const float *sourcePixel = reinterpret_cast<const float *>(sourceData + offset);

            uint8_t *destPixel = destData + row * destRowPitch + column * destPixelStride;

            loader(sourcePixel, destPixel);
        }
    }
}

void Depth32FStencil8ToDepth32F(const float *source, float *dest)
{
    *dest = *source;
}

void Depth24Stencil8ToDepth32F(const uint32_t *source, float *dest)
{
    uint32_t normDepth = source[0] & 0x00FFFFFF;
    float floatDepth   = gl::normalizedToFloat<24>(normDepth);
    *dest              = floatDepth;
}

void BlitD24S8ToD32F(const gl::Box &sourceArea,
                     const gl::Box &destArea,
                     const gl::Rectangle &clippedDestArea,
                     const gl::Extents &sourceSize,
                     unsigned int sourceRowPitch,
                     unsigned int destRowPitch,
                     ptrdiff_t readOffset,
                     ptrdiff_t writeOffset,
                     size_t copySize,
                     size_t srcPixelStride,
                     size_t destPixelStride,
                     const uint8_t *sourceData,
                     uint8_t *destData)
{
    // No stretching or subregions are supported, only full blits.
    ASSERT(sourceArea == destArea);
    ASSERT(sourceSize.width == sourceArea.width && sourceSize.height == sourceArea.height &&
           sourceSize.depth == 1);
    ASSERT(clippedDestArea.width == sourceSize.width &&
           clippedDestArea.height == sourceSize.height);
    ASSERT(readOffset == 0 && writeOffset == 0);
    ASSERT(destArea.x == 0 && destArea.y == 0);

    for (int row = 0; row < destArea.height; ++row)
    {
        for (int column = 0; column < destArea.width; ++column)
        {
            ptrdiff_t offset            = row * sourceRowPitch + column * srcPixelStride;
            const uint32_t *sourcePixel = reinterpret_cast<const uint32_t *>(sourceData + offset);

            float *destPixel =
                reinterpret_cast<float *>(destData + row * destRowPitch + column * destPixelStride);

            Depth24Stencil8ToDepth32F(sourcePixel, destPixel);
        }
    }
}

void BlitD32FS8ToD32F(const gl::Box &sourceArea,
                      const gl::Box &destArea,
                      const gl::Rectangle &clippedDestArea,
                      const gl::Extents &sourceSize,
                      unsigned int sourceRowPitch,
                      unsigned int destRowPitch,
                      ptrdiff_t readOffset,
                      ptrdiff_t writeOffset,
                      size_t copySize,
                      size_t srcPixelStride,
                      size_t destPixelStride,
                      const uint8_t *sourceData,
                      uint8_t *destData)
{
    // No stretching or subregions are supported, only full blits.
    ASSERT(sourceArea == destArea);
    ASSERT(sourceSize.width == sourceArea.width && sourceSize.height == sourceArea.height &&
           sourceSize.depth == 1);
    ASSERT(clippedDestArea.width == sourceSize.width &&
           clippedDestArea.height == sourceSize.height);
    ASSERT(readOffset == 0 && writeOffset == 0);
    ASSERT(destArea.x == 0 && destArea.y == 0);

    for (int row = 0; row < destArea.height; ++row)
    {
        for (int column = 0; column < destArea.width; ++column)
        {
            ptrdiff_t offset         = row * sourceRowPitch + column * srcPixelStride;
            const float *sourcePixel = reinterpret_cast<const float *>(sourceData + offset);
            float *destPixel =
                reinterpret_cast<float *>(destData + row * destRowPitch + column * destPixelStride);

            Depth32FStencil8ToDepth32F(sourcePixel, destPixel);
        }
    }
}

Blit11::BlitConvertFunction *GetCopyDepthStencilFunction(GLenum internalFormat)
{
    switch (internalFormat)
    {
        case GL_DEPTH_COMPONENT16:
            return &CopyDepthStencil<LoadDepth16>;
        case GL_DEPTH_COMPONENT24:
            return &CopyDepthStencil<LoadDepth24>;
        case GL_DEPTH_COMPONENT32F:
            return &CopyDepthStencil<LoadDepth32F>;
        case GL_STENCIL_INDEX8:
            return &CopyDepthStencil<LoadStencil8>;
        case GL_DEPTH24_STENCIL8:
            return &CopyDepthStencil<LoadDepth24Stencil8>;
        case GL_DEPTH32F_STENCIL8:
            return &CopyDepthStencil<LoadDepth32FStencil8>;
        default:
            UNREACHABLE();
            return nullptr;
    }
}

inline void GenerateVertexCoords(const gl::Box &sourceArea,
                                 const gl::Extents &sourceSize,
                                 const gl::Box &destArea,
                                 const gl::Extents &destSize,
                                 float *x1,
                                 float *y1,
                                 float *x2,
                                 float *y2,
                                 float *u1,
                                 float *v1,
                                 float *u2,
                                 float *v2)
{
    *x1 = (destArea.x / float(destSize.width)) * 2.0f - 1.0f;
    *y1 = ((destSize.height - destArea.y - destArea.height) / float(destSize.height)) * 2.0f - 1.0f;
    *x2 = ((destArea.x + destArea.width) / float(destSize.width)) * 2.0f - 1.0f;
    *y2 = ((destSize.height - destArea.y) / float(destSize.height)) * 2.0f - 1.0f;

    *u1 = sourceArea.x / float(sourceSize.width);
    *v1 = sourceArea.y / float(sourceSize.height);
    *u2 = (sourceArea.x + sourceArea.width) / float(sourceSize.width);
    *v2 = (sourceArea.y + sourceArea.height) / float(sourceSize.height);
}

void Write2DVertices(const gl::Box &sourceArea,
                     const gl::Extents &sourceSize,
                     const gl::Box &destArea,
                     const gl::Extents &destSize,
                     void *outVertices,
                     unsigned int *outStride,
                     unsigned int *outVertexCount,
                     D3D11_PRIMITIVE_TOPOLOGY *outTopology)
{
    float x1, y1, x2, y2, u1, v1, u2, v2;
    GenerateVertexCoords(sourceArea, sourceSize, destArea, destSize, &x1, &y1, &x2, &y2, &u1, &v1,
                         &u2, &v2);

    d3d11::PositionTexCoordVertex *vertices =
        static_cast<d3d11::PositionTexCoordVertex *>(outVertices);

    d3d11::SetPositionTexCoordVertex(&vertices[0], x1, y1, u1, v2);
    d3d11::SetPositionTexCoordVertex(&vertices[1], x1, y2, u1, v1);
    d3d11::SetPositionTexCoordVertex(&vertices[2], x2, y1, u2, v2);
    d3d11::SetPositionTexCoordVertex(&vertices[3], x2, y2, u2, v1);

    *outStride      = sizeof(d3d11::PositionTexCoordVertex);
    *outVertexCount = 4;
    *outTopology    = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
}

void Write3DVertices(const gl::Box &sourceArea,
                     const gl::Extents &sourceSize,
                     const gl::Box &destArea,
                     const gl::Extents &destSize,
                     void *outVertices,
                     unsigned int *outStride,
                     unsigned int *outVertexCount,
                     D3D11_PRIMITIVE_TOPOLOGY *outTopology)
{
    ASSERT(sourceSize.depth > 0 && destSize.depth > 0);

    float x1, y1, x2, y2, u1, v1, u2, v2;
    GenerateVertexCoords(sourceArea, sourceSize, destArea, destSize, &x1, &y1, &x2, &y2, &u1, &v1,
                         &u2, &v2);

    d3d11::PositionLayerTexCoord3DVertex *vertices =
        static_cast<d3d11::PositionLayerTexCoord3DVertex *>(outVertices);

    for (int i = 0; i < destSize.depth; i++)
    {
        float readDepth = (float)i / std::max(destSize.depth - 1, 1);

        d3d11::SetPositionLayerTexCoord3DVertex(&vertices[i * 6 + 0], x1, y1, i, u1, v2, readDepth);
        d3d11::SetPositionLayerTexCoord3DVertex(&vertices[i * 6 + 1], x1, y2, i, u1, v1, readDepth);
        d3d11::SetPositionLayerTexCoord3DVertex(&vertices[i * 6 + 2], x2, y1, i, u2, v2, readDepth);

        d3d11::SetPositionLayerTexCoord3DVertex(&vertices[i * 6 + 3], x1, y2, i, u1, v1, readDepth);
        d3d11::SetPositionLayerTexCoord3DVertex(&vertices[i * 6 + 4], x2, y2, i, u2, v1, readDepth);
        d3d11::SetPositionLayerTexCoord3DVertex(&vertices[i * 6 + 5], x2, y1, i, u2, v2, readDepth);
    }

    *outStride      = sizeof(d3d11::PositionLayerTexCoord3DVertex);
    *outVertexCount = destSize.depth * 6;
    *outTopology    = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

unsigned int GetSwizzleIndex(GLenum swizzle)
{
    unsigned int colorIndex = 0;

    switch (swizzle)
    {
        case GL_RED:
            colorIndex = 0;
            break;
        case GL_GREEN:
            colorIndex = 1;
            break;
        case GL_BLUE:
            colorIndex = 2;
            break;
        case GL_ALPHA:
            colorIndex = 3;
            break;
        case GL_ZERO:
            colorIndex = 4;
            break;
        case GL_ONE:
            colorIndex = 5;
            break;
        default:
            UNREACHABLE();
            break;
    }

    return colorIndex;
}

D3D11_BLEND_DESC GetAlphaMaskBlendStateDesc()
{
    D3D11_BLEND_DESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.RenderTarget[0].BlendEnable           = TRUE;
    desc.RenderTarget[0].SrcBlend              = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlend             = D3D11_BLEND_ZERO;
    desc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ZERO;
    desc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
    desc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED |
                                                 D3D11_COLOR_WRITE_ENABLE_GREEN |
                                                 D3D11_COLOR_WRITE_ENABLE_BLUE;
    return desc;
}

D3D11_INPUT_ELEMENT_DESC quad2DLayout[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

D3D11_INPUT_ELEMENT_DESC quad3DLayout[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"LAYER", 0, DXGI_FORMAT_R32_UINT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

DXGI_FORMAT GetStencilSRVFormat(const d3d11::Format &formatSet)
{
    switch (formatSet.texFormat)
    {
        case DXGI_FORMAT_R32G8X24_TYPELESS:
            return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
        case DXGI_FORMAT_R24G8_TYPELESS:
            return DXGI_FORMAT_X24_TYPELESS_G8_UINT;
        default:
            UNREACHABLE();
            return DXGI_FORMAT_UNKNOWN;
    }
}

}  // namespace

Blit11::Shader::Shader() = default;

Blit11::Shader::Shader(Shader &&other) = default;

Blit11::Shader::~Shader() = default;

Blit11::Shader &Blit11::Shader::operator=(Blit11::Shader &&other) = default;

Blit11::Blit11(Renderer11 *renderer)
    : mRenderer(renderer),
      mResourcesInitialized(false),
      mVertexBuffer(),
      mPointSampler(),
      mLinearSampler(),
      mScissorEnabledRasterizerState(),
      mScissorDisabledRasterizerState(),
      mDepthStencilState(),
      mQuad2DIL(quad2DLayout,
                ArraySize(quad2DLayout),
                g_VS_Passthrough2D,
                ArraySize(g_VS_Passthrough2D),
                "Blit11 2D input layout"),
      mQuad2DVS(g_VS_Passthrough2D, ArraySize(g_VS_Passthrough2D), "Blit11 2D vertex shader"),
      mDepthPS(g_PS_PassthroughDepth2D,
               ArraySize(g_PS_PassthroughDepth2D),
               "Blit11 2D depth pixel shader"),
      mQuad3DIL(quad3DLayout,
                ArraySize(quad3DLayout),
                g_VS_Passthrough3D,
                ArraySize(g_VS_Passthrough3D),
                "Blit11 3D input layout"),
      mQuad3DVS(g_VS_Passthrough3D, ArraySize(g_VS_Passthrough3D), "Blit11 3D vertex shader"),
      mQuad3DGS(g_GS_Passthrough3D, ArraySize(g_GS_Passthrough3D), "Blit11 3D geometry shader"),
      mAlphaMaskBlendState(GetAlphaMaskBlendStateDesc(), "Blit11 Alpha Mask Blend"),
      mSwizzleCB(),
      mResolveDepthStencilVS(g_VS_ResolveDepthStencil,
                             ArraySize(g_VS_ResolveDepthStencil),
                             "Blit11::mResolveDepthStencilVS"),
      mResolveDepthPS(g_PS_ResolveDepth, ArraySize(g_PS_ResolveDepth), "Blit11::mResolveDepthPS"),
      mResolveDepthStencilPS(g_PS_ResolveDepthStencil,
                             ArraySize(g_PS_ResolveDepthStencil),
                             "Blit11::mResolveDepthStencilPS"),
      mResolveStencilPS(g_PS_ResolveStencil,
                        ArraySize(g_PS_ResolveStencil),
                        "Blit11::mResolveStencilPS"),
      mStencilSRV(),
      mResolvedDepthStencilRTView()
{
}

Blit11::~Blit11()
{
}

gl::Error Blit11::initResources()
{
    if (mResourcesInitialized)
    {
        return gl::NoError();
    }

    TRACE_EVENT0("gpu.angle", "Blit11::initResources");

    D3D11_BUFFER_DESC vbDesc;
    vbDesc.ByteWidth =
        static_cast<unsigned int>(std::max(sizeof(d3d11::PositionLayerTexCoord3DVertex),
                                           sizeof(d3d11::PositionTexCoordVertex)) *
                                  6 * mRenderer->getNativeCaps().max3DTextureSize);
    vbDesc.Usage               = D3D11_USAGE_DYNAMIC;
    vbDesc.BindFlags           = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
    vbDesc.MiscFlags           = 0;
    vbDesc.StructureByteStride = 0;

    ANGLE_TRY(mRenderer->allocateResource(vbDesc, &mVertexBuffer));
    mVertexBuffer.setDebugName("Blit11 vertex buffer");

    D3D11_SAMPLER_DESC pointSamplerDesc;
    pointSamplerDesc.Filter         = D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
    pointSamplerDesc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    pointSamplerDesc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    pointSamplerDesc.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
    pointSamplerDesc.MipLODBias     = 0.0f;
    pointSamplerDesc.MaxAnisotropy  = 0;
    pointSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    pointSamplerDesc.BorderColor[0] = 0.0f;
    pointSamplerDesc.BorderColor[1] = 0.0f;
    pointSamplerDesc.BorderColor[2] = 0.0f;
    pointSamplerDesc.BorderColor[3] = 0.0f;
    pointSamplerDesc.MinLOD         = 0.0f;
    pointSamplerDesc.MaxLOD         = FLT_MAX;

    ANGLE_TRY(mRenderer->allocateResource(pointSamplerDesc, &mPointSampler));
    mPointSampler.setDebugName("Blit11 point sampler");

    D3D11_SAMPLER_DESC linearSamplerDesc;
    linearSamplerDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    linearSamplerDesc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    linearSamplerDesc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    linearSamplerDesc.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
    linearSamplerDesc.MipLODBias     = 0.0f;
    linearSamplerDesc.MaxAnisotropy  = 0;
    linearSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    linearSamplerDesc.BorderColor[0] = 0.0f;
    linearSamplerDesc.BorderColor[1] = 0.0f;
    linearSamplerDesc.BorderColor[2] = 0.0f;
    linearSamplerDesc.BorderColor[3] = 0.0f;
    linearSamplerDesc.MinLOD         = 0.0f;
    linearSamplerDesc.MaxLOD         = FLT_MAX;

    ANGLE_TRY(mRenderer->allocateResource(linearSamplerDesc, &mLinearSampler));
    mLinearSampler.setDebugName("Blit11 linear sampler");

    // Use a rasterizer state that will not cull so that inverted quads will not be culled
    D3D11_RASTERIZER_DESC rasterDesc;
    rasterDesc.FillMode              = D3D11_FILL_SOLID;
    rasterDesc.CullMode              = D3D11_CULL_NONE;
    rasterDesc.FrontCounterClockwise = FALSE;
    rasterDesc.DepthBias             = 0;
    rasterDesc.SlopeScaledDepthBias  = 0.0f;
    rasterDesc.DepthBiasClamp        = 0.0f;
    rasterDesc.DepthClipEnable       = TRUE;
    rasterDesc.MultisampleEnable     = FALSE;
    rasterDesc.AntialiasedLineEnable = FALSE;

    rasterDesc.ScissorEnable = TRUE;
    ANGLE_TRY(mRenderer->allocateResource(rasterDesc, &mScissorEnabledRasterizerState));
    mScissorEnabledRasterizerState.setDebugName("Blit11 scissoring rasterizer state");

    rasterDesc.ScissorEnable = FALSE;
    ANGLE_TRY(mRenderer->allocateResource(rasterDesc, &mScissorDisabledRasterizerState));
    mScissorDisabledRasterizerState.setDebugName("Blit11 no scissoring rasterizer state");

    D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
    depthStencilDesc.DepthEnable                  = TRUE;
    depthStencilDesc.DepthWriteMask               = D3D11_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc                    = D3D11_COMPARISON_ALWAYS;
    depthStencilDesc.StencilEnable                = FALSE;
    depthStencilDesc.StencilReadMask              = D3D11_DEFAULT_STENCIL_READ_MASK;
    depthStencilDesc.StencilWriteMask             = D3D11_DEFAULT_STENCIL_WRITE_MASK;
    depthStencilDesc.FrontFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilPassOp      = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilFunc        = D3D11_COMPARISON_ALWAYS;
    depthStencilDesc.BackFace.StencilFailOp       = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.BackFace.StencilDepthFailOp  = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.BackFace.StencilPassOp       = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.BackFace.StencilFunc         = D3D11_COMPARISON_ALWAYS;

    ANGLE_TRY(mRenderer->allocateResource(depthStencilDesc, &mDepthStencilState));
    mDepthStencilState.setDebugName("Blit11 depth stencil state");

    D3D11_BUFFER_DESC swizzleBufferDesc;
    swizzleBufferDesc.ByteWidth           = sizeof(unsigned int) * 4;
    swizzleBufferDesc.Usage               = D3D11_USAGE_DYNAMIC;
    swizzleBufferDesc.BindFlags           = D3D11_BIND_CONSTANT_BUFFER;
    swizzleBufferDesc.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
    swizzleBufferDesc.MiscFlags           = 0;
    swizzleBufferDesc.StructureByteStride = 0;

    ANGLE_TRY(mRenderer->allocateResource(swizzleBufferDesc, &mSwizzleCB));
    mSwizzleCB.setDebugName("Blit11 swizzle constant buffer");

    mResourcesInitialized = true;

    return gl::NoError();
}

// static
Blit11::BlitShaderType Blit11::GetBlitShaderType(GLenum destinationFormat,
                                                 GLenum sourceFormat,
                                                 bool isSigned,
                                                 bool unpackPremultiplyAlpha,
                                                 bool unpackUnmultiplyAlpha,
                                                 ShaderDimension dimension)
{
    if (dimension == SHADER_3D)
    {
        ASSERT(!unpackPremultiplyAlpha && !unpackUnmultiplyAlpha);

        if (isSigned)
        {
            switch (destinationFormat)
            {
                case GL_RGBA_INTEGER:
                    return BLITSHADER_3D_RGBAI;
                case GL_RGB_INTEGER:
                    return BLITSHADER_3D_RGBI;
                case GL_RG_INTEGER:
                    return BLITSHADER_3D_RGI;
                case GL_RED_INTEGER:
                    return BLITSHADER_3D_RI;
                default:
                    UNREACHABLE();
                    return BLITSHADER_INVALID;
            }
        }
        else
        {
            switch (destinationFormat)
            {
                case GL_RGBA:
                    return BLITSHADER_3D_RGBAF;
                case GL_RGBA_INTEGER:
                    return BLITSHADER_3D_RGBAUI;
                case GL_BGRA_EXT:
                    return BLITSHADER_3D_BGRAF;
                case GL_RGB:
                    return BLITSHADER_3D_RGBF;
                case GL_RGB_INTEGER:
                    return BLITSHADER_3D_RGBUI;
                case GL_RG:
                    return BLITSHADER_3D_RGF;
                case GL_RG_INTEGER:
                    return BLITSHADER_3D_RGUI;
                case GL_RED:
                    return BLITSHADER_3D_RF;
                case GL_RED_INTEGER:
                    return BLITSHADER_3D_RUI;
                case GL_ALPHA:
                    return BLITSHADER_3D_ALPHA;
                case GL_LUMINANCE:
                    return BLITSHADER_3D_LUMA;
                case GL_LUMINANCE_ALPHA:
                    return BLITSHADER_3D_LUMAALPHA;
                default:
                    UNREACHABLE();
                    return BLITSHADER_INVALID;
            }
        }
    }
    else if (isSigned)
    {
        ASSERT(!unpackPremultiplyAlpha && !unpackUnmultiplyAlpha);

        switch (destinationFormat)
        {
            case GL_RGBA_INTEGER:
                return BLITSHADER_2D_RGBAI;
            case GL_RGB_INTEGER:
                return BLITSHADER_2D_RGBI;
            case GL_RG_INTEGER:
                return BLITSHADER_2D_RGI;
            case GL_RED_INTEGER:
                return BLITSHADER_2D_RI;
            default:
                UNREACHABLE();
                return BLITSHADER_INVALID;
        }
    }
    else
    {
        bool floatToIntBlit =
            !gl::IsIntegerFormat(sourceFormat) && gl::IsIntegerFormat(destinationFormat);
        if (unpackPremultiplyAlpha != unpackUnmultiplyAlpha || floatToIntBlit)
        {
            switch (destinationFormat)
            {
                case GL_RGBA:
                case GL_BGRA_EXT:
                    ASSERT(!floatToIntBlit);
                    return unpackPremultiplyAlpha ? BLITSHADER_2D_RGBAF_PREMULTIPLY
                                                  : BLITSHADER_2D_RGBAF_UNMULTIPLY;

                case GL_RGB:
                case GL_RG:
                case GL_RED:
                    ASSERT(!floatToIntBlit);
                    return unpackPremultiplyAlpha ? BLITSHADER_2D_RGBF_PREMULTIPLY
                                                  : BLITSHADER_2D_RGBF_UNMULTIPLY;

                case GL_RGBA_INTEGER:
                    if (unpackPremultiplyAlpha == unpackUnmultiplyAlpha)
                    {
                        return BLITSHADER_2D_RGBAF_TOUI;
                    }
                    else
                    {
                        return unpackPremultiplyAlpha ? BLITSHADER_2D_RGBAF_TOUI_PREMULTIPLY
                                                      : BLITSHADER_2D_RGBAF_TOUI_UNMULTIPLY;
                    }

                case GL_RGB_INTEGER:
                case GL_RG_INTEGER:
                case GL_RED_INTEGER:
                    if (unpackPremultiplyAlpha == unpackUnmultiplyAlpha)
                    {
                        return BLITSHADER_2D_RGBF_TOUI;
                    }
                    else
                    {
                        return unpackPremultiplyAlpha ? BLITSHADER_2D_RGBF_TOUI_PREMULTIPLY
                                                      : BLITSHADER_2D_RGBF_TOUI_UNMULTIPLY;
                    }
                case GL_LUMINANCE:
                    ASSERT(!floatToIntBlit);
                    return unpackPremultiplyAlpha ? BLITSHADER_2D_LUMAF_PREMULTIPLY
                                                  : BLITSHADER_2D_LUMAF_UNMULTIPLY;
                case GL_LUMINANCE_ALPHA:
                    ASSERT(!floatToIntBlit);
                    return unpackPremultiplyAlpha ? BLITSHADER_2D_LUMAALPHAF_PREMULTIPLY
                                                  : BLITSHADER_2D_LUMAALPHAF_UNMULTIPLY;
                case GL_ALPHA:
                    ASSERT(!floatToIntBlit);
                    return BLITSHADER_2D_ALPHA;
                default:
                    UNREACHABLE();
                    return BLITSHADER_INVALID;
            }
        }
        else
        {
            switch (destinationFormat)
            {
                case GL_RGBA:
                    return BLITSHADER_2D_RGBAF;
                case GL_RGBA_INTEGER:
                    return BLITSHADER_2D_RGBAUI;
                case GL_BGRA_EXT:
                    return BLITSHADER_2D_BGRAF;
                case GL_RGB:
                    return BLITSHADER_2D_RGBF;
                case GL_RGB_INTEGER:
                    return BLITSHADER_2D_RGBUI;
                case GL_RG:
                    return BLITSHADER_2D_RGF;
                case GL_RG_INTEGER:
                    return BLITSHADER_2D_RGUI;
                case GL_RED:
                    return BLITSHADER_2D_RF;
                case GL_RED_INTEGER:
                    return BLITSHADER_2D_RUI;
                case GL_ALPHA:
                    return BLITSHADER_2D_ALPHA;
                case GL_LUMINANCE:
                    return BLITSHADER_2D_LUMA;
                case GL_LUMINANCE_ALPHA:
                    return BLITSHADER_2D_LUMAALPHA;
                default:
                    UNREACHABLE();
                    return BLITSHADER_INVALID;
            }
        }
    }
}

// static
Blit11::SwizzleShaderType Blit11::GetSwizzleShaderType(GLenum type,
                                                       D3D11_SRV_DIMENSION dimensionality)
{
    switch (dimensionality)
    {
        case D3D11_SRV_DIMENSION_TEXTURE2D:
            switch (type)
            {
                case GL_FLOAT:
                    return SWIZZLESHADER_2D_FLOAT;
                case GL_UNSIGNED_INT:
                    return SWIZZLESHADER_2D_UINT;
                case GL_INT:
                    return SWIZZLESHADER_2D_INT;
                default:
                    UNREACHABLE();
                    return SWIZZLESHADER_INVALID;
            }
        case D3D11_SRV_DIMENSION_TEXTURECUBE:
            switch (type)
            {
                case GL_FLOAT:
                    return SWIZZLESHADER_CUBE_FLOAT;
                case GL_UNSIGNED_INT:
                    return SWIZZLESHADER_CUBE_UINT;
                case GL_INT:
                    return SWIZZLESHADER_CUBE_INT;
                default:
                    UNREACHABLE();
                    return SWIZZLESHADER_INVALID;
            }
        case D3D11_SRV_DIMENSION_TEXTURE3D:
            switch (type)
            {
                case GL_FLOAT:
                    return SWIZZLESHADER_3D_FLOAT;
                case GL_UNSIGNED_INT:
                    return SWIZZLESHADER_3D_UINT;
                case GL_INT:
                    return SWIZZLESHADER_3D_INT;
                default:
                    UNREACHABLE();
                    return SWIZZLESHADER_INVALID;
            }
        case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
            switch (type)
            {
                case GL_FLOAT:
                    return SWIZZLESHADER_ARRAY_FLOAT;
                case GL_UNSIGNED_INT:
                    return SWIZZLESHADER_ARRAY_UINT;
                case GL_INT:
                    return SWIZZLESHADER_ARRAY_INT;
                default:
                    UNREACHABLE();
                    return SWIZZLESHADER_INVALID;
            }
        default:
            UNREACHABLE();
            return SWIZZLESHADER_INVALID;
    }
}

gl::Error Blit11::getShaderSupport(const Shader &shader, Blit11::ShaderSupport *supportOut)
{
    if (shader.dimension == SHADER_2D)
    {
        ANGLE_TRY(mQuad2DIL.resolve(mRenderer));
        ANGLE_TRY(mQuad2DVS.resolve(mRenderer));
        supportOut->inputLayout         = &mQuad2DIL.getObj();
        supportOut->vertexShader        = &mQuad2DVS.getObj();
        supportOut->geometryShader      = nullptr;
        supportOut->vertexWriteFunction = Write2DVertices;
    }
    else
    {
        ASSERT(shader.dimension == SHADER_3D);
        ANGLE_TRY(mQuad3DIL.resolve(mRenderer));
        ANGLE_TRY(mQuad3DVS.resolve(mRenderer));
        ANGLE_TRY(mQuad3DGS.resolve(mRenderer));
        supportOut->inputLayout         = &mQuad2DIL.getObj();
        supportOut->vertexShader        = &mQuad3DVS.getObj();
        supportOut->geometryShader      = &mQuad3DGS.getObj();
        supportOut->vertexWriteFunction = Write3DVertices;
    }

    return gl::NoError();
}

gl::Error Blit11::swizzleTexture(const gl::Context *context,
                                 const d3d11::SharedSRV &source,
                                 const d3d11::RenderTargetView &dest,
                                 const gl::Extents &size,
                                 const gl::SwizzleState &swizzleTarget)
{
    ANGLE_TRY(initResources());

    HRESULT result;
    ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();

    D3D11_SHADER_RESOURCE_VIEW_DESC sourceSRVDesc;
    source.get()->GetDesc(&sourceSRVDesc);

    GLenum componentType = d3d11::GetComponentType(sourceSRVDesc.Format);
    if (componentType == GL_NONE)
    {
        // We're swizzling the depth component of a depth-stencil texture.
        switch (sourceSRVDesc.Format)
        {
            case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
                componentType = GL_UNSIGNED_NORMALIZED;
                break;
            case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
                componentType = GL_FLOAT;
                break;
            default:
                UNREACHABLE();
                break;
        }
    }

    GLenum shaderType = GL_NONE;
    switch (componentType)
    {
        case GL_UNSIGNED_NORMALIZED:
        case GL_SIGNED_NORMALIZED:
        case GL_FLOAT:
            shaderType = GL_FLOAT;
            break;
        case GL_INT:
            shaderType = GL_INT;
            break;
        case GL_UNSIGNED_INT:
            shaderType = GL_UNSIGNED_INT;
            break;
        default:
            UNREACHABLE();
            break;
    }

    const Shader *shader = nullptr;
    ANGLE_TRY(getSwizzleShader(shaderType, sourceSRVDesc.ViewDimension, &shader));

    // Set vertices
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    result =
        deviceContext->Map(mVertexBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result))
    {
        return gl::OutOfMemory() << "Failed to map internal vertex buffer for swizzle, "
                                 << gl::FmtHR(result);
    }

    ShaderSupport support;
    ANGLE_TRY(getShaderSupport(*shader, &support));

    UINT stride    = 0;
    UINT drawCount = 0;
    D3D11_PRIMITIVE_TOPOLOGY topology;

    gl::Box area(0, 0, 0, size.width, size.height, size.depth);
    support.vertexWriteFunction(area, size, area, size, mappedResource.pData, &stride, &drawCount,
                                &topology);

    deviceContext->Unmap(mVertexBuffer.get(), 0);

    // Set constant buffer
    result = deviceContext->Map(mSwizzleCB.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result))
    {
        return gl::OutOfMemory() << "Failed to map internal constant buffer for swizzle, "
                                 << gl::FmtHR(result);
    }

    unsigned int *swizzleIndices = reinterpret_cast<unsigned int *>(mappedResource.pData);
    swizzleIndices[0]            = GetSwizzleIndex(swizzleTarget.swizzleRed);
    swizzleIndices[1]            = GetSwizzleIndex(swizzleTarget.swizzleGreen);
    swizzleIndices[2]            = GetSwizzleIndex(swizzleTarget.swizzleBlue);
    swizzleIndices[3]            = GetSwizzleIndex(swizzleTarget.swizzleAlpha);

    deviceContext->Unmap(mSwizzleCB.get(), 0);

    StateManager11 *stateManager = mRenderer->getStateManager();

    // Apply vertex buffer
    stateManager->setSingleVertexBuffer(&mVertexBuffer, stride, 0);

    // Apply constant buffer
    stateManager->setPixelConstantBuffer(0, &mSwizzleCB);

    // Apply state
    stateManager->setSimpleBlendState(nullptr);
    stateManager->setDepthStencilState(nullptr, 0xFFFFFFFF);
    stateManager->setRasterizerState(&mScissorDisabledRasterizerState);

    // Apply shaders
    stateManager->setInputLayout(support.inputLayout);
    stateManager->setPrimitiveTopology(topology);

    stateManager->setDrawShaders(support.vertexShader, support.geometryShader,
                                 &shader->pixelShader);

    // Apply render target
    stateManager->setRenderTarget(dest.get(), nullptr);

    // Set the viewport
    stateManager->setSimpleViewport(size);

    // Apply textures and sampler
    stateManager->setSimplePixelTextureAndSampler(source, mPointSampler);

    // Draw the quad
    deviceContext->Draw(drawCount, 0);

    return gl::NoError();
}

gl::Error Blit11::copyTexture(const gl::Context *context,
                              const d3d11::SharedSRV &source,
                              const gl::Box &sourceArea,
                              const gl::Extents &sourceSize,
                              GLenum sourceFormat,
                              const d3d11::RenderTargetView &dest,
                              const gl::Box &destArea,
                              const gl::Extents &destSize,
                              const gl::Rectangle *scissor,
                              GLenum destFormat,
                              GLenum filter,
                              bool maskOffAlpha,
                              bool unpackPremultiplyAlpha,
                              bool unpackUnmultiplyAlpha)
{
    ANGLE_TRY(initResources());

    HRESULT result;
    ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();

    // Determine if the source format is a signed integer format, the destFormat will already
    // be GL_XXXX_INTEGER but it does not tell us if it is signed or unsigned.
    D3D11_SHADER_RESOURCE_VIEW_DESC sourceSRVDesc;
    source.get()->GetDesc(&sourceSRVDesc);

    GLenum componentType = d3d11::GetComponentType(sourceSRVDesc.Format);

    ASSERT(componentType != GL_NONE);
    ASSERT(componentType != GL_SIGNED_NORMALIZED);
    bool isSigned = (componentType == GL_INT);

    ShaderDimension dimension =
        (sourceSRVDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE3D) ? SHADER_3D : SHADER_2D;

    const Shader *shader = nullptr;
    ANGLE_TRY(getBlitShader(destFormat, sourceFormat, isSigned, unpackPremultiplyAlpha,
                            unpackUnmultiplyAlpha, dimension, &shader));

    ShaderSupport support;
    ANGLE_TRY(getShaderSupport(*shader, &support));

    // Set vertices
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    result =
        deviceContext->Map(mVertexBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result))
    {
        return gl::OutOfMemory() << "Failed to map internal vertex buffer for texture copy, "
                                 << gl::FmtHR(result);
    }

    UINT stride    = 0;
    UINT drawCount = 0;
    D3D11_PRIMITIVE_TOPOLOGY topology;

    support.vertexWriteFunction(sourceArea, sourceSize, destArea, destSize, mappedResource.pData,
                                &stride, &drawCount, &topology);

    deviceContext->Unmap(mVertexBuffer.get(), 0);

    StateManager11 *stateManager = mRenderer->getStateManager();

    // Apply vertex buffer
    stateManager->setSingleVertexBuffer(&mVertexBuffer, stride, 0);

    // Apply state
    if (maskOffAlpha)
    {
        ANGLE_TRY(mAlphaMaskBlendState.resolve(mRenderer));
        stateManager->setSimpleBlendState(&mAlphaMaskBlendState.getObj());
    }
    else
    {
        stateManager->setSimpleBlendState(nullptr);
    }
    stateManager->setDepthStencilState(nullptr, 0xFFFFFFFF);

    if (scissor)
    {
        stateManager->setSimpleScissorRect(*scissor);
        stateManager->setRasterizerState(&mScissorEnabledRasterizerState);
    }
    else
    {
        stateManager->setRasterizerState(&mScissorDisabledRasterizerState);
    }

    // Apply shaders
    stateManager->setInputLayout(support.inputLayout);
    stateManager->setPrimitiveTopology(topology);

    stateManager->setDrawShaders(support.vertexShader, support.geometryShader,
                                 &shader->pixelShader);

    // Apply render target
    stateManager->setRenderTarget(dest.get(), nullptr);

    // Set the viewport
    stateManager->setSimpleViewport(destSize);

    // Apply texture and sampler
    switch (filter)
    {
        case GL_NEAREST:
            stateManager->setSimplePixelTextureAndSampler(source, mPointSampler);
            break;
        case GL_LINEAR:
            stateManager->setSimplePixelTextureAndSampler(source, mLinearSampler);
            break;

        default:
            UNREACHABLE();
            return gl::InternalError() << "Internal error, unknown blit filter mode.";
    }

    // Draw the quad
    deviceContext->Draw(drawCount, 0);

    return gl::NoError();
}

gl::Error Blit11::copyStencil(const gl::Context *context,
                              const TextureHelper11 &source,
                              unsigned int sourceSubresource,
                              const gl::Box &sourceArea,
                              const gl::Extents &sourceSize,
                              const TextureHelper11 &dest,
                              unsigned int destSubresource,
                              const gl::Box &destArea,
                              const gl::Extents &destSize,
                              const gl::Rectangle *scissor)
{
    return copyDepthStencilImpl(source, sourceSubresource, sourceArea, sourceSize, dest,
                                destSubresource, destArea, destSize, scissor, true);
}

gl::Error Blit11::copyDepth(const gl::Context *context,
                            const d3d11::SharedSRV &source,
                            const gl::Box &sourceArea,
                            const gl::Extents &sourceSize,
                            const d3d11::DepthStencilView &dest,
                            const gl::Box &destArea,
                            const gl::Extents &destSize,
                            const gl::Rectangle *scissor)
{
    ANGLE_TRY(initResources());

    HRESULT result;
    ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();

    // Set vertices
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    result =
        deviceContext->Map(mVertexBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result))
    {
        return gl::OutOfMemory() << "Failed to map internal vertex buffer for texture copy, "
                                 << gl::FmtHR(result);
    }

    UINT stride    = 0;
    UINT drawCount = 0;
    D3D11_PRIMITIVE_TOPOLOGY topology;

    Write2DVertices(sourceArea, sourceSize, destArea, destSize, mappedResource.pData, &stride,
                    &drawCount, &topology);

    deviceContext->Unmap(mVertexBuffer.get(), 0);

    StateManager11 *stateManager = mRenderer->getStateManager();

    // Apply vertex buffer
    stateManager->setSingleVertexBuffer(&mVertexBuffer, stride, 0);

    // Apply state
    stateManager->setSimpleBlendState(nullptr);
    stateManager->setDepthStencilState(&mDepthStencilState, 0xFFFFFFFF);

    if (scissor)
    {
        stateManager->setSimpleScissorRect(*scissor);
        stateManager->setRasterizerState(&mScissorEnabledRasterizerState);
    }
    else
    {
        stateManager->setRasterizerState(&mScissorDisabledRasterizerState);
    }

    ANGLE_TRY(mQuad2DIL.resolve(mRenderer));
    ANGLE_TRY(mQuad2DVS.resolve(mRenderer));
    ANGLE_TRY(mDepthPS.resolve(mRenderer));

    // Apply shaders
    stateManager->setInputLayout(&mQuad2DIL.getObj());
    stateManager->setPrimitiveTopology(topology);

    stateManager->setDrawShaders(&mQuad2DVS.getObj(), nullptr, &mDepthPS.getObj());

    // Apply render target
    stateManager->setRenderTarget(nullptr, dest.get());

    // Set the viewport
    stateManager->setSimpleViewport(destSize);

    // Apply texture and sampler
    stateManager->setSimplePixelTextureAndSampler(source, mPointSampler);

    // Draw the quad
    deviceContext->Draw(drawCount, 0);

    return gl::NoError();
}

gl::Error Blit11::copyDepthStencil(const TextureHelper11 &source,
                                   unsigned int sourceSubresource,
                                   const gl::Box &sourceArea,
                                   const gl::Extents &sourceSize,
                                   const TextureHelper11 &dest,
                                   unsigned int destSubresource,
                                   const gl::Box &destArea,
                                   const gl::Extents &destSize,
                                   const gl::Rectangle *scissor)
{
    return copyDepthStencilImpl(source, sourceSubresource, sourceArea, sourceSize, dest,
                                destSubresource, destArea, destSize, scissor, false);
}

gl::Error Blit11::copyDepthStencilImpl(const TextureHelper11 &source,
                                       unsigned int sourceSubresource,
                                       const gl::Box &sourceArea,
                                       const gl::Extents &sourceSize,
                                       const TextureHelper11 &dest,
                                       unsigned int destSubresource,
                                       const gl::Box &destArea,
                                       const gl::Extents &destSize,
                                       const gl::Rectangle *scissor,
                                       bool stencilOnly)
{
    auto srcDXGIFormat         = source.getFormat();
    const auto &srcSizeInfo    = d3d11::GetDXGIFormatSizeInfo(srcDXGIFormat);
    unsigned int srcPixelSize  = srcSizeInfo.pixelBytes;
    unsigned int copyOffset    = 0;
    unsigned int copySize      = srcPixelSize;
    auto destDXGIFormat        = dest.getFormat();
    const auto &destSizeInfo   = d3d11::GetDXGIFormatSizeInfo(destDXGIFormat);
    unsigned int destPixelSize = destSizeInfo.pixelBytes;

    ASSERT(srcDXGIFormat == destDXGIFormat || destDXGIFormat == DXGI_FORMAT_R32_TYPELESS);

    if (stencilOnly)
    {
        const auto &srcFormat = source.getFormatSet().format();

        // Stencil channel should be right after the depth channel. Some views to depth/stencil
        // resources have red channel for depth, in which case the depth channel bit width is in
        // redBits.
        ASSERT((srcFormat.redBits != 0) != (srcFormat.depthBits != 0));
        GLuint depthBits = srcFormat.redBits + srcFormat.depthBits;
        // Known formats have either 24 or 32 bits of depth.
        ASSERT(depthBits == 24 || depthBits == 32);
        copyOffset = depthBits / 8;

        // Stencil is assumed to be 8-bit - currently this is true for all possible formats.
        copySize = 1;
    }

    if (srcDXGIFormat != destDXGIFormat)
    {
        if (srcDXGIFormat == DXGI_FORMAT_R24G8_TYPELESS)
        {
            ASSERT(sourceArea == destArea && sourceSize == destSize && scissor == nullptr);
            return copyAndConvert(source, sourceSubresource, sourceArea, sourceSize, dest,
                                  destSubresource, destArea, destSize, scissor, copyOffset,
                                  copyOffset, copySize, srcPixelSize, destPixelSize,
                                  BlitD24S8ToD32F);
        }
        ASSERT(srcDXGIFormat == DXGI_FORMAT_R32G8X24_TYPELESS);
        return copyAndConvert(source, sourceSubresource, sourceArea, sourceSize, dest,
                              destSubresource, destArea, destSize, scissor, copyOffset, copyOffset,
                              copySize, srcPixelSize, destPixelSize, BlitD32FS8ToD32F);
    }

    return copyAndConvert(source, sourceSubresource, sourceArea, sourceSize, dest, destSubresource,
                          destArea, destSize, scissor, copyOffset, copyOffset, copySize,
                          srcPixelSize, destPixelSize, StretchedBlitNearest);
}

gl::Error Blit11::copyAndConvertImpl(const TextureHelper11 &source,
                                     unsigned int sourceSubresource,
                                     const gl::Box &sourceArea,
                                     const gl::Extents &sourceSize,
                                     const TextureHelper11 &destStaging,
                                     const gl::Box &destArea,
                                     const gl::Extents &destSize,
                                     const gl::Rectangle *scissor,
                                     size_t readOffset,
                                     size_t writeOffset,
                                     size_t copySize,
                                     size_t srcPixelStride,
                                     size_t destPixelStride,
                                     BlitConvertFunction *convertFunction)
{
    ANGLE_TRY(initResources());

    ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();

    TextureHelper11 sourceStaging;
    ANGLE_TRY_RESULT(mRenderer->createStagingTexture(ResourceType::Texture2D, source.getFormatSet(),
                                                     sourceSize, StagingAccess::READ),
                     sourceStaging);

    deviceContext->CopySubresourceRegion(sourceStaging.get(), 0, 0, 0, 0, source.get(),
                                         sourceSubresource, nullptr);

    D3D11_MAPPED_SUBRESOURCE sourceMapping;
    HRESULT result = deviceContext->Map(sourceStaging.get(), 0, D3D11_MAP_READ, 0, &sourceMapping);
    if (FAILED(result))
    {
        return gl::OutOfMemory()
               << "Failed to map internal source staging texture for depth stencil blit, "
               << gl::FmtHR(result);
    }

    D3D11_MAPPED_SUBRESOURCE destMapping;
    result = deviceContext->Map(destStaging.get(), 0, D3D11_MAP_WRITE, 0, &destMapping);
    if (FAILED(result))
    {
        deviceContext->Unmap(sourceStaging.get(), 0);
        return gl::OutOfMemory()
               << "Failed to map internal destination staging texture for depth stencil blit, "
               << gl::FmtHR(result);
    }

    // Clip dest area to the destination size
    gl::Rectangle clipRect = gl::Rectangle(0, 0, destSize.width, destSize.height);

    // Clip dest area to the scissor
    if (scissor)
    {
        gl::ClipRectangle(clipRect, *scissor, &clipRect);
    }

    convertFunction(sourceArea, destArea, clipRect, sourceSize, sourceMapping.RowPitch,
                    destMapping.RowPitch, readOffset, writeOffset, copySize, srcPixelStride,
                    destPixelStride, static_cast<const uint8_t *>(sourceMapping.pData),
                    static_cast<uint8_t *>(destMapping.pData));

    deviceContext->Unmap(sourceStaging.get(), 0);
    deviceContext->Unmap(destStaging.get(), 0);

    return gl::NoError();
}

gl::Error Blit11::copyAndConvert(const TextureHelper11 &source,
                                 unsigned int sourceSubresource,
                                 const gl::Box &sourceArea,
                                 const gl::Extents &sourceSize,
                                 const TextureHelper11 &dest,
                                 unsigned int destSubresource,
                                 const gl::Box &destArea,
                                 const gl::Extents &destSize,
                                 const gl::Rectangle *scissor,
                                 size_t readOffset,
                                 size_t writeOffset,
                                 size_t copySize,
                                 size_t srcPixelStride,
                                 size_t destPixelStride,
                                 BlitConvertFunction *convertFunction)
{
    ANGLE_TRY(initResources());

    ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();

    // HACK: Create the destination staging buffer as a read/write texture so
    // ID3D11DevicContext::UpdateSubresource can be called
    //       using it's mapped data as a source
    TextureHelper11 destStaging;
    ANGLE_TRY_RESULT(mRenderer->createStagingTexture(ResourceType::Texture2D, dest.getFormatSet(),
                                                     destSize, StagingAccess::READ_WRITE),
                     destStaging);

    deviceContext->CopySubresourceRegion(destStaging.get(), 0, 0, 0, 0, dest.get(), destSubresource,
                                         nullptr);

    ANGLE_TRY(copyAndConvertImpl(source, sourceSubresource, sourceArea, sourceSize, destStaging,
                                 destArea, destSize, scissor, readOffset, writeOffset, copySize,
                                 srcPixelStride, destPixelStride, convertFunction));

    // Work around timeouts/TDRs in older NVIDIA drivers.
    if (mRenderer->getWorkarounds().depthStencilBlitExtraCopy)
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        deviceContext->Map(destStaging.get(), 0, D3D11_MAP_READ, 0, &mapped);
        deviceContext->UpdateSubresource(dest.get(), destSubresource, nullptr, mapped.pData,
                                         mapped.RowPitch, mapped.DepthPitch);
        deviceContext->Unmap(destStaging.get(), 0);
    }
    else
    {
        deviceContext->CopySubresourceRegion(dest.get(), destSubresource, 0, 0, 0,
                                             destStaging.get(), 0, nullptr);
    }

    return gl::NoError();
}

gl::Error Blit11::addBlitShaderToMap(BlitShaderType blitShaderType,
                                     ShaderDimension dimension,
                                     const ShaderData &shaderData,
                                     const char *name)
{
    ASSERT(mBlitShaderMap.find(blitShaderType) == mBlitShaderMap.end());

    d3d11::PixelShader ps;
    ANGLE_TRY(mRenderer->allocateResource(shaderData, &ps));
    ps.setDebugName(name);

    Shader shader;
    shader.dimension   = dimension;
    shader.pixelShader = std::move(ps);

    mBlitShaderMap[blitShaderType] = std::move(shader);
    return gl::NoError();
}

gl::Error Blit11::addSwizzleShaderToMap(SwizzleShaderType swizzleShaderType,
                                        ShaderDimension dimension,
                                        const ShaderData &shaderData,
                                        const char *name)
{
    ASSERT(mSwizzleShaderMap.find(swizzleShaderType) == mSwizzleShaderMap.end());

    d3d11::PixelShader ps;
    ANGLE_TRY(mRenderer->allocateResource(shaderData, &ps));
    ps.setDebugName(name);

    Shader shader;
    shader.dimension   = dimension;
    shader.pixelShader = std::move(ps);

    mSwizzleShaderMap[swizzleShaderType] = std::move(shader);
    return gl::NoError();
}

void Blit11::clearShaderMap()
{
    mBlitShaderMap.clear();
    mSwizzleShaderMap.clear();
}

gl::Error Blit11::getBlitShader(GLenum destFormat,
                                GLenum sourceFormat,
                                bool isSigned,
                                bool unpackPremultiplyAlpha,
                                bool unpackUnmultiplyAlpha,
                                ShaderDimension dimension,
                                const Shader **shader)
{
    BlitShaderType blitShaderType =
        GetBlitShaderType(destFormat, sourceFormat, isSigned, unpackPremultiplyAlpha,
                          unpackUnmultiplyAlpha, dimension);

    if (blitShaderType == BLITSHADER_INVALID)
    {
        return gl::InternalError() << "Internal blit shader type mismatch";
    }

    auto blitShaderIt = mBlitShaderMap.find(blitShaderType);
    if (blitShaderIt != mBlitShaderMap.end())
    {
        *shader = &blitShaderIt->second;
        return gl::NoError();
    }

    ASSERT(dimension == SHADER_2D || mRenderer->isES3Capable());

    switch (blitShaderType)
    {
        case BLITSHADER_2D_RGBAF:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D,
                                         ShaderData(g_PS_PassthroughRGBA2D),
                                         "Blit11 2D RGBA pixel shader"));
            break;
        case BLITSHADER_2D_BGRAF:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D,
                                         ShaderData(g_PS_PassthroughRGBA2D),
                                         "Blit11 2D BGRA pixel shader"));
            break;
        case BLITSHADER_2D_RGBF:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D,
                                         ShaderData(g_PS_PassthroughRGB2D),
                                         "Blit11 2D RGB pixel shader"));
            break;
        case BLITSHADER_2D_RGF:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D,
                                         ShaderData(g_PS_PassthroughRG2D),
                                         "Blit11 2D RG pixel shader"));
            break;
        case BLITSHADER_2D_RF:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D, ShaderData(g_PS_PassthroughR2D),
                                         "Blit11 2D R pixel shader"));
            break;
        case BLITSHADER_2D_ALPHA:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D, ShaderData(g_PS_PassthroughA2D),
                                         "Blit11 2D alpha pixel shader"));
            break;
        case BLITSHADER_2D_LUMA:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D,
                                         ShaderData(g_PS_PassthroughLum2D),
                                         "Blit11 2D lum pixel shader"));
            break;
        case BLITSHADER_2D_LUMAALPHA:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D,
                                         ShaderData(g_PS_PassthroughLumAlpha2D),
                                         "Blit11 2D luminance alpha pixel shader"));
            break;
        case BLITSHADER_2D_RGBAUI:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D,
                                         ShaderData(g_PS_PassthroughRGBA2DUI),
                                         "Blit11 2D RGBA UI pixel shader"));
            break;
        case BLITSHADER_2D_RGBAI:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D,
                                         ShaderData(g_PS_PassthroughRGBA2DI),
                                         "Blit11 2D RGBA I pixel shader"));
            break;
        case BLITSHADER_2D_RGBUI:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D,
                                         ShaderData(g_PS_PassthroughRGB2DUI),
                                         "Blit11 2D RGB UI pixel shader"));
            break;
        case BLITSHADER_2D_RGBI:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D,
                                         ShaderData(g_PS_PassthroughRGB2DI),
                                         "Blit11 2D RGB I pixel shader"));
            break;
        case BLITSHADER_2D_RGUI:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D,
                                         ShaderData(g_PS_PassthroughRG2DUI),
                                         "Blit11 2D RG UI pixel shader"));
            break;
        case BLITSHADER_2D_RGI:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D,
                                         ShaderData(g_PS_PassthroughRG2DI),
                                         "Blit11 2D RG I pixel shader"));
            break;
        case BLITSHADER_2D_RUI:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D,
                                         ShaderData(g_PS_PassthroughR2DUI),
                                         "Blit11 2D R UI pixel shader"));
            break;
        case BLITSHADER_2D_RI:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D,
                                         ShaderData(g_PS_PassthroughR2DI),
                                         "Blit11 2D R I pixel shader"));
            break;
        case BLITSHADER_3D_RGBAF:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_3D,
                                         ShaderData(g_PS_PassthroughRGBA3D),
                                         "Blit11 3D RGBA pixel shader"));
            break;
        case BLITSHADER_3D_RGBAUI:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_3D,
                                         ShaderData(g_PS_PassthroughRGBA3DUI),
                                         "Blit11 3D UI RGBA pixel shader"));
            break;
        case BLITSHADER_3D_RGBAI:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_3D,
                                         ShaderData(g_PS_PassthroughRGBA3DI),
                                         "Blit11 3D I RGBA pixel shader"));
            break;
        case BLITSHADER_3D_BGRAF:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_3D,
                                         ShaderData(g_PS_PassthroughRGBA3D),
                                         "Blit11 3D BGRA pixel shader"));
            break;
        case BLITSHADER_3D_RGBF:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_3D,
                                         ShaderData(g_PS_PassthroughRGB3D),
                                         "Blit11 3D RGB pixel shader"));
            break;
        case BLITSHADER_3D_RGBUI:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_3D,
                                         ShaderData(g_PS_PassthroughRGB3DUI),
                                         "Blit11 3D RGB UI pixel shader"));
            break;
        case BLITSHADER_3D_RGBI:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_3D,
                                         ShaderData(g_PS_PassthroughRGB3DI),
                                         "Blit11 3D RGB I pixel shader"));
            break;
        case BLITSHADER_3D_RGF:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_3D,
                                         ShaderData(g_PS_PassthroughRG3D),
                                         "Blit11 3D RG pixel shader"));
            break;
        case BLITSHADER_3D_RGUI:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_3D,
                                         ShaderData(g_PS_PassthroughRG3DUI),
                                         "Blit11 3D RG UI pixel shader"));
            break;
        case BLITSHADER_3D_RGI:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_3D,
                                         ShaderData(g_PS_PassthroughRG3DI),
                                         "Blit11 3D RG I pixel shader"));
            break;
        case BLITSHADER_3D_RF:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_3D, ShaderData(g_PS_PassthroughR3D),
                                         "Blit11 3D R pixel shader"));
            break;
        case BLITSHADER_3D_RUI:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_3D,
                                         ShaderData(g_PS_PassthroughR3DUI),
                                         "Blit11 3D R UI pixel shader"));
            break;
        case BLITSHADER_3D_RI:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_3D,
                                         ShaderData(g_PS_PassthroughR3DI),
                                         "Blit11 3D R I pixel shader"));
            break;
        case BLITSHADER_3D_ALPHA:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_3D,
                                         ShaderData(g_PS_PassthroughRGBA3D),
                                         "Blit11 3D alpha pixel shader"));
            break;
        case BLITSHADER_3D_LUMA:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_3D,
                                         ShaderData(g_PS_PassthroughLum3D),
                                         "Blit11 3D luminance pixel shader"));
            break;
        case BLITSHADER_3D_LUMAALPHA:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_3D,
                                         ShaderData(g_PS_PassthroughLumAlpha3D),
                                         "Blit11 3D luminance alpha pixel shader"));
            break;

        case BLITSHADER_2D_RGBAF_PREMULTIPLY:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D, ShaderData(g_PS_FtoF_PM_RGBA),
                                         "Blit11 2D RGBA premultiply pixel shader"));
            break;

        case BLITSHADER_2D_RGBAF_UNMULTIPLY:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D, ShaderData(g_PS_FtoF_UM_RGBA),
                                         "Blit11 2D RGBA unmultiply pixel shader"));
            break;

        case BLITSHADER_2D_RGBF_PREMULTIPLY:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D, ShaderData(g_PS_FtoF_PM_RGB),
                                         "Blit11 2D RGB premultiply pixel shader"));
            break;

        case BLITSHADER_2D_RGBF_UNMULTIPLY:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D, ShaderData(g_PS_FtoF_UM_RGB),
                                         "Blit11 2D RGB unmultiply pixel shader"));
            break;

        case BLITSHADER_2D_RGBAF_TOUI:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D, ShaderData(g_PS_FtoU_PT_RGBA),
                                         "Blit11 2D RGBA to uint pixel shader"));
            break;

        case BLITSHADER_2D_RGBAF_TOUI_PREMULTIPLY:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D, ShaderData(g_PS_FtoU_PM_RGBA),
                                         "Blit11 2D RGBA to uint premultiply pixel shader"));
            break;

        case BLITSHADER_2D_RGBAF_TOUI_UNMULTIPLY:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D, ShaderData(g_PS_FtoU_UM_RGBA),
                                         "Blit11 2D RGBA to uint unmultiply pixel shader"));
            break;

        case BLITSHADER_2D_RGBF_TOUI:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D, ShaderData(g_PS_FtoU_PT_RGB),
                                         "Blit11 2D RGB to uint pixel shader"));
            break;

        case BLITSHADER_2D_RGBF_TOUI_PREMULTIPLY:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D, ShaderData(g_PS_FtoU_PM_RGB),
                                         "Blit11 2D RGB to uint premultiply pixel shader"));
            break;

        case BLITSHADER_2D_RGBF_TOUI_UNMULTIPLY:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D, ShaderData(g_PS_FtoU_UM_RGB),
                                         "Blit11 2D RGB to uint unmultiply pixel shader"));
            break;
        case BLITSHADER_2D_LUMAF_PREMULTIPLY:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D, ShaderData(g_PS_FtoF_PM_LUMA),
                                         "Blit11 2D LUMA premultiply pixel shader"));
            break;
        case BLITSHADER_2D_LUMAF_UNMULTIPLY:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D, ShaderData(g_PS_FtoF_UM_LUMA),
                                         "Blit11 2D LUMA unmultiply pixel shader"));
            break;
        case BLITSHADER_2D_LUMAALPHAF_PREMULTIPLY:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D,
                                         ShaderData(g_PS_FtoF_PM_LUMAALPHA),
                                         "Blit11 2D LUMAALPHA premultiply pixel shader"));
            break;
        case BLITSHADER_2D_LUMAALPHAF_UNMULTIPLY:
            ANGLE_TRY(addBlitShaderToMap(blitShaderType, SHADER_2D,
                                         ShaderData(g_PS_FtoF_UM_LUMAALPHA),
                                         "Blit11 2D LUMAALPHA unmultiply pixel shader"));
            break;

        default:
            UNREACHABLE();
            return gl::InternalError() << "Internal error";
    }

    blitShaderIt = mBlitShaderMap.find(blitShaderType);
    ASSERT(blitShaderIt != mBlitShaderMap.end());
    *shader = &blitShaderIt->second;
    return gl::NoError();
}

gl::Error Blit11::getSwizzleShader(GLenum type,
                                   D3D11_SRV_DIMENSION viewDimension,
                                   const Shader **shader)
{
    SwizzleShaderType swizzleShaderType = GetSwizzleShaderType(type, viewDimension);

    if (swizzleShaderType == SWIZZLESHADER_INVALID)
    {
        return gl::InternalError() << "Swizzle shader type not found";
    }

    auto swizzleShaderIt = mSwizzleShaderMap.find(swizzleShaderType);
    if (swizzleShaderIt != mSwizzleShaderMap.end())
    {
        *shader = &swizzleShaderIt->second;
        return gl::NoError();
    }

    // Swizzling shaders (OpenGL ES 3+)
    ASSERT(mRenderer->isES3Capable());

    switch (swizzleShaderType)
    {
        case SWIZZLESHADER_2D_FLOAT:
            ANGLE_TRY(addSwizzleShaderToMap(swizzleShaderType, SHADER_2D,
                                            ShaderData(g_PS_SwizzleF2D),
                                            "Blit11 2D F swizzle pixel shader"));
            break;
        case SWIZZLESHADER_2D_UINT:
            ANGLE_TRY(addSwizzleShaderToMap(swizzleShaderType, SHADER_2D,
                                            ShaderData(g_PS_SwizzleUI2D),
                                            "Blit11 2D UI swizzle pixel shader"));
            break;
        case SWIZZLESHADER_2D_INT:
            ANGLE_TRY(addSwizzleShaderToMap(swizzleShaderType, SHADER_2D,
                                            ShaderData(g_PS_SwizzleI2D),
                                            "Blit11 2D I swizzle pixel shader"));
            break;
        case SWIZZLESHADER_CUBE_FLOAT:
            ANGLE_TRY(addSwizzleShaderToMap(swizzleShaderType, SHADER_3D,
                                            ShaderData(g_PS_SwizzleF2DArray),
                                            "Blit11 2D Cube F swizzle pixel shader"));
            break;
        case SWIZZLESHADER_CUBE_UINT:
            ANGLE_TRY(addSwizzleShaderToMap(swizzleShaderType, SHADER_3D,
                                            ShaderData(g_PS_SwizzleUI2DArray),
                                            "Blit11 2D Cube UI swizzle pixel shader"));
            break;
        case SWIZZLESHADER_CUBE_INT:
            ANGLE_TRY(addSwizzleShaderToMap(swizzleShaderType, SHADER_3D,
                                            ShaderData(g_PS_SwizzleI2DArray),
                                            "Blit11 2D Cube I swizzle pixel shader"));
            break;
        case SWIZZLESHADER_3D_FLOAT:
            ANGLE_TRY(addSwizzleShaderToMap(swizzleShaderType, SHADER_3D,
                                            ShaderData(g_PS_SwizzleF3D),
                                            "Blit11 3D F swizzle pixel shader"));
            break;
        case SWIZZLESHADER_3D_UINT:
            ANGLE_TRY(addSwizzleShaderToMap(swizzleShaderType, SHADER_3D,
                                            ShaderData(g_PS_SwizzleUI3D),
                                            "Blit11 3D UI swizzle pixel shader"));
            break;
        case SWIZZLESHADER_3D_INT:
            ANGLE_TRY(addSwizzleShaderToMap(swizzleShaderType, SHADER_3D,
                                            ShaderData(g_PS_SwizzleI3D),
                                            "Blit11 3D I swizzle pixel shader"));
            break;
        case SWIZZLESHADER_ARRAY_FLOAT:
            ANGLE_TRY(addSwizzleShaderToMap(swizzleShaderType, SHADER_3D,
                                            ShaderData(g_PS_SwizzleF2DArray),
                                            "Blit11 2D Array F swizzle pixel shader"));
            break;
        case SWIZZLESHADER_ARRAY_UINT:
            ANGLE_TRY(addSwizzleShaderToMap(swizzleShaderType, SHADER_3D,
                                            ShaderData(g_PS_SwizzleUI2DArray),
                                            "Blit11 2D Array UI swizzle pixel shader"));
            break;
        case SWIZZLESHADER_ARRAY_INT:
            ANGLE_TRY(addSwizzleShaderToMap(swizzleShaderType, SHADER_3D,
                                            ShaderData(g_PS_SwizzleI2DArray),
                                            "Blit11 2D Array I swizzle pixel shader"));
            break;
        default:
            UNREACHABLE();
            return gl::InternalError() << "Internal error";
    }

    swizzleShaderIt = mSwizzleShaderMap.find(swizzleShaderType);
    ASSERT(swizzleShaderIt != mSwizzleShaderMap.end());
    *shader = &swizzleShaderIt->second;
    return gl::NoError();
}

gl::ErrorOrResult<TextureHelper11> Blit11::resolveDepth(const gl::Context *context,
                                                        RenderTarget11 *depth)
{
    ANGLE_TRY(initResources());

    // Multisampled depth stencil SRVs are not available in feature level 10.0
    ASSERT(mRenderer->getRenderer11DeviceCaps().featureLevel > D3D_FEATURE_LEVEL_10_0);

    const auto &extents          = depth->getExtents();
    auto *deviceContext          = mRenderer->getDeviceContext();
    auto *stateManager           = mRenderer->getStateManager();

    ANGLE_TRY(initResolveDepthOnly(depth->getFormatSet(), extents));

    ANGLE_TRY(mResolveDepthStencilVS.resolve(mRenderer));
    ANGLE_TRY(mResolveDepthPS.resolve(mRenderer));

    // Apply the necessary state changes to the D3D11 immediate device context.
    stateManager->setInputLayout(nullptr);
    stateManager->setPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    stateManager->setDrawShaders(&mResolveDepthStencilVS.getObj(), nullptr,
                                 &mResolveDepthPS.getObj());
    stateManager->setRasterizerState(nullptr);
    stateManager->setDepthStencilState(&mDepthStencilState, 0xFFFFFFFF);
    stateManager->setRenderTargets(nullptr, 0, mResolvedDepthDSView.get());
    stateManager->setSimpleBlendState(nullptr);
    stateManager->setSimpleViewport(extents);

    // Set the viewport
    stateManager->setShaderResourceShared(gl::SAMPLER_PIXEL, 0, &depth->getShaderResourceView());

    // Trigger the blit on the GPU.
    deviceContext->Draw(6, 0);

    return mResolvedDepth;
}

gl::Error Blit11::initResolveDepthOnly(const d3d11::Format &format, const gl::Extents &extents)
{
    if (mResolvedDepth.valid() && extents == mResolvedDepth.getExtents() &&
        format.texFormat == mResolvedDepth.getFormat())
    {
        return gl::NoError();
    }

    D3D11_TEXTURE2D_DESC textureDesc;
    textureDesc.Width              = extents.width;
    textureDesc.Height             = extents.height;
    textureDesc.MipLevels          = 1;
    textureDesc.ArraySize          = 1;
    textureDesc.Format             = format.texFormat;
    textureDesc.SampleDesc.Count   = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage              = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags     = 0;
    textureDesc.MiscFlags          = 0;

    ANGLE_TRY(mRenderer->allocateTexture(textureDesc, format, &mResolvedDepth));
    mResolvedDepth.setDebugName("Blit11::mResolvedDepth");

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Flags              = 0;
    dsvDesc.Format             = format.dsvFormat;
    dsvDesc.Texture2D.MipSlice = 0;
    dsvDesc.ViewDimension      = D3D11_DSV_DIMENSION_TEXTURE2D;

    ANGLE_TRY(mRenderer->allocateResource(dsvDesc, mResolvedDepth.get(), &mResolvedDepthDSView));
    mResolvedDepthDSView.setDebugName("Blit11::mResolvedDepthDSView");

    // Possibly D3D11 bug or undefined behaviour: Clear the DSV so that our first render
    // works as expected. Otherwise the results of the first use seem to be incorrect.
    ID3D11DeviceContext *context = mRenderer->getDeviceContext();
    context->ClearDepthStencilView(mResolvedDepthDSView.get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    return gl::NoError();
}

gl::Error Blit11::initResolveDepthStencil(const gl::Extents &extents)
{
    // Check if we need to recreate depth stencil view
    if (mResolvedDepthStencil.valid() && extents == mResolvedDepthStencil.getExtents())
    {
        ASSERT(mResolvedDepthStencil.getFormat() == DXGI_FORMAT_R32G32_FLOAT);
        return gl::NoError();
    }

    if (mResolvedDepthStencil.valid())
    {
        releaseResolveDepthStencilResources();
    }

    const auto &formatSet = d3d11::Format::Get(GL_RG32F, mRenderer->getRenderer11DeviceCaps());

    D3D11_TEXTURE2D_DESC textureDesc;
    textureDesc.Width              = extents.width;
    textureDesc.Height             = extents.height;
    textureDesc.MipLevels          = 1;
    textureDesc.ArraySize          = 1;
    textureDesc.Format             = formatSet.texFormat;
    textureDesc.SampleDesc.Count   = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage              = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags          = D3D11_BIND_RENDER_TARGET;
    textureDesc.CPUAccessFlags     = 0;
    textureDesc.MiscFlags          = 0;

    ANGLE_TRY(mRenderer->allocateTexture(textureDesc, formatSet, &mResolvedDepthStencil));
    mResolvedDepthStencil.setDebugName("Blit11::mResolvedDepthStencil");

    ANGLE_TRY(mRenderer->allocateResourceNoDesc(mResolvedDepthStencil.get(),
                                                &mResolvedDepthStencilRTView));
    mResolvedDepthStencilRTView.setDebugName("Blit11::mResolvedDepthStencilRTView");

    return gl::NoError();
}

gl::ErrorOrResult<TextureHelper11> Blit11::resolveStencil(const gl::Context *context,
                                                          RenderTarget11 *depthStencil,
                                                          bool alsoDepth)
{
    ANGLE_TRY(initResources());

    // Multisampled depth stencil SRVs are not available in feature level 10.0
    ASSERT(mRenderer->getRenderer11DeviceCaps().featureLevel > D3D_FEATURE_LEVEL_10_0);

    const auto &extents = depthStencil->getExtents();

    ANGLE_TRY(initResolveDepthStencil(extents));

    ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();
    auto *stateManager              = mRenderer->getStateManager();
    ID3D11Resource *stencilResource = depthStencil->getTexture().get();

    // Check if we need to re-create the stencil SRV.
    if (mStencilSRV.valid())
    {
        ID3D11Resource *priorResource = nullptr;
        mStencilSRV.get()->GetResource(&priorResource);

        if (stencilResource != priorResource)
        {
            mStencilSRV.reset();
        }

        SafeRelease(priorResource);
    }

    if (!mStencilSRV.valid())
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srViewDesc;
        srViewDesc.Format        = GetStencilSRVFormat(depthStencil->getFormatSet());
        srViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;

        ANGLE_TRY(mRenderer->allocateResource(srViewDesc, stencilResource, &mStencilSRV));
        mStencilSRV.setDebugName("Blit11::mStencilSRV");
    }

    // Notify the Renderer that all state should be invalidated.
    ANGLE_TRY(mResolveDepthStencilVS.resolve(mRenderer));

    // Resolving the depth buffer works by sampling the depth in the shader using a SRV, then
    // writing to the resolved depth buffer using SV_Depth. We can't use this method for stencil
    // because SV_StencilRef isn't supported until HLSL 5.1/D3D11.3.
    const d3d11::PixelShader *pixelShader = nullptr;
    if (alsoDepth)
    {
        ANGLE_TRY(mResolveDepthStencilPS.resolve(mRenderer));
        pixelShader = &mResolveDepthStencilPS.getObj();
    }
    else
    {
        ANGLE_TRY(mResolveStencilPS.resolve(mRenderer));
        pixelShader = &mResolveStencilPS.getObj();
    }

    // Apply the necessary state changes to the D3D11 immediate device context.
    stateManager->setInputLayout(nullptr);
    stateManager->setPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    stateManager->setDrawShaders(&mResolveDepthStencilVS.getObj(), nullptr, pixelShader);
    stateManager->setRasterizerState(nullptr);
    stateManager->setDepthStencilState(nullptr, 0xFFFFFFFF);
    stateManager->setRenderTarget(mResolvedDepthStencilRTView.get(), nullptr);
    stateManager->setSimpleBlendState(nullptr);

    // Set the viewport
    stateManager->setSimpleViewport(extents);
    stateManager->setShaderResourceShared(gl::SAMPLER_PIXEL, 0,
                                          &depthStencil->getShaderResourceView());
    stateManager->setShaderResource(gl::SAMPLER_PIXEL, 1, &mStencilSRV);

    // Trigger the blit on the GPU.
    deviceContext->Draw(6, 0);

    gl::Box copyBox(0, 0, 0, extents.width, extents.height, 1);

    TextureHelper11 dest;
    ANGLE_TRY_RESULT(
        mRenderer->createStagingTexture(ResourceType::Texture2D, depthStencil->getFormatSet(),
                                        extents, StagingAccess::READ_WRITE),
        dest);

    const auto &copyFunction = GetCopyDepthStencilFunction(depthStencil->getInternalFormat());
    const auto &dsFormatSet  = depthStencil->getFormatSet();
    const auto &dsDxgiInfo   = d3d11::GetDXGIFormatSizeInfo(dsFormatSet.texFormat);

    ANGLE_TRY(copyAndConvertImpl(mResolvedDepthStencil, 0, copyBox, extents, dest, copyBox, extents,
                                 nullptr, 0, 0, 0, 8u, dsDxgiInfo.pixelBytes, copyFunction));

    // Return the resolved depth texture, which the caller must Release.
    return dest;
}

void Blit11::releaseResolveDepthStencilResources()
{
    mStencilSRV.reset();
    mResolvedDepthStencilRTView.reset();
}

}  // namespace rx
