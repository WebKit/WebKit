/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
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
 * \brief Common object lifetime tests.
 *//*--------------------------------------------------------------------*/

#include "glsLifetimeTests.hpp"

#include "deString.h"
#include "deRandom.hpp"
#include "deSTLUtil.hpp"
#include "deStringUtil.hpp"
#include "tcuRGBA.hpp"
#include "tcuImageCompare.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"
#include "gluDrawUtil.hpp"
#include "gluObjectWrapper.hpp"
#include "gluPixelTransfer.hpp"
#include "gluShaderProgram.hpp"
#include "gluDefs.hpp"
#include "gluTextureUtil.hpp"
#include "gluStrUtil.hpp"
#include "glwFunctions.hpp"

#include <vector>
#include <map>
#include <algorithm>
#include <sstream>

namespace deqp
{
namespace gls
{
namespace LifetimeTests
{
namespace details
{

using de::Random;
using std::map;
using std::ostringstream;
using std::string;
using tcu::RenderTarget;
using tcu::RGBA;
using tcu::StringTemplate;
using tcu::TestCase;
typedef TestCase::IterateResult IterateResult;
using glu::Framebuffer;
using glu::Program;
using glu::Shader;
using glu::SHADERTYPE_FRAGMENT;
using glu::SHADERTYPE_VERTEX;
using tcu::ScopedLogSection;
using tcu::TestLog;
using namespace glw;

enum
{
    VIEWPORT_SIZE    = 128,
    FRAMEBUFFER_SIZE = 128
};

GLint getInteger(ContextWrapper &gl, GLenum queryParam)
{
    GLint ret = 0;
    GLU_CHECK_CALL_ERROR(gl.glGetIntegerv(queryParam, &ret), gl.glGetError());
    gl.log() << TestLog::Message << "// Single integer output: " << ret << TestLog::EndMessage;
    return ret;
}

#define GLSL100_SRC(BODY) ("#version 100\n" #BODY "\n")

static const char *const s_vertexShaderSrc =
    GLSL100_SRC(attribute vec2 pos; void main() { gl_Position = vec4(pos.xy, 0.0, 1.0); });

static const char *const s_fragmentShaderSrc = GLSL100_SRC(void main() { gl_FragColor = vec4(1.0); });

class CheckedShader : public Shader
{
public:
    CheckedShader(const RenderContext &renderCtx, glu::ShaderType type, const string &src) : Shader(renderCtx, type)
    {
        const char *const srcStr = src.c_str();
        setSources(1, &srcStr, nullptr);
        compile();
        TCU_CHECK(getCompileStatus());
    }
};

class CheckedProgram : public Program
{
public:
    CheckedProgram(const RenderContext &renderCtx, GLuint vtxShader, GLuint fragShader) : Program(renderCtx)
    {
        attachShader(vtxShader);
        attachShader(fragShader);
        link();
        TCU_CHECK(getLinkStatus());
    }
};

ContextWrapper::ContextWrapper(const Context &ctx) : CallLogWrapper(ctx.gl(), ctx.log()), m_ctx(ctx)
{
    enableLogging(true);
}

void SimpleBinder::bind(GLuint name)
{
    (this->*m_bindFunc)(m_bindTarget, name);
}

GLuint SimpleBinder::getBinding(void)
{
    return getInteger(*this, m_bindingParam);
}

GLuint SimpleType::gen(void)
{
    GLuint ret;
    (this->*m_genFunc)(1, &ret);
    return ret;
}

class VertexArrayBinder : public SimpleBinder
{
public:
    VertexArrayBinder(Context &ctx) : SimpleBinder(ctx, 0, GL_NONE, GL_VERTEX_ARRAY_BINDING, true)
    {
    }
    void bind(GLuint name)
    {
        glBindVertexArray(name);
    }
};

class QueryBinder : public Binder
{
public:
    QueryBinder(Context &ctx) : Binder(ctx)
    {
    }
    void bind(GLuint name)
    {
        if (name != 0)
            glBeginQuery(GL_ANY_SAMPLES_PASSED, name);
        else
            glEndQuery(GL_ANY_SAMPLES_PASSED);
    }
    GLuint getBinding(void)
    {
        return 0;
    }
};

bool ProgramType::isDeleteFlagged(GLuint name)
{
    GLint deleteFlagged = 0;
    glGetProgramiv(name, GL_DELETE_STATUS, &deleteFlagged);
    return deleteFlagged != 0;
}

bool ShaderType::isDeleteFlagged(GLuint name)
{
    GLint deleteFlagged = 0;
    glGetShaderiv(name, GL_DELETE_STATUS, &deleteFlagged);
    return deleteFlagged != 0;
}

void setupFbo(const Context &ctx, GLuint seed, GLuint fbo)
{
    const Functions &gl = ctx.getRenderContext().getFunctions();

    GLU_CHECK_CALL_ERROR(gl.bindFramebuffer(GL_FRAMEBUFFER, fbo), gl.getError());

    if (seed == 0)
    {
        gl.clearColor(0.0, 0.0, 0.0, 1.0);
        GLU_CHECK_CALL_ERROR(gl.clear(GL_COLOR_BUFFER_BIT), gl.getError());
    }
    else
    {
        Random rnd(seed);
        const GLsizei width  = rnd.getInt(0, FRAMEBUFFER_SIZE);
        const GLsizei height = rnd.getInt(0, FRAMEBUFFER_SIZE);
        const GLint x        = rnd.getInt(0, FRAMEBUFFER_SIZE - width);
        const GLint y        = rnd.getInt(0, FRAMEBUFFER_SIZE - height);
        const GLfloat r1     = rnd.getFloat();
        const GLfloat g1     = rnd.getFloat();
        const GLfloat b1     = rnd.getFloat();
        const GLfloat a1     = rnd.getFloat();
        const GLfloat r2     = rnd.getFloat();
        const GLfloat g2     = rnd.getFloat();
        const GLfloat b2     = rnd.getFloat();
        const GLfloat a2     = rnd.getFloat();

        GLU_CHECK_CALL_ERROR(gl.clearColor(r1, g1, b1, a1), gl.getError());
        GLU_CHECK_CALL_ERROR(gl.clear(GL_COLOR_BUFFER_BIT), gl.getError());
        gl.scissor(x, y, width, height);
        gl.enable(GL_SCISSOR_TEST);
        gl.clearColor(r2, g2, b2, a2);
        gl.clear(GL_COLOR_BUFFER_BIT);
        gl.disable(GL_SCISSOR_TEST);
    }

    gl.bindFramebuffer(GL_FRAMEBUFFER, 0);
    GLU_CHECK_ERROR(gl.getError());
}

void drawFbo(const Context &ctx, GLuint fbo, Surface &dst)
{
    const RenderContext &renderCtx = ctx.getRenderContext();
    const Functions &gl            = renderCtx.getFunctions();

    GLU_CHECK_CALL_ERROR(gl.bindFramebuffer(GL_FRAMEBUFFER, fbo), gl.getError());

    dst.setSize(FRAMEBUFFER_SIZE, FRAMEBUFFER_SIZE);
    glu::readPixels(renderCtx, 0, 0, dst.getAccess());
    GLU_EXPECT_NO_ERROR(gl.getError(), "Read pixels from framebuffer");

    GLU_CHECK_CALL_ERROR(gl.bindFramebuffer(GL_FRAMEBUFFER, 0), gl.getError());
}

GLuint getFboAttachment(const Functions &gl, GLuint fbo, GLenum requiredType)
{
    GLint type = 0, name = 0;
    gl.bindFramebuffer(GL_FRAMEBUFFER, fbo);
    GLU_CHECK_CALL_ERROR(gl.getFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                                                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type),
                         gl.getError());

