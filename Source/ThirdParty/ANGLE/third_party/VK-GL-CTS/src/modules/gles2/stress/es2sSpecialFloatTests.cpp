/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 2.0 Module
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
 * \brief Special float stress tests.
 *//*--------------------------------------------------------------------*/

#include "es2sSpecialFloatTests.hpp"
#include "gluRenderContext.hpp"
#include "gluShaderProgram.hpp"
#include "gluPixelTransfer.hpp"
#include "gluStrUtil.hpp"
#include "gluContextInfo.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuSurface.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"
#include "deStringUtil.hpp"
#include "deMath.h"
#include "deRandom.hpp"

#include <limits>
#include <sstream>

using namespace glw;

namespace deqp
{
namespace gles2
{
namespace Stress
{
namespace
{

static const int TEST_CANVAS_SIZE       = 256;
static const int TEST_TEXTURE_SIZE      = 128;
static const int TEST_TEXTURE_CUBE_SIZE = 32;
static const uint32_t s_specialFloats[] = {
    0x00000000, //          zero
    0x80000000, // negative zero
    0x3F800000, //          one
    0xBF800000, // negative one
    0x00800000, // minimum positive normalized value
    0x80800000, // maximum negative normalized value
    0x00000001, // minimum positive denorm value
    0x80000001, // maximum negative denorm value
    0x7F7FFFFF, // maximum finite value.
    0xFF7FFFFF, // minimum finite value.
    0x7F800000, //  inf
    0xFF800000, // -inf
    0x34000000, //          epsilon
    0xB4000000, // negative epsilon
    0x7FC00000, //          quiet_NaN
    0xFFC00000, // negative quiet_NaN
    0x7FC00001, //          signaling_NaN
    0xFFC00001, // negative signaling_NaN
    0x7FEAAAAA, //          quiet payloaded NaN        (payload of repeated pattern of 101010...)
    0xFFEAAAAA, // negative quiet payloaded NaN        ( .. )
    0x7FAAAAAA, //          signaling payloaded NaN    ( .. )
    0xFFAAAAAA, // negative signaling payloaded NaN    ( .. )
};

static const char *const s_colorPassthroughFragmentShaderSource = "varying mediump vec4 v_out;\n"
                                                                  "void main ()\n"
                                                                  "{\n"
                                                                  "    gl_FragColor = v_out;\n"
                                                                  "}\n";
static const char *const s_attrPassthroughVertexShaderSource    = "attribute highp vec4 a_pos;\n"
                                                                  "attribute highp vec4 a_attr;\n"
                                                                  "varying mediump vec4 v_attr;\n"
                                                                  "void main ()\n"
                                                                  "{\n"
                                                                  "    v_attr = a_attr;\n"
                                                                  "    gl_Position = a_pos;\n"
                                                                  "}\n";

class RenderCase : public TestCase
{
public:
    enum RenderTargetType
    {
        RENDERTARGETTYPE_SCREEN,
        RENDERTARGETTYPE_FBO
    };

    RenderCase(Context &context, const char *name, const char *desc,
               RenderTargetType renderTargetType = RENDERTARGETTYPE_SCREEN);
    virtual ~RenderCase(void);

    virtual void init(void);
    virtual void deinit(void);

protected:
    bool checkResultImage(const tcu::Surface &result);
    bool drawTestPattern(bool useTexture);

    virtual std::string genVertexSource(void) const   = 0;
    virtual std::string genFragmentSource(void) const = 0;

    const glu::ShaderProgram *m_program;
    const RenderTargetType m_renderTargetType;
};

RenderCase::RenderCase(Context &context, const char *name, const char *desc, RenderTargetType renderTargetType)
    : TestCase(context, name, desc)
    , m_program(nullptr)
    , m_renderTargetType(renderTargetType)
{
}

RenderCase::~RenderCase(void)
{
    deinit();
}

void RenderCase::init(void)
{
    const int width  = m_context.getRenderTarget().getWidth();
    const int height = m_context.getRenderTarget().getHeight();

    // check target size
    if (m_renderTargetType == RENDERTARGETTYPE_SCREEN)
    {
        if (width < TEST_CANVAS_SIZE || height < TEST_CANVAS_SIZE)
            throw tcu::NotSupportedError(std::string("Render target size must be at least ") +
                                         de::toString(TEST_CANVAS_SIZE) + "x" + de::toString(TEST_CANVAS_SIZE));
    }
    else if (m_renderTargetType == RENDERTARGETTYPE_FBO)
    {
        GLint maxTexSize = 0;
        m_context.getRenderContext().getFunctions().getIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);

        if (maxTexSize < TEST_CANVAS_SIZE)
            throw tcu::NotSupportedError(std::string("GL_MAX_TEXTURE_SIZE must be at least ") +
                                         de::toString(TEST_CANVAS_SIZE));
    }
    else
        DE_ASSERT(false);

    // gen shader

    m_testCtx.getLog() << tcu::TestLog::Message << "Creating test shader." << tcu::TestLog::EndMessage;

    m_program = new glu::ShaderProgram(m_context.getRenderContext(), glu::ProgramSources()
                                                                         << glu::VertexSource(genVertexSource())
                                                                         << glu::FragmentSource(genFragmentSource()));
    m_testCtx.getLog() << *m_program;

    if (!m_program->isOk())
        throw tcu::TestError("shader compile failed");
}

void RenderCase::deinit(void)
{
    if (m_program)
    {
        delete m_program;
        m_program = nullptr;
    }
}

bool RenderCase::checkResultImage(const tcu::Surface &result)
{
    tcu::Surface errorMask(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
    bool error = false;

    m_testCtx.getLog() << tcu::TestLog::Message << "Verifying output image." << tcu::TestLog::EndMessage;

    for (int y = 0; y < TEST_CANVAS_SIZE; ++y)
        for (int x = 0; x < TEST_CANVAS_SIZE; ++x)
        {
            const tcu::RGBA col = result.getPixel(x, y);

            if (col.getGreen() == 255)
                errorMask.setPixel(x, y, tcu::RGBA::green());
            else
            {
                errorMask.setPixel(x, y, tcu::RGBA::red());
                error = true;
            }
        }

    if (error)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Result image has missing or invalid pixels"
                           << tcu::TestLog::EndMessage;
        m_testCtx.getLog() << tcu::TestLog::ImageSet("Results", "Result verification")
                           << tcu::TestLog::Image("Result", "Result", result)
                           << tcu::TestLog::Image("Error mask", "Error mask", errorMask) << tcu::TestLog::EndImageSet;
    }
    else
    {
        m_testCtx.getLog() << tcu::TestLog::ImageSet("Results", "Result verification")
                           << tcu::TestLog::Image("Result", "Result", result) << tcu::TestLog::EndImageSet;
    }

    return !error;
}

bool RenderCase::drawTestPattern(bool useTexture)
{
    static const tcu::Vec4 fullscreenQuad[4] = {
        tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
        tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
    };
    const char *const vertexSource        = "attribute highp vec4 a_pos;\n"
                                            "varying mediump vec4 v_position;\n"
                                            "void main ()\n"
                                            "{\n"
                                            "    v_position = a_pos;\n"
                                            "    gl_Position = a_pos;\n"
                                            "}\n";
    const char *const fragmentSourceNoTex = "varying mediump vec4 v_position;\n"
                                            "void main ()\n"
                                            "{\n"
                                            "    gl_FragColor = vec4((v_position.x + 1.0) * 0.5, 1.0, 1.0, 1.0);\n"
                                            "}\n";
    const char *const fragmentSourceTex   = "uniform mediump sampler2D u_sampler;\n"
                                            "varying mediump vec4 v_position;\n"
                                            "void main ()\n"
                                            "{\n"
                                            "    gl_FragColor = texture2D(u_sampler, v_position.xy);\n"
                                            "}\n";
    const char *const fragmentSource      = (useTexture) ? (fragmentSourceTex) : (fragmentSourceNoTex);
    const tcu::RGBA formatThreshold       = m_context.getRenderTarget().getPixelFormat().getColorThreshold();

    tcu::Surface resultImage(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
    tcu::Surface errorMask(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
    bool error = false;

    m_testCtx.getLog() << tcu::TestLog::Message << "Drawing a test pattern to detect "
                       << ((useTexture) ? ("texture sampling") : ("")) << " side-effects." << tcu::TestLog::EndMessage;

    // draw pattern
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        const glu::ShaderProgram patternProgram(m_context.getRenderContext(),
                                                glu::ProgramSources() << glu::VertexSource(vertexSource)
                                                                      << glu::FragmentSource(fragmentSource));
        const GLint positionLoc = gl.getAttribLocation(patternProgram.getProgram(), "a_pos");
        GLuint textureID        = 0;

        if (useTexture)
        {
            const int textureSize = 32;
            std::vector<tcu::Vector<uint8_t, 4>> buffer(textureSize * textureSize);

            for (int x = 0; x < textureSize; ++x)
                for (int y = 0; y < textureSize; ++y)
                {
                    // sum of two axis aligned gradients. Each gradient is 127 at the edges and 0 at the center.
                    // pattern is symmetric (x and y) => no discontinuity near boundary => no need to worry of results with LINEAR filtering near boundaries
                    const uint8_t redComponent =
                        (uint8_t)de::clamp(de::abs((float)x / (float)textureSize - 0.5f) * 255.0f +
                                               de::abs((float)y / (float)textureSize - 0.5f) * 255.0f,
                                           0.0f, 255.0f);

                    buffer[x * textureSize + y] = tcu::Vector<uint8_t, 4>(redComponent, 255, 255, 255);
                }

            gl.genTextures(1, &textureID);
            gl.bindTexture(GL_TEXTURE_2D, textureID);
            gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureSize, textureSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                          buffer[0].getPtr());
            gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }

        gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
        gl.clear(GL_COLOR_BUFFER_BIT);
        gl.viewport(0, 0, TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
        gl.useProgram(patternProgram.getProgram());

        if (useTexture)
            gl.uniform1i(gl.getUniformLocation(patternProgram.getProgram(), "u_sampler"), 0);

        gl.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, &fullscreenQuad[0]);

        gl.enableVertexAttribArray(positionLoc);
        gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);
        gl.disableVertexAttribArray(positionLoc);

        gl.useProgram(0);
        gl.finish();
        GLU_EXPECT_NO_ERROR(gl.getError(), "RenderCase::drawTestPattern");

        if (textureID)
            gl.deleteTextures(1, &textureID);

        glu::readPixels(m_context.getRenderContext(), 0, 0, resultImage.getAccess());
    }

