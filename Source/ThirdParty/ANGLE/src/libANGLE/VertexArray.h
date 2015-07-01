//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class contains prototypes for representing GLES 3 Vertex Array Objects:
//
//   The buffer objects that are to be used by the vertex stage of the GL are collected
//   together to form a vertex array object. All state related to the definition of data used
//   by the vertex processor is encapsulated in a vertex array object.
//

#ifndef LIBANGLE_VERTEXARRAY_H_
#define LIBANGLE_VERTEXARRAY_H_

#include "libANGLE/RefCountObject.h"
#include "libANGLE/Constants.h"
#include "libANGLE/VertexAttribute.h"

#include <vector>

namespace rx
{
class VertexArrayImpl;
}

namespace gl
{
class Buffer;

class VertexArray
{
  public:
    VertexArray(rx::VertexArrayImpl *impl, GLuint id, size_t maxAttribs);
    ~VertexArray();

    GLuint id() const;

    const VertexAttribute& getVertexAttribute(size_t attributeIndex) const;
    const std::vector<VertexAttribute> &getVertexAttributes() const;

    void detachBuffer(GLuint bufferName);
    void setVertexAttribDivisor(GLuint index, GLuint divisor);
    void enableAttribute(unsigned int attributeIndex, bool enabledState);
    void setAttributeState(unsigned int attributeIndex, gl::Buffer *boundBuffer, GLint size, GLenum type,
                           bool normalized, bool pureInteger, GLsizei stride, const void *pointer);

    Buffer *getElementArrayBuffer() const { return mElementArrayBuffer.get(); }
    void setElementArrayBuffer(Buffer *buffer);
    GLuint getElementArrayBufferId() const { return mElementArrayBuffer.id(); }
    size_t getMaxAttribs() const { return mVertexAttributes.size(); }

    rx::VertexArrayImpl *getImplementation() { return mVertexArray; }
    const rx::VertexArrayImpl *getImplementation() const { return mVertexArray; }

    unsigned int getMaxEnabledAttribute() const { return mMaxEnabledAttribute; }

  private:
    GLuint mId;

    rx::VertexArrayImpl *mVertexArray;
    std::vector<VertexAttribute> mVertexAttributes;
    BindingPointer<Buffer> mElementArrayBuffer;
    unsigned int mMaxEnabledAttribute;
};

}

#endif // LIBANGLE_VERTEXARRAY_H_
