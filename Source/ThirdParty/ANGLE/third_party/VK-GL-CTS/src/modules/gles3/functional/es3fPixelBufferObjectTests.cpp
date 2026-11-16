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
 * \brief Pixel Buffer Object tests
 *//*--------------------------------------------------------------------*/

#include "es3fPixelBufferObjectTests.hpp"

#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "tcuRenderTarget.hpp"

#include "gluTextureUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "gluShaderProgram.hpp"

#include "deRandom.hpp"
#include "deString.h"

#include <string>
#include <sstream>

#include "glw.h"

using std::string;
using std::stringstream;

namespace deqp
{
namespace gles3
{
namespace Functional
{

namespace
{

class ReadPixelsTest : public TestCase
{
public:
    struct TestSpec
    {
        enum FramebufferType
        {
            FRAMEBUFFERTYPE_NATIVE = 0,
            FRAMEBUFFERTYPE_RENDERBUFFER
        };

        string name;
        string description;

        bool useColorClear;
        bool renderTriangles;

        FramebufferType framebufferType;
        GLenum renderbufferFormat;
    };

    ReadPixelsTest(Context &context, const TestSpec &spec);
    ~ReadPixelsTest(void);

    void init(void);
    void deinit(void);

    IterateResult iterate(void);

private:
    void renderTriangle(const tcu::Vec3 &a, const tcu::Vec3 &b, const tcu::Vec3 &c);
    void clearColor(float r, float g, float b, float a);

    de::Random m_random;
    tcu::TestLog &m_log;
    glu::ShaderProgram *m_program;

    TestSpec::FramebufferType m_framebuffeType;

    GLenum m_renderbufferFormat;
    tcu::TextureChannelClass m_texChannelClass;

    bool m_useColorClears;
    bool m_renderTriangles;

    GLfloat m_colorScale;
};

ReadPixelsTest::ReadPixelsTest(Context &context, const TestSpec &spec)
    : TestCase(context, spec.name.c_str(), spec.description.c_str())
    , m_random(deStringHash(spec.name.c_str()))
    , m_log(m_testCtx.getLog())
    , m_program(NULL)
    , m_framebuffeType(spec.framebufferType)
    , m_renderbufferFormat(spec.renderbufferFormat)
    , m_texChannelClass(tcu::TEXTURECHANNELCLASS_LAST)
    , m_useColorClears(spec.useColorClear)
    , m_renderTriangles(spec.renderTriangles)
    , m_colorScale(1.0f)
{

    if (m_framebuffeType == TestSpec::FRAMEBUFFERTYPE_NATIVE)
    {
        m_colorScale = 1.0f;
    }
    else if (m_framebuffeType == TestSpec::FRAMEBUFFERTYPE_RENDERBUFFER)
    {
        m_texChannelClass = tcu::getTextureChannelClass(glu::mapGLInternalFormat(spec.renderbufferFormat).type);
        switch (m_texChannelClass)
        {
        case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
            m_colorScale = 1.0f;
            break;

        case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
            m_colorScale = 100.0f;
            break;

        case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
            m_colorScale = 100.0f;
            break;

        case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
            m_colorScale = 100.0f;
            break;

        default:
            DE_ASSERT(false);
        }
    }
    else
        DE_ASSERT(false);
}

ReadPixelsTest::~ReadPixelsTest(void)
{
}

void ReadPixelsTest::init(void)
{
    // Check extensions
    if (m_framebuffeType == TestSpec::FRAMEBUFFERTYPE_RENDERBUFFER)
    {
        bool supported = false;

        if (m_renderbufferFormat == GL_RGBA16F || m_renderbufferFormat == GL_RG16F)
        {
            std::istringstream extensions(std::string((const char *)glGetString(GL_EXTENSIONS)));
            std::string extension;

            while (std::getline(extensions, extension, ' '))
            {
                if (extension == "GL_EXT_color_buffer_half_float")
                {
                    supported = true;
                    break;
                }
                if (extension == "GL_EXT_color_buffer_float")
                {
                    supported = true;
                    break;
                }
            }
        }
        else if (m_renderbufferFormat == GL_RGBA32F || m_renderbufferFormat == GL_RG32F ||
                 m_renderbufferFormat == GL_R11F_G11F_B10F)
        {
            std::istringstream extensions(std::string((const char *)glGetString(GL_EXTENSIONS)));
            std::string extension;

            while (std::getline(extensions, extension, ' '))
            {
                if (extension == "GL_EXT_color_buffer_float")
                {
                    supported = true;
                    break;
                }
            }
        }
        else
            supported = true;

        if (!supported)
            throw tcu::NotSupportedError("Renderbuffer format not supported", "", __FILE__, __LINE__);
    }

    std::string outtype = "";

    if (m_framebuffeType == TestSpec::FRAMEBUFFERTYPE_NATIVE)
        outtype = "vec4";
    else if (m_framebuffeType == TestSpec::FRAMEBUFFERTYPE_RENDERBUFFER)
    {
        switch (m_texChannelClass)
        {
        case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
            outtype = "vec4";
            break;

        case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
            outtype = "ivec4";
            break;

        case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
            outtype = "uvec4";
            break;

        case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
            outtype = "vec4";
            break;

        default:
            DE_ASSERT(false);
        }
    }
    else
        DE_ASSERT(false);

    const char *vertexShaderSource = "#version 300 es\n"
                                     "in mediump vec3 a_position;\n"
                                     "in mediump vec4 a_color;\n"
                                     "uniform mediump float u_colorScale;\n"
                                     "out mediump vec4 v_color;\n"
                                     "void main(void)\n"
                                     "{\n"
                                     "\tgl_Position = vec4(a_position, 1.0);\n"
                                     "\tv_color = u_colorScale * a_color;\n"
                                     "}";

    stringstream fragmentShaderSource;

    fragmentShaderSource << "#version 300 es\n"
                            "in mediump vec4 v_color;\n";

    fragmentShaderSource << "layout (location = 0) out mediump " << outtype
                         << " o_color;\n"
                            "void main(void)\n"
                            "{\n"
                            "\to_color = "
                         << outtype
                         << "(v_color);\n"
                            "}";

    m_program = new glu::ShaderProgram(m_context.getRenderContext(),
                                       glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource.str()));