    if (GLenum(type) != requiredType || GLenum(type) == GL_NONE)
        return 0;

    GLU_CHECK_CALL_ERROR(gl.getFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                                                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &name),
                         gl.getError());
    gl.bindFramebuffer(GL_FRAMEBUFFER, 0);
    GLU_CHECK_ERROR(gl.getError());

    return name;
}

void FboAttacher::initAttachment(GLuint seed, GLuint element)
{
    Binder &binder = *getElementType().binder();
    Framebuffer fbo(getRenderContext());

    enableLogging(false);

    binder.enableLogging(false);
    binder.bind(element);
    initStorage();
    binder.bind(0);
    binder.enableLogging(true);

    attach(element, *fbo);
    setupFbo(getContext(), seed, *fbo);
    detach(element, *fbo);

    enableLogging(true);

    log() << TestLog::Message << "// Drew to " << getElementType().getName() << " " << element << " with seed " << seed
          << "." << TestLog::EndMessage;
}

void FboInputAttacher::drawContainer(GLuint fbo, Surface &dst)
{
    drawFbo(getContext(), fbo, dst);
    log() << TestLog::Message << "// Read pixels from framebuffer " << fbo << " to output image."
          << TestLog::EndMessage;
}

void FboOutputAttacher::setupContainer(GLuint seed, GLuint fbo)
{
    setupFbo(getContext(), seed, fbo);
    log() << TestLog::Message << "// Drew to framebuffer " << fbo << " with seed " << seed << "."
          << TestLog::EndMessage;
}

void FboOutputAttacher::drawAttachment(GLuint element, Surface &dst)
{
    Framebuffer fbo(getRenderContext());
    m_attacher.enableLogging(false);
    m_attacher.attach(element, *fbo);
    drawFbo(getContext(), *fbo, dst);
    m_attacher.detach(element, *fbo);
    m_attacher.enableLogging(true);
    log() << TestLog::Message << "// Read pixels from " << m_attacher.getElementType().getName() << " " << element
          << " to output image." << TestLog::EndMessage;
    GLU_CHECK_ERROR(gl().getError());
}

void TextureFboAttacher::attach(GLuint texture, GLuint fbo)
{
    GLU_CHECK_CALL_ERROR(glBindFramebuffer(GL_FRAMEBUFFER, fbo), gl().getError());
    GLU_CHECK_CALL_ERROR(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0),
                         gl().getError());
    GLU_CHECK_CALL_ERROR(glBindFramebuffer(GL_FRAMEBUFFER, 0), gl().getError());
}

void TextureFboAttacher::detach(GLuint texture, GLuint fbo)
{
    DE_UNREF(texture);
    GLU_CHECK_CALL_ERROR(glBindFramebuffer(GL_FRAMEBUFFER, fbo), gl().getError());
    GLU_CHECK_CALL_ERROR(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0),
                         gl().getError());
    GLU_CHECK_CALL_ERROR(glBindFramebuffer(GL_FRAMEBUFFER, 0), gl().getError());
}

GLuint TextureFboAttacher::getAttachment(GLuint fbo)
{
    return getFboAttachment(gl(), fbo, GL_TEXTURE);
}

