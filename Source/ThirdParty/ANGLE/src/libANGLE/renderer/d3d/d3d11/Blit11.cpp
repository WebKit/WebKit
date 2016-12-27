//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Blit11.cpp: Texture copy utility class.

#include "libANGLE/renderer/d3d/d3d11/Blit11.h"

#include <float.h>

#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/RenderTarget11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"
#include "libANGLE/renderer/d3d/d3d11/formatutils11.h"
#include "libANGLE/renderer/d3d/d3d11/texture_format_table.h"
#include "third_party/trace_event/trace_event.h"

namespace rx
{

namespace
{

// Include inline shaders in the anonymous namespace to make sure no symbols are exported
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthrough2d11vs.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughdepth2d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgba2d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgbapremultiply2d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgbaunmultiply2d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgba2dui11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgba2di11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgb2d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgbpremultiply2d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgbunmultiply2d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgb2dui11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgb2di11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrg2d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrg2dui11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrg2di11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughr2d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughr2dui11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughr2di11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughlum2d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughlumalpha2d11ps.h"

#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthrough3d11vs.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthrough3d11gs.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgba3d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgba3dui11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgba3di11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgb3d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgb3dui11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgb3di11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrg3d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrg3dui11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrg3di11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughr3d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughr3dui11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughr3di11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughlum3d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughlumalpha3d11ps.h"

#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/resolvedepth11_ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/resolvedepthstencil11_ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/resolvedepthstencil11_vs.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/resolvestencil11_ps.h"

#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzlef2dps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzlei2dps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzleui2dps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzlef3dps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzlei3dps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzleui3dps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzlef2darrayps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzlei2darrayps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzleui2darrayps.h"

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

Blit11::Blit11(Renderer11 *renderer)
    : mRenderer(renderer),
      mResourcesInitialized(false),
      mVertexBuffer(nullptr),
      mPointSampler(nullptr),
      mLinearSampler(nullptr),
      mScissorEnabledRasterizerState(nullptr),
      mScissorDisabledRasterizerState(nullptr),
      mDepthStencilState(nullptr),
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
      mSwizzleCB(nullptr),
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
      mStencilSRV(nullptr),
      mResolvedDepthStencilRTView(nullptr)
{
}

Blit11::~Blit11()
{
    freeResources();

    mQuad2DIL.release();
    mQuad2DVS.release();
    mDepthPS.release();

    mQuad3DIL.release();
    mQuad3DVS.release();
    mQuad3DGS.release();

    clearShaderMap();
    releaseResolveDepthStencilResources();
}

gl::Error Blit11::initResources()
{
    if (mResourcesInitialized)
    {
        return gl::Error(GL_NO_ERROR);
    }

    TRACE_EVENT0("gpu.angle", "Blit11::initResources");

    HRESULT result;
    ID3D11Device *device = mRenderer->getDevice();

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

    result = device->CreateBuffer(&vbDesc, nullptr, &mVertexBuffer);
    ASSERT(SUCCEEDED(result));
    if (FAILED(result))
    {
        freeResources();
        return gl::Error(GL_OUT_OF_MEMORY, "Failed to create blit vertex buffer, HRESULT: 0x%X",
                         result);
    }
    d3d11::SetDebugName(mVertexBuffer, "Blit11 vertex buffer");

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

    result = device->CreateSamplerState(&pointSamplerDesc, &mPointSampler);
    ASSERT(SUCCEEDED(result));
    if (FAILED(result))
    {
        freeResources();
        return gl::Error(GL_OUT_OF_MEMORY,
                         "Failed to create blit point sampler state, HRESULT: 0x%X", result);
    }
    d3d11::SetDebugName(mPointSampler, "Blit11 point sampler");

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

    result = device->CreateSamplerState(&linearSamplerDesc, &mLinearSampler);
    ASSERT(SUCCEEDED(result));
    if (FAILED(result))
    {
        freeResources();
        return gl::Error(GL_OUT_OF_MEMORY,
                         "Failed to create blit linear sampler state, HRESULT: 0x%X", result);
    }
    d3d11::SetDebugName(mLinearSampler, "Blit11 linear sampler");

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
    result = device->CreateRasterizerState(&rasterDesc, &mScissorEnabledRasterizerState);
    ASSERT(SUCCEEDED(result));
    if (FAILED(result))
    {
        freeResources();
        return gl::Error(GL_OUT_OF_MEMORY,
                         "Failed to create blit scissoring rasterizer state, HRESULT: 0x%X",
                         result);
    }
    d3d11::SetDebugName(mScissorEnabledRasterizerState, "Blit11 scissoring rasterizer state");

    rasterDesc.ScissorEnable = FALSE;
    result = device->CreateRasterizerState(&rasterDesc, &mScissorDisabledRasterizerState);
    ASSERT(SUCCEEDED(result));
    if (FAILED(result))
    {
        freeResources();
        return gl::Error(GL_OUT_OF_MEMORY,
                         "Failed to create blit no scissoring rasterizer state, HRESULT: 0x%X",
                         result);
    }
    d3d11::SetDebugName(mScissorDisabledRasterizerState, "Blit11 no scissoring rasterizer state");

    D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
    depthStencilDesc.DepthEnable                  = true;
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

    result = device->CreateDepthStencilState(&depthStencilDesc, &mDepthStencilState);
    ASSERT(SUCCEEDED(result));
    if (FAILED(result))
    {
        freeResources();
        return gl::Error(GL_OUT_OF_MEMORY,
                         "Failed to create blit depth stencil state, HRESULT: 0x%X", result);
    }
    d3d11::SetDebugName(mDepthStencilState, "Blit11 depth stencil state");

    D3D11_BUFFER_DESC swizzleBufferDesc;
    swizzleBufferDesc.ByteWidth           = sizeof(unsigned int) * 4;
    swizzleBufferDesc.Usage               = D3D11_USAGE_DYNAMIC;
    swizzleBufferDesc.BindFlags           = D3D11_BIND_CONSTANT_BUFFER;
    swizzleBufferDesc.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
    swizzleBufferDesc.MiscFlags           = 0;
    swizzleBufferDesc.StructureByteStride = 0;

    result = device->CreateBuffer(&swizzleBufferDesc, nullptr, &mSwizzleCB);
    ASSERT(SUCCEEDED(result));
    if (FAILED(result))
    {
        freeResources();
        return gl::Error(GL_OUT_OF_MEMORY, "Failed to create blit swizzle buffer, HRESULT: 0x%X",
                         result);
    }
    d3d11::SetDebugName(mSwizzleCB, "Blit11 swizzle constant buffer");

    mResourcesInitialized = true;

    return gl::Error(GL_NO_ERROR);
}

void Blit11::freeResources()
{
    SafeRelease(mVertexBuffer);
    SafeRelease(mPointSampler);
    SafeRelease(mLinearSampler);
    SafeRelease(mScissorEnabledRasterizerState);
    SafeRelease(mScissorDisabledRasterizerState);
    SafeRelease(mDepthStencilState);
    SafeRelease(mSwizzleCB);

    mResourcesInitialized = false;
}

// static
Blit11::BlitShaderType Blit11::GetBlitShaderType(GLenum destinationFormat,
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
        if (unpackPremultiplyAlpha != unpackUnmultiplyAlpha)
        {
            switch (destinationFormat)
            {
                case GL_RGBA:
                    return unpackPremultiplyAlpha ? BLITSHADER_2D_RGBAF_PREMULTIPLY
                                                  : BLITSHADER_2D_RGBAF_UNMULTIPLY;
                case GL_BGRA_EXT:
                    return unpackPremultiplyAlpha ? BLITSHADER_2D_BGRAF_PREMULTIPLY
                                                  : BLITSHADER_2D_BGRAF_UNMULTIPLY;
                case GL_RGB:
                    return unpackPremultiplyAlpha ? BLITSHADER_2D_RGBF_PREMULTIPLY
                                                  : BLITSHADER_2D_RGBF_UNMULTIPLY;
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

Blit11::ShaderSupport Blit11::getShaderSupport(const Shader &shader)
{
    ID3D11Device *device = mRenderer->getDevice();
    ShaderSupport support;

    if (shader.dimension == SHADER_2D)
    {
        support.inputLayout         = mQuad2DIL.resolve(device);
        support.vertexShader        = mQuad2DVS.resolve(device);
        support.geometryShader      = nullptr;
        support.vertexWriteFunction = Write2DVertices;
    }
    else
    {
        ASSERT(shader.dimension == SHADER_3D);
        support.inputLayout         = mQuad3DIL.resolve(device);
        support.vertexShader        = mQuad3DVS.resolve(device);
        support.geometryShader      = mQuad3DGS.resolve(device);
        support.vertexWriteFunction = Write3DVertices;
    }

    return support;
}

gl::Error Blit11::swizzleTexture(ID3D11ShaderResourceView *source,
                                 ID3D11RenderTargetView *dest,
                                 const gl::Extents &size,
                                 const gl::SwizzleState &swizzleTarget)
{
    ANGLE_TRY(initResources());

    HRESULT result;
    ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();

    D3D11_SHADER_RESOURCE_VIEW_DESC sourceSRVDesc;
    source->GetDesc(&sourceSRVDesc);

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
    result = deviceContext->Map(mVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result))
    {
        return gl::Error(GL_OUT_OF_MEMORY,
                         "Failed to map internal vertex buffer for swizzle, HRESULT: 0x%X.",
                         result);
    }

    const ShaderSupport &support = getShaderSupport(*shader);

    UINT stride    = 0;
    UINT startIdx  = 0;
    UINT drawCount = 0;
    D3D11_PRIMITIVE_TOPOLOGY topology;

    gl::Box area(0, 0, 0, size.width, size.height, size.depth);
    support.vertexWriteFunction(area, size, area, size, mappedResource.pData, &stride, &drawCount,
                                &topology);

    deviceContext->Unmap(mVertexBuffer, 0);

    // Set constant buffer
    result = deviceContext->Map(mSwizzleCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result))
    {
        return gl::Error(GL_OUT_OF_MEMORY,
                         "Failed to map internal constant buffer for swizzle, HRESULT: 0x%X.",
                         result);
    }

    unsigned int *swizzleIndices = reinterpret_cast<unsigned int *>(mappedResource.pData);
    swizzleIndices[0]            = GetSwizzleIndex(swizzleTarget.swizzleRed);
    swizzleIndices[1]            = GetSwizzleIndex(swizzleTarget.swizzleGreen);
    swizzleIndices[2]            = GetSwizzleIndex(swizzleTarget.swizzleBlue);
    swizzleIndices[3]            = GetSwizzleIndex(swizzleTarget.swizzleAlpha);

    deviceContext->Unmap(mSwizzleCB, 0);

    // Apply vertex buffer
    deviceContext->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &startIdx);

    // Apply constant buffer
    deviceContext->PSSetConstantBuffers(0, 1, &mSwizzleCB);

    // Apply state
    deviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFF);
    deviceContext->OMSetDepthStencilState(nullptr, 0xFFFFFFFF);
    deviceContext->RSSetState(mScissorDisabledRasterizerState);

