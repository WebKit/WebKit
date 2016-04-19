//
// Copyright (c) 2013-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// formatutils11.cpp: Queries for GL image formats and their translations to D3D11
// formats.

#include "libANGLE/renderer/d3d/d3d11/formatutils11.h"

#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/d3d/copyimage.h"
#include "libANGLE/renderer/d3d/d3d11/copyvertex.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/generatemip.h"
#include "libANGLE/renderer/d3d/loadimage.h"
#include "libANGLE/renderer/Renderer.h"

namespace rx
{

namespace d3d11
{

struct D3D11FastCopyFormat
{
    GLenum destFormat;
    GLenum destType;
    ColorCopyFunction copyFunction;

    D3D11FastCopyFormat(GLenum destFormat, GLenum destType, ColorCopyFunction copyFunction)
        : destFormat(destFormat), destType(destType), copyFunction(copyFunction)
    { }

    bool operator<(const D3D11FastCopyFormat& other) const
    {
        return memcmp(this, &other, sizeof(D3D11FastCopyFormat)) < 0;
    }
};

typedef std::multimap<DXGI_FORMAT, D3D11FastCopyFormat> D3D11FastCopyMap;

static D3D11FastCopyMap BuildFastCopyMap()
{
    D3D11FastCopyMap map;

    map.insert(std::make_pair(DXGI_FORMAT_B8G8R8A8_UNORM, D3D11FastCopyFormat(GL_RGBA, GL_UNSIGNED_BYTE, CopyBGRA8ToRGBA8)));

    return map;
}

struct DXGIColorFormatInfo
{
    size_t redBits;
    size_t greenBits;
    size_t blueBits;

    size_t luminanceBits;

    size_t alphaBits;
    size_t sharedBits;
};

typedef std::map<DXGI_FORMAT, DXGIColorFormatInfo> ColorFormatInfoMap;
typedef std::pair<DXGI_FORMAT, DXGIColorFormatInfo> ColorFormatInfoPair;

static inline void InsertDXGIColorFormatInfo(ColorFormatInfoMap *map, DXGI_FORMAT format, size_t redBits, size_t greenBits,
                                             size_t blueBits, size_t alphaBits, size_t sharedBits)
{
    DXGIColorFormatInfo info;
    info.redBits = redBits;
    info.greenBits = greenBits;
    info.blueBits = blueBits;
    info.alphaBits = alphaBits;
    info.sharedBits = sharedBits;

    map->insert(std::make_pair(format, info));
}

static ColorFormatInfoMap BuildColorFormatInfoMap()
{
    ColorFormatInfoMap map;

    // clang-format off
    //                             | DXGI format                         | R | G | B | A | S |
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_A8_UNORM,                  0,  0,  0,  8,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R8_UNORM,                  8,  0,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R8G8_UNORM,                8,  8,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R8G8B8A8_UNORM,            8,  8,  8,  8,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,       8,  8,  8,  8,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_B8G8R8A8_UNORM,            8,  8,  8,  8,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R16_UNORM,                16,  0,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R16G16_UNORM,             16, 16,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R16G16B16A16_UNORM,       16, 16, 16, 16,  0);

    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R8_SNORM,                  8,  0,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R8G8_SNORM,                8,  8,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R8G8B8A8_SNORM,            8,  8,  8,  8,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R16_SNORM,                16,  0,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R16G16_SNORM,             16, 16,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R16G16B16A16_SNORM,       16, 16, 16, 16,  0);

    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R8_UINT,                   8,  0,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R16_UINT,                 16,  0,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R32_UINT,                 32,  0,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R8G8_UINT,                 8,  8,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R16G16_UINT,              16, 16,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R32G32_UINT,              32, 32,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R32G32B32_UINT,           32, 32, 32,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R8G8B8A8_UINT,             8,  8,  8,  8,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R16G16B16A16_UINT,        16, 16, 16, 16,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R32G32B32A32_UINT,        32, 32, 32, 32,  0);

    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R8_SINT,                   8,  0,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R16_SINT,                 16,  0,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R32_SINT,                 32,  0,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R8G8_SINT,                 8,  8,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R16G16_SINT,              16, 16,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R32G32_SINT,              32, 32,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R32G32B32_SINT,           32, 32, 32,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R8G8B8A8_SINT,             8,  8,  8,  8,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R16G16B16A16_SINT,        16, 16, 16, 16,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R32G32B32A32_SINT,        32, 32, 32, 32,  0);

    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R10G10B10A2_TYPELESS,     10, 10, 10,  2,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R10G10B10A2_UNORM,        10, 10, 10,  2,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R10G10B10A2_UINT,         10, 10, 10,  2,  0);

    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R16_FLOAT,                16,  0,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R16G16_FLOAT,             16, 16,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R16G16B16A16_FLOAT,       16, 16, 16, 16,  0);

    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R32_FLOAT,                32,  0,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R32G32_FLOAT,             32, 32,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R32G32B32_FLOAT,          32, 32, 32,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R32G32B32A32_FLOAT,       32, 32, 32, 32,  0);

    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R9G9B9E5_SHAREDEXP,        9,  9,  9,  0,  5);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R11G11B10_FLOAT,          11, 11, 10,  0,  0);

    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_B5G6R5_UNORM,              5,  6,  5,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_B4G4R4A4_UNORM,            4,  4,  4,  4,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_B5G5R5A1_UNORM,            5,  5,  5,  1,  0);

    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R8_TYPELESS,               8,  0,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R16_TYPELESS,             16,  0,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R32_TYPELESS,             32,  0,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R8G8_TYPELESS,             8,  8,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R16G16_TYPELESS,          16, 16,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R32G32_TYPELESS,          32, 32,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R32G32B32_TYPELESS,       32, 32, 32,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R8G8B8A8_TYPELESS,         8,  8,  8,  8,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R16G16B16A16_TYPELESS,    16, 16, 16, 16,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R32G32B32A32_TYPELESS,    32, 32, 32, 32,  0);

    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R24G8_TYPELESS,           24,  8,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R24_UNORM_X8_TYPELESS,    24,  0,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R32G8X24_TYPELESS,        32,  8,  0,  0,  0);
    InsertDXGIColorFormatInfo(&map, DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS, 32,  0,  0,  0,  0);
    // clang-format on

    return map;
}

struct DXGIDepthStencilInfo
{
    unsigned int depthBits;
    unsigned int stencilBits;
};

typedef std::map<DXGI_FORMAT, DXGIDepthStencilInfo> DepthStencilInfoMap;
typedef std::pair<DXGI_FORMAT, DXGIDepthStencilInfo> DepthStencilInfoPair;