static bool isTextureFormatColorRenderable(const glu::RenderContext &renderCtx, const glu::TransferFormat &format)
{
    const glw::Functions &gl = renderCtx.getFunctions();
    uint32_t curFbo          = ~0u;
    uint32_t curTex          = ~0u;
    uint32_t testFbo         = 0u;
    uint32_t testTex         = 0u;
    GLenum status            = GL_NONE;

    GLU_CHECK_GLW_CALL(gl, getIntegerv(GL_FRAMEBUFFER_BINDING, (int32_t *)&curFbo));
    GLU_CHECK_GLW_CALL(gl, getIntegerv(GL_TEXTURE_BINDING_2D, (int32_t *)&curTex));

    try
    {
        GLU_CHECK_GLW_CALL(gl, genTextures(1, &testTex));
        GLU_CHECK_GLW_CALL(gl, bindTexture(GL_TEXTURE_2D, testTex));
        GLU_CHECK_GLW_CALL(gl, texImage2D(GL_TEXTURE_2D, 0, format.format, FRAMEBUFFER_SIZE, FRAMEBUFFER_SIZE, 0,
                                          format.format, format.dataType, nullptr));

        GLU_CHECK_GLW_CALL(gl, genFramebuffers(1, &testFbo));
        GLU_CHECK_GLW_CALL(gl, bindFramebuffer(GL_FRAMEBUFFER, testFbo));
        GLU_CHECK_GLW_CALL(gl, framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, testTex, 0));

        status = gl.checkFramebufferStatus(GL_FRAMEBUFFER);
        GLU_CHECK_GLW_MSG(gl, "glCheckFramebufferStatus(GL_FRAMEBUFFER)");

        GLU_CHECK_GLW_CALL(gl, bindTexture(GL_TEXTURE_2D, curTex));
        GLU_CHECK_GLW_CALL(gl, bindFramebuffer(GL_FRAMEBUFFER, curFbo));

        GLU_CHECK_GLW_CALL(gl, deleteTextures(1, &testTex));
        GLU_CHECK_GLW_CALL(gl, deleteFramebuffers(1, &testFbo));
    }
    catch (...)
    {
        if (testTex != 0)
            gl.deleteTextures(1, &testTex);

        if (testFbo != 0)
            gl.deleteFramebuffers(1, &testFbo);

        throw;
    }

    if (status == GL_FRAMEBUFFER_COMPLETE)
        return true;
    else if (status == GL_FRAMEBUFFER_UNSUPPORTED)
        return false;
    else
        TCU_THROW(TestError, (std::string("glCheckFramebufferStatus() returned invalid result code ") +
                              de::toString(glu::getFramebufferStatusStr(status)))
                                 .c_str());
}

static glu::TransferFormat getRenderableColorTextureFormat(const glu::RenderContext &renderCtx)
{
    if (glu::contextSupports(renderCtx.getType(), glu::ApiType::es(3, 0)))
        return glu::TransferFormat(GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4);

    {
        const glu::TransferFormat candidates[] = {
            glu::TransferFormat(GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4),
            glu::TransferFormat(GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1),
            glu::TransferFormat(GL_RGB, GL_UNSIGNED_SHORT_5_6_5),
            glu::TransferFormat(GL_RGBA, GL_UNSIGNED_BYTE),
        };

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(candidates); ++ndx)
        {
            if (isTextureFormatColorRenderable(renderCtx, candidates[ndx]))
                return candidates[ndx];
        }
    }

    return glu::TransferFormat(GL_NONE, GL_NONE);
}

void TextureFboAttacher::initStorage(void)
{
    const glu::TransferFormat format = getRenderableColorTextureFormat(getRenderContext());

    if (format.format == GL_NONE)
        TCU_THROW(NotSupportedError, "No renderable texture format found");

    GLU_CHECK_CALL_ERROR(glTexImage2D(GL_TEXTURE_2D, 0, format.format, FRAMEBUFFER_SIZE, FRAMEBUFFER_SIZE, 0,
                                      format.format, format.dataType, nullptr),
                         gl().getError());
}

static bool isRenderbufferFormatColorRenderable(const glu::RenderContext &renderCtx, const uint32_t format)
{
    const glw::Functions &gl = renderCtx.getFunctions();
    uint32_t curFbo          = ~0u;
    uint32_t curRbo          = ~0u;
    uint32_t testFbo         = 0u;
    uint32_t testRbo         = 0u;
    GLenum status            = GL_NONE;

    GLU_CHECK_GLW_CALL(gl, getIntegerv(GL_FRAMEBUFFER_BINDING, (int32_t *)&curFbo));
    GLU_CHECK_GLW_CALL(gl, getIntegerv(GL_RENDERBUFFER_BINDING, (int32_t *)&curRbo));

    try
    {
        GLU_CHECK_GLW_CALL(gl, genRenderbuffers(1, &testRbo));
        GLU_CHECK_GLW_CALL(gl, bindRenderbuffer(GL_RENDERBUFFER, testRbo));
        GLU_CHECK_GLW_CALL(gl, renderbufferStorage(GL_RENDERBUFFER, format, FRAMEBUFFER_SIZE, FRAMEBUFFER_SIZE));

        GLU_CHECK_GLW_CALL(gl, genFramebuffers(1, &testFbo));
        GLU_CHECK_GLW_CALL(gl, bindFramebuffer(GL_FRAMEBUFFER, testFbo));
        GLU_CHECK_GLW_CALL(gl, framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, testRbo));

        status = gl.checkFramebufferStatus(GL_FRAMEBUFFER);
        GLU_CHECK_GLW_MSG(gl, "glCheckFramebufferStatus(GL_FRAMEBUFFER)");

        GLU_CHECK_GLW_CALL(gl, bindRenderbuffer(GL_RENDERBUFFER, curRbo));
        GLU_CHECK_GLW_CALL(gl, bindFramebuffer(GL_FRAMEBUFFER, curFbo));

        GLU_CHECK_GLW_CALL(gl, deleteRenderbuffers(1, &testRbo));
        GLU_CHECK_GLW_CALL(gl, deleteFramebuffers(1, &testFbo));
    }
    catch (...)
    {
        if (testRbo != 0)
            gl.deleteRenderbuffers(1, &testRbo);

        if (testFbo != 0)
            gl.deleteFramebuffers(1, &testFbo);

        throw;
    }

    if (status == GL_FRAMEBUFFER_COMPLETE)
        return true;
    else if (status == GL_FRAMEBUFFER_UNSUPPORTED)
        return false;
    else
        TCU_THROW(TestError, (std::string("glCheckFramebufferStatus() returned invalid result code ") +
                              de::toString(glu::getFramebufferStatusStr(status)))
                                 .c_str());
}

static uint32_t getRenderableColorRenderbufferFormat(const glu::RenderContext &renderCtx)
{
    if (glu::contextSupports(renderCtx.getType(), glu::ApiType::es(3, 0)))
        return GL_RGBA4;

    {
        const uint32_t candidates[] = {
            GL_RGBA4,
            GL_RGB5_A1,
            GL_RGB565,
        };

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(candidates); ++ndx)
        {
            if (isRenderbufferFormatColorRenderable(renderCtx, candidates[ndx]))
                return candidates[ndx];
        }
    }

    return GL_NONE;
}

