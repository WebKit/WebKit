//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper structure describing a single vertex attribute
//

#ifndef LIBGLESV2_VERTEXATTRIBUTE_H_
#define LIBGLESV2_VERTEXATTRIBUTE_H_

#include "libGLESv2/Buffer.h"

namespace gl
{

class VertexAttribute
{
  public:
    VertexAttribute() : mType(GL_FLOAT), mSize(4), mNormalized(false), mPureInteger(false),
                        mStride(0), mPointer(NULL), mArrayEnabled(false), mDivisor(0)
    {
    }

    int typeSize() const
    {
        switch (mType)
        {
          case GL_BYTE:                        return mSize * sizeof(GLbyte);
          case GL_UNSIGNED_BYTE:               return mSize * sizeof(GLubyte);
          case GL_SHORT:                       return mSize * sizeof(GLshort);
          case GL_UNSIGNED_SHORT:              return mSize * sizeof(GLushort);
          case GL_INT:                         return mSize * sizeof(GLint);
          case GL_UNSIGNED_INT:                return mSize * sizeof(GLuint);
          case GL_INT_2_10_10_10_REV:          return 4;
          case GL_UNSIGNED_INT_2_10_10_10_REV: return 4;
          case GL_FIXED:                       return mSize * sizeof(GLfixed);
          case GL_HALF_FLOAT:                  return mSize * sizeof(GLhalf);
          case GL_FLOAT:                       return mSize * sizeof(GLfloat);
          default: UNREACHABLE();              return mSize * sizeof(GLfloat);
        }
    }

    GLsizei stride() const
    {
        return (mArrayEnabled ? (mStride ? mStride : typeSize()) : 16);
    }

    void setState(gl::Buffer *boundBuffer, GLint size, GLenum type, bool normalized,
                  bool pureInteger, GLsizei stride, const void *pointer)
    {
        mBoundBuffer.set(boundBuffer);
        mSize = size;
        mType = type;
        mNormalized = normalized;
        mPureInteger = pureInteger;
        mStride = stride;
        mPointer = pointer;
    }

    template <typename T>
    T querySingleParameter(GLenum pname) const
    {
        switch (pname)
        {
          case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
            return static_cast<T>(mArrayEnabled ? GL_TRUE : GL_FALSE);
          case GL_VERTEX_ATTRIB_ARRAY_SIZE:
            return static_cast<T>(mSize);
          case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
            return static_cast<T>(mStride);
          case GL_VERTEX_ATTRIB_ARRAY_TYPE:
            return static_cast<T>(mType);
          case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
            return static_cast<T>(mNormalized ? GL_TRUE : GL_FALSE);
          case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
            return static_cast<T>(mBoundBuffer.id());
          case GL_VERTEX_ATTRIB_ARRAY_DIVISOR:
            return static_cast<T>(mDivisor);
          case GL_VERTEX_ATTRIB_ARRAY_INTEGER:
            return static_cast<T>(mPureInteger ? GL_TRUE : GL_FALSE);
          default:
            UNREACHABLE();
            return static_cast<T>(0);
        }
    }

    // From glVertexAttribPointer
    GLenum mType;
    GLint mSize;
    bool mNormalized;
    bool mPureInteger;
    GLsizei mStride;   // 0 means natural stride

    union
    {
        const void *mPointer;
        intptr_t mOffset;
    };

    BindingPointer<Buffer> mBoundBuffer;   // Captured when glVertexAttribPointer is called.
    bool mArrayEnabled;   // From glEnable/DisableVertexAttribArray
    unsigned int mDivisor;
};

struct VertexAttribCurrentValueData
{
    union
    {
        GLfloat FloatValues[4];
        GLint IntValues[4];
        GLuint UnsignedIntValues[4];
    };
    GLenum Type;

    void setFloatValues(const GLfloat floatValues[4])
    {
        for (unsigned int valueIndex = 0; valueIndex < 4; valueIndex++)
        {
            FloatValues[valueIndex] = floatValues[valueIndex];
        }
        Type = GL_FLOAT;
    }

    void setIntValues(const GLint intValues[4])
    {
        for (unsigned int valueIndex = 0; valueIndex < 4; valueIndex++)
        {
            IntValues[valueIndex] = intValues[valueIndex];
        }
        Type = GL_INT;
    }

    void setUnsignedIntValues(const GLuint unsignedIntValues[4])
    {
        for (unsigned int valueIndex = 0; valueIndex < 4; valueIndex++)
        {
            UnsignedIntValues[valueIndex] = unsignedIntValues[valueIndex];
        }
        Type = GL_UNSIGNED_INT;
    }

    bool operator==(const VertexAttribCurrentValueData &other)
    {
        return (Type == other.Type && memcmp(FloatValues, other.FloatValues, sizeof(float) * 4) == 0);
    }

    bool operator!=(const VertexAttribCurrentValueData &other)
    {
        return !(*this == other);
    }
};

}

#endif // LIBGLESV2_VERTEXATTRIBUTE_H_
