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
class GLWrapper : angle::NonCopyable
{
  public:
    GLWrapper() {}
    ~GLWrapper() { DeleteF(1, &mHandle); }

    // The move-constructor and move-assignment operators are necessary so that the data within a
    // GLWrapper object can be relocated.
    GLWrapper(GLWrapper &&rht) : mHandle(rht.mHandle) { rht.mHandle = 0u; }
    GLWrapper &operator=(GLWrapper &&rht)
    {
        if (this != &rht)
        {
            std::swap(mHandle, rht.mHandle);
        }
        return *this;
    }

    void reset()
    {
        if (mHandle != 0u)
        {
            DeleteF(1, &mHandle);
            mHandle = 0u;
        }
    }

    GLuint get()
    {
        if (!mHandle)
        {
            GenF(1, &mHandle);
        }
        return mHandle;
    }

    operator GLuint() { return get(); }

  private:
    GLuint mHandle = 0u;
};

using GLVertexArray       = GLWrapper<glGenVertexArrays, glDeleteVertexArrays>;
using GLBuffer            = GLWrapper<glGenBuffers, glDeleteBuffers>;
using GLTexture           = GLWrapper<glGenTextures, glDeleteTextures>;
using GLFramebuffer       = GLWrapper<glGenFramebuffers, glDeleteFramebuffers>;
using GLRenderbuffer      = GLWrapper<glGenRenderbuffers, glDeleteRenderbuffers>;
using GLSampler           = GLWrapper<glGenSamplers, glDeleteSamplers>;
using GLTransformFeedback = GLWrapper<glGenTransformFeedbacks, glDeleteTransformFeedbacks>;
using GLProgramPipeline   = GLWrapper<glGenProgramPipelines, glDeleteProgramPipelines>;
using GLQueryEXT          = GLWrapper<glGenQueriesEXT, glDeleteQueriesEXT>;

// Don't use GLProgram directly, use ANGLE_GL_PROGRAM.
namespace priv
{
class GLProgram
{
  public:
    GLProgram() : mHandle(0) {}

    ~GLProgram() { glDeleteProgram(mHandle); }

    void makeCompute(const std::string &computeShader)
    {
        mHandle = CompileComputeProgram(computeShader);
    }

    void makeRaster(const std::string &vertexShader, const std::string &fragmentShader)
    {
        mHandle = CompileProgram(vertexShader, fragmentShader);
    }

    void makeRasterWithTransformFeedback(const std::string &vertexShader,
                                         const std::string &fragmentShader,
                                         const std::vector<std::string> &tfVaryings,
                                         GLenum bufferMode)
    {
        mHandle = CompileProgramWithTransformFeedback(vertexShader, fragmentShader, tfVaryings,
                                                      bufferMode);
    }

    void makeBinaryOES(const std::vector<uint8_t> &binary, GLenum binaryFormat)
    {
        mHandle = LoadBinaryProgramOES(binary, binaryFormat);
    }

    void makeBinaryES3(const std::vector<uint8_t> &binary, GLenum binaryFormat)
    {
        mHandle = LoadBinaryProgramES3(binary, binaryFormat);
    }

    bool valid() const { return mHandle != 0; }

    GLuint get()
    {
        ASSERT(valid());
        return mHandle;
    }

    operator GLuint() { return get(); }

  private:
    GLuint mHandle;
};
}  // namespace priv

#define ANGLE_GL_PROGRAM(name, vertex, fragment) \
    priv::GLProgram name;                        \
    name.makeRaster(vertex, fragment);           \
    ASSERT_TRUE(name.valid());

#define ANGLE_GL_PROGRAM_TRANSFORM_FEEDBACK(name, vertex, fragment, tfVaryings, bufferMode) \
    priv::GLProgram name;                                                                   \
    name.makeRasterWithTransformFeedback(vertex, fragment, tfVaryings, bufferMode);         \
    ASSERT_TRUE(name.valid());

#define ANGLE_GL_COMPUTE_PROGRAM(name, compute) \
    priv::GLProgram name;                       \
    name.makeCompute(compute);                  \
    ASSERT_TRUE(name.valid());

#define ANGLE_GL_BINARY_OES_PROGRAM(name, binary, binaryFormat) \
    priv::GLProgram name;                                       \
    name.makeBinaryOES(binary, binaryFormat);                   \
    ASSERT_TRUE(name.valid());

#define ANGLE_GL_BINARY_ES3_PROGRAM(name, binary, binaryFormat) \
    priv::GLProgram name;                                       \
    name.makeBinaryES3(binary, binaryFormat);                   \
    ASSERT_TRUE(name.valid());

}  // namespace angle

#endif  // ANGLE_TESTS_GL_RAII_H_