void RboFboAttacher::initStorage(void)
{
    const uint32_t format = getRenderableColorRenderbufferFormat(getRenderContext());

    if (format == GL_NONE)
        TCU_THROW(TestError, "No color-renderable renderbuffer format found");

    GLU_CHECK_CALL_ERROR(glRenderbufferStorage(GL_RENDERBUFFER, format, FRAMEBUFFER_SIZE, FRAMEBUFFER_SIZE),
                         gl().getError());
}

void RboFboAttacher::attach(GLuint rbo, GLuint fbo)
{
    GLU_CHECK_CALL_ERROR(glBindFramebuffer(GL_FRAMEBUFFER, fbo), gl().getError());
    GLU_CHECK_CALL_ERROR(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo),
                         gl().getError());
    GLU_CHECK_CALL_ERROR(glBindFramebuffer(GL_FRAMEBUFFER, 0), gl().getError());
}

void RboFboAttacher::detach(GLuint rbo, GLuint fbo)
{
    DE_UNREF(rbo);
    GLU_CHECK_CALL_ERROR(glBindFramebuffer(GL_FRAMEBUFFER, fbo), gl().getError());
    GLU_CHECK_CALL_ERROR(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0),
                         gl().getError());
    GLU_CHECK_CALL_ERROR(glBindFramebuffer(GL_FRAMEBUFFER, 0), gl().getError());
}

GLuint RboFboAttacher::getAttachment(GLuint fbo)
{
    return getFboAttachment(gl(), fbo, GL_RENDERBUFFER);
}

static const char *const s_fragmentShaderTemplate =
    GLSL100_SRC(void main() { gl_FragColor = vec4(${RED}, ${GREEN}, ${BLUE}, 1.0); });

void ShaderProgramAttacher::initAttachment(GLuint seed, GLuint shader)
{
    using de::floatToString;
    using de::insert;

    Random rnd(seed);
    map<string, string> params;
    const StringTemplate sourceTmpl(s_fragmentShaderTemplate);

    insert(params, "RED", floatToString(rnd.getFloat(), 4));
    insert(params, "GREEN", floatToString(rnd.getFloat(), 4));
    insert(params, "BLUE", floatToString(rnd.getFloat(), 4));

    {
        const string source         = sourceTmpl.specialize(params);
        const char *const sourceStr = source.c_str();

        GLU_CHECK_CALL_ERROR(glShaderSource(shader, 1, &sourceStr, nullptr), gl().getError());
        GLU_CHECK_CALL_ERROR(glCompileShader(shader), gl().getError());

        {
            GLint compileStatus = 0;
            gl().getShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
            TCU_CHECK_MSG(compileStatus != 0, sourceStr);
        }
    }
}

void ShaderProgramAttacher::attach(GLuint shader, GLuint program)
{
    GLU_CHECK_CALL_ERROR(glAttachShader(program, shader), gl().getError());
}

void ShaderProgramAttacher::detach(GLuint shader, GLuint program)
{
    GLU_CHECK_CALL_ERROR(glDetachShader(program, shader), gl().getError());
}

GLuint ShaderProgramAttacher::getAttachment(GLuint program)
{
    GLuint shaders[2]        = {0, 0};
    const GLsizei shadersLen = DE_LENGTH_OF_ARRAY(shaders);
    GLsizei numShaders       = 0;
    GLuint ret               = 0;

    gl().getAttachedShaders(program, shadersLen, &numShaders, shaders);

    // There should ever be at most one attached shader in normal use, but if
    // something is wrong, the temporary vertex shader might not have been
    // detached properly, so let's find the fragment shader explicitly.
    for (int ndx = 0; ndx < de::min<GLsizei>(shadersLen, numShaders); ++ndx)
    {
        GLint shaderType = GL_NONE;
        gl().getShaderiv(shaders[ndx], GL_SHADER_TYPE, &shaderType);

        if (shaderType == GL_FRAGMENT_SHADER)
        {
            ret = shaders[ndx];
            break;
        }
    }

    return ret;
}

void setViewport(const RenderContext &renderCtx, const Rectangle &rect)
{
    renderCtx.getFunctions().viewport(rect.x, rect.y, rect.width, rect.height);
}

void readRectangle(const RenderContext &renderCtx, const Rectangle &rect, Surface &dst)
{
    dst.setSize(rect.width, rect.height);
    glu::readPixels(renderCtx, rect.x, rect.y, dst.getAccess());
}

Rectangle randomViewport(const RenderContext &ctx, GLint maxWidth, GLint maxHeight, Random &rnd)
{
    const RenderTarget &target = ctx.getRenderTarget();
    const GLint width          = de::min(target.getWidth(), maxWidth);
    const GLint xOff           = rnd.getInt(0, target.getWidth() - width);
    const GLint height         = de::min(target.getHeight(), maxHeight);
    const GLint yOff           = rnd.getInt(0, target.getHeight() - height);

    return Rectangle(xOff, yOff, width, height);
}

