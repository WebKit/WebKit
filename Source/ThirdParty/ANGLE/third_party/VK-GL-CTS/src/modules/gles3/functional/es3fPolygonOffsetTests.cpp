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
 * \brief Polygon offset tests.
 *//*--------------------------------------------------------------------*/

#include "es3fPolygonOffsetTests.hpp"
#include "deStringUtil.hpp"
#include "deRandom.hpp"
#include "gluContextInfo.hpp"
#include "gluRenderContext.hpp"
#include "gluShaderProgram.hpp"
#include "gluPixelTransfer.hpp"
#include "gluStrUtil.hpp"
#include "glwEnums.hpp"
#include "glwDefs.hpp"
#include "glwFunctions.hpp"
#include "tcuTestContext.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuVectorUtil.hpp"
#include "rrRenderer.hpp"
#include "rrFragmentOperations.hpp"

#include "sglrReferenceContext.hpp"

#include <string>
#include <limits>

using namespace glw; // GLint and other GL types

namespace deqp
{
namespace gles3
{
namespace Functional
{
namespace
{

const char *s_shaderSourceVertex   = "#version 300 es\n"
                                     "in highp vec4 a_position;\n"
                                     "in highp vec4 a_color;\n"
                                     "out highp vec4 v_color;\n"
                                     "void main (void)\n"
                                     "{\n"
                                     "    gl_Position = a_position;\n"
                                     "    v_color = a_color;\n"
                                     "}\n";
const char *s_shaderSourceFragment = "#version 300 es\n"
                                     "in highp vec4 v_color;\n"
                                     "layout(location = 0) out mediump vec4 fragColor;"
                                     "void main (void)\n"
                                     "{\n"
                                     "    fragColor = v_color;\n"
                                     "}\n";

static const tcu::Vec4 MASK_COLOR_OK   = tcu::Vec4(0.0f, 0.1f, 0.0f, 1.0f);
static const tcu::Vec4 MASK_COLOR_DEV  = tcu::Vec4(0.8f, 0.5f, 0.0f, 1.0f);
static const tcu::Vec4 MASK_COLOR_FAIL = tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f);

inline bool compareThreshold(const tcu::IVec4 &a, const tcu::IVec4 &b, const tcu::IVec4 &threshold)
{
    return tcu::boolAll(tcu::lessThanEqual(tcu::abs(a - b), threshold));
}

/*--------------------------------------------------------------------*//*!
* \brief Pixelwise comparison of two images.
* \note copied & modified from glsRasterizationTests
*
* Kernel radius defines maximum allowed distance. If radius is 0, only
* perfect match is allowed. Radius of 1 gives a 3x3 kernel.
*
* Return values: -1 = Perfect match
* 0 = Deviation within kernel
* >0 = Number of faulty pixels
*//*--------------------------------------------------------------------*/
int compareImages(tcu::TestLog &log, glu::RenderContext &renderCtx, const tcu::ConstPixelBufferAccess &test,
                  const tcu::ConstPixelBufferAccess &ref, const tcu::PixelBufferAccess &diffMask, int radius)
{
    const int height                = test.getHeight();
    const int width                 = test.getWidth();
    const int colorThreshold        = 128;
    const tcu::RGBA formatThreshold = renderCtx.getRenderTarget().getPixelFormat().getColorThreshold();
    const tcu::IVec4 threshold      = tcu::IVec4(colorThreshold, colorThreshold, colorThreshold,
                                            formatThreshold.getAlpha() > 0 ? colorThreshold : 0) +
                                 tcu::IVec4(formatThreshold.getRed(), formatThreshold.getGreen(),
                                            formatThreshold.getBlue(), formatThreshold.getAlpha());

    int faultyPixels  = 0;
    int compareFailed = -1;

    tcu::clear(diffMask, MASK_COLOR_OK);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            const tcu::IVec4 cRef = ref.getPixelInt(x, y);

            // Pixelwise match, no deviation or fault
            {
                const tcu::IVec4 cTest = test.getPixelInt(x, y);
                if (compareThreshold(cRef, cTest, threshold))
                    continue;
            }

            // If not, search within kernel radius
            {
                const int kYmin = deMax32(y - radius, 0);
                const int kYmax = deMin32(y + radius, height - 1);
                const int kXmin = deMax32(x - radius, 0);
                const int kXmax = deMin32(x + radius, width - 1);
                bool found      = false;

                for (int kY = kYmin; kY <= kYmax; kY++)
                    for (int kX = kXmin; kX <= kXmax; kX++)
                    {
                        const tcu::IVec4 cTest = test.getPixelInt(kX, kY);
                        if (compareThreshold(cRef, cTest, threshold))
                            found = true;
                    }

                if (found) // The pixel is deviating if the color is found inside the kernel
                {
                    diffMask.setPixel(MASK_COLOR_DEV, x, y);
                    if (compareFailed == -1)
                        compareFailed = 0;
                    continue;
                }
            }

            diffMask.setPixel(MASK_COLOR_FAIL, x, y);
            faultyPixels++; // The pixel is faulty if the color is not found
            compareFailed = 1;
        }
    }

    log << tcu::TestLog::Message << faultyPixels << " faulty pixel(s) found." << tcu::TestLog::EndMessage;

    return (compareFailed == 1 ? faultyPixels : compareFailed);
}

void verifyImages(tcu::TestLog &log, tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                  const tcu::ConstPixelBufferAccess &testImage, const tcu::ConstPixelBufferAccess &referenceImage)
{
    using tcu::TestLog;

    const int kernelRadius     = 1;
    const int faultyPixelLimit = 20;
    int faultyPixels;
    tcu::Surface diffMask(testImage.getWidth(), testImage.getHeight());

    faultyPixels = compareImages(log, renderCtx, referenceImage, testImage, diffMask.getAccess(), kernelRadius);

    if (faultyPixels > faultyPixelLimit)
    {
        log << TestLog::ImageSet("Images", "Image comparison");
        log << TestLog::Image("Test image", "Test image", testImage);
        log << TestLog::Image("Reference image", "Reference image", referenceImage);
        log << TestLog::Image("Difference mask", "Difference mask", diffMask.getAccess());
        log << TestLog::EndImageSet;

        log << tcu::TestLog::Message << "Got " << faultyPixels << " faulty pixel(s)." << tcu::TestLog::EndMessage;
        testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got faulty pixels");
    }
}

void verifyError(tcu::TestContext &testCtx, const glw::Functions &gl, GLenum expected)
{
    uint32_t got = gl.getError();
    if (got != expected)
    {
        testCtx.getLog() << tcu::TestLog::Message << "// ERROR: expected " << glu::getErrorStr(expected) << "; got "
                         << glu::getErrorStr(got) << tcu::TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid error");
    }
}

void checkCanvasSize(int width, int height, int minWidth, int minHeight)
{
    if (width < minWidth || height < minHeight)
        throw tcu::NotSupportedError(std::string("Render context size must be at least ") + de::toString(minWidth) +
                                     "x" + de::toString(minWidth));
}

class PositionColorShader : public sglr::ShaderProgram
{
public:
    enum
    {
        VARYINGLOC_COLOR = 0
    };

