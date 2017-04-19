//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the state classes for mananging GLES 3.1 Vertex Array Objects.
//

#include "libANGLE/VertexAttribute.h"

namespace gl
{

// [OpenGL ES 3.1] (November 3, 2016) Section 20 Page 361
// Table 20.2: Vertex Array Object State
VertexBinding::VertexBinding() : stride(16u), divisor(0), offset(0)
{
}

VertexBinding::VertexBinding(VertexBinding &&binding)
{
    *this = std::move(binding);
}

VertexBinding &VertexBinding::operator=(VertexBinding &&binding)
{
    if (this != &binding)
    {
        stride  = binding.stride;
        divisor = binding.divisor;
        offset  = binding.offset;

        buffer.set(binding.buffer.get());
        binding.buffer.set(nullptr);
    }
    return *this;
}

VertexAttribute::VertexAttribute(GLuint bindingIndex)
    : enabled(false),
      type(GL_FLOAT),
      size(4u),
      normalized(false),
      pureInteger(false),
      pointer(nullptr),
      relativeOffset(0),
      vertexAttribArrayStride(0),
      bindingIndex(bindingIndex)
{
}

VertexAttribute::VertexAttribute(VertexAttribute &&attrib)
    : enabled(attrib.enabled),
      type(attrib.type),
      size(attrib.size),
      normalized(attrib.normalized),
      pureInteger(attrib.pureInteger),
      pointer(attrib.pointer),
      relativeOffset(attrib.relativeOffset),
      vertexAttribArrayStride(attrib.vertexAttribArrayStride),
      bindingIndex(attrib.bindingIndex)
{
}

VertexAttribute &VertexAttribute::operator=(VertexAttribute &&attrib)
{
    if (this != &attrib)
    {
        enabled                 = attrib.enabled;
        type                    = attrib.type;
        size                    = attrib.size;
        normalized              = attrib.normalized;
        pureInteger             = attrib.pureInteger;
        pointer                 = attrib.pointer;
        relativeOffset          = attrib.relativeOffset;
        vertexAttribArrayStride = attrib.vertexAttribArrayStride;
        bindingIndex            = attrib.bindingIndex;
    }
    return *this;
}

size_t ComputeVertexAttributeTypeSize(const VertexAttribute& attrib)
{
    GLuint size = attrib.size;
    switch (attrib.type)
    {
      case GL_BYTE:                        return size * sizeof(GLbyte);
      case GL_UNSIGNED_BYTE:               return size * sizeof(GLubyte);
      case GL_SHORT:                       return size * sizeof(GLshort);
      case GL_UNSIGNED_SHORT:              return size * sizeof(GLushort);
      case GL_INT:                         return size * sizeof(GLint);
      case GL_UNSIGNED_INT:                return size * sizeof(GLuint);
      case GL_INT_2_10_10_10_REV:          return 4;
      case GL_UNSIGNED_INT_2_10_10_10_REV: return 4;
      case GL_FIXED:                       return size * sizeof(GLfixed);
      case GL_HALF_FLOAT:                  return size * sizeof(GLhalf);
      case GL_FLOAT:                       return size * sizeof(GLfloat);
      default: UNREACHABLE();              return size * sizeof(GLfloat);
    }
}

size_t ComputeVertexAttributeStride(const VertexAttribute &attrib, const VertexBinding &binding)
{
    // In ES 3.1, VertexAttribPointer will store the type size in the binding stride.
    // Hence, rendering always uses the binding's stride.
    return attrib.enabled ? binding.stride : 16u;
}

// Warning: you should ensure binding really matches attrib.bindingIndex before using this function.
GLintptr ComputeVertexAttributeOffset(const VertexAttribute &attrib, const VertexBinding &binding)
{
    return attrib.relativeOffset + binding.offset;
}

size_t ComputeVertexBindingElementCount(const VertexBinding &binding,
                                        size_t drawCount,
                                        size_t instanceCount)
{
    // For instanced rendering, we draw "instanceDrawCount" sets of "vertexDrawCount" vertices.
    //
    // A vertex attribute with a positive divisor loads one instanced vertex for every set of
    // non-instanced vertices, and the instanced vertex index advances once every "mDivisor"
    // instances.
    if (instanceCount > 0 && binding.divisor > 0)
    {
        // When instanceDrawCount is not a multiple attrib.divisor, the division must round up.
        // For instance, with 5 non-instanced vertices and a divisor equal to 3, we need 2 instanced
        // vertices.
        return (instanceCount + binding.divisor - 1u) / binding.divisor;
    }

    return drawCount;
}

}  // namespace gl