    // verify pattern
    for (int y = 0; y < TEST_CANVAS_SIZE; ++y)
        for (int x = 0; x < TEST_CANVAS_SIZE; ++x)
        {
            const float texGradientPosX   = deFloatFrac((float)x * 2.0f / (float)TEST_CANVAS_SIZE);
            const float texGradientPosY   = deFloatFrac((float)y * 2.0f / (float)TEST_CANVAS_SIZE);
            const uint8_t texRedComponent = (uint8_t)de::clamp(
                de::abs(texGradientPosX - 0.5f) * 255.0f + de::abs(texGradientPosY - 0.5f) * 255.0f, 0.0f, 255.0f);

            const tcu::RGBA refColTexture = tcu::RGBA(texRedComponent, 255, 255, 255);
            const tcu::RGBA refColGradient =
                tcu::RGBA((int)((float)x / (float)TEST_CANVAS_SIZE * 255.0f), 255, 255, 255);
            const tcu::RGBA &refCol = (useTexture) ? (refColTexture) : (refColGradient);

            const int colorThreshold   = 10;
            const tcu::RGBA col        = resultImage.getPixel(x, y);
            const tcu::IVec4 colorDiff = tcu::abs(col.toIVec() - refCol.toIVec());

            if (colorDiff.x() > formatThreshold.getRed() + colorThreshold ||
                colorDiff.y() > formatThreshold.getGreen() + colorThreshold ||
                colorDiff.z() > formatThreshold.getBlue() + colorThreshold)
            {
                errorMask.setPixel(x, y, tcu::RGBA::red());
                error = true;
            }
            else
                errorMask.setPixel(x, y, tcu::RGBA::green());
        }

    // report error
    if (error)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Test pattern has missing/invalid pixels"
                           << tcu::TestLog::EndMessage;
        m_testCtx.getLog() << tcu::TestLog::ImageSet("Results", "Result verification")
                           << tcu::TestLog::Image("Result", "Result", resultImage)
                           << tcu::TestLog::Image("Error mask", "Error mask", errorMask) << tcu::TestLog::EndImageSet;
    }
    else
        m_testCtx.getLog() << tcu::TestLog::Message << "No side-effects found." << tcu::TestLog::EndMessage;

    return !error;
}

class FramebufferRenderCase : public RenderCase
{
public:
    enum FrameBufferType
    {
        FBO_DEFAULT = 0,
        FBO_RGBA,
        FBO_RGBA4,
        FBO_RGB5_A1,
        FBO_RGB565,
        FBO_RGBA_FLOAT16,

        FBO_LAST
    };

    FramebufferRenderCase(Context &context, const char *name, const char *desc, FrameBufferType fboType);
    virtual ~FramebufferRenderCase(void);

    virtual void init(void);
    virtual void deinit(void);
    IterateResult iterate(void);

    virtual void testFBO(void) = 0;

protected:
    const FrameBufferType m_fboType;

private:
    GLuint m_texID;
    GLuint m_fboID;
};

FramebufferRenderCase::FramebufferRenderCase(Context &context, const char *name, const char *desc,
                                             FrameBufferType fboType)
    : RenderCase(context, name, desc, (fboType == FBO_DEFAULT) ? (RENDERTARGETTYPE_SCREEN) : (RENDERTARGETTYPE_FBO))
    , m_fboType(fboType)
    , m_texID(0)
    , m_fboID(0)
{
    DE_ASSERT(m_fboType < FBO_LAST);
}

FramebufferRenderCase::~FramebufferRenderCase(void)
{
    deinit();
}

void FramebufferRenderCase::init(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    // check requirements
    if (m_fboType == FBO_RGBA_FLOAT16)
    {
        // half float texture is allowed (OES_texture_half_float) and it is color renderable (EXT_color_buffer_half_float)
        if (!m_context.getContextInfo().isExtensionSupported("GL_OES_texture_half_float") ||
            !m_context.getContextInfo().isExtensionSupported("GL_EXT_color_buffer_half_float"))
            throw tcu::NotSupportedError("Color renderable half float texture required.");
    }

    // gen shader
    RenderCase::init();

    // create render target
    if (m_fboType == FBO_DEFAULT)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Using default framebuffer." << tcu::TestLog::EndMessage;
    }
    else
    {
        GLuint internalFormat = 0;
        GLuint format         = 0;
        GLuint type           = 0;

#if !defined(GL_HALF_FLOAT_OES)
#define GL_HALF_FLOAT_OES 0x8D61
#endif

        switch (m_fboType)
        {
        case FBO_RGBA:
            internalFormat = GL_RGBA;
            format         = GL_RGBA;
            type           = GL_UNSIGNED_BYTE;
            break;
        case FBO_RGBA4:
            internalFormat = GL_RGBA;
            format         = GL_RGBA;
            type           = GL_UNSIGNED_SHORT_4_4_4_4;
            break;
        case FBO_RGB5_A1:
            internalFormat = GL_RGBA;
            format         = GL_RGBA;
            type           = GL_UNSIGNED_SHORT_5_5_5_1;
            break;
        case FBO_RGB565:
            internalFormat = GL_RGB;
            format         = GL_RGB;
            type           = GL_UNSIGNED_SHORT_5_6_5;
            break;
        case FBO_RGBA_FLOAT16:
            internalFormat = GL_RGBA;
            format         = GL_RGBA;
            type           = GL_HALF_FLOAT_OES;
            break;

        default:
            DE_ASSERT(false);
            break;
        }

        m_testCtx.getLog() << tcu::TestLog::Message
                           << "Creating fbo. Texture internalFormat = " << glu::getTextureFormatStr(internalFormat)
                           << ", format = " << glu::getTextureFormatStr(format) << ", type = " << glu::getTypeStr(type)
                           << tcu::TestLog::EndMessage;

        // gen texture
        gl.genTextures(1, &m_texID);
        gl.bindTexture(GL_TEXTURE_2D, m_texID);
        gl.texImage2D(GL_TEXTURE_2D, 0, internalFormat, TEST_CANVAS_SIZE, TEST_CANVAS_SIZE, 0, format, type, nullptr);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texture init");

        // gen fbo
        gl.genFramebuffers(1, &m_fboID);
        gl.bindFramebuffer(GL_FRAMEBUFFER, m_fboID);
        gl.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texID, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "fbo init");

        if (gl.checkFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            throw tcu::NotSupportedError("could not create fbo for testing.");
    }
}

void FramebufferRenderCase::deinit(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    if (m_texID)
    {
        gl.deleteTextures(1, &m_texID);
        m_texID = 0;
    }

    if (m_fboID)
    {
        gl.deleteFramebuffers(1, &m_fboID);
        m_fboID = 0;
    }
}

FramebufferRenderCase::IterateResult FramebufferRenderCase::iterate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    // bind fbo (or don't if we are using default)
    if (m_fboID)
        gl.bindFramebuffer(GL_FRAMEBUFFER, m_fboID);

    // do something with special floats
    testFBO();

    return STOP;
}

/*--------------------------------------------------------------------*//*!
 * \brief Tests special floats as vertex attributes
 *
 * Tests that special floats transferred to the shader using vertex
 * attributes do not change the results of normal floating point
 * calculations. Special floats are put to 4-vector's x and y components and
 * value 1.0 is put to z and w. The resulting fragment's green channel
 * should be 1.0 everywhere.
 *
 * After the calculation test a test pattern is drawn to detect possible
 * floating point operation anomalies.
 *//*--------------------------------------------------------------------*/
class VertexAttributeCase : public RenderCase
{
public:
    enum Storage
    {
        STORAGE_BUFFER = 0,
        STORAGE_CLIENT,

        STORAGE_LAST
    };
    enum ShaderType
    {
        TYPE_VERTEX = 0,
        TYPE_FRAGMENT,

        TYPE_LAST
    };

    VertexAttributeCase(Context &context, const char *name, const char *desc, Storage storage, ShaderType type);
    ~VertexAttributeCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    std::string genVertexSource(void) const;
    std::string genFragmentSource(void) const;

    const Storage m_storage;
    const ShaderType m_type;
    GLuint m_positionVboID;
    GLuint m_attribVboID;
    GLuint m_elementVboID;
};