    // Apply shaders
    deviceContext->IASetInputLayout(support.inputLayout);
    deviceContext->IASetPrimitiveTopology(topology);
    deviceContext->VSSetShader(support.vertexShader, nullptr, 0);

    deviceContext->PSSetShader(shader->pixelShader, nullptr, 0);
    deviceContext->GSSetShader(support.geometryShader, nullptr, 0);

    // Unset the currently bound shader resource to avoid conflicts
    auto stateManager = mRenderer->getStateManager();
    stateManager->setShaderResource(gl::SAMPLER_PIXEL, 0, nullptr);

    // Apply render target
    stateManager->setOneTimeRenderTarget(dest, nullptr);

    // Set the viewport
    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width    = static_cast<FLOAT>(size.width);
    viewport.Height   = static_cast<FLOAT>(size.height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    deviceContext->RSSetViewports(1, &viewport);

    // Apply textures
    stateManager->setShaderResource(gl::SAMPLER_PIXEL, 0, source);

    // Apply samplers
    deviceContext->PSSetSamplers(0, 1, &mPointSampler);

    // Draw the quad
    deviceContext->Draw(drawCount, 0);

    // Unbind textures and render targets and vertex buffer
    stateManager->setShaderResource(gl::SAMPLER_PIXEL, 0, nullptr);

    UINT zero                      = 0;
    ID3D11Buffer *const nullBuffer = nullptr;
    deviceContext->IASetVertexBuffers(0, 1, &nullBuffer, &zero, &zero);

    mRenderer->markAllStateDirty();

    return gl::Error(GL_NO_ERROR);
}

gl::Error Blit11::copyTexture(ID3D11ShaderResourceView *source,
                              const gl::Box &sourceArea,
                              const gl::Extents &sourceSize,
                              ID3D11RenderTargetView *dest,
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
    source->GetDesc(&sourceSRVDesc);

    GLenum componentType = d3d11::GetComponentType(sourceSRVDesc.Format);

    ASSERT(componentType != GL_NONE);
    ASSERT(componentType != GL_SIGNED_NORMALIZED);
    bool isSigned = (componentType == GL_INT);

    ShaderDimension dimension =
        (sourceSRVDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE3D) ? SHADER_3D : SHADER_2D;

    const Shader *shader = nullptr;
    ANGLE_TRY(getBlitShader(destFormat, isSigned, unpackPremultiplyAlpha, unpackUnmultiplyAlpha,
                            dimension, &shader));

    const ShaderSupport &support = getShaderSupport(*shader);

    // Set vertices
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    result = deviceContext->Map(mVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result))
    {
        return gl::Error(GL_OUT_OF_MEMORY,
                         "Failed to map internal vertex buffer for texture copy, HRESULT: 0x%X.",
                         result);
    }