    PositionColorShader(void);
    void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets, const int numPackets) const;
    void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                        const rr::FragmentShadingContext &context) const;
};

PositionColorShader::PositionColorShader(void)
    : sglr::ShaderProgram(sglr::pdec::ShaderProgramDeclaration()
                          << sglr::pdec::VertexAttribute("a_position", rr::GENERICVECTYPE_FLOAT)
                          << sglr::pdec::VertexAttribute("a_color", rr::GENERICVECTYPE_FLOAT)
                          << sglr::pdec::VertexToFragmentVarying(rr::GENERICVECTYPE_FLOAT)
                          << sglr::pdec::FragmentOutput(rr::GENERICVECTYPE_FLOAT)
                          << sglr::pdec::VertexSource(s_shaderSourceVertex)
                          << sglr::pdec::FragmentSource(s_shaderSourceFragment))
{
}

void PositionColorShader::shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets,
                                        const int numPackets) const
{
    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
    {
        const int positionAttrLoc = 0;
        const int colorAttrLoc    = 1;

        rr::VertexPacket &packet = *packets[packetNdx];

        // Transform to position
        packet.position = rr::readVertexAttribFloat(inputs[positionAttrLoc], packet.instanceNdx, packet.vertexNdx);

        // Pass color to FS
        packet.outputs[VARYINGLOC_COLOR] =
            rr::readVertexAttribFloat(inputs[colorAttrLoc], packet.instanceNdx, packet.vertexNdx);
    }
}

void PositionColorShader::shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                                         const rr::FragmentShadingContext &context) const
{
    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
    {
        rr::FragmentPacket &packet = packets[packetNdx];

        for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
            rr::writeFragmentOutput(context, packetNdx, fragNdx, 0,
                                    rr::readTriangleVarying<float>(packet, context, VARYINGLOC_COLOR, fragNdx));
    }
}

// PolygonOffsetTestCase

class PolygonOffsetTestCase : public TestCase
{
public:
    PolygonOffsetTestCase(Context &context, const char *name, const char *description, GLenum internalFormat,
                          const char *internalFormatName, int canvasSize);

    virtual void testPolygonOffset(void) = 0;
    IterateResult iterate(void);

protected:
    const GLenum m_internalFormat;
    const char *m_internalFormatName;
    const int m_targetSize;
};

PolygonOffsetTestCase::PolygonOffsetTestCase(Context &context, const char *name, const char *description,
                                             GLenum internalFormat, const char *internalFormatName, int canvasSize)
    : TestCase(context, name, description)
    , m_internalFormat(internalFormat)
    , m_internalFormatName(internalFormatName)
    , m_targetSize(canvasSize)
{
}

PolygonOffsetTestCase::IterateResult PolygonOffsetTestCase::iterate(void)
{
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    m_testCtx.getLog() << tcu::TestLog::Message << "Testing PolygonOffset with " << m_internalFormatName
                       << " depth buffer." << tcu::TestLog::EndMessage;

    if (m_internalFormat == 0)
    {
        // default framebuffer
        const int width  = m_context.getRenderTarget().getWidth();
        const int height = m_context.getRenderTarget().getHeight();

        checkCanvasSize(width, height, m_targetSize, m_targetSize);

        if (m_context.getRenderTarget().getDepthBits() == 0)
            throw tcu::NotSupportedError("polygon offset tests require depth buffer");

        testPolygonOffset();
    }
    else
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();

        // framebuffer object
        GLuint colorRboId = 0;
        GLuint depthRboId = 0;
        GLuint fboId      = 0;
        bool fboComplete;

        gl.genRenderbuffers(1, &colorRboId);
        gl.bindRenderbuffer(GL_RENDERBUFFER, colorRboId);
        gl.renderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, m_targetSize, m_targetSize);
        verifyError(m_testCtx, gl, GL_NO_ERROR);

        gl.genRenderbuffers(1, &depthRboId);
        gl.bindRenderbuffer(GL_RENDERBUFFER, depthRboId);
        gl.renderbufferStorage(GL_RENDERBUFFER, m_internalFormat, m_targetSize, m_targetSize);
        verifyError(m_testCtx, gl, GL_NO_ERROR);

        gl.genFramebuffers(1, &fboId);
        gl.bindFramebuffer(GL_FRAMEBUFFER, fboId);
        gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRboId);
        gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRboId);
        verifyError(m_testCtx, gl, GL_NO_ERROR);

        fboComplete = gl.checkFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

        if (fboComplete)
            testPolygonOffset();

        gl.deleteFramebuffers(1, &fboId);
        gl.deleteRenderbuffers(1, &depthRboId);
        gl.deleteRenderbuffers(1, &colorRboId);

        if (!fboComplete)
            throw tcu::NotSupportedError("could not create fbo for testing.");
    }

    return STOP;
}

// UsageTestCase

class UsageTestCase : public PolygonOffsetTestCase
{
public:
    UsageTestCase(Context &context, const char *name, const char *description, GLenum internalFormat,
                  const char *internalFormatName);

    void testPolygonOffset(void);
};

UsageTestCase::UsageTestCase(Context &context, const char *name, const char *description, GLenum internalFormat,
                             const char *internalFormatName)
    : PolygonOffsetTestCase(context, name, description, internalFormat, internalFormatName, 200)
{
}

