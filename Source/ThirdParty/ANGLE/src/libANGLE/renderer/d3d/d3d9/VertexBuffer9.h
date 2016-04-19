//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// VertexBuffer9.h: Defines the D3D9 VertexBuffer implementation.

#ifndef LIBANGLE_RENDERER_D3D_D3D9_VERTEXBUFFER9_H_
#define LIBANGLE_RENDERER_D3D_D3D9_VERTEXBUFFER9_H_

#include "libANGLE/renderer/d3d/VertexBuffer.h"

namespace rx
{
class Renderer9;

class VertexBuffer9 : public VertexBuffer
{
  public:
    explicit VertexBuffer9(Renderer9 *renderer);

    gl::Error initialize(unsigned int size, bool dynamicUsage) override;

    gl::Error storeVertexAttributes(const gl::VertexAttribute &attrib,
                                    GLenum currentValueType,
                                    GLint start,
                                    GLsizei count,
                                    GLsizei instances,
                                    unsigned int offset,
                                    const uint8_t *sourceData) override;

    unsigned int getBufferSize() const override;
    gl::Error setBufferSize(unsigned int size) override;
    gl::Error discard() override;

    IDirect3DVertexBuffer9 *getBuffer() const;

  private:
    ~VertexBuffer9() override;
    Renderer9 *mRenderer;

    IDirect3DVertexBuffer9 *mVertexBuffer;
    unsigned int mBufferSize;
    bool mDynamicUsage;
};

}

#endif // LIBANGLE_RENDERER_D3D_D3D9_VERTEXBUFFER9_H_