    UINT stride    = 0;
    UINT startIdx  = 0;
    UINT drawCount = 0;
    D3D11_PRIMITIVE_TOPOLOGY topology;

    support.vertexWriteFunction(sourceArea, sourceSize, destArea, destSize, mappedResource.pData,
                                &stride, &drawCount, &topology);

    deviceContext->Unmap(mVertexBuffer, 0);

    // Apply vertex buffer
    deviceContext->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &startIdx);

    // Apply state
    if (maskOffAlpha)
    {
        ID3D11BlendState *blendState = mAlphaMaskBlendState.resolve(mRenderer->getDevice());
        ASSERT(blendState);
        deviceContext->OMSetBlendState(blendState, nullptr, 0xFFFFFFF);
    }
    else
    {
        deviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFF);
    }
    deviceContext->OMSetDepthStencilState(nullptr, 0xFFFFFFFF);

    if (scissor)
    {
        D3D11_RECT scissorRect;
        scissorRect.left   = scissor->x;
        scissorRect.right  = scissor->x + scissor->width;
        scissorRect.top    = scissor->y;
        scissorRect.bottom = scissor->y + scissor->height;

        deviceContext->RSSetScissorRects(1, &scissorRect);
        deviceContext->RSSetState(mScissorEnabledRasterizerState);
    }
    else
    {
        deviceContext->RSSetState(mScissorDisabledRasterizerState);
    }

    // Apply shaders
    deviceContext->IASetInputLayout(support.inputLayout);
    deviceContext->IASetPrimitiveTopology(topology);
    deviceContext->VSSetShader(support.vertexShader, nullptr, 0);

    deviceContext->PSSetShader(shader->pixelShader, nullptr, 0);
    deviceContext->GSSetShader(support.geometryShader, nullptr, 0);

    // Unset the currently bound shader resource to avoid conflicts
    auto stateManager = mRenderer->getStateManager();
    stateManager->setShaderResource(gl::SAMPLER_PIXEL, 0, nullptr);

    // Apply render target
    stateManager->setOneTimeRenderTarget(dest, nullptr);

    // Set the viewport
    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width    = static_cast<FLOAT>(destSize.width);
    viewport.Height   = static_cast<FLOAT>(destSize.height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    deviceContext->RSSetViewports(1, &viewport);

    // Apply textures
    stateManager->setShaderResource(gl::SAMPLER_PIXEL, 0, source);

    // Apply samplers
    ID3D11SamplerState *sampler = nullptr;
    switch (filter)
    {
        case GL_NEAREST:
            sampler = mPointSampler;
            break;
        case GL_LINEAR:
            sampler = mLinearSampler;
            break;

        default:
            UNREACHABLE();
            return gl::Error(GL_OUT_OF_MEMORY, "Internal error, unknown blit filter mode.");
    }
    deviceContext->PSSetSamplers(0, 1, &sampler);

    // Draw the quad
    deviceContext->Draw(drawCount, 0);

    // Unbind textures and render targets and vertex buffer
    stateManager->setShaderResource(gl::SAMPLER_PIXEL, 0, nullptr);

    UINT zero                      = 0;
    ID3D11Buffer *const nullBuffer = nullptr;
    deviceContext->IASetVertexBuffers(0, 1, &nullBuffer, &zero, &zero);

    mRenderer->markAllStateDirty();

    return gl::NoError();
}