void UsageTestCase::testPolygonOffset(void)
{
    using tcu::TestLog;

    const tcu::Vec4 triangle[] = {
        tcu::Vec4(-1, 1, 0, 1),
        tcu::Vec4(1, 1, 0, 1),
        tcu::Vec4(1, -1, 0, 1),
    };

    tcu::TestLog &log = m_testCtx.getLog();
    tcu::Surface testImage(m_targetSize, m_targetSize);
    tcu::Surface referenceImage(m_targetSize, m_targetSize);
    int subpixelBits = 0;

    // render test image
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        const glu::ShaderProgram program(m_context.getRenderContext(),
                                         glu::makeVtxFragSources(s_shaderSourceVertex, s_shaderSourceFragment));
        const GLint positionLoc = gl.getAttribLocation(program.getProgram(), "a_position");
        const GLint colorLoc    = gl.getAttribLocation(program.getProgram(), "a_color");

        if (!program.isOk())
        {
            log << program;
            TCU_FAIL("Shader compile failed.");
        }

        gl.clearColor(0, 0, 0, 1);
        gl.clearDepthf(1.0f);
        gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        gl.viewport(0, 0, m_targetSize, m_targetSize);
        gl.useProgram(program.getProgram());
        gl.enable(GL_DEPTH_TEST);
        gl.depthFunc(
            GL_LEQUAL); // make test pass if polygon offset doesn't do anything. It has its own test case. This test is only for to detect always-on cases.

        log << TestLog::Message << "DepthFunc = GL_LEQUAL" << TestLog::EndMessage;

        gl.enableVertexAttribArray(positionLoc);
        gl.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, triangle);

        //draw back (offset disabled)

        log << TestLog::Message
            << "Draw bottom-right. Color = White.\tState: PolygonOffset(0, -2), POLYGON_OFFSET_FILL disabled."
            << TestLog::EndMessage;

        gl.polygonOffset(0, -2);
        gl.disable(GL_POLYGON_OFFSET_FILL);
        gl.vertexAttrib4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
        gl.drawArrays(GL_TRIANGLES, 0, 3);

        //draw front

        log << TestLog::Message
            << "Draw bottom-right. Color = Red.\tState: PolygonOffset(0, -1), POLYGON_OFFSET_FILL enabled."
            << TestLog::EndMessage;

        gl.polygonOffset(0, -1);
        gl.enable(GL_POLYGON_OFFSET_FILL);
        gl.vertexAttrib4f(colorLoc, 1.0f, 0.0f, 0.0f, 1.0f);
        gl.drawArrays(GL_TRIANGLES, 0, 3);

        gl.disableVertexAttribArray(positionLoc);
        gl.useProgram(0);
        gl.finish();

        glu::readPixels(m_context.getRenderContext(), 0, 0, testImage.getAccess());

        gl.getIntegerv(GL_SUBPIXEL_BITS, &subpixelBits);
    }

    // render reference image
    {
        rr::Renderer referenceRenderer;
        rr::VertexAttrib attribs[2];
        rr::RenderState state((rr::ViewportState)(rr::WindowRectangle(0, 0, m_targetSize, m_targetSize)), subpixelBits);

        PositionColorShader program;

        attribs[0].type            = rr::VERTEXATTRIBTYPE_FLOAT;
        attribs[0].size            = 4;
        attribs[0].stride          = 0;
        attribs[0].instanceDivisor = 0;
        attribs[0].pointer         = triangle;

        attribs[1].type    = rr::VERTEXATTRIBTYPE_DONT_CARE;
        attribs[1].generic = tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);

        tcu::clear(referenceImage.getAccess(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

        log << TestLog::Message << "Expecting: Bottom-right = Red." << TestLog::EndMessage;

        referenceRenderer.draw(rr::DrawCommand(
            state,
            rr::RenderTarget(rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(referenceImage.getAccess())),
            rr::Program(program.getVertexShader(), program.getFragmentShader()), 2, attribs,
            rr::PrimitiveList(rr::PRIMITIVETYPE_TRIANGLES, 3, 0)));
    }

    // compare
    verifyImages(log, m_testCtx, m_context.getRenderContext(), testImage.getAccess(), referenceImage.getAccess());
}

// UsageDisplacementTestCase

class UsageDisplacementTestCase : public PolygonOffsetTestCase
{
public:
    UsageDisplacementTestCase(Context &context, const char *name, const char *description, GLenum internalFormat,
                              const char *internalFormatName);

private:
    tcu::Vec4 genRandomVec4(de::Random &rnd) const;
    void testPolygonOffset(void);
};

UsageDisplacementTestCase::UsageDisplacementTestCase(Context &context, const char *name, const char *description,
                                                     GLenum internalFormat, const char *internalFormatName)
    : PolygonOffsetTestCase(context, name, description, internalFormat, internalFormatName, 200)
{
}

tcu::Vec4 UsageDisplacementTestCase::genRandomVec4(de::Random &rnd) const
{
    // generater triangle endpoint with following properties
    //    1) it will not be clipped
    //    2) it is not near either far or near plane to prevent possible problems related to depth clamping
    // => w >= 1.0 and z in (-0.9, 0.9) range
    tcu::Vec4 retVal;

    retVal.x() = rnd.getFloat(-1, 1);
    retVal.y() = rnd.getFloat(-1, 1);
    retVal.z() = 0.5f;
    retVal.w() = 1.0f + rnd.getFloat();

    return retVal;
}

