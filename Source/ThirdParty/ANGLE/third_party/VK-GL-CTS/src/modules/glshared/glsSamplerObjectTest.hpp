#ifndef _GLSSAMPLEROBJECTTEST_HPP
#define _GLSSAMPLEROBJECTTEST_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
 * -------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Sampler object testcases.
 *//*--------------------------------------------------------------------*/

#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"
#include "deRandom.hpp"
#include "tcuSurface.hpp"
#include "gluRenderContext.hpp"
#include "glw.h"
#include "glwEnums.hpp"
#include "gluShaderProgram.hpp"

namespace deqp
{
namespace gls
{

class TextureSamplerTest : public tcu::TestCase
{
public:
    struct SamplingState
    {
        GLenum minFilter;
        GLenum magFilter;
        GLenum wrapT;
        GLenum wrapS;
        GLenum wrapR;
        GLfloat minLod;
        GLfloat maxLod;
    };

    struct TestSpec
    {
        const char *name;
        const char *desc;
        GLenum target;
        SamplingState textureState;
        SamplingState samplerState;
    };

    TextureSamplerTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const TestSpec &spec);
    ~TextureSamplerTest(void);

    void init(void);
    void deinit(void);

    IterateResult iterate(void);

private:
    void renderReferences(tcu::Surface &textureRef, tcu::Surface &samplerRef, int x, int y);
    void renderResults(tcu::Surface &textureResult, tcu::Surface &samplerResult, int x, int y);

    void render(void);

    static void setTextureState(const glw::Functions &gl, GLenum target, SamplingState state);
    static void setSamplerState(const glw::Functions &gl, SamplingState state, GLuint sampler);

    static GLuint createTexture2D(const glw::Functions &gl);
    static GLuint createTexture3D(const glw::Functions &gl);
    static GLuint createTextureCube(const glw::Functions &gl);
    static GLuint createTexture(const glw::Functions &gl, GLenum target);

    static const char *selectVertexShader(GLenum target);
    static const char *selectFragmentShader(GLenum target);

    glu::RenderContext &m_renderCtx;
    glu::ShaderProgram *m_program;

    GLenum m_target;
    SamplingState m_textureState;
    SamplingState m_samplerState;

    de::Random m_random;
};

class MultiTextureSamplerTest : public tcu::TestCase
{
public:
    struct SamplingState
    {
        GLenum minFilter;
        GLenum magFilter;
        GLenum wrapT;
        GLenum wrapS;
        GLenum wrapR;
        GLfloat minLod;
        GLfloat maxLod;
    };

    struct TestSpec
    {
        const char *name;
        const char *desc;
        GLenum target;
        SamplingState textureState1;
        SamplingState textureState2;
        SamplingState samplerState;
    };

    MultiTextureSamplerTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const TestSpec &spec);
    ~MultiTextureSamplerTest(void);

    void init(void);
    void deinit(void);

    IterateResult iterate(void);

private:
    void renderReferences(tcu::Surface &textureRef, tcu::Surface &samplerRef, int x, int y);
    void renderResults(tcu::Surface &textureResult, tcu::Surface &samplerResult, int x, int y);

    void render(void);

    static void setTextureState(const glw::Functions &gl, GLenum target, SamplingState state);
    static void setSamplerState(const glw::Functions &gl, SamplingState state, GLuint sampler);

    static GLuint createTexture2D(const glw::Functions &gl, int id);
    static GLuint createTexture3D(const glw::Functions &gl, int id);
    static GLuint createTextureCube(const glw::Functions &gl, int id);
    static GLuint createTexture(const glw::Functions &gl, GLenum target, int id);

    static const char *selectVertexShader(GLenum target);
    static const char *selectFragmentShader(GLenum target);

    glu::RenderContext &m_renderCtx;
    glu::ShaderProgram *m_program;

    GLenum m_target;
    SamplingState m_textureState1;
    SamplingState m_textureState2;
    SamplingState m_samplerState;

    de::Random m_random;
};

} // namespace gls
} // namespace deqp

#endif // _GLSSAMPLEROBJECTTEST_HPP
