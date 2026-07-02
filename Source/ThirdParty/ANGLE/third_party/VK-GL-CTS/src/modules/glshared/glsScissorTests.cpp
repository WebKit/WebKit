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
 * \brief GLES Scissor tests
 *//*--------------------------------------------------------------------*/

#include "glsScissorTests.hpp"
#include "glsTextureTestUtil.hpp"

#include "deMath.h"
#include "deRandom.hpp"
#include "deUniquePtr.hpp"

#include "tcuTestCase.hpp"
#include "tcuImageCompare.hpp"
#include "tcuVector.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTexture.hpp"
#include "tcuStringTemplate.hpp"

#include "gluStrUtil.hpp"
#include "gluDrawUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "gluObjectWrapper.hpp"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include <map>

namespace deqp
{
namespace gls
{
namespace Functional
{
namespace
{

using namespace ScissorTestInternal;
using namespace glw; // GL types

using tcu::ConstPixelBufferAccess;
using tcu::PixelBufferAccess;
using tcu::TestLog;

using std::map;
using std::string;
using std::vector;
using tcu::IVec4;
using tcu::UVec4;
using tcu::Vec3;
using tcu::Vec4;

void drawQuad(const glw::Functions &gl, uint32_t program, const Vec3 &p0, const Vec3 &p1)
{
    // Vertex data.
    const float hz         = (p0.z() + p1.z()) * 0.5f;
    const float position[] = {p0.x(), p0.y(), p0.z(), 1.0f, p0.x(), p1.y(), hz,     1.0f,
                              p1.x(), p0.y(), hz,     1.0f, p1.x(), p1.y(), p1.z(), 1.0f};

    const uint16_t indices[] = {0, 1, 2, 2, 1, 3};

    const int32_t posLoc = gl.getAttribLocation(program, "a_position");

    gl.useProgram(program);
    gl.enableVertexAttribArray(posLoc);
    gl.vertexAttribPointer(posLoc, 4, GL_FLOAT, GL_FALSE, 0, &position[0]);

    gl.drawElements(GL_TRIANGLES, DE_LENGTH_OF_ARRAY(indices), GL_UNSIGNED_SHORT, &indices[0]);

    gl.disableVertexAttribArray(posLoc);
}

void drawPrimitives(const glw::Functions &gl, uint32_t program, const uint32_t type, const vector<float> &vertices,
                    const vector<uint16_t> &indices)
{
    const int32_t posLoc = gl.getAttribLocation(program, "a_position");

    TCU_CHECK(posLoc >= 0);

    gl.useProgram(program);
    gl.enableVertexAttribArray(posLoc);
    gl.vertexAttribPointer(posLoc, 4, GL_FLOAT, GL_FALSE, 0, &vertices[0]);

    gl.drawElements(type, GLsizei(indices.size()), GL_UNSIGNED_SHORT, &indices[0]);

    gl.disableVertexAttribArray(posLoc);
}

template <typename T>
void clearEdges(const tcu::PixelBufferAccess &access, const T &color, const IVec4 &scissorArea)
{
    for (int y = 0; y < access.getHeight(); y++)
        for (int x = 0; x < access.getWidth(); x++)
        {
            if (y < scissorArea.y() || y >= scissorArea.y() + scissorArea.w() || x < scissorArea.x() ||
                x >= scissorArea.x() + scissorArea.z())
                access.setPixel(color, x, y);
        }
}

glu::ProgramSources genShaders(glu::GLSLVersion version, bool isPoint)
{
    string vtxSource;

    if (isPoint)
    {
        vtxSource = "${VERSION}\n"
                    "${IN} highp vec4 a_position;\n"
                    "void main(){\n"
                    "    gl_Position = a_position;\n"
                    "    gl_PointSize = 1.0;\n"
                    "}\n";
    }
    else
    {
        vtxSource = "${VERSION}\n"
                    "${IN} highp vec4 a_position;\n"
                    "void main(){\n"
                    "    gl_Position = a_position;\n"
                    "}\n";
    }

    const string frgSource = "${VERSION}\n"
                             "${OUT_DECL}"
                             "uniform highp vec4 u_color;\n"
                             "void main(){\n"
                             "    ${OUTPUT} = u_color;\n"
                             "}\n";

    map<string, string> params;

    switch (version)
    {
    case glu::GLSL_VERSION_100_ES:
        params["VERSION"]  = "#version 100";
        params["IN"]       = "attribute";
        params["OUT_DECL"] = "";
        params["OUTPUT"]   = "gl_FragColor";
        break;

    case glu::GLSL_VERSION_300_ES:
    case glu::GLSL_VERSION_310_ES: // Assumed to support 3.0
        params["VERSION"]  = "#version 300 es";
        params["IN"]       = "in";
        params["OUT_DECL"] = "out mediump vec4 f_color;\n";
        params["OUTPUT"]   = "f_color";
        break;

    default:
        DE_FATAL("Unsupported version");
    }

    return glu::makeVtxFragSources(tcu::StringTemplate(vtxSource).specialize(params),
                                   tcu::StringTemplate(frgSource).specialize(params));
}

// Wrapper class, provides iterator & reporting logic
class ScissorCase : public tcu::TestCase
{
public:
    ScissorCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name, const char *desc,
                const Vec4 &scissorArea);
    virtual ~ScissorCase(void)
    {
    }

