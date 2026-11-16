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

#include "glsSamplerObjectTest.hpp"

#include "tcuTexture.hpp"
#include "tcuSurface.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "tcuRGBA.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuStringTemplate.hpp"

#include "gluShaderProgram.hpp"
#include "gluPixelTransfer.hpp"
#include "gluDrawUtil.hpp"
#include "gluRenderContext.hpp"
#include "gluTextureUtil.hpp"

#include "glwFunctions.hpp"

#include "deRandom.hpp"
#include "deString.h"

#include "deString.h"

#include <map>

namespace deqp
{
namespace gls
{

namespace
{
const int VIEWPORT_WIDTH  = 128;
const int VIEWPORT_HEIGHT = 128;

const int TEXTURE2D_WIDTH  = 32;
const int TEXTURE2D_HEIGHT = 32;

const int TEXTURE3D_WIDTH  = 32;
const int TEXTURE3D_HEIGHT = 32;
const int TEXTURE3D_DEPTH  = 32;

const int CUBEMAP_SIZE = 32;

} // namespace

TextureSamplerTest::TextureSamplerTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const TestSpec &spec)
    : tcu::TestCase(testCtx, spec.name, spec.desc)
    , m_renderCtx(renderCtx)
    , m_program(NULL)
    , m_target(spec.target)
    , m_textureState(spec.textureState)
    , m_samplerState(spec.samplerState)
    , m_random(deStringHash(spec.name))
{
}

void TextureSamplerTest::setTextureState(const glw::Functions &gl, GLenum target, SamplingState state)
{
    gl.texParameteri(target, GL_TEXTURE_MIN_FILTER, state.minFilter);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri(target, GL_TEXTURE_MIN_FILTER, state.minFilter)");
    gl.texParameteri(target, GL_TEXTURE_MAG_FILTER, state.magFilter);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri(target, GL_TEXTURE_MAG_FILTER, state.magFilter)");
    gl.texParameteri(target, GL_TEXTURE_WRAP_S, state.wrapS);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri(target, GL_TEXTURE_WRAP_S, state.wrapS)");
    gl.texParameteri(target, GL_TEXTURE_WRAP_T, state.wrapT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri(target, GL_TEXTURE_WRAP_T, state.wrapT)");
    gl.texParameteri(target, GL_TEXTURE_WRAP_R, state.wrapR);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri(target, GL_TEXTURE_WRAP_R, state.wrapR)");
    gl.texParameterf(target, GL_TEXTURE_MAX_LOD, state.maxLod);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameterf(target, GL_TEXTURE_MAX_LOD, state.maxLod)");
    gl.texParameterf(target, GL_TEXTURE_MIN_LOD, state.minLod);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameterf(target, GL_TEXTURE_MIN_LOD, state.minLod)");
}

void TextureSamplerTest::setSamplerState(const glw::Functions &gl, SamplingState state, GLuint sampler)
{
    gl.samplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, state.minFilter);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, state.minFilter)");
    gl.samplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, state.magFilter);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, state.magFilter)");
    gl.samplerParameteri(sampler, GL_TEXTURE_WRAP_S, state.wrapS);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, state.wrapS)");
    gl.samplerParameteri(sampler, GL_TEXTURE_WRAP_T, state.wrapT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, state.wrapT)");
    gl.samplerParameteri(sampler, GL_TEXTURE_WRAP_R, state.wrapR);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, state.wrapR)");
    gl.samplerParameterf(sampler, GL_TEXTURE_MAX_LOD, state.maxLod);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glSamplerParameterf(sampler, GL_TEXTURE_MAX_LOD, state.maxLod)");
    gl.samplerParameterf(sampler, GL_TEXTURE_MIN_LOD, state.minLod);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glSamplerParameterf(sampler, GL_TEXTURE_MIN_LOD, state.minLod)");
}

const char *TextureSamplerTest::selectVertexShader(GLenum target)
{
    switch (target)
    {
    case GL_TEXTURE_2D:
        return "${VTX_HDR}"
               "${VTX_IN} ${HIGHP} vec2 a_position;\n"
               "uniform ${HIGHP} float u_posScale;\n"
               "${VTX_OUT} ${MEDIUMP} vec2 v_texCoord;\n"
               "void main (void)\n"
               "{\n"
               "\tv_texCoord = a_position;\n"
               "\tgl_Position = vec4(u_posScale * a_position, 0.0, 1.0);\n"
               "}";

    case GL_TEXTURE_3D:
        return "${VTX_HDR}"
               "${VTX_IN} ${HIGHP} vec3 a_position;\n"
               "uniform ${HIGHP} float u_posScale;\n"
               "${VTX_OUT} ${MEDIUMP} vec3 v_texCoord;\n"
               "void main (void)\n"
               "{\n"
               "\tv_texCoord = a_position;\n"
               "\tgl_Position = vec4(u_posScale * a_position.xy, 0.0, 1.0);\n"
               "}";

    case GL_TEXTURE_CUBE_MAP:
        return "${VTX_HDR}"
               "${VTX_IN} ${HIGHP} vec4 a_position;\n"
               "uniform ${HIGHP} float u_posScale;\n"
               "${VTX_OUT} ${MEDIUMP} vec2 v_texCoord;\n"
               "void main (void)\n"
               "{\n"
               "\tv_texCoord = a_position.zw;\n"
               "\tgl_Position = vec4(u_posScale * a_position.xy, 0.0, 1.0);\n"
               "}";

    default:
        DE_ASSERT(false);
        return NULL;
    }
}

