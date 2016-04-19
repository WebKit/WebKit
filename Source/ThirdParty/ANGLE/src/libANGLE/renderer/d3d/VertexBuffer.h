//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// VertexBuffer.h: Defines the abstract VertexBuffer class and VertexBufferInterface
// class with derivations, classes that perform graphics API agnostic vertex buffer operations.

#ifndef LIBANGLE_RENDERER_D3D_VERTEXBUFFER_H_
#define LIBANGLE_RENDERER_D3D_VERTEXBUFFER_H_

#include "common/angleutils.h"
#include "libANGLE/Error.h"

#include <GLES2/gl2.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gl
{
struct VertexAttribute;
struct VertexAttribCurrentValueData;
}

namespace rx
{
class BufferFactoryD3D;

// Use a ref-counting scheme with self-deletion on release. We do this so that we can more
// easily manage the static buffer cache, without deleting currently bound buffers.
class VertexBuffer : angle::NonCopyable
{
  public:
    VertexBuffer();

    virtual gl::Error initialize(unsigned int size, bool dynamicUsage) = 0;

    virtual gl::Error storeVertexAttributes(const gl::VertexAttribute &attrib,
                                            GLenum currentValueType,
                                            GLint start,
                                            GLsizei count,
                                            GLsizei instances,
                                            unsigned int offset,
                                            const uint8_t *sourceData) = 0;

    virtual unsigned int getBufferSize() const = 0;
    virtual gl::Error setBufferSize(unsigned int size) = 0;
    virtual gl::Error discard() = 0;

    unsigned int getSerial() const;

    // This may be overridden (e.g. by VertexBuffer11) if necessary.
    virtual void hintUnmapResource() { };

    // Reference counting.
    void addRef();
    void release();

  protected:
    void updateSerial();
    virtual ~VertexBuffer();

  private:
    unsigned int mSerial;
    static unsigned int mNextSerial;
    unsigned int mRefCount;
};

class VertexBufferInterface : angle::NonCopyable
{
  public:
    VertexBufferInterface(BufferFactoryD3D *factory, bool dynamic);
    virtual ~VertexBufferInterface();

    unsigned int getBufferSize() const;
    bool empty() const { return getBufferSize() == 0; }

    unsigned int getSerial() const;

    VertexBuffer *getVertexBuffer() const;

  protected:
    gl::Error discard();

    gl::Error setBufferSize(unsigned int size);
    gl::ErrorOrResult<unsigned int> getSpaceRequired(const gl::VertexAttribute &attrib,
                                                     GLsizei count,
                                                     GLsizei instances) const;
    BufferFactoryD3D *const mFactory;
    VertexBuffer *mVertexBuffer;
    bool mDynamic;
};

class StreamingVertexBufferInterface : public VertexBufferInterface
{
  public:
    StreamingVertexBufferInterface(BufferFactoryD3D *factory, std::size_t initialSize);
    ~StreamingVertexBufferInterface();

    gl::Error storeDynamicAttribute(const gl::VertexAttribute &attrib,
                                    GLenum currentValueType,
                                    GLint start,
                                    GLsizei count,
                                    GLsizei instances,
                                    unsigned int *outStreamOffset,
                                    const uint8_t *sourceData);

    gl::Error reserveVertexSpace(const gl::VertexAttribute &attribute,
                                 GLsizei count,
                                 GLsizei instances);

  private:
    gl::Error reserveSpace(unsigned int size);

    unsigned int mWritePosition;
    unsigned int mReservedSpace;
};

class StaticVertexBufferInterface : public VertexBufferInterface
{
  public:
    explicit StaticVertexBufferInterface(BufferFactoryD3D *factory);
    ~StaticVertexBufferInterface();

    gl::Error storeStaticAttribute(const gl::VertexAttribute &attrib,
                                   GLint start,
                                   GLsizei count,
                                   GLsizei instances,
                                   const uint8_t *sourceData);

    bool matchesAttribute(const gl::VertexAttribute &attribute) const;
    void setAttribute(const gl::VertexAttribute &attribute);

  private:
    class AttributeSignature final : angle::NonCopyable
    {
      public:
        AttributeSignature();
        bool matchesAttribute(const gl::VertexAttribute &attrib) const;
        void set(const gl::VertexAttribute &attrib);

      private:
        GLenum type;
        GLuint size;
        GLuint stride;
        bool normalized;
        bool pureInteger;
        size_t offset;
    };

    AttributeSignature mSignature;
};

}  // namespace rx

#endif // LIBANGLE_RENDERER_D3D_VERTEXBUFFER_H_