    virtual IterateResult iterate(void);

protected:
    virtual void render(GLuint program, const IVec4 &viewport) const = 0;

    // Initialize gl_PointSize to 1.0f when drawing points, or the point size is undefined according to spec.
    virtual bool isPoint(void) const = 0;

    glu::RenderContext &m_renderCtx;
    const Vec4 m_scissorArea;
};

ScissorCase::ScissorCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name, const char *desc,
                         const Vec4 &scissorArea)
    : TestCase(testCtx, name, desc)
    , m_renderCtx(renderCtx)
    , m_scissorArea(scissorArea)
{
}

ScissorCase::IterateResult ScissorCase::iterate(void)
{
    using TextureTestUtil::RandomViewport;

    const glw::Functions &gl            = m_renderCtx.getFunctions();
    TestLog &log                        = m_testCtx.getLog();
    const tcu::PixelFormat renderFormat = m_renderCtx.getRenderTarget().getPixelFormat();
    const tcu::Vec4 threshold =
        0.02f * UVec4(1u << de::max(0, 8 - renderFormat.redBits), 1u << de::max(0, 8 - renderFormat.greenBits),
                      1u << de::max(0, 8 - renderFormat.blueBits), 1u << de::max(0, 8 - renderFormat.alphaBits))
                    .asFloat();
    const glu::ShaderProgram shader(m_renderCtx,
                                    genShaders(glu::getContextTypeGLSLVersion(m_renderCtx.getType()), isPoint()));

    const RandomViewport viewport(m_renderCtx.getRenderTarget(), 256, 256, deStringHash(getName()));
    const IVec4 relScissorArea(
        int(m_scissorArea.x() * (float)viewport.width), int(m_scissorArea.y() * (float)viewport.height),
        int(m_scissorArea.z() * (float)viewport.width), int(m_scissorArea.w() * (float)viewport.height));
    const IVec4 absScissorArea(relScissorArea.x() + viewport.x, relScissorArea.y() + viewport.y, relScissorArea.z(),
                               relScissorArea.w());

    tcu::Surface refImage(viewport.width, viewport.height);
    tcu::Surface resImage(viewport.width, viewport.height);

    if (!shader.isOk())
    {
        log << shader;
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Shader compile/link failed");
        return STOP;
    }

    log << TestLog::Message << "Viewport area is " << IVec4(viewport.x, viewport.y, viewport.width, viewport.height)
        << TestLog::EndMessage;
    log << TestLog::Message << "Scissor area is " << absScissorArea << TestLog::EndMessage;

    // Render reference (no scissors)
    {
        log << TestLog::Message << "Rendering reference (scissors disabled)" << TestLog::EndMessage;

        gl.useProgram(shader.getProgram());
        gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);

        gl.clearColor(0.125f, 0.25f, 0.5f, 1.0f);
        gl.clearDepthf(1.0f);
        gl.clearStencil(0);
        gl.disable(GL_DEPTH_TEST);
        gl.disable(GL_STENCIL_TEST);
        gl.disable(GL_SCISSOR_TEST);
        gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        render(shader.getProgram(), IVec4(viewport.x, viewport.y, viewport.width, viewport.height));

        glu::readPixels(m_renderCtx, viewport.x, viewport.y, refImage.getAccess());
        GLU_CHECK_ERROR(gl.getError());
    }