    if (!m_program->isOk())
    {
        m_log << *m_program;
        TCU_FAIL("Failed to compile shader");
    }
}

void ReadPixelsTest::deinit(void)
{
    if (m_program)
        delete m_program;
    m_program = NULL;
}

void ReadPixelsTest::renderTriangle(const tcu::Vec3 &a, const tcu::Vec3 &b, const tcu::Vec3 &c)
{
    float positions[3 * 3];

    positions[0] = a.x();
    positions[1] = a.y();
    positions[2] = a.z();

    positions[3] = b.x();
    positions[4] = b.y();
    positions[5] = b.z();

    positions[6] = c.x();
    positions[7] = c.y();
    positions[8] = c.z();

    float colors[] = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f};

    GLU_CHECK_CALL(glUseProgram(m_program->getProgram()));

    GLuint coordLoc      = (GLuint)-1;
    GLuint colorLoc      = (GLuint)-1;
    GLuint colorScaleLoc = (GLuint)-1;

    colorScaleLoc = glGetUniformLocation(m_program->getProgram(), "u_colorScale");
    TCU_CHECK(colorScaleLoc != (GLuint)-1);

    GLU_CHECK_CALL(glUniform1f(colorScaleLoc, m_colorScale));

    coordLoc = glGetAttribLocation(m_program->getProgram(), "a_position");
    TCU_CHECK(coordLoc != (GLuint)-1);

    colorLoc = glGetAttribLocation(m_program->getProgram(), "a_color");
    TCU_CHECK(colorLoc != (GLuint)-1);

    GLU_CHECK_CALL(glEnableVertexAttribArray(colorLoc));
    GLU_CHECK_CALL(glEnableVertexAttribArray(coordLoc));

    GLU_CHECK_CALL(glVertexAttribPointer(coordLoc, 3, GL_FLOAT, GL_FALSE, 0, positions));
    GLU_CHECK_CALL(glVertexAttribPointer(colorLoc, 4, GL_FLOAT, GL_FALSE, 0, colors));

    GLU_CHECK_CALL(glDrawArrays(GL_TRIANGLES, 0, 3));

