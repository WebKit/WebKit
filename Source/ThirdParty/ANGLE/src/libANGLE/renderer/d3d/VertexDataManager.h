//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// VertexDataManager.h: Defines the VertexDataManager, a class that
// runs the Buffer translation process.

#ifndef LIBANGLE_RENDERER_D3D_VERTEXDATAMANAGER_H_
#define LIBANGLE_RENDERER_D3D_VERTEXDATAMANAGER_H_

#include "libANGLE/Constants.h"
#include "libANGLE/VertexAttribute.h"
#include "common/angleutils.h"

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

struct TranslatedAttribute
{
    TranslatedAttribute() : active(false), attribute(NULL), currentValueType(GL_NONE),
                            offset(0), stride(0), vertexBuffer(NULL), storage(NULL),
                            serial(0), divisor(0) {};
    bool active;

    const gl::VertexAttribute *attribute;
    GLenum currentValueType;
    unsigned int offset;
    unsigned int stride;   // 0 means not to advance the read pointer at all

    VertexBuffer *vertexBuffer;
    BufferD3D *storage;
    unsigned int serial;
    unsigned int divisor;
};

class VertexDataManager : angle::NonCopyable
{
  public:
    VertexDataManager(BufferFactoryD3D *factory);
    virtual ~VertexDataManager();

    gl::Error prepareVertexData(const gl::State &state, GLint start, GLsizei count,
                                TranslatedAttribute *outAttribs, GLsizei instances);

  private:
    gl::Error reserveSpaceForAttrib(const gl::VertexAttribute &attrib,
                                    const gl::VertexAttribCurrentValueData &currentValue,
                                    GLsizei count,
                                    GLsizei instances) const;

    void invalidateMatchingStaticData(const gl::VertexAttribute &attrib,
                                      const gl::VertexAttribCurrentValueData &currentValue) const;

    gl::Error storeAttribute(const gl::VertexAttribute &attrib,
                             const gl::VertexAttribCurrentValueData &currentValue,
                             TranslatedAttribute *translated,
                             GLint start,
                             GLsizei count,
                             GLsizei instances);

    gl::Error storeCurrentValue(const gl::VertexAttribute &attrib,
                                const gl::VertexAttribCurrentValueData &currentValue,
                                TranslatedAttribute *translated,
                                gl::VertexAttribCurrentValueData *cachedValue,
                                size_t *cachedOffset,
                                StreamingVertexBufferInterface *buffer);

    void hintUnmapAllResources(const std::vector<gl::VertexAttribute> &vertexAttributes);

    BufferFactoryD3D *const mFactory;

    StreamingVertexBufferInterface *mStreamingBuffer;

    gl::VertexAttribCurrentValueData mCurrentValue[gl::MAX_VERTEX_ATTRIBS];

    StreamingVertexBufferInterface *mCurrentValueBuffer[gl::MAX_VERTEX_ATTRIBS];
    std::size_t mCurrentValueOffsets[gl::MAX_VERTEX_ATTRIBS];
};

}

#endif   // LIBANGLE_RENDERER_D3D_VERTEXDATAMANAGER_H_