    // Render result (scissors)
    {
        log << TestLog::Message << "Rendering result (scissors enabled)" << TestLog::EndMessage;

        gl.useProgram(shader.getProgram());
        gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);

        gl.clearColor(0.125f, 0.25f, 0.5f, 1.0f);
        gl.clearDepthf(1.0f);
        gl.clearStencil(0);
        gl.disable(GL_DEPTH_TEST);
        gl.disable(GL_STENCIL_TEST);
        gl.disable(GL_SCISSOR_TEST);
        gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        gl.scissor(absScissorArea.x(), absScissorArea.y(), absScissorArea.z(), absScissorArea.w());
        gl.enable(GL_SCISSOR_TEST);

        render(shader.getProgram(), IVec4(viewport.x, viewport.y, viewport.width, viewport.height));

        glu::readPixels(m_renderCtx, viewport.x, viewport.y, resImage.getAccess());
        GLU_CHECK_ERROR(gl.getError());
    }

    // Manual 'scissors' for reference image
    log << TestLog::Message << "Clearing area outside scissor area from reference" << TestLog::EndMessage;
    clearEdges(refImage.getAccess(), IVec4(32, 64, 128, 255), relScissorArea);

    if (tcu::floatThresholdCompare(log, "ComparisonResult", "Image comparison result", refImage.getAccess(),
                                   resImage.getAccess(), threshold, tcu::COMPARE_LOG_RESULT))
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    else
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");

    return STOP;
}

// Tests scissoring with multiple primitive types
class ScissorPrimitiveCase : public ScissorCase
{
public:
    ScissorPrimitiveCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name, const char *desc,
                         const Vec4 &scissorArea, const Vec4 &renderArea, PrimitiveType type, int primitiveCount);
    virtual ~ScissorPrimitiveCase(void)
    {
    }

protected:
    virtual void render(GLuint program, const IVec4 &viewport) const;
    virtual bool isPoint(void) const;

private:
    const Vec4 m_renderArea;
    const PrimitiveType m_primitiveType;
    const int m_primitiveCount;
};

ScissorPrimitiveCase::ScissorPrimitiveCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                           const char *desc, const Vec4 &scissorArea, const Vec4 &renderArea,
                                           PrimitiveType type, int primitiveCount)
    : ScissorCase(testCtx, renderCtx, name, desc, scissorArea)
    , m_renderArea(renderArea)
    , m_primitiveType(type)
    , m_primitiveCount(primitiveCount)
{
}

bool ScissorPrimitiveCase::isPoint(void) const
{
    return (m_primitiveType == POINT);
}