    GLU_CHECK_CALL(glDisableVertexAttribArray(colorLoc));
    GLU_CHECK_CALL(glDisableVertexAttribArray(coordLoc));
}

void ReadPixelsTest::clearColor(float r, float g, float b, float a)
{
    if (m_framebuffeType == TestSpec::FRAMEBUFFERTYPE_NATIVE)
    {
        GLU_CHECK_CALL(glClearColor(r, g, b, a));
        GLU_CHECK_CALL(glClear(GL_COLOR_BUFFER_BIT));
    }
    else if (m_framebuffeType == TestSpec::FRAMEBUFFERTYPE_RENDERBUFFER)
    {
        switch (m_texChannelClass)
        {
        case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
        {
            GLU_CHECK_CALL(glClearColor(r, g, b, a));
            GLU_CHECK_CALL(glClear(GL_COLOR_BUFFER_BIT));
            break;
        }

        case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
        {
            GLint color[4] = {(GLint)r, (GLint)g, (GLint)b, (GLint)a};

            GLU_CHECK_CALL(glClearBufferiv(GL_COLOR, 0, color));
            break;
        }

        case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
        {
            GLuint color[4] = {(GLuint)r, (GLuint)g, (GLuint)b, (GLuint)a};

            GLU_CHECK_CALL(glClearBufferuiv(GL_COLOR, 0, color));
            break;
        }

        case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
        {
            GLfloat color[4] = {(GLfloat)r, (GLfloat)g, (GLfloat)b, (GLfloat)a};

            GLU_CHECK_CALL(glClearBufferfv(GL_COLOR, 0, color));
            break;
        }

        default:
            DE_ASSERT(false);
        }
    }
    else
        DE_ASSERT(false);
}

TestCase::IterateResult ReadPixelsTest::iterate(void)
{
    int width  = m_context.getRenderTarget().getWidth();
    int height = m_context.getRenderTarget().getHeight();

    GLuint framebuffer  = 0;
    GLuint renderbuffer = 0;

    switch (m_framebuffeType)
    {
    case TestSpec::FRAMEBUFFERTYPE_NATIVE:
        GLU_CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer));
        break;

    case TestSpec::FRAMEBUFFERTYPE_RENDERBUFFER:
    {
        GLU_CHECK_CALL(glGenFramebuffers(1, &framebuffer));
        GLU_CHECK_CALL(glGenRenderbuffers(1, &renderbuffer));

        GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer));
        GLU_CHECK_CALL(glRenderbufferStorage(GL_RENDERBUFFER, m_renderbufferFormat, width, height));

