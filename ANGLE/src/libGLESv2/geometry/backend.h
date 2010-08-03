//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// geometry/backend.h: Abstract classes BufferBackEnd, TranslatedVertexBuffer and TranslatedIndexBuffer
// that must be implemented by any API-specific implementation of ANGLE.

#ifndef LIBGLESV2_GEOMETRY_BACKEND_H_
#define LIBGLESV2_GEOMETRY_BACKEND_H_

#include <cstddef>

#define GL_APICALL
#include <GLES2/gl2.h>

#include "libGLESv2/Context.h"

namespace gl
{
class TranslatedVertexBuffer;
class TranslatedIndexBuffer;

struct FormatConverter
{
    bool identity;
    std::size_t outputVertexSize;
    void (*convertArray)(const void *in, std::size_t stride, std::size_t n, void *out);
};

struct TranslatedAttribute
{
    bool enabled;
    bool nonArray;

    // These are the original untranslated values. (Or just have some sort of BufferBackEnd::TranslatedTypeKey.)
    GLenum type;
    std::size_t size;
    bool normalized;

    std::size_t offset;

    std::size_t stride; // 0 means not to advance the read pointer at all

    std::size_t semanticIndex;

    TranslatedVertexBuffer *buffer;
};

class BufferBackEnd
{
  public:
    virtual ~BufferBackEnd() { }

    virtual bool supportIntIndices() = 0;

    virtual TranslatedVertexBuffer *createVertexBuffer(std::size_t size) = 0;
    virtual TranslatedVertexBuffer *createVertexBufferForStrideZero(std::size_t size) = 0;
    virtual TranslatedIndexBuffer *createIndexBuffer(std::size_t size, GLenum type) = 0;
    virtual FormatConverter getFormatConverter(GLenum type, std::size_t size, bool normalize) = 0;

    // For an identity-mappable stream, verify that the stride and offset are okay.
    virtual bool validateStream(GLenum type, std::size_t size, std::size_t stride, std::size_t offset) const = 0;

    virtual GLenum setupIndicesPreDraw(const TranslatedIndexData &indexInfo) = 0;
    virtual GLenum setupAttributesPreDraw(const TranslatedAttribute *attributes) = 0;
};

class TranslatedBuffer
{
  public:
    explicit TranslatedBuffer(std::size_t size) : mBufferSize(size), mCurrentPoint(0) { }
    virtual ~TranslatedBuffer() { }

    std::size_t size() const { return mBufferSize; }

    virtual void *map() = 0;
    virtual void unmap() = 0;

    void reserveSpace(std::size_t requiredSpace);

    void *map(std::size_t requiredSpace, std::size_t *offset);

  protected:
    virtual void recycle() = 0;
    virtual void *streamingMap(std::size_t offset, std::size_t size) = 0;

  private:
    std::size_t mBufferSize;
    std::size_t mCurrentPoint;

    DISALLOW_COPY_AND_ASSIGN(TranslatedBuffer);
};

class TranslatedVertexBuffer : public TranslatedBuffer
{
  public:
    explicit TranslatedVertexBuffer(std::size_t size) : TranslatedBuffer(size) { }
};

class TranslatedIndexBuffer : public TranslatedBuffer
{
  public:
    explicit TranslatedIndexBuffer(std::size_t size) : TranslatedBuffer(size) { }
};

}

#endif   // LIBGLESV2_GEOMETRY_BACKEND_H_