void ScissorPrimitiveCase::render(GLuint program, const IVec4 &) const
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    const Vec4 white(1.0f, 1.0f, 1.0f, 1.0);
    const Vec4 primitiveArea(m_renderArea.x() * 2.0f - 1.0f, m_renderArea.x() * 2.0f - 1.0f, m_renderArea.z() * 2.0f,
                             m_renderArea.w() * 2.0f);

    static const float quadPositions[] = {0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f};
    static const float triPositions[]  = {
        0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f,
    };
    static const float linePositions[] = {0.0f, 0.0f, 1.0f, 1.0f};
    static const float pointPosition[] = {0.5f, 0.5f};

    const float *positionSet[] = {pointPosition, linePositions, triPositions, quadPositions};
    const int vertexCountSet[] = {1, 2, 3, 4};
    const int indexCountSet[]  = {1, 2, 3, 6};

    const uint16_t baseIndices[] = {0, 1, 2, 2, 1, 3};
    const float *basePositions   = positionSet[m_primitiveType];
    const int vertexCount        = vertexCountSet[m_primitiveType];
    const int indexCount         = indexCountSet[m_primitiveType];

    const float scale =
        1.44f / deFloatSqrt(float(m_primitiveCount) *
                            2.0f); // Magic value to roughly fill the render area with primitives at a readable density
    vector<float> positions(4 * vertexCount * m_primitiveCount);
    vector<uint16_t> indices(indexCount * m_primitiveCount);
    de::Random rng(1234);

    for (int primNdx = 0; primNdx < m_primitiveCount; primNdx++)
    {
        const float dx = m_primitiveCount > 1 ? rng.getFloat() : 0.0f;
        const float dy = m_primitiveCount > 1 ? rng.getFloat() : 0.0f;

        for (int vertNdx = 0; vertNdx < vertexCount; vertNdx++)
        {
            const int ndx      = primNdx * 4 * vertexCount + vertNdx * 4;
            positions[ndx + 0] = (basePositions[vertNdx * 2 + 0] * scale + dx) * primitiveArea.z() + primitiveArea.x();
            positions[ndx + 1] = (basePositions[vertNdx * 2 + 1] * scale + dy) * primitiveArea.w() + primitiveArea.y();
            positions[ndx + 2] = 0.2f;
            positions[ndx + 3] = 1.0f;
        }

        for (int ndx = 0; ndx < indexCount; ndx++)
            indices[primNdx * indexCount + ndx] = (uint16_t)(baseIndices[ndx] + primNdx * vertexCount);
    }

    gl.uniform4fv(gl.getUniformLocation(program, "u_color"), 1, white.m_data);

    switch (m_primitiveType)
    {
    case TRIANGLE:
        drawPrimitives(gl, program, GL_TRIANGLES, positions, indices);
        break;
    case LINE:
        drawPrimitives(gl, program, GL_LINES, positions, indices);
        break;
    case POINT:
        drawPrimitives(gl, program, GL_POINTS, positions, indices);
        break;
    default:
        DE_ASSERT(false);
        break;
    }
}

// Test effect of scissor on default framebuffer clears
class ScissorClearCase : public ScissorCase
{
public:
    ScissorClearCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name, const char *desc,
                     const Vec4 &scissorArea, uint32_t clearMode);
    virtual ~ScissorClearCase(void)
    {
    }

    virtual void init(void);

protected:
    virtual void render(GLuint program, const IVec4 &viewport) const;
    virtual bool isPoint(void) const;

private:
    const uint32_t m_clearMode; //!< Combination of the flags accepted by glClear
};

ScissorClearCase::ScissorClearCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                   const char *desc, const Vec4 &scissorArea, uint32_t clearMode)
    : ScissorCase(testCtx, renderCtx, name, desc, scissorArea)
    , m_clearMode(clearMode)
{
}

void ScissorClearCase::init(void)
{
    if ((m_clearMode & GL_DEPTH_BUFFER_BIT) && m_renderCtx.getRenderTarget().getDepthBits() == 0)
        throw tcu::NotSupportedError("Cannot clear depth; no depth buffer present", "", __FILE__, __LINE__);
    else if ((m_clearMode & GL_STENCIL_BUFFER_BIT) && m_renderCtx.getRenderTarget().getStencilBits() == 0)
        throw tcu::NotSupportedError("Cannot clear stencil; no stencil buffer present", "", __FILE__, __LINE__);
}

bool ScissorClearCase::isPoint(void) const
{
    return false;
}