gl::Error Blit11::copyStencil(const TextureHelper11 &source,
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

gl::Error Blit11::copyDepth(ID3D11ShaderResourceView *source,
                            const gl::Box &sourceArea,
                            const gl::Extents &sourceSize,
                            ID3D11DepthStencilView *dest,
                            const gl::Box &destArea,
                            const gl::Extents &destSize,
                            const gl::Rectangle *scissor)
{
    ANGLE_TRY(initResources());

    HRESULT result;
    ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();

    // Set vertices
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    result = deviceContext->Map(mVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result))
    {
        return gl::Error(GL_OUT_OF_MEMORY,
                         "Failed to map internal vertex buffer for texture copy, HRESULT: 0x%X.",
                         result);
    }

    UINT stride    = 0;
    UINT startIdx  = 0;
    UINT drawCount = 0;
    D3D11_PRIMITIVE_TOPOLOGY topology;

    Write2DVertices(sourceArea, sourceSize, destArea, destSize, mappedResource.pData, &stride,
                    &drawCount, &topology);

    deviceContext->Unmap(mVertexBuffer, 0);

    // Apply vertex buffer
    deviceContext->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &startIdx);

    // Apply state
    deviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFF);
    deviceContext->OMSetDepthStencilState(mDepthStencilState, 0xFFFFFFFF);

    if (scissor)
    {
        D3D11_RECT scissorRect;
        scissorRect.left   = scissor->x;
        scissorRect.right  = scissor->x + scissor->width;
        scissorRect.top    = scissor->y;
        scissorRect.bottom = scissor->y + scissor->height;

        deviceContext->RSSetScissorRects(1, &scissorRect);
        deviceContext->RSSetState(mScissorEnabledRasterizerState);
    }
    else
    {
        deviceContext->RSSetState(mScissorDisabledRasterizerState);
    }

    ID3D11Device *device         = mRenderer->getDevice();
    ID3D11VertexShader *quad2DVS = mQuad2DVS.resolve(device);
    if (quad2DVS == nullptr)
    {
        return gl::Error(GL_INVALID_OPERATION, "Error compiling internal 2D blit vertex shader");
    }

    // Apply shaders
    deviceContext->IASetInputLayout(mQuad2DIL.resolve(device));
    deviceContext->IASetPrimitiveTopology(topology);
    deviceContext->VSSetShader(quad2DVS, nullptr, 0);

    deviceContext->PSSetShader(mDepthPS.resolve(device), nullptr, 0);
    deviceContext->GSSetShader(nullptr, nullptr, 0);

    // Unset the currently bound shader resource to avoid conflicts
    auto stateManager = mRenderer->getStateManager();
    stateManager->setShaderResource(gl::SAMPLER_PIXEL, 0, nullptr);

    // Apply render target
    stateManager->setOneTimeRenderTarget(nullptr, dest);

    // Set the viewport
    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width    = static_cast<FLOAT>(destSize.width);
    viewport.Height   = static_cast<FLOAT>(destSize.height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    deviceContext->RSSetViewports(1, &viewport);

    // Apply textures
    stateManager->setShaderResource(gl::SAMPLER_PIXEL, 0, source);

    // Apply samplers
    deviceContext->PSSetSamplers(0, 1, &mPointSampler);

    // Draw the quad
    deviceContext->Draw(drawCount, 0);

    // Unbind textures and render targets and vertex buffer
    stateManager->setShaderResource(gl::SAMPLER_PIXEL, 0, nullptr);

    UINT zero                      = 0;
    ID3D11Buffer *const nullBuffer = nullptr;
    deviceContext->IASetVertexBuffers(0, 1, &nullBuffer, &zero, &zero);

    mRenderer->markAllStateDirty();

    return gl::Error(GL_NO_ERROR);
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
        const auto &srcFormat = source.getFormatSet().format;

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

    ID3D11Device *device               = mRenderer->getDevice();
    ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();

    TextureHelper11 sourceStaging;
    ANGLE_TRY_RESULT(CreateStagingTexture(GL_TEXTURE_2D, source.getFormatSet(), sourceSize,
                                          StagingAccess::READ, device),
                     sourceStaging);

    deviceContext->CopySubresourceRegion(sourceStaging.getResource(), 0, 0, 0, 0,
                                         source.getResource(), sourceSubresource, nullptr);

    D3D11_MAPPED_SUBRESOURCE sourceMapping;
    HRESULT result =
        deviceContext->Map(sourceStaging.getResource(), 0, D3D11_MAP_READ, 0, &sourceMapping);
    if (FAILED(result))
    {
        return gl::Error(
            GL_OUT_OF_MEMORY,
            "Failed to map internal source staging texture for depth stencil blit, HRESULT: 0x%X.",
            result);
    }

    D3D11_MAPPED_SUBRESOURCE destMapping;
    result = deviceContext->Map(destStaging.getResource(), 0, D3D11_MAP_WRITE, 0, &destMapping);
    if (FAILED(result))
    {
        deviceContext->Unmap(sourceStaging.getResource(), 0);
        return gl::Error(GL_OUT_OF_MEMORY,
                         "Failed to map internal destination staging texture for depth stencil "
                         "blit, HRESULT: 0x%X.",
                         result);
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

    deviceContext->Unmap(sourceStaging.getResource(), 0);
    deviceContext->Unmap(destStaging.getResource(), 0);

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

    ID3D11Device *device               = mRenderer->getDevice();
    ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();

    // HACK: Create the destination staging buffer as a read/write texture so
    // ID3D11DevicContext::UpdateSubresource can be called
    //       using it's mapped data as a source
    TextureHelper11 destStaging;
    ANGLE_TRY_RESULT(CreateStagingTexture(GL_TEXTURE_2D, dest.getFormatSet(), destSize,
                                          StagingAccess::READ_WRITE, device),
                     destStaging);

    deviceContext->CopySubresourceRegion(destStaging.getResource(), 0, 0, 0, 0, dest.getResource(),
                                         destSubresource, nullptr);

    copyAndConvertImpl(source, sourceSubresource, sourceArea, sourceSize, destStaging, destArea,
                       destSize, scissor, readOffset, writeOffset, copySize, srcPixelStride,
                       destPixelStride, convertFunction);

    // Work around timeouts/TDRs in older NVIDIA drivers.
    if (mRenderer->getWorkarounds().depthStencilBlitExtraCopy)
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        deviceContext->Map(destStaging.getResource(), 0, D3D11_MAP_READ, 0, &mapped);
        deviceContext->UpdateSubresource(dest.getResource(), destSubresource, nullptr, mapped.pData,
                                         mapped.RowPitch, mapped.DepthPitch);
        deviceContext->Unmap(destStaging.getResource(), 0);
    }
    else
    {
        deviceContext->CopySubresourceRegion(dest.getResource(), destSubresource, 0, 0, 0,
                                             destStaging.getResource(), 0, nullptr);
    }

    return gl::NoError();
}

void Blit11::addBlitShaderToMap(BlitShaderType blitShaderType,
                                ShaderDimension dimension,
                                ID3D11PixelShader *ps)
{
    ASSERT(mBlitShaderMap.find(blitShaderType) == mBlitShaderMap.end());
    ASSERT(ps);

    Shader shader;
    shader.dimension   = dimension;
    shader.pixelShader = ps;

    mBlitShaderMap[blitShaderType] = shader;
}