VertexAttributeCase::VertexAttributeCase(Context &context, const char *name, const char *desc, Storage storage,
                                         ShaderType type)
    : RenderCase(context, name, desc)
    , m_storage(storage)
    , m_type(type)
    , m_positionVboID(0)
    , m_attribVboID(0)
    , m_elementVboID(0)
{
    DE_ASSERT(storage < STORAGE_LAST);
    DE_ASSERT(type < TYPE_LAST);
}

VertexAttributeCase::~VertexAttributeCase(void)
{
    deinit();
}

void VertexAttributeCase::init(void)
{
    RenderCase::init();

    // init gl resources
    if (m_storage == STORAGE_BUFFER)
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();

        gl.genBuffers(1, &m_positionVboID);
        gl.genBuffers(1, &m_attribVboID);
        gl.genBuffers(1, &m_elementVboID);
    }
}

void VertexAttributeCase::deinit(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    RenderCase::deinit();

    if (m_attribVboID)
    {
        gl.deleteBuffers(1, &m_attribVboID);
        m_attribVboID = 0;
    }

    if (m_positionVboID)
    {
        gl.deleteBuffers(1, &m_positionVboID);
        m_positionVboID = 0;
    }

    if (m_elementVboID)
    {
        gl.deleteBuffers(1, &m_elementVboID);
        m_elementVboID = 0;
    }
}

VertexAttributeCase::IterateResult VertexAttributeCase::iterate(void)
{
    // Create a [s_specialFloats] X [s_specialFloats] grid of vertices with each vertex having 2 [s_specialFloats] values
    // and calculate some basic operations with the floating point values. If all goes well, nothing special should happen

    std::vector<tcu::Vec4> gridVertices(DE_LENGTH_OF_ARRAY(s_specialFloats) * DE_LENGTH_OF_ARRAY(s_specialFloats));
    std::vector<tcu::UVec4> gridAttributes(DE_LENGTH_OF_ARRAY(s_specialFloats) * DE_LENGTH_OF_ARRAY(s_specialFloats));
    std::vector<uint16_t> indices((DE_LENGTH_OF_ARRAY(s_specialFloats) - 1) *
                                  (DE_LENGTH_OF_ARRAY(s_specialFloats) - 1) * 6);
    tcu::Surface resultImage(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);

    // vertices
    for (int x = 0; x < DE_LENGTH_OF_ARRAY(s_specialFloats); ++x)
        for (int y = 0; y < DE_LENGTH_OF_ARRAY(s_specialFloats); ++y)
        {
            const uint32_t one = 0x3F800000;
            const float posX   = (float)x / ((float)DE_LENGTH_OF_ARRAY(s_specialFloats) - 1.0f) * 2.0f -
                               1.0f; // map from [0, len(s_specialFloats) - 1] to [-1, 1]
            const float posY = (float)y / ((float)DE_LENGTH_OF_ARRAY(s_specialFloats) - 1.0f) * 2.0f - 1.0f;

            gridVertices[x * DE_LENGTH_OF_ARRAY(s_specialFloats) + y] = tcu::Vec4(posX, posY, 0.0f, 1.0f);
            gridAttributes[x * DE_LENGTH_OF_ARRAY(s_specialFloats) + y] =
                tcu::UVec4(s_specialFloats[x], s_specialFloats[y], one, one);
        }

    // tiles
    for (int x = 0; x < DE_LENGTH_OF_ARRAY(s_specialFloats) - 1; ++x)
        for (int y = 0; y < DE_LENGTH_OF_ARRAY(s_specialFloats) - 1; ++y)
        {
            const int baseNdx = (x * (DE_LENGTH_OF_ARRAY(s_specialFloats) - 1) + y) * 6;

            indices[baseNdx + 0] = (uint16_t)((x + 0) * DE_LENGTH_OF_ARRAY(s_specialFloats) + (y + 0));
            indices[baseNdx + 1] = (uint16_t)((x + 1) * DE_LENGTH_OF_ARRAY(s_specialFloats) + (y + 1));
            indices[baseNdx + 2] = (uint16_t)((x + 1) * DE_LENGTH_OF_ARRAY(s_specialFloats) + (y + 0));

            indices[baseNdx + 3] = (uint16_t)((x + 0) * DE_LENGTH_OF_ARRAY(s_specialFloats) + (y + 0));
            indices[baseNdx + 4] = (uint16_t)((x + 1) * DE_LENGTH_OF_ARRAY(s_specialFloats) + (y + 1));
            indices[baseNdx + 5] = (uint16_t)((x + 0) * DE_LENGTH_OF_ARRAY(s_specialFloats) + (y + 1));
        }

    m_testCtx.getLog() << tcu::TestLog::Message
                       << "Drawing a grid with the shader. Setting a_attr for each vertex to (special, special, 1, 1)."
                       << tcu::TestLog::EndMessage;

    // Draw grid
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        const GLint positionLoc  = gl.getAttribLocation(m_program->getProgram(), "a_pos");
        const GLint attribLoc    = gl.getAttribLocation(m_program->getProgram(), "a_attr");

        if (m_storage == STORAGE_BUFFER)
        {
            gl.bindBuffer(GL_ARRAY_BUFFER, m_positionVboID);
            gl.bufferData(GL_ARRAY_BUFFER, (glw::GLsizeiptr)(gridVertices.size() * sizeof(tcu::Vec4)), &gridVertices[0],
                          GL_STATIC_DRAW);
            GLU_EXPECT_NO_ERROR(gl.getError(), "VertexAttributeCase::iterate");

            gl.bindBuffer(GL_ARRAY_BUFFER, m_attribVboID);
            gl.bufferData(GL_ARRAY_BUFFER, (glw::GLsizeiptr)(gridAttributes.size() * sizeof(tcu::UVec4)),
                          &gridAttributes[0], GL_STATIC_DRAW);
            GLU_EXPECT_NO_ERROR(gl.getError(), "VertexAttributeCase::iterate");

            gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_elementVboID);
            gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, (glw::GLsizeiptr)(indices.size() * sizeof(uint16_t)), &indices[0],
                          GL_STATIC_DRAW);
            GLU_EXPECT_NO_ERROR(gl.getError(), "VertexAttributeCase::iterate");
        }

        gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
        gl.clear(GL_COLOR_BUFFER_BIT);
        gl.viewport(0, 0, TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
        gl.useProgram(m_program->getProgram());

        if (m_storage == STORAGE_BUFFER)
        {
            gl.bindBuffer(GL_ARRAY_BUFFER, m_positionVboID);
            gl.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

            gl.bindBuffer(GL_ARRAY_BUFFER, m_attribVboID);
            gl.vertexAttribPointer(attribLoc, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

            gl.enableVertexAttribArray(positionLoc);
            gl.enableVertexAttribArray(attribLoc);
            gl.drawElements(GL_TRIANGLES, (glw::GLsizei)(indices.size()), GL_UNSIGNED_SHORT, nullptr);
            gl.disableVertexAttribArray(positionLoc);
            gl.disableVertexAttribArray(attribLoc);

            gl.bindBuffer(GL_ARRAY_BUFFER, 0);
            gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        }
        else if (m_storage == STORAGE_CLIENT)
        {
            gl.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, &gridVertices[0]);
            gl.vertexAttribPointer(attribLoc, 4, GL_FLOAT, GL_FALSE, 0, &gridAttributes[0]);

            gl.enableVertexAttribArray(positionLoc);
            gl.enableVertexAttribArray(attribLoc);
            gl.drawElements(GL_TRIANGLES, (glw::GLsizei)(indices.size()), GL_UNSIGNED_SHORT, &indices[0]);
            gl.disableVertexAttribArray(positionLoc);
            gl.disableVertexAttribArray(attribLoc);
        }
        else
            DE_ASSERT(false);

        gl.useProgram(0);
        gl.finish();
        GLU_EXPECT_NO_ERROR(gl.getError(), "VertexAttributeCase::iterate");

        glu::readPixels(m_context.getRenderContext(), 0, 0, resultImage.getAccess());
    }

    // verify everywhere was drawn (all pixels have Green = 255)
    if (!checkResultImage(resultImage))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "missing or invalid fragments");
        return STOP;
    }

    // test drawing still works
    if (!drawTestPattern(false))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "test pattern failed");
        return STOP;
    }

    // all ok
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    return STOP;
}

std::string VertexAttributeCase::genVertexSource(void) const
{
    if (m_type == TYPE_VERTEX)
        return "attribute highp vec4 a_pos;\n"
               "attribute highp vec4 a_attr;\n"
               "varying mediump vec4 v_out;\n"
               "void main ()\n"
               "{\n"
               "    highp vec2 a1 = a_attr.xz + a_attr.yw; // add\n"
               "    highp vec2 a2 = a_attr.xz - a_attr.yw; // sub\n"
               "    highp vec2 a3 = a_attr.xz * a_attr.yw; // mul\n"
               "    highp vec2 a4 = a_attr.xz / a_attr.yw; // div\n"
               "    highp vec2 a5 = a_attr.xz + a_attr.yw * a_attr.xz; // fma\n"
               "\n"
               "    highp float green = 1.0 - abs((a1.y + a2.y + a3.y + a4.y + a5.y) - 6.0);\n"
               "    v_out = vec4(a1.x*a3.x + a2.x*a4.x, green, a5.x, 1.0);\n"
               "    gl_Position = a_pos;\n"
               "}\n";
    else
        return s_attrPassthroughVertexShaderSource;
}

