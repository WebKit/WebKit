//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper routines for the D3D11 texture format table.

#include "libANGLE/renderer/d3d/d3d11/texture_format_table.h"

#include "libANGLE/renderer/load_functions_table.h"

namespace rx
{

namespace d3d11
{

Format::Format()
    : internalFormat(GL_NONE),
      format(angle::Format::Get(angle::Format::ID::NONE)),
      texFormat(DXGI_FORMAT_UNKNOWN),
      srvFormat(DXGI_FORMAT_UNKNOWN),
      rtvFormat(DXGI_FORMAT_UNKNOWN),
      dsvFormat(DXGI_FORMAT_UNKNOWN),
      blitSRVFormat(DXGI_FORMAT_UNKNOWN),
      swizzle(*this),
      dataInitializerFunction(nullptr)
{
}

Format::Format(GLenum internalFormat,
               angle::Format::ID formatID,
               DXGI_FORMAT texFormat,
               DXGI_FORMAT srvFormat,
               DXGI_FORMAT rtvFormat,
               DXGI_FORMAT dsvFormat,
               DXGI_FORMAT blitSRVFormat,
               GLenum swizzleFormat,
               InitializeTextureDataFunction internalFormatInitializer,
               const Renderer11DeviceCaps &deviceCaps)
    : internalFormat(internalFormat),
      format(angle::Format::Get(formatID)),
      texFormat(texFormat),
      srvFormat(srvFormat),
      rtvFormat(rtvFormat),
      dsvFormat(dsvFormat),
      blitSRVFormat(blitSRVFormat),
      swizzle(swizzleFormat == internalFormat ? *this : Format::Get(swizzleFormat, deviceCaps)),
      dataInitializerFunction(internalFormatInitializer),
      loadFunctions(GetLoadFunctionsMap(internalFormat, formatID))
{
}

}  // namespace d3d11

}  // namespace rx
