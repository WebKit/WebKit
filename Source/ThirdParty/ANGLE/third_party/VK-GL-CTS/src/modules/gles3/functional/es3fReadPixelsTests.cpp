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
 * \brief Read pixels tests
 *//*--------------------------------------------------------------------*/

#include "es3fReadPixelsTests.hpp"

#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "tcuRenderTarget.hpp"

#include "deRandom.hpp"
#include "deMath.h"
#include "deString.h"
#include "deStringUtil.hpp"

#include "gluDefs.hpp"
#include "gluShaderProgram.hpp"
#include "gluStrUtil.hpp"
#include "gluTextureUtil.hpp"

#include <cstring>
#include <sstream>

#include "glw.h"

using std::vector;

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
    enum
    {
        FLAG_NO_FLAGS      = 0x0,
        FLAG_CHOOSE_FORMAT = 0x1,
        FLAG_USE_RBO       = 0x2,
    };

    ReadPixelsTest(Context &context, const char *name, const char *description, int flags, int alignment,
                   GLint rowLength, GLint skipRows, GLint skipPixels, GLenum format = GL_RGBA,
                   GLenum type = GL_UNSIGNED_BYTE);

    IterateResult iterate(void);
    void render(tcu::Texture2D &reference);

private:
    int m_seed;
    bool m_chooseFormat;
    bool m_useRenderBuffer;
    int m_alignment;
    GLint m_rowLength;
    GLint m_skipRows;
    GLint m_skipPixels;
    GLint m_format;
    GLint m_type;

    const int m_width;
    const int m_height;

    void getFormatInfo(tcu::TextureFormat &format, int &pixelSize);
    void clearColor(tcu::Texture2D &reference, vector<uint8_t> &pixelData, int pixelSize);
};

ReadPixelsTest::ReadPixelsTest(Context &context, const char *name, const char *description, int flags, int alignment,
                               GLint rowLength, GLint skipRows, GLint skipPixels, GLenum format, GLenum type)
    : TestCase(context, name, description)
    , m_seed(deStringHash(name))
    , m_chooseFormat((flags & FLAG_CHOOSE_FORMAT) != 0)
    , m_useRenderBuffer((flags & FLAG_USE_RBO) != 0)
    , m_alignment(alignment)
    , m_rowLength(rowLength)
    , m_skipRows(skipRows)
    , m_skipPixels(skipPixels)
    , m_format(format)
    , m_type(type)
    , m_width(13)
    , m_height(13)
{
}