void ShaderProgramInputAttacher::drawContainer(GLuint program, Surface &dst)
{
    static const float s_vertices[6] = {-1.0, 0.0, 1.0, 1.0, 0.0, -1.0};
    Random rnd(program);
    CheckedShader vtxShader(getRenderContext(), SHADERTYPE_VERTEX, s_vertexShaderSrc);
    const Rectangle viewport = randomViewport(getRenderContext(), VIEWPORT_SIZE, VIEWPORT_SIZE, rnd);

    gl().attachShader(program, vtxShader.getShader());
    gl().linkProgram(program);

    {
        GLint linkStatus = 0;
        gl().getProgramiv(program, GL_LINK_STATUS, &linkStatus);
        TCU_CHECK(linkStatus != 0);
    }

    log() << TestLog::Message << "// Attached a temporary vertex shader and linked program " << program
          << TestLog::EndMessage;

    setViewport(getRenderContext(), viewport);
    log() << TestLog::Message << "// Positioned viewport randomly" << TestLog::EndMessage;

    glUseProgram(program);
    {
        GLint posLoc = gl().getAttribLocation(program, "pos");
        TCU_CHECK(posLoc >= 0);

        gl().enableVertexAttribArray(posLoc);

        gl().clearColor(0, 0, 0, 1);
        gl().clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        gl().vertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 0, s_vertices);
        gl().drawArrays(GL_TRIANGLES, 0, 3);

        gl().disableVertexAttribArray(posLoc);
        log() << TestLog::Message << "// Drew a fixed triangle" << TestLog::EndMessage;
    }
    glUseProgram(0);

    readRectangle(getRenderContext(), viewport, dst);
    log() << TestLog::Message << "// Copied viewport to output image" << TestLog::EndMessage;

    gl().detachShader(program, vtxShader.getShader());
    log() << TestLog::Message << "// Removed temporary vertex shader" << TestLog::EndMessage;
}

ES2Types::ES2Types(const Context &ctx)
    : Types(ctx)
    , m_bufferBind(ctx, &CallLogWrapper::glBindBuffer, GL_ARRAY_BUFFER, GL_ARRAY_BUFFER_BINDING)
    , m_bufferType(ctx, "buffer", &CallLogWrapper::glGenBuffers, &CallLogWrapper::glDeleteBuffers,
                   &CallLogWrapper::glIsBuffer, &m_bufferBind)
    , m_textureBind(ctx, &CallLogWrapper::glBindTexture, GL_TEXTURE_2D, GL_TEXTURE_BINDING_2D)
    , m_textureType(ctx, "texture", &CallLogWrapper::glGenTextures, &CallLogWrapper::glDeleteTextures,
                    &CallLogWrapper::glIsTexture, &m_textureBind)
    , m_rboBind(ctx, &CallLogWrapper::glBindRenderbuffer, GL_RENDERBUFFER, GL_RENDERBUFFER_BINDING)
    , m_rboType(ctx, "renderbuffer", &CallLogWrapper::glGenRenderbuffers, &CallLogWrapper::glDeleteRenderbuffers,
                &CallLogWrapper::glIsRenderbuffer, &m_rboBind)
    , m_fboBind(ctx, &CallLogWrapper::glBindFramebuffer, GL_FRAMEBUFFER, GL_FRAMEBUFFER_BINDING)
    , m_fboType(ctx, "framebuffer", &CallLogWrapper::glGenFramebuffers, &CallLogWrapper::glDeleteFramebuffers,
                &CallLogWrapper::glIsFramebuffer, &m_fboBind)
    , m_shaderType(ctx)
    , m_programType(ctx)
    , m_texFboAtt(ctx, m_textureType, m_fboType)
    , m_texFboInAtt(m_texFboAtt)
    , m_texFboOutAtt(m_texFboAtt)
    , m_rboFboAtt(ctx, m_rboType, m_fboType)
    , m_rboFboInAtt(m_rboFboAtt)
    , m_rboFboOutAtt(m_rboFboAtt)
    , m_shaderAtt(ctx, m_shaderType, m_programType)
    , m_shaderInAtt(m_shaderAtt)
{
    Type *const types[] = {&m_bufferType, &m_textureType, &m_rboType, &m_fboType, &m_shaderType, &m_programType};
    m_types.insert(m_types.end(), DE_ARRAY_BEGIN(types), DE_ARRAY_END(types));

    m_attachers.push_back(&m_texFboAtt);
    m_attachers.push_back(&m_rboFboAtt);
    m_attachers.push_back(&m_shaderAtt);

    m_inAttachers.push_back(&m_texFboInAtt);
    m_inAttachers.push_back(&m_rboFboInAtt);
    m_inAttachers.push_back(&m_shaderInAtt);

    m_outAttachers.push_back(&m_texFboOutAtt);
    m_outAttachers.push_back(&m_rboFboOutAtt);
}

class Name
{
public:
    Name(Type &type) : m_type(type), m_name(type.gen())
    {
    }
    Name(Type &type, GLuint name) : m_type(type), m_name(name)
    {
    }
    ~Name(void)
    {
        m_type.release(m_name);
    }
    GLuint operator*(void) const
    {
        return m_name;
    }

private:
    Type &m_type;
    const GLuint m_name;
};

class ResultCollector
{
public:
    ResultCollector(TestContext &testCtx);
    bool check(bool cond, const char *msg);
    void fail(const char *msg);
    void warn(const char *msg);
    ~ResultCollector(void);

private:
    void addResult(qpTestResult result, const char *msg);

    TestContext &m_testCtx;
    TestLog &m_log;
    qpTestResult m_result;
    const char *m_message;
};

ResultCollector::ResultCollector(TestContext &testCtx)
    : m_testCtx(testCtx)
    , m_log(testCtx.getLog())
    , m_result(QP_TEST_RESULT_PASS)
    , m_message("Pass")
{
}

bool ResultCollector::check(bool cond, const char *msg)
{
    if (!cond)
        fail(msg);
    return cond;
}

void ResultCollector::addResult(qpTestResult result, const char *msg)
{
    m_log << TestLog::Message << "// Fail: " << msg << TestLog::EndMessage;
    if (m_result == QP_TEST_RESULT_PASS)
    {
        m_result  = result;
        m_message = msg;
    }
    else
    {
        if (result == QP_TEST_RESULT_FAIL)
            m_result = result;
        m_message = "Multiple problems, see log for details";
    }
}

void ResultCollector::fail(const char *msg)
{
    addResult(QP_TEST_RESULT_FAIL, msg);
}

void ResultCollector::warn(const char *msg)
{
    addResult(QP_TEST_RESULT_QUALITY_WARNING, msg);
}

ResultCollector::~ResultCollector(void)
{
    m_testCtx.setTestResult(m_result, m_message);
}