static inline void InsertDXGIDepthStencilInfo(DepthStencilInfoMap *map,
                                              DXGI_FORMAT format,
                                              unsigned int depthBits,
                                              unsigned int stencilBits)
{
    DXGIDepthStencilInfo info;
    info.depthBits = depthBits;
    info.stencilBits = stencilBits;

    map->insert(std::make_pair(format, info));
}

static DepthStencilInfoMap BuildDepthStencilInfoMap()
{
    DepthStencilInfoMap map;

    // clang-format off
    InsertDXGIDepthStencilInfo(&map, DXGI_FORMAT_D16_UNORM,                16, 0);
    InsertDXGIDepthStencilInfo(&map, DXGI_FORMAT_D24_UNORM_S8_UINT,        24, 8);
    InsertDXGIDepthStencilInfo(&map, DXGI_FORMAT_D32_FLOAT,                32, 0);
    InsertDXGIDepthStencilInfo(&map, DXGI_FORMAT_D32_FLOAT_S8X24_UINT,     32, 8);
    // clang-format on

    return map;
}

typedef std::map<DXGI_FORMAT, DXGIFormat> DXGIFormatInfoMap;

DXGIFormat::DXGIFormat()
    : redBits(0),
      greenBits(0),
      blueBits(0),
      alphaBits(0),
      sharedBits(0),
      depthBits(0),
      stencilBits(0),
      componentType(GL_NONE),
      fastCopyFunctions(),
      nativeMipmapSupport(NULL)
{
}

static bool NeverSupported(D3D_FEATURE_LEVEL)
{
    return false;
}

template <D3D_FEATURE_LEVEL requiredFeatureLevel>
static bool RequiresFeatureLevel(D3D_FEATURE_LEVEL featureLevel)
{
    return featureLevel >= requiredFeatureLevel;
}

ColorCopyFunction DXGIFormat::getFastCopyFunction(GLenum format, GLenum type) const
{
    FastCopyFunctionMap::const_iterator iter = fastCopyFunctions.find(std::make_pair(format, type));
    return (iter != fastCopyFunctions.end()) ? iter->second : NULL;
}

void AddDXGIFormat(DXGIFormatInfoMap *map,
                   DXGI_FORMAT dxgiFormat,
                   GLenum componentType,
                   NativeMipmapGenerationSupportFunction nativeMipmapSupport)
{
    DXGIFormat info;

    static const ColorFormatInfoMap colorInfoMap = BuildColorFormatInfoMap();
    ColorFormatInfoMap::const_iterator colorInfoIter = colorInfoMap.find(dxgiFormat);
    if (colorInfoIter != colorInfoMap.end())
    {
        const DXGIColorFormatInfo &colorInfo = colorInfoIter->second;
        info.redBits                         = static_cast<GLuint>(colorInfo.redBits);
        info.greenBits                       = static_cast<GLuint>(colorInfo.greenBits);
        info.blueBits                        = static_cast<GLuint>(colorInfo.blueBits);
        info.alphaBits                       = static_cast<GLuint>(colorInfo.alphaBits);
        info.sharedBits                      = static_cast<GLuint>(colorInfo.sharedBits);
    }

    static const DepthStencilInfoMap dsInfoMap = BuildDepthStencilInfoMap();
    DepthStencilInfoMap::const_iterator dsInfoIter = dsInfoMap.find(dxgiFormat);
    if (dsInfoIter != dsInfoMap.end())
    {
        const DXGIDepthStencilInfo &dsInfo = dsInfoIter->second;
        info.depthBits = dsInfo.depthBits;
        info.stencilBits = dsInfo.stencilBits;
    }

    info.componentType = componentType;

    static const D3D11FastCopyMap fastCopyMap = BuildFastCopyMap();
    std::pair<D3D11FastCopyMap::const_iterator, D3D11FastCopyMap::const_iterator> fastCopyIter = fastCopyMap.equal_range(dxgiFormat);
    for (D3D11FastCopyMap::const_iterator i = fastCopyIter.first; i != fastCopyIter.second; i++)
    {
        info.fastCopyFunctions.insert(std::make_pair(std::make_pair(i->second.destFormat, i->second.destType), i->second.copyFunction));
    }

    info.nativeMipmapSupport = nativeMipmapSupport;

    map->insert(std::make_pair(dxgiFormat, info));
}