void ReadPixelsTest::render(tcu::Texture2D &reference)
{
    // Create program
    const char *vertexSource = "#version 300 es\n"
                               "in mediump vec2 i_coord;\n"
                               "void main (void)\n"
                               "{\n"
                               "\tgl_Position = vec4(i_coord, 0.0, 1.0);\n"
                               "}\n";

    std::stringstream fragmentSource;

    fragmentSource << "#version 300 es\n";

    if (reference.getFormat().type == tcu::TextureFormat::SIGNED_INT32)
        fragmentSource << "layout(location = 0) out mediump ivec4 o_color;\n";
    else if (reference.getFormat().type == tcu::TextureFormat::UNSIGNED_INT32)
        fragmentSource << "layout(location = 0) out mediump uvec4 o_color;\n";
    else
        fragmentSource << "layout(location = 0) out mediump vec4 o_color;\n";

    fragmentSource << "void main (void)\n"
                      "{\n";

    if (reference.getFormat().type == tcu::TextureFormat::UNSIGNED_INT32)
        fragmentSource << "\to_color = uvec4(0, 0, 0, 1000);\n";
    else if (reference.getFormat().type == tcu::TextureFormat::SIGNED_INT32)
        fragmentSource << "\to_color = ivec4(0, 0, 0, 1000);\n";
    else
        fragmentSource << "\to_color = vec4(0.0, 0.0, 0.0, 1.0);\n";

    fragmentSource << "}\n";

    glu::ShaderProgram program(m_context.getRenderContext(),
                               glu::makeVtxFragSources(vertexSource, fragmentSource.str()));

    m_testCtx.getLog() << program;
    TCU_CHECK(program.isOk());
    GLU_CHECK_CALL(glUseProgram(program.getProgram()));

    // Render
    {
        const float coords[] = {-0.5f, -0.5f, 0.5f,  -0.5f, 0.5f,  0.5f,

                                0.5f,  0.5f,  -0.5f, 0.5f,  -0.5f, -0.5f};
        GLuint coordLoc;

        coordLoc = glGetAttribLocation(program.getProgram(), "i_coord");
        GLU_CHECK_MSG("glGetAttribLocation()");

        GLU_CHECK_CALL(glEnableVertexAttribArray(coordLoc));

        GLU_CHECK_CALL(glVertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, coords));

        GLU_CHECK_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
        GLU_CHECK_CALL(glDisableVertexAttribArray(coordLoc));
    }

    // Render reference

    const int coordX1 = (int)((-0.5f * (float)reference.getWidth() / 2.0f) + (float)reference.getWidth() / 2.0f);
    const int coordY1 = (int)((-0.5f * (float)reference.getHeight() / 2.0f) + (float)reference.getHeight() / 2.0f);
    const int coordX2 = (int)((0.5f * (float)reference.getWidth() / 2.0f) + (float)reference.getWidth() / 2.0f);
    const int coordY2 = (int)((0.5f * (float)reference.getHeight() / 2.0f) + (float)reference.getHeight() / 2.0f);

    for (int x = 0; x < reference.getWidth(); x++)
    {
        if (x < coordX1 || x > coordX2)
            continue;

        for (int y = 0; y < reference.getHeight(); y++)
        {
            if (y >= coordY1 && y <= coordY2)
            {
                if (reference.getFormat().type == tcu::TextureFormat::SIGNED_INT32)
                    reference.getLevel(0).setPixel(tcu::IVec4(0, 0, 0, 1000), x, y);
                else if (reference.getFormat().type == tcu::TextureFormat::UNSIGNED_INT32)
                    reference.getLevel(0).setPixel(tcu::UVec4(0, 0, 0, 1000), x, y);
                else
                    reference.getLevel(0).setPixel(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), x, y);
            }
        }
    }
}

void ReadPixelsTest::getFormatInfo(tcu::TextureFormat &format, int &pixelSize)
{
    if (m_chooseFormat)
    {
        GLU_CHECK_CALL(glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &m_format));
        GLU_CHECK_CALL(glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &m_type));

        if (m_format != GL_RGBA && m_format != GL_BGRA && m_format != GL_RGB)
            TCU_THROW(NotSupportedError, ("Unsupported IMPLEMENTATION_COLOR_READ_FORMAT: " +
                                          de::toString(glu::getTextureFormatStr(m_format)))
                                             .c_str());
        if (glu::getTypeName(m_type) == nullptr)
            TCU_THROW(NotSupportedError,
                      ("Unsupported GL_IMPLEMENTATION_COLOR_READ_TYPE: " + de::toString(tcu::Format::Hex<4>(m_type)))
                          .c_str());
    }

    format    = glu::mapGLTransferFormat(m_format, m_type);
    pixelSize = format.getPixelSize();
}

