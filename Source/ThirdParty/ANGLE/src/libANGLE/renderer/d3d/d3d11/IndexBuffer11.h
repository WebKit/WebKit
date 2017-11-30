//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// IndexBuffer11.h: Defines the D3D11 IndexBuffer implementation.

#ifndef LIBANGLE_RENDERER_D3D_D3D11_INDEXBUFFER11_H_
#define LIBANGLE_RENDERER_D3D_D3D11_INDEXBUFFER11_H_

#include "libANGLE/renderer/d3d/IndexBuffer.h"
#include "libANGLE/renderer/d3d/d3d11/ResourceManager11.h"

namespace rx
{
class Renderer11;

class IndexBuffer11 : public IndexBuffer
{
  public:
    explicit IndexBuffer11(Renderer11 *const renderer);
    ~IndexBuffer11() override;

    gl::Error initialize(unsigned int bufferSize, GLenum indexType, bool dynamic) override;

    gl::Error mapBuffer(unsigned int offset, unsigned int size, void **outMappedMemory) override;
    gl::Error unmapBuffer() override;

    GLenum getIndexType() const override;
    unsigned int getBufferSize() const override;
    gl::Error setSize(unsigned int bufferSize, GLenum indexType) override;

    gl::Error discard() override;

    DXGI_FORMAT getIndexFormat() const;
    const d3d11::Buffer &getBuffer() const;

  private:
    Renderer11 *const mRenderer;

    d3d11::Buffer mBuffer;
    unsigned int mBufferSize;
    GLenum mIndexType;
    bool mDynamicUsage;
};

}  // namespace rx

#endif // LIBANGLE_RENDERER_D3D_D3D11_INDEXBUFFER11_H_