void Blit11::addSwizzleShaderToMap(SwizzleShaderType swizzleShaderType,
                                   ShaderDimension dimension,
                                   ID3D11PixelShader *ps)
{
    ASSERT(mSwizzleShaderMap.find(swizzleShaderType) == mSwizzleShaderMap.end());
    ASSERT(ps);

    Shader shader;
    shader.dimension   = dimension;
    shader.pixelShader = ps;

    mSwizzleShaderMap[swizzleShaderType] = shader;
}

void Blit11::clearShaderMap()
{
    for (auto &blitShader : mBlitShaderMap)
    {
        SafeRelease(blitShader.second.pixelShader);
    }
    mBlitShaderMap.clear();

    for (auto &swizzleShader : mSwizzleShaderMap)
    {
        SafeRelease(swizzleShader.second.pixelShader);
    }
    mSwizzleShaderMap.clear();
}

gl::Error Blit11::getBlitShader(GLenum destFormat,
                                bool isSigned,
                                bool unpackPremultiplyAlpha,
                                bool unpackUnmultiplyAlpha,
                                ShaderDimension dimension,
                                const Shader **shader)
{
    BlitShaderType blitShaderType = GetBlitShaderType(destFormat, isSigned, unpackPremultiplyAlpha,
                                                      unpackUnmultiplyAlpha, dimension);

    if (blitShaderType == BLITSHADER_INVALID)
    {
        return gl::Error(GL_INVALID_OPERATION, "Internal blit shader type mismatch");
    }

    auto blitShaderIt = mBlitShaderMap.find(blitShaderType);
    if (blitShaderIt != mBlitShaderMap.end())
    {
        *shader = &blitShaderIt->second;
        return gl::Error(GL_NO_ERROR);
    }

    ASSERT(dimension == SHADER_2D || mRenderer->isES3Capable());

    ID3D11Device *device = mRenderer->getDevice();

    switch (blitShaderType)
    {
        case BLITSHADER_2D_RGBAF:
            addBlitShaderToMap(
                blitShaderType, SHADER_2D,
                d3d11::CompilePS(device, g_PS_PassthroughRGBA2D, "Blit11 2D RGBA pixel shader"));
            break;
        case BLITSHADER_2D_RGBAF_PREMULTIPLY:
            addBlitShaderToMap(blitShaderType, SHADER_2D,
                               d3d11::CompilePS(device, g_PS_PassthroughRGBAPremultiply2D,
                                                "Blit11 2D RGBA premultiply pixel shader"));
            break;
        case BLITSHADER_2D_RGBAF_UNMULTIPLY:
            addBlitShaderToMap(blitShaderType, SHADER_2D,
                               d3d11::CompilePS(device, g_PS_PassthroughRGBAUnmultiply2D,
                                                "Blit11 2D RGBA unmultiply pixel shader"));
            break;
        case BLITSHADER_2D_BGRAF:
            addBlitShaderToMap(
                blitShaderType, SHADER_2D,
                d3d11::CompilePS(device, g_PS_PassthroughRGBA2D, "Blit11 2D BGRA pixel shader"));
            break;
        case BLITSHADER_2D_BGRAF_PREMULTIPLY:
            addBlitShaderToMap(blitShaderType, SHADER_2D,
                               d3d11::CompilePS(device, g_PS_PassthroughRGBAPremultiply2D,
                                                "Blit11 2D BGRA premultiply pixel shader"));
            break;
        case BLITSHADER_2D_BGRAF_UNMULTIPLY:
            addBlitShaderToMap(blitShaderType, SHADER_2D,
                               d3d11::CompilePS(device, g_PS_PassthroughRGBAUnmultiply2D,
                                                "Blit11 2D BGRA unmultiply pixel shader"));
            break;
        case BLITSHADER_2D_RGBF:
            addBlitShaderToMap(
                blitShaderType, SHADER_2D,
                d3d11::CompilePS(device, g_PS_PassthroughRGB2D, "Blit11 2D RGB pixel shader"));
            break;
        case BLITSHADER_2D_RGBF_PREMULTIPLY:
            addBlitShaderToMap(blitShaderType, SHADER_2D,
                               d3d11::CompilePS(device, g_PS_PassthroughRGBPremultiply2D,
                                                "Blit11 2D RGB premultiply pixel shader"));
            break;
        case BLITSHADER_2D_RGBF_UNMULTIPLY:
            addBlitShaderToMap(blitShaderType, SHADER_2D,
                               d3d11::CompilePS(device, g_PS_PassthroughRGBUnmultiply2D,
                                                "Blit11 2D RGB unmultiply pixel shader"));
            break;
        case BLITSHADER_2D_RGF:
            addBlitShaderToMap(
                blitShaderType, SHADER_2D,
                d3d11::CompilePS(device, g_PS_PassthroughRG2D, "Blit11 2D RG pixel shader"));
            break;
        case BLITSHADER_2D_RF:
            addBlitShaderToMap(
                blitShaderType, SHADER_2D,
                d3d11::CompilePS(device, g_PS_PassthroughR2D, "Blit11 2D R pixel shader"));
            break;
        case BLITSHADER_2D_ALPHA:
            addBlitShaderToMap(
                blitShaderType, SHADER_2D,
                d3d11::CompilePS(device, g_PS_PassthroughRGBA2D, "Blit11 2D alpha pixel shader"));
            break;
        case BLITSHADER_2D_LUMA:
            addBlitShaderToMap(
                blitShaderType, SHADER_2D,
                d3d11::CompilePS(device, g_PS_PassthroughLum2D, "Blit11 2D lum pixel shader"));
            break;
        case BLITSHADER_2D_LUMAALPHA:
            addBlitShaderToMap(blitShaderType, SHADER_2D,
                               d3d11::CompilePS(device, g_PS_PassthroughLumAlpha2D,
                                                "Blit11 2D luminance alpha pixel shader"));
            break;
        case BLITSHADER_2D_RGBAUI:
            addBlitShaderToMap(blitShaderType, SHADER_2D,
                               d3d11::CompilePS(device, g_PS_PassthroughRGBA2DUI,
                                                "Blit11 2D RGBA UI pixel shader"));
            break;
        case BLITSHADER_2D_RGBAI:
            addBlitShaderToMap(
                blitShaderType, SHADER_2D,
                d3d11::CompilePS(device, g_PS_PassthroughRGBA2DI, "Blit11 2D RGBA I pixel shader"));
            break;
        case BLITSHADER_2D_RGBUI:
            addBlitShaderToMap(
                blitShaderType, SHADER_2D,
                d3d11::CompilePS(device, g_PS_PassthroughRGB2DUI, "Blit11 2D RGB UI pixel shader"));
            break;
        case BLITSHADER_2D_RGBI:
            addBlitShaderToMap(
                blitShaderType, SHADER_2D,
                d3d11::CompilePS(device, g_PS_PassthroughRGB2DI, "Blit11 2D RGB I pixel shader"));
            break;
        case BLITSHADER_2D_RGUI:
            addBlitShaderToMap(
                blitShaderType, SHADER_2D,
                d3d11::CompilePS(device, g_PS_PassthroughRG2DUI, "Blit11 2D RG UI pixel shader"));
            break;
        case BLITSHADER_2D_RGI:
            addBlitShaderToMap(
                blitShaderType, SHADER_2D,
                d3d11::CompilePS(device, g_PS_PassthroughRG2DI, "Blit11 2D RG I pixel shader"));
            break;
        case BLITSHADER_2D_RUI:
            addBlitShaderToMap(
                blitShaderType, SHADER_2D,
                d3d11::CompilePS(device, g_PS_PassthroughR2DUI, "Blit11 2D R UI pixel shader"));
            break;
        case BLITSHADER_2D_RI:
            addBlitShaderToMap(
                blitShaderType, SHADER_2D,
                d3d11::CompilePS(device, g_PS_PassthroughR2DI, "Blit11 2D R I pixel shader"));
            break;
        case BLITSHADER_3D_RGBAF:
            addBlitShaderToMap(
                blitShaderType, SHADER_3D,
                d3d11::CompilePS(device, g_PS_PassthroughRGBA3D, "Blit11 3D RGBA pixel shader"));
            break;
        case BLITSHADER_3D_RGBAUI:
            addBlitShaderToMap(blitShaderType, SHADER_3D,
                               d3d11::CompilePS(device, g_PS_PassthroughRGBA3DUI,
                                                "Blit11 3D UI RGBA pixel shader"));
            break;
        case BLITSHADER_3D_RGBAI:
            addBlitShaderToMap(
                blitShaderType, SHADER_3D,
                d3d11::CompilePS(device, g_PS_PassthroughRGBA3DI, "Blit11 3D I RGBA pixel shader"));
            break;
        case BLITSHADER_3D_BGRAF:
            addBlitShaderToMap(
                blitShaderType, SHADER_3D,
                d3d11::CompilePS(device, g_PS_PassthroughRGBA3D, "Blit11 3D BGRA pixel shader"));
            break;
        case BLITSHADER_3D_RGBF:
            addBlitShaderToMap(
                blitShaderType, SHADER_3D,
                d3d11::CompilePS(device, g_PS_PassthroughRGB3D, "Blit11 3D RGB pixel shader"));
            break;
        case BLITSHADER_3D_RGBUI:
            addBlitShaderToMap(
                blitShaderType, SHADER_3D,
                d3d11::CompilePS(device, g_PS_PassthroughRGB3DUI, "Blit11 3D RGB UI pixel shader"));
            break;
        case BLITSHADER_3D_RGBI:
            addBlitShaderToMap(
                blitShaderType, SHADER_3D,
                d3d11::CompilePS(device, g_PS_PassthroughRGB3DI, "Blit11 3D RGB I pixel shader"));
            break;
        case BLITSHADER_3D_RGF:
            addBlitShaderToMap(
                blitShaderType, SHADER_3D,
                d3d11::CompilePS(device, g_PS_PassthroughRG3D, "Blit11 3D RG pixel shader"));
            break;
        case BLITSHADER_3D_RGUI:
            addBlitShaderToMap(
                blitShaderType, SHADER_3D,
                d3d11::CompilePS(device, g_PS_PassthroughRG3DUI, "Blit11 3D RG UI pixel shader"));
            break;
        case BLITSHADER_3D_RGI:
            addBlitShaderToMap(
                blitShaderType, SHADER_3D,
                d3d11::CompilePS(device, g_PS_PassthroughRG3DI, "Blit11 3D RG I pixel shader"));
            break;
        case BLITSHADER_3D_RF:
            addBlitShaderToMap(
                blitShaderType, SHADER_3D,
                d3d11::CompilePS(device, g_PS_PassthroughR3D, "Blit11 3D R pixel shader"));
            break;
        case BLITSHADER_3D_RUI:
            addBlitShaderToMap(
                blitShaderType, SHADER_3D,
                d3d11::CompilePS(device, g_PS_PassthroughR3DUI, "Blit11 3D R UI pixel shader"));
            break;
        case BLITSHADER_3D_RI:
            addBlitShaderToMap(
                blitShaderType, SHADER_3D,
                d3d11::CompilePS(device, g_PS_PassthroughR3DI, "Blit11 3D R I pixel shader"));
            break;
        case BLITSHADER_3D_ALPHA:
            addBlitShaderToMap(
                blitShaderType, SHADER_3D,
                d3d11::CompilePS(device, g_PS_PassthroughRGBA3D, "Blit11 3D alpha pixel shader"));
            break;
        case BLITSHADER_3D_LUMA:
            addBlitShaderToMap(blitShaderType, SHADER_3D,
                               d3d11::CompilePS(device, g_PS_PassthroughLum3D,
                                                "Blit11 3D luminance pixel shader"));
            break;
        case BLITSHADER_3D_LUMAALPHA:
            addBlitShaderToMap(blitShaderType, SHADER_3D,
                               d3d11::CompilePS(device, g_PS_PassthroughLumAlpha3D,
                                                "Blit11 3D luminance alpha pixel shader"));
            break;
        default:
            UNREACHABLE();
            return gl::Error(GL_INVALID_OPERATION, "Internal error");
    }

    blitShaderIt = mBlitShaderMap.find(blitShaderType);
    ASSERT(blitShaderIt != mBlitShaderMap.end());
    *shader = &blitShaderIt->second;
    return gl::Error(GL_NO_ERROR);
}