void ReadPixelsTest::clearColor(tcu::Texture2D &reference, vector<uint8_t> &pixelData, int pixelSize)
{
    de::Random rnd(m_seed);
    GLuint framebuffer  = 0;
    GLuint renderbuffer = 0;

    if (m_useRenderBuffer)
    {
        if (m_type == GL_UNSIGNED_INT)
        {
            GLU_CHECK_CALL(glGenRenderbuffers(1, &renderbuffer));
            GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer));
            GLU_CHECK_CALL(glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32UI, m_width, m_height));
        }
        else if (m_type == GL_INT)
        {
            GLU_CHECK_CALL(glGenRenderbuffers(1, &renderbuffer));
            GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer));
            GLU_CHECK_CALL(glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32I, m_width, m_height));
        }
        else
            DE_ASSERT(false);

        GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, 0));
        GLU_CHECK_CALL(glGenFramebuffers(1, &framebuffer));
        GLU_CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer));
        GLU_CHECK_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer));
    }
    else if (m_format == GL_RGBA || m_format == GL_BGRA || m_format == GL_RGB)
    {
        // Empty
    }
    else
        DE_ASSERT(false);

    GLU_CHECK_CALL(glViewport(0, 0, reference.getWidth(), reference.getHeight()));

    // Clear color
    if (m_format == GL_RGBA || m_format == GL_BGRA || m_format == GL_RGB)
    {
        const float red   = rnd.getFloat();
        const float green = rnd.getFloat();
        const float blue  = rnd.getFloat();
        const float alpha = rnd.getFloat();

        const GLfloat color[] = {red, green, blue, alpha};
        // Clear target
        GLU_CHECK_CALL(glClearColor(red, green, blue, alpha));
        m_testCtx.getLog() << tcu::TestLog::Message << "ClearColor: (" << red << ", " << green << ", " << blue << ")"
                           << tcu::TestLog::EndMessage;

        GLU_CHECK_CALL(glClearBufferfv(GL_COLOR, 0, color));

        tcu::clear(reference.getLevel(0), tcu::Vec4(red, green, blue, alpha));
    }
    else if (m_format == GL_RGBA_INTEGER)
    {
        if (m_type == GL_INT)
        {
            const GLint red   = rnd.getUint32();
            const GLint green = rnd.getUint32();
            const GLint blue  = rnd.getUint32();
            const GLint alpha = rnd.getUint32();

            const GLint color[] = {red, green, blue, alpha};
            m_testCtx.getLog() << tcu::TestLog::Message << "ClearColor: (" << red << ", " << green << ", " << blue
                               << ")" << tcu::TestLog::EndMessage;

            GLU_CHECK_CALL(glClearBufferiv(GL_COLOR, 0, color));

            tcu::clear(reference.getLevel(0), tcu::IVec4(red, green, blue, alpha));
        }
        else if (m_type == GL_UNSIGNED_INT)
        {
            const GLuint red   = rnd.getUint32();
            const GLuint green = rnd.getUint32();
            const GLuint blue  = rnd.getUint32();
            const GLuint alpha = rnd.getUint32();

            const GLuint color[] = {red, green, blue, alpha};
            m_testCtx.getLog() << tcu::TestLog::Message << "ClearColor: (" << red << ", " << green << ", " << blue
                               << ")" << tcu::TestLog::EndMessage;

            GLU_CHECK_CALL(glClearBufferuiv(GL_COLOR, 0, color));

            tcu::clear(reference.getLevel(0), tcu::UVec4(red, green, blue, alpha));
        }
        else
            DE_ASSERT(false);
    }
    else
        DE_ASSERT(false);

    render(reference);

    const int rowWidth = (m_rowLength == 0 ? m_width : m_rowLength) + m_skipPixels;
    const int rowPitch = m_alignment * deCeilFloatToInt32(float(pixelSize * rowWidth) / (float)m_alignment);

    pixelData.resize(rowPitch * (m_height + m_skipRows), 0);

    GLU_CHECK_CALL(glReadPixels(0, 0, m_width, m_height, m_format, m_type, &(pixelData[0])));

    if (framebuffer)
        GLU_CHECK_CALL(glDeleteFramebuffers(1, &framebuffer));

    if (renderbuffer)
        GLU_CHECK_CALL(glDeleteRenderbuffers(1, &renderbuffer));
}