// A map to determine the color read function and mipmap generation function of a given DXGI format
static DXGIFormatInfoMap BuildDXGIFormatInfoMap()
{
    DXGIFormatInfoMap map;

    // clang-format off
    //                | DXGI format                         | Component Type        | Color read function              | Native mipmap function
    AddDXGIFormat(&map, DXGI_FORMAT_UNKNOWN,                  GL_NONE,                NeverSupported);

    AddDXGIFormat(&map, DXGI_FORMAT_A8_UNORM,                 GL_UNSIGNED_NORMALIZED, RequiresFeatureLevel<D3D_FEATURE_LEVEL_10_0>);
    AddDXGIFormat(&map, DXGI_FORMAT_R8_UNORM,                 GL_UNSIGNED_NORMALIZED, RequiresFeatureLevel<D3D_FEATURE_LEVEL_10_0>);
    AddDXGIFormat(&map, DXGI_FORMAT_R16_UNORM,                GL_UNSIGNED_NORMALIZED, NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R8G8_UNORM,               GL_UNSIGNED_NORMALIZED, RequiresFeatureLevel<D3D_FEATURE_LEVEL_10_0>);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16_UNORM,             GL_UNSIGNED_NORMALIZED, NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R8G8B8A8_UNORM,           GL_UNSIGNED_NORMALIZED, RequiresFeatureLevel<D3D_FEATURE_LEVEL_9_1>);
    AddDXGIFormat(&map, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,      GL_UNSIGNED_NORMALIZED, RequiresFeatureLevel<D3D_FEATURE_LEVEL_9_1>);
    AddDXGIFormat(&map, DXGI_FORMAT_B8G8R8A8_UNORM,           GL_UNSIGNED_NORMALIZED, RequiresFeatureLevel<D3D_FEATURE_LEVEL_9_1>);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16B16A16_UNORM,       GL_UNSIGNED_NORMALIZED, NeverSupported);

    AddDXGIFormat(&map, DXGI_FORMAT_R8_SNORM,                 GL_SIGNED_NORMALIZED,   RequiresFeatureLevel<D3D_FEATURE_LEVEL_10_0>);
    AddDXGIFormat(&map, DXGI_FORMAT_R16_SNORM,                GL_SIGNED_NORMALIZED,   NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R8G8_SNORM,               GL_SIGNED_NORMALIZED,   RequiresFeatureLevel<D3D_FEATURE_LEVEL_10_0>);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16_SNORM,             GL_SIGNED_NORMALIZED,   NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R8G8B8A8_SNORM,           GL_SIGNED_NORMALIZED,   RequiresFeatureLevel<D3D_FEATURE_LEVEL_10_0>);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16B16A16_SNORM,       GL_SIGNED_NORMALIZED,   NeverSupported);

    AddDXGIFormat(&map, DXGI_FORMAT_R8_UINT,                  GL_UNSIGNED_INT,        NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R16_UINT,                 GL_UNSIGNED_INT,        NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R32_UINT,                 GL_UNSIGNED_INT,        NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R8G8_UINT,                GL_UNSIGNED_INT,        NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16_UINT,              GL_UNSIGNED_INT,        NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G32_UINT,              GL_UNSIGNED_INT,        NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G32B32_UINT,           GL_UNSIGNED_INT,        NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R8G8B8A8_UINT,            GL_UNSIGNED_INT,        NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16B16A16_UINT,        GL_UNSIGNED_INT,        NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G32B32A32_UINT,        GL_UNSIGNED_INT,        NeverSupported);

    AddDXGIFormat(&map, DXGI_FORMAT_R8_SINT,                  GL_INT,                 NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R16_SINT,                 GL_INT,                 NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R32_SINT,                 GL_INT,                 NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R8G8_SINT,                GL_INT,                 NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16_SINT,              GL_INT,                 NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G32_SINT,              GL_INT,                 NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G32B32_SINT,           GL_INT,                 NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R8G8B8A8_SINT,            GL_INT,                 NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16B16A16_SINT,        GL_INT,                 NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G32B32A32_SINT,        GL_INT,                 NeverSupported);

    AddDXGIFormat(&map, DXGI_FORMAT_R10G10B10A2_TYPELESS,     GL_NONE,                NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R10G10B10A2_UNORM,        GL_UNSIGNED_NORMALIZED, RequiresFeatureLevel<D3D_FEATURE_LEVEL_10_0>);
    AddDXGIFormat(&map, DXGI_FORMAT_R10G10B10A2_UINT,         GL_UNSIGNED_INT,        NeverSupported);

    AddDXGIFormat(&map, DXGI_FORMAT_R16_FLOAT,                GL_FLOAT,               RequiresFeatureLevel<D3D_FEATURE_LEVEL_10_0>);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16_FLOAT,             GL_FLOAT,               RequiresFeatureLevel<D3D_FEATURE_LEVEL_9_2>);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16B16A16_FLOAT,       GL_FLOAT,               RequiresFeatureLevel<D3D_FEATURE_LEVEL_9_2>);

    AddDXGIFormat(&map, DXGI_FORMAT_R32_FLOAT,                GL_FLOAT,               RequiresFeatureLevel<D3D_FEATURE_LEVEL_9_2>);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G32_FLOAT,             GL_FLOAT,               RequiresFeatureLevel<D3D_FEATURE_LEVEL_10_0>);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G32B32_FLOAT,          GL_FLOAT,               NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G32B32A32_FLOAT,       GL_FLOAT,               RequiresFeatureLevel<D3D_FEATURE_LEVEL_9_3>);

    AddDXGIFormat(&map, DXGI_FORMAT_R9G9B9E5_SHAREDEXP,       GL_FLOAT,               NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R11G11B10_FLOAT,          GL_FLOAT,               RequiresFeatureLevel<D3D_FEATURE_LEVEL_10_0>);

    AddDXGIFormat(&map, DXGI_FORMAT_R8_TYPELESS,              GL_NONE,                NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R16_TYPELESS,             GL_NONE,                NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R32_TYPELESS,             GL_NONE,                NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R8G8_TYPELESS,            GL_NONE,                NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16_TYPELESS,          GL_NONE,                NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G32_TYPELESS,          GL_NONE,                NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R8G8_TYPELESS,            GL_NONE,                NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16_TYPELESS,          GL_NONE,                NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G32_TYPELESS,          GL_NONE,                NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G32B32_TYPELESS,       GL_NONE,                NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R8G8B8A8_TYPELESS,        GL_NONE,                NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16B16A16_TYPELESS,    GL_NONE,                NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G32B32A32_TYPELESS,    GL_NONE,                NeverSupported);

    AddDXGIFormat(&map, DXGI_FORMAT_R24G8_TYPELESS,           GL_NONE,                NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R24_UNORM_X8_TYPELESS,    GL_NONE,                NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G8X24_TYPELESS,        GL_NONE,                NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS, GL_NONE,                NeverSupported);

    AddDXGIFormat(&map, DXGI_FORMAT_D16_UNORM,                GL_UNSIGNED_NORMALIZED, NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_D24_UNORM_S8_UINT,        GL_UNSIGNED_INT,        NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_D32_FLOAT_S8X24_UINT,     GL_UNSIGNED_INT,        NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_D32_FLOAT,                GL_FLOAT,               NeverSupported);

    AddDXGIFormat(&map, DXGI_FORMAT_BC1_UNORM,                GL_UNSIGNED_NORMALIZED, NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_BC2_UNORM,                GL_UNSIGNED_NORMALIZED, NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_BC3_UNORM,                GL_UNSIGNED_NORMALIZED, NeverSupported);

    // B5G6R5 in D3D11 is treated the same as R5G6B5 in D3D9, so reuse the R5G6B5 functions used by the D3D9 renderer.
    // The same applies to B4G4R4A4 and B5G5R5A1 with A4R4G4B4 and A1R5G5B5 respectively.
    AddDXGIFormat(&map, DXGI_FORMAT_B5G6R5_UNORM,             GL_UNSIGNED_NORMALIZED, RequiresFeatureLevel<D3D_FEATURE_LEVEL_9_1>);
    AddDXGIFormat(&map, DXGI_FORMAT_B4G4R4A4_UNORM,           GL_UNSIGNED_NORMALIZED, NeverSupported);
    AddDXGIFormat(&map, DXGI_FORMAT_B5G5R5A1_UNORM,           GL_UNSIGNED_NORMALIZED, NeverSupported);
    // clang-format on

    return map;
}

const DXGIFormat &GetDXGIFormatInfo(DXGI_FORMAT format)
{
    static const DXGIFormatInfoMap infoMap = BuildDXGIFormatInfoMap();
    DXGIFormatInfoMap::const_iterator iter = infoMap.find(format);
    if (iter != infoMap.end())
    {
        return iter->second;
    }
    else
    {
        static DXGIFormat defaultInfo;
        return defaultInfo;
    }
}

