//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// texture_format_table:
//   Queries for full textureFormat information based on internalFormat
//

#ifndef LIBANGLE_RENDERER_D3D_D3D11_TEXTUREFORMATTABLE_H_
#define LIBANGLE_RENDERER_D3D_D3D11_TEXTUREFORMATTABLE_H_

#include <map>

#include "common/angleutils.h"
#include "common/platform.h"
#include "libANGLE/renderer/Format.h"
#include "libANGLE/renderer/renderer_utils.h"
#include "libANGLE/renderer/d3d/formatutilsD3D.h"

namespace rx
{

struct Renderer11DeviceCaps;

namespace d3d11
{

// For sized GL internal formats, there are several possible corresponding D3D11 formats depending
// on device capabilities.
// This structure allows querying for the DXGI texture formats to use for textures, SRVs, RTVs and
// DSVs given a GL internal format.
struct Format final : angle::NonCopyable
{
    Format();
    Format(GLenum internalFormat,
           angle::Format::ID formatID,
           DXGI_FORMAT texFormat,
           DXGI_FORMAT srvFormat,
           DXGI_FORMAT rtvFormat,
           DXGI_FORMAT dsvFormat,
           DXGI_FORMAT blitSRVFormat,
           GLenum swizzleFormat,
           InitializeTextureDataFunction internalFormatInitializer,
           const Renderer11DeviceCaps &deviceCaps);

    static const Format &Get(GLenum internalFormat, const Renderer11DeviceCaps &deviceCaps);

    GLenum internalFormat;
    const angle::Format &format;

    DXGI_FORMAT texFormat;
    DXGI_FORMAT srvFormat;
    DXGI_FORMAT rtvFormat;
    DXGI_FORMAT dsvFormat;

    DXGI_FORMAT blitSRVFormat;

    const Format &swizzle;

    InitializeTextureDataFunction dataInitializerFunction;

    LoadFunctionMap loadFunctions;
};

}  // namespace d3d11

}  // namespace rx

#endif  // LIBANGLE_RENDERER_D3D_D3D11_TEXTUREFORMATTABLE_H_