void UsageDisplacementTestCase::testPolygonOffset(void)
{
    using tcu::TestLog;

    de::Random rnd(0xdec0de);
    tcu::TestLog &log = m_testCtx.getLog();
    tcu::Surface testImage(m_targetSize, m_targetSize);
    tcu::Surface referenceImage(m_targetSize, m_targetSize);

    // render test image
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        const glu::ShaderProgram program(m_context.getRenderContext(),
                                         glu::makeVtxFragSources(s_shaderSourceVertex, s_shaderSourceFragment));
        const GLint positionLoc = gl.getAttribLocation(program.getProgram(), "a_position");
        const GLint colorLoc    = gl.getAttribLocation(program.getProgram(), "a_color");
        const int numIterations = 40;

        if (!program.isOk())
        {
            log << program;
            TCU_FAIL("Shader compile failed.");
        }

        gl.clearColor(0, 0, 0, 1);
        gl.clearDepthf(1.0f);
        gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        gl.viewport(0, 0, m_targetSize, m_targetSize);
        gl.useProgram(program.getProgram());
        gl.enable(GL_DEPTH_TEST);
        gl.enable(GL_POLYGON_OFFSET_FILL);
        gl.enableVertexAttribArray(positionLoc);
        gl.vertexAttrib4f(colorLoc, 0.0f, 1.0f, 0.0f, 1.0f);

        log << TestLog::Message << "Framebuffer cleared, clear color = Black." << TestLog::EndMessage;
        log << TestLog::Message << "POLYGON_OFFSET_FILL enabled." << TestLog::EndMessage;

        // draw colorless (mask = 0,0,0) triangle at random* location, set offset and render green triangle with depthfunc = equal
        // *) w >= 1.0 and z in (-1, 1) range
        for (int iterationNdx = 0; iterationNdx < numIterations; ++iterationNdx)
        {
            const bool offsetDirection = rnd.getBool();
            const float offset         = offsetDirection ? -1.0f : 1.0f;
            tcu::Vec4 triangle[3];

            for (int vertexNdx = 0; vertexNdx < DE_LENGTH_OF_ARRAY(triangle); ++vertexNdx)
                triangle[vertexNdx] = genRandomVec4(rnd);

            gl.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, triangle);

            log << TestLog::Message << "Setup triangle with random coordinates:" << TestLog::EndMessage;
            for (size_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(triangle); ++ndx)
                log << TestLog::Message << "\tx=" << triangle[ndx].x() << "\ty=" << triangle[ndx].y()
                    << "\tz=" << triangle[ndx].z() << "\tw=" << triangle[ndx].w() << TestLog::EndMessage;

            log << TestLog::Message << "Draw colorless triangle.\tState: DepthFunc = GL_ALWAYS, PolygonOffset(0, 0)."
                << TestLog::EndMessage;

            gl.depthFunc(GL_ALWAYS);
            gl.polygonOffset(0, 0);
            gl.colorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            gl.drawArrays(GL_TRIANGLES, 0, 3);

            // all fragments should have different Z => DepthFunc == GL_EQUAL fails with every fragment

            log << TestLog::Message << "Draw green triangle.\tState: DepthFunc = GL_EQUAL, PolygonOffset(0, " << offset
                << ")." << TestLog::EndMessage;

            gl.depthFunc(GL_EQUAL);
            gl.polygonOffset(0, offset);
            gl.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            gl.drawArrays(GL_TRIANGLES, 0, 3);

            log << TestLog::Message << TestLog::EndMessage; // empty line for clarity
        }

        gl.disableVertexAttribArray(positionLoc);
        gl.useProgram(0);
        gl.finish();

        glu::readPixels(m_context.getRenderContext(), 0, 0, testImage.getAccess());
    }

    // render reference image
    log << TestLog::Message << "Expecting black framebuffer." << TestLog::EndMessage;
    tcu::clear(referenceImage.getAccess(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    // compare
    verifyImages(log, m_testCtx, m_context.getRenderContext(), testImage.getAccess(), referenceImage.getAccess());
}

// UsagePositiveNegativeTestCase

class UsagePositiveNegativeTestCase : public PolygonOffsetTestCase
{
public:
    UsagePositiveNegativeTestCase(Context &context, const char *name, const char *description, GLenum internalFormat,
                                  const char *internalFormatName);

    void testPolygonOffset(void);
};

UsagePositiveNegativeTestCase::UsagePositiveNegativeTestCase(Context &context, const char *name,
                                                             const char *description, GLenum internalFormat,
                                                             const char *internalFormatName)
    : PolygonOffsetTestCase(context, name, description, internalFormat, internalFormatName, 200)
{
}

void UsagePositiveNegativeTestCase::testPolygonOffset(void)
{
    using tcu::TestLog;

    const tcu::Vec4 triangleBottomRight[] = {
        tcu::Vec4(-1, 1, 0, 1),
        tcu::Vec4(1, 1, 0, 1),
        tcu::Vec4(1, -1, 0, 1),
    };
    const tcu::Vec4 triangleTopLeft[] = {
        tcu::Vec4(-1, -1, 0, 1),
        tcu::Vec4(-1, 1, 0, 1),
        tcu::Vec4(1, -1, 0, 1),
    };

    tcu::TestLog &log = m_testCtx.getLog();
    tcu::Surface testImage(m_targetSize, m_targetSize);
    tcu::Surface referenceImage(m_targetSize, m_targetSize);
    int subpixelBits = 0;
    // render test image
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        const glu::ShaderProgram program(m_context.getRenderContext(),
                                         glu::makeVtxFragSources(s_shaderSourceVertex, s_shaderSourceFragment));
        const GLint positionLoc = gl.getAttribLocation(program.getProgram(), "a_position");
        const GLint colorLoc    = gl.getAttribLocation(program.getProgram(), "a_color");

        if (!program.isOk())
        {
            log << program;
            TCU_FAIL("Shader compile failed.");
        }

        gl.clearColor(0, 0, 0, 1);
        gl.clearDepthf(1.0f);
        gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        gl.viewport(0, 0, m_targetSize, m_targetSize);
        gl.depthFunc(GL_LESS);
        gl.useProgram(program.getProgram());
        gl.enable(GL_DEPTH_TEST);
        gl.enable(GL_POLYGON_OFFSET_FILL);
        gl.enableVertexAttribArray(positionLoc);

        log << TestLog::Message << "DepthFunc = GL_LESS." << TestLog::EndMessage;
        log << TestLog::Message << "POLYGON_OFFSET_FILL enabled." << TestLog::EndMessage;

        //draw top left (negative offset test)
        {
            gl.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, triangleTopLeft);

            log << TestLog::Message << "Draw top-left. Color = White.\tState: PolygonOffset(0, 0)."
                << TestLog::EndMessage;

            gl.polygonOffset(0, 0);
            gl.vertexAttrib4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
            gl.drawArrays(GL_TRIANGLES, 0, 3);

            log << TestLog::Message << "Draw top-left. Color = Green.\tState: PolygonOffset(0, -1)."
                << TestLog::EndMessage;

            gl.polygonOffset(0, -1);
            gl.vertexAttrib4f(colorLoc, 0.0f, 1.0f, 0.0f, 1.0f);
            gl.drawArrays(GL_TRIANGLES, 0, 3);
        }

        //draw bottom right (positive offset test)
        {
            gl.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, triangleBottomRight);

            log << TestLog::Message << "Draw bottom-right. Color = White.\tState: PolygonOffset(0, 1)."
                << TestLog::EndMessage;

            gl.polygonOffset(0, 1);
            gl.vertexAttrib4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
            gl.drawArrays(GL_TRIANGLES, 0, 3);

            log << TestLog::Message << "Draw bottom-right. Color = Yellow.\tState: PolygonOffset(0, 0)."
                << TestLog::EndMessage;

            gl.polygonOffset(0, 0);
            gl.vertexAttrib4f(colorLoc, 1.0f, 1.0f, 0.0f, 1.0f);
            gl.drawArrays(GL_TRIANGLES, 0, 3);
        }

        gl.disableVertexAttribArray(positionLoc);
        gl.useProgram(0);
        gl.finish();

        glu::readPixels(m_context.getRenderContext(), 0, 0, testImage.getAccess());

        gl.getIntegerv(GL_SUBPIXEL_BITS, &subpixelBits);
    }

    // render reference image
    {
        rr::Renderer referenceRenderer;
        rr::VertexAttrib attribs[2];
        rr::RenderState state((rr::ViewportState)(rr::WindowRectangle(0, 0, m_targetSize, m_targetSize)), subpixelBits);

        PositionColorShader program;

        attribs[0].type            = rr::VERTEXATTRIBTYPE_FLOAT;
        attribs[0].size            = 4;
        attribs[0].stride          = 0;
        attribs[0].instanceDivisor = 0;
        attribs[0].pointer         = triangleTopLeft;

        attribs[1].type    = rr::VERTEXATTRIBTYPE_DONT_CARE;
        attribs[1].generic = tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);

        tcu::clear(referenceImage.getAccess(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

        log << TestLog::Message << "Expecting: Top-left = Green, Bottom-right = Yellow." << TestLog::EndMessage;

        referenceRenderer.draw(rr::DrawCommand(
            state,
            rr::RenderTarget(rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(referenceImage.getAccess())),
            rr::Program(program.getVertexShader(), program.getFragmentShader()), 2, attribs,
            rr::PrimitiveList(rr::PRIMITIVETYPE_TRIANGLES, 3, 0)));

        attribs[0].pointer = triangleBottomRight;
        attribs[1].generic = tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f);

        referenceRenderer.draw(rr::DrawCommand(
            state,
            rr::RenderTarget(rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(referenceImage.getAccess())),
            rr::Program(program.getVertexShader(), program.getFragmentShader()), 2, attribs,
            rr::PrimitiveList(rr::PRIMITIVETYPE_TRIANGLES, 3, 0)));
    }

    // compare
    verifyImages(log, m_testCtx, m_context.getRenderContext(), testImage.getAccess(), referenceImage.getAccess());
}

