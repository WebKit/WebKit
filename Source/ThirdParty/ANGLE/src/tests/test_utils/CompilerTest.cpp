//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "tests/test_utils/CompilerTest.h"

GLuint CompiledShader::compile(GLenum type, const char *source)
{
    mShader = glCreateShader(type);

    const char *sourceArray[1] = {source};
    glShaderSource(mShader, 1, sourceArray, nullptr);
    glCompileShader(mShader);

    GLint compileResult;
    glGetShaderiv(mShader, GL_COMPILE_STATUS, &compileResult);

    GLint infoLogLength;
    glGetShaderiv(mShader, GL_INFO_LOG_LENGTH, &infoLogLength);
    mInfoLog.clear();

    if (infoLogLength > 0)
    {
        std::vector<GLchar> infoLog(infoLogLength);
        glGetShaderInfoLog(mShader, infoLogLength, nullptr, infoLog.data());

        // Note: the info log is nul-terminated.
        mInfoLog = infoLog.data();
    }

    if (compileResult == 0)
    {
        std::cerr << "shader compilation failed: "
                  << (mInfoLog.empty() ? "<Empty message log>" : mInfoLog.data());

        destroy();
    }
    else if (IsGLExtensionEnabled("GL_ANGLE_translated_shader_source"))
    {
        GLint translatedSourceLength;
        glGetShaderiv(mShader, GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE, &translatedSourceLength);

        if (translatedSourceLength > 0)
        {
            std::vector<GLchar> translatedSource(translatedSourceLength);
            glGetTranslatedShaderSourceANGLE(mShader, translatedSourceLength, nullptr,
                                             translatedSource.data());

            // Note: the translatedSource is nul-terminated.
            mTranslatedSource = translatedSource.data();
        }
    }

    return mShader;
}

CompiledShader::~CompiledShader()
{
    if (mShader != 0)
    {
        destroy();
    }
}

void CompiledShader::destroy()
{
    glDeleteShader(mShader);
    ASSERT_GL_NO_ERROR();
    mShader = 0;

    mTranslatedSource.clear();
}

bool CompiledShader::hasInfoLog(const char *expect) const
{
    return mInfoLog.find(expect) != std::string::npos;
}

bool CompiledShader::verifyInTranslatedSource(const char *expect) const
{
    return mTranslatedSource.empty() || mTranslatedSource.find(expect) != std::string::npos;
}

bool CompiledShader::verifyNotInTranslatedSource(const char *expect) const
{
    return mTranslatedSource.empty() || mTranslatedSource.find(expect) == std::string::npos;
}

void CompilerTest::testSetUp() {}

void CompilerTest::testTearDown()
{
    reset();
}

void CompilerTest::reset()
{
    mVertexShader.destroy();
    mTessellationControlShader.destroy();
    mTessellationEvaluationShader.destroy();
    mGeometryShader.destroy();
    mFragmentShader.destroy();
    mComputeShader.destroy();

    glDeleteProgram(mProgram);
    mProgram = 0;
}

const CompiledShader &CompilerTest::compile(GLenum type, const char *source)
{
    CompiledShader &shader = getCompiledShader(type);
    shader.compile(type, source);
    return shader;
}

CompiledShader &CompilerTest::getCompiledShader(GLenum type)
{
    switch (type)
    {
        case GL_VERTEX_SHADER:
            return mVertexShader;
        case GL_TESS_CONTROL_SHADER:
            return mTessellationControlShader;
        case GL_TESS_EVALUATION_SHADER:
            return mTessellationEvaluationShader;
        case GL_GEOMETRY_SHADER:
            return mGeometryShader;
        case GL_FRAGMENT_SHADER:
            return mFragmentShader;
        case GL_COMPUTE_SHADER:
            return mComputeShader;
        default:
            // Unsupported type
            ASSERT(false);
            return mVertexShader;
    }
}

GLuint CompilerTest::link()
{
    mProgram = glCreateProgram();

    if (mComputeShader.success())
    {
        glAttachShader(mProgram, mComputeShader.getShader());
    }
    else
    {
        if (mVertexShader.success())
        {
            glAttachShader(mProgram, mVertexShader.getShader());
        }
        if (mTessellationControlShader.success())
        {
            glAttachShader(mProgram, mTessellationControlShader.getShader());
        }
        if (mTessellationEvaluationShader.success())
        {
            glAttachShader(mProgram, mTessellationEvaluationShader.getShader());
        }
        if (mGeometryShader.success())
        {
            glAttachShader(mProgram, mGeometryShader.getShader());
        }
        if (mFragmentShader.success())
        {
            glAttachShader(mProgram, mFragmentShader.getShader());
        }
    }

    glLinkProgram(mProgram);
    EXPECT_GL_NO_ERROR();

    GLint linkStatus;
    glGetProgramiv(mProgram, GL_LINK_STATUS, &linkStatus);
    EXPECT_NE(linkStatus, 0);

    return mProgram;
}