void ScissorClearCase::render(GLuint program, const IVec4 &) const
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    const Vec4 white(1.0f, 1.0f, 1.0f, 1.0);

    gl.clearColor(0.6f, 0.1f, 0.1f, 1.0);
    gl.clearDepthf(0.0f);

    if (m_clearMode & GL_DEPTH_BUFFER_BIT)
    {
        gl.enable(GL_DEPTH_TEST);
        gl.depthFunc(GL_GREATER);
    }

    if (m_clearMode & GL_STENCIL_BUFFER_BIT)
    {
        gl.clearStencil(123);
        gl.enable(GL_STENCIL_TEST);
        gl.stencilFunc(GL_EQUAL, 123, ~0u);
    }

    if (m_clearMode & GL_COLOR_BUFFER_BIT)
        gl.clearColor(0.1f, 0.6f, 0.1f, 1.0);

    gl.clear(m_clearMode);
    gl.disable(GL_SCISSOR_TEST);

    gl.uniform4fv(gl.getUniformLocation(program, "u_color"), 1, white.getPtr());

    if (!(m_clearMode & GL_COLOR_BUFFER_BIT))
        drawQuad(gl, program, Vec3(-1.0f, -1.0f, 0.5f), Vec3(1.0f, 1.0f, 0.5f));

    gl.disable(GL_DEPTH_TEST);
    gl.disable(GL_STENCIL_TEST);
}

class FramebufferBlitCase : public ScissorCase
{
public:
    FramebufferBlitCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name, const char *desc,
                        const Vec4 &scissorArea);
    virtual ~FramebufferBlitCase(void)
    {
    }

    virtual void init(void);
    virtual void deinit(void);

protected:
    typedef de::MovePtr<glu::Framebuffer> FramebufferP;

    enum
    {
        SIZE = 64
    };

    virtual void render(GLuint program, const IVec4 &viewport) const;
    virtual bool isPoint(void) const;

    FramebufferP m_fbo;
};

FramebufferBlitCase::FramebufferBlitCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                         const char *desc, const Vec4 &scissorArea)
    : ScissorCase(testCtx, renderCtx, name, desc, scissorArea)
{
}

void FramebufferBlitCase::init(void)
{
    if (m_renderCtx.getRenderTarget().getNumSamples())
        throw tcu::NotSupportedError("Cannot blit to multisampled framebuffer", "", __FILE__, __LINE__);

    const glw::Functions &gl = m_renderCtx.getFunctions();
    const glu::Renderbuffer colorbuf(gl);
    const tcu::Vec4 clearColor(1.0f, 0.5, 0.125f, 1.0f);

    m_fbo = FramebufferP(new glu::Framebuffer(gl));

    gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, **m_fbo);

    gl.bindRenderbuffer(GL_RENDERBUFFER, *colorbuf);
    gl.renderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, SIZE, SIZE);
    gl.framebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, *colorbuf);

    gl.clearBufferfv(GL_COLOR, 0, clearColor.getPtr());
    gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, m_renderCtx.getDefaultFramebuffer());
}

void FramebufferBlitCase::deinit(void)
{
    m_fbo.clear();
}

bool FramebufferBlitCase::isPoint(void) const
{
    return false;
}

void FramebufferBlitCase::render(GLuint program, const IVec4 &viewport) const
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    const int width                  = viewport.z();
    const int height                 = viewport.w();
    const int32_t defaultFramebuffer = m_renderCtx.getDefaultFramebuffer();

    DE_UNREF(program);

    // blit to default framebuffer
    gl.bindFramebuffer(GL_READ_FRAMEBUFFER, **m_fbo);
    gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, defaultFramebuffer);

    gl.blitFramebuffer(0, 0, SIZE, SIZE, viewport.x(), viewport.y(), viewport.x() + width, viewport.y() + height,
                       GL_COLOR_BUFFER_BIT, GL_NEAREST);

    gl.bindFramebuffer(GL_READ_FRAMEBUFFER, defaultFramebuffer);
}

struct BufferFmtDesc
{
    tcu::TextureFormat texFmt;
    GLenum colorFmt;
};

struct Color
{
    enum Type
    {
        FLOAT,
        INT,
        UINT
    };

    Type type;

    union
    {
        float f[4];
        int32_t i[4];
        uint32_t u[4];
    };

    Color(const float f_[4]) : type(FLOAT)
    {
        f[0] = f_[0];
        f[1] = f_[1];
        f[2] = f_[2];
        f[3] = f_[3];
    }
    Color(const int32_t i_[4]) : type(INT)
    {
        i[0] = i_[0];
        i[1] = i_[1];
        i[2] = i_[2];
        i[3] = i_[3];
    }
    Color(const uint32_t u_[4]) : type(UINT)
    {
        u[0] = u_[0];
        u[1] = u_[1];
        u[2] = u_[2];
        u[3] = u_[3];
    }
};