class TestBase : public TestCase, protected CallLogWrapper
{
protected:
    TestBase(const char *name, const char *description, const Context &ctx);

    // Copy ContextWrapper since MI (except for CallLogWrapper) is a no-no.
    const Context &getContext(void) const
    {
        return m_ctx;
    }
    const RenderContext &getRenderContext(void) const
    {
        return m_ctx.getRenderContext();
    }
    const Functions &gl(void) const
    {
        return m_ctx.gl();
    }
    TestLog &log(void) const
    {
        return m_ctx.log();
    }
    void init(void);

    Context m_ctx;
    Random m_rnd;
};

TestBase::TestBase(const char *name, const char *description, const Context &ctx)
    : TestCase(ctx.getTestContext(), name, description)
    , CallLogWrapper(ctx.gl(), ctx.log())
    , m_ctx(ctx)
    , m_rnd(deStringHash(name))
{
    enableLogging(true);
}

void TestBase::init(void)
{
    m_rnd = Random(deStringHash(getName()));
}

class LifeTest : public TestBase
{
public:
    typedef void (LifeTest::*TestFunction)(void);

    LifeTest(const char *name, const char *description, Type &type, TestFunction test)
        : TestBase(name, description, type.getContext())
        , m_type(type)
        , m_test(test)
    {
    }

    IterateResult iterate(void);

    void testGen(void);
    void testDelete(void);
    void testBind(void);
    void testDeleteBound(void);
    void testBindNoGen(void);
    void testDeleteUsed(void);

private:
    Binder &binder(void)
    {
        return *m_type.binder();
    }

    Type &m_type;
    TestFunction m_test;
};

IterateResult LifeTest::iterate(void)
{
    (this->*m_test)();
    return STOP;
}

void LifeTest::testGen(void)
{
    ResultCollector errors(getTestContext());
    Name name(m_type);

    if (m_type.genCreates())
        errors.check(m_type.exists(*name), "Gen* should have created an object, but didn't");
    else
        errors.check(!m_type.exists(*name), "Gen* should not have created an object, but did");
}

void LifeTest::testDelete(void)
{
    ResultCollector errors(getTestContext());
    GLuint name = m_type.gen();

    m_type.release(name);
    errors.check(!m_type.exists(name), "Object still exists after deletion");
}

void LifeTest::testBind(void)
{
    ResultCollector errors(getTestContext());
    Name name(m_type);

    binder().bind(*name);
    GLU_EXPECT_NO_ERROR(gl().getError(), "Bind failed");
    errors.check(m_type.exists(*name), "Object does not exist after binding");
    binder().bind(0);
}

void LifeTest::testDeleteBound(void)
{
    const GLuint id = m_type.gen();
    ResultCollector errors(getTestContext());

    binder().bind(id);
    m_type.release(id);

    if (m_type.nameLingers())
    {
        errors.check(gl().getError() == GL_NO_ERROR, "Deleting bound object failed");
        errors.check(binder().getBinding() == id, "Deleting bound object did not retain binding");
        errors.check(m_type.exists(id), "Deleting bound object made its name invalid");
        errors.check(m_type.isDeleteFlagged(id), "Deleting bound object did not flag the object for deletion");
        binder().bind(0);
    }
    else
    {
        errors.check(gl().getError() == GL_NO_ERROR, "Deleting bound object failed");
        errors.check(binder().getBinding() == 0, "Deleting bound object did not remove binding");
        errors.check(!m_type.exists(id), "Deleting bound object did not make its name invalid");
        binder().bind(0);
    }

    errors.check(binder().getBinding() == 0, "Unbinding didn't remove binding");
    errors.check(!m_type.exists(id), "Name is still valid after deleting and unbinding");
}

void LifeTest::testBindNoGen(void)
{
    ResultCollector errors(getTestContext());
    const GLuint id = m_rnd.getUint32();

    if (!errors.check(!m_type.exists(id), "Randomly chosen identifier already exists"))
        return;

    Name name(m_type, id);
    binder().bind(*name);

    if (binder().genRequired())
    {
        errors.check(glGetError() == GL_INVALID_OPERATION,
                     "Did not fail when binding a name not generated by Gen* call");
        errors.check(!m_type.exists(*name), "Bind* created an object for a name not generated by a Gen* call");
    }
    else
    {
        errors.check(glGetError() == GL_NO_ERROR, "Failed when binding a name not generated by Gen* call");
        errors.check(m_type.exists(*name), "Object was not created by the Bind* call");
    }
}

void LifeTest::testDeleteUsed(void)
{
    ResultCollector errors(getTestContext());
    GLuint programId = 0;

    {
        CheckedShader vtxShader(getRenderContext(), SHADERTYPE_VERTEX, s_vertexShaderSrc);
        CheckedShader fragShader(getRenderContext(), SHADERTYPE_FRAGMENT, s_fragmentShaderSrc);
        CheckedProgram program(getRenderContext(), vtxShader.getShader(), fragShader.getShader());

        programId = program.getProgram();

        log() << TestLog::Message << "// Created and linked program " << programId << TestLog::EndMessage;
        GLU_CHECK_CALL_ERROR(glUseProgram(programId), gl().getError());

        log() << TestLog::Message << "// Deleted program " << programId << TestLog::EndMessage;
    }
    TCU_CHECK(glIsProgram(programId));
    {
        GLint deleteFlagged = 0;
        glGetProgramiv(programId, GL_DELETE_STATUS, &deleteFlagged);
        errors.check(deleteFlagged != 0, "Program object was not flagged as deleted");
    }
    GLU_CHECK_CALL_ERROR(glUseProgram(0), gl().getError());
    errors.check(!gl().isProgram(programId), "Deleted program name still valid after being made non-current");
}

class AttachmentTest : public TestBase
{
public:
    typedef void (AttachmentTest::*TestFunction)(void);
    AttachmentTest(const char *name, const char *description, Attacher &attacher, TestFunction test)
        : TestBase(name, description, attacher.getContext())
        , m_attacher(attacher)
        , m_test(test)
    {
    }
    IterateResult iterate(void);