std::string VertexAttributeCase::genFragmentSource(void) const
{
    if (m_type == TYPE_VERTEX)
        return s_colorPassthroughFragmentShaderSource;
    else
        return "varying mediump vec4 v_attr;\n"
               "void main ()\n"
               "{\n"
               "    mediump vec2 a1 = v_attr.xz + v_attr.yw; // add\n"
               "    mediump vec2 a2 = v_attr.xz - v_attr.yw; // sub\n"
               "    mediump vec2 a3 = v_attr.xz * v_attr.yw; // mul\n"
               "    mediump vec2 a4 = v_attr.xz / v_attr.yw; // div\n"
               "    mediump vec2 a5 = v_attr.xz + v_attr.yw * v_attr.xz; // fma\n"
               "\n"
               "    const mediump float epsilon = 0.1; // allow small differences. To results to be wrong they must be "
               "more wrong than that.\n"
               "    mediump float green = 1.0 + epsilon - abs((a1.y + a2.y + a3.y + a4.y + a5.y) - 6.0);\n"
               "    gl_FragColor = vec4(a1.x*a3.x + a2.x*a4.x, green, a5.x, 1.0);\n"
               "}\n";
}

/*--------------------------------------------------------------------*//*!
 * \brief Tests special floats as uniforms
 *
 * Tests that special floats transferred to the shader as uniforms do
 * not change the results of normal floating point calculations. Special
 * floats are put to 4-vector's x and y components and value 1.0 is put to
 * z and w. The resulting fragment's green channel should be 1.0
 * everywhere.
 *
 * After the calculation test a test pattern is drawn to detect possible
 * floating point operation anomalies.
 *//*--------------------------------------------------------------------*/
class UniformCase : public RenderCase
{
public:
    enum ShaderType
    {
        TYPE_VERTEX = 0,
        TYPE_FRAGMENT,
    };

    UniformCase(Context &context, const char *name, const char *desc, ShaderType type);
    ~UniformCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    std::string genVertexSource(void) const;
    std::string genFragmentSource(void) const;

    const ShaderType m_type;
};

UniformCase::UniformCase(Context &context, const char *name, const char *desc, ShaderType type)
    : RenderCase(context, name, desc)
    , m_type(type)
{
}

UniformCase::~UniformCase(void)
{
    deinit();
}

void UniformCase::init(void)
{
    RenderCase::init();
}

void UniformCase::deinit(void)
{
    RenderCase::deinit();
}

UniformCase::IterateResult UniformCase::iterate(void)
{
    // Create a [s_specialFloats] X [s_specialFloats] grid of tile with each tile having 2 [s_specialFloats] values
    // and calculate some basic operations with the floating point values. If all goes well, nothing special should happen

    std::vector<tcu::Vec4> gridVertices((DE_LENGTH_OF_ARRAY(s_specialFloats) + 1) *
                                        (DE_LENGTH_OF_ARRAY(s_specialFloats) + 1));
    std::vector<uint16_t> indices(DE_LENGTH_OF_ARRAY(s_specialFloats) * DE_LENGTH_OF_ARRAY(s_specialFloats) * 6);
    tcu::Surface resultImage(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);

    // vertices
    for (int x = 0; x < DE_LENGTH_OF_ARRAY(s_specialFloats) + 1; ++x)
        for (int y = 0; y < DE_LENGTH_OF_ARRAY(s_specialFloats) + 1; ++y)
        {
            const float posX = (float)x / (float)DE_LENGTH_OF_ARRAY(s_specialFloats) * 2.0f -
                               1.0f; // map from [0, len(s_specialFloats) ] to [-1, 1]
            const float posY = (float)y / (float)DE_LENGTH_OF_ARRAY(s_specialFloats) * 2.0f - 1.0f;

            gridVertices[x * (DE_LENGTH_OF_ARRAY(s_specialFloats) + 1) + y] = tcu::Vec4(posX, posY, 0.0f, 1.0f);
        }

    // tiles
    for (int x = 0; x < DE_LENGTH_OF_ARRAY(s_specialFloats); ++x)
        for (int y = 0; y < DE_LENGTH_OF_ARRAY(s_specialFloats); ++y)
        {
            const int baseNdx = (x * (DE_LENGTH_OF_ARRAY(s_specialFloats)) + y) * 6;

            indices[baseNdx + 0] = (uint16_t)((x + 0) * (DE_LENGTH_OF_ARRAY(s_specialFloats) + 1) + (y + 0));
            indices[baseNdx + 1] = (uint16_t)((x + 1) * (DE_LENGTH_OF_ARRAY(s_specialFloats) + 1) + (y + 1));
            indices[baseNdx + 2] = (uint16_t)((x + 1) * (DE_LENGTH_OF_ARRAY(s_specialFloats) + 1) + (y + 0));

            indices[baseNdx + 3] = (uint16_t)((x + 0) * (DE_LENGTH_OF_ARRAY(s_specialFloats) + 1) + (y + 0));
            indices[baseNdx + 4] = (uint16_t)((x + 1) * (DE_LENGTH_OF_ARRAY(s_specialFloats) + 1) + (y + 1));
            indices[baseNdx + 5] = (uint16_t)((x + 0) * (DE_LENGTH_OF_ARRAY(s_specialFloats) + 1) + (y + 1));
        }

    m_testCtx.getLog()
        << tcu::TestLog::Message
        << "Drawing a grid with the shader. Setting u_special for vertex each tile to (special, special, 1, 1)."
        << tcu::TestLog::EndMessage;

    // Draw grid
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        const GLint positionLoc  = gl.getAttribLocation(m_program->getProgram(), "a_pos");
        const GLint specialLoc   = gl.getUniformLocation(m_program->getProgram(), "u_special");

        gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
        gl.clear(GL_COLOR_BUFFER_BIT);
        gl.viewport(0, 0, TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
        gl.useProgram(m_program->getProgram());

        gl.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, &gridVertices[0]);
        gl.enableVertexAttribArray(positionLoc);

        for (int x = 0; x < DE_LENGTH_OF_ARRAY(s_specialFloats); ++x)
            for (int y = 0; y < DE_LENGTH_OF_ARRAY(s_specialFloats); ++y)
            {
                const uint32_t one            = 0x3F800000;
                const tcu::UVec4 uniformValue = tcu::UVec4(s_specialFloats[x], s_specialFloats[y], one, one);
                const int indexIndex          = (x * DE_LENGTH_OF_ARRAY(s_specialFloats) + y) * 6;

                gl.uniform4fv(specialLoc, 1, (const float *)uniformValue.getPtr());
                gl.drawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, &indices[indexIndex]);
            }

        gl.disableVertexAttribArray(positionLoc);

        gl.useProgram(0);
        gl.finish();
        GLU_EXPECT_NO_ERROR(gl.getError(), "UniformCase::iterate");

        glu::readPixels(m_context.getRenderContext(), 0, 0, resultImage.getAccess());
    }

    // verify everywhere was drawn (all pixels have Green = 255)
    if (!checkResultImage(resultImage))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "missing or invalid fragments");
        return STOP;
    }

    // test drawing still works
    if (!drawTestPattern(false))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "test pattern failed");
        return STOP;
    }

    // all ok
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    return STOP;
}

std::string UniformCase::genVertexSource(void) const
{
    if (m_type == TYPE_VERTEX)
        return "attribute highp vec4 a_pos;\n"
               "uniform highp vec4 u_special;\n"
               "varying mediump vec4 v_out;\n"
               "void main ()\n"
               "{\n"
               "    highp vec2 a1 = u_special.xz + u_special.yw; // add\n"
               "    highp vec2 a2 = u_special.xz - u_special.yw; // sub\n"
               "    highp vec2 a3 = u_special.xz * u_special.yw; // mul\n"
               "    highp vec2 a4 = u_special.xz / u_special.yw; // div\n"
               "    highp vec2 a5 = u_special.xz + u_special.yw * u_special.xz; // fma\n"
               "\n"
               "    highp float green = 1.0 - abs((a1.y + a2.y + a3.y + a4.y + a5.y) - 6.0);\n"
               "    v_out = vec4(a1.x*a3.x + a2.x*a4.x, green, a5.x, 1.0);\n"
               "    gl_Position = a_pos;\n"
               "}\n";
    else
        return "attribute highp vec4 a_pos;\n"
               "void main ()\n"
               "{\n"
               "    gl_Position = a_pos;\n"
               "}\n";
}

std::string UniformCase::genFragmentSource(void) const
{
    if (m_type == TYPE_VERTEX)
        return s_colorPassthroughFragmentShaderSource;
    else
        return "uniform mediump vec4 u_special;\n"
               "void main ()\n"
               "{\n"
               "    mediump vec2 a1 = u_special.xz + u_special.yw; // add\n"
               "    mediump vec2 a2 = u_special.xz - u_special.yw; // sub\n"
               "    mediump vec2 a3 = u_special.xz * u_special.yw; // mul\n"
               "    mediump vec2 a4 = u_special.xz / u_special.yw; // div\n"
               "    mediump vec2 a5 = u_special.xz + u_special.yw * u_special.xz; // fma\n"
               "    mediump vec2 a6 = mod(u_special.xz, u_special.yw);\n"
               "    mediump vec2 a7 = mix(u_special.xz, u_special.yw, a6);\n"
               "\n"
               "    mediump float green = 1.0 - abs((a1.y + a2.y + a3.y + a4.y + a5.y + a6.y + a7.y) - 7.0);\n"
               "    gl_FragColor = vec4(a1.x*a3.x, green, a5.x*a4.x + a2.x*a7.x, 1.0);\n"
               "}\n";
}

