#include "precompiled.h"
//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// formatutils11.cpp: Queries for GL image formats and their translations to D3D11
// formats.

#include "libGLESv2/renderer/d3d11/formatutils11.h"
#include "libGLESv2/renderer/generatemip.h"
#include "libGLESv2/renderer/loadimage.h"
#include "libGLESv2/renderer/copyimage.h"
#include "libGLESv2/renderer/Renderer.h"
#include "libGLESv2/renderer/copyvertex.h"

namespace rx
{

struct D3D11ES3FormatInfo
{
    DXGI_FORMAT mTexFormat;
    DXGI_FORMAT mSRVFormat;
    DXGI_FORMAT mRTVFormat;
    DXGI_FORMAT mDSVFormat;

    D3D11ES3FormatInfo()
        : mTexFormat(DXGI_FORMAT_UNKNOWN), mDSVFormat(DXGI_FORMAT_UNKNOWN), mRTVFormat(DXGI_FORMAT_UNKNOWN), mSRVFormat(DXGI_FORMAT_UNKNOWN)
    { }

    D3D11ES3FormatInfo(DXGI_FORMAT texFormat, DXGI_FORMAT srvFormat, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat)
        : mTexFormat(texFormat), mDSVFormat(dsvFormat), mRTVFormat(rtvFormat), mSRVFormat(srvFormat)
    { }
};

// For sized GL internal formats, there is only one corresponding D3D11 format. This map type allows
// querying for the DXGI texture formats to use for textures, SRVs, RTVs and DSVs given a GL internal
// format.
typedef std::pair<GLenum, D3D11ES3FormatInfo> D3D11ES3FormatPair;
typedef std::map<GLenum, D3D11ES3FormatInfo> D3D11ES3FormatMap;

static D3D11ES3FormatMap BuildD3D11ES3FormatMap()
{
    D3D11ES3FormatMap map;

    //                           | GL internal format  |                  | D3D11 texture format            | D3D11 SRV format               | D3D11 RTV format               | D3D11 DSV format   |
    map.insert(D3D11ES3FormatPair(GL_NONE,              D3D11ES3FormatInfo(DXGI_FORMAT_UNKNOWN,              DXGI_FORMAT_UNKNOWN,             DXGI_FORMAT_UNKNOWN,             DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_R8,                D3D11ES3FormatInfo(DXGI_FORMAT_R8_UNORM,             DXGI_FORMAT_R8_UNORM,            DXGI_FORMAT_R8_UNORM,            DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_R8_SNORM,          D3D11ES3FormatInfo(DXGI_FORMAT_R8_SNORM,             DXGI_FORMAT_R8_SNORM,            DXGI_FORMAT_UNKNOWN,             DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RG8,               D3D11ES3FormatInfo(DXGI_FORMAT_R8G8_UNORM,           DXGI_FORMAT_R8G8_UNORM,          DXGI_FORMAT_R8G8_UNORM,          DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RG8_SNORM,         D3D11ES3FormatInfo(DXGI_FORMAT_R8G8_SNORM,           DXGI_FORMAT_R8G8_SNORM,          DXGI_FORMAT_UNKNOWN,             DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGB8,              D3D11ES3FormatInfo(DXGI_FORMAT_R8G8B8A8_UNORM,       DXGI_FORMAT_R8G8B8A8_UNORM,      DXGI_FORMAT_R8G8B8A8_UNORM,      DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGB8_SNORM,        D3D11ES3FormatInfo(DXGI_FORMAT_R8G8B8A8_SNORM,       DXGI_FORMAT_R8G8B8A8_SNORM,      DXGI_FORMAT_UNKNOWN,             DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGB565,            D3D11ES3FormatInfo(DXGI_FORMAT_R8G8B8A8_UNORM,       DXGI_FORMAT_R8G8B8A8_UNORM,      DXGI_FORMAT_R8G8B8A8_UNORM,      DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGBA4,             D3D11ES3FormatInfo(DXGI_FORMAT_R8G8B8A8_UNORM,       DXGI_FORMAT_R8G8B8A8_UNORM,      DXGI_FORMAT_R8G8B8A8_UNORM,      DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGB5_A1,           D3D11ES3FormatInfo(DXGI_FORMAT_R8G8B8A8_UNORM,       DXGI_FORMAT_R8G8B8A8_UNORM,      DXGI_FORMAT_R8G8B8A8_UNORM,      DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGBA8,             D3D11ES3FormatInfo(DXGI_FORMAT_R8G8B8A8_UNORM,       DXGI_FORMAT_R8G8B8A8_UNORM,      DXGI_FORMAT_R8G8B8A8_UNORM,      DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGBA8_SNORM,       D3D11ES3FormatInfo(DXGI_FORMAT_R8G8B8A8_SNORM,       DXGI_FORMAT_R8G8B8A8_SNORM,      DXGI_FORMAT_UNKNOWN,             DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGB10_A2,          D3D11ES3FormatInfo(DXGI_FORMAT_R10G10B10A2_UNORM,    DXGI_FORMAT_R10G10B10A2_UNORM,   DXGI_FORMAT_R10G10B10A2_UNORM,   DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGB10_A2UI,        D3D11ES3FormatInfo(DXGI_FORMAT_R10G10B10A2_UINT,     DXGI_FORMAT_R10G10B10A2_UINT,    DXGI_FORMAT_R10G10B10A2_UINT,    DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_SRGB8,             D3D11ES3FormatInfo(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,  DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_UNKNOWN,             DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_SRGB8_ALPHA8,      D3D11ES3FormatInfo(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,  DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_R16F,              D3D11ES3FormatInfo(DXGI_FORMAT_R16_FLOAT,            DXGI_FORMAT_R16_FLOAT,           DXGI_FORMAT_R16_FLOAT,           DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RG16F,             D3D11ES3FormatInfo(DXGI_FORMAT_R16G16_FLOAT,         DXGI_FORMAT_R16G16_FLOAT,        DXGI_FORMAT_R16G16_FLOAT,        DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGB16F,            D3D11ES3FormatInfo(DXGI_FORMAT_R16G16B16A16_FLOAT,   DXGI_FORMAT_R16G16B16A16_FLOAT,  DXGI_FORMAT_R16G16B16A16_FLOAT,  DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGBA16F,           D3D11ES3FormatInfo(DXGI_FORMAT_R16G16B16A16_FLOAT,   DXGI_FORMAT_R16G16B16A16_FLOAT,  DXGI_FORMAT_R16G16B16A16_FLOAT,  DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_R32F,              D3D11ES3FormatInfo(DXGI_FORMAT_R32_FLOAT,            DXGI_FORMAT_R32_FLOAT,           DXGI_FORMAT_R32_FLOAT,           DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RG32F,             D3D11ES3FormatInfo(DXGI_FORMAT_R32G32_FLOAT,         DXGI_FORMAT_R32G32_FLOAT,        DXGI_FORMAT_R32G32_FLOAT,        DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGB32F,            D3D11ES3FormatInfo(DXGI_FORMAT_R32G32B32A32_FLOAT,   DXGI_FORMAT_R32G32B32A32_FLOAT,  DXGI_FORMAT_R32G32B32A32_FLOAT,  DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGBA32F,           D3D11ES3FormatInfo(DXGI_FORMAT_R32G32B32A32_FLOAT,   DXGI_FORMAT_R32G32B32A32_FLOAT,  DXGI_FORMAT_R32G32B32A32_FLOAT,  DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_R11F_G11F_B10F,    D3D11ES3FormatInfo(DXGI_FORMAT_R11G11B10_FLOAT,      DXGI_FORMAT_R11G11B10_FLOAT,     DXGI_FORMAT_R11G11B10_FLOAT,     DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGB9_E5,           D3D11ES3FormatInfo(DXGI_FORMAT_R9G9B9E5_SHAREDEXP,   DXGI_FORMAT_R9G9B9E5_SHAREDEXP,  DXGI_FORMAT_UNKNOWN,             DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_R8I,               D3D11ES3FormatInfo(DXGI_FORMAT_R8_SINT,              DXGI_FORMAT_R8_SINT,             DXGI_FORMAT_R8_SINT,             DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_R8UI,              D3D11ES3FormatInfo(DXGI_FORMAT_R8_UINT,              DXGI_FORMAT_R8_UINT,             DXGI_FORMAT_R8_UINT,             DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_R16I,              D3D11ES3FormatInfo(DXGI_FORMAT_R16_SINT,             DXGI_FORMAT_R16_SINT,            DXGI_FORMAT_R16_SINT,            DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_R16UI,             D3D11ES3FormatInfo(DXGI_FORMAT_R16_UINT,             DXGI_FORMAT_R16_UINT,            DXGI_FORMAT_R16_UINT,            DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_R32I,              D3D11ES3FormatInfo(DXGI_FORMAT_R32_SINT,             DXGI_FORMAT_R32_SINT,            DXGI_FORMAT_R32_SINT,            DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_R32UI,             D3D11ES3FormatInfo(DXGI_FORMAT_R32_UINT,             DXGI_FORMAT_R32_UINT,            DXGI_FORMAT_R32_UINT,            DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RG8I,              D3D11ES3FormatInfo(DXGI_FORMAT_R8G8_SINT,            DXGI_FORMAT_R8G8_SINT,           DXGI_FORMAT_R8G8_SINT,           DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RG8UI,             D3D11ES3FormatInfo(DXGI_FORMAT_R8G8_UINT,            DXGI_FORMAT_R8G8_UINT,           DXGI_FORMAT_R8G8_UINT,           DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RG16I,             D3D11ES3FormatInfo(DXGI_FORMAT_R16G16_SINT,          DXGI_FORMAT_R16G16_SINT,         DXGI_FORMAT_R16G16_SINT,         DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RG16UI,            D3D11ES3FormatInfo(DXGI_FORMAT_R16G16_UINT,          DXGI_FORMAT_R16G16_UINT,         DXGI_FORMAT_R16G16_UINT,         DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RG32I,             D3D11ES3FormatInfo(DXGI_FORMAT_R32G32_SINT,          DXGI_FORMAT_R32G32_SINT,         DXGI_FORMAT_R32G32_SINT,         DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RG32UI,            D3D11ES3FormatInfo(DXGI_FORMAT_R32G32_UINT,          DXGI_FORMAT_R32G32_UINT,         DXGI_FORMAT_R32G32_UINT,         DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGB8I,             D3D11ES3FormatInfo(DXGI_FORMAT_R8G8B8A8_SINT,        DXGI_FORMAT_R8G8B8A8_SINT,       DXGI_FORMAT_R8G8B8A8_SINT,       DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGB8UI,            D3D11ES3FormatInfo(DXGI_FORMAT_R8G8B8A8_UINT,        DXGI_FORMAT_R8G8B8A8_UINT,       DXGI_FORMAT_R8G8B8A8_UINT,       DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGB16I,            D3D11ES3FormatInfo(DXGI_FORMAT_R16G16B16A16_SINT,    DXGI_FORMAT_R16G16B16A16_SINT,   DXGI_FORMAT_R16G16B16A16_SINT,   DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGB16UI,           D3D11ES3FormatInfo(DXGI_FORMAT_R16G16B16A16_UINT,    DXGI_FORMAT_R16G16B16A16_UINT,   DXGI_FORMAT_R16G16B16A16_UINT,   DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGB32I,            D3D11ES3FormatInfo(DXGI_FORMAT_R32G32B32A32_SINT,    DXGI_FORMAT_R32G32B32A32_SINT,   DXGI_FORMAT_R32G32B32A32_SINT,   DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGB32UI,           D3D11ES3FormatInfo(DXGI_FORMAT_R32G32B32A32_UINT,    DXGI_FORMAT_R32G32B32A32_UINT,   DXGI_FORMAT_R32G32B32A32_UINT,   DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGBA8I,            D3D11ES3FormatInfo(DXGI_FORMAT_R8G8B8A8_SINT,        DXGI_FORMAT_R8G8B8A8_SINT,       DXGI_FORMAT_R8G8B8A8_SINT,       DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGBA8UI,           D3D11ES3FormatInfo(DXGI_FORMAT_R8G8B8A8_UINT,        DXGI_FORMAT_R8G8B8A8_UINT,       DXGI_FORMAT_R8G8B8A8_UINT,       DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGBA16I,           D3D11ES3FormatInfo(DXGI_FORMAT_R16G16B16A16_SINT,    DXGI_FORMAT_R16G16B16A16_SINT,   DXGI_FORMAT_R16G16B16A16_SINT,   DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGBA16UI,          D3D11ES3FormatInfo(DXGI_FORMAT_R16G16B16A16_UINT,    DXGI_FORMAT_R16G16B16A16_UINT,   DXGI_FORMAT_R16G16B16A16_UINT,   DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGBA32I,           D3D11ES3FormatInfo(DXGI_FORMAT_R32G32B32A32_SINT,    DXGI_FORMAT_R32G32B32A32_SINT,   DXGI_FORMAT_R32G32B32A32_SINT,   DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGBA32UI,          D3D11ES3FormatInfo(DXGI_FORMAT_R32G32B32A32_UINT,    DXGI_FORMAT_R32G32B32A32_UINT,   DXGI_FORMAT_R32G32B32A32_UINT,   DXGI_FORMAT_UNKNOWN)));

    // Unsized formats, TODO: Are types of float and half float allowed for the unsized types? Would it change the DXGI format?
    map.insert(D3D11ES3FormatPair(GL_ALPHA,             D3D11ES3FormatInfo(DXGI_FORMAT_A8_UNORM,             DXGI_FORMAT_A8_UNORM,            DXGI_FORMAT_A8_UNORM,            DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_LUMINANCE,         D3D11ES3FormatInfo(DXGI_FORMAT_R8G8B8A8_UNORM,       DXGI_FORMAT_R8G8B8A8_UNORM,      DXGI_FORMAT_R8G8B8A8_UNORM,      DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_LUMINANCE_ALPHA,   D3D11ES3FormatInfo(DXGI_FORMAT_R8G8B8A8_UNORM,       DXGI_FORMAT_R8G8B8A8_UNORM,      DXGI_FORMAT_R8G8B8A8_UNORM,      DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGB,               D3D11ES3FormatInfo(DXGI_FORMAT_R8G8B8A8_UNORM,       DXGI_FORMAT_R8G8B8A8_UNORM,      DXGI_FORMAT_R8G8B8A8_UNORM,      DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_RGBA,              D3D11ES3FormatInfo(DXGI_FORMAT_R8G8B8A8_UNORM,       DXGI_FORMAT_R8G8B8A8_UNORM,      DXGI_FORMAT_R8G8B8A8_UNORM,      DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_BGRA_EXT,          D3D11ES3FormatInfo(DXGI_FORMAT_B8G8R8A8_UNORM,       DXGI_FORMAT_B8G8R8A8_UNORM,      DXGI_FORMAT_B8G8R8A8_UNORM,      DXGI_FORMAT_UNKNOWN)));

    // From GL_EXT_texture_storage
    //                           | GL internal format       |                  | D3D11 texture format          | D3D11 SRV format                    | D3D11 RTV format              | D3D11 DSV format               |
    map.insert(D3D11ES3FormatPair(GL_ALPHA8_EXT,             D3D11ES3FormatInfo(DXGI_FORMAT_A8_UNORM,           DXGI_FORMAT_A8_UNORM,                 DXGI_FORMAT_A8_UNORM,           DXGI_FORMAT_UNKNOWN             )));
    map.insert(D3D11ES3FormatPair(GL_LUMINANCE8_EXT,         D3D11ES3FormatInfo(DXGI_FORMAT_R8G8B8A8_UNORM,     DXGI_FORMAT_R8G8B8A8_UNORM,           DXGI_FORMAT_R8G8B8A8_UNORM,     DXGI_FORMAT_UNKNOWN             )));
    map.insert(D3D11ES3FormatPair(GL_ALPHA32F_EXT,           D3D11ES3FormatInfo(DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,       DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_UNKNOWN             )));
    map.insert(D3D11ES3FormatPair(GL_LUMINANCE32F_EXT,       D3D11ES3FormatInfo(DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,       DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_UNKNOWN             )));
    map.insert(D3D11ES3FormatPair(GL_ALPHA16F_EXT,           D3D11ES3FormatInfo(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,       DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_UNKNOWN             )));
    map.insert(D3D11ES3FormatPair(GL_LUMINANCE16F_EXT,       D3D11ES3FormatInfo(DXGI_FORMAT_UNKNOWN,            DXGI_FORMAT_UNKNOWN,                  DXGI_FORMAT_UNKNOWN,            DXGI_FORMAT_UNKNOWN             )));
    map.insert(D3D11ES3FormatPair(GL_LUMINANCE8_ALPHA8_EXT,  D3D11ES3FormatInfo(DXGI_FORMAT_R8G8B8A8_UNORM,     DXGI_FORMAT_R8G8B8A8_UNORM,           DXGI_FORMAT_R8G8B8A8_UNORM,     DXGI_FORMAT_UNKNOWN             )));
    map.insert(D3D11ES3FormatPair(GL_LUMINANCE_ALPHA32F_EXT, D3D11ES3FormatInfo(DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,       DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_UNKNOWN             )));
    map.insert(D3D11ES3FormatPair(GL_LUMINANCE_ALPHA16F_EXT, D3D11ES3FormatInfo(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,       DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_UNKNOWN             )));
    map.insert(D3D11ES3FormatPair(GL_BGRA8_EXT,              D3D11ES3FormatInfo(DXGI_FORMAT_B8G8R8A8_UNORM,     DXGI_FORMAT_B8G8R8A8_UNORM,           DXGI_FORMAT_B8G8R8A8_UNORM,     DXGI_FORMAT_UNKNOWN             )));
    map.insert(D3D11ES3FormatPair(GL_BGRA4_ANGLEX,           D3D11ES3FormatInfo(DXGI_FORMAT_B8G8R8A8_UNORM,     DXGI_FORMAT_B8G8R8A8_UNORM,           DXGI_FORMAT_B8G8R8A8_UNORM,     DXGI_FORMAT_UNKNOWN             )));
    map.insert(D3D11ES3FormatPair(GL_BGR5_A1_ANGLEX,         D3D11ES3FormatInfo(DXGI_FORMAT_B8G8R8A8_UNORM,     DXGI_FORMAT_B8G8R8A8_UNORM,           DXGI_FORMAT_B8G8R8A8_UNORM,     DXGI_FORMAT_UNKNOWN             )));

    // Depth stencil formats
    map.insert(D3D11ES3FormatPair(GL_DEPTH_COMPONENT16,     D3D11ES3FormatInfo(DXGI_FORMAT_R16_TYPELESS,        DXGI_FORMAT_R16_UNORM,                DXGI_FORMAT_UNKNOWN,            DXGI_FORMAT_D16_UNORM           )));
    map.insert(D3D11ES3FormatPair(GL_DEPTH_COMPONENT24,     D3D11ES3FormatInfo(DXGI_FORMAT_R24G8_TYPELESS,      DXGI_FORMAT_R24_UNORM_X8_TYPELESS,    DXGI_FORMAT_UNKNOWN,            DXGI_FORMAT_D24_UNORM_S8_UINT   )));
    map.insert(D3D11ES3FormatPair(GL_DEPTH_COMPONENT32F,    D3D11ES3FormatInfo(DXGI_FORMAT_R32_TYPELESS,        DXGI_FORMAT_R32_FLOAT,                DXGI_FORMAT_UNKNOWN,            DXGI_FORMAT_D32_FLOAT           )));
    map.insert(D3D11ES3FormatPair(GL_DEPTH24_STENCIL8,      D3D11ES3FormatInfo(DXGI_FORMAT_R24G8_TYPELESS,      DXGI_FORMAT_R24_UNORM_X8_TYPELESS,    DXGI_FORMAT_UNKNOWN,            DXGI_FORMAT_D24_UNORM_S8_UINT   )));
    map.insert(D3D11ES3FormatPair(GL_DEPTH32F_STENCIL8,     D3D11ES3FormatInfo(DXGI_FORMAT_R32G8X24_TYPELESS,   DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS, DXGI_FORMAT_UNKNOWN,            DXGI_FORMAT_D32_FLOAT_S8X24_UINT)));
    map.insert(D3D11ES3FormatPair(GL_STENCIL_INDEX8,        D3D11ES3FormatInfo(DXGI_FORMAT_R24G8_TYPELESS,      DXGI_FORMAT_X24_TYPELESS_G8_UINT,     DXGI_FORMAT_UNKNOWN,            DXGI_FORMAT_D24_UNORM_S8_UINT   )));

    // From GL_ANGLE_depth_texture
    map.insert(D3D11ES3FormatPair(GL_DEPTH_COMPONENT32_OES, D3D11ES3FormatInfo(DXGI_FORMAT_R24G8_TYPELESS,      DXGI_FORMAT_R24_UNORM_X8_TYPELESS,    DXGI_FORMAT_UNKNOWN,             DXGI_FORMAT_D24_UNORM_S8_UINT  )));

    // Compressed formats, From ES 3.0.1 spec, table 3.16
    //                           | GL internal format                          |                  | D3D11 texture format | D3D11 SRV format     | D3D11 RTV format   | D3D11 DSV format  |
    map.insert(D3D11ES3FormatPair(GL_COMPRESSED_R11_EAC,                        D3D11ES3FormatInfo(DXGI_FORMAT_UNKNOWN,   DXGI_FORMAT_UNKNOWN,   DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_COMPRESSED_SIGNED_R11_EAC,                 D3D11ES3FormatInfo(DXGI_FORMAT_UNKNOWN,   DXGI_FORMAT_UNKNOWN,   DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_COMPRESSED_RG11_EAC,                       D3D11ES3FormatInfo(DXGI_FORMAT_UNKNOWN,   DXGI_FORMAT_UNKNOWN,   DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_COMPRESSED_SIGNED_RG11_EAC,                D3D11ES3FormatInfo(DXGI_FORMAT_UNKNOWN,   DXGI_FORMAT_UNKNOWN,   DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_COMPRESSED_RGB8_ETC2,                      D3D11ES3FormatInfo(DXGI_FORMAT_UNKNOWN,   DXGI_FORMAT_UNKNOWN,   DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_COMPRESSED_SRGB8_ETC2,                     D3D11ES3FormatInfo(DXGI_FORMAT_UNKNOWN,   DXGI_FORMAT_UNKNOWN,   DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,  D3D11ES3FormatInfo(DXGI_FORMAT_UNKNOWN,   DXGI_FORMAT_UNKNOWN,   DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, D3D11ES3FormatInfo(DXGI_FORMAT_UNKNOWN,   DXGI_FORMAT_UNKNOWN,   DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_COMPRESSED_RGBA8_ETC2_EAC,                 D3D11ES3FormatInfo(DXGI_FORMAT_UNKNOWN,   DXGI_FORMAT_UNKNOWN,   DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,          D3D11ES3FormatInfo(DXGI_FORMAT_UNKNOWN,   DXGI_FORMAT_UNKNOWN,   DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN)));

    // From GL_EXT_texture_compression_dxt1
    map.insert(D3D11ES3FormatPair(GL_COMPRESSED_RGB_S3TC_DXT1_EXT,              D3D11ES3FormatInfo(DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN)));
    map.insert(D3D11ES3FormatPair(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,             D3D11ES3FormatInfo(DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN)));

    // From GL_ANGLE_texture_compression_dxt3
    map.insert(D3D11ES3FormatPair(GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE,           D3D11ES3FormatInfo(DXGI_FORMAT_BC2_UNORM, DXGI_FORMAT_BC2_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN)));

    // From GL_ANGLE_texture_compression_dxt5
    map.insert(D3D11ES3FormatPair(GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE,           D3D11ES3FormatInfo(DXGI_FORMAT_BC3_UNORM, DXGI_FORMAT_BC3_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN)));

    return map;
}

static bool GetD3D11ES3FormatInfo(GLenum internalFormat, GLuint clientVersion, D3D11ES3FormatInfo *outFormatInfo)
{
    static const D3D11ES3FormatMap formatMap = BuildD3D11ES3FormatMap();
    D3D11ES3FormatMap::const_iterator iter = formatMap.find(internalFormat);
    if (iter != formatMap.end())
    {
        if (outFormatInfo)
        {
            *outFormatInfo = iter->second;
        }
        return true;
    }
    else
    {
        return false;
    }
}

// ES3 image loading functions vary based on the internal format and data type given,
// this map type determines the loading function from the internal format and type supplied
// to glTex*Image*D and the destination DXGI_FORMAT. Source formats and types are taken from
// Tables 3.2 and 3.3 of the ES 3 spec.
typedef std::pair<GLenum, GLenum> InternalFormatTypePair;
typedef std::pair<InternalFormatTypePair, LoadImageFunction> D3D11LoadFunctionPair;
typedef std::map<InternalFormatTypePair, LoadImageFunction> D3D11LoadFunctionMap;

static void UnimplementedLoadFunction(int width, int height, int depth,
                                      const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                                      void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    UNIMPLEMENTED();
}

static void UnreachableLoadFunction(int width, int height, int depth,
                                    const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                                    void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch)
{
    UNREACHABLE();
}

// A helper function to insert data into the D3D11LoadFunctionMap with fewer characters.
static inline void insertLoadFunction(D3D11LoadFunctionMap *map, GLenum internalFormat, GLenum type,
                                      LoadImageFunction loadFunc)
{
    map->insert(D3D11LoadFunctionPair(InternalFormatTypePair(internalFormat, type), loadFunc));
}

D3D11LoadFunctionMap buildD3D11LoadFunctionMap()
{
    D3D11LoadFunctionMap map;

    //                      | Internal format      | Type                             | Load function                       |
    insertLoadFunction(&map, GL_RGBA8,              GL_UNSIGNED_BYTE,                  loadToNative<GLubyte, 4>             );
    insertLoadFunction(&map, GL_RGB5_A1,            GL_UNSIGNED_BYTE,                  loadToNative<GLubyte, 4>             );
    insertLoadFunction(&map, GL_RGBA4,              GL_UNSIGNED_BYTE,                  loadToNative<GLubyte, 4>             );
    insertLoadFunction(&map, GL_SRGB8_ALPHA8,       GL_UNSIGNED_BYTE,                  loadToNative<GLubyte, 4>             );
    insertLoadFunction(&map, GL_RGBA8_SNORM,        GL_BYTE,                           loadToNative<GLbyte, 4>              );
    insertLoadFunction(&map, GL_RGBA4,              GL_UNSIGNED_SHORT_4_4_4_4,         loadRGBA4444DataToRGBA               );
    insertLoadFunction(&map, GL_RGB10_A2,           GL_UNSIGNED_INT_2_10_10_10_REV,    loadToNative<GLuint, 1>              );
    insertLoadFunction(&map, GL_RGB5_A1,            GL_UNSIGNED_SHORT_5_5_5_1,         loadRGBA5551DataToRGBA               );
    insertLoadFunction(&map, GL_RGB5_A1,            GL_UNSIGNED_INT_2_10_10_10_REV,    loadRGBA2101010ToRGBA                );
    insertLoadFunction(&map, GL_RGBA16F,            GL_HALF_FLOAT,                     loadToNative<GLhalf, 4>              );
    insertLoadFunction(&map, GL_RGBA32F,            GL_FLOAT,                          loadToNative<GLfloat, 4>             );
    insertLoadFunction(&map, GL_RGBA16F,            GL_FLOAT,                          loadFloatDataToHalfFloat<4>          );
    insertLoadFunction(&map, GL_RGBA8UI,            GL_UNSIGNED_BYTE,                  loadToNative<GLubyte, 4>             );
    insertLoadFunction(&map, GL_RGBA8I,             GL_BYTE,                           loadToNative<GLbyte, 4>              );
    insertLoadFunction(&map, GL_RGBA16UI,           GL_UNSIGNED_SHORT,                 loadToNative<GLushort, 4>            );
    insertLoadFunction(&map, GL_RGBA16I,            GL_SHORT,                          loadToNative<GLshort, 4>             );
    insertLoadFunction(&map, GL_RGBA32UI,           GL_UNSIGNED_INT,                   loadToNative<GLuint, 4>              );
    insertLoadFunction(&map, GL_RGBA32I,            GL_INT,                            loadToNative<GLint, 4>               );
    insertLoadFunction(&map, GL_RGB10_A2UI,         GL_UNSIGNED_INT_2_10_10_10_REV,    loadToNative<GLuint, 1>              );
    insertLoadFunction(&map, GL_RGB8,               GL_UNSIGNED_BYTE,                  loadRGBUByteDataToRGBA               );
    insertLoadFunction(&map, GL_RGB565,             GL_UNSIGNED_BYTE,                  loadToNative3To4<GLubyte, 0xFF>      );
    insertLoadFunction(&map, GL_SRGB8,              GL_UNSIGNED_BYTE,                  loadToNative3To4<GLubyte, 0xFF>      );
    insertLoadFunction(&map, GL_RGB8_SNORM,         GL_BYTE,                           loadRGBSByteDataToRGBA               );
    insertLoadFunction(&map, GL_RGB565,             GL_UNSIGNED_SHORT_5_6_5,           loadRGB565DataToRGBA                 );
    insertLoadFunction(&map, GL_R11F_G11F_B10F,     GL_UNSIGNED_INT_10F_11F_11F_REV,   loadToNative<GLuint, 1>              );
    insertLoadFunction(&map, GL_RGB9_E5,            GL_UNSIGNED_INT_5_9_9_9_REV,       loadToNative<GLuint, 1>              );
    insertLoadFunction(&map, GL_RGB16F,             GL_HALF_FLOAT,                     loadToNative3To4<GLhalf, gl::Float16One>);
    insertLoadFunction(&map, GL_R11F_G11F_B10F,     GL_HALF_FLOAT,                     loadRGBHalfFloatDataTo111110Float    );
    insertLoadFunction(&map, GL_RGB9_E5,            GL_HALF_FLOAT,                     loadRGBHalfFloatDataTo999E5          );
    insertLoadFunction(&map, GL_RGB32F,             GL_FLOAT,                          loadToNative3To4<GLfloat, gl::Float32One>);
    insertLoadFunction(&map, GL_RGB16F,             GL_FLOAT,                          loadFloatRGBDataToHalfFloatRGBA      );
    insertLoadFunction(&map, GL_R11F_G11F_B10F,     GL_FLOAT,                          loadRGBFloatDataTo111110Float        );
    insertLoadFunction(&map, GL_RGB9_E5,            GL_FLOAT,                          loadRGBFloatDataTo999E5              );
    insertLoadFunction(&map, GL_RGB8UI,             GL_UNSIGNED_BYTE,                  loadToNative3To4<GLubyte, 0x01>      );
    insertLoadFunction(&map, GL_RGB8I,              GL_BYTE,                           loadToNative3To4<GLbyte, 0x01>       );
    insertLoadFunction(&map, GL_RGB16UI,            GL_UNSIGNED_SHORT,                 loadToNative3To4<GLushort, 0x0001>   );
    insertLoadFunction(&map, GL_RGB16I,             GL_SHORT,                          loadToNative3To4<GLshort, 0x0001>    );
    insertLoadFunction(&map, GL_RGB32UI,            GL_UNSIGNED_INT,                   loadToNative3To4<GLuint, 0x00000001> );
    insertLoadFunction(&map, GL_RGB32I,             GL_INT,                            loadToNative3To4<GLint, 0x00000001>  );
    insertLoadFunction(&map, GL_RG8,                GL_UNSIGNED_BYTE,                  loadToNative<GLubyte, 2>             );
    insertLoadFunction(&map, GL_RG8_SNORM,          GL_BYTE,                           loadToNative<GLbyte, 2>              );
    insertLoadFunction(&map, GL_RG16F,              GL_HALF_FLOAT,                     loadToNative<GLhalf, 2>              );
    insertLoadFunction(&map, GL_RG32F,              GL_FLOAT,                          loadToNative<GLfloat, 2>             );
    insertLoadFunction(&map, GL_RG16F,              GL_FLOAT,                          loadFloatDataToHalfFloat<2>          );
    insertLoadFunction(&map, GL_RG8UI,              GL_UNSIGNED_BYTE,                  loadToNative<GLubyte, 2>             );
    insertLoadFunction(&map, GL_RG8I,               GL_BYTE,                           loadToNative<GLbyte, 2>              );
    insertLoadFunction(&map, GL_RG16UI,             GL_UNSIGNED_SHORT,                 loadToNative<GLushort, 2>            );
    insertLoadFunction(&map, GL_RG16I,              GL_SHORT,                          loadToNative<GLshort, 2>             );
    insertLoadFunction(&map, GL_RG32UI,             GL_UNSIGNED_INT,                   loadToNative<GLuint, 2>              );
    insertLoadFunction(&map, GL_RG32I,              GL_INT,                            loadToNative<GLint, 2>               );
    insertLoadFunction(&map, GL_R8,                 GL_UNSIGNED_BYTE,                  loadToNative<GLubyte, 1>             );
    insertLoadFunction(&map, GL_R8_SNORM,           GL_BYTE,                           loadToNative<GLbyte, 1>              );
    insertLoadFunction(&map, GL_R16F,               GL_HALF_FLOAT,                     loadToNative<GLhalf, 1>              );
    insertLoadFunction(&map, GL_R32F,               GL_FLOAT,                          loadToNative<GLfloat, 1>             );
    insertLoadFunction(&map, GL_R16F,               GL_FLOAT,                          loadFloatDataToHalfFloat<1>          );
    insertLoadFunction(&map, GL_R8UI,               GL_UNSIGNED_BYTE,                  loadToNative<GLubyte, 1>             );
    insertLoadFunction(&map, GL_R8I,                GL_BYTE,                           loadToNative<GLbyte, 1>              );
    insertLoadFunction(&map, GL_R16UI,              GL_UNSIGNED_SHORT,                 loadToNative<GLushort, 1>            );
    insertLoadFunction(&map, GL_R16I,               GL_SHORT,                          loadToNative<GLshort, 1>             );
    insertLoadFunction(&map, GL_R32UI,              GL_UNSIGNED_INT,                   loadToNative<GLuint, 1>              );
    insertLoadFunction(&map, GL_R32I,               GL_INT,                            loadToNative<GLint, 1>               );
    insertLoadFunction(&map, GL_DEPTH_COMPONENT16,  GL_UNSIGNED_SHORT,                 loadToNative<GLushort, 1>            );
    insertLoadFunction(&map, GL_DEPTH_COMPONENT24,  GL_UNSIGNED_INT,                   loadG8R24DataToR24G8                 );
    insertLoadFunction(&map, GL_DEPTH_COMPONENT16,  GL_UNSIGNED_INT,                   loadUintDataToUshort                 );
    insertLoadFunction(&map, GL_DEPTH_COMPONENT32F, GL_FLOAT,                          loadToNative<GLfloat, 1>             );
    insertLoadFunction(&map, GL_DEPTH24_STENCIL8,   GL_UNSIGNED_INT_24_8,              loadG8R24DataToR24G8                 );
    insertLoadFunction(&map, GL_DEPTH32F_STENCIL8,  GL_FLOAT_32_UNSIGNED_INT_24_8_REV, loadToNative<GLuint, 2>              );

    // Unsized formats
    // Load functions are unreachable because they are converted to sized internal formats based on
    // the format and type before loading takes place.
    insertLoadFunction(&map, GL_RGBA,               GL_UNSIGNED_BYTE,                  UnreachableLoadFunction              );
    insertLoadFunction(&map, GL_RGBA,               GL_UNSIGNED_SHORT_4_4_4_4,         UnreachableLoadFunction              );
    insertLoadFunction(&map, GL_RGBA,               GL_UNSIGNED_SHORT_5_5_5_1,         UnreachableLoadFunction              );
    insertLoadFunction(&map, GL_RGB,                GL_UNSIGNED_BYTE,                  UnreachableLoadFunction              );
    insertLoadFunction(&map, GL_RGB,                GL_UNSIGNED_SHORT_5_6_5,           UnreachableLoadFunction              );
    insertLoadFunction(&map, GL_LUMINANCE_ALPHA,    GL_UNSIGNED_BYTE,                  UnreachableLoadFunction              );
    insertLoadFunction(&map, GL_LUMINANCE,          GL_UNSIGNED_BYTE,                  UnreachableLoadFunction              );
    insertLoadFunction(&map, GL_ALPHA,              GL_UNSIGNED_BYTE,                  UnreachableLoadFunction              );

    // From GL_OES_texture_float
    insertLoadFunction(&map, GL_LUMINANCE_ALPHA,    GL_FLOAT,                          loadLuminanceAlphaFloatDataToRGBA    );
    insertLoadFunction(&map, GL_LUMINANCE,          GL_FLOAT,                          loadLuminanceFloatDataToRGB          );
    insertLoadFunction(&map, GL_ALPHA,              GL_FLOAT,                          loadAlphaFloatDataToRGBA             );

    // From GL_OES_texture_half_float
    insertLoadFunction(&map, GL_LUMINANCE_ALPHA,    GL_HALF_FLOAT,                     loadLuminanceAlphaHalfFloatDataToRGBA);
    insertLoadFunction(&map, GL_LUMINANCE,          GL_HALF_FLOAT,                     loadLuminanceHalfFloatDataToRGBA     );
    insertLoadFunction(&map, GL_ALPHA,              GL_HALF_FLOAT,                     loadAlphaHalfFloatDataToRGBA         );

    // From GL_EXT_texture_storage
    insertLoadFunction(&map, GL_ALPHA8_EXT,             GL_UNSIGNED_BYTE,              loadToNative<GLubyte, 1>             );
    insertLoadFunction(&map, GL_LUMINANCE8_EXT,         GL_UNSIGNED_BYTE,              loadLuminanceDataToBGRA              );
    insertLoadFunction(&map, GL_LUMINANCE8_ALPHA8_EXT,  GL_UNSIGNED_BYTE,              loadLuminanceAlphaDataToBGRA         );
    insertLoadFunction(&map, GL_ALPHA32F_EXT,           GL_FLOAT,                      loadAlphaFloatDataToRGBA             );
    insertLoadFunction(&map, GL_LUMINANCE32F_EXT,       GL_FLOAT,                      loadLuminanceFloatDataToRGB          );
    insertLoadFunction(&map, GL_LUMINANCE_ALPHA32F_EXT, GL_FLOAT,                      loadLuminanceAlphaFloatDataToRGBA    );
    insertLoadFunction(&map, GL_ALPHA16F_EXT,           GL_HALF_FLOAT,                 loadAlphaHalfFloatDataToRGBA         );
    insertLoadFunction(&map, GL_LUMINANCE16F_EXT,       GL_HALF_FLOAT,                 loadLuminanceHalfFloatDataToRGBA     );
    insertLoadFunction(&map, GL_LUMINANCE_ALPHA16F_EXT, GL_HALF_FLOAT,                 loadLuminanceAlphaHalfFloatDataToRGBA);

    insertLoadFunction(&map, GL_BGRA8_EXT,              GL_UNSIGNED_BYTE,                  loadToNative<GLubyte, 4>         );
    insertLoadFunction(&map, GL_BGRA4_ANGLEX,           GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT, loadRGBA4444DataToRGBA           );
    insertLoadFunction(&map, GL_BGRA4_ANGLEX,           GL_UNSIGNED_BYTE,                  loadToNative<GLubyte, 4>         );
    insertLoadFunction(&map, GL_BGR5_A1_ANGLEX,         GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, loadRGBA5551DataToRGBA           );
    insertLoadFunction(&map, GL_BGR5_A1_ANGLEX,         GL_UNSIGNED_BYTE,                  loadToNative<GLubyte, 4>         );

    // Compressed formats
    // From ES 3.0.1 spec, table 3.16
    //                      | Internal format                             | Type            | Load function                     |
    insertLoadFunction(&map, GL_COMPRESSED_R11_EAC,                        GL_UNSIGNED_BYTE, UnimplementedLoadFunction          );
    insertLoadFunction(&map, GL_COMPRESSED_R11_EAC,                        GL_UNSIGNED_BYTE, UnimplementedLoadFunction          );
    insertLoadFunction(&map, GL_COMPRESSED_SIGNED_R11_EAC,                 GL_UNSIGNED_BYTE, UnimplementedLoadFunction          );
    insertLoadFunction(&map, GL_COMPRESSED_RG11_EAC,                       GL_UNSIGNED_BYTE, UnimplementedLoadFunction          );
    insertLoadFunction(&map, GL_COMPRESSED_SIGNED_RG11_EAC,                GL_UNSIGNED_BYTE, UnimplementedLoadFunction          );
    insertLoadFunction(&map, GL_COMPRESSED_RGB8_ETC2,                      GL_UNSIGNED_BYTE, UnimplementedLoadFunction          );
    insertLoadFunction(&map, GL_COMPRESSED_SRGB8_ETC2,                     GL_UNSIGNED_BYTE, UnimplementedLoadFunction          );
    insertLoadFunction(&map, GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,  GL_UNSIGNED_BYTE, UnimplementedLoadFunction          );
    insertLoadFunction(&map, GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, GL_UNSIGNED_BYTE, UnimplementedLoadFunction          );
    insertLoadFunction(&map, GL_COMPRESSED_RGBA8_ETC2_EAC,                 GL_UNSIGNED_BYTE, UnimplementedLoadFunction          );
    insertLoadFunction(&map, GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,          GL_UNSIGNED_BYTE, UnimplementedLoadFunction          );

    // From GL_EXT_texture_compression_dxt1
    insertLoadFunction(&map, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,              GL_UNSIGNED_BYTE, loadCompressedBlockDataToNative<4, 4,  8>);
    insertLoadFunction(&map, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,             GL_UNSIGNED_BYTE, loadCompressedBlockDataToNative<4, 4,  8>);

    // From GL_ANGLE_texture_compression_dxt3
    insertLoadFunction(&map, GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE,           GL_UNSIGNED_BYTE, loadCompressedBlockDataToNative<4, 4, 16>);

    // From GL_ANGLE_texture_compression_dxt5
    insertLoadFunction(&map, GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE,           GL_UNSIGNED_BYTE, loadCompressedBlockDataToNative<4, 4, 16>);

    return map;
}

struct D3D11ES2FormatInfo
{
    DXGI_FORMAT mTexFormat;
    DXGI_FORMAT mSRVFormat;
    DXGI_FORMAT mRTVFormat;
    DXGI_FORMAT mDSVFormat;

    LoadImageFunction mLoadImageFunction;

    D3D11ES2FormatInfo()
        : mTexFormat(DXGI_FORMAT_UNKNOWN), mDSVFormat(DXGI_FORMAT_UNKNOWN), mRTVFormat(DXGI_FORMAT_UNKNOWN),
        mSRVFormat(DXGI_FORMAT_UNKNOWN), mLoadImageFunction(NULL)
    { }

    D3D11ES2FormatInfo(DXGI_FORMAT texFormat, DXGI_FORMAT srvFormat, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat,
        LoadImageFunction loadFunc)
        : mTexFormat(texFormat), mDSVFormat(dsvFormat), mRTVFormat(rtvFormat), mSRVFormat(srvFormat),
        mLoadImageFunction(loadFunc)
    { }
};

// ES2 internal formats can map to DXGI formats and loading functions
typedef std::pair<GLenum, D3D11ES2FormatInfo> D3D11ES2FormatPair;
typedef std::map<GLenum, D3D11ES2FormatInfo> D3D11ES2FormatMap;

static D3D11ES2FormatMap BuildD3D11ES2FormatMap()
{
    D3D11ES2FormatMap map;

    //                           | Internal format                   |                  | Texture format                | SRV format                       | RTV format                    | DSV format                   | Load function                           |
    map.insert(D3D11ES2FormatPair(GL_NONE,                            D3D11ES2FormatInfo(DXGI_FORMAT_UNKNOWN,            DXGI_FORMAT_UNKNOWN,               DXGI_FORMAT_UNKNOWN,            DXGI_FORMAT_UNKNOWN,           UnreachableLoadFunction                  )));
    map.insert(D3D11ES2FormatPair(GL_DEPTH_COMPONENT16,               D3D11ES2FormatInfo(DXGI_FORMAT_R16_TYPELESS,       DXGI_FORMAT_R16_UNORM,             DXGI_FORMAT_UNKNOWN,            DXGI_FORMAT_D16_UNORM,         UnreachableLoadFunction                  )));
    map.insert(D3D11ES2FormatPair(GL_DEPTH_COMPONENT32_OES,           D3D11ES2FormatInfo(DXGI_FORMAT_R32_TYPELESS,       DXGI_FORMAT_R32_FLOAT,             DXGI_FORMAT_UNKNOWN,            DXGI_FORMAT_D32_FLOAT,         UnreachableLoadFunction                  )));
    map.insert(D3D11ES2FormatPair(GL_DEPTH24_STENCIL8_OES,            D3D11ES2FormatInfo(DXGI_FORMAT_R24G8_TYPELESS,     DXGI_FORMAT_R24_UNORM_X8_TYPELESS, DXGI_FORMAT_UNKNOWN,            DXGI_FORMAT_D24_UNORM_S8_UINT, UnreachableLoadFunction                  )));
    map.insert(D3D11ES2FormatPair(GL_STENCIL_INDEX8,                  D3D11ES2FormatInfo(DXGI_FORMAT_R24G8_TYPELESS,     DXGI_FORMAT_X24_TYPELESS_G8_UINT,  DXGI_FORMAT_UNKNOWN,            DXGI_FORMAT_D24_UNORM_S8_UINT, UnreachableLoadFunction                  )));

    map.insert(D3D11ES2FormatPair(GL_RGBA32F_EXT,                     D3D11ES2FormatInfo(DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,    DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_UNKNOWN,           loadRGBAFloatDataToRGBA                  )));
    map.insert(D3D11ES2FormatPair(GL_RGB32F_EXT,                      D3D11ES2FormatInfo(DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,    DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_UNKNOWN,           loadRGBFloatDataToRGBA                   )));
    map.insert(D3D11ES2FormatPair(GL_ALPHA32F_EXT,                    D3D11ES2FormatInfo(DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,    DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_UNKNOWN,           loadAlphaFloatDataToRGBA                 )));
    map.insert(D3D11ES2FormatPair(GL_LUMINANCE32F_EXT,                D3D11ES2FormatInfo(DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,    DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_UNKNOWN,           loadLuminanceFloatDataToRGBA             )));
    map.insert(D3D11ES2FormatPair(GL_LUMINANCE_ALPHA32F_EXT,          D3D11ES2FormatInfo(DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,    DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_UNKNOWN,           loadLuminanceAlphaFloatDataToRGBA        )));

    map.insert(D3D11ES2FormatPair(GL_RGBA16F_EXT,                     D3D11ES2FormatInfo(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,    DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_UNKNOWN,           loadRGBAHalfFloatDataToRGBA              )));
    map.insert(D3D11ES2FormatPair(GL_RGB16F_EXT,                      D3D11ES2FormatInfo(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,    DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_UNKNOWN,           loadRGBHalfFloatDataToRGBA               )));
    map.insert(D3D11ES2FormatPair(GL_ALPHA16F_EXT,                    D3D11ES2FormatInfo(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,    DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_UNKNOWN,           loadAlphaHalfFloatDataToRGBA             )));
    map.insert(D3D11ES2FormatPair(GL_LUMINANCE16F_EXT,                D3D11ES2FormatInfo(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,    DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_UNKNOWN,           loadLuminanceHalfFloatDataToRGBA         )));
    map.insert(D3D11ES2FormatPair(GL_LUMINANCE_ALPHA16F_EXT,          D3D11ES2FormatInfo(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,    DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_UNKNOWN,           loadLuminanceAlphaHalfFloatDataToRGBA    )));

    map.insert(D3D11ES2FormatPair(GL_ALPHA8_EXT,                      D3D11ES2FormatInfo(DXGI_FORMAT_A8_UNORM,           DXGI_FORMAT_A8_UNORM,              DXGI_FORMAT_A8_UNORM,           DXGI_FORMAT_UNKNOWN,           loadAlphaDataToNative                    )));
    map.insert(D3D11ES2FormatPair(GL_LUMINANCE8_EXT,                  D3D11ES2FormatInfo(DXGI_FORMAT_R8G8B8A8_UNORM,     DXGI_FORMAT_R8G8B8A8_UNORM,        DXGI_FORMAT_R8G8B8A8_UNORM,     DXGI_FORMAT_UNKNOWN,           loadLuminanceDataToBGRA                  )));
    map.insert(D3D11ES2FormatPair(GL_LUMINANCE8_ALPHA8_EXT,           D3D11ES2FormatInfo(DXGI_FORMAT_R8G8B8A8_UNORM,     DXGI_FORMAT_R8G8B8A8_UNORM,        DXGI_FORMAT_R8G8B8A8_UNORM,     DXGI_FORMAT_UNKNOWN,           loadLuminanceAlphaDataToBGRA             )));

    map.insert(D3D11ES2FormatPair(GL_RGB8_OES,                        D3D11ES2FormatInfo(DXGI_FORMAT_R8G8B8A8_UNORM,     DXGI_FORMAT_R8G8B8A8_UNORM,        DXGI_FORMAT_R8G8B8A8_UNORM,     DXGI_FORMAT_UNKNOWN,           loadRGBUByteDataToRGBA                   )));
    map.insert(D3D11ES2FormatPair(GL_RGB565,                          D3D11ES2FormatInfo(DXGI_FORMAT_R8G8B8A8_UNORM,     DXGI_FORMAT_R8G8B8A8_UNORM,        DXGI_FORMAT_R8G8B8A8_UNORM,     DXGI_FORMAT_UNKNOWN,           loadRGB565DataToRGBA                     )));
    map.insert(D3D11ES2FormatPair(GL_RGBA8_OES,                       D3D11ES2FormatInfo(DXGI_FORMAT_R8G8B8A8_UNORM,     DXGI_FORMAT_R8G8B8A8_UNORM,        DXGI_FORMAT_R8G8B8A8_UNORM,     DXGI_FORMAT_UNKNOWN,           loadRGBAUByteDataToNative                )));
    map.insert(D3D11ES2FormatPair(GL_RGBA4,                           D3D11ES2FormatInfo(DXGI_FORMAT_R8G8B8A8_UNORM,     DXGI_FORMAT_R8G8B8A8_UNORM,        DXGI_FORMAT_R8G8B8A8_UNORM,     DXGI_FORMAT_UNKNOWN,           loadRGBA4444DataToRGBA                   )));
    map.insert(D3D11ES2FormatPair(GL_RGB5_A1,                         D3D11ES2FormatInfo(DXGI_FORMAT_R8G8B8A8_UNORM,     DXGI_FORMAT_R8G8B8A8_UNORM,        DXGI_FORMAT_R8G8B8A8_UNORM,     DXGI_FORMAT_UNKNOWN,           loadRGBA5551DataToRGBA                   )));
    map.insert(D3D11ES2FormatPair(GL_BGRA8_EXT,                       D3D11ES2FormatInfo(DXGI_FORMAT_B8G8R8A8_UNORM,     DXGI_FORMAT_B8G8R8A8_UNORM,        DXGI_FORMAT_B8G8R8A8_UNORM,     DXGI_FORMAT_UNKNOWN,           loadBGRADataToBGRA                       )));
    map.insert(D3D11ES2FormatPair(GL_BGRA4_ANGLEX,                    D3D11ES2FormatInfo(DXGI_FORMAT_B8G8R8A8_UNORM,     DXGI_FORMAT_B8G8R8A8_UNORM,        DXGI_FORMAT_B8G8R8A8_UNORM,     DXGI_FORMAT_UNKNOWN,           loadRGBA4444DataToRGBA                   )));
    map.insert(D3D11ES2FormatPair(GL_BGR5_A1_ANGLEX,                  D3D11ES2FormatInfo(DXGI_FORMAT_B8G8R8A8_UNORM,     DXGI_FORMAT_B8G8R8A8_UNORM,        DXGI_FORMAT_B8G8R8A8_UNORM,     DXGI_FORMAT_UNKNOWN,           loadRGBA5551DataToRGBA                   )));

    // From GL_EXT_texture_rg
    map.insert(D3D11ES2FormatPair(GL_R8_EXT,                          D3D11ES2FormatInfo(DXGI_FORMAT_R8_UNORM,           DXGI_FORMAT_R8_UNORM,              DXGI_FORMAT_R8_UNORM,           DXGI_FORMAT_UNKNOWN,           loadToNative<GLubyte, 1>                 )));
    map.insert(D3D11ES2FormatPair(GL_R32F_EXT,                        D3D11ES2FormatInfo(DXGI_FORMAT_R32_FLOAT,          DXGI_FORMAT_R32_FLOAT,             DXGI_FORMAT_R32_FLOAT,          DXGI_FORMAT_UNKNOWN,           loadToNative<GLfloat, 1>                 )));
    map.insert(D3D11ES2FormatPair(GL_R16F_EXT,                        D3D11ES2FormatInfo(DXGI_FORMAT_R16_FLOAT,          DXGI_FORMAT_R16_FLOAT,             DXGI_FORMAT_R16_FLOAT,          DXGI_FORMAT_UNKNOWN,           loadToNative<GLhalf, 1>                  )));
    map.insert(D3D11ES2FormatPair(GL_RG8_EXT,                         D3D11ES2FormatInfo(DXGI_FORMAT_R8G8_UNORM,         DXGI_FORMAT_R8G8_UNORM,            DXGI_FORMAT_R8G8_UNORM,         DXGI_FORMAT_UNKNOWN,           loadToNative<GLubyte, 2>                 )));
    map.insert(D3D11ES2FormatPair(GL_RG32F_EXT,                       D3D11ES2FormatInfo(DXGI_FORMAT_R32G32_FLOAT,       DXGI_FORMAT_R32G32_FLOAT,          DXGI_FORMAT_R32G32_FLOAT,       DXGI_FORMAT_UNKNOWN,           loadToNative<GLfloat, 2>                 )));
    map.insert(D3D11ES2FormatPair(GL_RG16F_EXT,                       D3D11ES2FormatInfo(DXGI_FORMAT_R16G16_FLOAT,       DXGI_FORMAT_R16G16_FLOAT,          DXGI_FORMAT_R16G16_FLOAT,       DXGI_FORMAT_UNKNOWN,           loadToNative<GLhalf, 2>                  )));

    map.insert(D3D11ES2FormatPair(GL_COMPRESSED_RGB_S3TC_DXT1_EXT,    D3D11ES2FormatInfo(DXGI_FORMAT_BC1_UNORM,          DXGI_FORMAT_BC1_UNORM,             DXGI_FORMAT_UNKNOWN,            DXGI_FORMAT_UNKNOWN,           loadCompressedBlockDataToNative<4, 4,  8>)));
    map.insert(D3D11ES2FormatPair(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,   D3D11ES2FormatInfo(DXGI_FORMAT_BC1_UNORM,          DXGI_FORMAT_BC1_UNORM,             DXGI_FORMAT_UNKNOWN,            DXGI_FORMAT_UNKNOWN,           loadCompressedBlockDataToNative<4, 4,  8>)));
    map.insert(D3D11ES2FormatPair(GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE, D3D11ES2FormatInfo(DXGI_FORMAT_BC2_UNORM,          DXGI_FORMAT_BC2_UNORM,             DXGI_FORMAT_UNKNOWN,            DXGI_FORMAT_UNKNOWN,           loadCompressedBlockDataToNative<4, 4, 16>)));
    map.insert(D3D11ES2FormatPair(GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE, D3D11ES2FormatInfo(DXGI_FORMAT_BC3_UNORM,          DXGI_FORMAT_BC3_UNORM,             DXGI_FORMAT_UNKNOWN,            DXGI_FORMAT_UNKNOWN,           loadCompressedBlockDataToNative<4, 4, 16>)));

    return map;
}

static bool GetD3D11ES2FormatInfo(GLenum internalFormat, GLuint clientVersion, D3D11ES2FormatInfo *outFormatInfo)
{
    static const D3D11ES2FormatMap formatMap = BuildD3D11ES2FormatMap();
    D3D11ES2FormatMap::const_iterator iter = formatMap.find(internalFormat);
    if (iter != formatMap.end())
    {
        if (outFormatInfo)
        {
            *outFormatInfo = iter->second;
        }
        return true;
    }
    else
    {
        return false;
    }
}

// A map to determine the pixel size and mipmap generation function of a given DXGI format
struct DXGIFormatInfo
{
    GLuint mPixelBits;
    GLuint mBlockWidth;
    GLuint mBlockHeight;
    GLenum mComponentType;

    MipGenerationFunction mMipGenerationFunction;
    ColorReadFunction mColorReadFunction;

    DXGIFormatInfo()
        : mPixelBits(0), mBlockWidth(0), mBlockHeight(0), mComponentType(GL_NONE), mMipGenerationFunction(NULL),
          mColorReadFunction(NULL)
    { }

    DXGIFormatInfo(GLuint pixelBits, GLuint blockWidth, GLuint blockHeight, GLenum componentType,
                   MipGenerationFunction mipFunc, ColorReadFunction readFunc)
        : mPixelBits(pixelBits), mBlockWidth(blockWidth), mBlockHeight(blockHeight), mComponentType(componentType),
          mMipGenerationFunction(mipFunc), mColorReadFunction(readFunc)
    { }
};

typedef std::map<DXGI_FORMAT, DXGIFormatInfo> DXGIFormatInfoMap;

void AddDXGIFormat(DXGIFormatInfoMap *map, DXGI_FORMAT dxgiFormat, GLuint pixelBits, GLuint blockWidth, GLuint blockHeight,
                   GLenum componentType, MipGenerationFunction mipFunc, ColorReadFunction readFunc)
{
    map->insert(std::make_pair(dxgiFormat, DXGIFormatInfo(pixelBits, blockWidth, blockHeight, componentType, mipFunc, readFunc)));
}

static DXGIFormatInfoMap BuildDXGIFormatInfoMap()
{
    DXGIFormatInfoMap map;

    //                | DXGI format                          |S   |W |H |Component Type        | Mip generation function   | Color read function
    AddDXGIFormat(&map, DXGI_FORMAT_UNKNOWN,                  0,   0, 0, GL_NONE,                NULL,                       NULL);

    AddDXGIFormat(&map, DXGI_FORMAT_A8_UNORM,                 8,   1, 1, GL_UNSIGNED_NORMALIZED, GenerateMip<A8>,            ReadColor<A8, GLfloat>);
    AddDXGIFormat(&map, DXGI_FORMAT_R8_UNORM,                 8,   1, 1, GL_UNSIGNED_NORMALIZED, GenerateMip<R8>,            ReadColor<R8, GLfloat>);
    AddDXGIFormat(&map, DXGI_FORMAT_R8G8_UNORM,               16,  1, 1, GL_UNSIGNED_NORMALIZED, GenerateMip<R8G8>,          ReadColor<R8G8, GLfloat>);
    AddDXGIFormat(&map, DXGI_FORMAT_R8G8B8A8_UNORM,           32,  1, 1, GL_UNSIGNED_NORMALIZED, GenerateMip<R8G8B8A8>,      ReadColor<R8G8B8A8, GLfloat>);
    AddDXGIFormat(&map, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,      32,  1, 1, GL_UNSIGNED_NORMALIZED, GenerateMip<R8G8B8A8>,      ReadColor<R8G8B8A8, GLfloat>);
    AddDXGIFormat(&map, DXGI_FORMAT_B8G8R8A8_UNORM,           32,  1, 1, GL_UNSIGNED_NORMALIZED, GenerateMip<B8G8R8A8>,      ReadColor<B8G8R8A8, GLfloat>);

    AddDXGIFormat(&map, DXGI_FORMAT_R8_SNORM,                 8,   1, 1, GL_SIGNED_NORMALIZED,   GenerateMip<R8S>,           ReadColor<R8S, GLfloat>);
    AddDXGIFormat(&map, DXGI_FORMAT_R8G8_SNORM,               16,  1, 1, GL_SIGNED_NORMALIZED,   GenerateMip<R8G8S>,         ReadColor<R8G8S, GLfloat>);
    AddDXGIFormat(&map, DXGI_FORMAT_R8G8B8A8_SNORM,           32,  1, 1, GL_SIGNED_NORMALIZED,   GenerateMip<R8G8B8A8S>,     ReadColor<R8G8B8A8S, GLfloat>);

    AddDXGIFormat(&map, DXGI_FORMAT_R8_UINT,                  8,   1, 1, GL_UNSIGNED_INT,        GenerateMip<R8>,            ReadColor<R8, GLuint>);
    AddDXGIFormat(&map, DXGI_FORMAT_R16_UINT,                 16,  1, 1, GL_UNSIGNED_INT,        GenerateMip<R16>,           ReadColor<R16, GLuint>);
    AddDXGIFormat(&map, DXGI_FORMAT_R32_UINT,                 32,  1, 1, GL_UNSIGNED_INT,        GenerateMip<R32>,           ReadColor<R32, GLuint>);
    AddDXGIFormat(&map, DXGI_FORMAT_R8G8_UINT,                16,  1, 1, GL_UNSIGNED_INT,        GenerateMip<R8G8>,          ReadColor<R8G8, GLuint>);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16_UINT,              32,  1, 1, GL_UNSIGNED_INT,        GenerateMip<R16G16>,        ReadColor<R16G16, GLuint>);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G32_UINT,              64,  1, 1, GL_UNSIGNED_INT,        GenerateMip<R32G32>,        ReadColor<R32G32, GLuint>);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G32B32_UINT,           96,  1, 1, GL_UNSIGNED_INT,        GenerateMip<R32G32B32>,     ReadColor<R32G32B32, GLuint>);
    AddDXGIFormat(&map, DXGI_FORMAT_R8G8B8A8_UINT,            32,  1, 1, GL_UNSIGNED_INT,        GenerateMip<R8G8B8A8>,      ReadColor<R8G8B8A8, GLuint>);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16B16A16_UINT,        64,  1, 1, GL_UNSIGNED_INT,        GenerateMip<R16G16B16A16>,  ReadColor<R16G16B16A16, GLuint>);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G32B32A32_UINT,        128, 1, 1, GL_UNSIGNED_INT,        GenerateMip<R32G32B32A32>,  ReadColor<R32G32B32A32, GLuint>);

    AddDXGIFormat(&map, DXGI_FORMAT_R8_SINT,                  8,   1, 1, GL_INT,                 GenerateMip<R8S>,           ReadColor<R8S, GLint>);
    AddDXGIFormat(&map, DXGI_FORMAT_R16_SINT,                 16,  1, 1, GL_INT,                 GenerateMip<R16S>,          ReadColor<R16S, GLint>);
    AddDXGIFormat(&map, DXGI_FORMAT_R32_SINT,                 32,  1, 1, GL_INT,                 GenerateMip<R32S>,          ReadColor<R32S, GLint>);
    AddDXGIFormat(&map, DXGI_FORMAT_R8G8_SINT,                16,  1, 1, GL_INT,                 GenerateMip<R8G8S>,         ReadColor<R8G8S, GLint>);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16_SINT,              32,  1, 1, GL_INT,                 GenerateMip<R16G16S>,       ReadColor<R16G16S, GLint>);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G32_SINT,              64,  1, 1, GL_INT,                 GenerateMip<R32G32S>,       ReadColor<R32G32S, GLint>);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G32B32_SINT,           96,  1, 1, GL_INT,                 GenerateMip<R32G32B32S>,    ReadColor<R32G32B32S, GLint>);
    AddDXGIFormat(&map, DXGI_FORMAT_R8G8B8A8_SINT,            32,  1, 1, GL_INT,                 GenerateMip<R8G8B8A8S>,     ReadColor<R8G8B8A8S, GLint>);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16B16A16_SINT,        64,  1, 1, GL_INT,                 GenerateMip<R16G16B16A16S>, ReadColor<R16G16B16A16S, GLint>);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G32B32A32_SINT,        128, 1, 1, GL_INT,                 GenerateMip<R32G32B32A32S>, ReadColor<R32G32B32A32S, GLint>);

    AddDXGIFormat(&map, DXGI_FORMAT_R10G10B10A2_UNORM,        32,  1, 1, GL_UNSIGNED_NORMALIZED, GenerateMip<R10G10B10A2>,   ReadColor<R10G10B10A2, GLfloat>);
    AddDXGIFormat(&map, DXGI_FORMAT_R10G10B10A2_UINT,         32,  1, 1, GL_UNSIGNED_INT,        GenerateMip<R10G10B10A2>,   ReadColor<R10G10B10A2, GLuint>);

    AddDXGIFormat(&map, DXGI_FORMAT_R16_FLOAT,                16,  1, 1, GL_FLOAT,               GenerateMip<R16F>,          ReadColor<R16F, GLfloat>);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16_FLOAT,             32,  1, 1, GL_FLOAT,               GenerateMip<R16G16F>,       ReadColor<R16G16F, GLfloat>);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16B16A16_FLOAT,       64,  1, 1, GL_FLOAT,               GenerateMip<R16G16B16A16F>, ReadColor<R16G16B16A16F, GLfloat>);

    AddDXGIFormat(&map, DXGI_FORMAT_R32_FLOAT,                32,  1, 1, GL_FLOAT,               GenerateMip<R32F>,          ReadColor<R32F, GLfloat>);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G32_FLOAT,             64,  1, 1, GL_FLOAT,               GenerateMip<R32G32F>,       ReadColor<R32G32F, GLfloat>);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G32B32_FLOAT,          96,  1, 1, GL_FLOAT,               NULL,                       NULL);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G32B32A32_FLOAT,       128, 1, 1, GL_FLOAT,               GenerateMip<R32G32B32A32F>, ReadColor<R32G32B32A32F, GLfloat>);

    AddDXGIFormat(&map, DXGI_FORMAT_R9G9B9E5_SHAREDEXP,       32,  1, 1, GL_FLOAT,               GenerateMip<R9G9B9E5>,      ReadColor<R9G9B9E5, GLfloat>);
    AddDXGIFormat(&map, DXGI_FORMAT_R11G11B10_FLOAT,          32,  1, 1, GL_FLOAT,               GenerateMip<R11G11B10F>,    ReadColor<R11G11B10F, GLfloat>);

    AddDXGIFormat(&map, DXGI_FORMAT_R16_TYPELESS,             16,  1, 1, GL_NONE,                NULL,                       NULL);
    AddDXGIFormat(&map, DXGI_FORMAT_R16_UNORM,                16,  1, 1, GL_UNSIGNED_NORMALIZED, NULL,                       NULL);
    AddDXGIFormat(&map, DXGI_FORMAT_D16_UNORM,                16,  1, 1, GL_UNSIGNED_NORMALIZED, NULL,                       NULL);
    AddDXGIFormat(&map, DXGI_FORMAT_R24G8_TYPELESS,           32,  1, 1, GL_NONE,                NULL,                       NULL);
    AddDXGIFormat(&map, DXGI_FORMAT_R24_UNORM_X8_TYPELESS,    32,  1, 1, GL_NONE,                NULL,                       NULL);
    AddDXGIFormat(&map, DXGI_FORMAT_D24_UNORM_S8_UINT,        32,  1, 1, GL_UNSIGNED_INT,        NULL,                       NULL);
    AddDXGIFormat(&map, DXGI_FORMAT_R32G8X24_TYPELESS,        64,  1, 1, GL_NONE,                NULL,                       NULL);
    AddDXGIFormat(&map, DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS, 64,  1, 1, GL_NONE,                NULL,                       NULL);
    AddDXGIFormat(&map, DXGI_FORMAT_D32_FLOAT_S8X24_UINT,     64,  1, 1, GL_UNSIGNED_INT,        NULL,                       NULL);
    AddDXGIFormat(&map, DXGI_FORMAT_R32_TYPELESS,             32,  1, 1, GL_NONE,                NULL,                       NULL);
    AddDXGIFormat(&map, DXGI_FORMAT_D32_FLOAT,                32,  1, 1, GL_FLOAT,               NULL,                       NULL);

    AddDXGIFormat(&map, DXGI_FORMAT_BC1_UNORM,                64,  4, 4, GL_UNSIGNED_NORMALIZED, NULL,                       NULL);
    AddDXGIFormat(&map, DXGI_FORMAT_BC2_UNORM,                128, 4, 4, GL_UNSIGNED_NORMALIZED, NULL,                       NULL);
    AddDXGIFormat(&map, DXGI_FORMAT_BC3_UNORM,                128, 4, 4, GL_UNSIGNED_NORMALIZED, NULL,                       NULL);

    // Useful formats for vertex buffers
    AddDXGIFormat(&map, DXGI_FORMAT_R16_UNORM,                16,  1, 1, GL_UNSIGNED_NORMALIZED, NULL,                       NULL);
    AddDXGIFormat(&map, DXGI_FORMAT_R16_SNORM,                16,  1, 1, GL_SIGNED_NORMALIZED,   NULL,                       NULL);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16_UNORM,             32,  1, 1, GL_UNSIGNED_NORMALIZED, NULL,                       NULL);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16_SNORM,             32,  1, 1, GL_SIGNED_NORMALIZED,   NULL,                       NULL);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16B16A16_UNORM,       64,  1, 1, GL_UNSIGNED_NORMALIZED, NULL,                       NULL);
    AddDXGIFormat(&map, DXGI_FORMAT_R16G16B16A16_SNORM,       64,  1, 1, GL_SIGNED_NORMALIZED,   NULL,                       NULL);

    return map;
}

typedef std::map<DXGI_FORMAT, GLenum> DXGIToESFormatMap;

inline void AddDXGIToESEntry(DXGIToESFormatMap *map, DXGI_FORMAT key, GLenum value)
{
    map->insert(std::make_pair(key, value));
}

static DXGIToESFormatMap BuildCommonDXGIToESFormatMap()
{
    DXGIToESFormatMap map;

    AddDXGIToESEntry(&map, DXGI_FORMAT_UNKNOWN,                  GL_NONE);

    AddDXGIToESEntry(&map, DXGI_FORMAT_A8_UNORM,                 GL_ALPHA8_EXT);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R8_UNORM,                 GL_R8);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R8G8_UNORM,               GL_RG8);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R8G8B8A8_UNORM,           GL_RGBA8);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,      GL_SRGB8_ALPHA8);
    AddDXGIToESEntry(&map, DXGI_FORMAT_B8G8R8A8_UNORM,           GL_BGRA8_EXT);

    AddDXGIToESEntry(&map, DXGI_FORMAT_R8_SNORM,                 GL_R8_SNORM);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R8G8_SNORM,               GL_RG8_SNORM);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R8G8B8A8_SNORM,           GL_RGBA8_SNORM);

    AddDXGIToESEntry(&map, DXGI_FORMAT_R8_UINT,                  GL_R8UI);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R16_UINT,                 GL_R16UI);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R32_UINT,                 GL_R32UI);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R8G8_UINT,                GL_RG8UI);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R16G16_UINT,              GL_RG16UI);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R32G32_UINT,              GL_RG32UI);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R32G32B32_UINT,           GL_RGB32UI);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R8G8B8A8_UINT,            GL_RGBA8UI);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R16G16B16A16_UINT,        GL_RGBA16UI);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R32G32B32A32_UINT,        GL_RGBA32UI);

    AddDXGIToESEntry(&map, DXGI_FORMAT_R8_SINT,                  GL_R8I);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R16_SINT,                 GL_R16I);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R32_SINT,                 GL_R32I);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R8G8_SINT,                GL_RG8I);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R16G16_SINT,              GL_RG16I);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R32G32_SINT,              GL_RG32I);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R32G32B32_SINT,           GL_RGB32I);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R8G8B8A8_SINT,            GL_RGBA8I);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R16G16B16A16_SINT,        GL_RGBA16I);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R32G32B32A32_SINT,        GL_RGBA32I);

    AddDXGIToESEntry(&map, DXGI_FORMAT_R10G10B10A2_UNORM,        GL_RGB10_A2);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R10G10B10A2_UINT,         GL_RGB10_A2UI);

    AddDXGIToESEntry(&map, DXGI_FORMAT_R16_FLOAT,                GL_R16F);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R16G16_FLOAT,             GL_RG16F);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R16G16B16A16_FLOAT,       GL_RGBA16F);

    AddDXGIToESEntry(&map, DXGI_FORMAT_R32_FLOAT,                GL_R32F);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R32G32_FLOAT,             GL_RG32F);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R32G32B32_FLOAT,          GL_RGB32F);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R32G32B32A32_FLOAT,       GL_RGBA32F);

    AddDXGIToESEntry(&map, DXGI_FORMAT_R9G9B9E5_SHAREDEXP,       GL_RGB9_E5);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R11G11B10_FLOAT,          GL_R11F_G11F_B10F);

    AddDXGIToESEntry(&map, DXGI_FORMAT_R16_TYPELESS,             GL_DEPTH_COMPONENT16);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R16_UNORM,                GL_DEPTH_COMPONENT16);
    AddDXGIToESEntry(&map, DXGI_FORMAT_D16_UNORM,                GL_DEPTH_COMPONENT16);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R24G8_TYPELESS,           GL_DEPTH24_STENCIL8_OES);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R24_UNORM_X8_TYPELESS,    GL_DEPTH24_STENCIL8_OES);
    AddDXGIToESEntry(&map, DXGI_FORMAT_D24_UNORM_S8_UINT,        GL_DEPTH24_STENCIL8_OES);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R32G8X24_TYPELESS,        GL_DEPTH32F_STENCIL8);
    AddDXGIToESEntry(&map, DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS, GL_DEPTH32F_STENCIL8);
    AddDXGIToESEntry(&map, DXGI_FORMAT_D32_FLOAT_S8X24_UINT,     GL_DEPTH32F_STENCIL8);

    AddDXGIToESEntry(&map, DXGI_FORMAT_BC1_UNORM,                GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);
    AddDXGIToESEntry(&map, DXGI_FORMAT_BC2_UNORM,                GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE);
    AddDXGIToESEntry(&map, DXGI_FORMAT_BC3_UNORM,                GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE);

    return map;
}

static DXGIToESFormatMap BuildDXGIToES2FormatMap()
{
    DXGIToESFormatMap map = BuildCommonDXGIToESFormatMap();

    AddDXGIToESEntry(&map, DXGI_FORMAT_R32_TYPELESS, GL_DEPTH_COMPONENT32_OES);
    AddDXGIToESEntry(&map, DXGI_FORMAT_D32_FLOAT,    GL_DEPTH_COMPONENT32_OES);

    return map;
}

static DXGIToESFormatMap BuildDXGIToES3FormatMap()
{
    DXGIToESFormatMap map = BuildCommonDXGIToESFormatMap();

    AddDXGIToESEntry(&map, DXGI_FORMAT_R32_TYPELESS, GL_DEPTH_COMPONENT32F);
    AddDXGIToESEntry(&map, DXGI_FORMAT_D32_FLOAT,    GL_DEPTH_COMPONENT32F);

    return map;
}

static const DXGIFormatInfoMap &GetDXGIFormatInfoMap()
{
    static const DXGIFormatInfoMap infoMap = BuildDXGIFormatInfoMap();
    return infoMap;
}

static bool GetDXGIFormatInfo(DXGI_FORMAT format, DXGIFormatInfo *outFormatInfo)
{
    const DXGIFormatInfoMap &infoMap = GetDXGIFormatInfoMap();
    DXGIFormatInfoMap::const_iterator iter = infoMap.find(format);
    if (iter != infoMap.end())
    {
        if (outFormatInfo)
        {
            *outFormatInfo = iter->second;
        }
        return true;
    }
    else
    {
        return false;
    }
}

static d3d11::DXGIFormatSet BuildAllDXGIFormatSet()
{
    d3d11::DXGIFormatSet set;

    const DXGIFormatInfoMap &infoMap = GetDXGIFormatInfoMap();
    for (DXGIFormatInfoMap::const_iterator i = infoMap.begin(); i != infoMap.end(); ++i)
    {
        set.insert(i->first);
    }

    return set;
}

struct D3D11FastCopyFormat
{
    DXGI_FORMAT mSourceFormat;
    GLenum mDestFormat;
    GLenum mDestType;

    D3D11FastCopyFormat(DXGI_FORMAT sourceFormat, GLenum destFormat, GLenum destType)
        : mSourceFormat(sourceFormat), mDestFormat(destFormat), mDestType(destType)
    { }

    bool operator<(const D3D11FastCopyFormat& other) const
    {
        return memcmp(this, &other, sizeof(D3D11FastCopyFormat)) < 0;
    }
};

typedef std::map<D3D11FastCopyFormat, ColorCopyFunction> D3D11FastCopyMap;
typedef std::pair<D3D11FastCopyFormat, ColorCopyFunction> D3D11FastCopyPair;

static D3D11FastCopyMap BuildFastCopyMap()
{
    D3D11FastCopyMap map;

    map.insert(D3D11FastCopyPair(D3D11FastCopyFormat(DXGI_FORMAT_B8G8R8A8_UNORM, GL_RGBA, GL_UNSIGNED_BYTE), CopyBGRAUByteToRGBAUByte));

    return map;
}

struct DXGIDepthStencilInfo
{
    unsigned int mDepthBits;
    unsigned int mDepthOffset;
    unsigned int mStencilBits;
    unsigned int mStencilOffset;

    DXGIDepthStencilInfo()
        : mDepthBits(0), mDepthOffset(0), mStencilBits(0), mStencilOffset(0)
    { }

    DXGIDepthStencilInfo(unsigned int depthBits, unsigned int depthOffset, unsigned int stencilBits, unsigned int stencilOffset)
        : mDepthBits(depthBits), mDepthOffset(depthOffset), mStencilBits(stencilBits), mStencilOffset(stencilOffset)
    { }
};

typedef std::map<DXGI_FORMAT, DXGIDepthStencilInfo> DepthStencilInfoMap;
typedef std::pair<DXGI_FORMAT, DXGIDepthStencilInfo> DepthStencilInfoPair;

static DepthStencilInfoMap BuildDepthStencilInfoMap()
{
    DepthStencilInfoMap map;

    map.insert(DepthStencilInfoPair(DXGI_FORMAT_R16_TYPELESS,             DXGIDepthStencilInfo(16, 0, 0,  0)));
    map.insert(DepthStencilInfoPair(DXGI_FORMAT_R16_UNORM,                DXGIDepthStencilInfo(16, 0, 0,  0)));
    map.insert(DepthStencilInfoPair(DXGI_FORMAT_D16_UNORM,                DXGIDepthStencilInfo(16, 0, 0,  0)));

    map.insert(DepthStencilInfoPair(DXGI_FORMAT_R24G8_TYPELESS,           DXGIDepthStencilInfo(24, 0, 8, 24)));
    map.insert(DepthStencilInfoPair(DXGI_FORMAT_R24_UNORM_X8_TYPELESS,    DXGIDepthStencilInfo(24, 0, 8, 24)));
    map.insert(DepthStencilInfoPair(DXGI_FORMAT_D24_UNORM_S8_UINT,        DXGIDepthStencilInfo(24, 0, 8, 24)));

    map.insert(DepthStencilInfoPair(DXGI_FORMAT_R32_TYPELESS,             DXGIDepthStencilInfo(32, 0, 0,  0)));
    map.insert(DepthStencilInfoPair(DXGI_FORMAT_R32_FLOAT,                DXGIDepthStencilInfo(32, 0, 0,  0)));
    map.insert(DepthStencilInfoPair(DXGI_FORMAT_D32_FLOAT,                DXGIDepthStencilInfo(32, 0, 0,  0)));

    map.insert(DepthStencilInfoPair(DXGI_FORMAT_R32G8X24_TYPELESS,        DXGIDepthStencilInfo(32, 0, 8, 32)));
    map.insert(DepthStencilInfoPair(DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS, DXGIDepthStencilInfo(32, 0, 8, 32)));
    map.insert(DepthStencilInfoPair(DXGI_FORMAT_D32_FLOAT_S8X24_UINT,     DXGIDepthStencilInfo(32, 0, 8, 32)));

    return map;
}

static const DepthStencilInfoMap &GetDepthStencilInfoMap()
{
    static const DepthStencilInfoMap infoMap = BuildDepthStencilInfoMap();
    return infoMap;
}

bool GetDepthStencilInfo(DXGI_FORMAT format, DXGIDepthStencilInfo *outDepthStencilInfo)
{
    const DepthStencilInfoMap& infoMap = GetDepthStencilInfoMap();
    DepthStencilInfoMap::const_iterator iter = infoMap.find(format);
    if (iter != infoMap.end())
    {
        if (outDepthStencilInfo)
        {
            *outDepthStencilInfo = iter->second;
        }
        return true;
    }
    else
    {
        return false;
    }
}

struct SwizzleSizeType
{
    unsigned int mMaxComponentSize;
    GLenum mComponentType;

    SwizzleSizeType()
        : mMaxComponentSize(0), mComponentType(GL_NONE)
    { }

    SwizzleSizeType(unsigned int maxComponentSize, GLenum componentType)
        : mMaxComponentSize(maxComponentSize), mComponentType(componentType)
    { }

    bool operator<(const SwizzleSizeType& other) const
    {
        return (mMaxComponentSize != other.mMaxComponentSize) ? (mMaxComponentSize < other.mMaxComponentSize)
                                                              : (mComponentType < other.mComponentType);
    }
};

struct SwizzleFormatInfo
{
    DXGI_FORMAT mTexFormat;
    DXGI_FORMAT mSRVFormat;
    DXGI_FORMAT mRTVFormat;

    SwizzleFormatInfo()
        : mTexFormat(DXGI_FORMAT_UNKNOWN), mSRVFormat(DXGI_FORMAT_UNKNOWN), mRTVFormat(DXGI_FORMAT_UNKNOWN)
    { }

    SwizzleFormatInfo(DXGI_FORMAT texFormat, DXGI_FORMAT srvFormat, DXGI_FORMAT rtvFormat)
        : mTexFormat(texFormat), mSRVFormat(srvFormat), mRTVFormat(rtvFormat)
    { }
};

typedef std::map<SwizzleSizeType, SwizzleFormatInfo> SwizzleInfoMap;
typedef std::pair<SwizzleSizeType, SwizzleFormatInfo> SwizzleInfoPair;

static SwizzleInfoMap BuildSwizzleInfoMap()
{
    SwizzleInfoMap map;

    map.insert(SwizzleInfoPair(SwizzleSizeType( 8, GL_UNSIGNED_NORMALIZED), SwizzleFormatInfo(DXGI_FORMAT_R8G8B8A8_UNORM,     DXGI_FORMAT_R8G8B8A8_UNORM,     DXGI_FORMAT_R8G8B8A8_UNORM    )));
    map.insert(SwizzleInfoPair(SwizzleSizeType(16, GL_UNSIGNED_NORMALIZED), SwizzleFormatInfo(DXGI_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_R16G16B16A16_UNORM)));
    map.insert(SwizzleInfoPair(SwizzleSizeType(24, GL_UNSIGNED_NORMALIZED), SwizzleFormatInfo(DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT)));
    map.insert(SwizzleInfoPair(SwizzleSizeType(32, GL_UNSIGNED_NORMALIZED), SwizzleFormatInfo(DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT)));

    map.insert(SwizzleInfoPair(SwizzleSizeType( 8, GL_SIGNED_NORMALIZED  ), SwizzleFormatInfo(DXGI_FORMAT_R8G8B8A8_SNORM,     DXGI_FORMAT_R8G8B8A8_SNORM,     DXGI_FORMAT_R8G8B8A8_SNORM    )));

    map.insert(SwizzleInfoPair(SwizzleSizeType(16, GL_FLOAT              ), SwizzleFormatInfo(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT)));
    map.insert(SwizzleInfoPair(SwizzleSizeType(32, GL_FLOAT              ), SwizzleFormatInfo(DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT)));

    map.insert(SwizzleInfoPair(SwizzleSizeType( 8, GL_UNSIGNED_INT       ), SwizzleFormatInfo(DXGI_FORMAT_R8G8B8A8_UINT,      DXGI_FORMAT_R8G8B8A8_UINT,      DXGI_FORMAT_R8G8B8A8_UINT     )));
    map.insert(SwizzleInfoPair(SwizzleSizeType(16, GL_UNSIGNED_INT       ), SwizzleFormatInfo(DXGI_FORMAT_R16G16B16A16_UINT,  DXGI_FORMAT_R16G16B16A16_UINT,  DXGI_FORMAT_R16G16B16A16_UINT )));
    map.insert(SwizzleInfoPair(SwizzleSizeType(32, GL_UNSIGNED_INT       ), SwizzleFormatInfo(DXGI_FORMAT_R32G32B32A32_UINT,  DXGI_FORMAT_R32G32B32A32_UINT,  DXGI_FORMAT_R32G32B32A32_UINT )));

    map.insert(SwizzleInfoPair(SwizzleSizeType( 8, GL_INT                ), SwizzleFormatInfo(DXGI_FORMAT_R8G8B8A8_SINT,      DXGI_FORMAT_R8G8B8A8_SINT,      DXGI_FORMAT_R8G8B8A8_SINT     )));
    map.insert(SwizzleInfoPair(SwizzleSizeType(16, GL_INT                ), SwizzleFormatInfo(DXGI_FORMAT_R16G16B16A16_SINT,  DXGI_FORMAT_R16G16B16A16_SINT,  DXGI_FORMAT_R16G16B16A16_SINT )));
    map.insert(SwizzleInfoPair(SwizzleSizeType(32, GL_INT                ), SwizzleFormatInfo(DXGI_FORMAT_R32G32B32A32_SINT,  DXGI_FORMAT_R32G32B32A32_SINT,  DXGI_FORMAT_R32G32B32A32_SINT )));

    return map;
}
typedef std::pair<GLint, InitializeTextureDataFunction> InternalFormatInitializerPair;
typedef std::map<GLint, InitializeTextureDataFunction> InternalFormatInitializerMap;

static InternalFormatInitializerMap BuildInternalFormatInitializerMap()
{
    InternalFormatInitializerMap map;

    map.insert(InternalFormatInitializerPair(GL_RGB8,    initialize4ComponentData<GLubyte,  0x00,       0x00,       0x00,       0xFF>          ));
    map.insert(InternalFormatInitializerPair(GL_RGB565,  initialize4ComponentData<GLubyte,  0x00,       0x00,       0x00,       0xFF>          ));
    map.insert(InternalFormatInitializerPair(GL_SRGB8,   initialize4ComponentData<GLubyte,  0x00,       0x00,       0x00,       0xFF>          ));
    map.insert(InternalFormatInitializerPair(GL_RGB16F,  initialize4ComponentData<GLhalf,   0x0000,     0x0000,     0x0000,     gl::Float16One>));
    map.insert(InternalFormatInitializerPair(GL_RGB32F,  initialize4ComponentData<GLfloat,  0x00000000, 0x00000000, 0x00000000, gl::Float32One>));
    map.insert(InternalFormatInitializerPair(GL_RGB8UI,  initialize4ComponentData<GLubyte,  0x00,       0x00,       0x00,       0x01>          ));
    map.insert(InternalFormatInitializerPair(GL_RGB8I,   initialize4ComponentData<GLbyte,   0x00,       0x00,       0x00,       0x01>          ));
    map.insert(InternalFormatInitializerPair(GL_RGB16UI, initialize4ComponentData<GLushort, 0x0000,     0x0000,     0x0000,     0x0001>        ));
    map.insert(InternalFormatInitializerPair(GL_RGB16I,  initialize4ComponentData<GLshort,  0x0000,     0x0000,     0x0000,     0x0001>        ));
    map.insert(InternalFormatInitializerPair(GL_RGB32UI, initialize4ComponentData<GLuint,   0x00000000, 0x00000000, 0x00000000, 0x00000001>    ));
    map.insert(InternalFormatInitializerPair(GL_RGB32I,  initialize4ComponentData<GLint,    0x00000000, 0x00000000, 0x00000000, 0x00000001>    ));

    return map;
}

static const SwizzleInfoMap &GetSwizzleInfoMap()
{
    static const SwizzleInfoMap map = BuildSwizzleInfoMap();
    return map;
}

static const SwizzleFormatInfo GetSwizzleFormatInfo(GLint internalFormat, GLuint clientVersion)
{
    // Get the maximum sized component
    unsigned int maxBits = 1;

    if (gl::IsFormatCompressed(internalFormat, clientVersion))
    {
        unsigned int compressedBitsPerBlock = gl::GetPixelBytes(internalFormat, clientVersion) * 8;
        unsigned int blockSize = gl::GetCompressedBlockWidth(internalFormat, clientVersion) *
                                 gl::GetCompressedBlockHeight(internalFormat, clientVersion);
        maxBits = std::max(compressedBitsPerBlock / blockSize, maxBits);
    }
    else
    {
        maxBits = std::max(maxBits, gl::GetAlphaBits(    internalFormat, clientVersion));
        maxBits = std::max(maxBits, gl::GetRedBits(      internalFormat, clientVersion));
        maxBits = std::max(maxBits, gl::GetGreenBits(    internalFormat, clientVersion));
        maxBits = std::max(maxBits, gl::GetBlueBits(     internalFormat, clientVersion));
        maxBits = std::max(maxBits, gl::GetLuminanceBits(internalFormat, clientVersion));
        maxBits = std::max(maxBits, gl::GetDepthBits(    internalFormat, clientVersion));
    }

    maxBits = roundUp(maxBits, 8U);

    GLenum componentType = gl::GetComponentType(internalFormat, clientVersion);

    const SwizzleInfoMap &map = GetSwizzleInfoMap();
    SwizzleInfoMap::const_iterator iter = map.find(SwizzleSizeType(maxBits, componentType));

    if (iter != map.end())
    {
        return iter->second;
    }
    else
    {
        UNREACHABLE();
        static const SwizzleFormatInfo defaultFormatInfo;
        return defaultFormatInfo;
    }
}

static const InternalFormatInitializerMap &GetInternalFormatInitializerMap()
{
    static const InternalFormatInitializerMap map = BuildInternalFormatInitializerMap();
    return map;
}

namespace d3d11
{

MipGenerationFunction GetMipGenerationFunction(DXGI_FORMAT format)
{
    DXGIFormatInfo formatInfo;
    if (GetDXGIFormatInfo(format, &formatInfo))
    {
        return formatInfo.mMipGenerationFunction;
    }
    else
    {
        UNREACHABLE();
        return NULL;
    }
}

LoadImageFunction GetImageLoadFunction(GLenum internalFormat, GLenum type, GLuint clientVersion)
{
    if (clientVersion == 2)
    {
        D3D11ES2FormatInfo d3d11FormatInfo;
        if (GetD3D11ES2FormatInfo(internalFormat, clientVersion, &d3d11FormatInfo))
        {
            return d3d11FormatInfo.mLoadImageFunction;
        }
        else
        {
            UNREACHABLE();
            return NULL;
        }
    }
    else if (clientVersion == 3)
    {
        static const D3D11LoadFunctionMap loadImageMap = buildD3D11LoadFunctionMap();
        D3D11LoadFunctionMap::const_iterator iter = loadImageMap.find(InternalFormatTypePair(internalFormat, type));
        if (iter != loadImageMap.end())
        {
            return iter->second;
        }
        else
        {
            UNREACHABLE();
            return NULL;
        }
    }
    else
    {
        UNREACHABLE();
        return NULL;
    }
}

GLuint GetFormatPixelBytes(DXGI_FORMAT format)
{
    DXGIFormatInfo dxgiFormatInfo;
    if (GetDXGIFormatInfo(format, &dxgiFormatInfo))
    {
        return dxgiFormatInfo.mPixelBits / 8;
    }
    else
    {
        UNREACHABLE();
        return 0;
    }
}

GLuint GetBlockWidth(DXGI_FORMAT format)
{
    DXGIFormatInfo dxgiFormatInfo;
    if (GetDXGIFormatInfo(format, &dxgiFormatInfo))
    {
        return dxgiFormatInfo.mBlockWidth;
    }
    else
    {
        UNREACHABLE();
        return 0;
    }
}

GLuint GetBlockHeight(DXGI_FORMAT format)
{
    DXGIFormatInfo dxgiFormatInfo;
    if (GetDXGIFormatInfo(format, &dxgiFormatInfo))
    {
        return dxgiFormatInfo.mBlockHeight;
    }
    else
    {
        UNREACHABLE();
        return 0;
    }
}

GLenum GetComponentType(DXGI_FORMAT format)
{
    DXGIFormatInfo dxgiFormatInfo;
    if (GetDXGIFormatInfo(format, &dxgiFormatInfo))
    {
        return dxgiFormatInfo.mComponentType;
    }
    else
    {
        UNREACHABLE();
        return GL_NONE;
    }
}

GLuint GetDepthBits(DXGI_FORMAT format)
{
    DXGIDepthStencilInfo dxgiDSInfo;
    if (GetDepthStencilInfo(format, &dxgiDSInfo))
    {
        return dxgiDSInfo.mDepthBits;
    }
    else
    {
        // Since the depth stencil info map does not contain all used DXGI formats,
        // we should not assert that the format exists
        return 0;
    }
}

GLuint GetDepthOffset(DXGI_FORMAT format)
{
    DXGIDepthStencilInfo dxgiDSInfo;
    if (GetDepthStencilInfo(format, &dxgiDSInfo))
    {
        return dxgiDSInfo.mDepthOffset;
    }
    else
    {
        // Since the depth stencil info map does not contain all used DXGI formats,
        // we should not assert that the format exists
        return 0;
    }
}

GLuint GetStencilBits(DXGI_FORMAT format)
{
    DXGIDepthStencilInfo dxgiDSInfo;
    if (GetDepthStencilInfo(format, &dxgiDSInfo))
    {
        return dxgiDSInfo.mStencilBits;
    }
    else
    {
        // Since the depth stencil info map does not contain all used DXGI formats,
        // we should not assert that the format exists
        return 0;
    }
}

GLuint GetStencilOffset(DXGI_FORMAT format)
{
    DXGIDepthStencilInfo dxgiDSInfo;
    if (GetDepthStencilInfo(format, &dxgiDSInfo))
    {
        return dxgiDSInfo.mStencilOffset;
    }
    else
    {
        // Since the depth stencil info map does not contain all used DXGI formats,
        // we should not assert that the format exists
        return 0;
    }
}

void MakeValidSize(bool isImage, DXGI_FORMAT format, GLsizei *requestWidth, GLsizei *requestHeight, int *levelOffset)
{
    DXGIFormatInfo dxgiFormatInfo;
    if (GetDXGIFormatInfo(format, &dxgiFormatInfo))
    {
        int upsampleCount = 0;

        GLsizei blockWidth = dxgiFormatInfo.mBlockWidth;
        GLsizei blockHeight = dxgiFormatInfo.mBlockHeight;

        // Don't expand the size of full textures that are at least (blockWidth x blockHeight) already.
        if (isImage || *requestWidth < blockWidth || *requestHeight < blockHeight)
        {
            while (*requestWidth % blockWidth != 0 || *requestHeight % blockHeight != 0)
            {
                *requestWidth <<= 1;
                *requestHeight <<= 1;
                upsampleCount++;
            }
        }
        *levelOffset = upsampleCount;
    }
    else
    {
        UNREACHABLE();
    }
}

const DXGIFormatSet &GetAllUsedDXGIFormats()
{
    static DXGIFormatSet formatSet = BuildAllDXGIFormatSet();
    return formatSet;
}

ColorReadFunction GetColorReadFunction(DXGI_FORMAT format)
{
    DXGIFormatInfo dxgiFormatInfo;
    if (GetDXGIFormatInfo(format, &dxgiFormatInfo))
    {
        return dxgiFormatInfo.mColorReadFunction;
    }
    else
    {
        UNREACHABLE();
        return NULL;
    }
}

ColorCopyFunction GetFastCopyFunction(DXGI_FORMAT sourceFormat, GLenum destFormat, GLenum destType)
{
    static const D3D11FastCopyMap fastCopyMap = BuildFastCopyMap();
    D3D11FastCopyMap::const_iterator iter = fastCopyMap.find(D3D11FastCopyFormat(sourceFormat, destFormat, destType));
    return (iter != fastCopyMap.end()) ? iter->second : NULL;
}

}

namespace gl_d3d11
{

DXGI_FORMAT GetTexFormat(GLenum internalFormat, GLuint clientVersion)
{
    if (clientVersion == 2)
    {
        D3D11ES2FormatInfo d3d11FormatInfo;
        if (GetD3D11ES2FormatInfo(internalFormat, clientVersion, &d3d11FormatInfo))
        {
            return d3d11FormatInfo.mTexFormat;
        }
        else
        {
            UNREACHABLE();
            return DXGI_FORMAT_UNKNOWN;
        }
    }
    else if (clientVersion == 3)
    {
        D3D11ES3FormatInfo d3d11FormatInfo;
        if (GetD3D11ES3FormatInfo(internalFormat, clientVersion, &d3d11FormatInfo))
        {
            return d3d11FormatInfo.mTexFormat;
        }
        else
        {
            UNREACHABLE();
            return DXGI_FORMAT_UNKNOWN;
        }
    }
    else
    {
        UNREACHABLE();
        return DXGI_FORMAT_UNKNOWN;
    }
}

DXGI_FORMAT GetSRVFormat(GLenum internalFormat, GLuint clientVersion)
{
    if (clientVersion == 2)
    {
        D3D11ES2FormatInfo d3d11FormatInfo;
        if (GetD3D11ES2FormatInfo(internalFormat, clientVersion, &d3d11FormatInfo))
        {
            return d3d11FormatInfo.mSRVFormat;
        }
        else
        {
            UNREACHABLE();
            return DXGI_FORMAT_UNKNOWN;
        }
    }
    else if (clientVersion == 3)
    {
        D3D11ES3FormatInfo d3d11FormatInfo;
        if (GetD3D11ES3FormatInfo(internalFormat, clientVersion, &d3d11FormatInfo))
        {
            return d3d11FormatInfo.mSRVFormat;
        }
        else
        {
            UNREACHABLE();
            return DXGI_FORMAT_UNKNOWN;
        }
    }
    else
    {
        UNREACHABLE();
        return DXGI_FORMAT_UNKNOWN;
    }
}

DXGI_FORMAT GetRTVFormat(GLenum internalFormat, GLuint clientVersion)
{
    if (clientVersion == 2)
    {
        D3D11ES2FormatInfo d3d11FormatInfo;
        if (GetD3D11ES2FormatInfo(internalFormat, clientVersion, &d3d11FormatInfo))
        {
            return d3d11FormatInfo.mRTVFormat;
        }
        else
        {
            UNREACHABLE();
            return DXGI_FORMAT_UNKNOWN;
        }
    }
    else if (clientVersion == 3)
    {
        D3D11ES3FormatInfo d3d11FormatInfo;
        if (GetD3D11ES3FormatInfo(internalFormat, clientVersion, &d3d11FormatInfo))
        {
            return d3d11FormatInfo.mRTVFormat;
        }
        else
        {
            UNREACHABLE();
            return DXGI_FORMAT_UNKNOWN;
        }
    }
    else
    {
        UNREACHABLE();
        return DXGI_FORMAT_UNKNOWN;
    }
}

DXGI_FORMAT GetDSVFormat(GLenum internalFormat, GLuint clientVersion)
{
    if (clientVersion == 2)
    {
        D3D11ES2FormatInfo d3d11FormatInfo;
        if (GetD3D11ES2FormatInfo(internalFormat, clientVersion, &d3d11FormatInfo))
        {
            return d3d11FormatInfo.mDSVFormat;
        }
        else
        {
            return DXGI_FORMAT_UNKNOWN;
        }
    }
    else if (clientVersion == 3)
    {
        D3D11ES3FormatInfo d3d11FormatInfo;
        if (GetD3D11ES3FormatInfo(internalFormat, clientVersion, &d3d11FormatInfo))
        {
            return d3d11FormatInfo.mDSVFormat;
        }
        else
        {
            return DXGI_FORMAT_UNKNOWN;
        }
    }
    else
    {
        UNREACHABLE();
        return DXGI_FORMAT_UNKNOWN;
    }
}

// Given a GL internal format, this function returns the DSV format if it is depth- or stencil-renderable,
// the RTV format if it is color-renderable, and the (nonrenderable) texture format otherwise.
DXGI_FORMAT GetRenderableFormat(GLenum internalFormat, GLuint clientVersion)
{
    DXGI_FORMAT targetFormat = GetDSVFormat(internalFormat, clientVersion);
    if (targetFormat == DXGI_FORMAT_UNKNOWN)
        targetFormat = GetRTVFormat(internalFormat, clientVersion);
    if (targetFormat == DXGI_FORMAT_UNKNOWN)
        targetFormat = GetTexFormat(internalFormat, clientVersion);

    return targetFormat;
}

DXGI_FORMAT GetSwizzleTexFormat(GLint internalFormat, const Renderer *renderer)
{
    GLuint clientVersion = renderer->getCurrentClientVersion();
    if (gl::GetComponentCount(internalFormat, clientVersion) != 4 || !gl::IsColorRenderingSupported(internalFormat, renderer))
    {
        const SwizzleFormatInfo &swizzleInfo = GetSwizzleFormatInfo(internalFormat, clientVersion);
        return swizzleInfo.mTexFormat;
    }
    else
    {
        return GetTexFormat(internalFormat, clientVersion);
    }
}

DXGI_FORMAT GetSwizzleSRVFormat(GLint internalFormat, const Renderer *renderer)
{
    GLuint clientVersion = renderer->getCurrentClientVersion();
    if (gl::GetComponentCount(internalFormat, clientVersion) != 4 || !gl::IsColorRenderingSupported(internalFormat, renderer))
    {
        const SwizzleFormatInfo &swizzleInfo = GetSwizzleFormatInfo(internalFormat, clientVersion);
        return swizzleInfo.mSRVFormat;
    }
    else
    {
        return GetTexFormat(internalFormat, clientVersion);
    }
}

DXGI_FORMAT GetSwizzleRTVFormat(GLint internalFormat, const Renderer *renderer)
{
    GLuint clientVersion = renderer->getCurrentClientVersion();
    if (gl::GetComponentCount(internalFormat, clientVersion) != 4 || !gl::IsColorRenderingSupported(internalFormat, renderer))
    {
        const SwizzleFormatInfo &swizzleInfo = GetSwizzleFormatInfo(internalFormat, clientVersion);
        return swizzleInfo.mRTVFormat;
    }
    else
    {
        return GetTexFormat(internalFormat, clientVersion);
    }
}

bool RequiresTextureDataInitialization(GLint internalFormat)
{
    const InternalFormatInitializerMap &map = GetInternalFormatInitializerMap();
    return map.find(internalFormat) != map.end();
}

InitializeTextureDataFunction GetTextureDataInitializationFunction(GLint internalFormat)
{
    const InternalFormatInitializerMap &map = GetInternalFormatInitializerMap();
    InternalFormatInitializerMap::const_iterator iter = map.find(internalFormat);
    if (iter != map.end())
    {
        return iter->second;
    }
    else
    {
        UNREACHABLE();
        return NULL;
    }
}

struct D3D11VertexFormatInfo
{
    rx::VertexConversionType mConversionType;
    DXGI_FORMAT mNativeFormat;
    VertexCopyFunction mCopyFunction;

    D3D11VertexFormatInfo()
        : mConversionType(VERTEX_CONVERT_NONE),
          mNativeFormat(DXGI_FORMAT_UNKNOWN),
          mCopyFunction(NULL)
    {}

    D3D11VertexFormatInfo(VertexConversionType conversionType, DXGI_FORMAT nativeFormat, VertexCopyFunction copyFunction)
        : mConversionType(conversionType),
          mNativeFormat(nativeFormat),
          mCopyFunction(copyFunction)
    {}
};

typedef std::map<gl::VertexFormat, D3D11VertexFormatInfo> D3D11VertexFormatInfoMap;

typedef std::pair<gl::VertexFormat, D3D11VertexFormatInfo> D3D11VertexFormatPair;

static void addVertexFormatInfo(D3D11VertexFormatInfoMap *map, GLenum inputType, GLboolean normalized, GLuint componentCount,
                                VertexConversionType conversionType, DXGI_FORMAT nativeFormat, VertexCopyFunction copyFunction)
{
    gl::VertexFormat inputFormat(inputType, normalized, componentCount, false);
    map->insert(D3D11VertexFormatPair(inputFormat, D3D11VertexFormatInfo(conversionType, nativeFormat, copyFunction)));
}

static void addIntegerVertexFormatInfo(D3D11VertexFormatInfoMap *map, GLenum inputType, GLuint componentCount,
                                       VertexConversionType conversionType, DXGI_FORMAT nativeFormat, VertexCopyFunction copyFunction)
{
    gl::VertexFormat inputFormat(inputType, GL_FALSE, componentCount, true);
    map->insert(D3D11VertexFormatPair(inputFormat, D3D11VertexFormatInfo(conversionType, nativeFormat, copyFunction)));
}

static D3D11VertexFormatInfoMap BuildD3D11VertexFormatInfoMap()
{
    D3D11VertexFormatInfoMap map;

    // TODO: column legend

    //
    // Float formats
    //

    // GL_BYTE -- un-normalized
    addVertexFormatInfo(&map, GL_BYTE,           GL_FALSE, 1, VERTEX_CONVERT_GPU,  DXGI_FORMAT_R8_SINT,            &copyVertexData<GLbyte, 1, 0>);
    addVertexFormatInfo(&map, GL_BYTE,           GL_FALSE, 2, VERTEX_CONVERT_GPU,  DXGI_FORMAT_R8G8_SINT,          &copyVertexData<GLbyte, 2, 0>);
    addVertexFormatInfo(&map, GL_BYTE,           GL_FALSE, 3, VERTEX_CONVERT_BOTH, DXGI_FORMAT_R8G8B8A8_SINT,      &copyVertexData<GLbyte, 3, 1>);
    addVertexFormatInfo(&map, GL_BYTE,           GL_FALSE, 4, VERTEX_CONVERT_GPU,  DXGI_FORMAT_R8G8B8A8_SINT,      &copyVertexData<GLbyte, 4, 0>);

    // GL_BYTE -- normalized
    addVertexFormatInfo(&map, GL_BYTE,           GL_TRUE,  1, VERTEX_CONVERT_NONE, DXGI_FORMAT_R8_SNORM,           &copyVertexData<GLbyte, 1, 0>);
    addVertexFormatInfo(&map, GL_BYTE,           GL_TRUE,  2, VERTEX_CONVERT_NONE, DXGI_FORMAT_R8G8_SNORM,         &copyVertexData<GLbyte, 2, 0>);
    addVertexFormatInfo(&map, GL_BYTE,           GL_TRUE,  3, VERTEX_CONVERT_CPU,  DXGI_FORMAT_R8G8B8A8_SNORM,     &copyVertexData<GLbyte, 3, INT8_MAX>);
    addVertexFormatInfo(&map, GL_BYTE,           GL_TRUE,  4, VERTEX_CONVERT_NONE, DXGI_FORMAT_R8G8B8A8_SNORM,     &copyVertexData<GLbyte, 4, 0>);

    // GL_UNSIGNED_BYTE -- un-normalized
    addVertexFormatInfo(&map, GL_UNSIGNED_BYTE,  GL_FALSE, 1, VERTEX_CONVERT_GPU,  DXGI_FORMAT_R8_UINT,            &copyVertexData<GLubyte, 1, 0>);
    addVertexFormatInfo(&map, GL_UNSIGNED_BYTE,  GL_FALSE, 2, VERTEX_CONVERT_GPU,  DXGI_FORMAT_R8G8_UINT,          &copyVertexData<GLubyte, 2, 0>);
    addVertexFormatInfo(&map, GL_UNSIGNED_BYTE,  GL_FALSE, 3, VERTEX_CONVERT_BOTH, DXGI_FORMAT_R8G8B8A8_UINT,      &copyVertexData<GLubyte, 3, 1>);
    addVertexFormatInfo(&map, GL_UNSIGNED_BYTE,  GL_FALSE, 4, VERTEX_CONVERT_GPU,  DXGI_FORMAT_R8G8B8A8_UINT,      &copyVertexData<GLubyte, 4, 0>);

    // GL_UNSIGNED_BYTE -- normalized
    addVertexFormatInfo(&map, GL_UNSIGNED_BYTE,  GL_TRUE,  1, VERTEX_CONVERT_NONE, DXGI_FORMAT_R8_UNORM,           &copyVertexData<GLubyte, 1, 0>);
    addVertexFormatInfo(&map, GL_UNSIGNED_BYTE,  GL_TRUE,  2, VERTEX_CONVERT_NONE, DXGI_FORMAT_R8G8_UNORM,         &copyVertexData<GLubyte, 2, 0>);
    addVertexFormatInfo(&map, GL_UNSIGNED_BYTE,  GL_TRUE,  3, VERTEX_CONVERT_CPU,  DXGI_FORMAT_R8G8B8A8_UNORM,     &copyVertexData<GLubyte, 3, UINT8_MAX>);
    addVertexFormatInfo(&map, GL_UNSIGNED_BYTE,  GL_TRUE,  4, VERTEX_CONVERT_NONE, DXGI_FORMAT_R8G8B8A8_UNORM,     &copyVertexData<GLubyte, 4, 0>);

    // GL_SHORT -- un-normalized
    addVertexFormatInfo(&map, GL_SHORT,          GL_FALSE, 1, VERTEX_CONVERT_GPU,  DXGI_FORMAT_R16_SINT,           &copyVertexData<GLshort, 1, 0>);
    addVertexFormatInfo(&map, GL_SHORT,          GL_FALSE, 2, VERTEX_CONVERT_GPU,  DXGI_FORMAT_R16G16_SINT,        &copyVertexData<GLshort, 2, 0>);
    addVertexFormatInfo(&map, GL_SHORT,          GL_FALSE, 3, VERTEX_CONVERT_BOTH, DXGI_FORMAT_R16G16B16A16_SINT,  &copyVertexData<GLshort, 4, 1>);
    addVertexFormatInfo(&map, GL_SHORT,          GL_FALSE, 4, VERTEX_CONVERT_GPU,  DXGI_FORMAT_R16G16B16A16_SINT,  &copyVertexData<GLshort, 4, 0>);

    // GL_SHORT -- normalized
    addVertexFormatInfo(&map, GL_SHORT,          GL_TRUE,  1, VERTEX_CONVERT_NONE, DXGI_FORMAT_R16_SNORM,          &copyVertexData<GLshort, 1, 0>);
    addVertexFormatInfo(&map, GL_SHORT,          GL_TRUE,  2, VERTEX_CONVERT_NONE, DXGI_FORMAT_R16G16_SNORM,       &copyVertexData<GLshort, 2, 0>);
    addVertexFormatInfo(&map, GL_SHORT,          GL_TRUE,  3, VERTEX_CONVERT_CPU,  DXGI_FORMAT_R16G16B16A16_SNORM, &copyVertexData<GLshort, 3, INT16_MAX>);
    addVertexFormatInfo(&map, GL_SHORT,          GL_TRUE,  4, VERTEX_CONVERT_NONE, DXGI_FORMAT_R16G16B16A16_SNORM, &copyVertexData<GLshort, 4, 0>);

    // GL_UNSIGNED_SHORT -- un-normalized
    addVertexFormatInfo(&map, GL_UNSIGNED_SHORT, GL_FALSE, 1, VERTEX_CONVERT_GPU,  DXGI_FORMAT_R16_UINT,           &copyVertexData<GLushort, 1, 0>);
    addVertexFormatInfo(&map, GL_UNSIGNED_SHORT, GL_FALSE, 2, VERTEX_CONVERT_GPU,  DXGI_FORMAT_R16G16_UINT,        &copyVertexData<GLushort, 2, 0>);
    addVertexFormatInfo(&map, GL_UNSIGNED_SHORT, GL_FALSE, 3, VERTEX_CONVERT_BOTH, DXGI_FORMAT_R16G16B16A16_UINT,  &copyVertexData<GLushort, 3, 1>);
    addVertexFormatInfo(&map, GL_UNSIGNED_SHORT, GL_FALSE, 4, VERTEX_CONVERT_GPU,  DXGI_FORMAT_R16G16B16A16_UINT,  &copyVertexData<GLushort, 4, 0>);

    // GL_UNSIGNED_SHORT -- normalized
    addVertexFormatInfo(&map, GL_UNSIGNED_SHORT, GL_TRUE,  1, VERTEX_CONVERT_NONE, DXGI_FORMAT_R16_UNORM,          &copyVertexData<GLushort, 1, 0>);
    addVertexFormatInfo(&map, GL_UNSIGNED_SHORT, GL_TRUE,  2, VERTEX_CONVERT_NONE, DXGI_FORMAT_R16G16_UNORM,       &copyVertexData<GLushort, 2, 0>);
    addVertexFormatInfo(&map, GL_UNSIGNED_SHORT, GL_TRUE,  3, VERTEX_CONVERT_CPU,  DXGI_FORMAT_R16G16B16A16_UNORM, &copyVertexData<GLushort, 3, UINT16_MAX>);
    addVertexFormatInfo(&map, GL_UNSIGNED_SHORT, GL_TRUE,  4, VERTEX_CONVERT_NONE, DXGI_FORMAT_R16G16B16A16_UNORM, &copyVertexData<GLushort, 4, 0>);

    // GL_INT -- un-normalized
    addVertexFormatInfo(&map, GL_INT,            GL_FALSE, 1, VERTEX_CONVERT_GPU,  DXGI_FORMAT_R32_SINT,           &copyVertexData<GLint, 1, 0>);
    addVertexFormatInfo(&map, GL_INT,            GL_FALSE, 2, VERTEX_CONVERT_GPU,  DXGI_FORMAT_R32G32_SINT,        &copyVertexData<GLint, 2, 0>);
    addVertexFormatInfo(&map, GL_INT,            GL_FALSE, 3, VERTEX_CONVERT_GPU,  DXGI_FORMAT_R32G32B32_SINT,     &copyVertexData<GLint, 3, 0>);
    addVertexFormatInfo(&map, GL_INT,            GL_FALSE, 4, VERTEX_CONVERT_GPU,  DXGI_FORMAT_R32G32B32A32_SINT,  &copyVertexData<GLint, 4, 0>);

    // GL_INT -- normalized
    addVertexFormatInfo(&map, GL_INT,            GL_TRUE,  1, VERTEX_CONVERT_CPU,  DXGI_FORMAT_R32_FLOAT,          &copyToFloatVertexData<GLint, 1, true>);
    addVertexFormatInfo(&map, GL_INT,            GL_TRUE,  2, VERTEX_CONVERT_CPU,  DXGI_FORMAT_R32G32_FLOAT,       &copyToFloatVertexData<GLint, 2, true>);
    addVertexFormatInfo(&map, GL_INT,            GL_TRUE,  3, VERTEX_CONVERT_CPU,  DXGI_FORMAT_R32G32B32_FLOAT,    &copyToFloatVertexData<GLint, 3, true>);
    addVertexFormatInfo(&map, GL_INT,            GL_TRUE,  4, VERTEX_CONVERT_CPU,  DXGI_FORMAT_R32G32B32A32_FLOAT, &copyToFloatVertexData<GLint, 4, true>);

    // GL_UNSIGNED_INT -- un-normalized
    addVertexFormatInfo(&map, GL_UNSIGNED_INT,   GL_FALSE, 1, VERTEX_CONVERT_GPU,  DXGI_FORMAT_R32_UINT,           &copyVertexData<GLuint, 1, 0>);
    addVertexFormatInfo(&map, GL_UNSIGNED_INT,   GL_FALSE, 2, VERTEX_CONVERT_GPU,  DXGI_FORMAT_R32G32_UINT,        &copyVertexData<GLuint, 2, 0>);
    addVertexFormatInfo(&map, GL_UNSIGNED_INT,   GL_FALSE, 3, VERTEX_CONVERT_GPU,  DXGI_FORMAT_R32G32B32_UINT,     &copyVertexData<GLuint, 3, 0>);
    addVertexFormatInfo(&map, GL_UNSIGNED_INT,   GL_FALSE, 4, VERTEX_CONVERT_GPU,  DXGI_FORMAT_R32G32B32A32_UINT,  &copyVertexData<GLuint, 4, 0>);

    // GL_UNSIGNED_INT -- normalized
    addVertexFormatInfo(&map, GL_UNSIGNED_INT,   GL_TRUE,  1, VERTEX_CONVERT_NONE, DXGI_FORMAT_R32_FLOAT,          &copyToFloatVertexData<GLuint, 1, true>);
    addVertexFormatInfo(&map, GL_UNSIGNED_INT,   GL_TRUE,  2, VERTEX_CONVERT_NONE, DXGI_FORMAT_R32G32_FLOAT,       &copyToFloatVertexData<GLuint, 2, true>);
    addVertexFormatInfo(&map, GL_UNSIGNED_INT,   GL_TRUE,  3, VERTEX_CONVERT_CPU,  DXGI_FORMAT_R32G32B32_FLOAT,    &copyToFloatVertexData<GLuint, 3, true>);
    addVertexFormatInfo(&map, GL_UNSIGNED_INT,   GL_TRUE,  4, VERTEX_CONVERT_NONE, DXGI_FORMAT_R32G32B32A32_FLOAT, &copyToFloatVertexData<GLuint, 4, true>);

    // GL_FIXED
    addVertexFormatInfo(&map, GL_FIXED,          GL_FALSE, 1, VERTEX_CONVERT_CPU,  DXGI_FORMAT_R32_FLOAT,          &copyFixedVertexData<1>);
    addVertexFormatInfo(&map, GL_FIXED,          GL_FALSE, 2, VERTEX_CONVERT_CPU,  DXGI_FORMAT_R32G32_FLOAT,       &copyFixedVertexData<2>);
    addVertexFormatInfo(&map, GL_FIXED,          GL_FALSE, 3, VERTEX_CONVERT_CPU,  DXGI_FORMAT_R32G32B32_FLOAT,    &copyFixedVertexData<3>);
    addVertexFormatInfo(&map, GL_FIXED,          GL_FALSE, 4, VERTEX_CONVERT_CPU,  DXGI_FORMAT_R32G32B32A32_FLOAT, &copyFixedVertexData<4>);

    // GL_HALF_FLOAT
    addVertexFormatInfo(&map, GL_HALF_FLOAT,     GL_FALSE, 1, VERTEX_CONVERT_NONE, DXGI_FORMAT_R16_FLOAT,          &copyVertexData<GLhalf, 1, 0>);
    addVertexFormatInfo(&map, GL_HALF_FLOAT,     GL_FALSE, 2, VERTEX_CONVERT_NONE, DXGI_FORMAT_R16G16_FLOAT,       &copyVertexData<GLhalf, 2, 0>);
    addVertexFormatInfo(&map, GL_HALF_FLOAT,     GL_FALSE, 3, VERTEX_CONVERT_CPU,  DXGI_FORMAT_R16G16B16A16_FLOAT, &copyVertexData<GLhalf, 3, gl::Float16One>);
    addVertexFormatInfo(&map, GL_HALF_FLOAT,     GL_FALSE, 4, VERTEX_CONVERT_NONE, DXGI_FORMAT_R16G16B16A16_FLOAT, &copyVertexData<GLhalf, 4, 0>);

    // GL_FLOAT
    addVertexFormatInfo(&map, GL_FLOAT,          GL_FALSE, 1, VERTEX_CONVERT_NONE, DXGI_FORMAT_R32_FLOAT,          &copyVertexData<GLfloat, 1, 0>);
    addVertexFormatInfo(&map, GL_FLOAT,          GL_FALSE, 2, VERTEX_CONVERT_NONE, DXGI_FORMAT_R32G32_FLOAT,       &copyVertexData<GLfloat, 2, 0>);
    addVertexFormatInfo(&map, GL_FLOAT,          GL_FALSE, 3, VERTEX_CONVERT_NONE, DXGI_FORMAT_R32G32B32_FLOAT,    &copyVertexData<GLfloat, 3, 0>);
    addVertexFormatInfo(&map, GL_FLOAT,          GL_FALSE, 4, VERTEX_CONVERT_NONE, DXGI_FORMAT_R32G32B32A32_FLOAT, &copyVertexData<GLfloat, 4, 0>);

    // GL_INT_2_10_10_10_REV
    addVertexFormatInfo(&map, GL_INT_2_10_10_10_REV,          GL_FALSE,  4, VERTEX_CONVERT_CPU,  DXGI_FORMAT_R32G32B32A32_FLOAT, &copyPackedVertexData<true, false, true>);
    addVertexFormatInfo(&map, GL_INT_2_10_10_10_REV,          GL_TRUE,   4, VERTEX_CONVERT_CPU,  DXGI_FORMAT_R32G32B32A32_FLOAT, &copyPackedVertexData<true, true,  true>);

    // GL_UNSIGNED_INT_2_10_10_10_REV
    addVertexFormatInfo(&map, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE,  4, VERTEX_CONVERT_CPU,  DXGI_FORMAT_R32G32B32A32_FLOAT, &copyPackedVertexData<false, false, true>);
    addVertexFormatInfo(&map, GL_UNSIGNED_INT_2_10_10_10_REV, GL_TRUE,   4, VERTEX_CONVERT_NONE, DXGI_FORMAT_R10G10B10A2_UNORM,  &copyPackedUnsignedVertexData);

    //
    // Integer Formats
    //

    // GL_BYTE
    addIntegerVertexFormatInfo(&map, GL_BYTE,           1, VERTEX_CONVERT_NONE,  DXGI_FORMAT_R8_SINT,           &copyVertexData<GLbyte, 1, 0>);
    addIntegerVertexFormatInfo(&map, GL_BYTE,           2, VERTEX_CONVERT_NONE,  DXGI_FORMAT_R8G8_SINT,         &copyVertexData<GLbyte, 2, 0>);
    addIntegerVertexFormatInfo(&map, GL_BYTE,           3, VERTEX_CONVERT_CPU,   DXGI_FORMAT_R8G8B8A8_SINT,     &copyVertexData<GLbyte, 3, 1>);
    addIntegerVertexFormatInfo(&map, GL_BYTE,           4, VERTEX_CONVERT_NONE,  DXGI_FORMAT_R8G8B8A8_SINT,     &copyVertexData<GLbyte, 4, 0>);

    // GL_UNSIGNED_BYTE
    addIntegerVertexFormatInfo(&map, GL_UNSIGNED_BYTE,  1, VERTEX_CONVERT_NONE,  DXGI_FORMAT_R8_UINT,           &copyVertexData<GLubyte, 1, 0>);
    addIntegerVertexFormatInfo(&map, GL_UNSIGNED_BYTE,  2, VERTEX_CONVERT_NONE,  DXGI_FORMAT_R8G8_UINT,         &copyVertexData<GLubyte, 2, 0>);
    addIntegerVertexFormatInfo(&map, GL_UNSIGNED_BYTE,  3, VERTEX_CONVERT_CPU,   DXGI_FORMAT_R8G8B8A8_UINT,     &copyVertexData<GLubyte, 3, 1>);
    addIntegerVertexFormatInfo(&map, GL_UNSIGNED_BYTE,  4, VERTEX_CONVERT_NONE,  DXGI_FORMAT_R8G8B8A8_UINT,     &copyVertexData<GLubyte, 4, 0>);

    // GL_SHORT
    addIntegerVertexFormatInfo(&map, GL_SHORT,          1, VERTEX_CONVERT_NONE,  DXGI_FORMAT_R16_SINT,          &copyVertexData<GLshort, 1, 0>);
    addIntegerVertexFormatInfo(&map, GL_SHORT,          2, VERTEX_CONVERT_NONE,  DXGI_FORMAT_R16G16_SINT,       &copyVertexData<GLshort, 2, 0>);
    addIntegerVertexFormatInfo(&map, GL_SHORT,          3, VERTEX_CONVERT_CPU,   DXGI_FORMAT_R16G16B16A16_SINT, &copyVertexData<GLshort, 3, 1>);
    addIntegerVertexFormatInfo(&map, GL_SHORT,          4, VERTEX_CONVERT_NONE,  DXGI_FORMAT_R16G16B16A16_SINT, &copyVertexData<GLshort, 4, 0>);

    // GL_UNSIGNED_SHORT
    addIntegerVertexFormatInfo(&map, GL_UNSIGNED_SHORT, 1, VERTEX_CONVERT_NONE,  DXGI_FORMAT_R16_UINT,          &copyVertexData<GLushort, 1, 0>);
    addIntegerVertexFormatInfo(&map, GL_UNSIGNED_SHORT, 2, VERTEX_CONVERT_NONE,  DXGI_FORMAT_R16G16_UINT,       &copyVertexData<GLushort, 2, 0>);
    addIntegerVertexFormatInfo(&map, GL_UNSIGNED_SHORT, 3, VERTEX_CONVERT_CPU,   DXGI_FORMAT_R16G16B16A16_UINT, &copyVertexData<GLushort, 3, 1>);
    addIntegerVertexFormatInfo(&map, GL_UNSIGNED_SHORT, 4, VERTEX_CONVERT_NONE,  DXGI_FORMAT_R16G16B16A16_UINT, &copyVertexData<GLushort, 4, 0>);

    // GL_INT
    addIntegerVertexFormatInfo(&map, GL_INT,            1, VERTEX_CONVERT_NONE,  DXGI_FORMAT_R32_SINT,          &copyVertexData<GLint, 1, 0>);
    addIntegerVertexFormatInfo(&map, GL_INT,            2, VERTEX_CONVERT_NONE,  DXGI_FORMAT_R32G32_SINT,       &copyVertexData<GLint, 2, 0>);
    addIntegerVertexFormatInfo(&map, GL_INT,            3, VERTEX_CONVERT_NONE,  DXGI_FORMAT_R32G32B32_SINT,    &copyVertexData<GLint, 3, 0>);
    addIntegerVertexFormatInfo(&map, GL_INT,            4, VERTEX_CONVERT_NONE,  DXGI_FORMAT_R32G32B32A32_SINT, &copyVertexData<GLint, 4, 0>);

    // GL_UNSIGNED_INT
    addIntegerVertexFormatInfo(&map, GL_UNSIGNED_INT,   1, VERTEX_CONVERT_NONE,  DXGI_FORMAT_R32_SINT,          &copyVertexData<GLuint, 1, 0>);
    addIntegerVertexFormatInfo(&map, GL_UNSIGNED_INT,   2, VERTEX_CONVERT_NONE,  DXGI_FORMAT_R32G32_SINT,       &copyVertexData<GLuint, 2, 0>);
    addIntegerVertexFormatInfo(&map, GL_UNSIGNED_INT,   3, VERTEX_CONVERT_NONE,  DXGI_FORMAT_R32G32B32_SINT,    &copyVertexData<GLuint, 3, 0>);
    addIntegerVertexFormatInfo(&map, GL_UNSIGNED_INT,   4, VERTEX_CONVERT_NONE,  DXGI_FORMAT_R32G32B32A32_SINT, &copyVertexData<GLuint, 4, 0>);

    // GL_INT_2_10_10_10_REV
    addIntegerVertexFormatInfo(&map, GL_INT_2_10_10_10_REV, 4, VERTEX_CONVERT_CPU, DXGI_FORMAT_R16G16B16A16_SINT, &copyPackedVertexData<true, true, false>);

    // GL_UNSIGNED_INT_2_10_10_10_REV
    addIntegerVertexFormatInfo(&map, GL_UNSIGNED_INT_2_10_10_10_REV, 4, VERTEX_CONVERT_NONE, DXGI_FORMAT_R10G10B10A2_UINT, &copyPackedUnsignedVertexData);

    return map;
}

static bool GetD3D11VertexFormatInfo(const gl::VertexFormat &vertexFormat, D3D11VertexFormatInfo *outVertexFormatInfo)
{
    static const D3D11VertexFormatInfoMap vertexFormatMap = BuildD3D11VertexFormatInfoMap();

    D3D11VertexFormatInfoMap::const_iterator iter = vertexFormatMap.find(vertexFormat);
    if (iter != vertexFormatMap.end())
    {
        if (outVertexFormatInfo)
        {
            *outVertexFormatInfo = iter->second;
        }
        return true;
    }
    else
    {
        return false;
    }
}

VertexCopyFunction GetVertexCopyFunction(const gl::VertexFormat &vertexFormat)
{
    D3D11VertexFormatInfo vertexFormatInfo;
    if (GetD3D11VertexFormatInfo(vertexFormat, &vertexFormatInfo))
    {
        return vertexFormatInfo.mCopyFunction;
    }
    else
    {
        UNREACHABLE();
        return NULL;
    }
}

size_t GetVertexElementSize(const gl::VertexFormat &vertexFormat)
{
    D3D11VertexFormatInfo vertexFormatInfo;
    if (GetD3D11VertexFormatInfo(vertexFormat, &vertexFormatInfo))
    {
        // FIXME: should not need a client version, and is not a pixel!
        return d3d11::GetFormatPixelBytes(vertexFormatInfo.mNativeFormat);
    }
    else
    {
        UNREACHABLE();
        return 0;
    }
}

rx::VertexConversionType GetVertexConversionType(const gl::VertexFormat &vertexFormat)
{
    D3D11VertexFormatInfo vertexFormatInfo;
    if (GetD3D11VertexFormatInfo(vertexFormat, &vertexFormatInfo))
    {
        return vertexFormatInfo.mConversionType;
    }
    else
    {
        UNREACHABLE();
        return VERTEX_CONVERT_NONE;
    }
}

DXGI_FORMAT GetNativeVertexFormat(const gl::VertexFormat &vertexFormat)
{
    D3D11VertexFormatInfo vertexFormatInfo;
    if (GetD3D11VertexFormatInfo(vertexFormat, &vertexFormatInfo))
    {
        return vertexFormatInfo.mNativeFormat;
    }
    else
    {
        UNREACHABLE();
        return DXGI_FORMAT_UNKNOWN;
    }
}

}

namespace d3d11_gl
{

GLenum GetInternalFormat(DXGI_FORMAT format, GLuint clientVersion)
{
    if (clientVersion == 2)
    {
        static DXGIToESFormatMap es2FormatMap = BuildDXGIToES2FormatMap();
        auto formatIt = es2FormatMap.find(format);
        if (formatIt != es2FormatMap.end())
        {
            return formatIt->second;
        }
    }
    else if (clientVersion == 3)
    {
        static DXGIToESFormatMap es3FormatMap = BuildDXGIToES3FormatMap();
        auto formatIt = es3FormatMap.find(format);
        if (formatIt != es3FormatMap.end())
        {
            return formatIt->second;
        }
    }

    UNREACHABLE();
    return GL_NONE;
}

}

}