gl::Error Blit11::getSwizzleShader(GLenum type,
                                   D3D11_SRV_DIMENSION viewDimension,
                                   const Shader **shader)
{
    SwizzleShaderType swizzleShaderType = GetSwizzleShaderType(type, viewDimension);

    if (swizzleShaderType == SWIZZLESHADER_INVALID)
    {
        return gl::Error(GL_INVALID_OPERATION, "Swizzle shader type not found");
    }

    auto swizzleShaderIt = mSwizzleShaderMap.find(swizzleShaderType);
    if (swizzleShaderIt != mSwizzleShaderMap.end())
    {
        *shader = &swizzleShaderIt->second;
        return gl::Error(GL_NO_ERROR);
    }

    // Swizzling shaders (OpenGL ES 3+)
    ASSERT(mRenderer->isES3Capable());

    ID3D11Device *device = mRenderer->getDevice();

    switch (swizzleShaderType)
    {
        case SWIZZLESHADER_2D_FLOAT:
            addSwizzleShaderToMap(
                swizzleShaderType, SHADER_2D,
                d3d11::CompilePS(device, g_PS_SwizzleF2D, "Blit11 2D F swizzle pixel shader"));
            break;
        case SWIZZLESHADER_2D_UINT:
            addSwizzleShaderToMap(
                swizzleShaderType, SHADER_2D,
                d3d11::CompilePS(device, g_PS_SwizzleUI2D, "Blit11 2D UI swizzle pixel shader"));
            break;
        case SWIZZLESHADER_2D_INT:
            addSwizzleShaderToMap(
                swizzleShaderType, SHADER_2D,
                d3d11::CompilePS(device, g_PS_SwizzleI2D, "Blit11 2D I swizzle pixel shader"));
            break;
        case SWIZZLESHADER_CUBE_FLOAT:
            addSwizzleShaderToMap(swizzleShaderType, SHADER_3D,
                                  d3d11::CompilePS(device, g_PS_SwizzleF2DArray,
                                                   "Blit11 2D Cube F swizzle pixel shader"));
            break;
        case SWIZZLESHADER_CUBE_UINT:
            addSwizzleShaderToMap(swizzleShaderType, SHADER_3D,
                                  d3d11::CompilePS(device, g_PS_SwizzleUI2DArray,
                                                   "Blit11 2D Cube UI swizzle pixel shader"));
            break;
        case SWIZZLESHADER_CUBE_INT:
            addSwizzleShaderToMap(swizzleShaderType, SHADER_3D,
                                  d3d11::CompilePS(device, g_PS_SwizzleI2DArray,
                                                   "Blit11 2D Cube I swizzle pixel shader"));
            break;
        case SWIZZLESHADER_3D_FLOAT:
            addSwizzleShaderToMap(
                swizzleShaderType, SHADER_3D,
                d3d11::CompilePS(device, g_PS_SwizzleF3D, "Blit11 3D F swizzle pixel shader"));
            break;
        case SWIZZLESHADER_3D_UINT:
            addSwizzleShaderToMap(
                swizzleShaderType, SHADER_3D,
                d3d11::CompilePS(device, g_PS_SwizzleUI3D, "Blit11 3D UI swizzle pixel shader"));
            break;
        case SWIZZLESHADER_3D_INT:
            addSwizzleShaderToMap(
                swizzleShaderType, SHADER_3D,
                d3d11::CompilePS(device, g_PS_SwizzleI3D, "Blit11 3D I swizzle pixel shader"));
            break;
        case SWIZZLESHADER_ARRAY_FLOAT:
            addSwizzleShaderToMap(swizzleShaderType, SHADER_3D,
                                  d3d11::CompilePS(device, g_PS_SwizzleF2DArray,
                                                   "Blit11 2D Array F swizzle pixel shader"));
            break;
        case SWIZZLESHADER_ARRAY_UINT:
            addSwizzleShaderToMap(swizzleShaderType, SHADER_3D,
                                  d3d11::CompilePS(device, g_PS_SwizzleUI2DArray,
                                                   "Blit11 2D Array UI swizzle pixel shader"));
            break;
        case SWIZZLESHADER_ARRAY_INT:
            addSwizzleShaderToMap(swizzleShaderType, SHADER_3D,
                                  d3d11::CompilePS(device, g_PS_SwizzleI2DArray,
                                                   "Blit11 2D Array I swizzle pixel shader"));
            break;
        default:
            UNREACHABLE();
            return gl::Error(GL_INVALID_OPERATION, "Internal error");
    }

    swizzleShaderIt = mSwizzleShaderMap.find(swizzleShaderType);
    ASSERT(swizzleShaderIt != mSwizzleShaderMap.end());
    *shader = &swizzleShaderIt->second;
    return gl::NoError();
}