class FramebufferClearCase : public tcu::TestCase
{
public:
    FramebufferClearCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name, const char *desc,
                         ClearType clearType);
    virtual ~FramebufferClearCase(void)
    {
    }

    virtual IterateResult iterate(void);

private:
    static void clearBuffers(const glw::Functions &gl, Color color, float depth, int stencil);
    static Color getBaseColor(const BufferFmtDesc &bufferFmt);
    static Color getMainColor(const BufferFmtDesc &bufferFmt);
    static BufferFmtDesc getBufferFormat(ClearType type);

    virtual void render(GLuint program) const;
    virtual bool isPoint(void) const;

    glu::RenderContext &m_renderCtx;
    const ClearType m_clearType;
};

FramebufferClearCase::FramebufferClearCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                           const char *desc, ClearType clearType)
    : tcu::TestCase(testCtx, name, desc)
    , m_renderCtx(renderCtx)
    , m_clearType(clearType)
{
}

void FramebufferClearCase::clearBuffers(const glw::Functions &gl, Color color, float depth, int stencil)
{
    switch (color.type)
    {
    case Color::FLOAT:
        gl.clearBufferfv(GL_COLOR, 0, color.f);
        break;
    case Color::INT:
        gl.clearBufferiv(GL_COLOR, 0, color.i);
        break;
    case Color::UINT:
        gl.clearBufferuiv(GL_COLOR, 0, color.u);
        break;
    default:
        DE_ASSERT(false);
    }

    gl.clearBufferfv(GL_DEPTH, 0, &depth);
    gl.clearBufferiv(GL_STENCIL, 0, &stencil);
}