// ResultClampingTestCase

class ResultClampingTestCase : public PolygonOffsetTestCase
{
public:
    ResultClampingTestCase(Context &context, const char *name, const char *description, GLenum internalFormat,
                           const char *internalFormatName);

    void testPolygonOffset(void);
};

ResultClampingTestCase::ResultClampingTestCase(Context &context, const char *name, const char *description,
                                               GLenum internalFormat, const char *internalFormatName)
    : PolygonOffsetTestCase(context, name, description, internalFormat, internalFormatName, 200)
{
}

void ResultClampingTestCase::testPolygonOffset(void)
{
    using tcu::TestLog;

    const tcu::Vec4 triangleBottomRight[] = {
        tcu::Vec4(-1, 1, 1, 1),
        tcu::Vec4(1, 1, 1, 1),
        tcu::Vec4(1, -1, 1, 1),
    };
    const tcu::Vec4 triangleTopLeft[] = {
        tcu::Vec4(-1, -1, -1, 1),
        tcu::Vec4(-1, 1, -1, 1),
        tcu::Vec4(1, -1, -1, 1),
    };

    tcu::TestLog &log = m_testCtx.getLog();
    tcu::Surface testImage(m_targetSize, m_targetSize);
    tcu::Surface referenceImage(m_targetSize, m_targetSize);

    // render test image
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        const glu::ShaderProgram program(m_context.getRenderContext(),
                                         glu::makeVtxFragSources(s_shaderSourceVertex, s_shaderSourceFragment));
        const GLint positionLoc = gl.getAttribLocation(program.getProgram(), "a_position");
        const GLint colorLoc    = gl.getAttribLocation(program.getProgram(), "a_color");

        if (!program.isOk())
        {
            log << program;
            TCU_FAIL("Shader compile failed.");
        }

        gl.clearColor(0, 0, 0, 1);
        gl.clearDepthf(1.0f);
        gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        gl.viewport(0, 0, m_targetSize, m_targetSize);
        gl.useProgram(program.getProgram());
        gl.enable(GL_DEPTH_TEST);
        gl.enable(GL_POLYGON_OFFSET_FILL);
        gl.enableVertexAttribArray(positionLoc);

        log << TestLog::Message << "POLYGON_OFFSET_FILL enabled." << TestLog::EndMessage;

        //draw bottom right (far)
        {
            gl.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, triangleBottomRight);

            log << TestLog::Message
                << "Draw bottom-right. Color = White.\tState: DepthFunc = ALWAYS, PolygonOffset(0, 8), Polygon Z = "
                   "1.0. (Result depth should clamp to 1.0)."
                << TestLog::EndMessage;

            gl.depthFunc(GL_ALWAYS);
            gl.polygonOffset(0, 8);
            gl.vertexAttrib4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
            gl.drawArrays(GL_TRIANGLES, 0, 3);

            log << TestLog::Message
                << "Draw bottom-right. Color = Red.\tState: DepthFunc = GREATER, PolygonOffset(0, 9), Polygon Z = 1.0. "
                   "(Result depth should clamp to 1.0 too)"
                << TestLog::EndMessage;

            gl.depthFunc(GL_GREATER);
            gl.polygonOffset(0, 9);
            gl.vertexAttrib4f(colorLoc, 1.0f, 0.0f, 0.0f, 1.0f);
            gl.drawArrays(GL_TRIANGLES, 0, 3);
        }

        //draw top left (near)
        {
            gl.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, triangleTopLeft);

            log << TestLog::Message
                << "Draw top-left. Color = White.\tState: DepthFunc = ALWAYS, PolygonOffset(0, -8), Polygon Z = -1.0. "
                   "(Result depth should clamp to -1.0)"
                << TestLog::EndMessage;

            gl.depthFunc(GL_ALWAYS);
            gl.polygonOffset(0, -8);
            gl.vertexAttrib4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
            gl.drawArrays(GL_TRIANGLES, 0, 3);

            log << TestLog::Message
                << "Draw top-left. Color = Yellow.\tState: DepthFunc = LESS, PolygonOffset(0, -9), Polygon Z = -1.0. "
                   "(Result depth should clamp to -1.0 too)."
                << TestLog::EndMessage;

            gl.depthFunc(GL_LESS);
            gl.polygonOffset(0, -9);
            gl.vertexAttrib4f(colorLoc, 1.0f, 1.0f, 0.0f, 1.0f);
            gl.drawArrays(GL_TRIANGLES, 0, 3);
        }

        gl.disableVertexAttribArray(positionLoc);
        gl.useProgram(0);
        gl.finish();

        glu::readPixels(m_context.getRenderContext(), 0, 0, testImage.getAccess());
    }

    // render reference image
    log << TestLog::Message << "Expecting: Top-left = White, Bottom-right = White." << TestLog::EndMessage;
    tcu::clear(referenceImage.getAccess(), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

    // compare
    verifyImages(log, m_testCtx, m_context.getRenderContext(), testImage.getAccess(), referenceImage.getAccess());
}