const char *TextureSamplerTest::selectFragmentShader(GLenum target)
{
    switch (target)
    {
    case GL_TEXTURE_2D:
        return "${FRAG_HDR}"
               "uniform ${LOWP} sampler2D u_sampler;\n"
               "${FRAG_IN} ${MEDIUMP} vec2 v_texCoord;\n"
               "void main (void)\n"
               "{\n"
               "\t${FRAG_COLOR} = texture(u_sampler, v_texCoord);\n"
               "}";

    case GL_TEXTURE_3D:
        return "${FRAG_HDR}"
               "uniform ${LOWP} sampler3D u_sampler;\n"
               "${FRAG_IN} ${MEDIUMP} vec3 v_texCoord;\n"
               "void main (void)\n"
               "{\n"
               "\t${FRAG_COLOR} = texture(u_sampler, v_texCoord);\n"
               "}";

    case GL_TEXTURE_CUBE_MAP:
        return "${FRAG_HDR}"
               "uniform ${LOWP} samplerCube u_sampler;\n"
               "${FRAG_IN} ${MEDIUMP} vec2 v_texCoord;\n"
               "void main (void)\n"
               "{\n"
               "\t${FRAG_COLOR} = texture(u_sampler, vec3(cos(3.14 * v_texCoord.y) * sin(3.14 * v_texCoord.x), "
               "sin(3.14 * v_texCoord.y), cos(3.14 * v_texCoord.y) * cos(3.14 * v_texCoord.x)));\n"
               "}";

    default:
        DE_ASSERT(false);
        return NULL;
    }
}

void TextureSamplerTest::init(void)
{
    const char *vertexShaderTemplate   = selectVertexShader(m_target);
    const char *fragmentShaderTemplate = selectFragmentShader(m_target);

    std::map<std::string, std::string> params;

    if (glu::isGLSLVersionSupported(m_renderCtx.getType(), glu::GLSL_VERSION_300_ES))
    {
        params["VTX_HDR"]    = "#version 300 es\n";
        params["FRAG_HDR"]   = "#version 300 es\nlayout(location = 0) out mediump vec4 o_color;\n";
        params["VTX_IN"]     = "in";
        params["VTX_OUT"]    = "out";
        params["FRAG_IN"]    = "in";
        params["FRAG_COLOR"] = "o_color";
        params["HIGHP"]      = "highp";
        params["LOWP"]       = "lowp";
        params["MEDIUMP"]    = "mediump";
    }
    else if (glu::isGLSLVersionSupported(m_renderCtx.getType(), glu::GLSL_VERSION_330))
    {
        params["VTX_HDR"]    = "#version 330\n";
        params["FRAG_HDR"]   = "#version 330\nlayout(location = 0) out mediump vec4 o_color;\n";
        params["VTX_IN"]     = "in";
        params["VTX_OUT"]    = "out";
        params["FRAG_IN"]    = "in";
        params["FRAG_COLOR"] = "o_color";
        params["HIGHP"]      = "highp";
        params["LOWP"]       = "lowp";
        params["MEDIUMP"]    = "mediump";
    }
    else
        DE_ASSERT(false);

    DE_ASSERT(!m_program);
    m_program = new glu::ShaderProgram(
        m_renderCtx, glu::makeVtxFragSources(tcu::StringTemplate(vertexShaderTemplate).specialize(params),
                                             tcu::StringTemplate(fragmentShaderTemplate).specialize(params)));

    if (!m_program->isOk())
    {
        tcu::TestLog &log = m_testCtx.getLog();
        log << *m_program;
        TCU_FAIL("Failed to compile shaders");
    }
}

void TextureSamplerTest::deinit(void)
{
    delete m_program;
    m_program = NULL;
}

TextureSamplerTest::~TextureSamplerTest(void)
{
    deinit();
}

const float s_positions[] = {-1.0, -1.0, 1.0,  -1.0, 1.0,  1.0,

                             1.0,  1.0,  -1.0, 1.0,  -1.0, -1.0};

const float s_positions3D[] = {-1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f, 1.0f,  1.0f,  -1.0f,

                               1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  1.0f, -1.0f, -1.0f, -1.0f};

const float s_positionsCube[] = {-1.0f, -1.0f, -1.0f, -0.5f, 1.0f,  -1.0f, 1.0f,  -0.5f, 1.0f,  1.0f,  1.0f,  0.5f,

                                 1.0f,  1.0f,  1.0f,  0.5f,  -1.0f, 1.0f,  -1.0f, 0.5f,  -1.0f, -1.0f, -1.0f, -0.5f};