DXGIFormatSize::DXGIFormatSize(GLuint pixelBits, GLuint blockWidth, GLuint blockHeight)
    : pixelBytes(pixelBits / 8), blockWidth(blockWidth), blockHeight(blockHeight)
{
}

const DXGIFormatSize &GetDXGIFormatSizeInfo(DXGI_FORMAT format)
{
    static const DXGIFormatSize sizeUnknown(0, 0, 0);
    static const DXGIFormatSize size128(128, 1, 1);
    static const DXGIFormatSize size96(96, 1, 1);
    static const DXGIFormatSize size64(64, 1, 1);
    static const DXGIFormatSize size32(32, 1, 1);
    static const DXGIFormatSize size16(16, 1, 1);
    static const DXGIFormatSize size8(8, 1, 1);
    static const DXGIFormatSize sizeBC1(64, 4, 4);
    static const DXGIFormatSize sizeBC2(128, 4, 4);
    static const DXGIFormatSize sizeBC3(128, 4, 4);
    static const DXGIFormatSize sizeBC4(64, 4, 4);
    static const DXGIFormatSize sizeBC5(128, 4, 4);
    static const DXGIFormatSize sizeBC6H(128, 4, 4);
    static const DXGIFormatSize sizeBC7(128, 4, 4);
    switch (format)
    {
        case DXGI_FORMAT_UNKNOWN:
            return sizeUnknown;
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_UINT:
        case DXGI_FORMAT_R32G32B32A32_SINT:
            return size128;
        case DXGI_FORMAT_R32G32B32_TYPELESS:
        case DXGI_FORMAT_R32G32B32_FLOAT:
        case DXGI_FORMAT_R32G32B32_UINT:
        case DXGI_FORMAT_R32G32B32_SINT:
            return size96;
        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UINT:
        case DXGI_FORMAT_R16G16B16A16_SNORM:
        case DXGI_FORMAT_R16G16B16A16_SINT:
        case DXGI_FORMAT_R32G32_TYPELESS:
        case DXGI_FORMAT_R32G32_FLOAT:
        case DXGI_FORMAT_R32G32_UINT:
        case DXGI_FORMAT_R32G32_SINT:
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
            return size64;
        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UINT:
        case DXGI_FORMAT_R11G11B10_FLOAT:
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SNORM:
        case DXGI_FORMAT_R8G8B8A8_SINT:
        case DXGI_FORMAT_R16G16_TYPELESS:
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R16G16_UNORM:
        case DXGI_FORMAT_R16G16_UINT:
        case DXGI_FORMAT_R16G16_SNORM:
        case DXGI_FORMAT_R16G16_SINT:
        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R32_UINT:
        case DXGI_FORMAT_R32_SINT:
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
            return size32;
        case DXGI_FORMAT_R8G8_TYPELESS:
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8G8_UINT:
        case DXGI_FORMAT_R8G8_SNORM:
        case DXGI_FORMAT_R8G8_SINT:
        case DXGI_FORMAT_R16_TYPELESS:
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_R16_UNORM:
        case DXGI_FORMAT_R16_UINT:
        case DXGI_FORMAT_R16_SNORM:
        case DXGI_FORMAT_R16_SINT:
            return size16;
        case DXGI_FORMAT_R8_TYPELESS:
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_UINT:
        case DXGI_FORMAT_R8_SNORM:
        case DXGI_FORMAT_R8_SINT:
        case DXGI_FORMAT_A8_UNORM:
            return size8;
        case DXGI_FORMAT_R1_UNORM:
            UNREACHABLE();
            return sizeUnknown;
        case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
        case DXGI_FORMAT_R8G8_B8G8_UNORM:
        case DXGI_FORMAT_G8R8_G8B8_UNORM:
            return size32;
        case DXGI_FORMAT_BC1_TYPELESS:
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
            return sizeBC1;
        case DXGI_FORMAT_BC2_TYPELESS:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
            return sizeBC2;
        case DXGI_FORMAT_BC3_TYPELESS:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
            return sizeBC3;
        case DXGI_FORMAT_BC4_TYPELESS:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
            return sizeBC4;
        case DXGI_FORMAT_BC5_TYPELESS:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
            return sizeBC5;
        case DXGI_FORMAT_B5G6R5_UNORM:
        case DXGI_FORMAT_B5G5R5A1_UNORM:
            return size16;
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8X8_TYPELESS:
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
            return size32;
        case DXGI_FORMAT_BC6H_TYPELESS:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
            return sizeBC6H;
        case DXGI_FORMAT_BC7_TYPELESS:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            return sizeBC7;
        case DXGI_FORMAT_AYUV:
        case DXGI_FORMAT_Y410:
        case DXGI_FORMAT_Y416:
        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_P010:
        case DXGI_FORMAT_P016:
        case DXGI_FORMAT_420_OPAQUE:
        case DXGI_FORMAT_YUY2:
        case DXGI_FORMAT_Y210:
        case DXGI_FORMAT_Y216:
        case DXGI_FORMAT_NV11:
        case DXGI_FORMAT_AI44:
        case DXGI_FORMAT_IA44:
        case DXGI_FORMAT_P8:
        case DXGI_FORMAT_A8P8:
            UNREACHABLE();
            return sizeUnknown;
        case DXGI_FORMAT_B4G4R4A4_UNORM:
            return size16;
        default:
            UNREACHABLE();
            return sizeUnknown;
    }
}

typedef std::map<gl::VertexFormatType, VertexFormat> D3D11VertexFormatInfoMap;
typedef std::pair<gl::VertexFormatType, VertexFormat> D3D11VertexFormatPair;

VertexFormat::VertexFormat()
    : conversionType(VERTEX_CONVERT_NONE),
      nativeFormat(DXGI_FORMAT_UNKNOWN),
      copyFunction(NULL)
{
}

VertexFormat::VertexFormat(VertexConversionType conversionTypeIn,
                           DXGI_FORMAT nativeFormatIn,
                           VertexCopyFunction copyFunctionIn)
    : conversionType(conversionTypeIn),
      nativeFormat(nativeFormatIn),
      copyFunction(copyFunctionIn)
{
}

static void AddVertexFormatInfo(D3D11VertexFormatInfoMap *map,
                                GLenum inputType,
                                GLboolean normalized,
                                GLuint componentCount,
                                VertexConversionType conversionType,
                                DXGI_FORMAT nativeFormat,
                                VertexCopyFunction copyFunction)
{
    gl::VertexFormatType formatType = gl::GetVertexFormatType(inputType, normalized, componentCount, false);

    VertexFormat info;
    info.conversionType = conversionType;
    info.nativeFormat = nativeFormat;
    info.copyFunction = copyFunction;

    map->insert(D3D11VertexFormatPair(formatType, info));
}