/*--------------------------------------------------------------------*//*!
 * \brief Tests special floats as texture samping arguments
 *
 * Tests that special floats given as texture coordinates or LOD levels
 * to sampling functions do not return invalid values (values not in the
 * texture). Every texel's green component is 1.0.
 *
 * After the calculation test a test pattern is drawn to detect possible
 * texture sampling anomalies.
 *//*--------------------------------------------------------------------*/
class TextureSamplerCase : public RenderCase
{
public:
    enum ShaderType
    {
        TYPE_VERTEX = 0,
        TYPE_FRAGMENT,

        TYPE_LAST
    };
    enum TestType
    {
        TEST_TEX_COORD = 0,
        TEST_LOD,
        TEST_TEX_COORD_CUBE,

        TEST_LAST
    };

    TextureSamplerCase(Context &context, const char *name, const char *desc, ShaderType type, TestType testType);
    ~TextureSamplerCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    std::string genVertexSource(void) const;
    std::string genFragmentSource(void) const;

    const ShaderType m_type;
    const TestType m_testType;
    GLuint m_textureID;
};

TextureSamplerCase::TextureSamplerCase(Context &context, const char *name, const char *desc, ShaderType type,
                                       TestType testType)
    : RenderCase(context, name, desc)
    , m_type(type)
    , m_testType(testType)
    , m_textureID(0)
{
    DE_ASSERT(type < TYPE_LAST);
    DE_ASSERT(testType < TEST_LAST);
}

TextureSamplerCase::~TextureSamplerCase(void)
{
    deinit();
}

void TextureSamplerCase::init(void)
{
    // requirements
    {
        GLint maxTextureSize = 0;
        m_context.getRenderContext().getFunctions().getIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
        if (maxTextureSize < TEST_TEXTURE_SIZE)
            throw tcu::NotSupportedError(std::string("GL_MAX_TEXTURE_SIZE must be at least ") +
                                         de::toString(TEST_TEXTURE_SIZE));
    }

    // vertex shader supports textures?
    if (m_type == TYPE_VERTEX)
    {
        GLint maxVertexTexUnits = 0;
        m_context.getRenderContext().getFunctions().getIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &maxVertexTexUnits);
        if (maxVertexTexUnits < 1)
            throw tcu::NotSupportedError("GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS must be at least 1");
    }

    RenderCase::init();

    // gen texture
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        std::vector<uint8_t> texData(TEST_TEXTURE_SIZE * TEST_TEXTURE_SIZE * 4);
        de::Random rnd(12345);

        gl.genTextures(1, &m_textureID);

        for (int x = 0; x < TEST_TEXTURE_SIZE; ++x)
            for (int y = 0; y < TEST_TEXTURE_SIZE; ++y)
            {
                // RGBA8, green and alpha channel are always 255 for verification
                texData[(x * TEST_TEXTURE_SIZE + y) * 4 + 0] = rnd.getUint32() & 0xFF;
                texData[(x * TEST_TEXTURE_SIZE + y) * 4 + 1] = 0xFF;
                texData[(x * TEST_TEXTURE_SIZE + y) * 4 + 2] = rnd.getUint32() & 0xFF;
                texData[(x * TEST_TEXTURE_SIZE + y) * 4 + 3] = 0xFF;
            }

        if (m_testType == TEST_TEX_COORD)
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "Creating a 2D texture with a test pattern."
                               << tcu::TestLog::EndMessage;

            gl.bindTexture(GL_TEXTURE_2D, m_textureID);
            gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEST_TEXTURE_SIZE, TEST_TEXTURE_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                          &texData[0]);
            gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            GLU_EXPECT_NO_ERROR(gl.getError(), "TextureSamplerCase::init");
        }
        else if (m_testType == TEST_LOD)
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "Creating a mipmapped 2D texture with a test pattern."
                               << tcu::TestLog::EndMessage;

            gl.bindTexture(GL_TEXTURE_2D, m_textureID);

            for (int level = 0; (TEST_TEXTURE_SIZE >> level); ++level)
                gl.texImage2D(GL_TEXTURE_2D, level, GL_RGBA, TEST_TEXTURE_SIZE >> level, TEST_TEXTURE_SIZE >> level, 0,
                              GL_RGBA, GL_UNSIGNED_BYTE, &texData[0]);

            gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            GLU_EXPECT_NO_ERROR(gl.getError(), "TextureSamplerCase::init");
        }
        else if (m_testType == TEST_TEX_COORD_CUBE)
        {
            DE_STATIC_ASSERT(TEST_TEXTURE_CUBE_SIZE <= TEST_TEXTURE_SIZE);

            static const GLenum faces[] = {
                GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
                GL_TEXTURE_CUBE_MAP_NEGATIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
            };

            m_testCtx.getLog() << tcu::TestLog::Message << "Creating a cube map with a test pattern."
                               << tcu::TestLog::EndMessage;

            gl.bindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);

            for (int faceNdx = 0; faceNdx < DE_LENGTH_OF_ARRAY(faces); ++faceNdx)
                gl.texImage2D(faces[faceNdx], 0, GL_RGBA, TEST_TEXTURE_CUBE_SIZE, TEST_TEXTURE_CUBE_SIZE, 0, GL_RGBA,
                              GL_UNSIGNED_BYTE, &texData[0]);

            gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            GLU_EXPECT_NO_ERROR(gl.getError(), "TextureSamplerCase::init");
        }
        else
            DE_ASSERT(false);
    }
}

void TextureSamplerCase::deinit(void)
{
    RenderCase::deinit();

    if (m_textureID)
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();

        gl.deleteTextures(1, &m_textureID);
        m_textureID = 0;
    }
}

TextureSamplerCase::IterateResult TextureSamplerCase::iterate(void)
{
    // Draw a grid and texture it with a texture and sample it using special special values. The result samples should all have the green channel at 255 as per the test image.

    std::vector<tcu::Vec4> gridVertices(DE_LENGTH_OF_ARRAY(s_specialFloats) * DE_LENGTH_OF_ARRAY(s_specialFloats));
    std::vector<tcu::UVec2> gridTexCoords(DE_LENGTH_OF_ARRAY(s_specialFloats) * DE_LENGTH_OF_ARRAY(s_specialFloats));
    std::vector<uint16_t> indices((DE_LENGTH_OF_ARRAY(s_specialFloats) - 1) *
                                  (DE_LENGTH_OF_ARRAY(s_specialFloats) - 1) * 6);
    tcu::Surface resultImage(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);

    // vertices
    for (int x = 0; x < DE_LENGTH_OF_ARRAY(s_specialFloats); ++x)
        for (int y = 0; y < DE_LENGTH_OF_ARRAY(s_specialFloats); ++y)
        {
            const float posX = (float)x / ((float)DE_LENGTH_OF_ARRAY(s_specialFloats) - 1.0f) * 2.0f -
                               1.0f; // map from [0, len(s_specialFloats) - 1] to [-1, 1]
            const float posY = (float)y / ((float)DE_LENGTH_OF_ARRAY(s_specialFloats) - 1.0f) * 2.0f - 1.0f;

            gridVertices[x * DE_LENGTH_OF_ARRAY(s_specialFloats) + y] = tcu::Vec4(posX, posY, 0.0f, 1.0f);
            gridTexCoords[x * DE_LENGTH_OF_ARRAY(s_specialFloats) + y] =
                tcu::UVec2(s_specialFloats[x], s_specialFloats[y]);
        }

    // tiles
    for (int x = 0; x < DE_LENGTH_OF_ARRAY(s_specialFloats) - 1; ++x)
        for (int y = 0; y < DE_LENGTH_OF_ARRAY(s_specialFloats) - 1; ++y)
        {
            const int baseNdx = (x * (DE_LENGTH_OF_ARRAY(s_specialFloats) - 1) + y) * 6;

            indices[baseNdx + 0] = (uint16_t)((x + 0) * DE_LENGTH_OF_ARRAY(s_specialFloats) + (y + 0));
            indices[baseNdx + 1] = (uint16_t)((x + 1) * DE_LENGTH_OF_ARRAY(s_specialFloats) + (y + 1));
            indices[baseNdx + 2] = (uint16_t)((x + 1) * DE_LENGTH_OF_ARRAY(s_specialFloats) + (y + 0));

            indices[baseNdx + 3] = (uint16_t)((x + 0) * DE_LENGTH_OF_ARRAY(s_specialFloats) + (y + 0));
            indices[baseNdx + 4] = (uint16_t)((x + 1) * DE_LENGTH_OF_ARRAY(s_specialFloats) + (y + 1));
            indices[baseNdx + 5] = (uint16_t)((x + 0) * DE_LENGTH_OF_ARRAY(s_specialFloats) + (y + 1));
        }

    m_testCtx.getLog()
        << tcu::TestLog::Message
        << "Drawing a textured grid with the shader. Sampling from the texture using special floating point values."
        << tcu::TestLog::EndMessage;

    // Draw grid
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        const GLint positionLoc  = gl.getAttribLocation(m_program->getProgram(), "a_pos");
        const GLint texCoordLoc  = gl.getAttribLocation(m_program->getProgram(), "a_attr");
        const GLint samplerLoc   = gl.getUniformLocation(m_program->getProgram(), "u_sampler");

        gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
        gl.clear(GL_COLOR_BUFFER_BIT);
        gl.viewport(0, 0, TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
        gl.useProgram(m_program->getProgram());

        gl.uniform1i(samplerLoc, 0);
        if (m_testType != TEST_TEX_COORD_CUBE)
            gl.bindTexture(GL_TEXTURE_2D, m_textureID);
        else
            gl.bindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);

        gl.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, &gridVertices[0]);
        gl.vertexAttribPointer(texCoordLoc, 2, GL_FLOAT, GL_FALSE, 0, &gridTexCoords[0]);

        gl.enableVertexAttribArray(positionLoc);
        gl.enableVertexAttribArray(texCoordLoc);
        gl.drawElements(GL_TRIANGLES, (glw::GLsizei)(indices.size()), GL_UNSIGNED_SHORT, &indices[0]);
        gl.disableVertexAttribArray(positionLoc);
        gl.disableVertexAttribArray(texCoordLoc);

        gl.useProgram(0);
        gl.finish();
        GLU_EXPECT_NO_ERROR(gl.getError(), "TextureSamplerCase::iterate");

        glu::readPixels(m_context.getRenderContext(), 0, 0, resultImage.getAccess());
    }

    // verify everywhere was drawn and samples were from the texture (all pixels have Green = 255)
    if (!checkResultImage(resultImage))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "missing or invalid fragments");
        return STOP;
    }

    // test drawing and textures still works
    if (!drawTestPattern(true))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "test pattern failed");
        return STOP;
    }

    // all ok
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    return STOP;
}

