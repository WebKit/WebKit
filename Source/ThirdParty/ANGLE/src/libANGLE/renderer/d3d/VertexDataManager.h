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
class VertexBinding;
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
    TranslatedAttribute(const TranslatedAttribute &other);

    // Computes the correct offset from baseOffset, usesFirstVertexOffset, stride and startVertex.
    // Can throw an error on integer overflow.
    gl::ErrorOrResult<unsigned int> computeOffset(GLint startVertex) const;

    bool active;

    const gl::VertexAttribute *attribute;
    const gl::VertexBinding *binding;
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
VertexStorageType ClassifyAttributeStorage(const gl::VertexAttribute &attrib,
                                           const gl::VertexBinding &binding);

class VertexDataManager : angle::NonCopyable
{
  public:
    VertexDataManager(BufferFactoryD3D *factory);
    virtual ~VertexDataManager();

    gl::Error initialize();
    void deinitialize();

    gl::Error prepareVertexData(const gl::Context *context,
                                GLint start,
                                GLsizei count,
                                std::vector<TranslatedAttribute> *translatedAttribs,
                                GLsizei instances);

    static void StoreDirectAttrib(TranslatedAttribute *directAttrib);

    static gl::Error StoreStaticAttrib(const gl::Context *context, TranslatedAttribute *translated);

    gl::Error storeDynamicAttribs(const gl::Context *context,
                                  std::vector<TranslatedAttribute> *translatedAttribs,
                                  const gl::AttributesMask &dynamicAttribsMask,
                                  GLint start,
                                  GLsizei count,
                                  GLsizei instances);

    // Promote static usage of dynamic buffers.
    static void PromoteDynamicAttribs(const gl::Context *context,
                                      const std::vector<TranslatedAttribute> &translatedAttribs,
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

        std::unique_ptr<StreamingVertexBufferInterface> buffer;
        gl::VertexAttribCurrentValueData data;
        size_t offset;
    };

    gl::Error reserveSpaceForAttrib(const TranslatedAttribute &translatedAttrib,
                                    GLsizei count,
                                    GLint start,
                                    GLsizei instances) const;

    gl::Error storeDynamicAttrib(const gl::Context *context,
                                 TranslatedAttribute *translated,
                                 GLint start,
                                 GLsizei count,
                                 GLsizei instances);

    BufferFactoryD3D *const mFactory;

    std::unique_ptr<StreamingVertexBufferInterface> mStreamingBuffer;
    std::vector<CurrentValueState> mCurrentValueCache;
    gl::AttributesMask mDynamicAttribsMaskCache;
};

}  // namespace rx

#endif   // LIBANGLE_RENDERER_D3D_VERTEXDATAMANAGER_H_
