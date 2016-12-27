//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// gl_raii:
//   Helper methods for containing GL objects like buffers and textures.

#ifndef ANGLE_TESTS_GL_RAII_H_
#define ANGLE_TESTS_GL_RAII_H_

#include <functional>

#include "angle_gl.h"

namespace angle
{

// This is a bit of hack to work around a bug in MSVS intellisense, and make it very easy to
// use the correct function pointer type without worrying about the various definitions of
// GL_APICALL.
using GLGen    = decltype(glGenBuffers);
using GLDelete = decltype(glDeleteBuffers);

template <GLGen GenF, GLDelete DeleteF>
class GLWrapper
{
  public:
    GLWrapper() {}
    ~GLWrapper() { DeleteF(1, &mHandle); }

    GLuint get()
    {
        if (!mHandle)
        {
            GenF(1, &mHandle);
        }
        return mHandle;
    }

  private:
    GLuint mHandle = 0;
};

using GLBuffer       = GLWrapper<glGenBuffers, glDeleteBuffers>;
using GLTexture      = GLWrapper<glGenTextures, glDeleteTextures>;
using GLFramebuffer  = GLWrapper<glGenFramebuffers, glDeleteFramebuffers>;
using GLRenderbuffer = GLWrapper<glGenRenderbuffers, glDeleteRenderbuffers>;

class GLProgram
{
  public:
    GLProgram(const std::string &vertexShader, const std::string &fragmentShader)
        : mHandle(0), mVertexShader(vertexShader), mFragmentShader(fragmentShader)
    {
    }

    GLProgram(const std::string &computeShader) : mHandle(0), mComputeShader(computeShader) {}

    ~GLProgram() { glDeleteProgram(mHandle); }

    GLuint get()
    {
        if (mHandle == 0)
        {
            if (!mComputeShader.empty())
            {
                mHandle = CompileComputeProgram(mComputeShader);
            }
            else
            {
                mHandle = CompileProgram(mVertexShader, mFragmentShader);
            }
        }
        return mHandle;
    }

  private:
    GLuint mHandle;
    const std::string mVertexShader;
    const std::string mFragmentShader;
    const std::string mComputeShader;
};

#define ANGLE_GL_PROGRAM(name, vertex, fragment) \
    GLProgram name(vertex, fragment);            \
    ASSERT_NE(0u, name.get());

#define ANGLE_GL_COMPUTE_PROGRAM(name, compute) \
    GLProgram name(compute);                    \
    ASSERT_NE(0u, name.get());

}  // namespace angle

#endif  // ANGLE_TESTS_GL_RAII_H_
