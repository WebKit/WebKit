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
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