        GLU_CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer));
        GLU_CHECK_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer));

        break;
    }

    default:
        DE_ASSERT(false);
    }

    clearColor(m_colorScale * 0.4f, m_colorScale * 1.0f, m_colorScale * 0.5f, m_colorScale * 1.0f);

    if (m_useColorClears)
    {
        const int maxClearCount = 10;
        const int minClearCount = 6;
        const int minClearSize  = 15;

        int clearCount = m_random.getInt(minClearCount, maxClearCount);

        for (int clearNdx = 0; clearNdx < clearCount; clearNdx++)
        {
            int clearX = m_random.getInt(0, width - minClearSize);
            int clearY = m_random.getInt(0, height - minClearSize);

            int clearWidth  = m_random.getInt(minClearSize, width - clearX);
            int clearHeight = m_random.getInt(minClearSize, height - clearY);

            float clearRed   = m_colorScale * m_random.getFloat();
            float clearGreen = m_colorScale * m_random.getFloat();
            float clearBlue  = m_colorScale * m_random.getFloat();
            float clearAlpha = m_colorScale * (0.5f + 0.5f * m_random.getFloat());

            GLU_CHECK_CALL(glEnable(GL_SCISSOR_TEST));
            GLU_CHECK_CALL(glScissor(clearX, clearY, clearWidth, clearHeight));

            clearColor(clearRed, clearGreen, clearBlue, clearAlpha);
        }

        GLU_CHECK_CALL(glDisable(GL_SCISSOR_TEST));
    }

    if (m_renderTriangles)
    {
        const int minTriangleCount = 4;
        const int maxTriangleCount = 10;

        int triangleCount = m_random.getInt(minTriangleCount, maxTriangleCount);

        for (int triangleNdx = 0; triangleNdx < triangleCount; triangleNdx++)
        {
            float x1 = 2.0f * m_random.getFloat() - 1.0f;
            float y1 = 2.0f * m_random.getFloat() - 1.0f;
            float z1 = 2.0f * m_random.getFloat() - 1.0f;

            float x2 = 2.0f * m_random.getFloat() - 1.0f;
            float y2 = 2.0f * m_random.getFloat() - 1.0f;
            float z2 = 2.0f * m_random.getFloat() - 1.0f;

            float x3 = 2.0f * m_random.getFloat() - 1.0f;
            float y3 = 2.0f * m_random.getFloat() - 1.0f;
            float z3 = 2.0f * m_random.getFloat() - 1.0f;

            renderTriangle(tcu::Vec3(x1, y1, z1), tcu::Vec3(x2, y2, z2), tcu::Vec3(x3, y3, z3));
        }
    }

    tcu::TextureFormat readFormat;
    GLenum readPixelsFormat;
    GLenum readPixelsType;
    bool floatCompare;

    if (m_framebuffeType == TestSpec::FRAMEBUFFERTYPE_NATIVE)
    {
        readFormat       = glu::mapGLTransferFormat(GL_RGBA, GL_UNSIGNED_BYTE);
        readPixelsFormat = GL_RGBA;
        readPixelsType   = GL_UNSIGNED_BYTE;
        floatCompare     = false;
    }
    else if (m_framebuffeType == TestSpec::FRAMEBUFFERTYPE_RENDERBUFFER)
    {
        switch (m_texChannelClass)
        {
        case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
            readFormat       = glu::mapGLTransferFormat(GL_RGBA, GL_UNSIGNED_BYTE);
            readPixelsFormat = GL_RGBA;
            readPixelsType   = GL_UNSIGNED_BYTE;
            floatCompare     = true;
            break;

        case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
            readFormat       = glu::mapGLTransferFormat(GL_RGBA_INTEGER, GL_INT);
            readPixelsFormat = GL_RGBA_INTEGER;
            readPixelsType   = GL_INT;
            floatCompare     = false;
            break;

        case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
            readFormat       = glu::mapGLTransferFormat(GL_RGBA_INTEGER, GL_UNSIGNED_INT);
            readPixelsFormat = GL_RGBA_INTEGER;
            readPixelsType   = GL_UNSIGNED_INT;
            floatCompare     = false;
            break;

        case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
            readFormat       = glu::mapGLTransferFormat(GL_RGBA, GL_FLOAT);
            readPixelsFormat = GL_RGBA;
            readPixelsType   = GL_FLOAT;
            floatCompare     = true;
            break;

        default:
            DE_ASSERT(false);
            // Silence warnings
            readFormat       = glu::mapGLTransferFormat(GL_RGBA, GL_FLOAT);
            readPixelsFormat = GL_RGBA;
            readPixelsType   = GL_FLOAT;
            floatCompare     = true;
        }
    }
    else
    {
        // Silence warnings
        readFormat       = glu::mapGLTransferFormat(GL_RGBA, GL_FLOAT);
        readPixelsFormat = GL_RGBA;
        readPixelsType   = GL_FLOAT;
        floatCompare     = true;
        DE_ASSERT(false);
    }

    tcu::Texture2D readRefrence(readFormat, width, height);
    const int readDataSize = readRefrence.getWidth() * readRefrence.getHeight() * readFormat.getPixelSize();

    readRefrence.allocLevel(0);

    GLuint pixelBuffer = (GLuint)-1;

    GLU_CHECK_CALL(glGenBuffers(1, &pixelBuffer));
    GLU_CHECK_CALL(glBindBuffer(GL_PIXEL_PACK_BUFFER, pixelBuffer));
    GLU_CHECK_CALL(glBufferData(GL_PIXEL_PACK_BUFFER, readDataSize, NULL, GL_STREAM_READ));

    GLU_CHECK_CALL(glReadPixels(0, 0, width, height, readPixelsFormat, readPixelsType, 0));

    const uint8_t *bufferData =
        (const uint8_t *)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, readDataSize, GL_MAP_READ_BIT);
    GLU_CHECK_MSG("glMapBufferRange() failed");

    tcu::ConstPixelBufferAccess readResult(readFormat, width, height, 1, bufferData);

    GLU_CHECK_CALL(glBindBuffer(GL_PIXEL_PACK_BUFFER, 0));

    GLU_CHECK_CALL(
        glReadPixels(0, 0, width, height, readPixelsFormat, readPixelsType, readRefrence.getLevel(0).getDataPtr()));

    if (framebuffer)
        GLU_CHECK_CALL(glDeleteFramebuffers(1, &framebuffer));

    if (renderbuffer)
        GLU_CHECK_CALL(glDeleteRenderbuffers(1, &renderbuffer));

    bool isOk = false;

    if (floatCompare)
    {
        const tcu::IVec4 formatBitDepths = tcu::getTextureFormatBitDepth(readFormat);
        const float redThreshold =
            2.0f / (float)(1 << deMin32(m_context.getRenderTarget().getPixelFormat().redBits, formatBitDepths.x()));
        const float greenThreshold =
            2.0f / (float)(1 << deMin32(m_context.getRenderTarget().getPixelFormat().greenBits, formatBitDepths.y()));
        const float blueThreshold =
            2.0f / (float)(1 << deMin32(m_context.getRenderTarget().getPixelFormat().blueBits, formatBitDepths.z()));
        const float alphaThreshold =
            2.0f / (float)(1 << deMin32(m_context.getRenderTarget().getPixelFormat().alphaBits, formatBitDepths.w()));

        isOk = tcu::floatThresholdCompare(
            m_log, "Result comparision",
            "Result of read pixels to memory compared with result of read pixels to buffer", readRefrence.getLevel(0),
            readResult, tcu::Vec4(redThreshold, greenThreshold, blueThreshold, alphaThreshold),
            tcu::COMPARE_LOG_RESULT);
    }
    else
    {
        isOk = tcu::intThresholdCompare(m_log, "Result comparision",
                                        "Result of read pixels to memory compared with result of read pixels to buffer",
                                        readRefrence.getLevel(0), readResult, tcu::UVec4(0, 0, 0, 0),
                                        tcu::COMPARE_LOG_RESULT);
    }

    GLU_CHECK_CALL(glBindBuffer(GL_PIXEL_PACK_BUFFER, pixelBuffer));
    GLU_CHECK_CALL(glUnmapBuffer(GL_PIXEL_PACK_BUFFER));
    GLU_CHECK_CALL(glDeleteBuffers(1, &pixelBuffer));

    if (isOk)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        return STOP;
    }
    else
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        return STOP;
    }
}