static D3D11VertexFormatInfoMap BuildD3D11_FL9_3VertexFormatInfoOverrideMap()
{
    // D3D11 Feature Level 9_3 doesn't support as many formats for vertex buffer resource as Feature Level 10_0+.
    // http://msdn.microsoft.com/en-us/library/windows/desktop/ff471324(v=vs.85).aspx

    D3D11VertexFormatInfoMap map;

    // GL_BYTE -- unnormalized
    AddVertexFormatInfo(&map, GL_BYTE,           GL_FALSE,  1,  VERTEX_CONVERT_BOTH,    DXGI_FORMAT_R16G16_SINT,         &Copy8SintTo16SintVertexData<1, 2>);
    AddVertexFormatInfo(&map, GL_BYTE,           GL_FALSE,  2,  VERTEX_CONVERT_BOTH,    DXGI_FORMAT_R16G16_SINT,         &Copy8SintTo16SintVertexData<2, 2>);
    AddVertexFormatInfo(&map, GL_BYTE,           GL_FALSE,  3,  VERTEX_CONVERT_BOTH,    DXGI_FORMAT_R16G16B16A16_SINT,   &Copy8SintTo16SintVertexData<3, 4>);
    AddVertexFormatInfo(&map, GL_BYTE,           GL_FALSE,  4,  VERTEX_CONVERT_BOTH,    DXGI_FORMAT_R16G16B16A16_SINT,   &Copy8SintTo16SintVertexData<4, 4>);

    // GL_BYTE -- normalized
    AddVertexFormatInfo(&map, GL_BYTE,           GL_TRUE,   1,  VERTEX_CONVERT_CPU,     DXGI_FORMAT_R16G16_SNORM,        &Copy8SnormTo16SnormVertexData<1, 2>);
    AddVertexFormatInfo(&map, GL_BYTE,           GL_TRUE,   2,  VERTEX_CONVERT_CPU,     DXGI_FORMAT_R16G16_SNORM,        &Copy8SnormTo16SnormVertexData<2, 2>);
    AddVertexFormatInfo(&map, GL_BYTE,           GL_TRUE,   3,  VERTEX_CONVERT_CPU,     DXGI_FORMAT_R16G16B16A16_SNORM,  &Copy8SnormTo16SnormVertexData<3, 4>);
    AddVertexFormatInfo(&map, GL_BYTE,           GL_TRUE,   4,  VERTEX_CONVERT_CPU,     DXGI_FORMAT_R16G16B16A16_SNORM,  &Copy8SnormTo16SnormVertexData<4, 4>);

    // GL_UNSIGNED_BYTE -- unnormalized
    AddVertexFormatInfo(&map, GL_UNSIGNED_BYTE,  GL_FALSE,  1,  VERTEX_CONVERT_BOTH,    DXGI_FORMAT_R8G8B8A8_UINT,       &CopyNativeVertexData<GLubyte, 1, 4, 1>);
    AddVertexFormatInfo(&map, GL_UNSIGNED_BYTE,  GL_FALSE,  2,  VERTEX_CONVERT_BOTH,    DXGI_FORMAT_R8G8B8A8_UINT,       &CopyNativeVertexData<GLubyte, 2, 4, 1>);
    // NOTE: 3 and 4 component unnormalized GL_UNSIGNED_BYTE should use the default format table.

    // GL_UNSIGNED_BYTE -- normalized
    AddVertexFormatInfo(&map, GL_UNSIGNED_BYTE,  GL_TRUE,   1,  VERTEX_CONVERT_CPU,     DXGI_FORMAT_R8G8B8A8_UNORM,      &CopyNativeVertexData<GLubyte, 1, 4, UINT8_MAX>);
    AddVertexFormatInfo(&map, GL_UNSIGNED_BYTE,  GL_TRUE,   2,  VERTEX_CONVERT_CPU,     DXGI_FORMAT_R8G8B8A8_UNORM,      &CopyNativeVertexData<GLubyte, 2, 4, UINT8_MAX>);
    // NOTE: 3 and 4 component normalized GL_UNSIGNED_BYTE should use the default format table.

    // GL_SHORT -- unnormalized
    AddVertexFormatInfo(&map, GL_SHORT,          GL_FALSE,  1,  VERTEX_CONVERT_BOTH,     DXGI_FORMAT_R16G16_SINT,        &CopyNativeVertexData<GLshort, 1, 2, 0>);
    // NOTE: 2, 3 and 4 component unnormalized GL_SHORT should use the default format table.

    // GL_SHORT -- normalized
    AddVertexFormatInfo(&map, GL_SHORT,          GL_TRUE,   1,  VERTEX_CONVERT_CPU,     DXGI_FORMAT_R16G16_SNORM,        &CopyNativeVertexData<GLshort, 1, 2, 0>);
    // NOTE: 2, 3 and 4 component normalized GL_SHORT should use the default format table.

    // GL_UNSIGNED_SHORT -- unnormalized
    AddVertexFormatInfo(&map, GL_UNSIGNED_SHORT, GL_FALSE,  1,  VERTEX_CONVERT_CPU,     DXGI_FORMAT_R32G32_FLOAT,        &CopyTo32FVertexData<GLushort, 1, 2, false>);
    AddVertexFormatInfo(&map, GL_UNSIGNED_SHORT, GL_FALSE,  2,  VERTEX_CONVERT_CPU,     DXGI_FORMAT_R32G32_FLOAT,        &CopyTo32FVertexData<GLushort, 2, 2, false>);
    AddVertexFormatInfo(&map, GL_UNSIGNED_SHORT, GL_FALSE,  3,  VERTEX_CONVERT_CPU,     DXGI_FORMAT_R32G32B32_FLOAT,     &CopyTo32FVertexData<GLushort, 3, 3, false>);
    AddVertexFormatInfo(&map, GL_UNSIGNED_SHORT, GL_FALSE,  4,  VERTEX_CONVERT_CPU,     DXGI_FORMAT_R32G32B32A32_FLOAT,  &CopyTo32FVertexData<GLushort, 4, 4, false>);

    // GL_UNSIGNED_SHORT -- normalized
    AddVertexFormatInfo(&map, GL_UNSIGNED_SHORT, GL_TRUE,   1,  VERTEX_CONVERT_CPU,     DXGI_FORMAT_R32G32_FLOAT,        &CopyTo32FVertexData<GLushort, 1, 2, true>);
    AddVertexFormatInfo(&map, GL_UNSIGNED_SHORT, GL_TRUE,   2,  VERTEX_CONVERT_CPU,     DXGI_FORMAT_R32G32_FLOAT,        &CopyTo32FVertexData<GLushort, 2, 2, true>);
    AddVertexFormatInfo(&map, GL_UNSIGNED_SHORT, GL_TRUE,   3,  VERTEX_CONVERT_CPU,     DXGI_FORMAT_R32G32B32_FLOAT,     &CopyTo32FVertexData<GLushort, 3, 3, true>);
    AddVertexFormatInfo(&map, GL_UNSIGNED_SHORT, GL_TRUE,   4,  VERTEX_CONVERT_CPU,     DXGI_FORMAT_R32G32B32A32_FLOAT,  &CopyTo32FVertexData<GLushort, 4, 4, true>);

    // GL_FIXED
    // TODO: Add test to verify that this works correctly.
    AddVertexFormatInfo(&map, GL_FIXED,          GL_FALSE,  1, VERTEX_CONVERT_CPU,      DXGI_FORMAT_R32G32_FLOAT,        &Copy32FixedTo32FVertexData<1, 2>);
    // NOTE: 2, 3 and 4 component GL_FIXED should use the default format table.

    // GL_FLOAT
    // TODO: Add test to verify that this works correctly.
    AddVertexFormatInfo(&map, GL_FLOAT,          GL_FALSE,  1, VERTEX_CONVERT_CPU,      DXGI_FORMAT_R32G32_FLOAT,        &CopyNativeVertexData<GLfloat, 1, 2, 0>);
    // NOTE: 2, 3 and 4 component GL_FLOAT should use the default format table.

    return map;
}