FramebufferClearCase::IterateResult FramebufferClearCase::iterate(void)
{
    TestLog &log             = m_testCtx.getLog();
    const glw::Functions &gl = m_renderCtx.getFunctions();
    const glu::ShaderProgram shader(m_renderCtx,
                                    genShaders(glu::getContextTypeGLSLVersion(m_renderCtx.getType()), isPoint()));

    const glu::Framebuffer fbo(gl);
    const glu::Renderbuffer colorbuf(gl);
    const glu::Renderbuffer depthbuf(gl);

    const BufferFmtDesc bufferFmt = getBufferFormat(m_clearType);
    const Color baseColor         = getBaseColor(bufferFmt);

    const int width  = 64;
    const int height = 64;

    const IVec4 scissorArea(8, 8, 48, 48);

    vector<uint8_t> refData(width * height * bufferFmt.texFmt.getPixelSize());
    vector<uint8_t> resData(width * height * bufferFmt.texFmt.getPixelSize());

    tcu::PixelBufferAccess refAccess(bufferFmt.texFmt, width, height, 1, &refData[0]);
    tcu::PixelBufferAccess resAccess(bufferFmt.texFmt, width, height, 1, &resData[0]);

    if (!shader.isOk())
    {
        log << shader;
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Shader compile/link failed");
        return STOP;
    }

    gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, *fbo);
    gl.bindFramebuffer(GL_READ_FRAMEBUFFER, *fbo);

    // Color
    gl.bindRenderbuffer(GL_RENDERBUFFER, *colorbuf);
    gl.renderbufferStorage(GL_RENDERBUFFER, bufferFmt.colorFmt, width, height);
    gl.framebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, *colorbuf);

    // Depth/stencil
    gl.bindRenderbuffer(GL_RENDERBUFFER, *depthbuf);
    gl.renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    gl.framebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, *depthbuf);

    log << TestLog::Message << "Scissor area is " << scissorArea << TestLog::EndMessage;

    // Render reference
    {
        log << TestLog::Message << "Rendering reference (scissors disabled)" << TestLog::EndMessage;

        gl.useProgram(shader.getProgram());
        gl.viewport(0, 0, width, height);

        gl.disable(GL_DEPTH_TEST);
        gl.disable(GL_STENCIL_TEST);
        gl.disable(GL_SCISSOR_TEST);

        clearBuffers(gl, baseColor, 1.0f, 0);

        render(shader.getProgram());

        glu::readPixels(m_renderCtx, 0, 0, refAccess);
        GLU_CHECK_ERROR(gl.getError());
    }

    // Render result
    {
        log << TestLog::Message << "Rendering result (scissors enabled)" << TestLog::EndMessage;

        gl.useProgram(shader.getProgram());
        gl.viewport(0, 0, width, height);

        gl.disable(GL_DEPTH_TEST);
        gl.disable(GL_STENCIL_TEST);
        gl.disable(GL_SCISSOR_TEST);

        clearBuffers(gl, baseColor, 1.0f, 0);

        gl.enable(GL_SCISSOR_TEST);
        gl.scissor(scissorArea.x(), scissorArea.y(), scissorArea.z(), scissorArea.w());

        render(shader.getProgram());

        glu::readPixels(m_renderCtx, 0, 0, resAccess);
        GLU_CHECK_ERROR(gl.getError());
    }

    {
        bool resultOk = false;

        switch (baseColor.type)
        {
        case Color::FLOAT:
            clearEdges(refAccess, Vec4(baseColor.f[0], baseColor.f[1], baseColor.f[2], baseColor.f[3]), scissorArea);
            resultOk = tcu::floatThresholdCompare(log, "ComparisonResult", "Image comparison result", refAccess,
                                                  resAccess, Vec4(0.02f, 0.02f, 0.02f, 0.02f), tcu::COMPARE_LOG_RESULT);
            break;

        case Color::INT:
            clearEdges(refAccess, IVec4(baseColor.i[0], baseColor.i[1], baseColor.i[2], baseColor.i[3]), scissorArea);
            resultOk = tcu::intThresholdCompare(log, "ComparisonResult", "Image comparison result", refAccess,
                                                resAccess, UVec4(2, 2, 2, 2), tcu::COMPARE_LOG_RESULT);
            break;

        case Color::UINT:
            clearEdges(refAccess, UVec4(baseColor.u[0], baseColor.u[1], baseColor.u[2], baseColor.u[3]), scissorArea);
            resultOk = tcu::intThresholdCompare(log, "ComparisonResult", "Image comparison result", refAccess,
                                                resAccess, UVec4(2, 2, 2, 2), tcu::COMPARE_LOG_RESULT);
            break;
        }

        if (resultOk)
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        else
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
    }

    return STOP;
}

Color FramebufferClearCase::getBaseColor(const BufferFmtDesc &bufferFmt)
{
    const float f[4]    = {0.125f, 0.25f, 0.5f, 1.0f};
    const int32_t i[4]  = {0, 0, 0, 0};
    const uint32_t u[4] = {0, 0, 0, 0};

    switch (bufferFmt.colorFmt)
    {
    case GL_RGBA8:
        return Color(f);
    case GL_RGBA8I:
        return Color(i);
    case GL_RGBA8UI:
        return Color(u);
    default:
        DE_ASSERT(false);
    }

    return Color(f);
}

Color FramebufferClearCase::getMainColor(const BufferFmtDesc &bufferFmt)
{
    const float f[4]    = {1.0f, 1.0f, 0.5f, 1.0f};
    const int32_t i[4]  = {127, -127, 0, 127};
    const uint32_t u[4] = {255, 255, 0, 255};

    switch (bufferFmt.colorFmt)
    {
    case GL_RGBA8:
        return Color(f);
    case GL_RGBA8I:
        return Color(i);
    case GL_RGBA8UI:
        return Color(u);
    default:
        DE_ASSERT(false);
    }

    return Color(f);
}