// UsageSlopeTestCase

class UsageSlopeTestCase : public PolygonOffsetTestCase
{
public:
    UsageSlopeTestCase(Context &context, const char *name, const char *description, GLenum internalFormat,
                       const char *internalFormatName);

    void testPolygonOffset(void);
};

UsageSlopeTestCase::UsageSlopeTestCase(Context &context, const char *name, const char *description,
                                       GLenum internalFormat, const char *internalFormatName)
    : PolygonOffsetTestCase(context, name, description, internalFormat, internalFormatName, 200)
{
}

void UsageSlopeTestCase::testPolygonOffset(void)
{
    using tcu::TestLog;

    const tcu::Vec4 triangleBottomRight[] = {
        tcu::Vec4(-1, 1, 0.0f, 1),
        tcu::Vec4(1, 1, 0.9f, 1),
        tcu::Vec4(1, -1, 0.9f, 1),
    };
    const tcu::Vec4 triangleTopLeft[] = {
        tcu::Vec4(-1, -1, -0.9f, 1),
        tcu::Vec4(-1, 1, 0.9f, 1),
        tcu::Vec4(1, -1, 0.0f, 1),
    };

    tcu::TestLog &log = m_testCtx.getLog();
    tcu::Surface testImage(m_targetSize, m_targetSize);
    tcu::Surface referenceImage(m_targetSize, m_targetSize);

    // render test image
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        const glu::ShaderProgram program(m_context.getRenderContext(),
                                         glu::makeVtxFragSources(s_shaderSourceVertex, s_shaderSourceFragment));
        const GLint positionLoc = gl.getAttribLocation(program.getProgram(), "a_position");
        const GLint colorLoc    = gl.getAttribLocation(program.getProgram(), "a_color");

        if (!program.isOk())
        {
            log << program;
            TCU_FAIL("Shader compile failed.");
        }

        gl.clearColor(0, 0, 0, 1);
        gl.clearDepthf(1.0f);
        gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        gl.viewport(0, 0, m_targetSize, m_targetSize);
        gl.useProgram(program.getProgram());
        gl.enable(GL_DEPTH_TEST);
        gl.enable(GL_POLYGON_OFFSET_FILL);
        gl.enableVertexAttribArray(positionLoc);

        log << TestLog::Message << "POLYGON_OFFSET_FILL enabled." << TestLog::EndMessage;

        //draw top left (negative offset test)
        {
            gl.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, triangleTopLeft);

            log << TestLog::Message << "Draw top-left. Color = White.\tState: DepthFunc = ALWAYS, PolygonOffset(0, 0)."
                << TestLog::EndMessage;

            gl.depthFunc(GL_ALWAYS);
            gl.polygonOffset(0, 0);
            gl.vertexAttrib4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
            gl.drawArrays(GL_TRIANGLES, 0, 3);

            log << TestLog::Message << "Draw top-left. Color = Green.\tState: DepthFunc = LESS, PolygonOffset(-1, 0)."
                << TestLog::EndMessage;

            gl.depthFunc(GL_LESS);
            gl.polygonOffset(-1, 0);
            gl.vertexAttrib4f(colorLoc, 0.0f, 1.0f, 0.0f, 1.0f);
            gl.drawArrays(GL_TRIANGLES, 0, 3);
        }

        //draw bottom right (positive offset test)
        {
            gl.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, triangleBottomRight);

            log << TestLog::Message
                << "Draw bottom-right. Color = White.\tState: DepthFunc = ALWAYS, PolygonOffset(0, 0)."
                << TestLog::EndMessage;

            gl.depthFunc(GL_ALWAYS);
            gl.polygonOffset(0, 0);
            gl.vertexAttrib4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
            gl.drawArrays(GL_TRIANGLES, 0, 3);

            log << TestLog::Message
                << "Draw bottom-right. Color = Green.\tState: DepthFunc = GREATER, PolygonOffset(1, 0)."
                << TestLog::EndMessage;

            gl.depthFunc(GL_GREATER);
            gl.polygonOffset(1, 0);
            gl.vertexAttrib4f(colorLoc, 0.0f, 1.0f, 0.0f, 1.0f);
            gl.drawArrays(GL_TRIANGLES, 0, 3);
        }

        gl.disableVertexAttribArray(positionLoc);
        gl.useProgram(0);
        gl.finish();

        glu::readPixels(m_context.getRenderContext(), 0, 0, testImage.getAccess());
    }

    // render reference image
    log << TestLog::Message << "Expecting: Top-left = Green, Bottom-right = Green." << TestLog::EndMessage;
    tcu::clear(referenceImage.getAccess(), tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));

    // compare
    verifyImages(log, m_testCtx, m_context.getRenderContext(), testImage.getAccess(), referenceImage.getAccess());
}

// ZeroSlopeTestCase