gl::ErrorOrResult<TextureHelper11> Blit11::resolveDepth(RenderTarget11 *depth)
{
    const auto &extents          = depth->getExtents();
    ID3D11Device *device         = mRenderer->getDevice();
    ID3D11DeviceContext *context = mRenderer->getDeviceContext();

    ANGLE_TRY(initResolveDepthStencil(extents));

    // Notify the Renderer that all state should be invalidated.
    mRenderer->markAllStateDirty();

    // Apply the necessary state changes to the D3D11 immediate device context.
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(mResolveDepthStencilVS.resolve(device), nullptr, 0);
    context->GSSetShader(nullptr, nullptr, 0);
    context->RSSetState(nullptr);
    context->OMSetDepthStencilState(nullptr, 0xFFFFFFFF);
    context->OMSetRenderTargets(1, &mResolvedDepthStencilRTView, nullptr);
    context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFF);

    // Set the viewport
    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width    = static_cast<FLOAT>(extents.width);
    viewport.Height   = static_cast<FLOAT>(extents.height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);

    ID3D11ShaderResourceView *pixelViews[] = {depth->getShaderResourceView()};

    context->PSSetShaderResources(0, 1, pixelViews);

    context->PSSetShader(mResolveDepthPS.resolve(device), nullptr, 0);

    // Trigger the blit on the GPU.
    context->Draw(6, 0);

    gl::Box copyBox(0, 0, 0, extents.width, extents.height, 1);

    const auto &copyFunction = GetCopyDepthStencilFunction(depth->getInternalFormat());
    const auto &dsFormatSet  = depth->getFormatSet();
    const auto &dsDxgiInfo   = d3d11::GetDXGIFormatSizeInfo(dsFormatSet.texFormat);

    ID3D11Texture2D *destTex = nullptr;

    D3D11_TEXTURE2D_DESC destDesc;
    destDesc.Width              = extents.width;
    destDesc.Height             = extents.height;
    destDesc.MipLevels          = 1;
    destDesc.ArraySize          = 1;
    destDesc.Format             = dsFormatSet.texFormat;
    destDesc.SampleDesc.Count   = 1;
    destDesc.SampleDesc.Quality = 0;
    destDesc.Usage              = D3D11_USAGE_DEFAULT;
    destDesc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
    destDesc.CPUAccessFlags     = 0;
    destDesc.MiscFlags          = 0;

    HRESULT hr = device->CreateTexture2D(&destDesc, nullptr, &destTex);
    if (FAILED(hr))
    {
        return gl::Error(GL_OUT_OF_MEMORY, "Error creating depth resolve dest texture.");
    }
    d3d11::SetDebugName(destTex, "resolveDepthDest");

    TextureHelper11 dest = TextureHelper11::MakeAndPossess2D(destTex, depth->getFormatSet());
    ANGLE_TRY(copyAndConvert(mResolvedDepthStencil, 0, copyBox, extents, dest, 0, copyBox, extents,
                             nullptr, 0, 0, 0, 8, dsDxgiInfo.pixelBytes, copyFunction));
    return dest;
}