    void testDeletedNames(void);
    void testDeletedBinding(void);
    void testDeletedReattach(void);

private:
    Attacher &m_attacher;
    const TestFunction m_test;
};

IterateResult AttachmentTest::iterate(void)
{
    (this->*m_test)();
    return STOP;
}

GLuint getAttachment(Attacher &attacher, GLuint container)
{
    const GLuint queriedAttachment = attacher.getAttachment(container);
    attacher.log() << TestLog::Message << "// Result of query for " << attacher.getElementType().getName()
                   << " attached to " << attacher.getContainerType().getName() << " " << container << ": "
                   << queriedAttachment << "." << TestLog::EndMessage;
    return queriedAttachment;
}

void AttachmentTest::testDeletedNames(void)
{
    Type &elemType      = m_attacher.getElementType();
    Type &containerType = m_attacher.getContainerType();
    Name container(containerType);
    ResultCollector errors(getTestContext());
    GLuint elementId = 0;

    {
        Name element(elemType);
        elementId = *element;
        m_attacher.initAttachment(0, *element);
        m_attacher.attach(*element, *container);
        errors.check(getAttachment(m_attacher, *container) == elementId,
                     "Attachment name not returned by query even before deletion.");
    }

    // "Such a container or other context may continue using the object, and
    // may still contain state identifying its name as being currently bound"
    //
    // We here interpret "may" to mean that whenever the container has a
    // deleted object attached to it, a query will return that object's former
    // name.
    errors.check(getAttachment(m_attacher, *container) == elementId,
                 "Attachment name not returned by query after attachment was deleted.");

    if (elemType.nameLingers())
        errors.check(elemType.exists(elementId), "Attached object name no longer valid after deletion.");
    else
        errors.check(!elemType.exists(elementId), "Attached object name still valid after deletion.");

    m_attacher.detach(elementId, *container);
    errors.check(getAttachment(m_attacher, *container) == 0,
                 "Attachment name returned by query even after detachment.");
    errors.check(!elemType.exists(elementId), "Deleted attached object name still usable after detachment.");
}

class InputAttachmentTest : public TestBase
{
public:
    InputAttachmentTest(const char *name, const char *description, InputAttacher &inputAttacher)
        : TestBase(name, description, inputAttacher.getContext())
        , m_inputAttacher(inputAttacher)
    {
    }

    IterateResult iterate(void);

private:
    InputAttacher &m_inputAttacher;
};

GLuint replaceName(Type &type, GLuint oldName, TestLog &log)
{
    const Binder *const binder = type.binder();
    const bool genRequired     = binder == nullptr || binder->genRequired();

    if (genRequired)
        return type.gen();

    log << TestLog::Message << "// Type does not require Gen* for binding, reusing old id " << oldName << "."
        << TestLog::EndMessage;

    return oldName;
}

IterateResult InputAttachmentTest::iterate(void)
{
    Attacher &attacher  = m_inputAttacher.getAttacher();
    Type &containerType = attacher.getContainerType();
    Type &elementType   = attacher.getElementType();
    Name container(containerType);
    GLuint elementId     = 0;
    const GLuint refSeed = m_rnd.getUint32();
    const GLuint newSeed = m_rnd.getUint32();
    ResultCollector errors(getTestContext());

    Surface refSurface; // Surface from drawing with refSeed-seeded attachment
    Surface delSurface; // Surface from drawing with deleted refSeed attachment
    Surface newSurface; // Surface from drawing with newSeed-seeded attachment

    log() << TestLog::Message << "Testing if writing to a newly created object modifies a deleted attachment"
          << TestLog::EndMessage;

    {
        ScopedLogSection section(log(), "Write to original", "Writing to an original attachment");
        const Name element(elementType);

        elementId = *element;
        attacher.initAttachment(refSeed, elementId);
        attacher.attach(elementId, *container);
        m_inputAttacher.drawContainer(*container, refSurface);
        // element gets deleted here
        log() << TestLog::Message << "// Deleting attachment";
    }
    {
        ScopedLogSection section(log(), "Write to new", "Writing to a new attachment after deleting the original");
        const GLuint newId = replaceName(elementType, elementId, log());
        const Name newElement(elementType, newId);

        attacher.initAttachment(newSeed, newId);

        m_inputAttacher.drawContainer(*container, delSurface);
        attacher.detach(elementId, *container);

        attacher.attach(newId, *container);
        m_inputAttacher.drawContainer(*container, newSurface);
        attacher.detach(newId, *container);
    }
    {
        const bool surfacesMatch =
            tcu::pixelThresholdCompare(log(), "Reading from deleted",
                                       "Comparison result from reading from a container with a deleted attachment "
                                       "before and after writing to a fresh object.",
                                       refSurface, delSurface, RGBA(0, 0, 0, 0), tcu::COMPARE_LOG_RESULT);

        errors.check(surfacesMatch, "Writing to a fresh object modified the container with a deleted attachment.");

        if (!surfacesMatch)
            log() << TestLog::Image("New attachment", "Container state after attached to the fresh object", newSurface);
    }

    return STOP;
}

class OutputAttachmentTest : public TestBase
{
public:
    OutputAttachmentTest(const char *name, const char *description, OutputAttacher &outputAttacher)
        : TestBase(name, description, outputAttacher.getContext())
        , m_outputAttacher(outputAttacher)
    {
    }
    IterateResult iterate(void);

private:
    OutputAttacher &m_outputAttacher;
};