TestCase::IterateResult ReadPixelsTest::iterate(void)
{
    tcu::TextureFormat format(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8);
    int pixelSize;

    getFormatInfo(format, pixelSize);
    m_testCtx.getLog() << tcu::TestLog::Message << "Format: " << glu::getTextureFormatStr(m_format)
                       << ", Type: " << glu::getTypeStr(m_type) << tcu::TestLog::EndMessage;

    tcu::Texture2D reference(format, m_width, m_height);
    reference.allocLevel(0);

    GLU_CHECK_CALL(glPixelStorei(GL_PACK_ALIGNMENT, m_alignment));
    m_testCtx.getLog() << tcu::TestLog::Message << "GL_PACK_ALIGNMENT: " << m_alignment << tcu::TestLog::EndMessage;

    GLU_CHECK_CALL(glPixelStorei(GL_PACK_ROW_LENGTH, m_rowLength));
    m_testCtx.getLog() << tcu::TestLog::Message << "GL_PACK_ROW_LENGTH: " << m_rowLength << tcu::TestLog::EndMessage;

    GLU_CHECK_CALL(glPixelStorei(GL_PACK_SKIP_ROWS, m_skipRows));
    m_testCtx.getLog() << tcu::TestLog::Message << "GL_PACK_SKIP_ROWS: " << m_skipRows << tcu::TestLog::EndMessage;

    GLU_CHECK_CALL(glPixelStorei(GL_PACK_SKIP_PIXELS, m_skipPixels));
    m_testCtx.getLog() << tcu::TestLog::Message << "GL_PACK_SKIP_PIXELS: " << m_skipPixels << tcu::TestLog::EndMessage;

    GLU_CHECK_CALL(glViewport(0, 0, m_width, m_height));

    vector<uint8_t> pixelData;
    clearColor(reference, pixelData, pixelSize);

    const int rowWidth = (m_rowLength == 0 ? m_width : m_rowLength);
    const int rowPitch = m_alignment * deCeilFloatToInt32((float)(pixelSize * rowWidth) / (float)m_alignment);
    const tcu::ConstPixelBufferAccess resultAccess = tcu::ConstPixelBufferAccess(
        format, m_width, m_height, 1, rowPitch, 0, &(pixelData[pixelSize * m_skipPixels + m_skipRows * rowPitch]));

    // \note Renderbuffers are never multisampled
    if (!m_useRenderBuffer && m_context.getRenderTarget().getNumSamples() > 1)
    {
        const tcu::IVec4 formatBitDepths = tcu::getTextureFormatBitDepth(format);
        const uint8_t redThreshold       = (uint8_t)deCeilFloatToInt32(
            256.0f *
            (2.0f / (float)(1 << deMin32(m_context.getRenderTarget().getPixelFormat().redBits, formatBitDepths.x()))));
        const uint8_t greenThreshold = (uint8_t)deCeilFloatToInt32(
            256.0f * (2.0f / (float)(1 << deMin32(m_context.getRenderTarget().getPixelFormat().greenBits,
                                                  formatBitDepths.y()))));
        const uint8_t blueThreshold = (uint8_t)deCeilFloatToInt32(
            256.0f *
            (2.0f / (float)(1 << deMin32(m_context.getRenderTarget().getPixelFormat().blueBits, formatBitDepths.z()))));
        const uint8_t alphaThreshold = (uint8_t)deCeilFloatToInt32(
            256.0f * (2.0f / (float)(1 << deMin32(m_context.getRenderTarget().getPixelFormat().alphaBits,
                                                  formatBitDepths.w()))));

        // bilinearCompare only accepts RGBA, UINT8
        tcu::Texture2D referenceRGBA8(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8),
                                      m_width, m_height);
        tcu::Texture2D resultRGBA8(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8),
                                   m_width, m_height);

        referenceRGBA8.allocLevel(0);
        resultRGBA8.allocLevel(0);

        tcu::copy(referenceRGBA8.getLevel(0), reference.getLevel(0));
        tcu::copy(resultRGBA8.getLevel(0), resultAccess);

        if (tcu::bilinearCompare(
                m_testCtx.getLog(), "Result", "Result", referenceRGBA8.getLevel(0), resultRGBA8.getLevel(0),
                tcu::RGBA(redThreshold, greenThreshold, blueThreshold, alphaThreshold), tcu::COMPARE_LOG_RESULT))
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        else
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
    }
    else
    {
        const tcu::IVec4 formatBitDepths = tcu::getTextureFormatBitDepth(format);
        const float redThreshold =
            2.0f / (float)(1 << deMin32(m_context.getRenderTarget().getPixelFormat().redBits, formatBitDepths.x()));
        const float greenThreshold =
            2.0f / (float)(1 << deMin32(m_context.getRenderTarget().getPixelFormat().greenBits, formatBitDepths.y()));
        const float blueThreshold =
            2.0f / (float)(1 << deMin32(m_context.getRenderTarget().getPixelFormat().blueBits, formatBitDepths.z()));
        const float alphaThreshold =
            2.0f / (float)(1 << deMin32(m_context.getRenderTarget().getPixelFormat().alphaBits, formatBitDepths.w()));

        // Compare
        if (tcu::floatThresholdCompare(m_testCtx.getLog(), "Result", "Result", reference.getLevel(0), resultAccess,
                                       tcu::Vec4(redThreshold, greenThreshold, blueThreshold, alphaThreshold),
                                       tcu::COMPARE_LOG_RESULT))
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        else
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
    }

    return STOP;
}

} // namespace