gl::Error Blit11::initResolveDepthStencil(const gl::Extents &extents)
{
    // Check if we need to recreate depth stencil view
    if (mResolvedDepthStencil.valid() && extents == mResolvedDepthStencil.getExtents())
    {
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

    ID3D11Device *device = mRenderer->getDevice();

    ID3D11Texture2D *resolvedDepthStencil = nullptr;
    HRESULT hr = device->CreateTexture2D(&textureDesc, nullptr, &resolvedDepthStencil);
    if (FAILED(hr))
    {
        return gl::Error(GL_OUT_OF_MEMORY, "Failed to allocate resolved depth stencil texture");
    }
    d3d11::SetDebugName(resolvedDepthStencil, "Blit11::mResolvedDepthStencil");

    ASSERT(mResolvedDepthStencilRTView == nullptr);
    hr =
        device->CreateRenderTargetView(resolvedDepthStencil, nullptr, &mResolvedDepthStencilRTView);
    if (FAILED(hr))
    {
        return gl::Error(GL_OUT_OF_MEMORY,
                         "Failed to allocate Blit11::mResolvedDepthStencilRTView");
    }
    d3d11::SetDebugName(mResolvedDepthStencilRTView, "Blit11::mResolvedDepthStencilRTView");

    mResolvedDepthStencil = TextureHelper11::MakeAndPossess2D(resolvedDepthStencil, formatSet);

    return gl::NoError();
}

gl::ErrorOrResult<TextureHelper11> Blit11::resolveStencil(RenderTarget11 *depthStencil,
                                                          bool alsoDepth)
{
    const auto &extents = depthStencil->getExtents();

    ANGLE_TRY(initResolveDepthStencil(extents));

    ID3D11Device *device         = mRenderer->getDevice();
    ID3D11DeviceContext *context = mRenderer->getDeviceContext();

    ID3D11Resource *stencilResource = depthStencil->getTexture();

    // Check if we need to re-create the stencil SRV.
    if (mStencilSRV)
    {
        ID3D11Resource *priorResource = nullptr;
        mStencilSRV->GetResource(&priorResource);

        if (stencilResource != priorResource)
        {
            SafeRelease(mStencilSRV);
        }
    }

    if (!mStencilSRV)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srViewDesc;
        srViewDesc.Format        = GetStencilSRVFormat(depthStencil->getFormatSet());
        srViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;

        HRESULT hr = device->CreateShaderResourceView(stencilResource, &srViewDesc, &mStencilSRV);
        if (FAILED(hr))
        {
            return gl::Error(GL_OUT_OF_MEMORY, "Error creating Blit11 stencil SRV");
        }
        d3d11::SetDebugName(mStencilSRV, "Blit11::mStencilSRV");
    }

    // Notify the Renderer that all state should be invalidated.
    mRenderer->markAllStateDirty();

    // Apply the necessary state changes to the D3D11 immediate device context.
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(mResolveDepthStencilVS.resolve(device), nullptr, 0);
    context->GSSetShader(nullptr, nullptr, 0);
    context->RSSetState(nullptr);
    context->OMSetDepthStencilState(nullptr, 0xFFFFFFFF);
    context->OMSetRenderTargets(1, &mResolvedDepthStencilRTView, nullptr);
    context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFF);

    // Set the viewport
    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width    = static_cast<FLOAT>(extents.width);
    viewport.Height   = static_cast<FLOAT>(extents.height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);

    ID3D11ShaderResourceView *pixelViews[] = {
        depthStencil->getShaderResourceView(), mStencilSRV,
    };

    context->PSSetShaderResources(0, 2, pixelViews);

    // Resolving the depth buffer works by sampling the depth in the shader using a SRV, then
    // writing to the resolved depth buffer using SV_Depth. We can't use this method for stencil
    // because SV_StencilRef isn't supported until HLSL 5.1/D3D11.3.
    if (alsoDepth)
    {
        context->PSSetShader(mResolveDepthStencilPS.resolve(device), nullptr, 0);
    }
    else
    {
        context->PSSetShader(mResolveStencilPS.resolve(device), nullptr, 0);
    }

    // Trigger the blit on the GPU.
    context->Draw(6, 0);

    gl::Box copyBox(0, 0, 0, extents.width, extents.height, 1);

    TextureHelper11 dest;
    ANGLE_TRY_RESULT(CreateStagingTexture(GL_TEXTURE_2D, depthStencil->getFormatSet(), extents,
                                          StagingAccess::READ_WRITE, device),
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
    SafeRelease(mStencilSRV);
    SafeRelease(mResolvedDepthStencilRTView);
}

}  // namespace rx