std::string TextureSamplerCase::genVertexSource(void) const
{
    // vertex shader is passthrough, fragment does the calculations
    if (m_type == TYPE_FRAGMENT)
        return s_attrPassthroughVertexShaderSource;

    // vertex shader does the calculations
    std::ostringstream buf;
    buf << "attribute highp vec4 a_pos;\n"
           "attribute highp vec2 a_attr;\n";

    if (m_testType != TEST_TEX_COORD_CUBE)
        buf << "uniform highp sampler2D u_sampler;\n";
    else
        buf << "uniform highp samplerCube u_sampler;\n";

    buf << "varying mediump vec4 v_out;\n"
           "void main ()\n"
           "{\n";

    if (m_testType == TEST_TEX_COORD)
        buf << "    v_out = texture2DLod(u_sampler, a_attr, 0.0);\n";
    else if (m_testType == TEST_LOD)
        buf << "    v_out = texture2DLod(u_sampler, a_attr, a_attr.x);\n";
    else if (m_testType == TEST_TEX_COORD_CUBE)
        buf << "    v_out = textureCubeLod(u_sampler, vec3(a_attr, a_attr.x+a_attr.y), 0.0);\n";
    else
        DE_ASSERT(false);

    buf << "\n"
           "    gl_Position = a_pos;\n"
           "}\n";

    return buf.str();
}

std::string TextureSamplerCase::genFragmentSource(void) const
{
    // fragment shader is passthrough
    if (m_type == TYPE_VERTEX)
        return s_colorPassthroughFragmentShaderSource;

    // fragment shader does the calculations
    std::ostringstream buf;
    if (m_testType != TEST_TEX_COORD_CUBE)
        buf << "uniform mediump sampler2D u_sampler;\n";
    else
        buf << "uniform mediump samplerCube u_sampler;\n";

    buf << "varying mediump vec4 v_attr;\n"
           "void main ()\n"
           "{\n";

    if (m_testType == TEST_TEX_COORD)
        buf << "    gl_FragColor = texture2D(u_sampler, v_attr.xy);\n";
    else if (m_testType == TEST_LOD)
        buf << "    gl_FragColor = texture2D(u_sampler, v_attr.xy, v_attr.x);\n";
    else if (m_testType == TEST_TEX_COORD_CUBE)
        buf << "    gl_FragColor = textureCube(u_sampler, vec3(v_attr.xy, v_attr.x + v_attr.y));\n";
    else
        DE_ASSERT(false);

    buf << "}\n";

    return buf.str();
}

/*--------------------------------------------------------------------*//*!
 * \brief Tests special floats as fragment shader outputs
 *
 * Tests that outputting special floats from a fragment shader does not change
 * the normal floating point values of outputted from a fragment shader. Special
 * floats are outputted in the green component, normal floating point values
 * in the red and blue component. Potential changes are tested by rendering
 * test pattern two times with different floating point values. The resulting
 * images' red and blue channels should be equal.
 *//*--------------------------------------------------------------------*/
class OutputCase : public FramebufferRenderCase
{
public:
    OutputCase(Context &context, const char *name, const char *desc, FramebufferRenderCase::FrameBufferType type);
    ~OutputCase(void);

    void testFBO(void);

private:
    std::string genVertexSource(void) const;
    std::string genFragmentSource(void) const;
};

OutputCase::OutputCase(Context &context, const char *name, const char *desc,
                       FramebufferRenderCase::FrameBufferType type)
    : FramebufferRenderCase(context, name, desc, type)
{
}

OutputCase::~OutputCase(void)
{
    deinit();
}

void OutputCase::testFBO(void)
{
    // Create a 1 X [s_specialFloats] grid of tiles (stripes).
    std::vector<tcu::Vec4> gridVertices((DE_LENGTH_OF_ARRAY(s_specialFloats) + 1) * 2);
    std::vector<uint16_t> indices(DE_LENGTH_OF_ARRAY(s_specialFloats) * 6);
    tcu::TextureFormat textureFormat(tcu::TextureFormat::RGBA, (m_fboType == FBO_RGBA_FLOAT16) ?
                                                                   (tcu::TextureFormat::FLOAT) :
                                                                   (tcu::TextureFormat::UNORM_INT8));
    tcu::TextureLevel specialImage(textureFormat, TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
    tcu::TextureLevel normalImage(textureFormat, TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);

    // vertices
    for (int y = 0; y < DE_LENGTH_OF_ARRAY(s_specialFloats) + 1; ++y)
    {
        const float posY = (float)y / (float)DE_LENGTH_OF_ARRAY(s_specialFloats) * 2.0f -
                           1.0f; // map from [0, len(s_specialFloats) ] to [-1, 1]

        gridVertices[y * 2 + 0] = tcu::Vec4(-1.0, posY, 0.0f, 1.0f);
        gridVertices[y * 2 + 1] = tcu::Vec4(1.0, posY, 0.0f, 1.0f);
    }

    // tiles
    for (int y = 0; y < DE_LENGTH_OF_ARRAY(s_specialFloats); ++y)
    {
        const int baseNdx = y * 6;

        indices[baseNdx + 0] = (uint16_t)((y + 0) * 2);
        indices[baseNdx + 1] = (uint16_t)((y + 1) * 2);
        indices[baseNdx + 2] = (uint16_t)((y + 1) * 2 + 1);

        indices[baseNdx + 3] = (uint16_t)((y + 0) * 2);
        indices[baseNdx + 4] = (uint16_t)((y + 1) * 2 + 1);
        indices[baseNdx + 5] = (uint16_t)((y + 0) * 2 + 1);
    }

    // Draw grids
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        const GLint positionLoc  = gl.getAttribLocation(m_program->getProgram(), "a_pos");
        const GLint specialLoc   = gl.getUniformLocation(m_program->getProgram(), "u_special");

        gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
        gl.viewport(0, 0, TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
        gl.useProgram(m_program->getProgram());
        GLU_EXPECT_NO_ERROR(gl.getError(), "pre-draw");

        gl.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, &gridVertices[0]);
        gl.enableVertexAttribArray(positionLoc);

        // draw 2 passes. Special and normal.
        for (int passNdx = 0; passNdx < 2; ++passNdx)
        {
            const bool specialPass = (passNdx == 0);

            m_testCtx.getLog() << tcu::TestLog::Message << "Pass " << passNdx
                               << ": Drawing stripes with the shader. Setting u_special for each stripe to ("
                               << ((specialPass) ? ("special") : ("1.0")) << ")." << tcu::TestLog::EndMessage;

            // draw stripes
            gl.clear(GL_COLOR_BUFFER_BIT);
            for (int y = 0; y < DE_LENGTH_OF_ARRAY(s_specialFloats); ++y)
            {
                const uint32_t one          = 0x3F800000;
                const uint32_t special      = s_specialFloats[y];
                const uint32_t uniformValue = (specialPass) ? (special) : (one);
                const int indexIndex        = y * 6;

                gl.uniform1fv(specialLoc, 1, (const float *)&uniformValue);
                gl.drawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, &indices[indexIndex]);
            }

            gl.finish();
            glu::readPixels(m_context.getRenderContext(), 0, 0,
                            ((specialPass) ? (specialImage) : (normalImage)).getAccess());
        }

        gl.disableVertexAttribArray(positionLoc);
        gl.useProgram(0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "OutputCase::iterate");
    }

    // Check results
    {
        tcu::Surface errorMask(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
        const tcu::RGBA badPixelColor = tcu::RGBA::red();
        const tcu::RGBA okPixelColor  = tcu::RGBA::green();
        int badPixels                 = 0;

        m_testCtx.getLog() << tcu::TestLog::Message
                           << "Checking passes have identical red and blue channels and the green channel is correct "
                              "in the constant pass."
                           << tcu::TestLog::EndMessage;

        for (int y = 0; y < specialImage.getHeight(); ++y)
            for (int x = 0; x < specialImage.getWidth(); ++x)
            {
                const float greenThreshold = 0.1f;
                const tcu::Vec4 cNormal    = normalImage.getAccess().getPixel(x, y);
                const tcu::Vec4 cSpecial   = specialImage.getAccess().getPixel(x, y);

                if (cNormal.x() != cSpecial.x() || cNormal.z() != cSpecial.z() || cNormal.y() < 1.0f - greenThreshold)
                {
                    ++badPixels;
                    errorMask.setPixel(x, y, badPixelColor);
                }
                else
                    errorMask.setPixel(x, y, okPixelColor);
            }

        m_testCtx.getLog() << tcu::TestLog::Message << "Found " << badPixels << " invalid pixel(s)."
                           << tcu::TestLog::EndMessage;

        if (badPixels)
        {
            m_testCtx.getLog() << tcu::TestLog::ImageSet("Results", "Result verification")
                               << tcu::TestLog::Image("Image with special green channel",
                                                      "Image with special green channel", specialImage)
                               << tcu::TestLog::Image("Image with constant green channel",
                                                      "Image with constant green channel", normalImage)
                               << tcu::TestLog::Image("Error Mask", "Error Mask", errorMask)
                               << tcu::TestLog::EndImageSet;

            // all ok?
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
        }
        else
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    }
}