const VertexFormat &GetVertexFormatInfo(gl::VertexFormatType vertexFormatType, D3D_FEATURE_LEVEL featureLevel)
{
    if (featureLevel == D3D_FEATURE_LEVEL_9_3)
    {
        static const D3D11VertexFormatInfoMap vertexFormatMapFL9_3Override =
            BuildD3D11_FL9_3VertexFormatInfoOverrideMap();

        // First see if the format has a special mapping for FL9_3
        auto iter = vertexFormatMapFL9_3Override.find(vertexFormatType);
        if (iter != vertexFormatMapFL9_3Override.end())
        {
            return iter->second;
        }
    }

    switch (vertexFormatType)
    {
        //
        // Float formats
        //

        // GL_BYTE -- un-normalized
        case gl::VERTEX_FORMAT_SBYTE1:
        {
            static const VertexFormat info(VERTEX_CONVERT_GPU, DXGI_FORMAT_R8_SINT, &CopyNativeVertexData<GLbyte, 1, 1, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_SBYTE2:
        {
            static const VertexFormat info(VERTEX_CONVERT_GPU, DXGI_FORMAT_R8G8_SINT, &CopyNativeVertexData<GLbyte, 2, 2, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_SBYTE3:
        {
            static const VertexFormat info(VERTEX_CONVERT_BOTH, DXGI_FORMAT_R8G8B8A8_SINT, &CopyNativeVertexData<GLbyte, 3, 4, 1>);
            return info;
        }
        case gl::VERTEX_FORMAT_SBYTE4:
        {
            static const VertexFormat info(VERTEX_CONVERT_GPU, DXGI_FORMAT_R8G8B8A8_SINT, &CopyNativeVertexData<GLbyte, 4, 4, 0>);
            return info;
        }

        // GL_BYTE -- normalized
        case gl::VERTEX_FORMAT_SBYTE1_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R8_SNORM, &CopyNativeVertexData<GLbyte, 1, 1, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_SBYTE2_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R8G8_SNORM, &CopyNativeVertexData<GLbyte, 2, 2, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_SBYTE3_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R8G8B8A8_SNORM, &CopyNativeVertexData<GLbyte, 3, 4, INT8_MAX>);
            return info;
        }
        case gl::VERTEX_FORMAT_SBYTE4_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R8G8B8A8_SNORM, &CopyNativeVertexData<GLbyte, 4, 4, 0>);
            return info;
        }

        // GL_UNSIGNED_BYTE -- un-normalized
        case gl::VERTEX_FORMAT_UBYTE1:
        {
            static const VertexFormat info(VERTEX_CONVERT_GPU, DXGI_FORMAT_R8_UINT, &CopyNativeVertexData<GLubyte, 1, 1, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_UBYTE2:
        {
            static const VertexFormat info(VERTEX_CONVERT_GPU, DXGI_FORMAT_R8G8_UINT, &CopyNativeVertexData<GLubyte, 2, 2, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_UBYTE3:
        {
            static const VertexFormat info(VERTEX_CONVERT_BOTH, DXGI_FORMAT_R8G8B8A8_UINT, &CopyNativeVertexData<GLubyte, 3, 4, 1>);
            return info;
        }
        case gl::VERTEX_FORMAT_UBYTE4:
        {
            static const VertexFormat info(VERTEX_CONVERT_GPU, DXGI_FORMAT_R8G8B8A8_UINT, &CopyNativeVertexData<GLubyte, 4, 4, 0>);
            return info;
        }

        // GL_UNSIGNED_BYTE -- normalized
        case gl::VERTEX_FORMAT_UBYTE1_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R8_UNORM, &CopyNativeVertexData<GLubyte, 1, 1, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_UBYTE2_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R8G8_UNORM, &CopyNativeVertexData<GLubyte, 2, 2, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_UBYTE3_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R8G8B8A8_UNORM, &CopyNativeVertexData<GLubyte, 3, 4, UINT8_MAX>);
            return info;
        }
        case gl::VERTEX_FORMAT_UBYTE4_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R8G8B8A8_UNORM, &CopyNativeVertexData<GLubyte, 4, 4, 0>);
            return info;
        }

        // GL_SHORT -- un-normalized
        case gl::VERTEX_FORMAT_SSHORT1:
        {
            static const VertexFormat info(VERTEX_CONVERT_GPU, DXGI_FORMAT_R16_SINT, &CopyNativeVertexData<GLshort, 1, 1, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_SSHORT2:
        {
            static const VertexFormat info(VERTEX_CONVERT_GPU, DXGI_FORMAT_R16G16_SINT, &CopyNativeVertexData<GLshort, 2, 2, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_SSHORT3:
        {
            static const VertexFormat info(VERTEX_CONVERT_BOTH, DXGI_FORMAT_R16G16B16A16_SINT, &CopyNativeVertexData<GLshort, 3, 4, 1>);
            return info;
        }
        case gl::VERTEX_FORMAT_SSHORT4:
        {
            static const VertexFormat info(VERTEX_CONVERT_GPU, DXGI_FORMAT_R16G16B16A16_SINT, &CopyNativeVertexData<GLshort, 4, 4, 0>);
            return info;
        }

        // GL_SHORT -- normalized
        case gl::VERTEX_FORMAT_SSHORT1_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R16_SNORM, &CopyNativeVertexData<GLshort, 1, 1, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_SSHORT2_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R16G16_SNORM, &CopyNativeVertexData<GLshort, 2, 2, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_SSHORT3_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R16G16B16A16_SNORM, &CopyNativeVertexData<GLshort, 3, 4, INT16_MAX>);
            return info;
        }
        case gl::VERTEX_FORMAT_SSHORT4_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R16G16B16A16_SNORM, &CopyNativeVertexData<GLshort, 4, 4, 0>);
            return info;
        }

        // GL_UNSIGNED_SHORT -- un-normalized
        case gl::VERTEX_FORMAT_USHORT1:
        {
            static const VertexFormat info(VERTEX_CONVERT_GPU, DXGI_FORMAT_R16_UINT, &CopyNativeVertexData<GLushort, 1, 1, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_USHORT2:
        {
            static const VertexFormat info(VERTEX_CONVERT_GPU, DXGI_FORMAT_R16G16_UINT, &CopyNativeVertexData<GLushort, 2, 2, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_USHORT3:
        {
            static const VertexFormat info(VERTEX_CONVERT_BOTH, DXGI_FORMAT_R16G16B16A16_UINT, &CopyNativeVertexData<GLushort, 3, 4, 1>);
            return info;
        }
        case gl::VERTEX_FORMAT_USHORT4:
        {
            static const VertexFormat info(VERTEX_CONVERT_GPU, DXGI_FORMAT_R16G16B16A16_UINT, &CopyNativeVertexData<GLushort, 4, 4, 0>);
            return info;
        }

        // GL_UNSIGNED_SHORT -- normalized
        case gl::VERTEX_FORMAT_USHORT1_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R16_UNORM, &CopyNativeVertexData<GLushort, 1, 1, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_USHORT2_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R16G16_UNORM, &CopyNativeVertexData<GLushort, 2, 2, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_USHORT3_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R16G16B16A16_UNORM, &CopyNativeVertexData<GLushort, 3, 4, UINT16_MAX>);
            return info;
        }
        case gl::VERTEX_FORMAT_USHORT4_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R16G16B16A16_UNORM, &CopyNativeVertexData<GLushort, 4, 4, 0>);
            return info;
        }

        // GL_INT -- un-normalized
        case gl::VERTEX_FORMAT_SINT1:
        {
            static const VertexFormat info(VERTEX_CONVERT_GPU, DXGI_FORMAT_R32_SINT, &CopyNativeVertexData<GLint, 1, 1, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_SINT2:
        {
            static const VertexFormat info(VERTEX_CONVERT_GPU, DXGI_FORMAT_R32G32_SINT, &CopyNativeVertexData<GLint, 2, 2, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_SINT3:
        {
            static const VertexFormat info(VERTEX_CONVERT_GPU, DXGI_FORMAT_R32G32B32_SINT, &CopyNativeVertexData<GLint, 3, 3, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_SINT4:
        {
            static const VertexFormat info(VERTEX_CONVERT_GPU, DXGI_FORMAT_R32G32B32A32_SINT, &CopyNativeVertexData<GLint, 4, 4, 0>);
            return info;
        }

        // GL_INT -- normalized
        case gl::VERTEX_FORMAT_SINT1_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R32_FLOAT, &CopyTo32FVertexData<GLint, 1, 1, true>);
            return info;
        }
        case gl::VERTEX_FORMAT_SINT2_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R32G32_FLOAT, &CopyTo32FVertexData<GLint, 2, 2, true>);
            return info;
        }
        case gl::VERTEX_FORMAT_SINT3_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R32G32B32_FLOAT, &CopyTo32FVertexData<GLint, 3, 3, true>);
            return info;
        }
        case gl::VERTEX_FORMAT_SINT4_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R32G32B32A32_FLOAT, &CopyTo32FVertexData<GLint, 4, 4, true>);
            return info;
        }

        // GL_UNSIGNED_INT -- un-normalized
        case gl::VERTEX_FORMAT_UINT1:
        {
            static const VertexFormat info(VERTEX_CONVERT_GPU, DXGI_FORMAT_R32_UINT, &CopyNativeVertexData<GLuint, 1, 1, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_UINT2:
        {
            static const VertexFormat info(VERTEX_CONVERT_GPU, DXGI_FORMAT_R32G32_UINT, &CopyNativeVertexData<GLuint, 2, 2, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_UINT3:
        {
            static const VertexFormat info(VERTEX_CONVERT_GPU, DXGI_FORMAT_R32G32B32_UINT, &CopyNativeVertexData<GLuint, 3, 3, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_UINT4:
        {
            static const VertexFormat info(VERTEX_CONVERT_GPU, DXGI_FORMAT_R32G32B32A32_UINT, &CopyNativeVertexData<GLuint, 4, 4, 0>);
            return info;
        }

        // GL_UNSIGNED_INT -- normalized
        case gl::VERTEX_FORMAT_UINT1_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R32_FLOAT,
                                           &CopyTo32FVertexData<GLuint, 1, 1, true>);
            return info;
        }
        case gl::VERTEX_FORMAT_UINT2_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R32G32_FLOAT,
                                           &CopyTo32FVertexData<GLuint, 2, 2, true>);
            return info;
        }
        case gl::VERTEX_FORMAT_UINT3_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R32G32B32_FLOAT, &CopyTo32FVertexData<GLuint, 3, 3, true>);
            return info;
        }
        case gl::VERTEX_FORMAT_UINT4_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R32G32B32A32_FLOAT,
                                           &CopyTo32FVertexData<GLuint, 4, 4, true>);
            return info;
        }

        // GL_FIXED
        case gl::VERTEX_FORMAT_FIXED1:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R32_FLOAT, &Copy32FixedTo32FVertexData<1, 1>);
            return info;
        }
        case gl::VERTEX_FORMAT_FIXED2:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R32G32_FLOAT, &Copy32FixedTo32FVertexData<2, 2>);
            return info;
        }
        case gl::VERTEX_FORMAT_FIXED3:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R32G32B32_FLOAT, &Copy32FixedTo32FVertexData<3, 3>);
            return info;
        }
        case gl::VERTEX_FORMAT_FIXED4:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R32G32B32A32_FLOAT, &Copy32FixedTo32FVertexData<4, 4>);
            return info;
        }

        // GL_HALF_FLOAT
        case gl::VERTEX_FORMAT_HALF1:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R16_FLOAT, &CopyNativeVertexData<GLhalf, 1, 1, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_HALF2:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R16G16_FLOAT, &CopyNativeVertexData<GLhalf, 2, 2, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_HALF3:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R16G16B16A16_FLOAT, &CopyNativeVertexData<GLhalf, 3, 4, gl::Float16One>);
            return info;
        }
        case gl::VERTEX_FORMAT_HALF4:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R16G16B16A16_FLOAT, &CopyNativeVertexData<GLhalf, 4, 4, 0>);
            return info;
        }

        // GL_FLOAT
        case gl::VERTEX_FORMAT_FLOAT1:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R32_FLOAT, &CopyNativeVertexData<GLfloat, 1, 1, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_FLOAT2:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R32G32_FLOAT, &CopyNativeVertexData<GLfloat, 2, 2, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_FLOAT3:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R32G32B32_FLOAT, &CopyNativeVertexData<GLfloat, 3, 3, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_FLOAT4:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R32G32B32A32_FLOAT, &CopyNativeVertexData<GLfloat, 4, 4, 0>);
            return info;
        }

        // GL_INT_2_10_10_10_REV
        case gl::VERTEX_FORMAT_SINT210:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R32G32B32A32_FLOAT, &CopyXYZ10W2ToXYZW32FVertexData<true, false, true>);
            return info;
        }
        case gl::VERTEX_FORMAT_SINT210_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R32G32B32A32_FLOAT, &CopyXYZ10W2ToXYZW32FVertexData<true, true, true>);
            return info;
        }

        // GL_UNSIGNED_INT_2_10_10_10_REV
        case gl::VERTEX_FORMAT_UINT210:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R32G32B32A32_FLOAT, &CopyXYZ10W2ToXYZW32FVertexData<false, false, true>);
            return info;
        }
        case gl::VERTEX_FORMAT_UINT210_NORM:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R10G10B10A2_UNORM, &CopyNativeVertexData<GLuint, 1, 1, 0>);
            return info;
        }

        //
        // Integer Formats
        //

        // GL_BYTE
        case gl::VERTEX_FORMAT_SBYTE1_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R8_SINT, &CopyNativeVertexData<GLbyte, 1, 1, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_SBYTE2_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R8G8_SINT, &CopyNativeVertexData<GLbyte, 2, 2, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_SBYTE3_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R8G8B8A8_SINT, &CopyNativeVertexData<GLbyte, 3, 4, 1>);
            return info;
        }
        case gl::VERTEX_FORMAT_SBYTE4_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R8G8B8A8_SINT, &CopyNativeVertexData<GLbyte, 4, 4, 0>);
            return info;
        }

        // GL_UNSIGNED_BYTE
        case gl::VERTEX_FORMAT_UBYTE1_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R8_UINT, &CopyNativeVertexData<GLubyte, 1, 1, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_UBYTE2_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R8G8_UINT, &CopyNativeVertexData<GLubyte, 2, 2, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_UBYTE3_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R8G8B8A8_UINT, &CopyNativeVertexData<GLubyte, 3, 4, 1>);
            return info;
        }
        case gl::VERTEX_FORMAT_UBYTE4_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R8G8B8A8_UINT, &CopyNativeVertexData<GLubyte, 4, 4, 0>);
            return info;
        }

        // GL_SHORT
        case gl::VERTEX_FORMAT_SSHORT1_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R16_SINT, &CopyNativeVertexData<GLshort, 1, 1, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_SSHORT2_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R16G16_SINT, &CopyNativeVertexData<GLshort, 2, 2, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_SSHORT3_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R16G16B16A16_SINT, &CopyNativeVertexData<GLshort, 3, 4, 1>);
            return info;
        }
        case gl::VERTEX_FORMAT_SSHORT4_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R16G16B16A16_SINT, &CopyNativeVertexData<GLshort, 4, 4, 0>);
            return info;
        }

        // GL_UNSIGNED_SHORT
        case gl::VERTEX_FORMAT_USHORT1_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R16_UINT, &CopyNativeVertexData<GLushort, 1, 1, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_USHORT2_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R16G16_UINT, &CopyNativeVertexData<GLushort, 2, 2, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_USHORT3_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R16G16B16A16_UINT, &CopyNativeVertexData<GLushort, 3, 4, 1>);
            return info;
        }
        case gl::VERTEX_FORMAT_USHORT4_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R16G16B16A16_UINT, &CopyNativeVertexData<GLushort, 4, 4, 0>);
            return info;
        }

        // GL_INT
        case gl::VERTEX_FORMAT_SINT1_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R32_SINT, &CopyNativeVertexData<GLint, 1, 1, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_SINT2_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R32G32_SINT, &CopyNativeVertexData<GLint, 2, 2, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_SINT3_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R32G32B32_SINT, &CopyNativeVertexData<GLint, 3, 3, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_SINT4_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R32G32B32A32_SINT, &CopyNativeVertexData<GLint, 4, 4, 0>);
            return info;
        }

        // GL_UNSIGNED_INT
        case gl::VERTEX_FORMAT_UINT1_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R32_SINT, &CopyNativeVertexData<GLuint, 1, 1, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_UINT2_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R32G32_SINT, &CopyNativeVertexData<GLuint, 2, 2, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_UINT3_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R32G32B32_SINT, &CopyNativeVertexData<GLuint, 3, 3, 0>);
            return info;
        }
        case gl::VERTEX_FORMAT_UINT4_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R32G32B32A32_SINT, &CopyNativeVertexData<GLuint, 4, 4, 0>);
            return info;
        }

        // GL_INT_2_10_10_10_REV
        case gl::VERTEX_FORMAT_SINT210_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_CPU, DXGI_FORMAT_R16G16B16A16_SINT, &CopyXYZ10W2ToXYZW32FVertexData<true, true, false>);
            return info;
        }

        // GL_UNSIGNED_INT_2_10_10_10_REV
        case gl::VERTEX_FORMAT_UINT210_INT:
        {
            static const VertexFormat info(VERTEX_CONVERT_NONE, DXGI_FORMAT_R10G10B10A2_UINT, &CopyNativeVertexData<GLuint, 1, 1, 0>);
            return info;
        }

        default:
        {
            static const VertexFormat info;
            return info;
        }
    }
}

}

}
