//
// Copyright (c) 2013-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// formatutils11.h: Queries for GL image formats and their translations to D3D11
// formats.

#ifndef LIBANGLE_RENDERER_D3D_D3D11_FORMATUTILS11_H_
#define LIBANGLE_RENDERER_D3D_D3D11_FORMATUTILS11_H_

#include <map>

#include "common/platform.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/renderer_utils.h"
#include "libANGLE/renderer/d3d/formatutilsD3D.h"

namespace rx
{
struct Renderer11DeviceCaps;

namespace d3d11
{

// A texture might be stored as DXGI_FORMAT_R16_TYPELESS but store integer components,
// which are accessed through an DXGI_FORMAT_R16_SINT view. It's easy to write code which queries
// information about the wrong format. Therefore, use of this should be avoided where possible.

bool SupportsMipGen(DXGI_FORMAT dxgiFormat, D3D_FEATURE_LEVEL featureLevel);

struct DXGIFormatSize
{
    DXGIFormatSize(GLuint pixelBits, GLuint blockWidth, GLuint blockHeight);

    GLuint pixelBytes;
    GLuint blockWidth;
    GLuint blockHeight;
};
const DXGIFormatSize &GetDXGIFormatSizeInfo(DXGI_FORMAT format);

struct VertexFormat : angle::NonCopyable
{
    constexpr VertexFormat();
    constexpr VertexFormat(VertexConversionType conversionType,
                           DXGI_FORMAT nativeFormat,
                           VertexCopyFunction copyFunction);

    VertexConversionType conversionType;
    DXGI_FORMAT nativeFormat;
    VertexCopyFunction copyFunction;
};

const VertexFormat &GetVertexFormatInfo(gl::VertexFormatType vertexFormatType,
                                        D3D_FEATURE_LEVEL featureLevel);

// Auto-generated in dxgi_format_map_autogen.cpp.
GLenum GetComponentType(DXGI_FORMAT dxgiFormat);

}  // namespace d3d11

namespace d3d11_angle
{
const angle::Format &GetFormat(DXGI_FORMAT dxgiFormat);
}

}  // namespace rx

#endif // LIBANGLE_RENDERER_D3D_D3D11_FORMATUTILS11_H_