std::string OutputCase::genVertexSource(void) const
{
    return "attribute highp vec4 a_pos;\n"
           "varying mediump vec2 v_pos;\n"
           "void main ()\n"
           "{\n"
           "    gl_Position = a_pos;\n"
           "    v_pos = a_pos.xy;\n"
           "}\n";
}

std::string OutputCase::genFragmentSource(void) const
{
    return "uniform mediump float u_special;\n"
           "varying mediump vec2 v_pos;\n"
           "void main ()\n"
           "{\n"
           "    gl_FragColor = vec4((v_pos.x + 1.0) * 0.5, u_special, (v_pos.y + 1.0) * 0.5, 1.0);\n"
           "}\n";
}

/*--------------------------------------------------------------------*//*!
 * \brief Tests special floats in blending
 *
 * Tests special floats as alpha and color components with various blending
 * modes. Test draws a test pattern and then does various blend operations
 * with special float values. After the blending test another test pattern
 * is drawn to detect possible blending anomalies. Test patterns should be
 * identical.
 *//*--------------------------------------------------------------------*/
class BlendingCase : public FramebufferRenderCase
{
public:
    BlendingCase(Context &context, const char *name, const char *desc, FramebufferRenderCase::FrameBufferType type);
    ~BlendingCase(void);

    void testFBO(void);

private:
    void drawTestImage(tcu::PixelBufferAccess dst, GLuint uColorLoc, int maxVertexIndex);

    std::string genVertexSource(void) const;
    std::string genFragmentSource(void) const;
};

BlendingCase::BlendingCase(Context &context, const char *name, const char *desc,
                           FramebufferRenderCase::FrameBufferType type)
    : FramebufferRenderCase(context, name, desc, type)
{
}

BlendingCase::~BlendingCase(void)
{
    deinit();
}

void BlendingCase::testFBO(void)
{
    static const GLenum equations[] = {
        GL_FUNC_ADD,
        GL_FUNC_SUBTRACT,
        GL_FUNC_REVERSE_SUBTRACT,
    };
    static const GLenum functions[] = {
        GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
    };

    // Create a [BlendFuncs] X [s_specialFloats] grid of tiles. ( BlendFuncs = equations x functions )

    const int numBlendFuncs = DE_LENGTH_OF_ARRAY(equations) * DE_LENGTH_OF_ARRAY(functions);
    std::vector<tcu::Vec4> gridVertices((numBlendFuncs + 1) * (DE_LENGTH_OF_ARRAY(s_specialFloats) + 1));
    std::vector<uint16_t> indices(numBlendFuncs * DE_LENGTH_OF_ARRAY(s_specialFloats) * 6);
    tcu::TextureFormat textureFormat(tcu::TextureFormat::RGBA, (m_fboType == FBO_RGBA_FLOAT16) ?
                                                                   (tcu::TextureFormat::FLOAT) :
                                                                   (tcu::TextureFormat::UNORM_INT8));
    tcu::TextureLevel beforeImage(textureFormat, TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
    tcu::TextureLevel afterImage(textureFormat, TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);

    // vertices
    for (int x = 0; x < DE_LENGTH_OF_ARRAY(s_specialFloats) + 1; ++x)
        for (int y = 0; y < numBlendFuncs + 1; ++y)
        {
            const float posX = (float)x / (float)DE_LENGTH_OF_ARRAY(s_specialFloats) * 2.0f -
                               1.0f; // map from [0, len(s_specialFloats)] to [-1, 1]
            const float posY = (float)y / (float)numBlendFuncs * 2.0f - 1.0f;

            gridVertices[x * (numBlendFuncs + 1) + y] = tcu::Vec4(posX, posY, 0.0f, 1.0f);
        }

    // tiles
    for (int x = 0; x < DE_LENGTH_OF_ARRAY(s_specialFloats); ++x)
        for (int y = 0; y < numBlendFuncs; ++y)
        {
            const int baseNdx = (x * numBlendFuncs + y) * 6;

            indices[baseNdx + 0] = (uint16_t)((x + 0) * (numBlendFuncs + 1) + (y + 0));
            indices[baseNdx + 1] = (uint16_t)((x + 1) * (numBlendFuncs + 1) + (y + 1));
            indices[baseNdx + 2] = (uint16_t)((x + 1) * (numBlendFuncs + 1) + (y + 0));

            indices[baseNdx + 3] = (uint16_t)((x + 0) * (numBlendFuncs + 1) + (y + 0));
            indices[baseNdx + 4] = (uint16_t)((x + 1) * (numBlendFuncs + 1) + (y + 1));
            indices[baseNdx + 5] = (uint16_t)((x + 0) * (numBlendFuncs + 1) + (y + 1));
        }

    // Draw tiles
    {
        const int numPasses      = 5;
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        const GLint positionLoc  = gl.getAttribLocation(m_program->getProgram(), "a_pos");
        const GLint specialLoc   = gl.getUniformLocation(m_program->getProgram(), "u_special");

        gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
        gl.clear(GL_COLOR_BUFFER_BIT);
        gl.viewport(0, 0, TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
        gl.useProgram(m_program->getProgram());
        gl.enable(GL_BLEND);
        GLU_EXPECT_NO_ERROR(gl.getError(), "pre-draw");

        gl.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, &gridVertices[0]);
        gl.enableVertexAttribArray(positionLoc);

        // draw "before" image
        m_testCtx.getLog() << tcu::TestLog::Message << "Drawing pre-draw pattern." << tcu::TestLog::EndMessage;
        drawTestImage(beforeImage.getAccess(), specialLoc, (int)gridVertices.size() - 1);
        GLU_EXPECT_NO_ERROR(gl.getError(), "pre-draw pattern");

        // draw multiple passes with special floats
        gl.clear(GL_COLOR_BUFFER_BIT);
        for (int passNdx = 0; passNdx < numPasses; ++passNdx)
        {
            de::Random rnd(123 + 567 * passNdx);

            m_testCtx.getLog() << tcu::TestLog::Message << "Pass " << passNdx << ": Drawing tiles with the shader.\n"
                               << "\tVarying u_special for each tile.\n"
                               << "\tVarying blend function and blend equation for each tile.\n"
                               << tcu::TestLog::EndMessage;

            // draw tiles
            for (int x = 0; x < DE_LENGTH_OF_ARRAY(s_specialFloats); ++x)
                for (int y = 0; y < numBlendFuncs; ++y)
                {
                    const GLenum blendEquation = equations[y % DE_LENGTH_OF_ARRAY(equations)];
                    const GLenum blendFunction = functions[y / DE_LENGTH_OF_ARRAY(equations)];
                    const GLenum blendFunctionDst =
                        rnd.choose<GLenum>(DE_ARRAY_BEGIN(functions), DE_ARRAY_END(functions));
                    const int indexIndex = (x * numBlendFuncs + y) * 6;

                    // "rnd.get"s are run in a deterministic order
                    const uint32_t componentR =
                        rnd.choose<uint32_t>(DE_ARRAY_BEGIN(s_specialFloats), DE_ARRAY_END(s_specialFloats));
                    const uint32_t componentG =
                        rnd.choose<uint32_t>(DE_ARRAY_BEGIN(s_specialFloats), DE_ARRAY_END(s_specialFloats));
                    const uint32_t componentB =
                        rnd.choose<uint32_t>(DE_ARRAY_BEGIN(s_specialFloats), DE_ARRAY_END(s_specialFloats));
                    const uint32_t componentA =
                        rnd.choose<uint32_t>(DE_ARRAY_BEGIN(s_specialFloats), DE_ARRAY_END(s_specialFloats));
                    const tcu::UVec4 uniformValue = tcu::UVec4(componentR, componentG, componentB, componentA);

                    gl.uniform4fv(specialLoc, 1, (const float *)uniformValue.getPtr());
                    gl.blendEquation(blendEquation);
                    gl.blendFunc(blendFunction, blendFunctionDst);
                    gl.drawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, &indices[indexIndex]);
                }
        }
        GLU_EXPECT_NO_ERROR(gl.getError(), "special passes");

        // draw "after" image
        m_testCtx.getLog() << tcu::TestLog::Message << "Drawing post-draw pattern." << tcu::TestLog::EndMessage;
        drawTestImage(afterImage.getAccess(), specialLoc, (int)gridVertices.size() - 1);
        GLU_EXPECT_NO_ERROR(gl.getError(), "post-draw pattern");

        gl.disableVertexAttribArray(positionLoc);
        gl.useProgram(0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "OutputCase::iterate");
    }

    // Check results
    {
        tcu::Surface errorMask(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
        const tcu::RGBA badPixelColor = tcu::RGBA::red();
        const tcu::RGBA okPixelColor  = tcu::RGBA::green();
        int badPixels                 = 0;

        m_testCtx.getLog() << tcu::TestLog::Message << "Checking patterns are identical." << tcu::TestLog::EndMessage;

        for (int y = 0; y < beforeImage.getHeight(); ++y)
            for (int x = 0; x < beforeImage.getWidth(); ++x)
            {
                const tcu::Vec4 cBefore = beforeImage.getAccess().getPixel(x, y);
                const tcu::Vec4 cAfter  = afterImage.getAccess().getPixel(x, y);

                if (cBefore != cAfter)
                {
                    ++badPixels;
                    errorMask.setPixel(x, y, badPixelColor);
                }
                else
                    errorMask.setPixel(x, y, okPixelColor);
            }

        m_testCtx.getLog() << tcu::TestLog::Message << "Found " << badPixels << " invalid pixel(s)."
                           << tcu::TestLog::EndMessage;

        if (badPixels)
        {
            m_testCtx.getLog() << tcu::TestLog::ImageSet("Results", "Result verification")
                               << tcu::TestLog::Image("Pattern drawn before special float blending",
                                                      "Pattern drawn before special float blending", beforeImage)
                               << tcu::TestLog::Image("Pattern drawn after special float blending",
                                                      "Pattern drawn after special float blending", afterImage)
                               << tcu::TestLog::EndImageSet;

            // all ok?
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
        }
        else
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    }
}

void BlendingCase::drawTestImage(tcu::PixelBufferAccess dst, GLuint uColorLoc, int maxVertexIndex)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    de::Random rnd(123);

    gl.clear(GL_COLOR_BUFFER_BIT);
    gl.blendEquation(GL_FUNC_ADD);
    gl.blendFunc(GL_ONE, GL_ONE);

    for (int tri = 0; tri < 20; ++tri)
    {
        tcu::Vec4 color;
        color.x() = rnd.getFloat();
        color.y() = rnd.getFloat();
        color.z() = rnd.getFloat();
        color.w() = rnd.getFloat();
        gl.uniform4fv(uColorLoc, 1, color.getPtr());

        uint16_t indices[3];
        indices[0] = (uint16_t)rnd.getInt(0, maxVertexIndex);
        indices[1] = (uint16_t)rnd.getInt(0, maxVertexIndex);
        indices[2] = (uint16_t)rnd.getInt(0, maxVertexIndex);

        gl.drawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, indices);
    }

    gl.finish();
    glu::readPixels(m_context.getRenderContext(), 0, 0, dst);
}

