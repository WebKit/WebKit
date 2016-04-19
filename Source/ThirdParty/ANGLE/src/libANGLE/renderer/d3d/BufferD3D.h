//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BufferD3D.h: Defines the rx::BufferD3D class, an implementation of BufferImpl.

#ifndef LIBANGLE_RENDERER_D3D_BUFFERD3D_H_
#define LIBANGLE_RENDERER_D3D_BUFFERD3D_H_

#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/BufferImpl.h"

#include <stdint.h>
#include <vector>

namespace rx
{
class BufferFactoryD3D;
class StaticIndexBufferInterface;
class StaticVertexBufferInterface;

enum class D3DBufferUsage
{
    STATIC,
    DYNAMIC,
};

class BufferD3D : public BufferImpl
{
  public:
    BufferD3D(BufferFactoryD3D *factory);
    virtual ~BufferD3D();

    unsigned int getSerial() const { return mSerial; }

    virtual size_t getSize() const = 0;
    virtual bool supportsDirectBinding() const = 0;
    virtual gl::Error markTransformFeedbackUsage() = 0;
    virtual gl::Error getData(const uint8_t **outData) = 0;

    StaticVertexBufferInterface *getStaticVertexBuffer(const gl::VertexAttribute &attribute);
    StaticIndexBufferInterface *getStaticIndexBuffer();

    virtual void initializeStaticData();
    virtual void invalidateStaticData();

    void promoteStaticUsage(int dataSize);

    gl::Error getIndexRange(GLenum type,
                            size_t offset,
                            size_t count,
                            bool primitiveRestartEnabled,
                            gl::IndexRange *outRange) override;

    BufferFactoryD3D *getFactory() const { return mFactory; }
    D3DBufferUsage getUsage() const { return mUsage; }

  protected:
    void updateSerial();
    void updateD3DBufferUsage(GLenum usage);
    void emptyStaticBufferCache();

    BufferFactoryD3D *mFactory;
    unsigned int mSerial;
    static unsigned int mNextSerial;

    std::vector<std::unique_ptr<StaticVertexBufferInterface>> mStaticVertexBuffers;
    StaticIndexBufferInterface *mStaticIndexBuffer;
    unsigned int mStaticBufferCacheTotalSize;
    unsigned int mStaticVertexBufferOutOfDate;
    unsigned int mUnmodifiedDataUse;
    D3DBufferUsage mUsage;
};

}

#endif // LIBANGLE_RENDERER_D3D_BUFFERD3D_H_