class PBOSplitUploadTest : public TestCase
{
public:
    enum TestType
    {
        TESTTYPE_LAST_LINE = 0,
        TESTTYPE_LAST_PIXEL
    };

    PBOSplitUploadTest(Context &context, const char *name, const char *description, TestType testType)
        : TestCase(context, name, description)
        , m_testType(testType)
        , m_prog0(nullptr)
        , m_progTex(nullptr)
        , m_vao(0)
        , m_vbo(0)
        , m_frame(0)
        , m_seed(0xdeadbeef)
    {
        for (int i = 0; i < P; i++)
        {
            m_texs[i]     = 0;
            m_tex_w[i]    = 0;
            m_tex_h[i]    = 0;
            m_tex_type[i] = 0;
        }
    }

    ~PBOSplitUploadTest(void) override
    {
    }

    void init(void) override
    {
        m_frame = 0;
        m_seed  = 0x3833388d ^ (uint32_t)m_testType;

        const char *vSrc    = "#version 300 es\n"
                              "layout(location=0) in vec4 a;\n"
                              "void main() { gl_Position = a; }\n";
        const char *fSrc    = "#version 300 es\n"
                              "precision mediump float;\n"
                              "out vec4 c;\n"
                              "void main() { c = vec4(1.0); }\n";
        const char *vTexSrc = "#version 300 es\n"
                              "layout(location=0) in vec4 a;\n"
                              "out vec2 uv;\n"
                              "void main() { gl_Position = a; uv = a.xy * 0.5 + 0.5; }\n";
        const char *fTexSrc = "#version 300 es\n"
                              "precision mediump float;\n"
                              "uniform sampler2D tex;\n"
                              "in vec2 uv;\n"
                              "out vec4 c;\n"
                              "void main() { c = texture(tex, uv); }\n";

        m_prog0   = new glu::ShaderProgram(m_context.getRenderContext(), glu::makeVtxFragSources(vSrc, fSrc));
        m_progTex = new glu::ShaderProgram(m_context.getRenderContext(), glu::makeVtxFragSources(vTexSrc, fTexSrc));

        if (!m_prog0->isOk() || !m_progTex->isOk())
            throw tcu::TestError("Failed to compile shaders");

        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);