std::string BlendingCase::genVertexSource(void) const
{
    return "attribute highp vec4 a_pos;\n"
           "void main ()\n"
           "{\n"
           "    gl_Position = a_pos;\n"
           "}\n";
}

std::string BlendingCase::genFragmentSource(void) const
{
    return "uniform mediump vec4 u_special;\n"
           "void main ()\n"
           "{\n"
           "    gl_FragColor = u_special;\n"
           "}\n";
}

} // namespace

SpecialFloatTests::SpecialFloatTests(Context &context) : TestCaseGroup(context, "special_float", "Special float tests")
{
}

SpecialFloatTests::~SpecialFloatTests(void)
{
}

void SpecialFloatTests::init(void)
{
    tcu::TestCaseGroup *const vertexGroup =
        new tcu::TestCaseGroup(m_testCtx, "vertex", "Run vertex shader with special float values");
    tcu::TestCaseGroup *const fragmentGroup =
        new tcu::TestCaseGroup(m_testCtx, "fragment", "Run fragment shader with special float values");
    tcu::TestCaseGroup *const framebufferGroup =
        new tcu::TestCaseGroup(m_testCtx, "framebuffer", "Test framebuffers containing special float values");

    // .vertex
    {
        vertexGroup->addChild(
            new VertexAttributeCase(m_context, "attribute_buffer", "special attribute values in a buffer",
                                    VertexAttributeCase::STORAGE_BUFFER, VertexAttributeCase::TYPE_VERTEX));
        vertexGroup->addChild(
            new VertexAttributeCase(m_context, "attribute_client", "special attribute values in a client storage",
                                    VertexAttributeCase::STORAGE_CLIENT, VertexAttributeCase::TYPE_VERTEX));
        vertexGroup->addChild(
            new UniformCase(m_context, "uniform", "special uniform values", UniformCase::TYPE_VERTEX));
        vertexGroup->addChild(new TextureSamplerCase(m_context, "sampler_tex_coord", "special texture coords",
                                                     TextureSamplerCase::TYPE_VERTEX,
                                                     TextureSamplerCase::TEST_TEX_COORD));
        vertexGroup->addChild(
            new TextureSamplerCase(m_context, "sampler_tex_coord_cube", "special texture coords to cubemap",
                                   TextureSamplerCase::TYPE_VERTEX, TextureSamplerCase::TEST_TEX_COORD_CUBE));
        vertexGroup->addChild(new TextureSamplerCase(m_context, "sampler_lod", "special texture lod",
                                                     TextureSamplerCase::TYPE_VERTEX, TextureSamplerCase::TEST_LOD));

        addChild(vertexGroup);
    }

    // .fragment
    {
        fragmentGroup->addChild(
            new VertexAttributeCase(m_context, "attribute_buffer", "special attribute values in a buffer",
                                    VertexAttributeCase::STORAGE_BUFFER, VertexAttributeCase::TYPE_FRAGMENT));
        fragmentGroup->addChild(
            new VertexAttributeCase(m_context, "attribute_client", "special attribute values in a client storage",
                                    VertexAttributeCase::STORAGE_CLIENT, VertexAttributeCase::TYPE_FRAGMENT));
        fragmentGroup->addChild(
            new UniformCase(m_context, "uniform", "special uniform values", UniformCase::TYPE_FRAGMENT));
        fragmentGroup->addChild(new TextureSamplerCase(m_context, "sampler_tex_coord", "special texture coords",
                                                       TextureSamplerCase::TYPE_FRAGMENT,
                                                       TextureSamplerCase::TEST_TEX_COORD));
        fragmentGroup->addChild(
            new TextureSamplerCase(m_context, "sampler_tex_coord_cube", "special texture coords to cubemap",
                                   TextureSamplerCase::TYPE_FRAGMENT, TextureSamplerCase::TEST_TEX_COORD_CUBE));
        fragmentGroup->addChild(new TextureSamplerCase(m_context, "sampler_lod", "special texture lod",
                                                       TextureSamplerCase::TYPE_FRAGMENT,
                                                       TextureSamplerCase::TEST_LOD));

        addChild(fragmentGroup);
    }

    // .framebuffer
    {
        framebufferGroup->addChild(new OutputCase(m_context, "write_default",
                                                  "write special floating point values to default framebuffer",
                                                  FramebufferRenderCase::FBO_DEFAULT));
        framebufferGroup->addChild(new OutputCase(m_context, "write_rgba",
                                                  "write special floating point values to RGBA framebuffer",
                                                  FramebufferRenderCase::FBO_RGBA));
        framebufferGroup->addChild(new OutputCase(m_context, "write_rgba4",
                                                  "write special floating point values to RGBA4 framebuffer",
                                                  FramebufferRenderCase::FBO_RGBA4));
        framebufferGroup->addChild(new OutputCase(m_context, "write_rgb5_a1",
                                                  "write special floating point values to RGB5_A1 framebuffer",
                                                  FramebufferRenderCase::FBO_RGB5_A1));
        framebufferGroup->addChild(new OutputCase(m_context, "write_rgb565",
                                                  "write special floating point values to RGB565 framebuffer",
                                                  FramebufferRenderCase::FBO_RGB565));
        framebufferGroup->addChild(new OutputCase(m_context, "write_float16",
                                                  "write special floating point values to float16 framebuffer",
                                                  FramebufferRenderCase::FBO_RGBA_FLOAT16));

        framebufferGroup->addChild(new BlendingCase(m_context, "blend_default",
                                                    "blend special floating point values in a default framebuffer",
                                                    FramebufferRenderCase::FBO_DEFAULT));
        framebufferGroup->addChild(new BlendingCase(m_context, "blend_rgba",
                                                    "blend special floating point values in a RGBA framebuffer",
                                                    FramebufferRenderCase::FBO_RGBA));
        framebufferGroup->addChild(new BlendingCase(m_context, "blend_float16",
                                                    "blend special floating point values in a float16 framebuffer",
                                                    FramebufferRenderCase::FBO_RGBA_FLOAT16));

        addChild(framebufferGroup);
    }
}

} // namespace Stress
} // namespace gles2
} // namespace deqp
