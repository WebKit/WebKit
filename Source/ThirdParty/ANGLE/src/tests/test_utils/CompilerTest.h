//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CompilerTest.h:
//   Helpers to test shader validation and translated output.
//

#ifndef TESTS_TEST_UTILS_COMPILER_TEST_H_
#define TESTS_TEST_UTILS_COMPILER_TEST_H_

#include "test_utils/ANGLETest.h"

class CompiledShader
{
  public:
    ~CompiledShader();

    // Compile the source and return the GL shader object.
    //
    // If compilation is successful, the return value will be non-zero and mTranslatedSource will
    // contain the translated source if ANGLE_translated_shader_source is supported.  Note that with
    // SPIR-V, the translated source is mostly hex numbers, so the test pretends the extension is
    // not supported in that case.  If compilation fails, 0 is returned.
    //
    // In either case, the compiler's logs are placed in mInfoLog.
    GLuint compile(GLenum type, const char *source);
    void destroy();

    bool success() const { return mShader != 0; }
    GLuint getShader() const { return mShader; }
    const std::string &getInfoLog() const { return mInfoLog; }
    const std::string &getTranslatedSource() const { return mTranslatedSource; }

    // Returns true if the expected message is found in the info log.
    bool hasInfoLog(const char *expect) const;

    // Returns true if either the translated source does not exist or it contains this substring.
    bool verifyInTranslatedSource(const char *expect) const;
    // Returns true if either the translated source does not exist or it does not contains this
    // substring.
    bool verifyNotInTranslatedSource(const char *expect) const;

  private:
    std::string mInfoLog;
    std::string mTranslatedSource;
    GLuint mShader = 0;
};

class CompilerTest : public ANGLETest<>
{
  public:
    CompilerTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void reset();

    const CompiledShader &compile(GLenum type, const char *source);
    CompiledShader &getCompiledShader(GLenum type);

    // Link the compiled shaders into a program, and return it.  The focus of this class is the
    // shader compiler, so if asked to link, it assumes that link is going to succeed.
    GLuint link();

  protected:
    void testSetUp() override;
    void testTearDown() override;

  private:
    CompiledShader mVertexShader;
    CompiledShader mTessellationControlShader;
    CompiledShader mTessellationEvaluationShader;
    CompiledShader mGeometryShader;
    CompiledShader mFragmentShader;
    CompiledShader mComputeShader;

    GLuint mProgram = 0;
};

#endif  // TESTS_TEST_UTILS_COMPILER_TEST_H_
