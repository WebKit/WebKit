//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// VertexDataManager.h: Defines the VertexDataManager, a class that
// runs the Buffer translation process.

#ifndef LIBANGLE_RENDERER_D3D_VERTEXDATAMANAGER_H_
#define LIBANGLE_RENDERER_D3D_VERTEXDATAMANAGER_H_

#include "common/angleutils.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/Constants.h"
#include "libANGLE/VertexAttribute.h"

namespace gl
{
class State;
struct VertexAttribute;
struct VertexAttribCurrentValueData;
}

namespace rx
{
class BufferD3D;
class BufferFactoryD3D;
class StreamingVertexBufferInterface;
class VertexBuffer;

class VertexBufferBinding final
{
  public:
    VertexBufferBinding();
    VertexBufferBinding(const VertexBufferBinding &other);
    ~VertexBufferBinding();

    void set(VertexBuffer *vertexBuffer);
    VertexBuffer *get() const;
    VertexBufferBinding &operator=(const VertexBufferBinding &other);

  private:
    VertexBuffer *mBoundVertexBuffer;
};

struct TranslatedAttribute
{
    TranslatedAttribute();

    // Computes the correct offset from baseOffset, usesFirstVertexOffset, stride and startVertex.
    // Can throw an error on integer overflow.
    gl::ErrorOrResult<unsigned int> computeOffset(GLint startVertex) const;

    bool active;

    const gl::VertexAttribute *attribute;
    GLenum currentValueType;
    unsigned int baseOffset;
    bool usesFirstVertexOffset;
    unsigned int stride;   // 0 means not to advance the read pointer at all

    VertexBufferBinding vertexBuffer;
    BufferD3D *storage;
    unsigned int serial;
    unsigned int divisor;
};

enum class VertexStorageType
{
    UNKNOWN,
    STATIC,         // Translate the vertex data once and re-use it.
    DYNAMIC,        // Translate the data every frame into a ring buffer.
    DIRECT,         // Bind a D3D buffer directly without any translation.
    CURRENT_VALUE,  // Use a single value for the attribute.
};

// Given a vertex attribute, return the type of storage it will use.
VertexStorageType ClassifyAttributeStorage(const gl::VertexAttribute &attrib);

class VertexDataManager : angle::NonCopyable
{
  public:
    VertexDataManager(BufferFactoryD3D *factory);
    virtual ~VertexDataManager();

    gl::Error prepareVertexData(const gl::State &state,
                                GLint start,
                                GLsizei count,
                                std::vector<TranslatedAttribute> *translatedAttribs,
                                GLsizei instances);

    static void StoreDirectAttrib(TranslatedAttribute *directAttrib);

    static gl::Error StoreStaticAttrib(TranslatedAttribute *translated,
                                       GLsizei count,
                                       GLsizei instances);

    gl::Error storeDynamicAttribs(std::vector<TranslatedAttribute> *translatedAttribs,
                                  const gl::AttributesMask &dynamicAttribsMask,
                                  GLint start,
                                  GLsizei count,
                                  GLsizei instances);

    // Promote static usage of dynamic buffers.
    static void PromoteDynamicAttribs(const std::vector<TranslatedAttribute> &translatedAttribs,
                                      const gl::AttributesMask &dynamicAttribsMask,
                                      GLsizei count);

    gl::Error storeCurrentValue(const gl::VertexAttribCurrentValueData &currentValue,
                                TranslatedAttribute *translated,
                                size_t attribIndex);

  private:
    struct CurrentValueState
    {
        CurrentValueState();
        ~CurrentValueState();

        StreamingVertexBufferInterface *buffer;
        gl::VertexAttribCurrentValueData data;
        size_t offset;
    };

    gl::Error reserveSpaceForAttrib(const TranslatedAttribute &translatedAttrib,
                                    GLsizei count,
                                    GLsizei instances) const;

    gl::Error storeDynamicAttrib(TranslatedAttribute *translated,
                                 GLint start,
                                 GLsizei count,
                                 GLsizei instances);

    BufferFactoryD3D *const mFactory;

    StreamingVertexBufferInterface *mStreamingBuffer;
    std::vector<CurrentValueState> mCurrentValueCache;
    gl::AttributesMask mDynamicAttribsMaskCache;
};

}  // namespace rx

#endif   // LIBANGLE_RENDERER_D3D_VERTEXDATAMANAGER_H_