void TextureSamplerTest::render(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    GLuint samplerLoc = (GLuint)-1;
    GLuint scaleLoc   = (GLuint)-1;

    gl.useProgram(m_program->getProgram());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram(m_program->getProgram())");

    samplerLoc = gl.getUniformLocation(m_program->getProgram(), "u_sampler");
    TCU_CHECK(samplerLoc != (GLuint)-1);

    scaleLoc = gl.getUniformLocation(m_program->getProgram(), "u_posScale");
    TCU_CHECK(scaleLoc != (GLuint)-1);

    gl.clearColor(0.5f, 0.5f, 0.5f, 1.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glClearColor(0.5f, 0.5f, 0.5f, 1.0f)");

    gl.clear(GL_COLOR_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glClear(GL_COLOR_BUFFER_BIT)");

    gl.uniform1i(samplerLoc, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i(samplerLoc, 0)");

    gl.uniform1f(scaleLoc, 1.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1f(scaleLoc, 1.0f)");

    switch (m_target)
    {
    case GL_TEXTURE_2D:
    {
        glu::VertexArrayBinding vertexArrays[] = {glu::VertexArrayBinding(
            glu::BindingPoint("a_position"),
            glu::VertexArrayPointer(glu::VTX_COMP_FLOAT, glu::VTX_COMP_CONVERT_NONE, 2, 6, 0, s_positions))};

        glu::draw(m_renderCtx, m_program->getProgram(), DE_LENGTH_OF_ARRAY(vertexArrays), vertexArrays,
                  glu::PrimitiveList(glu::PRIMITIVETYPE_TRIANGLES, 6));

        gl.uniform1f(scaleLoc, 0.25f);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1f(scaleLoc, 0.25f)");

        glu::draw(m_renderCtx, m_program->getProgram(), DE_LENGTH_OF_ARRAY(vertexArrays), vertexArrays,
                  glu::PrimitiveList(glu::PRIMITIVETYPE_TRIANGLES, 6));

        break;
    }

    case GL_TEXTURE_3D:
    {
        glu::VertexArrayBinding vertexArrays[] = {glu::VertexArrayBinding(
            glu::BindingPoint("a_position"),
            glu::VertexArrayPointer(glu::VTX_COMP_FLOAT, glu::VTX_COMP_CONVERT_NONE, 3, 6, 0, s_positions3D))};

        glu::draw(m_renderCtx, m_program->getProgram(), DE_LENGTH_OF_ARRAY(vertexArrays), vertexArrays,
                  glu::PrimitiveList(glu::PRIMITIVETYPE_TRIANGLES, 6));

        gl.uniform1f(scaleLoc, 0.25f);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1f(scaleLoc, 0.25f)");

        glu::draw(m_renderCtx, m_program->getProgram(), DE_LENGTH_OF_ARRAY(vertexArrays), vertexArrays,
                  glu::PrimitiveList(glu::PRIMITIVETYPE_TRIANGLES, 6));

        break;
    }

    case GL_TEXTURE_CUBE_MAP:
    {
        glu::VertexArrayBinding vertexArrays[] = {glu::VertexArrayBinding(
            glu::BindingPoint("a_position"),
            glu::VertexArrayPointer(glu::VTX_COMP_FLOAT, glu::VTX_COMP_CONVERT_NONE, 4, 6, 0, s_positionsCube))};

        glu::draw(m_renderCtx, m_program->getProgram(), DE_LENGTH_OF_ARRAY(vertexArrays), vertexArrays,
                  glu::PrimitiveList(glu::PRIMITIVETYPE_TRIANGLES, 6));

        gl.uniform1f(scaleLoc, 0.25f);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1f(scaleLoc, 0.25f)");

        glu::draw(m_renderCtx, m_program->getProgram(), DE_LENGTH_OF_ARRAY(vertexArrays), vertexArrays,
                  glu::PrimitiveList(glu::PRIMITIVETYPE_TRIANGLES, 6));

        break;
    }

    default:
        DE_ASSERT(false);
    }
}

GLuint TextureSamplerTest::createTexture2D(const glw::Functions &gl)
{
    GLuint texture = (GLuint)-1;
    tcu::Texture2D refTexture(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8),
                              TEXTURE2D_WIDTH, TEXTURE2D_HEIGHT);

    refTexture.allocLevel(0);
    tcu::fillWithComponentGradients(refTexture.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
                                    tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

    gl.genTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures(1, &texture)");

    gl.bindTexture(GL_TEXTURE_2D, texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(GL_TEXTURE_2D, texture)");

    gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, refTexture.getWidth(), refTexture.getHeight(), 0, GL_RGBA,
                  GL_UNSIGNED_BYTE, refTexture.getLevel(0).getDataPtr());
    GLU_EXPECT_NO_ERROR(gl.getError(),
                        "glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, refTexture.getWidth(), refTexture.getHeight(), 0, "
                        "GL_RGBA, GL_UNSIGNED_BYTE, refTexture.getLevel(0).getDataPtr())");

    gl.generateMipmap(GL_TEXTURE_2D);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenerateMipmap(GL_TEXTURE_2D)");

    gl.bindTexture(GL_TEXTURE_2D, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(GL_TEXTURE_2D, texture)");

    return texture;
}

GLuint TextureSamplerTest::createTexture3D(const glw::Functions &gl)
{
    GLuint texture = (GLuint)-1;
    tcu::Texture3D refTexture(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8),
                              TEXTURE3D_WIDTH, TEXTURE3D_HEIGHT, TEXTURE3D_DEPTH);

    refTexture.allocLevel(0);
    tcu::fillWithComponentGradients(refTexture.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
                                    tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

    gl.genTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures(1, &texture)");

    gl.bindTexture(GL_TEXTURE_3D, texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(GL_TEXTURE_3D, texture)");

    gl.texImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, refTexture.getWidth(), refTexture.getHeight(), refTexture.getDepth(), 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, refTexture.getLevel(0).getDataPtr());
    GLU_EXPECT_NO_ERROR(gl.getError(),
                        "glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, refTexture.getWidth(), refTexture.getHeight(), "
                        "refTexture.getDepth(), 0, GL_RGBA, GL_UNSIGNED_BYTE, refTexture.getLevel(0).getDataPtr())");

    gl.generateMipmap(GL_TEXTURE_3D);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenerateMipmap(GL_TEXTURE_3D)");

    gl.bindTexture(GL_TEXTURE_3D, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(GL_TEXTURE_3D, 0)");

    return texture;
}

GLuint TextureSamplerTest::createTextureCube(const glw::Functions &gl)
{
    GLuint texture = (GLuint)-1;
    tcu::TextureCube refTexture(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8),
                                CUBEMAP_SIZE);

    refTexture.allocLevel(tcu::CUBEFACE_POSITIVE_X, 0);
    refTexture.allocLevel(tcu::CUBEFACE_POSITIVE_Y, 0);
    refTexture.allocLevel(tcu::CUBEFACE_POSITIVE_Z, 0);
    refTexture.allocLevel(tcu::CUBEFACE_NEGATIVE_X, 0);
    refTexture.allocLevel(tcu::CUBEFACE_NEGATIVE_Y, 0);
    refTexture.allocLevel(tcu::CUBEFACE_NEGATIVE_Z, 0);

    tcu::fillWithComponentGradients(refTexture.getLevelFace(0, tcu::CUBEFACE_POSITIVE_X),
                                    tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    tcu::fillWithComponentGradients(refTexture.getLevelFace(0, tcu::CUBEFACE_POSITIVE_Y),
                                    tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    tcu::fillWithComponentGradients(refTexture.getLevelFace(0, tcu::CUBEFACE_POSITIVE_Z),
                                    tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    tcu::fillWithComponentGradients(refTexture.getLevelFace(0, tcu::CUBEFACE_NEGATIVE_X),
                                    tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    tcu::fillWithComponentGradients(refTexture.getLevelFace(0, tcu::CUBEFACE_NEGATIVE_Y),
                                    tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    tcu::fillWithComponentGradients(refTexture.getLevelFace(0, tcu::CUBEFACE_NEGATIVE_Z),
                                    tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

    gl.bindTexture(GL_TEXTURE_CUBE_MAP, texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(GL_TEXTURE_CUBE_MAP, texture)");

    for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
    {
        const uint32_t target = glu::getGLCubeFace((tcu::CubeFace)face);
        gl.texImage2D(target, 0, GL_RGBA8, refTexture.getSize(), refTexture.getSize(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      refTexture.getLevelFace(0, (tcu::CubeFace)face).getDataPtr());
    }
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D(GL_TEXTURE_CUBE_MAP_...) failed");

    gl.generateMipmap(GL_TEXTURE_CUBE_MAP);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenerateMipmap(GL_TEXTURE_CUBE_MAP)");
    gl.bindTexture(GL_TEXTURE_CUBE_MAP, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(GL_TEXTURE_CUBE_MAP, texture)");

    return texture;
}

GLuint TextureSamplerTest::createTexture(const glw::Functions &gl, GLenum target)
{
    switch (target)
    {
    case GL_TEXTURE_2D:
        return createTexture2D(gl);

    case GL_TEXTURE_3D:
        return createTexture3D(gl);

    case GL_TEXTURE_CUBE_MAP:
        return createTextureCube(gl);

    default:
        DE_ASSERT(false);
        return (GLuint)-1;
    }
}

void TextureSamplerTest::renderReferences(tcu::Surface &textureRef, tcu::Surface &samplerRef, int x, int y)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    GLuint texture           = createTexture(gl, m_target);

    gl.viewport(x, y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport(x, y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT)");

    gl.bindTexture(m_target, texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(m_target, texture)");

    setTextureState(gl, m_target, m_textureState);
    render();
    glu::readPixels(m_renderCtx, x, y, textureRef.getAccess());

    setTextureState(gl, m_target, m_samplerState);
    render();
    glu::readPixels(m_renderCtx, x, y, samplerRef.getAccess());

    gl.deleteTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures(1, &texture)");
}

void TextureSamplerTest::renderResults(tcu::Surface &textureResult, tcu::Surface &samplerResult, int x, int y)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    GLuint texture           = createTexture(gl, m_target);
    GLuint sampler           = -1;

    gl.viewport(x, y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport(x, y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT)");

    gl.genSamplers(1, &sampler);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenSamplers(1, &sampler)");
    TCU_CHECK(sampler != (GLuint)-1);

    gl.bindSampler(0, sampler);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindSampler(0, sampler)");

    // First set sampler state
    setSamplerState(gl, m_samplerState, sampler);

    // Set texture state
    gl.bindTexture(m_target, texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(m_target, texture)");

    setTextureState(gl, m_target, m_textureState);

    // Render using sampler
    render();
    glu::readPixels(m_renderCtx, x, y, samplerResult.getAccess());

    // Render without sampler
    gl.bindSampler(0, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindSampler(0, 0)");

    render();
    glu::readPixels(m_renderCtx, x, y, textureResult.getAccess());

    gl.deleteSamplers(1, &sampler);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteSamplers(1, &sampler)");
    gl.deleteTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures(1, &texture)");
}

tcu::TestCase::IterateResult TextureSamplerTest::iterate(void)
{
    tcu::TestLog &log = m_testCtx.getLog();

    tcu::Surface textureRef(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
    tcu::Surface samplerRef(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    tcu::Surface textureResult(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
    tcu::Surface samplerResult(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    int x = m_random.getInt(0, m_renderCtx.getRenderTarget().getWidth() - VIEWPORT_WIDTH);
    int y = m_random.getInt(0, m_renderCtx.getRenderTarget().getHeight() - VIEWPORT_HEIGHT);

    renderReferences(textureRef, samplerRef, x, y);
    renderResults(textureResult, samplerResult, x, y);

    bool isOk = pixelThresholdCompare(log, "Sampler render result", "Result from rendering with sampler", samplerRef,
                                      samplerResult, tcu::RGBA(0, 0, 0, 0), tcu::COMPARE_LOG_RESULT);

    if (!pixelThresholdCompare(log, "Texture render result", "Result from rendering with texture state", textureRef,
                               textureResult, tcu::RGBA(0, 0, 0, 0), tcu::COMPARE_LOG_RESULT))
        isOk = false;

    if (!isOk)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        return STOP;
    }

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    return STOP;
}

MultiTextureSamplerTest::MultiTextureSamplerTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                 const TestSpec &spec)
    : TestCase(testCtx, spec.name, spec.desc)
    , m_renderCtx(renderCtx)
    , m_program(NULL)
    , m_target(spec.target)
    , m_textureState1(spec.textureState1)
    , m_textureState2(spec.textureState2)
    , m_samplerState(spec.samplerState)
    , m_random(deStringHash(spec.name))
{
}

void MultiTextureSamplerTest::setTextureState(const glw::Functions &gl, GLenum target, SamplingState state)
{
    gl.texParameteri(target, GL_TEXTURE_MIN_FILTER, state.minFilter);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri(target, GL_TEXTURE_MIN_FILTER, state.minFilter)");
    gl.texParameteri(target, GL_TEXTURE_MAG_FILTER, state.magFilter);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri(target, GL_TEXTURE_MAG_FILTER, state.magFilter)");
    gl.texParameteri(target, GL_TEXTURE_WRAP_S, state.wrapS);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri(target, GL_TEXTURE_WRAP_S, state.wrapS)");
    gl.texParameteri(target, GL_TEXTURE_WRAP_T, state.wrapT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri(target, GL_TEXTURE_WRAP_T, state.wrapT)");
    gl.texParameteri(target, GL_TEXTURE_WRAP_R, state.wrapR);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri(target, GL_TEXTURE_WRAP_R, state.wrapR)");
    gl.texParameterf(target, GL_TEXTURE_MAX_LOD, state.maxLod);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameterf(target, GL_TEXTURE_MAX_LOD, state.maxLod)");
    gl.texParameterf(target, GL_TEXTURE_MIN_LOD, state.minLod);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameterf(target, GL_TEXTURE_MIN_LOD, state.minLod)");
}

void MultiTextureSamplerTest::setSamplerState(const glw::Functions &gl, SamplingState state, GLuint sampler)
{
    gl.samplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, state.minFilter);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, state.minFilter)");
    gl.samplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, state.magFilter);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, state.magFilter)");
    gl.samplerParameteri(sampler, GL_TEXTURE_WRAP_S, state.wrapS);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, state.wrapS)");
    gl.samplerParameteri(sampler, GL_TEXTURE_WRAP_T, state.wrapT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, state.wrapT)");
    gl.samplerParameteri(sampler, GL_TEXTURE_WRAP_R, state.wrapR);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, state.wrapR)");
    gl.samplerParameterf(sampler, GL_TEXTURE_MAX_LOD, state.maxLod);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glSamplerParameterf(sampler, GL_TEXTURE_MAX_LOD, state.maxLod)");
    gl.samplerParameterf(sampler, GL_TEXTURE_MIN_LOD, state.minLod);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glSamplerParameterf(sampler, GL_TEXTURE_MIN_LOD, state.minLod)");
}

const char *MultiTextureSamplerTest::selectVertexShader(GLenum target)
{
    switch (target)
    {
    case GL_TEXTURE_2D:
        return "${VTX_HDR}"
               "${VTX_IN} ${HIGHP} vec2 a_position;\n"
               "uniform ${HIGHP} float u_posScale;\n"
               "${VTX_OUT} ${MEDIUMP} vec2 v_texCoord;\n"
               "void main (void)\n"
               "{\n"
               "\tv_texCoord = a_position;\n"
               "\tgl_Position = vec4(u_posScale * a_position, 0.0, 1.0);\n"
               "}";

    case GL_TEXTURE_3D:
        return "${VTX_HDR}"
               "${VTX_IN} ${HIGHP} vec3 a_position;\n"
               "uniform ${HIGHP} float u_posScale;\n"
               "${VTX_OUT} ${MEDIUMP} vec3 v_texCoord;\n"
               "void main (void)\n"
               "{\n"
               "\tv_texCoord = a_position;\n"
               "\tgl_Position = vec4(u_posScale * a_position.xy, 0.0, 1.0);\n"
               "}";

    case GL_TEXTURE_CUBE_MAP:
        return "${VTX_HDR}"
               "${VTX_IN} ${HIGHP} vec4 a_position;\n"
               "uniform ${HIGHP} float u_posScale;\n"
               "${VTX_OUT} ${MEDIUMP} vec2 v_texCoord;\n"
               "void main (void)\n"
               "{\n"
               "\tv_texCoord = a_position.zw;\n"
               "\tgl_Position = vec4(u_posScale * a_position.xy, 0.0, 1.0);\n"
               "}";

    default:
        DE_ASSERT(false);
        return NULL;
    }
}

const char *MultiTextureSamplerTest::selectFragmentShader(GLenum target)
{
    switch (target)
    {
    case GL_TEXTURE_2D:
        return "${FRAG_HDR}"
               "uniform ${LOWP} sampler2D u_sampler1;\n"
               "uniform ${LOWP} sampler2D u_sampler2;\n"
               "${FRAG_IN} ${MEDIUMP} vec2 v_texCoord;\n"
               "void main (void)\n"
               "{\n"
               "\t${FRAG_COLOR} = vec4(0.75, 0.75, 0.75, 1.0) * (texture(u_sampler1, v_texCoord) + texture(u_sampler2, "
               "v_texCoord));\n"
               "}";

    case GL_TEXTURE_3D:
        return "${FRAG_HDR}"
               "uniform ${LOWP} sampler3D u_sampler1;\n"
               "uniform ${LOWP} sampler3D u_sampler2;\n"
               "${FRAG_IN} ${MEDIUMP} vec3 v_texCoord;\n"
               "void main (void)\n"
               "{\n"
               "\t${FRAG_COLOR} = vec4(0.75, 0.75, 0.75, 1.0) * (texture(u_sampler1, v_texCoord) + texture(u_sampler2, "
               "v_texCoord));\n"
               "}";

    case GL_TEXTURE_CUBE_MAP:
        return "${FRAG_HDR}"
               "uniform ${LOWP} samplerCube u_sampler1;\n"
               "uniform ${LOWP} samplerCube u_sampler2;\n"
               "${FRAG_IN} ${MEDIUMP} vec2 v_texCoord;\n"
               "void main (void)\n"
               "{\n"
               "\t${FRAG_COLOR} = vec4(0.5, 0.5, 0.5, 1.0) * (texture(u_sampler1, vec3(cos(3.14 * v_texCoord.y) * "
               "sin(3.14 * v_texCoord.x), sin(3.14 * v_texCoord.y), cos(3.14 * v_texCoord.y) * cos(3.14 * "
               "v_texCoord.x)))"
               "+ texture(u_sampler2, vec3(cos(3.14 * v_texCoord.y) * sin(3.14 * v_texCoord.x), sin(3.14 * "
               "v_texCoord.y), cos(3.14 * v_texCoord.y) * cos(3.14 * v_texCoord.x))));\n"
               "}";

    default:
        DE_ASSERT(false);
        return NULL;
    }
}

void MultiTextureSamplerTest::init(void)
{
    const char *vertexShaderTemplate   = selectVertexShader(m_target);
    const char *fragmentShaderTemplate = selectFragmentShader(m_target);

    std::map<std::string, std::string> params;

    if (glu::isGLSLVersionSupported(m_renderCtx.getType(), glu::GLSL_VERSION_300_ES))
    {
        params["VTX_HDR"]    = "#version 300 es\n";
        params["FRAG_HDR"]   = "#version 300 es\nlayout(location = 0) out mediump vec4 o_color;\n";
        params["VTX_IN"]     = "in";
        params["VTX_OUT"]    = "out";
        params["FRAG_IN"]    = "in";
        params["FRAG_COLOR"] = "o_color";
        params["HIGHP"]      = "highp";
        params["LOWP"]       = "lowp";
        params["MEDIUMP"]    = "mediump";
    }
    else if (glu::isGLSLVersionSupported(m_renderCtx.getType(), glu::GLSL_VERSION_330))
    {
        params["VTX_HDR"]    = "#version 330\n";
        params["FRAG_HDR"]   = "#version 330\nlayout(location = 0) out mediump vec4 o_color;\n";
        params["VTX_IN"]     = "in";
        params["VTX_OUT"]    = "out";
        params["FRAG_IN"]    = "in";
        params["FRAG_COLOR"] = "o_color";
        params["HIGHP"]      = "highp";
        params["LOWP"]       = "lowp";
        params["MEDIUMP"]    = "mediump";
    }
    else
        DE_ASSERT(false);

    DE_ASSERT(!m_program);
    m_program = new glu::ShaderProgram(
        m_renderCtx, glu::makeVtxFragSources(tcu::StringTemplate(vertexShaderTemplate).specialize(params),
                                             tcu::StringTemplate(fragmentShaderTemplate).specialize(params)));
    if (!m_program->isOk())
    {
        tcu::TestLog &log = m_testCtx.getLog();

        log << *m_program;
        TCU_FAIL("Failed to compile shaders");
    }
}

void MultiTextureSamplerTest::deinit(void)
{
    delete m_program;
    m_program = NULL;
}

MultiTextureSamplerTest::~MultiTextureSamplerTest(void)
{
    deinit();
}

void MultiTextureSamplerTest::render(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    GLuint samplerLoc1 = (GLuint)-1;
    GLuint samplerLoc2 = (GLuint)-1;
    GLuint scaleLoc    = (GLuint)-1;

    gl.useProgram(m_program->getProgram());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram(m_program->getProgram())");

    samplerLoc1 = glGetUniformLocation(m_program->getProgram(), "u_sampler1");
    TCU_CHECK(samplerLoc1 != (GLuint)-1);

    samplerLoc2 = glGetUniformLocation(m_program->getProgram(), "u_sampler2");
    TCU_CHECK(samplerLoc2 != (GLuint)-1);

    scaleLoc = glGetUniformLocation(m_program->getProgram(), "u_posScale");
    TCU_CHECK(scaleLoc != (GLuint)-1);

    gl.clearColor(0.5f, 0.5f, 0.5f, 1.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glClearColor(0.5f, 0.5f, 0.5f, 1.0f)");

    gl.clear(GL_COLOR_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glClear(GL_COLOR_BUFFER_BIT)");

    gl.uniform1i(samplerLoc1, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i(samplerLoc1, 0)");

    gl.uniform1i(samplerLoc2, 1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i(samplerLoc2, 1)");

    gl.uniform1f(scaleLoc, 1.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1f(scaleLoc, 1.0f)");

    switch (m_target)
    {
    case GL_TEXTURE_2D:
    {
        glu::VertexArrayBinding vertexArrays[] = {glu::VertexArrayBinding(
            glu::BindingPoint("a_position"),
            glu::VertexArrayPointer(glu::VTX_COMP_FLOAT, glu::VTX_COMP_CONVERT_NONE, 2, 6, 0, s_positions))};

        glu::draw(m_renderCtx, m_program->getProgram(), DE_LENGTH_OF_ARRAY(vertexArrays), vertexArrays,
                  glu::PrimitiveList(glu::PRIMITIVETYPE_TRIANGLES, 6));

        gl.uniform1f(scaleLoc, 0.25f);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1f(scaleLoc, 0.25f)");

        glu::draw(m_renderCtx, m_program->getProgram(), DE_LENGTH_OF_ARRAY(vertexArrays), vertexArrays,
                  glu::PrimitiveList(glu::PRIMITIVETYPE_TRIANGLES, 6));

        break;
    }

    case GL_TEXTURE_3D:
    {
        glu::VertexArrayBinding vertexArrays[] = {glu::VertexArrayBinding(
            glu::BindingPoint("a_position"),
            glu::VertexArrayPointer(glu::VTX_COMP_FLOAT, glu::VTX_COMP_CONVERT_NONE, 3, 6, 0, s_positions3D))};

        glu::draw(m_renderCtx, m_program->getProgram(), DE_LENGTH_OF_ARRAY(vertexArrays), vertexArrays,
                  glu::PrimitiveList(glu::PRIMITIVETYPE_TRIANGLES, 6));

        gl.uniform1f(scaleLoc, 0.25f);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1f(scaleLoc, 0.25f)");

        glu::draw(m_renderCtx, m_program->getProgram(), DE_LENGTH_OF_ARRAY(vertexArrays), vertexArrays,
                  glu::PrimitiveList(glu::PRIMITIVETYPE_TRIANGLES, 6));

        break;
    }

    case GL_TEXTURE_CUBE_MAP:
    {
        glu::VertexArrayBinding vertexArrays[] = {glu::VertexArrayBinding(
            glu::BindingPoint("a_position"),
            glu::VertexArrayPointer(glu::VTX_COMP_FLOAT, glu::VTX_COMP_CONVERT_NONE, 4, 6, 0, s_positionsCube))};

        glu::draw(m_renderCtx, m_program->getProgram(), DE_LENGTH_OF_ARRAY(vertexArrays), vertexArrays,
                  glu::PrimitiveList(glu::PRIMITIVETYPE_TRIANGLES, 6));

        gl.uniform1f(scaleLoc, 0.25f);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1f(scaleLoc, 0.25f)");

        glu::draw(m_renderCtx, m_program->getProgram(), DE_LENGTH_OF_ARRAY(vertexArrays), vertexArrays,
                  glu::PrimitiveList(glu::PRIMITIVETYPE_TRIANGLES, 6));

        break;
    }

    default:
        DE_ASSERT(false);
    }
}

GLuint MultiTextureSamplerTest::createTexture2D(const glw::Functions &gl, int id)
{
    GLuint texture = (GLuint)-1;
    tcu::Texture2D refTexture(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8),
                              TEXTURE2D_WIDTH, TEXTURE2D_HEIGHT);

    refTexture.allocLevel(0);

    gl.genTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures(1, &texture)");

    switch (id)
    {
    case 0:
        tcu::fillWithComponentGradients(refTexture.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
                                        tcu::Vec4(1.0f, 1.0f, 0.5f, 0.5f));
        break;

    case 1:
        tcu::fillWithComponentGradients(refTexture.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
                                        tcu::Vec4(0.5f, 0.5f, 1.0f, 1.0f));
        break;

    default:
        DE_ASSERT(false);
    }

    gl.bindTexture(GL_TEXTURE_2D, texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(GL_TEXTURE_2D, texture)");

    gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, refTexture.getWidth(), refTexture.getHeight(), 0, GL_RGBA,
                  GL_UNSIGNED_BYTE, refTexture.getLevel(0).getDataPtr());
    GLU_EXPECT_NO_ERROR(gl.getError(),
                        "glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, refTexture.getWidth(), refTexture.getHeight(), 0, "
                        "GL_RGBA, GL_UNSIGNED_BYTE, refTexture.getLevel(0).getDataPtr())");

    gl.generateMipmap(GL_TEXTURE_2D);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenerateMipmap(GL_TEXTURE_2D)");

    gl.bindTexture(GL_TEXTURE_2D, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(GL_TEXTURE_2D, 0)");

    return texture;
}

GLuint MultiTextureSamplerTest::createTexture3D(const glw::Functions &gl, int id)
{
    GLuint texture = (GLuint)-1;
    tcu::Texture3D refTexture(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8),
                              TEXTURE3D_WIDTH, TEXTURE3D_HEIGHT, TEXTURE3D_DEPTH);

    refTexture.allocLevel(0);

    gl.genTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures(1, &texture)");

    switch (id)
    {
    case 0:
        tcu::fillWithComponentGradients(refTexture.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
                                        tcu::Vec4(1.0f, 1.0f, 0.5f, 0.5f));
        break;

    case 1:
        tcu::fillWithComponentGradients(refTexture.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
                                        tcu::Vec4(0.5f, 0.5f, 1.0f, 1.0f));
        break;

    default:
        DE_ASSERT(false);
    }

    gl.bindTexture(GL_TEXTURE_3D, texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(GL_TEXTURE_3D, texture)");

    gl.texImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, refTexture.getWidth(), refTexture.getHeight(), refTexture.getDepth(), 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, refTexture.getLevel(0).getDataPtr());
    GLU_EXPECT_NO_ERROR(gl.getError(),
                        "glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, refTexture.getWidth(), refTexture.getHeight(), "
                        "refTexture.getDepth(), 0, GL_RGBA, GL_UNSIGNED_BYTE, refTexture.getLevel(0).getDataPtr())");

    gl.generateMipmap(GL_TEXTURE_3D);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenerateMipmap(GL_TEXTURE_3D)");

    gl.bindTexture(GL_TEXTURE_3D, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(GL_TEXTURE_3D, 0)");

    return texture;
}

GLuint MultiTextureSamplerTest::createTextureCube(const glw::Functions &gl, int id)
{
    GLuint texture = (GLuint)-1;
    tcu::TextureCube refTexture(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8),
                                CUBEMAP_SIZE);

    gl.genTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures(1, &texture)");

    refTexture.allocLevel(tcu::CUBEFACE_POSITIVE_X, 0);
    refTexture.allocLevel(tcu::CUBEFACE_POSITIVE_Y, 0);
    refTexture.allocLevel(tcu::CUBEFACE_POSITIVE_Z, 0);
    refTexture.allocLevel(tcu::CUBEFACE_NEGATIVE_X, 0);
    refTexture.allocLevel(tcu::CUBEFACE_NEGATIVE_Y, 0);
    refTexture.allocLevel(tcu::CUBEFACE_NEGATIVE_Z, 0);

    switch (id)
    {
    case 0:
        tcu::fillWithComponentGradients(refTexture.getLevelFace(0, tcu::CUBEFACE_POSITIVE_X),
                                        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.5f, 0.5f));
        tcu::fillWithComponentGradients(refTexture.getLevelFace(0, tcu::CUBEFACE_POSITIVE_Y),
                                        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.5f, 0.5f));
        tcu::fillWithComponentGradients(refTexture.getLevelFace(0, tcu::CUBEFACE_POSITIVE_Z),
                                        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.5f, 0.5f));
        tcu::fillWithComponentGradients(refTexture.getLevelFace(0, tcu::CUBEFACE_NEGATIVE_X),
                                        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.5f, 0.5f));
        tcu::fillWithComponentGradients(refTexture.getLevelFace(0, tcu::CUBEFACE_NEGATIVE_Y),
                                        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.5f, 0.5f));
        tcu::fillWithComponentGradients(refTexture.getLevelFace(0, tcu::CUBEFACE_NEGATIVE_Z),
                                        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.5f, 0.5f));
        break;

    case 1:
        tcu::fillWithComponentGradients(refTexture.getLevelFace(0, tcu::CUBEFACE_POSITIVE_X),
                                        tcu::Vec4(0.5f, 0.5f, 0.5f, 0.5f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
        tcu::fillWithComponentGradients(refTexture.getLevelFace(0, tcu::CUBEFACE_POSITIVE_Y),
                                        tcu::Vec4(0.5f, 0.5f, 0.5f, 0.5f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
        tcu::fillWithComponentGradients(refTexture.getLevelFace(0, tcu::CUBEFACE_POSITIVE_Z),
                                        tcu::Vec4(0.5f, 0.5f, 0.5f, 0.5f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
        tcu::fillWithComponentGradients(refTexture.getLevelFace(0, tcu::CUBEFACE_NEGATIVE_X),
                                        tcu::Vec4(0.5f, 0.5f, 0.5f, 0.5f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
        tcu::fillWithComponentGradients(refTexture.getLevelFace(0, tcu::CUBEFACE_NEGATIVE_Y),
                                        tcu::Vec4(0.5f, 0.5f, 0.5f, 0.5f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
        tcu::fillWithComponentGradients(refTexture.getLevelFace(0, tcu::CUBEFACE_NEGATIVE_Z),
                                        tcu::Vec4(0.5f, 0.5f, 0.5f, 0.5f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
        break;

    default:
        DE_ASSERT(false);
    }

    gl.bindTexture(GL_TEXTURE_CUBE_MAP, texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(GL_TEXTURE_CUBE_MAP, texture)");

    for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
    {
        const uint32_t target = glu::getGLCubeFace((tcu::CubeFace)face);
        gl.texImage2D(target, 0, GL_RGBA8, refTexture.getSize(), refTexture.getSize(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      refTexture.getLevelFace(0, (tcu::CubeFace)face).getDataPtr());
    }
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D(GL_TEXTURE_CUBE_MAP_...) failed");

    gl.generateMipmap(GL_TEXTURE_CUBE_MAP);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenerateMipmap(GL_TEXTURE_CUBE_MAP)");
    gl.bindTexture(GL_TEXTURE_CUBE_MAP, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(GL_TEXTURE_CUBE_MAP, 0)");

    return texture;
}

GLuint MultiTextureSamplerTest::createTexture(const glw::Functions &gl, GLenum target, int id)
{
    switch (target)
    {
    case GL_TEXTURE_2D:
        return createTexture2D(gl, id);

    case GL_TEXTURE_3D:
        return createTexture3D(gl, id);

    case GL_TEXTURE_CUBE_MAP:
        return createTextureCube(gl, id);

    default:
        DE_ASSERT(false);
        return (GLuint)-1;
    }
}

void MultiTextureSamplerTest::renderReferences(tcu::Surface &textureRef, tcu::Surface &samplerRef, int x, int y)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    GLuint texture1          = createTexture(gl, m_target, 0);
    GLuint texture2          = createTexture(gl, m_target, 1);

    gl.viewport(x, y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport(x, y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT)");

    // Generate texture rendering reference
    gl.activeTexture(GL_TEXTURE0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glActiveTexture(GL_TEXTURE0)");
    gl.bindTexture(m_target, texture1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(m_target, texture1)");
    setTextureState(gl, m_target, m_textureState1);

    gl.activeTexture(GL_TEXTURE1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glActiveTexture(GL_TEXTURE1)");
    gl.bindTexture(m_target, texture2);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(m_target, texture2)");
    setTextureState(gl, m_target, m_textureState2);

    render();
    glu::readPixels(m_renderCtx, x, y, textureRef.getAccess());

    // Generate sampler rendering reference
    gl.activeTexture(GL_TEXTURE0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glActiveTexture(GL_TEXTURE0)");
    gl.bindTexture(m_target, texture1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(m_target, texture1)");
    setTextureState(gl, m_target, m_samplerState);

    gl.activeTexture(GL_TEXTURE1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glActiveTexture(GL_TEXTURE1)");
    gl.bindTexture(m_target, texture2);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(m_target, texture2)");
    setTextureState(gl, m_target, m_samplerState);

    render();
    glu::readPixels(m_renderCtx, x, y, samplerRef.getAccess());
}

void MultiTextureSamplerTest::renderResults(tcu::Surface &textureResult, tcu::Surface &samplerResult, int x, int y)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    GLuint texture1          = createTexture(gl, m_target, 0);
    GLuint texture2          = createTexture(gl, m_target, 1);
    GLuint sampler           = -1;

    gl.viewport(x, y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport(x, y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT)");

    gl.genSamplers(1, &sampler);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenSamplers(1, &sampler)");
    TCU_CHECK(sampler != (GLuint)-1);

    gl.bindSampler(0, sampler);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindSampler(0, sampler)");
    gl.bindSampler(1, sampler);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindSampler(1, sampler)");

    // First set sampler state
    setSamplerState(gl, m_samplerState, sampler);

    // Set texture state
    gl.bindTexture(m_target, texture1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(m_target, texture1)");
    setTextureState(gl, m_target, m_textureState1);

    gl.bindTexture(m_target, texture2);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(m_target, texture2)");
    setTextureState(gl, m_target, m_textureState2);

    gl.activeTexture(GL_TEXTURE0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glActiveTexture(GL_TEXTURE0)");
    gl.bindTexture(m_target, texture1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(m_target, texture1)");

    gl.activeTexture(GL_TEXTURE1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glActiveTexture(GL_TEXTURE1)");
    gl.bindTexture(m_target, texture2);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(m_target, texture2)");

    // Render using sampler
    render();
    glu::readPixels(m_renderCtx, x, y, samplerResult.getAccess());

    gl.bindSampler(0, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindSampler(0, 0)");
    gl.bindSampler(1, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindSampler(1, 0)");

    render();
    glu::readPixels(m_renderCtx, x, y, textureResult.getAccess());

    gl.activeTexture(GL_TEXTURE0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glActiveTexture(GL_TEXTURE0)");
    gl.bindTexture(m_target, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(m_target, 0)");

    gl.activeTexture(GL_TEXTURE1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glActiveTexture(GL_TEXTURE1)");
    gl.bindTexture(m_target, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(m_target, 0)");

    gl.deleteSamplers(1, &sampler);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteSamplers(1, &sampler)");
    gl.deleteTextures(1, &texture1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures(1, &texture1)");
    gl.deleteTextures(1, &texture2);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures(1, &texture2)");
}

tcu::TestCase::IterateResult MultiTextureSamplerTest::iterate(void)
{
    tcu::TestLog &log = m_testCtx.getLog();

    tcu::Surface textureRef(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
    tcu::Surface samplerRef(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    tcu::Surface textureResult(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
    tcu::Surface samplerResult(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    int x = m_random.getInt(0, m_renderCtx.getRenderTarget().getWidth() - VIEWPORT_WIDTH);
    int y = m_random.getInt(0, m_renderCtx.getRenderTarget().getHeight() - VIEWPORT_HEIGHT);

    renderReferences(textureRef, samplerRef, x, y);
    renderResults(textureResult, samplerResult, x, y);

    bool isOk = pixelThresholdCompare(log, "Sampler render result", "Result from rendering with sampler", samplerRef,
                                      samplerResult, tcu::RGBA(0, 0, 0, 0), tcu::COMPARE_LOG_RESULT);

    if (!pixelThresholdCompare(log, "Texture render result", "Result from rendering with texture state", textureRef,
                               textureResult, tcu::RGBA(0, 0, 0, 0), tcu::COMPARE_LOG_RESULT))
        isOk = false;

    if (!isOk)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        return STOP;
    }

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    return STOP;
}

} // namespace gls
} // namespace deqp