BufferFmtDesc FramebufferClearCase::getBufferFormat(ClearType type)
{
    BufferFmtDesc retval;

    switch (type)
    {
    case CLEAR_COLOR_FLOAT:
        retval.colorFmt = GL_RGBA16F;
        retval.texFmt   = tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::HALF_FLOAT);
        DE_FATAL(
            "Floating point clear not implemented"); // \todo [2014-1-23 otto] pixel read format & type, nothing guaranteed, need extension...
        break;

    case CLEAR_COLOR_INT:
        retval.colorFmt = GL_RGBA8I;
        retval.texFmt   = tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::SIGNED_INT32);
        break;

    case CLEAR_COLOR_UINT:
        retval.colorFmt = GL_RGBA8UI;
        retval.texFmt   = tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNSIGNED_INT32);
        break;

    default:
        retval.colorFmt = GL_RGBA8;
        retval.texFmt   = tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8);
        break;
    }

    return retval;
}

void FramebufferClearCase::render(GLuint program) const
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    const BufferFmtDesc bufferFmt = getBufferFormat(m_clearType);
    const Color clearColor        = getMainColor(bufferFmt);

    const int clearStencil = 123;
    const float clearDepth = 0.5f;

    switch (m_clearType)
    {
    case CLEAR_COLOR_FIXED:
        gl.clearBufferfv(GL_COLOR, 0, clearColor.f);
        break;
    case CLEAR_COLOR_FLOAT:
        gl.clearBufferfv(GL_COLOR, 0, clearColor.f);
        break;
    case CLEAR_COLOR_INT:
        gl.clearBufferiv(GL_COLOR, 0, clearColor.i);
        break;
    case CLEAR_COLOR_UINT:
        gl.clearBufferuiv(GL_COLOR, 0, clearColor.u);
        break;
    case CLEAR_DEPTH:
        gl.clearBufferfv(GL_DEPTH, 0, &clearDepth);
        break;
    case CLEAR_STENCIL:
        gl.clearBufferiv(GL_STENCIL, 0, &clearStencil);
        break;
    case CLEAR_DEPTH_STENCIL:
        gl.clearBufferfi(GL_DEPTH_STENCIL, 0, clearDepth, clearStencil);
        break;

    default:
        DE_ASSERT(false);
    }

    const bool useDepth   = (m_clearType == CLEAR_DEPTH || m_clearType == CLEAR_DEPTH_STENCIL);
    const bool useStencil = (m_clearType == CLEAR_STENCIL || m_clearType == CLEAR_DEPTH_STENCIL);

    // Render something to expose changes to depth/stencil buffer
    if (useDepth || useStencil)
    {
        if (useDepth)
            gl.enable(GL_DEPTH_TEST);

        if (useStencil)
            gl.enable(GL_STENCIL_TEST);

        gl.stencilFunc(GL_EQUAL, clearStencil, ~0u);
        gl.depthFunc(GL_GREATER);
        gl.disable(GL_SCISSOR_TEST);

        gl.uniform4fv(gl.getUniformLocation(program, "u_color"), 1, clearColor.f);
        drawQuad(gl, program, tcu::Vec3(-1.0f, -1.0f, 0.6f), tcu::Vec3(1.0f, 1.0f, 0.6f));
    }
}

bool FramebufferClearCase::isPoint(void) const
{
    return false;
}

} // namespace

namespace ScissorTestInternal
{

tcu::TestNode *createPrimitiveTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                   const char *desc, const Vec4 &scissorArea, const Vec4 &renderArea,
                                   PrimitiveType type, int primitiveCount)
{
    return new ScissorPrimitiveCase(testCtx, renderCtx, name, desc, scissorArea, renderArea, type, primitiveCount);
}

tcu::TestNode *createClearTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                               const char *desc, const Vec4 &scissorArea, uint32_t clearMode)
{
    return new ScissorClearCase(testCtx, renderCtx, name, desc, scissorArea, clearMode);
}

tcu::TestNode *createFramebufferClearTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                          const char *desc, ClearType clearType)
{
    return new FramebufferClearCase(testCtx, renderCtx, name, desc, clearType);
}

tcu::TestNode *createFramebufferBlitTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                         const char *desc, const Vec4 &scissorArea)
{
    return new FramebufferBlitCase(testCtx, renderCtx, name, desc, scissorArea);
}

} // namespace ScissorTestInternal
} // namespace Functional
} // namespace gls
} // namespace deqp
