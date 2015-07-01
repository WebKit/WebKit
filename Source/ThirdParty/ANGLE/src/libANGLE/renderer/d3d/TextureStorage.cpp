//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureStorage.cpp: Shared members of abstract rx::TextureStorage class.

#include "libANGLE/renderer/d3d/TextureStorage.h"
#include "libANGLE/renderer/d3d/TextureD3D.h"
#include "libANGLE/renderer/d3d/RenderTargetD3D.h"
#include "libANGLE/renderer/Renderer.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/Texture.h"

#include "common/debug.h"
#include "common/mathutil.h"

namespace rx
{

TextureStorage::TextureStorage()
    : mFirstRenderTargetSerial(0),
      mRenderTargetSerialsLayerStride(0)
{}

void TextureStorage::initializeSerials(unsigned int rtSerialsToReserve, unsigned int rtSerialsLayerStride)
{
    mFirstRenderTargetSerial = RenderTargetD3D::issueSerials(rtSerialsToReserve);
    mRenderTargetSerialsLayerStride = rtSerialsLayerStride;
}

unsigned int TextureStorage::getRenderTargetSerial(const gl::ImageIndex &index) const
{
    unsigned int layerOffset = (index.hasLayer() ? (static_cast<unsigned int>(index.layerIndex) * mRenderTargetSerialsLayerStride) : 0);
    return mFirstRenderTargetSerial + static_cast<unsigned int>(index.mipIndex) + layerOffset;
}

}