IterateResult OutputAttachmentTest::iterate(void)
{
    Attacher &attacher  = m_outputAttacher.getAttacher();
    Type &containerType = attacher.getContainerType();
    Type &elementType   = attacher.getElementType();
    Name container(containerType);
    GLuint elementId     = 0;
    const GLuint refSeed = m_rnd.getUint32();
    const GLuint newSeed = m_rnd.getUint32();
    ResultCollector errors(getTestContext());
    Surface refSurface; // Surface drawn from attachment to refSeed container
    Surface newSurface; // Surface drawn from attachment to newSeed container
    Surface delSurface; // Like newSurface, after writing to a deleted attachment

    log() << TestLog::Message << "Testing if writing to a container with a deleted attachment "
          << "modifies a newly created object" << TestLog::EndMessage;

    {
        ScopedLogSection section(log(), "Write to existing", "Writing to a container with an existing attachment");
        const Name element(elementType);

        elementId = *element;
        attacher.initAttachment(0, elementId);
        attacher.attach(elementId, *container);

        // For reference purposes, make note of what refSeed looks like.
        m_outputAttacher.setupContainer(refSeed, *container);
        m_outputAttacher.drawAttachment(elementId, refSurface);
    }
    {
        ScopedLogSection section(log(), "Write to deleted", "Writing to a container after deletion of attachment");
        const GLuint newId = replaceName(elementType, elementId, log());
        const Name newElement(elementType, newId);

        log() << TestLog::Message << "Creating a new object " << newId << TestLog::EndMessage;

        log() << TestLog::Message << "Recording state of new object before writing to container" << TestLog::EndMessage;
        attacher.initAttachment(newSeed, newId);
        m_outputAttacher.drawAttachment(newId, newSurface);

        log() << TestLog::Message << "Writing to container" << TestLog::EndMessage;

        // Now re-write refSeed to the container.
        m_outputAttacher.setupContainer(refSeed, *container);
        // Does it affect the newly created attachment object?
        m_outputAttacher.drawAttachment(newId, delSurface);
    }
    attacher.detach(elementId, *container);

    const bool surfacesMatch =
        tcu::pixelThresholdCompare(log(), "Writing to deleted",
                                   "Comparison result from reading from a fresh object before and after "
                                   "writing to a container with a deleted attachment",
                                   newSurface, delSurface, RGBA(0, 0, 0, 0), tcu::COMPARE_LOG_RESULT);

    errors.check(surfacesMatch, "Writing to container with deleted attachment modified a new object.");

    if (!surfacesMatch)
        log() << TestLog::Image("Original attachment",
                                "Result of container modification on original attachment before deletion.", refSurface);
    return STOP;
}

struct LifeTestSpec
{
    const char *name;
    LifeTest::TestFunction func;
    bool needBind;
};

MovePtr<TestCaseGroup> createLifeTestGroup(TestContext &testCtx, const LifeTestSpec &spec, const vector<Type *> &types)
{
    MovePtr<TestCaseGroup> group(new TestCaseGroup(testCtx, spec.name, spec.name));

    for (vector<Type *>::const_iterator it = types.begin(); it != types.end(); ++it)
    {
        Type &type       = **it;
        const char *name = type.getName();
        if (!spec.needBind || type.binder() != nullptr)
            group->addChild(new LifeTest(name, name, type, spec.func));
    }

    return group;
}

static const LifeTestSpec s_lifeTests[] = {
    {"gen", &LifeTest::testGen, false},
    {"delete", &LifeTest::testDelete, false},
    {"bind", &LifeTest::testBind, true},
    {"delete_bound", &LifeTest::testDeleteBound, true},
    {"bind_no_gen", &LifeTest::testBindNoGen, true},
};

string attacherName(Attacher &attacher)
{
    ostringstream os;
    os << attacher.getElementType().getName() << "_" << attacher.getContainerType().getName();
    return os.str();
}

void addTestCases(TestCaseGroup &group, Types &types)
{
    TestContext &testCtx = types.getTestContext();

    for (const LifeTestSpec *it = DE_ARRAY_BEGIN(s_lifeTests); it != DE_ARRAY_END(s_lifeTests); ++it)
        group.addChild(createLifeTestGroup(testCtx, *it, types.getTypes()).release());

    {
        TestCaseGroup *const delUsedGroup = new TestCaseGroup(testCtx, "delete_used", "Delete current program");
        group.addChild(delUsedGroup);

        delUsedGroup->addChild(new LifeTest("program", "program", types.getProgramType(), &LifeTest::testDeleteUsed));
    }

    {
        TestCaseGroup *const attGroup = new TestCaseGroup(testCtx, "attach", "Attachment tests");
        group.addChild(attGroup);

        {
            TestCaseGroup *const nameGroup = new TestCaseGroup(testCtx, "deleted_name", "Name of deleted attachment");
            attGroup->addChild(nameGroup);

            const vector<Attacher *> &atts = types.getAttachers();
            for (vector<Attacher *>::const_iterator it = atts.begin(); it != atts.end(); ++it)
            {
                const string name = attacherName(**it);
                nameGroup->addChild(
                    new AttachmentTest(name.c_str(), name.c_str(), **it, &AttachmentTest::testDeletedNames));
            }
        }
        {
            TestCaseGroup *inputGroup = new TestCaseGroup(testCtx, "deleted_input", "Input from deleted attachment");
            attGroup->addChild(inputGroup);

            const vector<InputAttacher *> &inAtts = types.getInputAttachers();
            for (vector<InputAttacher *>::const_iterator it = inAtts.begin(); it != inAtts.end(); ++it)
            {
                const string name = attacherName((*it)->getAttacher());
                inputGroup->addChild(new InputAttachmentTest(name.c_str(), name.c_str(), **it));
            }
        }
        {
            TestCaseGroup *outputGroup = new TestCaseGroup(testCtx, "deleted_output", "Output to deleted attachment");
            attGroup->addChild(outputGroup);

            const vector<OutputAttacher *> &outAtts = types.getOutputAttachers();
            for (vector<OutputAttacher *>::const_iterator it = outAtts.begin(); it != outAtts.end(); ++it)
            {
                string name = attacherName((*it)->getAttacher());
                outputGroup->addChild(new OutputAttachmentTest(name.c_str(), name.c_str(), **it));
            }
        }
    }
}

} // namespace details
} // namespace LifetimeTests
} // namespace gls
} // namespace deqp