class ZeroSlopeTestCase : public PolygonOffsetTestCase
{
public:
    ZeroSlopeTestCase(Context &context, const char *name, const char *description, GLenum internalFormat,
                      const char *internalFormatName);

    void testPolygonOffset(void);
};

ZeroSlopeTestCase::ZeroSlopeTestCase(Context &context, const char *name, const char *description, GLenum internalFormat,
                                     const char *internalFormatName)
    : PolygonOffsetTestCase(context, name, description, internalFormat, internalFormatName, 200)
{
}

void ZeroSlopeTestCase::testPolygonOffset(void)
{
    using tcu::TestLog;

    const tcu::Vec4 triangle[] = {
        tcu::Vec4(-0.4f, 0.4f, 0.0f, 1.0f),
        tcu::Vec4(-0.8f, -0.5f, 0.0f, 1.0f),
        tcu::Vec4(0.7f, 0.2f, 0.0f, 1.0f),
    };

    tcu::TestLog &log = m_testCtx.getLog();
    tcu::Surface testImage(m_targetSize, m_targetSize);
    tcu::Surface referenceImage(m_targetSize, m_targetSize);

    // log the triangle
    log << TestLog::Message << "Setup triangle with coordinates:" << TestLog::EndMessage;
    for (size_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(triangle); ++ndx)
        log << TestLog::Message << "\tx=" << triangle[ndx].x() << "\ty=" << triangle[ndx].y()
            << "\tz=" << triangle[ndx].z() << "\tw=" << triangle[ndx].w() << TestLog::EndMessage;

    // render test image
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        const glu::ShaderProgram program(m_context.getRenderContext(),
                                         glu::makeVtxFragSources(s_shaderSourceVertex, s_shaderSourceFragment));
        const GLint positionLoc = gl.getAttribLocation(program.getProgram(), "a_position");
        const GLint colorLoc    = gl.getAttribLocation(program.getProgram(), "a_color");

        if (!program.isOk())
        {
            log << program;
            TCU_FAIL("Shader compile failed.");
        }

        gl.clearColor(0, 0, 0, 1);
        gl.clearDepthf(1.0f);
        gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        gl.viewport(0, 0, m_targetSize, m_targetSize);
        gl.useProgram(program.getProgram());
        gl.enable(GL_DEPTH_TEST);
        gl.enable(GL_POLYGON_OFFSET_FILL);
        gl.enableVertexAttribArray(positionLoc);

        log << TestLog::Message << "POLYGON_OFFSET_FILL enabled." << TestLog::EndMessage;

        {
            gl.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, triangle);

            log << TestLog::Message << "Draw triangle. Color = Red.\tState: DepthFunc = ALWAYS, PolygonOffset(0, 0)."
                << TestLog::EndMessage;

            gl.depthFunc(GL_ALWAYS);
            gl.polygonOffset(0, 0);
            gl.vertexAttrib4f(colorLoc, 1.0f, 0.0f, 0.0f, 1.0f);
            gl.drawArrays(GL_TRIANGLES, 0, 3);

            log << TestLog::Message << "Draw triangle. Color = Black.\tState: DepthFunc = EQUAL, PolygonOffset(4, 0)."
                << TestLog::EndMessage;

            gl.depthFunc(GL_EQUAL);
            gl.polygonOffset(4, 0); // triangle slope == 0
            gl.vertexAttrib4f(colorLoc, 0.0f, 0.0f, 0.0f, 1.0f);
            gl.drawArrays(GL_TRIANGLES, 0, 3);
        }

        gl.disableVertexAttribArray(positionLoc);
        gl.useProgram(0);
        gl.finish();

        glu::readPixels(m_context.getRenderContext(), 0, 0, testImage.getAccess());
    }

    // render reference image
    log << TestLog::Message << "Expecting black triangle." << TestLog::EndMessage;
    tcu::clear(referenceImage.getAccess(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    // compare
    verifyImages(log, m_testCtx, m_context.getRenderContext(), testImage.getAccess(), referenceImage.getAccess());
}

// OneSlopeTestCase

class OneSlopeTestCase : public PolygonOffsetTestCase
{
public:
    OneSlopeTestCase(Context &context, const char *name, const char *description, GLenum internalFormat,
                     const char *internalFormatName);

    void testPolygonOffset(void);
};

OneSlopeTestCase::OneSlopeTestCase(Context &context, const char *name, const char *description, GLenum internalFormat,
                                   const char *internalFormatName)
    : PolygonOffsetTestCase(context, name, description, internalFormat, internalFormatName, 200)
{
}

void OneSlopeTestCase::testPolygonOffset(void)
{
    using tcu::TestLog;

    /*
     * setup vertices subject to following properties
     *   dz_w / dx_w == 1
     *   dz_w / dy_w == 0
     * or
     *   dz_w / dx_w == 0
     *   dz_w / dy_w == 1
     * ==> m == 1
     */
    const float cornerDepth         = float(m_targetSize);
    const tcu::Vec4 triangles[2][3] = {
        {
            tcu::Vec4(-1, -1, -cornerDepth, 1),
            tcu::Vec4(-1, 1, -cornerDepth, 1),
            tcu::Vec4(1, -1, cornerDepth, 1),
        },
        {
            tcu::Vec4(-1, 1, cornerDepth, 1),
            tcu::Vec4(1, 1, cornerDepth, 1),
            tcu::Vec4(1, -1, -cornerDepth, 1),
        },
    };

    tcu::TestLog &log = m_testCtx.getLog();
    tcu::Surface testImage(m_targetSize, m_targetSize);
    tcu::Surface referenceImage(m_targetSize, m_targetSize);

    // log triangle info
    log << TestLog::Message << "Setup triangle0 coordinates: (slope in window coordinates = 1.0)"
        << TestLog::EndMessage;
    for (size_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(triangles[0]); ++ndx)
        log << TestLog::Message << "\tx=" << triangles[0][ndx].x() << "\ty=" << triangles[0][ndx].y()
            << "\tz=" << triangles[0][ndx].z() << "\tw=" << triangles[0][ndx].w() << TestLog::EndMessage;
    log << TestLog::Message << "Setup triangle1 coordinates: (slope in window coordinates = 1.0)"
        << TestLog::EndMessage;
    for (size_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(triangles[1]); ++ndx)
        log << TestLog::Message << "\tx=" << triangles[1][ndx].x() << "\ty=" << triangles[1][ndx].y()
            << "\tz=" << triangles[1][ndx].z() << "\tw=" << triangles[1][ndx].w() << TestLog::EndMessage;

    // render test image
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        const glu::ShaderProgram program(m_context.getRenderContext(),
                                         glu::makeVtxFragSources(s_shaderSourceVertex, s_shaderSourceFragment));
        const GLint positionLoc = gl.getAttribLocation(program.getProgram(), "a_position");
        const GLint colorLoc    = gl.getAttribLocation(program.getProgram(), "a_color");

        if (!program.isOk())
        {
            log << program;
            TCU_FAIL("Shader compile failed.");
        }

        gl.clearColor(0, 0, 0, 1);
        gl.clear(GL_COLOR_BUFFER_BIT);
        gl.viewport(0, 0, m_targetSize, m_targetSize);
        gl.useProgram(program.getProgram());
        gl.enable(GL_DEPTH_TEST);
        gl.enable(GL_POLYGON_OFFSET_FILL);
        gl.enableVertexAttribArray(positionLoc);

        log << TestLog::Message << "Framebuffer cleared, clear color = Black." << TestLog::EndMessage;
        log << TestLog::Message << "POLYGON_OFFSET_FILL enabled." << TestLog::EndMessage;

        // top left (positive offset)
        {
            log << TestLog::Message << "Clear depth to 1.0." << TestLog::EndMessage;

            gl.clearDepthf(1.0f); // far
            gl.clear(GL_DEPTH_BUFFER_BIT);

            gl.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, triangles[0]);

            log << TestLog::Message
                << "Draw triangle0. Color = Red.\tState: DepthFunc = NOTEQUAL, PolygonOffset(10, 0). (Result depth "
                   "should clamp to 1.0)."
                << TestLog::EndMessage;

            gl.polygonOffset(10, 0); // clamps any depth on the triangle to 1
            gl.depthFunc(GL_NOTEQUAL);
            gl.vertexAttrib4f(colorLoc, 1.0f, 0.0f, 0.0f, 1.0f);
            gl.drawArrays(GL_TRIANGLES, 0, 3);
        }
        // bottom right (negative offset)
        {
            log << TestLog::Message << "Clear depth to 0.0." << TestLog::EndMessage;

            gl.clearDepthf(0.0f); // far
            gl.clear(GL_DEPTH_BUFFER_BIT);

            gl.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, triangles[1]);

            log << TestLog::Message
                << "Draw triangle1. Color = Green.\tState: DepthFunc = NOTEQUAL, PolygonOffset(-10, 0). (Result depth "
                   "should clamp to 0.0)."
                << TestLog::EndMessage;

            gl.polygonOffset(-10, 0); // clamps depth to 0
            gl.depthFunc(GL_NOTEQUAL);
            gl.vertexAttrib4f(colorLoc, 0.0f, 1.0f, 0.0f, 1.0f);
            gl.drawArrays(GL_TRIANGLES, 0, 3);
        }

        gl.disableVertexAttribArray(positionLoc);
        gl.useProgram(0);
        gl.finish();

        glu::readPixels(m_context.getRenderContext(), 0, 0, testImage.getAccess());
    }

    // render reference image
    log << TestLog::Message << "Expecting black framebuffer." << TestLog::EndMessage;
    tcu::clear(referenceImage.getAccess(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    // compare
    verifyImages(log, m_testCtx, m_context.getRenderContext(), testImage.getAccess(), referenceImage.getAccess());
}

} // namespace

