//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper structures about Generic Vertex Attribute.
//

#ifndef LIBANGLE_VERTEXATTRIBUTE_H_
#define LIBANGLE_VERTEXATTRIBUTE_H_

#include "libANGLE/Buffer.h"

namespace gl
{
class VertexArray;

//
// Implementation of Generic Vertex Attribute Bindings for ES3.1. The members are intentionally made
// private in order to hide implementation details.
//
class VertexBinding final : angle::NonCopyable
{
  public:
    VertexBinding();
    VertexBinding(VertexBinding &&binding);
    ~VertexBinding();
    VertexBinding &operator=(VertexBinding &&binding);

    GLuint getStride() const { return mStride; }
    void setStride(GLuint strideIn) { mStride = strideIn; }

    GLuint getDivisor() const { return mDivisor; }
    void setDivisor(GLuint divisorIn) { mDivisor = divisorIn; }

    GLintptr getOffset() const { return mOffset; }
    void setOffset(GLintptr offsetIn) { mOffset = offsetIn; }

    const BindingPointer<Buffer> &getBuffer() const { return mBuffer; }
    void setBuffer(const gl::Context *context, Buffer *bufferIn) { mBuffer.set(context, bufferIn); }

  private:
    GLuint mStride;
    GLuint mDivisor;
    GLintptr mOffset;

    BindingPointer<Buffer> mBuffer;
};

//
// Implementation of Generic Vertex Attributes for ES3.1
//
struct VertexAttribute final : private angle::NonCopyable
{
    explicit VertexAttribute(GLuint bindingIndex);
    explicit VertexAttribute(VertexAttribute &&attrib);
    VertexAttribute &operator=(VertexAttribute &&attrib);

    bool enabled;  // For glEnable/DisableVertexAttribArray
    GLenum type;
    GLuint size;
    bool normalized;
    bool pureInteger;

    const void *pointer;
    GLuint relativeOffset;

    GLuint vertexAttribArrayStride;  // ONLY for queries of VERTEX_ATTRIB_ARRAY_STRIDE
    GLuint bindingIndex;
};

size_t ComputeVertexAttributeTypeSize(const VertexAttribute &attrib);

// Warning: you should ensure binding really matches attrib.bindingIndex before using this function.
size_t ComputeVertexAttributeStride(const VertexAttribute &attrib, const VertexBinding &binding);

// Warning: you should ensure binding really matches attrib.bindingIndex before using this function.
GLintptr ComputeVertexAttributeOffset(const VertexAttribute &attrib, const VertexBinding &binding);

size_t ComputeVertexBindingElementCount(GLuint divisor, size_t drawCount, size_t instanceCount);

GLenum GetVertexAttributeBaseType(const VertexAttribute &attrib);

struct VertexAttribCurrentValueData
{
    union {
        GLfloat FloatValues[4];
        GLint IntValues[4];
        GLuint UnsignedIntValues[4];
    };
    GLenum Type;

    VertexAttribCurrentValueData();

    void setFloatValues(const GLfloat floatValues[4]);
    void setIntValues(const GLint intValues[4]);
    void setUnsignedIntValues(const GLuint unsignedIntValues[4]);
};

bool operator==(const VertexAttribCurrentValueData &a, const VertexAttribCurrentValueData &b);
bool operator!=(const VertexAttribCurrentValueData &a, const VertexAttribCurrentValueData &b);

}  // namespace gl

#include "VertexAttribute.inl"

#endif  // LIBANGLE_VERTEXATTRIBUTE_H_
