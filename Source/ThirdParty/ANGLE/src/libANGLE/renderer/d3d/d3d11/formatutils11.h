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
#include "libANGLE/renderer/d3d/formatutilsD3D.h"

namespace rx
{
struct Renderer11DeviceCaps;

namespace d3d11
{

typedef std::map<std::pair<GLenum, GLenum>, ColorCopyFunction> FastCopyFunctionMap;
typedef bool (*NativeMipmapGenerationSupportFunction)(D3D_FEATURE_LEVEL);

struct DXGIFormat
{
    DXGIFormat();

    GLuint redBits;
    GLuint greenBits;
    GLuint blueBits;
    GLuint alphaBits;
    GLuint sharedBits;

    GLuint depthBits;
    GLuint stencilBits;

    GLenum componentType;

    FastCopyFunctionMap fastCopyFunctions;

    NativeMipmapGenerationSupportFunction nativeMipmapSupport;

    ColorCopyFunction getFastCopyFunction(GLenum format, GLenum type) const;
};

// This structure is problematic because a resource is associated with multiple DXGI formats.
// For example, a texture might be stored as DXGI_FORMAT_R16_TYPELESS but store integer components,
// which are accessed through an DXGI_FORMAT_R16_SINT view. It's easy to write code which queries
// information about the wrong format. Therefore, use of this should be avoided where possible.
const DXGIFormat &GetDXGIFormatInfo(DXGI_FORMAT format);

struct DXGIFormatSize
{
    DXGIFormatSize(GLuint pixelBits, GLuint blockWidth, GLuint blockHeight);

    GLuint pixelBytes;
    GLuint blockWidth;
    GLuint blockHeight;
};
const DXGIFormatSize &GetDXGIFormatSizeInfo(DXGI_FORMAT format);

struct VertexFormat
{
    VertexFormat();
    VertexFormat(VertexConversionType conversionType,
                 DXGI_FORMAT nativeFormat,
                 VertexCopyFunction copyFunction);

    VertexConversionType conversionType;
    DXGI_FORMAT nativeFormat;
    VertexCopyFunction copyFunction;
};
const VertexFormat &GetVertexFormatInfo(gl::VertexFormatType vertexFormatType,
                                        D3D_FEATURE_LEVEL featureLevel);

}  // namespace d3d11

}  // namespace rx

#endif // LIBANGLE_RENDERER_D3D_D3D11_FORMATUTILS11_H_