        const float verts[] = {-1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f,
                               -1.0f, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f,  1.0f, 1.0f};
        glGenBuffers(1, &m_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, 0);
        glEnableVertexAttribArray(0);
    }

    void deinit(void) override
    {
        for (int i = 0; i < P; i++)
        {
            if (m_texs[i])
            {
                glDeleteTextures(1, &m_texs[i]);
                m_texs[i] = 0;
            }
        }
        if (m_vbo)
        {
            glDeleteBuffers(1, &m_vbo);
            m_vbo = 0;
        }
        if (m_vao)
        {
            glDeleteVertexArrays(1, &m_vao);
            m_vao = 0;
        }

        delete m_prog0;
        m_prog0 = nullptr;
        delete m_progTex;
        m_progTex = nullptr;
    }

    IterateResult iterate(void) override
    {
        auto rng = [this]() -> uint32_t
        {
            m_seed ^= m_seed << 13;
            m_seed ^= m_seed >> 7;
            m_seed ^= m_seed << 17;
            return m_seed;
        };

        // Candidate texture dimensions (width/height in pixels)
        const int texSizes[] = {1, 2, 4, 8, 16, 32, 64, 128};

        const GLenum fmts[] = {GL_RGBA8, GL_SRGB8_ALPHA8};
        std::vector<uint8_t> px(65536, 0x42);

        GLuint p0   = m_prog0->getProgram();
        GLuint pTex = m_progTex->getProgram();

        glBindVertexArray(m_vao);
        glUseProgram(p0);

        // Fetch target size dynamically to be safe against smaller framebuffers
        const int targetWidth  = m_context.getRenderTarget().getWidth();
        const int targetHeight = m_context.getRenderTarget().getHeight();

        const int ACTIONS_PER_FRAME = 5000;
        for (int a = 0; a < ACTIONS_PER_FRAME; a++)
        {
            uint32_t r = rng() % 600;

            if (r < 150)
            {
                // copyTexImage2D / copyTexSubImage2D from default framebuffer
                int i = rng() % P;
                if (m_texs[i] && m_tex_w[i] >= 4 && m_tex_type[i] == 0)
                {
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glDrawArrays(GL_TRIANGLES, 0, 3);
                    glBindTexture(GL_TEXTURE_2D, m_texs[i]);
                    int copyW = de::min(m_tex_w[i], targetWidth);
                    int copyH = de::min(m_tex_h[i], targetHeight);
                    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, copyW, copyH);
                }
            }
            else if (r < 250)
            {
                // PBO texture upload + immediate delete (vulnerable pattern)
                int i = rng() % P;
                if (m_texs[i] && m_tex_w[i] > 0 && m_tex_type[i] == 0)
                {
                    GLuint pbo = 0;
                    int psz    = m_tex_w[i] * m_tex_h[i] * 4;
                    glGenBuffers(1, &pbo);
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
                    glBufferData(GL_PIXEL_UNPACK_BUFFER, psz, nullptr, GL_STREAM_DRAW);
                    glBindTexture(GL_TEXTURE_2D, m_texs[i]);
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_tex_w[i], m_tex_h[i], GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                    glDeleteBuffers(1, &pbo);
                    glDrawArrays(GL_TRIANGLES, 0, 3);
                }
            }
            else if (r < 300)
            {
                // Texture self-feedback
                int i = rng() % P;
                if (m_texs[i] && m_tex_w[i] >= 4 && m_tex_type[i] == 0)
                {
                    GLuint ff = 0;
                    glGenFramebuffers(1, &ff);
                    glBindFramebuffer(GL_FRAMEBUFFER, ff);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texs[i], 0);
                    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
                    {
                        glViewport(0, 0, m_tex_w[i], m_tex_h[i]);
                        glClear(GL_COLOR_BUFFER_BIT);
                        glDrawArrays(GL_TRIANGLES, 0, 3);
                        glUseProgram(pTex);
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, m_texs[i]);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                        glUseProgram(p0);
                    }
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glDeleteFramebuffers(1, &ff);

                    glViewport(0, 0, targetWidth, targetHeight);
                }
            }
            else if (r < 500)
            {
                // Texture creation
                int i = rng() % P;
                if (!m_texs[i])
                {
                    int w = texSizes[rng() % DE_LENGTH_OF_ARRAY(texSizes)];
                    glGenTextures(1, &m_texs[i]);
                    if (rng() % 5 == 0)
                    {
                        glBindTexture(GL_TEXTURE_CUBE_MAP, m_texs[i]);
                        const GLenum faces[] = {GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                                                GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                                                GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};
                        for (int k = 0; k < 6; k++)
                            glTexImage2D(faces[k], 0, GL_RGBA8, w, w, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                        m_tex_type[i] = 1; // 1 indicates Cubemap, 0 indicates 2D texture

                        m_tex_w[i] = w;
                        m_tex_h[i] = w; // Cubemaps must be square
                    }
                    else
                    {
                        glBindTexture(GL_TEXTURE_2D, m_texs[i]);
                        GLenum fmt = fmts[rng() % DE_LENGTH_OF_ARRAY(fmts)];

                        int h = (m_testType == TESTTYPE_LAST_PIXEL) ? 1 : w;

                        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                        m_tex_type[i] = 0;

                        m_tex_w[i] = w;
                        m_tex_h[i] = h;
                    }
                }
            }
            else if (r < 550)
            {
                // Immutable texture storage
                int i = rng() % P;
                if (!m_texs[i])
                {
                    glGenTextures(1, &m_texs[i]);
                    glBindTexture(GL_TEXTURE_2D, m_texs[i]);
                    const int cand[] = {8, 16, 32, 64};
                    int w            = cand[rng() % 4];

                    int h = (m_testType == TESTTYPE_LAST_PIXEL) ? 1 : w;

                    int levels = deLog2Floor32(w) + 1;
                    glTexStorage2D(GL_TEXTURE_2D, levels, GL_RGBA8, w, h);
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
                    glGenerateMipmap(GL_TEXTURE_2D);

                    m_tex_w[i]    = w;
                    m_tex_h[i]    = h;
                    m_tex_type[i] = 0;
                }
            }
            else if (rng() % 3 == 0)
            {
                int j = rng() % P;
                if (m_texs[j])
                {
                    glDeleteTextures(1, &m_texs[j]);
                    m_texs[j] = 0;
                }
            }

            if (a % 256 == 0)
            {
                // Periodically clear the GL error queue to ignore generated errors
                // and prevent driver crashes due to error queue overflow.
                while (glGetError() != GL_NO_ERROR)
                {
                }
            }
        }

        // Clear any remaining errors at the end of the frame
        while (glGetError() != GL_NO_ERROR)
        {
        }

        // Draw to default framebuffer for swap
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        m_frame++;
        const int MAX_FRAMES = 50;
        if (m_frame < MAX_FRAMES)
        {
            return CONTINUE;
        }

        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        return STOP;
    }