PolygonOffsetTests::PolygonOffsetTests(Context &context)
    : TestCaseGroup(context, "polygon_offset", "Polygon offset tests")
{
}

PolygonOffsetTests::~PolygonOffsetTests(void)
{
}

void PolygonOffsetTests::init(void)
{
    const struct DepthBufferFormat
    {
        enum BufferType
        {
            TYPE_FIXED_POINT,
            TYPE_FLOATING_POINT,
            TYPE_UNKNOWN
        };

        GLenum internalFormat;
        int bits;
        BufferType floatingPoint;
        const char *name;
    } depthFormats[] = {
        {0, 0, DepthBufferFormat::TYPE_UNKNOWN, "default"},
        {GL_DEPTH_COMPONENT16, 16, DepthBufferFormat::TYPE_FIXED_POINT, "fixed16"},
        {GL_DEPTH_COMPONENT24, 24, DepthBufferFormat::TYPE_FIXED_POINT, "fixed24"},
        {GL_DEPTH_COMPONENT32F, 32, DepthBufferFormat::TYPE_FLOATING_POINT, "float32"},
    };

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(depthFormats); ++ndx)
    {
        const DepthBufferFormat &format = depthFormats[ndx];

        // enable works?
        addChild(new UsageTestCase(m_context, (std::string(format.name) + "_enable").c_str(),
                                   "test enable GL_POLYGON_OFFSET_FILL", format.internalFormat, format.name));

        // Really moves the polygons ?
        addChild(new UsageDisplacementTestCase(m_context,
                                               (std::string(format.name) + "_displacement_with_units").c_str(),
                                               "test polygon offset", format.internalFormat, format.name));

        // Really moves the polygons to right direction ?
        addChild(new UsagePositiveNegativeTestCase(m_context, (std::string(format.name) + "_render_with_units").c_str(),
                                                   "test polygon offset", format.internalFormat, format.name));

        // Is total result clamped to [0,1] like promised?
        addChild(new ResultClampingTestCase(m_context, (std::string(format.name) + "_result_depth_clamp").c_str(),
                                            "test polygon offset clamping", format.internalFormat, format.name));

        // Slope really moves the polygon?
        addChild(new UsageSlopeTestCase(m_context, (std::string(format.name) + "_render_with_factor").c_str(),
                                        "test polygon offset factor", format.internalFormat, format.name));

        // Factor with zero slope
        addChild(new ZeroSlopeTestCase(m_context, (std::string(format.name) + "_factor_0_slope").c_str(),
                                       "test polygon offset factor", format.internalFormat, format.name));

        // Factor with 1.0 slope
        addChild(new OneSlopeTestCase(m_context, (std::string(format.name) + "_factor_1_slope").c_str(),
                                      "test polygon offset factor", format.internalFormat, format.name));
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