ReadPixelsTests::ReadPixelsTests(Context &context) : TestCaseGroup(context, "read_pixels", "ReadPixel tests")
{
}

void ReadPixelsTests::init(void)
{
    {
        TestCaseGroup *group = new TestCaseGroup(m_context, "alignment", "Read pixels pack alignment parameter tests");

        group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_1", "", ReadPixelsTest::FLAG_NO_FLAGS, 1, 0, 0, 0,
                                           GL_RGBA, GL_UNSIGNED_BYTE));
        group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_2", "", ReadPixelsTest::FLAG_NO_FLAGS, 2, 0, 0, 0,
                                           GL_RGBA, GL_UNSIGNED_BYTE));
        group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_4", "", ReadPixelsTest::FLAG_NO_FLAGS, 4, 0, 0, 0,
                                           GL_RGBA, GL_UNSIGNED_BYTE));
        group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_8", "", ReadPixelsTest::FLAG_NO_FLAGS, 8, 0, 0, 0,
                                           GL_RGBA, GL_UNSIGNED_BYTE));

        group->addChild(new ReadPixelsTest(m_context, "rgba_int_1", "", ReadPixelsTest::FLAG_USE_RBO, 1, 0, 0, 0,
                                           GL_RGBA_INTEGER, GL_INT));
        group->addChild(new ReadPixelsTest(m_context, "rgba_int_2", "", ReadPixelsTest::FLAG_USE_RBO, 2, 0, 0, 0,
                                           GL_RGBA_INTEGER, GL_INT));
        group->addChild(new ReadPixelsTest(m_context, "rgba_int_4", "", ReadPixelsTest::FLAG_USE_RBO, 4, 0, 0, 0,
                                           GL_RGBA_INTEGER, GL_INT));
        group->addChild(new ReadPixelsTest(m_context, "rgba_int_8", "", ReadPixelsTest::FLAG_USE_RBO, 8, 0, 0, 0,
                                           GL_RGBA_INTEGER, GL_INT));

        group->addChild(new ReadPixelsTest(m_context, "rgba_uint_1", "", ReadPixelsTest::FLAG_USE_RBO, 1, 0, 0, 0,
                                           GL_RGBA_INTEGER, GL_UNSIGNED_INT));
        group->addChild(new ReadPixelsTest(m_context, "rgba_uint_2", "", ReadPixelsTest::FLAG_USE_RBO, 2, 0, 0, 0,
                                           GL_RGBA_INTEGER, GL_UNSIGNED_INT));
        group->addChild(new ReadPixelsTest(m_context, "rgba_uint_4", "", ReadPixelsTest::FLAG_USE_RBO, 4, 0, 0, 0,
                                           GL_RGBA_INTEGER, GL_UNSIGNED_INT));
        group->addChild(new ReadPixelsTest(m_context, "rgba_uint_8", "", ReadPixelsTest::FLAG_USE_RBO, 8, 0, 0, 0,
                                           GL_RGBA_INTEGER, GL_UNSIGNED_INT));

        group->addChild(new ReadPixelsTest(m_context, "choose_1", "", ReadPixelsTest::FLAG_CHOOSE_FORMAT, 1, 0, 0, 0));
        group->addChild(new ReadPixelsTest(m_context, "choose_2", "", ReadPixelsTest::FLAG_CHOOSE_FORMAT, 2, 0, 0, 0));
        group->addChild(new ReadPixelsTest(m_context, "choose_4", "", ReadPixelsTest::FLAG_CHOOSE_FORMAT, 4, 0, 0, 0));
        group->addChild(new ReadPixelsTest(m_context, "choose_8", "", ReadPixelsTest::FLAG_CHOOSE_FORMAT, 8, 0, 0, 0));

        addChild(group);
    }

    {
        TestCaseGroup *group = new TestCaseGroup(m_context, "rowlength", "Read pixels rowlength test");
        group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_17", "", ReadPixelsTest::FLAG_NO_FLAGS, 4, 17, 0, 0,
                                           GL_RGBA, GL_UNSIGNED_BYTE));
        group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_19", "", ReadPixelsTest::FLAG_NO_FLAGS, 4, 19, 0, 0,
                                           GL_RGBA, GL_UNSIGNED_BYTE));
        group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_23", "", ReadPixelsTest::FLAG_NO_FLAGS, 4, 23, 0, 0,
                                           GL_RGBA, GL_UNSIGNED_BYTE));
        group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_29", "", ReadPixelsTest::FLAG_NO_FLAGS, 4, 29, 0, 0,
                                           GL_RGBA, GL_UNSIGNED_BYTE));

        group->addChild(new ReadPixelsTest(m_context, "rgba_int_17", "", ReadPixelsTest::FLAG_USE_RBO, 4, 17, 0, 0,
                                           GL_RGBA_INTEGER, GL_INT));
        group->addChild(new ReadPixelsTest(m_context, "rgba_int_19", "", ReadPixelsTest::FLAG_USE_RBO, 4, 19, 0, 0,
                                           GL_RGBA_INTEGER, GL_INT));
        group->addChild(new ReadPixelsTest(m_context, "rgba_int_23", "", ReadPixelsTest::FLAG_USE_RBO, 4, 23, 0, 0,
                                           GL_RGBA_INTEGER, GL_INT));
        group->addChild(new ReadPixelsTest(m_context, "rgba_int_29", "", ReadPixelsTest::FLAG_USE_RBO, 4, 29, 0, 0,
                                           GL_RGBA_INTEGER, GL_INT));

        group->addChild(new ReadPixelsTest(m_context, "rgba_uint_17", "", ReadPixelsTest::FLAG_USE_RBO, 4, 17, 0, 0,
                                           GL_RGBA_INTEGER, GL_UNSIGNED_INT));
        group->addChild(new ReadPixelsTest(m_context, "rgba_uint_19", "", ReadPixelsTest::FLAG_USE_RBO, 4, 19, 0, 0,
                                           GL_RGBA_INTEGER, GL_UNSIGNED_INT));
        group->addChild(new ReadPixelsTest(m_context, "rgba_uint_23", "", ReadPixelsTest::FLAG_USE_RBO, 4, 23, 0, 0,
                                           GL_RGBA_INTEGER, GL_UNSIGNED_INT));
        group->addChild(new ReadPixelsTest(m_context, "rgba_uint_29", "", ReadPixelsTest::FLAG_USE_RBO, 4, 29, 0, 0,
                                           GL_RGBA_INTEGER, GL_UNSIGNED_INT));

        group->addChild(
            new ReadPixelsTest(m_context, "choose_17", "", ReadPixelsTest::FLAG_CHOOSE_FORMAT, 4, 17, 0, 0));
        group->addChild(
            new ReadPixelsTest(m_context, "choose_19", "", ReadPixelsTest::FLAG_CHOOSE_FORMAT, 4, 19, 0, 0));
        group->addChild(
            new ReadPixelsTest(m_context, "choose_23", "", ReadPixelsTest::FLAG_CHOOSE_FORMAT, 4, 23, 0, 0));
        group->addChild(
            new ReadPixelsTest(m_context, "choose_29", "", ReadPixelsTest::FLAG_CHOOSE_FORMAT, 4, 29, 0, 0));

        addChild(group);
    }

    {
        TestCaseGroup *group = new TestCaseGroup(m_context, "skip", "Read pixels skip pixels and rows test");
        group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_0_3", "", ReadPixelsTest::FLAG_NO_FLAGS, 4, 17, 0, 3,
                                           GL_RGBA, GL_UNSIGNED_BYTE));
        group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_3_0", "", ReadPixelsTest::FLAG_NO_FLAGS, 4, 17, 3, 0,
                                           GL_RGBA, GL_UNSIGNED_BYTE));
        group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_3_3", "", ReadPixelsTest::FLAG_NO_FLAGS, 4, 17, 3, 3,
                                           GL_RGBA, GL_UNSIGNED_BYTE));
        group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_3_5", "", ReadPixelsTest::FLAG_NO_FLAGS, 4, 17, 3, 5,
                                           GL_RGBA, GL_UNSIGNED_BYTE));

        group->addChild(new ReadPixelsTest(m_context, "rgba_int_0_3", "", ReadPixelsTest::FLAG_USE_RBO, 4, 17, 0, 3,
                                           GL_RGBA_INTEGER, GL_INT));
        group->addChild(new ReadPixelsTest(m_context, "rgba_int_3_0", "", ReadPixelsTest::FLAG_USE_RBO, 4, 17, 3, 0,
                                           GL_RGBA_INTEGER, GL_INT));
        group->addChild(new ReadPixelsTest(m_context, "rgba_int_3_3", "", ReadPixelsTest::FLAG_USE_RBO, 4, 17, 3, 3,
                                           GL_RGBA_INTEGER, GL_INT));
        group->addChild(new ReadPixelsTest(m_context, "rgba_int_3_5", "", ReadPixelsTest::FLAG_USE_RBO, 4, 17, 3, 5,
                                           GL_RGBA_INTEGER, GL_INT));

        group->addChild(new ReadPixelsTest(m_context, "rgba_uint_0_3", "", ReadPixelsTest::FLAG_USE_RBO, 4, 17, 0, 3,
                                           GL_RGBA_INTEGER, GL_UNSIGNED_INT));
        group->addChild(new ReadPixelsTest(m_context, "rgba_uint_3_0", "", ReadPixelsTest::FLAG_USE_RBO, 4, 17, 3, 0,
                                           GL_RGBA_INTEGER, GL_UNSIGNED_INT));
        group->addChild(new ReadPixelsTest(m_context, "rgba_uint_3_3", "", ReadPixelsTest::FLAG_USE_RBO, 4, 17, 3, 3,
                                           GL_RGBA_INTEGER, GL_UNSIGNED_INT));
        group->addChild(new ReadPixelsTest(m_context, "rgba_uint_3_5", "", ReadPixelsTest::FLAG_USE_RBO, 4, 17, 3, 5,
                                           GL_RGBA_INTEGER, GL_UNSIGNED_INT));

        group->addChild(
            new ReadPixelsTest(m_context, "choose_0_3", "", ReadPixelsTest::FLAG_CHOOSE_FORMAT, 4, 17, 0, 3));
        group->addChild(
            new ReadPixelsTest(m_context, "choose_3_0", "", ReadPixelsTest::FLAG_CHOOSE_FORMAT, 4, 17, 3, 0));
        group->addChild(
            new ReadPixelsTest(m_context, "choose_3_3", "", ReadPixelsTest::FLAG_CHOOSE_FORMAT, 4, 17, 3, 3));
        group->addChild(
            new ReadPixelsTest(m_context, "choose_3_5", "", ReadPixelsTest::FLAG_CHOOSE_FORMAT, 4, 17, 3, 5));

        addChild(group);
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