private:
    static const int P = 16;
    TestType m_testType;
    glu::ShaderProgram *m_prog0;
    glu::ShaderProgram *m_progTex;
    GLuint m_vao;
    GLuint m_vbo;
    int m_frame;
    uint32_t m_seed;
    GLuint m_texs[P];
    int m_tex_w[P];
    int m_tex_h[P];
    int m_tex_type[P]; // 0 for 2D texture, 1 for cubemap
};

} // namespace

PixelBufferObjectTests::PixelBufferObjectTests(Context &context)
    : TestCaseGroup(context, "pbo", "Pixel buffer objects tests")
{
}

PixelBufferObjectTests::~PixelBufferObjectTests(void)
{
}

void PixelBufferObjectTests::init(void)
{
    TestCaseGroup *nativeFramebufferGroup =
        new TestCaseGroup(m_context, "native", "Tests with reading from native framebuffer");

    ReadPixelsTest::TestSpec nativeFramebufferTests[] = {
        {"clears", "Simple read pixels test with color clears", true, false,
         ReadPixelsTest::TestSpec::FRAMEBUFFERTYPE_NATIVE, GL_NONE},
        {"triangles", "Simple read pixels test rendering triangles", false, true,
         ReadPixelsTest::TestSpec::FRAMEBUFFERTYPE_NATIVE, GL_NONE}};

    for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(nativeFramebufferTests); testNdx++)
    {
        nativeFramebufferGroup->addChild(new ReadPixelsTest(m_context, nativeFramebufferTests[testNdx]));
    }

    addChild(nativeFramebufferGroup);

    TestCaseGroup *renderbufferGroup =
        new TestCaseGroup(m_context, "renderbuffer", "Tests with reading from renderbuffer");

    GLenum renderbufferFormats[] = {GL_RGBA8,
                                    GL_RGBA8I,
                                    GL_RGBA8UI,
                                    GL_RGBA16F,
                                    GL_RGBA16I,
                                    GL_RGBA16UI,
                                    GL_RGBA32F,
                                    GL_RGBA32I,
                                    GL_RGBA32UI,

                                    GL_SRGB8_ALPHA8,
                                    GL_RGB10_A2,
                                    GL_RGB10_A2UI,
                                    GL_RGBA4,
                                    GL_RGB5_A1,

                                    GL_RGB8,
                                    GL_RGB565,

                                    GL_R11F_G11F_B10F,

                                    GL_RG8,
                                    GL_RG8I,
                                    GL_RG8UI,
                                    GL_RG16F,
                                    GL_RG16I,
                                    GL_RG16UI,
                                    GL_RG32F,
                                    GL_RG32I,
                                    GL_RG32UI};

    const char *renderbufferFormatsStr[] = {"rgba8",
                                            "rgba8i",
                                            "rgba8ui",
                                            "rgba16f",
                                            "rgba16i",
                                            "rgba16ui",
                                            "rgba32f",
                                            "rgba32i",
                                            "rgba32ui",

                                            "srgb8_alpha8",
                                            "rgb10_a2",
                                            "rgb10_a2ui",
                                            "rgba4",
                                            "rgb5_a1",

                                            "rgb8",
                                            "rgb565",

                                            "r11f_g11f_b10f",

                                            "rg8",
                                            "rg8i",
                                            "rg8ui",
                                            "rg16f",
                                            "rg16i",
                                            "rg16ui",
                                            "rg32f",
                                            "rg32i",
                                            "rg32ui"};

    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(renderbufferFormatsStr) == DE_LENGTH_OF_ARRAY(renderbufferFormats));

    for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(renderbufferFormats); formatNdx++)
    {
        for (int trianglesClears = 0; trianglesClears < 2; trianglesClears++)
        {
            ReadPixelsTest::TestSpec testSpec;

            testSpec.name =
                string(renderbufferFormatsStr[formatNdx]) + "_" + (trianglesClears == 0 ? "triangles" : "clears");
            testSpec.description        = testSpec.name;
            testSpec.useColorClear      = trianglesClears == 1;
            testSpec.renderTriangles    = trianglesClears == 0;
            testSpec.framebufferType    = ReadPixelsTest::TestSpec::FRAMEBUFFERTYPE_RENDERBUFFER;
            testSpec.renderbufferFormat = renderbufferFormats[formatNdx];

            renderbufferGroup->addChild(new ReadPixelsTest(m_context, testSpec));
        }
    }

    addChild(renderbufferGroup);

    // PBO Upload and split transfer tests (matching ANGLE split upload tests)
    TestCaseGroup *uploadGroup =
        new TestCaseGroup(m_context, "upload", "Tests for PBO texture upload and split transfers");

    uploadGroup->addChild(new PBOSplitUploadTest(
        m_context, "split_upload_last_line",
        "Test that full level 0 PBO uploads with height > 1 correctly upload the last line when split",
        PBOSplitUploadTest::TESTTYPE_LAST_LINE));

    uploadGroup->addChild(new PBOSplitUploadTest(
        m_context, "split_upload_last_pixel",
        "Test that full level 0 PBO uploads with height == 1 correctly upload the last pixel when split",
        PBOSplitUploadTest::TESTTYPE_LAST_PIXEL));

    addChild(uploadGroup);
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
