//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Buffer.h: Defines the gl::Buffer class, representing storage of vertex and/or
// index data. Implements GL buffer objects and related functionality.
// [OpenGL ES 2.0.24] section 2.9 page 21.

#ifndef LIBGLESV2_BUFFER_H_
#define LIBGLESV2_BUFFER_H_

#include <cstddef>
#include <vector>

#define GL_APICALL
#include <GLES2/gl2.h>

#include "common/angleutils.h"
#include "libGLESv2/RefCountObject.h"

namespace gl
{

class Buffer : public RefCountObject
{
  public:
    explicit Buffer(GLuint id);

    virtual ~Buffer();

    void bufferData(const void *data, GLsizeiptr size, GLenum usage);
    void bufferSubData(const void *data, GLsizeiptr size, GLintptr offset);

    void *data() { return mContents; }
    size_t size() const { return mSize; }
    GLenum usage() const { return mUsage; }

  private:
    DISALLOW_COPY_AND_ASSIGN(Buffer);

    GLubyte *mContents;
    size_t mSize;
    GLenum mUsage;
};

}

#endif   // LIBGLESV2_BUFFER_H_
