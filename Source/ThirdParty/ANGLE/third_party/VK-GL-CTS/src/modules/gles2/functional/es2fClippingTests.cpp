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
 * \brief Clipping tests.
 *//*--------------------------------------------------------------------*/

#include "es2fClippingTests.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuVectorUtil.hpp"
#include "deStringUtil.hpp"
#include "deRandom.hpp"

#include "sglrReferenceContext.hpp"
#include "sglrGLContext.hpp"

#include "glwEnums.hpp"
#include "glwDefs.hpp"
#include "glwFunctions.hpp"

using namespace glw; // GLint and other GL types

namespace deqp
{
namespace gles2
{
namespace Functional
{
namespace
{

using tcu::ConstPixelBufferAccess;
using tcu::PixelBufferAccess;
using tcu::TestLog;

static const tcu::Vec4 MASK_COLOR_OK   = tcu::Vec4(0.0f, 0.1f, 0.0f, 1.0f);
static const tcu::Vec4 MASK_COLOR_DEV  = tcu::Vec4(0.8f, 0.5f, 0.0f, 1.0f);
static const tcu::Vec4 MASK_COLOR_FAIL = tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f);

const int TEST_CANVAS_SIZE = 200;
const rr::WindowRectangle VIEWPORT_WHOLE(0, 0, TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
const rr::WindowRectangle VIEWPORT_CENTER(TEST_CANVAS_SIZE / 4, TEST_CANVAS_SIZE / 4, TEST_CANVAS_SIZE / 2,
                                          TEST_CANVAS_SIZE / 2);
const rr::WindowRectangle VIEWPORT_CORNER(TEST_CANVAS_SIZE / 2, TEST_CANVAS_SIZE / 2, TEST_CANVAS_SIZE / 2,
                                          TEST_CANVAS_SIZE / 2);

const char *shaderSourceVertex   = "attribute highp vec4 a_position;\n"
                                   "attribute highp vec4 a_color;\n"
                                   "attribute highp float a_pointSize;\n"
                                   "varying mediump vec4 varFragColor;\n"
                                   "void main (void)\n"
                                   "{\n"
                                   "    gl_Position = a_position;\n"
                                   "    gl_PointSize = a_pointSize;\n"
                                   "    varFragColor = a_color;\n"
                                   "}\n";
const char *shaderSourceFragment = "varying mediump vec4 varFragColor;\n"
                                   "void main (void)\n"
                                   "{\n"
                                   "    gl_FragColor = varFragColor;\n"
                                   "}\n";

inline bool isBlack(const tcu::IVec4 &a)
{
    return a.x() == 0 && a.y() == 0 && a.z() == 0;
}

inline bool isHalfFilled(const tcu::IVec4 &a)
{
    const tcu::IVec4 halfFilled(127, 0, 0, 0);
    const tcu::IVec4 threshold(20, 256, 256, 256);

    return tcu::boolAll(tcu::lessThanEqual(tcu::abs(a - halfFilled), threshold));
}

inline bool isLessThanHalfFilled(const tcu::IVec4 &a)
{
    const int halfFilled = 127;
    const int threshold  = 20;

    return a.x() + threshold < halfFilled;
}

inline bool compareBlackNonBlackPixels(const tcu::IVec4 &a, const tcu::IVec4 &b)
{
    return isBlack(a) == isBlack(b);
}

inline bool compareColoredPixels(const tcu::IVec4 &a, const tcu::IVec4 &b)
{
    const bool aIsBlack = isBlack(a);
    const bool bIsBlack = isBlack(b);
    const tcu::IVec4 threshold(20, 20, 20, 0);

    if (aIsBlack && bIsBlack)
        return true;
    if (aIsBlack != bIsBlack)
        return false;

    return tcu::boolAll(tcu::lessThanEqual(tcu::abs(a - b), threshold));
}

void blitImageOnBlackSurface(const ConstPixelBufferAccess &src, const PixelBufferAccess &dst)
{
    const int height = src.getHeight();
    const int width  = src.getWidth();

    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
        {
            const tcu::IVec4 cSrc = src.getPixelInt(x, y);
            const tcu::IVec4 cDst = tcu::IVec4(cSrc.x(), cSrc.y(), cSrc.z(), 255);

            dst.setPixel(cDst, x, y);
        }
}

/*--------------------------------------------------------------------*//*!
 * \brief Pixelwise comparison of two images.
 * \note copied & modified from glsRasterizationTests
 *
 * Kernel radius defines maximum allowed distance. If radius is 0, only
 * perfect match is allowed. Radius of 1 gives a 3x3 kernel. Pixels are
 * equal if pixelCmp returns true..
 *
 * Return values:  -1 = Perfect match
 *                    0 = Deviation within kernel
 *                   >0 = Number of faulty pixels
 *//*--------------------------------------------------------------------*/
inline int compareImages(tcu::TestLog &log, const ConstPixelBufferAccess &test, const ConstPixelBufferAccess &ref,
                         const PixelBufferAccess &diffMask, int kernelRadius,
                         bool (*pixelCmp)(const tcu::IVec4 &a, const tcu::IVec4 &b))
{
    const int height    = test.getHeight();
    const int width     = test.getWidth();
    int deviatingPixels = 0;
    int faultyPixels    = 0;
    int compareFailed   = -1;

    tcu::clear(diffMask, MASK_COLOR_OK);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            const tcu::IVec4 cRef  = ref.getPixelInt(x, y);
            const tcu::IVec4 cTest = test.getPixelInt(x, y);

            // Pixelwise match, no deviation or fault
            if ((*pixelCmp)(cRef, cTest))
                continue;

            // Deviation
            {
                const int radius = kernelRadius;
                bool foundRef    = false;
                bool foundTest   = false;

                // edges are considered a "deviation" too. The suitable pixel could be "behind" the edge
                if (y < radius || x < radius || y + radius >= height || x + radius >= width)
                {
                    foundRef  = true;
                    foundTest = true;
                }
                else
                {
                    // find ref
                    for (int kY = y - radius; kY <= y + radius; kY++)
                        for (int kX = x - radius; kX <= x + radius; kX++)
                        {
                            if ((*pixelCmp)(cRef, test.getPixelInt(kX, kY)))
                            {
                                foundRef = true;
                                break;
                            }
                        }

                    // find result
                    for (int kY = y - radius; kY <= y + radius; kY++)
                        for (int kX = x - radius; kX <= x + radius; kX++)
                        {
                            if ((*pixelCmp)(cTest, ref.getPixelInt(kX, kY)))
                            {
                                foundTest = true;
                                break;
                            }
                        }
                }

                // A pixel is deviating if the reference color is found inside the kernel and (~= every pixel reference draws must be drawn by the gl too)
                // the result color is found in the reference image inside the kernel         (~= every pixel gl draws must be drawn by the reference too)
                if (foundRef && foundTest)
                {
                    diffMask.setPixel(MASK_COLOR_DEV, x, y);
                    if (compareFailed == -1)
                        compareFailed = 0;
                    deviatingPixels++;
                    continue;
                }
            }

            diffMask.setPixel(MASK_COLOR_FAIL, x, y);
            faultyPixels++; // The pixel is faulty if the color is not found
            compareFailed = 1;
        }
    }

    log << TestLog::Message << deviatingPixels << " deviating pixel(s) found." << TestLog::EndMessage;
    log << TestLog::Message << faultyPixels << " faulty pixel(s) found." << TestLog::EndMessage;

    return (compareFailed == 1 ? faultyPixels : compareFailed);
}

/*--------------------------------------------------------------------*//*!
 * \brief Pixelwise comparison of two images.
 *
 * Kernel radius defines maximum allowed distance. If radius is 0, only
 * perfect match is allowed. Radius of 1 gives a 3x3 kernel. Pixels are
 * equal if they both are black, or both are non-black.
 *
 * Return values:  -1 = Perfect match
 *                    0 = Deviation within kernel
 *                   >0 = Number of faulty pixels
 *//*--------------------------------------------------------------------*/
int compareBlackNonBlackImages(tcu::TestLog &log, const ConstPixelBufferAccess &test, const ConstPixelBufferAccess &ref,
                               const PixelBufferAccess &diffMask, int kernelRadius)
{
    return compareImages(log, test, ref, diffMask, kernelRadius, compareBlackNonBlackPixels);
}

/*--------------------------------------------------------------------*//*!
 * \brief Pixelwise comparison of two images.
 *
 * Kernel radius defines maximum allowed distance. If radius is 0, only
 * perfect match is allowed. Radius of 1 gives a 3x3 kernel. Pixels are
 * equal if they both are black, or both are non-black with color values
 * close to each other.
 *
 * Return values:  -1 = Perfect match
 *                    0 = Deviation within kernel
 *                   >0 = Number of faulty pixels
 *//*--------------------------------------------------------------------*/
int compareColoredImages(tcu::TestLog &log, const ConstPixelBufferAccess &test, const ConstPixelBufferAccess &ref,
                         const PixelBufferAccess &diffMask, int kernelRadius)
{
    return compareImages(log, test, ref, diffMask, kernelRadius, compareColoredPixels);
}

/*--------------------------------------------------------------------*//*!
 * \brief Overdraw check verification
 *
 * Check that image does not have at any point a
 * pixel with red component value > 0.5
 *
 * Return values:  false = area not filled, or leaking
 *//*--------------------------------------------------------------------*/
bool checkHalfFilledImageOverdraw(tcu::TestLog &log, const tcu::RenderTarget &m_renderTarget,
                                  const ConstPixelBufferAccess &image, const PixelBufferAccess &output)
{
    const int height = image.getHeight();
    const int width  = image.getWidth();

    bool faulty = false;

    tcu::clear(output, MASK_COLOR_OK);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            const tcu::IVec4 cTest = image.getPixelInt(x, y);

            const bool pixelValid = isBlack(cTest) || isHalfFilled(cTest) ||
                                    (m_renderTarget.getNumSamples() > 1 && isLessThanHalfFilled(cTest));

            if (!pixelValid)
            {
                output.setPixel(MASK_COLOR_FAIL, x, y);
                faulty = true;
            }
        }
    }

    if (faulty)
        log << TestLog::Message << "Faulty pixel(s) found." << TestLog::EndMessage;

    return !faulty;
}

void checkPointSize(const glw::Functions &gl, float pointSize)
{
    GLfloat pointSizeRange[2] = {0, 0};
    gl.getFloatv(GL_ALIASED_POINT_SIZE_RANGE, pointSizeRange);
    if (pointSizeRange[1] < pointSize)
        throw tcu::NotSupportedError("Maximum point size is too low for this test");
}

void checkLineWidth(const glw::Functions &gl, float lineWidth)
{
    GLfloat lineWidthRange[2] = {0, 0};
    gl.getFloatv(GL_ALIASED_LINE_WIDTH_RANGE, lineWidthRange);
    if (lineWidthRange[1] < lineWidth)
        throw tcu::NotSupportedError("Maximum line width is too low for this test");
}

tcu::Vec3 IVec3ToVec3(const tcu::IVec3 &v)
{
    return tcu::Vec3((float)v.x(), (float)v.y(), (float)v.z());
}

bool pointOnTriangle(const tcu::IVec3 &p, const tcu::IVec3 &t0, const tcu::IVec3 &t1, const tcu::IVec3 &t2)
{
    // Must be on the plane
    const tcu::IVec3 n = tcu::cross(t1 - t0, t2 - t0);
    const tcu::IVec3 d = (p - t0);

    if (tcu::dot(n, d))
        return false;

    // Must be within the triangle area
    if (deSign32(tcu::dot(n, tcu::cross(t1 - t0, p - t0))) == deSign32(tcu::dot(n, tcu::cross(t2 - t0, p - t0))))
        return false;
    if (deSign32(tcu::dot(n, tcu::cross(t2 - t1, p - t1))) == deSign32(tcu::dot(n, tcu::cross(t0 - t1, p - t1))))
        return false;
    if (deSign32(tcu::dot(n, tcu::cross(t0 - t2, p - t2))) == deSign32(tcu::dot(n, tcu::cross(t1 - t2, p - t2))))
        return false;

    return true;
}

bool pointsOnLine(const tcu::IVec2 &t0, const tcu::IVec2 &t1, const tcu::IVec2 &t2)
{
    return (t1 - t0).x() * (t2 - t0).y() - (t2 - t0).x() * (t1 - t0).y() == 0;
}

// returns true for cases where polygon is (almost) along xz or yz planes (normal.z < 0.1)
// \note[jarkko] Doesn't have to be accurate, just to detect some obviously bad cases
bool twoPointClippedTriangleInvisible(const tcu::Vec3 &p, const tcu::IVec3 &dir1, const tcu::IVec3 &dir2)
{
    // fixed-point-like coords
    const int64_t fixedScale         = 64;
    const int64_t farValue           = 1024;
    const tcu::Vector<int64_t, 3> d1 = tcu::Vector<int64_t, 3>(dir1.x(), dir1.y(), dir1.z());
    const tcu::Vector<int64_t, 3> d2 = tcu::Vector<int64_t, 3>(dir2.x(), dir2.y(), dir2.z());
    const tcu::Vector<int64_t, 3> pfixed =
        tcu::Vector<int64_t, 3>(deFloorFloatToInt32(p.x() * fixedScale), deFloorFloatToInt32(p.y() * fixedScale),
                                deFloorFloatToInt32(p.z() * fixedScale));
    const tcu::Vector<int64_t, 3> normalDir = tcu::cross(d1 * farValue - pfixed, d2 * farValue - pfixed);
    const int64_t normalLen2                = tcu::lengthSquared(normalDir);

    return (normalDir.z() * normalDir.z() - normalLen2 / 100) < 0;
}

std::string genClippingPointInfoString(const tcu::Vec4 &p)
{
    std::ostringstream msg;

    if (p.x() < -p.w())
        msg << "\t(-X clip)";
    if (p.x() > p.w())
        msg << "\t(+X clip)";
    if (p.y() < -p.w())
        msg << "\t(-Y clip)";
    if (p.y() > p.w())
        msg << "\t(+Y clip)";
    if (p.z() < -p.w())
        msg << "\t(-Z clip)";
    if (p.z() > p.w())
        msg << "\t(+Z clip)";

    return msg.str();
}

std::string genColorString(const tcu::Vec4 &p)
{
    const tcu::Vec4 white(1.0f, 1.0f, 1.0f, 1.0f);
    const tcu::Vec4 red(1.0f, 0.0f, 0.0f, 1.0f);
    const tcu::Vec4 yellow(1.0f, 1.0f, 0.0f, 1.0f);
    const tcu::Vec4 blue(0.0f, 0.0f, 1.0f, 1.0f);

    if (p == white)
        return "(white)";
    if (p == red)
        return "(red)";
    if (p == yellow)
        return "(yellow)";
    if (p == blue)
        return "(blue)";
    return "";
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
                          << sglr::pdec::VertexAttribute("a_pointSize", rr::GENERICVECTYPE_FLOAT)
                          << sglr::pdec::VertexToFragmentVarying(rr::GENERICVECTYPE_FLOAT)
                          << sglr::pdec::FragmentOutput(rr::GENERICVECTYPE_FLOAT)
                          << sglr::pdec::VertexSource(shaderSourceVertex)
                          << sglr::pdec::FragmentSource(shaderSourceFragment))
{
}

void PositionColorShader::shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets,
                                        const int numPackets) const
{
    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
    {
        const int positionAttrLoc  = 0;
        const int colorAttrLoc     = 1;
        const int pointSizeAttrLoc = 2;

        rr::VertexPacket &packet = *packets[packetNdx];

        // Transform to position
        packet.position = rr::readVertexAttribFloat(inputs[positionAttrLoc], packet.instanceNdx, packet.vertexNdx);

        // output point size
        packet.pointSize =
            rr::readVertexAttribFloat(inputs[pointSizeAttrLoc], packet.instanceNdx, packet.vertexNdx).x();

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
                                    rr::readVarying<float>(packet, context, VARYINGLOC_COLOR, fragNdx));
    }
}

class RenderTestCase : public TestCase
{
public:
    RenderTestCase(Context &context, const char *name, const char *description);

    virtual void testRender(void) = 0;
    virtual void init(void)
    {
    }

    IterateResult iterate(void);
};

RenderTestCase::RenderTestCase(Context &context, const char *name, const char *description)
    : TestCase(context, name, description)
{
}

RenderTestCase::IterateResult RenderTestCase::iterate(void)
{
    const int width  = m_context.getRenderTarget().getWidth();
    const int height = m_context.getRenderTarget().getHeight();

    m_testCtx.getLog() << TestLog::Message << "Render target size: " << width << "x" << height << TestLog::EndMessage;
    if (width < TEST_CANVAS_SIZE || height < TEST_CANVAS_SIZE)
        throw tcu::NotSupportedError(std::string("Render target size must be at least ") +
                                     de::toString(TEST_CANVAS_SIZE) + "x" + de::toString(TEST_CANVAS_SIZE));

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass"); // success by default
    testRender();

    return STOP;
}

class PointCase : public RenderTestCase
{
public:
    PointCase(Context &context, const char *name, const char *description, const tcu::Vec4 *pointsBegin,
              const tcu::Vec4 *pointsEnd, float pointSize, const rr::WindowRectangle &viewport);

    void init(void);
    void testRender(void);

private:
    const std::vector<tcu::Vec4> m_points;
    const float m_pointSize;
    const rr::WindowRectangle m_viewport;
};

PointCase::PointCase(Context &context, const char *name, const char *description, const tcu::Vec4 *pointsBegin,
                     const tcu::Vec4 *pointsEnd, float pointSize, const rr::WindowRectangle &viewport)
    : RenderTestCase(context, name, description)
    , m_points(pointsBegin, pointsEnd)
    , m_pointSize(pointSize)
    , m_viewport(viewport)
{
}

void PointCase::init(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    checkPointSize(gl, m_pointSize);
}

void PointCase::testRender(void)
{
    using tcu::TestLog;

    const int numSamples = de::max(m_context.getRenderTarget().getNumSamples(), 1);

    tcu::TestLog &log = m_testCtx.getLog();
    sglr::GLContext glesContext(m_context.getRenderContext(), log, 0,
                                tcu::IVec4(0, 0, TEST_CANVAS_SIZE, TEST_CANVAS_SIZE));
    sglr::ReferenceContextLimits limits;
    sglr::ReferenceContextBuffers buffers(m_context.getRenderTarget().getPixelFormat(),
                                          m_context.getRenderTarget().getDepthBits(), 0, TEST_CANVAS_SIZE,
                                          TEST_CANVAS_SIZE, numSamples);
    sglr::ReferenceContext refContext(limits, buffers.getColorbuffer(), buffers.getDepthbuffer(),
                                      buffers.getStencilbuffer());
    PositionColorShader program;
    tcu::Surface testSurface(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
    tcu::Surface refSurface(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
    sglr::Context *contexts[2] = {&glesContext, &refContext};
    tcu::Surface *surfaces[2]  = {&testSurface, &refSurface};

    // log the purpose of the test
    log << TestLog::Message << "Viewport: left=" << m_viewport.left << "\tbottom=" << m_viewport.bottom
        << "\twidth=" << m_viewport.width << "\theight=" << m_viewport.height << TestLog::EndMessage;
    log << TestLog::Message << "Rendering points with point size " << m_pointSize
        << ". Coordinates:" << TestLog::EndMessage;
    for (size_t ndx = 0; ndx < m_points.size(); ++ndx)
        log << TestLog::Message << "\tx=" << m_points[ndx].x() << "\ty=" << m_points[ndx].y()
            << "\tz=" << m_points[ndx].z() << "\tw=" << m_points[ndx].w() << "\t"
            << genClippingPointInfoString(m_points[ndx]) << TestLog::EndMessage;

    for (int contextNdx = 0; contextNdx < 2; ++contextNdx)
    {
        sglr::Context &ctx       = *contexts[contextNdx];
        tcu::Surface &dstSurface = *surfaces[contextNdx];
        const uint32_t programId = ctx.createProgram(&program);
        const GLint positionLoc  = ctx.getAttribLocation(programId, "a_position");
        const GLint pointSizeLoc = ctx.getAttribLocation(programId, "a_pointSize");
        const GLint colorLoc     = ctx.getAttribLocation(programId, "a_color");

        ctx.clearColor(0, 0, 0, 1);
        ctx.clearDepthf(1.0f);
        ctx.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        ctx.viewport(m_viewport.left, m_viewport.bottom, m_viewport.width, m_viewport.height);
        ctx.useProgram(programId);
        ctx.enableVertexAttribArray(positionLoc);
        ctx.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, &m_points[0]);
        ctx.vertexAttrib1f(pointSizeLoc, m_pointSize);
        ctx.vertexAttrib4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
        ctx.drawArrays(GL_POINTS, 0, (glw::GLsizei)m_points.size());
        ctx.disableVertexAttribArray(positionLoc);
        ctx.useProgram(0);
        ctx.deleteProgram(programId);
        ctx.finish();

        ctx.readPixels(dstSurface, 0, 0, TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
    }

    // do the comparison
    {
        tcu::Surface diffMask(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
        const int kernelRadius = 1;
        int faultyPixels;

        log << TestLog::Message << "Comparing images... " << TestLog::EndMessage;
        log << TestLog::Message << "Deviation within radius of " << kernelRadius << " is allowed."
            << TestLog::EndMessage;

        faultyPixels = compareBlackNonBlackImages(log, testSurface.getAccess(), refSurface.getAccess(),
                                                  diffMask.getAccess(), kernelRadius);

        if (faultyPixels > 0)
        {
            log << TestLog::ImageSet("Images", "Image comparison")
                << TestLog::Image("TestImage", "Test image", testSurface.getAccess())
                << TestLog::Image("ReferenceImage", "Reference image", refSurface.getAccess())
                << TestLog::Image("DifferenceMask", "Difference mask", diffMask.getAccess()) << TestLog::EndImageSet
                << tcu::TestLog::Message << "Got " << faultyPixels << " faulty pixel(s)." << tcu::TestLog::EndMessage;

            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got faulty pixels");
        }
    }
}

class LineRenderTestCase : public RenderTestCase
{
public:
    struct ColoredLineData
    {
        tcu::Vec4 p0;
        tcu::Vec4 c0;
        tcu::Vec4 p1;
        tcu::Vec4 c1;
    };

    struct ColorlessLineData
    {
        tcu::Vec4 p0;
        tcu::Vec4 p1;
    };
    LineRenderTestCase(Context &context, const char *name, const char *description, const ColoredLineData *linesBegin,
                       const ColoredLineData *linesEnd, float lineWidth, const rr::WindowRectangle &viewport);
    LineRenderTestCase(Context &context, const char *name, const char *description, const ColorlessLineData *linesBegin,
                       const ColorlessLineData *linesEnd, float lineWidth, const rr::WindowRectangle &viewport);

    virtual void verifyImage(const tcu::ConstPixelBufferAccess &testImageAccess,
                             const tcu::ConstPixelBufferAccess &referenceImageAccess) = 0;
    void init(void);
    void testRender(void);

protected:
    const float m_lineWidth;

private:
    std::vector<ColoredLineData> convertToColoredLines(const ColorlessLineData *linesBegin,
                                                       const ColorlessLineData *linesEnd);

    const std::vector<ColoredLineData> m_lines;
    const rr::WindowRectangle m_viewport;
};

LineRenderTestCase::LineRenderTestCase(Context &context, const char *name, const char *description,
                                       const ColoredLineData *linesBegin, const ColoredLineData *linesEnd,
                                       float lineWidth, const rr::WindowRectangle &viewport)
    : RenderTestCase(context, name, description)
    , m_lineWidth(lineWidth)
    , m_lines(linesBegin, linesEnd)
    , m_viewport(viewport)
{
}

LineRenderTestCase::LineRenderTestCase(Context &context, const char *name, const char *description,
                                       const ColorlessLineData *linesBegin, const ColorlessLineData *linesEnd,
                                       float lineWidth, const rr::WindowRectangle &viewport)
    : RenderTestCase(context, name, description)
    , m_lineWidth(lineWidth)
    , m_lines(convertToColoredLines(linesBegin, linesEnd))
    , m_viewport(viewport)
{
}

void LineRenderTestCase::init(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    checkLineWidth(gl, m_lineWidth);
}

void LineRenderTestCase::testRender(void)
{
    using tcu::TestLog;

    const int numSamples      = de::max(m_context.getRenderTarget().getNumSamples(), 1);
    const int verticesPerLine = 2;

    tcu::TestLog &log = m_testCtx.getLog();
    sglr::GLContext glesContext(m_context.getRenderContext(), log, 0,
                                tcu::IVec4(0, 0, TEST_CANVAS_SIZE, TEST_CANVAS_SIZE));
    sglr::ReferenceContextLimits limits;
    sglr::ReferenceContextBuffers buffers(m_context.getRenderTarget().getPixelFormat(),
                                          m_context.getRenderTarget().getDepthBits(), 0, TEST_CANVAS_SIZE,
                                          TEST_CANVAS_SIZE, numSamples);
    sglr::ReferenceContext refContext(limits, buffers.getColorbuffer(), buffers.getDepthbuffer(),
                                      buffers.getStencilbuffer());
    PositionColorShader program;
    tcu::Surface testSurface(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
    tcu::Surface refSurface(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
    sglr::Context *contexts[2] = {&glesContext, &refContext};
    tcu::Surface *surfaces[2]  = {&testSurface, &refSurface};

    // log the purpose of the test
    log << TestLog::Message << "Viewport: left=" << m_viewport.left << "\tbottom=" << m_viewport.bottom
        << "\twidth=" << m_viewport.width << "\theight=" << m_viewport.height << TestLog::EndMessage;
    log << TestLog::Message << "Rendering lines with line width " << m_lineWidth
        << ". Coordinates:" << TestLog::EndMessage;
    for (size_t ndx = 0; ndx < m_lines.size(); ++ndx)
    {
        const std::string fromProperties = genClippingPointInfoString(m_lines[ndx].p0);
        const std::string toProperties   = genClippingPointInfoString(m_lines[ndx].p1);

        log << TestLog::Message << "\tfrom (x=" << m_lines[ndx].p0.x() << "\ty=" << m_lines[ndx].p0.y()
            << "\tz=" << m_lines[ndx].p0.z() << "\tw=" << m_lines[ndx].p0.w() << ")\t" << fromProperties
            << TestLog::EndMessage;
        log << TestLog::Message << "\tto   (x=" << m_lines[ndx].p1.x() << "\ty=" << m_lines[ndx].p1.y()
            << "\tz=" << m_lines[ndx].p1.z() << "\tw=" << m_lines[ndx].p1.w() << ")\t" << toProperties
            << TestLog::EndMessage;
        log << TestLog::Message << TestLog::EndMessage;
    }

    // render test image
    for (int contextNdx = 0; contextNdx < 2; ++contextNdx)
    {
        sglr::Context &ctx       = *contexts[contextNdx];
        tcu::Surface &dstSurface = *surfaces[contextNdx];
        const uint32_t programId = ctx.createProgram(&program);
        const GLint positionLoc  = ctx.getAttribLocation(programId, "a_position");
        const GLint colorLoc     = ctx.getAttribLocation(programId, "a_color");

        ctx.clearColor(0, 0, 0, 1);
        ctx.clearDepthf(1.0f);
        ctx.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        ctx.viewport(m_viewport.left, m_viewport.bottom, m_viewport.width, m_viewport.height);
        ctx.useProgram(programId);
        ctx.enableVertexAttribArray(positionLoc);
        ctx.enableVertexAttribArray(colorLoc);
        ctx.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat[8]), &m_lines[0].p0);
        ctx.vertexAttribPointer(colorLoc, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat[8]), &m_lines[0].c0);
        ctx.lineWidth(m_lineWidth);
        ctx.drawArrays(GL_LINES, 0, verticesPerLine * (glw::GLsizei)m_lines.size());
        ctx.disableVertexAttribArray(positionLoc);
        ctx.disableVertexAttribArray(colorLoc);
        ctx.useProgram(0);
        ctx.deleteProgram(programId);
        ctx.finish();

        ctx.readPixels(dstSurface, 0, 0, TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
    }

    // compare
    verifyImage(testSurface.getAccess(), refSurface.getAccess());
}

std::vector<LineRenderTestCase::ColoredLineData> LineRenderTestCase::convertToColoredLines(
    const ColorlessLineData *linesBegin, const ColorlessLineData *linesEnd)
{
    std::vector<ColoredLineData> ret;

    for (const ColorlessLineData *it = linesBegin; it != linesEnd; ++it)
    {
        ColoredLineData r;

        r.p0 = (*it).p0;
        r.c0 = tcu::Vec4(1, 1, 1, 1);
        r.p1 = (*it).p1;
        r.c1 = tcu::Vec4(1, 1, 1, 1);

        ret.push_back(r);
    }

    return ret;
}

class LineCase : public LineRenderTestCase
{
public:
    LineCase(Context &context, const char *name, const char *description,
             const LineRenderTestCase::ColorlessLineData *linesBegin,
             const LineRenderTestCase::ColorlessLineData *linesEnd, float lineWidth,
             const rr::WindowRectangle &viewport, int searchKernelSize = 1);

    void verifyImage(const tcu::ConstPixelBufferAccess &testImageAccess,
                     const tcu::ConstPixelBufferAccess &referenceImageAccess);

private:
    const int m_searchKernelSize;
};

LineCase::LineCase(Context &context, const char *name, const char *description,
                   const LineRenderTestCase::ColorlessLineData *linesBegin,
                   const LineRenderTestCase::ColorlessLineData *linesEnd, float lineWidth,
                   const rr::WindowRectangle &viewport, int searchKernelSize)
    : LineRenderTestCase(context, name, description, linesBegin, linesEnd, lineWidth, viewport)
    , m_searchKernelSize(searchKernelSize)
{
}

void LineCase::verifyImage(const tcu::ConstPixelBufferAccess &testImageAccess,
                           const tcu::ConstPixelBufferAccess &referenceImageAccess)
{
    const int faultyLimit = 6;
    int faultyPixels;

    const bool isMsaa = m_context.getRenderTarget().getNumSamples() > 1;
    tcu::TestLog &log = m_testCtx.getLog();
    tcu::Surface diffMask(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);

    log << TestLog::Message << "Comparing images... " << TestLog::EndMessage;
    log << TestLog::Message << "Deviation within radius of " << m_searchKernelSize << " is allowed."
        << TestLog::EndMessage;
    log << TestLog::Message << faultyLimit << " faulty pixels are allowed." << TestLog::EndMessage;

    faultyPixels = compareBlackNonBlackImages(log, testImageAccess, referenceImageAccess, diffMask.getAccess(),
                                              m_searchKernelSize);

    if (faultyPixels > faultyLimit)
    {
        log << TestLog::ImageSet("Images", "Image comparison")
            << TestLog::Image("TestImage", "Test image", testImageAccess)
            << TestLog::Image("ReferenceImage", "Reference image", referenceImageAccess)
            << TestLog::Image("DifferenceMask", "Difference mask", diffMask.getAccess()) << TestLog::EndImageSet
            << tcu::TestLog::Message << "Got " << faultyPixels << " faulty pixel(s)." << tcu::TestLog::EndMessage;

        if (m_lineWidth != 1.0f && isMsaa)
        {
            log << TestLog::Message << "Wide line support is optional, reporting compatibility warning."
                << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_COMPATIBILITY_WARNING, "Wide line clipping failed");
        }
        else
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got faulty pixels");
    }
}

class ColoredLineCase : public LineRenderTestCase
{
public:
    ColoredLineCase(Context &context, const char *name, const char *description,
                    const LineRenderTestCase::ColoredLineData *linesBegin,
                    const LineRenderTestCase::ColoredLineData *linesEnd, float lineWidth,
                    const rr::WindowRectangle &viewport);

    void verifyImage(const tcu::ConstPixelBufferAccess &testImageAccess,
                     const tcu::ConstPixelBufferAccess &referenceImageAccess);
};

ColoredLineCase::ColoredLineCase(Context &context, const char *name, const char *description,
                                 const LineRenderTestCase::ColoredLineData *linesBegin,
                                 const LineRenderTestCase::ColoredLineData *linesEnd, float lineWidth,
                                 const rr::WindowRectangle &viewport)
    : LineRenderTestCase(context, name, description, linesBegin, linesEnd, lineWidth, viewport)
{
}

void ColoredLineCase::verifyImage(const tcu::ConstPixelBufferAccess &testImageAccess,
                                  const tcu::ConstPixelBufferAccess &referenceImageAccess)
{
    const bool msaa   = m_context.getRenderTarget().getNumSamples() > 1;
    tcu::TestLog &log = m_testCtx.getLog();

    if (!msaa)
    {
        const int kernelRadius = 1;
        const int faultyLimit  = 6;
        int faultyPixels;
        tcu::Surface diffMask(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);

        log << TestLog::Message << "Comparing images... " << TestLog::EndMessage;
        log << TestLog::Message << "Deviation within radius of " << kernelRadius << " is allowed."
            << TestLog::EndMessage;
        log << TestLog::Message << faultyLimit << " faulty pixels are allowed." << TestLog::EndMessage;

        faultyPixels =
            compareColoredImages(log, testImageAccess, referenceImageAccess, diffMask.getAccess(), kernelRadius);

        if (faultyPixels > faultyLimit)
        {
            log << TestLog::ImageSet("Images", "Image comparison")
                << TestLog::Image("TestImage", "Test image", testImageAccess)
                << TestLog::Image("ReferenceImage", "Reference image", referenceImageAccess)
                << TestLog::Image("DifferenceMask", "Difference mask", diffMask.getAccess()) << TestLog::EndImageSet
                << tcu::TestLog::Message << "Got " << faultyPixels << " faulty pixel(s)." << tcu::TestLog::EndMessage;

            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got faulty pixels");
        }
    }
    else
    {
        const float threshold = 0.3f;
        if (!tcu::fuzzyCompare(log, "Images", "", referenceImageAccess, testImageAccess, threshold,
                               tcu::COMPARE_LOG_ON_ERROR))
        {
            if (m_lineWidth != 1.0f)
            {
                log << TestLog::Message << "Wide line support is optional, reporting compatibility warning."
                    << TestLog::EndMessage;
                m_testCtx.setTestResult(QP_TEST_RESULT_COMPATIBILITY_WARNING, "Wide line clipping failed");
            }
            else
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got faulty pixels");
        }
    }
}

class TriangleCaseBase : public RenderTestCase
{
public:
    struct TriangleData
    {
        tcu::Vec4 p0;
        tcu::Vec4 c0;
        tcu::Vec4 p1;
        tcu::Vec4 c1;
        tcu::Vec4 p2;
        tcu::Vec4 c2;
    };

    TriangleCaseBase(Context &context, const char *name, const char *description, const TriangleData *polysBegin,
                     const TriangleData *polysEnd, const rr::WindowRectangle &viewport);

    virtual void verifyImage(const tcu::ConstPixelBufferAccess &testImageAccess,
                             const tcu::ConstPixelBufferAccess &referenceImageAccess) = 0;
    void testRender(void);

private:
    const std::vector<TriangleData> m_polys;
    const rr::WindowRectangle m_viewport;
};

TriangleCaseBase::TriangleCaseBase(Context &context, const char *name, const char *description,
                                   const TriangleData *polysBegin, const TriangleData *polysEnd,
                                   const rr::WindowRectangle &viewport)
    : RenderTestCase(context, name, description)
    , m_polys(polysBegin, polysEnd)
    , m_viewport(viewport)
{
}

void TriangleCaseBase::testRender(void)
{
    using tcu::TestLog;

    const int numSamples          = de::max(m_context.getRenderTarget().getNumSamples(), 1);
    const int verticesPerTriangle = 3;

    tcu::TestLog &log = m_testCtx.getLog();
    sglr::GLContext glesContext(m_context.getRenderContext(), log, 0,
                                tcu::IVec4(0, 0, TEST_CANVAS_SIZE, TEST_CANVAS_SIZE));
    sglr::ReferenceContextLimits limits;
    sglr::ReferenceContextBuffers buffers(m_context.getRenderTarget().getPixelFormat(),
                                          m_context.getRenderTarget().getDepthBits(), 0, TEST_CANVAS_SIZE,
                                          TEST_CANVAS_SIZE, numSamples);
    sglr::ReferenceContext refContext(limits, buffers.getColorbuffer(), buffers.getDepthbuffer(),
                                      buffers.getStencilbuffer());
    PositionColorShader program;
    tcu::Surface testSurface(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
    tcu::Surface refSurface(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
    sglr::Context *contexts[2] = {&glesContext, &refContext};
    tcu::Surface *surfaces[2]  = {&testSurface, &refSurface};

    // log the purpose of the test
    log << TestLog::Message << "Viewport: left=" << m_viewport.left << "\tbottom=" << m_viewport.bottom
        << "\twidth=" << m_viewport.width << "\theight=" << m_viewport.height << TestLog::EndMessage;
    log << TestLog::Message << "Rendering triangles. Coordinates:" << TestLog::EndMessage;
    for (size_t ndx = 0; ndx < m_polys.size(); ++ndx)
    {
        const std::string v0Properties = genClippingPointInfoString(m_polys[ndx].p0);
        const std::string v1Properties = genClippingPointInfoString(m_polys[ndx].p1);
        const std::string v2Properties = genClippingPointInfoString(m_polys[ndx].p2);
        const std::string c0Properties = genColorString(m_polys[ndx].c0);
        const std::string c1Properties = genColorString(m_polys[ndx].c1);
        const std::string c2Properties = genColorString(m_polys[ndx].c2);

        log << TestLog::Message << "\tv0 (x=" << m_polys[ndx].p0.x() << "\ty=" << m_polys[ndx].p0.y()
            << "\tz=" << m_polys[ndx].p0.z() << "\tw=" << m_polys[ndx].p0.w() << ")\t" << v0Properties << "\t"
            << c0Properties << TestLog::EndMessage;
        log << TestLog::Message << "\tv1 (x=" << m_polys[ndx].p1.x() << "\ty=" << m_polys[ndx].p1.y()
            << "\tz=" << m_polys[ndx].p1.z() << "\tw=" << m_polys[ndx].p1.w() << ")\t" << v1Properties << "\t"
            << c1Properties << TestLog::EndMessage;
        log << TestLog::Message << "\tv2 (x=" << m_polys[ndx].p2.x() << "\ty=" << m_polys[ndx].p2.y()
            << "\tz=" << m_polys[ndx].p2.z() << "\tw=" << m_polys[ndx].p2.w() << ")\t" << v2Properties << "\t"
            << c2Properties << TestLog::EndMessage;
        log << TestLog::Message << TestLog::EndMessage;
    }

    // render test image
    for (int contextNdx = 0; contextNdx < 2; ++contextNdx)
    {
        sglr::Context &ctx       = *contexts[contextNdx];
        tcu::Surface &dstSurface = *surfaces[contextNdx];
        const uint32_t programId = ctx.createProgram(&program);
        const GLint positionLoc  = ctx.getAttribLocation(programId, "a_position");
        const GLint colorLoc     = ctx.getAttribLocation(programId, "a_color");

        ctx.clearColor(0, 0, 0, 1);
        ctx.clearDepthf(1.0f);
        ctx.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        ctx.viewport(m_viewport.left, m_viewport.bottom, m_viewport.width, m_viewport.height);
        ctx.useProgram(programId);
        ctx.enableVertexAttribArray(positionLoc);
        ctx.enableVertexAttribArray(colorLoc);
        ctx.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat[8]), &m_polys[0].p0);
        ctx.vertexAttribPointer(colorLoc, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat[8]), &m_polys[0].c0);
        ctx.drawArrays(GL_TRIANGLES, 0, verticesPerTriangle * (glw::GLsizei)m_polys.size());
        ctx.disableVertexAttribArray(positionLoc);
        ctx.disableVertexAttribArray(colorLoc);
        ctx.useProgram(0);
        ctx.deleteProgram(programId);
        ctx.finish();

        ctx.readPixels(dstSurface, 0, 0, TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
    }

    verifyImage(testSurface.getAccess(), refSurface.getAccess());
}

class TriangleCase : public TriangleCaseBase
{
public:
    TriangleCase(Context &context, const char *name, const char *description, const TriangleData *polysBegin,
                 const TriangleData *polysEnd, const rr::WindowRectangle &viewport);

    void verifyImage(const tcu::ConstPixelBufferAccess &testImageAccess,
                     const tcu::ConstPixelBufferAccess &referenceImageAccess);
};

TriangleCase::TriangleCase(Context &context, const char *name, const char *description, const TriangleData *polysBegin,
                           const TriangleData *polysEnd, const rr::WindowRectangle &viewport)
    : TriangleCaseBase(context, name, description, polysBegin, polysEnd, viewport)
{
}

void TriangleCase::verifyImage(const tcu::ConstPixelBufferAccess &testImageAccess,
                               const tcu::ConstPixelBufferAccess &referenceImageAccess)
{
    const int kernelRadius = 1;
    const int faultyLimit  = 6;
    tcu::TestLog &log      = m_testCtx.getLog();
    tcu::Surface diffMask(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
    int faultyPixels;

    log << TestLog::Message << "Comparing images... " << TestLog::EndMessage;
    log << TestLog::Message << "Deviation within radius of " << kernelRadius << " is allowed." << TestLog::EndMessage;
    log << TestLog::Message << faultyLimit << " faulty pixels are allowed." << TestLog::EndMessage;

    faultyPixels =
        compareBlackNonBlackImages(log, testImageAccess, referenceImageAccess, diffMask.getAccess(), kernelRadius);

    if (faultyPixels > faultyLimit)
    {
        log << TestLog::ImageSet("Images", "Image comparison")
            << TestLog::Image("TestImage", "Test image", testImageAccess)
            << TestLog::Image("ReferenceImage", "Reference image", referenceImageAccess)
            << TestLog::Image("DifferenceMask", "Difference mask", diffMask.getAccess()) << TestLog::EndImageSet
            << tcu::TestLog::Message << "Got " << faultyPixels << " faulty pixel(s)." << tcu::TestLog::EndMessage;

        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got faulty pixels");
    }
}

class TriangleAttributeCase : public TriangleCaseBase
{
public:
    TriangleAttributeCase(Context &context, const char *name, const char *description, const TriangleData *polysBegin,
                          const TriangleData *polysEnd, const rr::WindowRectangle &viewport);

    void verifyImage(const tcu::ConstPixelBufferAccess &testImageAccess,
                     const tcu::ConstPixelBufferAccess &referenceImageAccess);
};

TriangleAttributeCase::TriangleAttributeCase(Context &context, const char *name, const char *description,
                                             const TriangleData *polysBegin, const TriangleData *polysEnd,
                                             const rr::WindowRectangle &viewport)
    : TriangleCaseBase(context, name, description, polysBegin, polysEnd, viewport)
{
}

void TriangleAttributeCase::verifyImage(const tcu::ConstPixelBufferAccess &testImageAccess,
                                        const tcu::ConstPixelBufferAccess &referenceImageAccess)
{
    const bool msaa   = m_context.getRenderTarget().getNumSamples() > 1;
    tcu::TestLog &log = m_testCtx.getLog();

    if (!msaa)
    {
        const int kernelRadius = 1;
        const int faultyLimit  = 6;
        int faultyPixels;
        tcu::Surface diffMask(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);

        log << TestLog::Message << "Comparing images... " << TestLog::EndMessage;
        log << TestLog::Message << "Deviation within radius of " << kernelRadius << " is allowed."
            << TestLog::EndMessage;
        log << TestLog::Message << faultyLimit << " faulty pixels are allowed." << TestLog::EndMessage;
        faultyPixels =
            compareColoredImages(log, testImageAccess, referenceImageAccess, diffMask.getAccess(), kernelRadius);

        if (faultyPixels > faultyLimit)
        {
            log << TestLog::ImageSet("Images", "Image comparison")
                << TestLog::Image("TestImage", "Test image", testImageAccess)
                << TestLog::Image("ReferenceImage", "Reference image", referenceImageAccess)
                << TestLog::Image("DifferenceMask", "Difference mask", diffMask.getAccess()) << TestLog::EndImageSet
                << tcu::TestLog::Message << "Got " << faultyPixels << " faulty pixel(s)." << tcu::TestLog::EndMessage;

            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got faulty pixels");
        }
    }
    else
    {
        const float threshold = 0.3f;
        if (!tcu::fuzzyCompare(log, "Images", "", referenceImageAccess, testImageAccess, threshold,
                               tcu::COMPARE_LOG_ON_ERROR))
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got faulty pixels");
    }
}

class FillTest : public RenderTestCase
{
public:
    FillTest(Context &context, const char *name, const char *description, const rr::WindowRectangle &viewport);

    virtual void render(sglr::Context &ctx) = 0;
    void testRender(void);

protected:
    const rr::WindowRectangle m_viewport;
};

FillTest::FillTest(Context &context, const char *name, const char *description, const rr::WindowRectangle &viewport)
    : RenderTestCase(context, name, description)
    , m_viewport(viewport)
{
}

void FillTest::testRender(void)
{
    using tcu::TestLog;

    const int numSamples = 1;

    tcu::TestLog &log = m_testCtx.getLog();
    sglr::GLContext glesContext(m_context.getRenderContext(), log, 0,
                                tcu::IVec4(0, 0, TEST_CANVAS_SIZE, TEST_CANVAS_SIZE));
    sglr::ReferenceContextLimits limits;
    sglr::ReferenceContextBuffers buffers(m_context.getRenderTarget().getPixelFormat(),
                                          m_context.getRenderTarget().getDepthBits(), 0, TEST_CANVAS_SIZE,
                                          TEST_CANVAS_SIZE, numSamples);
    sglr::ReferenceContext refContext(limits, buffers.getColorbuffer(), buffers.getDepthbuffer(),
                                      buffers.getStencilbuffer());
    tcu::Surface testSurface(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
    tcu::Surface refSurface(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);

    render(glesContext);
    glesContext.readPixels(testSurface, 0, 0, TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);

    render(refContext);
    refContext.readPixels(refSurface, 0, 0, TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);

    // check overdraw
    {
        bool overdrawOk;
        tcu::Surface outputImage(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);

        log << TestLog::Message << "Checking for overdraw " << TestLog::EndMessage;
        overdrawOk = checkHalfFilledImageOverdraw(log, m_context.getRenderTarget(), testSurface.getAccess(),
                                                  outputImage.getAccess());

        if (!overdrawOk)
        {
            log << TestLog::ImageSet("Images", "Image comparison")
                << TestLog::Image("TestImage", "Test image", testSurface.getAccess())
                << TestLog::Image("InvalidPixels", "Invalid pixels", outputImage.getAccess()) << TestLog::EndImageSet
                << tcu::TestLog::Message << "Got overdraw." << tcu::TestLog::EndMessage;

            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got overdraw");
        }
    }

    // compare & check missing pixels
    {
        const int kernelRadius = 1;
        tcu::Surface diffMask(TEST_CANVAS_SIZE, TEST_CANVAS_SIZE);
        int faultyPixels;

        log << TestLog::Message << "Comparing images... " << TestLog::EndMessage;
        log << TestLog::Message << "Deviation within radius of " << kernelRadius << " is allowed."
            << TestLog::EndMessage;

        blitImageOnBlackSurface(refSurface.getAccess(), refSurface.getAccess()); // makes images look right in Candy

        faultyPixels = compareBlackNonBlackImages(log, testSurface.getAccess(), refSurface.getAccess(),
                                                  diffMask.getAccess(), kernelRadius);

        if (faultyPixels > 0)
        {
            log << TestLog::ImageSet("Images", "Image comparison")
                << TestLog::Image("TestImage", "Test image", testSurface.getAccess())
                << TestLog::Image("ReferenceImage", "Reference image", refSurface.getAccess())
                << TestLog::Image("DifferenceMask", "Difference mask", diffMask.getAccess()) << TestLog::EndImageSet
                << tcu::TestLog::Message << "Got " << faultyPixels << " faulty pixel(s)." << tcu::TestLog::EndMessage;

            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got faulty pixels");
        }
    }
}

class TriangleFillTest : public FillTest
{
public:
    struct FillTriangle
    {
        tcu::Vec4 v0;
        tcu::Vec4 c0;
        tcu::Vec4 v1;
        tcu::Vec4 c1;
        tcu::Vec4 v2;
        tcu::Vec4 c2;
    };

    TriangleFillTest(Context &context, const char *name, const char *description, const rr::WindowRectangle &viewport);

    void render(sglr::Context &ctx);

protected:
    std::vector<FillTriangle> m_triangles;
};

TriangleFillTest::TriangleFillTest(Context &context, const char *name, const char *description,
                                   const rr::WindowRectangle &viewport)
    : FillTest(context, name, description, viewport)
{
}

void TriangleFillTest::render(sglr::Context &ctx)
{
    const int verticesPerTriangle = 3;
    PositionColorShader program;
    const uint32_t programId = ctx.createProgram(&program);
    const GLint positionLoc  = ctx.getAttribLocation(programId, "a_position");
    const GLint colorLoc     = ctx.getAttribLocation(programId, "a_color");
    tcu::TestLog &log        = m_testCtx.getLog();

    // log the purpose of the test
    log << TestLog::Message << "Viewport: left=" << m_viewport.left << "\tbottom=" << m_viewport.bottom
        << "\twidth=" << m_viewport.width << "\theight=" << m_viewport.height << TestLog::EndMessage;
    log << TestLog::Message << "Rendering triangles. Coordinates:" << TestLog::EndMessage;
    for (size_t ndx = 0; ndx < m_triangles.size(); ++ndx)
    {
        const std::string v0Properties = genClippingPointInfoString(m_triangles[ndx].v0);
        const std::string v1Properties = genClippingPointInfoString(m_triangles[ndx].v1);
        const std::string v2Properties = genClippingPointInfoString(m_triangles[ndx].v2);

        log << TestLog::Message << "\tv0 (x=" << m_triangles[ndx].v0.x() << "\ty=" << m_triangles[ndx].v0.y()
            << "\tz=" << m_triangles[ndx].v0.z() << "\tw=" << m_triangles[ndx].v0.w() << ")\t" << v0Properties
            << TestLog::EndMessage;
        log << TestLog::Message << "\tv1 (x=" << m_triangles[ndx].v1.x() << "\ty=" << m_triangles[ndx].v1.y()
            << "\tz=" << m_triangles[ndx].v1.z() << "\tw=" << m_triangles[ndx].v1.w() << ")\t" << v1Properties
            << TestLog::EndMessage;
        log << TestLog::Message << "\tv2 (x=" << m_triangles[ndx].v2.x() << "\ty=" << m_triangles[ndx].v2.y()
            << "\tz=" << m_triangles[ndx].v2.z() << "\tw=" << m_triangles[ndx].v2.w() << ")\t" << v2Properties
            << TestLog::EndMessage;
        log << TestLog::Message << TestLog::EndMessage;
    }

    ctx.clearColor(0, 0, 0, 1);
    ctx.clearDepthf(1.0f);
    ctx.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ctx.viewport(m_viewport.left, m_viewport.bottom, m_viewport.width, m_viewport.height);
    ctx.useProgram(programId);
    ctx.blendFunc(GL_ONE, GL_ONE);
    ctx.enable(GL_BLEND);
    ctx.enableVertexAttribArray(positionLoc);
    ctx.enableVertexAttribArray(colorLoc);
    ctx.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat[8]), &m_triangles[0].v0);
    ctx.vertexAttribPointer(colorLoc, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat[8]), &m_triangles[0].c0);
    ctx.drawArrays(GL_TRIANGLES, 0, verticesPerTriangle * (glw::GLsizei)m_triangles.size());
    ctx.disableVertexAttribArray(positionLoc);
    ctx.disableVertexAttribArray(colorLoc);
    ctx.useProgram(0);
    ctx.deleteProgram(programId);
    ctx.finish();
}

class QuadFillTest : public TriangleFillTest
{
public:
    QuadFillTest(Context &context, const char *name, const char *description, const rr::WindowRectangle &viewport,
                 const tcu::Vec3 &d1, const tcu::Vec3 &d2, const tcu::Vec3 &center_ = tcu::Vec3(0, 0, 0));
};

QuadFillTest::QuadFillTest(Context &context, const char *name, const char *description,
                           const rr::WindowRectangle &viewport, const tcu::Vec3 &d1, const tcu::Vec3 &d2,
                           const tcu::Vec3 &center_)
    : TriangleFillTest(context, name, description, viewport)
{
    const float radius        = 40000.0f;
    const tcu::Vec4 center    = tcu::Vec4(center_.x(), center_.y(), center_.z(), 1.0f);
    const tcu::Vec4 halfWhite = tcu::Vec4(0.5f, 0.5f, 0.5f, 0.5f);
    const tcu::Vec4 halfRed   = tcu::Vec4(0.5f, 0.0f, 0.0f, 0.5f);
    const tcu::Vec4 e1        = radius * tcu::Vec4(d1.x(), d1.y(), d1.z(), 0.0f);
    const tcu::Vec4 e2        = radius * tcu::Vec4(d2.x(), d2.y(), d2.z(), 0.0f);

    FillTriangle triangle1;
    FillTriangle triangle2;

    triangle1.c0 = halfWhite;
    triangle1.c1 = halfWhite;
    triangle1.c2 = halfWhite;
    triangle1.v0 = center + e1 + e2;
    triangle1.v1 = center + e1 - e2;
    triangle1.v2 = center - e1 - e2;
    m_triangles.push_back(triangle1);

    triangle2.c0 = halfRed;
    triangle2.c1 = halfRed;
    triangle2.c2 = halfRed;
    triangle2.v0 = center + e1 + e2;
    triangle2.v1 = center - e1 - e2;
    triangle2.v2 = center - e1 + e2;
    m_triangles.push_back(triangle2);
}

class TriangleFanFillTest : public TriangleFillTest
{
public:
    TriangleFanFillTest(Context &context, const char *name, const char *description,
                        const rr::WindowRectangle &viewport);
};

TriangleFanFillTest::TriangleFanFillTest(Context &context, const char *name, const char *description,
                                         const rr::WindowRectangle &viewport)
    : TriangleFillTest(context, name, description, viewport)
{
    const float radius            = 70000.0f;
    const int trianglesPerVisit   = 40;
    const tcu::Vec4 center        = tcu::Vec4(0, 0, 0, 1.0f);
    const tcu::Vec4 halfWhite     = tcu::Vec4(0.5f, 0.5f, 0.5f, 0.5f);
    const tcu::Vec4 oddSliceColor = tcu::Vec4(0.0f, 0.0f, 0.5f, 0.0f);

    // create a continuous surface that goes through all 6 clip planes

    /*
     *   /           /
     *  /_ _ _ _ _  /x
     * |           |  |
     * |           | /
     * |       / --xe /
     * |      |    | /
     * |_ _ _ e _ _|/
     *
     * e = enter
     * x = exit
     */
    const struct ClipPlaneVisit
    {
        const tcu::Vec3 corner;
        const tcu::Vec3 entryPoint;
        const tcu::Vec3 exitPoint;
    } visits[] = {
        {tcu::Vec3(1, 1, 1), tcu::Vec3(0, 1, 1), tcu::Vec3(1, 0, 1)},
        {tcu::Vec3(1, -1, 1), tcu::Vec3(1, 0, 1), tcu::Vec3(1, -1, 0)},
        {tcu::Vec3(1, -1, -1), tcu::Vec3(1, -1, 0), tcu::Vec3(0, -1, -1)},
        {tcu::Vec3(-1, -1, -1), tcu::Vec3(0, -1, -1), tcu::Vec3(-1, 0, -1)},
        {tcu::Vec3(-1, 1, -1), tcu::Vec3(-1, 0, -1), tcu::Vec3(-1, 1, 0)},
        {tcu::Vec3(-1, 1, 1), tcu::Vec3(-1, 1, 0), tcu::Vec3(0, 1, 1)},
    };

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(visits); ++ndx)
    {
        const ClipPlaneVisit &visit = visits[ndx];

        for (int tri = 0; tri < trianglesPerVisit; ++tri)
        {
            tcu::Vec3 vertex0;
            tcu::Vec3 vertex1;

            if (tri == 0) // first vertex is magic
            {
                vertex0 = visit.entryPoint;
            }
            else
            {
                const tcu::Vec3 v1 = visit.entryPoint - visit.corner;
                const tcu::Vec3 v2 = visit.exitPoint - visit.corner;

                vertex0 = visit.corner + tcu::normalize(tcu::mix(v1, v2, tcu::Vec3(float(tri) / trianglesPerVisit)));
            }

            if (tri == trianglesPerVisit - 1) // last vertex is magic
            {
                vertex1 = visit.exitPoint;
            }
            else
            {
                const tcu::Vec3 v1 = visit.entryPoint - visit.corner;
                const tcu::Vec3 v2 = visit.exitPoint - visit.corner;

                vertex1 =
                    visit.corner + tcu::normalize(tcu::mix(v1, v2, tcu::Vec3(float(tri + 1) / trianglesPerVisit)));
            }

            // write vec out
            {
                FillTriangle triangle;

                triangle.c0 = (tri % 2) ? halfWhite : halfWhite + oddSliceColor;
                triangle.c1 = (tri % 2) ? halfWhite : halfWhite + oddSliceColor;
                triangle.c2 = (tri % 2) ? halfWhite : halfWhite + oddSliceColor;
                triangle.v0 = center;
                triangle.v1 = tcu::Vec4(vertex0.x() * radius, vertex0.y() * radius, vertex0.z() * radius, 1.0f);
                triangle.v2 = tcu::Vec4(vertex1.x() * radius, vertex1.y() * radius, vertex1.z() * radius, 1.0f);

                m_triangles.push_back(triangle);
            }
        }
    }
}

class PointsTestGroup : public TestCaseGroup
{
public:
    PointsTestGroup(Context &context);

    void init(void);
};

PointsTestGroup::PointsTestGroup(Context &context) : TestCaseGroup(context, "point", "Point clipping tests")
{
}

void PointsTestGroup::init(void)
{
    const float littleOverViewport =
        1.0f +
        (2.0f /
         (TEST_CANVAS_SIZE)); // one pixel over the viewport edge in VIEWPORT_WHOLE, half pixels over in the reduced viewport.

    const tcu::Vec4 viewportTestPoints[] = {
        // in clip volume
        tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(0.1f, 0.1f, 0.1f, 1.0f),
        tcu::Vec4(-0.1f, 0.1f, -0.1f, 1.0f),
        tcu::Vec4(-0.1f, -0.1f, 0.1f, 1.0f),
        tcu::Vec4(0.1f, -0.1f, -0.1f, 1.0f),

        // in clip volume with w != 1
        tcu::Vec4(2.0f, 2.0f, 2.0f, 3.0f),
        tcu::Vec4(-2.0f, -2.0f, 2.0f, 3.0f),
        tcu::Vec4(0.5f, -0.5f, 0.5f, 0.7f),
        tcu::Vec4(-0.5f, 0.5f, -0.5f, 0.7f),

        // near the edge
        tcu::Vec4(-2.0f, -2.0f, 0.0f, 2.2f),
        tcu::Vec4(1.0f, -1.0f, 0.0f, 1.1f),
        tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.1f),

        // not in the volume but still between near and far planes
        tcu::Vec4(1.3f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(-1.3f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(0.0f, 1.3f, 0.0f, 1.0f),
        tcu::Vec4(0.0f, -1.3f, 0.0f, 1.0f),

        tcu::Vec4(-1.3f, -1.3f, 0.0f, 1.0f),
        tcu::Vec4(-1.3f, 1.3f, 0.0f, 1.0f),
        tcu::Vec4(1.3f, 1.3f, 0.0f, 1.0f),
        tcu::Vec4(1.3f, -1.3f, 0.0f, 1.0f),

        // outside the viewport, wide points have fragments in the viewport
        tcu::Vec4(littleOverViewport, littleOverViewport, 0.0f, 1.0f),
        tcu::Vec4(0.0f, littleOverViewport, 0.0f, 1.0f),
        tcu::Vec4(littleOverViewport, 0.0f, 0.0f, 1.0f),
    };
    const tcu::Vec4 depthTestPoints[] = {// in clip volume
                                         tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(0.1f, 0.1f, 0.1f, 1.0f),
                                         tcu::Vec4(-0.1f, 0.1f, -0.1f, 1.0f), tcu::Vec4(-0.1f, -0.1f, 0.1f, 1.0f),
                                         tcu::Vec4(0.1f, -0.1f, -0.1f, 1.0f),

                                         // not between the near and the far planes. These should be clipped
                                         tcu::Vec4(0.1f, 0.0f, 1.1f, 1.0f), tcu::Vec4(-0.1f, 0.0f, -1.1f, 1.0f),
                                         tcu::Vec4(-0.0f, -0.1f, 1.1f, 1.0f), tcu::Vec4(0.0f, 0.1f, -1.1f, 1.0f)};

    addChild(new PointCase(m_context, "point_z_clip", "point z clipping", DE_ARRAY_BEGIN(depthTestPoints),
                           DE_ARRAY_END(depthTestPoints), 1.0f, VIEWPORT_WHOLE));
    addChild(new PointCase(m_context, "point_z_clip_viewport_center", "point z clipping",
                           DE_ARRAY_BEGIN(depthTestPoints), DE_ARRAY_END(depthTestPoints), 1.0f, VIEWPORT_CENTER));
    addChild(new PointCase(m_context, "point_z_clip_viewport_corner", "point z clipping",
                           DE_ARRAY_BEGIN(depthTestPoints), DE_ARRAY_END(depthTestPoints), 1.0f, VIEWPORT_CORNER));

    addChild(new PointCase(m_context, "point_clip_viewport_center", "point viewport clipping",
                           DE_ARRAY_BEGIN(viewportTestPoints), DE_ARRAY_END(viewportTestPoints), 1.0f,
                           VIEWPORT_CENTER));
    addChild(new PointCase(m_context, "point_clip_viewport_corner", "point viewport clipping",
                           DE_ARRAY_BEGIN(viewportTestPoints), DE_ARRAY_END(viewportTestPoints), 1.0f,
                           VIEWPORT_CORNER));

    addChild(new PointCase(m_context, "wide_point_z_clip", "point z clipping", DE_ARRAY_BEGIN(depthTestPoints),
                           DE_ARRAY_END(depthTestPoints), 5.0f, VIEWPORT_WHOLE));
    addChild(new PointCase(m_context, "wide_point_z_clip_viewport_center", "point z clipping",
                           DE_ARRAY_BEGIN(depthTestPoints), DE_ARRAY_END(depthTestPoints), 5.0f, VIEWPORT_CENTER));
    addChild(new PointCase(m_context, "wide_point_z_clip_viewport_corner", "point z clipping",
                           DE_ARRAY_BEGIN(depthTestPoints), DE_ARRAY_END(depthTestPoints), 5.0f, VIEWPORT_CORNER));

    addChild(new PointCase(m_context, "wide_point_clip", "point viewport clipping", DE_ARRAY_BEGIN(viewportTestPoints),
                           DE_ARRAY_END(viewportTestPoints), 5.0f, VIEWPORT_WHOLE));
    addChild(new PointCase(m_context, "wide_point_clip_viewport_center", "point viewport clipping",
                           DE_ARRAY_BEGIN(viewportTestPoints), DE_ARRAY_END(viewportTestPoints), 5.0f,
                           VIEWPORT_CENTER));
    addChild(new PointCase(m_context, "wide_point_clip_viewport_corner", "point viewport clipping",
                           DE_ARRAY_BEGIN(viewportTestPoints), DE_ARRAY_END(viewportTestPoints), 5.0f,
                           VIEWPORT_CORNER));
}

class LinesTestGroup : public TestCaseGroup
{
public:
    LinesTestGroup(Context &context);

    void init(void);
};

LinesTestGroup::LinesTestGroup(Context &context) : TestCaseGroup(context, "line", "Line clipping tests")
{
}

void LinesTestGroup::init(void)
{
    const float littleOverViewport =
        1.0f +
        (2.0f /
         (TEST_CANVAS_SIZE)); // one pixel over the viewport edge in VIEWPORT_WHOLE, half pixels over in the reduced viewport.

    // lines
    const LineRenderTestCase::ColorlessLineData viewportTestLines[] = {
        // from center to outside of viewport
        {tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 1.5f, 0.0f, 1.0f)},
        {tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(-1.5f, 1.0f, 0.0f, 1.0f)},
        {tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(-1.5f, 0.0f, 0.0f, 1.0f)},
        {tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(0.2f, 0.4f, 1.5f, 1.0f)},
        {tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(-2.0f, -1.0f, 0.0f, 1.0f)},
        {tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.1f, 0.0f, 0.6f)},

        // from outside to inside of viewport
        {tcu::Vec4(1.5f, 0.0f, 0.0f, 1.0f), tcu::Vec4(0.8f, -0.2f, 0.0f, 1.0f)},
        {tcu::Vec4(0.0f, -1.5f, 0.0f, 1.0f), tcu::Vec4(0.9f, -0.7f, 0.0f, 1.0f)},

        // from outside to outside
        {tcu::Vec4(0.0f, -1.3f, 0.0f, 1.0f), tcu::Vec4(1.3f, 0.0f, 0.0f, 1.0f)},

        // outside the viewport, wide lines have fragments in the viewport
        {tcu::Vec4(-0.8f, -littleOverViewport, 0.0f, 1.0f), tcu::Vec4(0.0f, -littleOverViewport, 0.0f, 1.0f)},
        {tcu::Vec4(-littleOverViewport - 1.0f, 0.0f, 0.0f, 1.0f),
         tcu::Vec4(0.0f, -littleOverViewport - 1.0f, 0.0f, 1.0f)},
    };
    const LineRenderTestCase::ColorlessLineData depthTestLines[] = {
        {tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.3f, 1.0f, 2.0f, 1.0f)},
        {tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.3f, -1.0f, 2.0f, 1.0f)},
        {tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(-1.0f, -1.1f, -2.0f, 1.0f)},
        {tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(-1.0f, 1.1f, -2.0f, 1.0f)},
        {tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.1f, 2.0f, 0.6f)},
    };
    const LineRenderTestCase::ColorlessLineData longTestLines[] = {
        {tcu::Vec4(-41000.0f, -40000.0f, -1000000.0f, 1.0f), tcu::Vec4(41000.0f, 40000.0f, 1000000.0f, 1.0f)},
        {tcu::Vec4(41000.0f, -40000.0f, 1000000.0f, 1.0f), tcu::Vec4(-41000.0f, 40000.0f, -1000000.0f, 1.0f)},
        {tcu::Vec4(0.5f, -40000.0f, 100000.0f, 1.0f), tcu::Vec4(0.5f, 40000.0f, -100000.0f, 1.0f)},
        {tcu::Vec4(-0.5f, 40000.0f, 100000.0f, 1.0f), tcu::Vec4(-0.5f, -40000.0f, -100000.0f, 1.0f)},
    };

    // line attribute clipping
    const tcu::Vec4 red(1.0f, 0.0f, 0.0f, 1.0f);
    const tcu::Vec4 yellow(1.0f, 1.0f, 0.0f, 1.0f);
    const tcu::Vec4 lightBlue(0.3f, 0.3f, 1.0f, 1.0f);
    const LineRenderTestCase::ColoredLineData colorTestLines[] = {
        {tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), red, tcu::Vec4(1.3f, 1.0f, 2.0f, 1.0f), yellow},
        {tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), red, tcu::Vec4(1.3f, -1.0f, 2.0f, 1.0f), lightBlue},
        {tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), red, tcu::Vec4(-1.0f, -1.0f, -2.0f, 1.0f), yellow},
        {tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), red, tcu::Vec4(-1.0f, 1.0f, -2.0f, 1.0f), lightBlue},
    };

    // line clipping
    addChild(new LineCase(m_context, "line_z_clip", "line z clipping", DE_ARRAY_BEGIN(depthTestLines),
                          DE_ARRAY_END(depthTestLines), 1.0f, VIEWPORT_WHOLE));
    addChild(new LineCase(m_context, "line_z_clip_viewport_center", "line z clipping", DE_ARRAY_BEGIN(depthTestLines),
                          DE_ARRAY_END(depthTestLines), 1.0f, VIEWPORT_CENTER));
    addChild(new LineCase(m_context, "line_z_clip_viewport_corner", "line z clipping", DE_ARRAY_BEGIN(depthTestLines),
                          DE_ARRAY_END(depthTestLines), 1.0f, VIEWPORT_CORNER));

    addChild(new LineCase(m_context, "line_clip_viewport_center", "line viewport clipping",
                          DE_ARRAY_BEGIN(viewportTestLines), DE_ARRAY_END(viewportTestLines), 1.0f, VIEWPORT_CENTER));
    addChild(new LineCase(m_context, "line_clip_viewport_corner", "line viewport clipping",
                          DE_ARRAY_BEGIN(viewportTestLines), DE_ARRAY_END(viewportTestLines), 1.0f, VIEWPORT_CORNER));

    addChild(new LineCase(m_context, "wide_line_z_clip", "line z clipping", DE_ARRAY_BEGIN(depthTestLines),
                          DE_ARRAY_END(depthTestLines), 5.0f, VIEWPORT_WHOLE));
    addChild(new LineCase(m_context, "wide_line_z_clip_viewport_center", "line z clipping",
                          DE_ARRAY_BEGIN(depthTestLines), DE_ARRAY_END(depthTestLines), 5.0f, VIEWPORT_CENTER));
    addChild(new LineCase(m_context, "wide_line_z_clip_viewport_corner", "line z clipping",
                          DE_ARRAY_BEGIN(depthTestLines), DE_ARRAY_END(depthTestLines), 5.0f, VIEWPORT_CORNER));

    addChild(new LineCase(m_context, "wide_line_clip", "line viewport clipping", DE_ARRAY_BEGIN(viewportTestLines),
                          DE_ARRAY_END(viewportTestLines), 5.0f, VIEWPORT_WHOLE));
    addChild(new LineCase(m_context, "wide_line_clip_viewport_center", "line viewport clipping",
                          DE_ARRAY_BEGIN(viewportTestLines), DE_ARRAY_END(viewportTestLines), 5.0f, VIEWPORT_CENTER));
    addChild(new LineCase(m_context, "wide_line_clip_viewport_corner", "line viewport clipping",
                          DE_ARRAY_BEGIN(viewportTestLines), DE_ARRAY_END(viewportTestLines), 5.0f, VIEWPORT_CORNER));

    addChild(new LineCase(m_context, "long_line_clip", "line viewport clipping", DE_ARRAY_BEGIN(longTestLines),
                          DE_ARRAY_END(longTestLines), 1.0f, VIEWPORT_WHOLE, 2));
    addChild(new LineCase(m_context, "long_wide_line_clip", "line viewport clipping", DE_ARRAY_BEGIN(longTestLines),
                          DE_ARRAY_END(longTestLines), 5.0f, VIEWPORT_WHOLE, 2));

    // line attribute clipping
    addChild(new ColoredLineCase(m_context, "line_attrib_clip", "line attribute clipping",
                                 DE_ARRAY_BEGIN(colorTestLines), DE_ARRAY_END(colorTestLines), 1.0f, VIEWPORT_WHOLE));
    addChild(new ColoredLineCase(m_context, "wide_line_attrib_clip", "line attribute clipping",
                                 DE_ARRAY_BEGIN(colorTestLines), DE_ARRAY_END(colorTestLines), 5.0f, VIEWPORT_WHOLE));
}

class PolysTestGroup : public TestCaseGroup
{
public:
    PolysTestGroup(Context &context);

    void init(void);
};

PolysTestGroup::PolysTestGroup(Context &context) : TestCaseGroup(context, "polygon", "Polygon clipping tests")
{
}

void PolysTestGroup::init(void)
{
    const float large  = 100000.0f;
    const float offset = 0.9f;
    const tcu::Vec4 white(1.0f, 1.0f, 1.0f, 1.0f);
    const tcu::Vec4 red(1.0f, 0.0f, 0.0f, 1.0f);
    const tcu::Vec4 yellow(1.0f, 1.0f, 0.0f, 1.0f);
    const tcu::Vec4 blue(0.0f, 0.0f, 1.0f, 1.0f);

    // basic cases
    {
        const TriangleCase::TriangleData viewportPolys[] = {
            // one vertex clipped
            {tcu::Vec4(-0.8f, -0.2f, 0.0f, 1.0f), white, tcu::Vec4(-0.8f, 0.2f, 0.0f, 1.0f), white,
             tcu::Vec4(-1.3f, 0.05f, 0.0f, 1.0f), white},

            // two vertices clipped
            {tcu::Vec4(-0.6f, -1.2f, 0.0f, 1.0f), white, tcu::Vec4(-1.2f, -0.6f, 0.0f, 1.0f), white,
             tcu::Vec4(-0.6f, -0.6f, 0.0f, 1.0f), white},

            // three vertices clipped
            {tcu::Vec4(-1.1f, 0.6f, 0.0f, 1.0f), white, tcu::Vec4(-1.1f, 1.1f, 0.0f, 1.0f), white,
             tcu::Vec4(-0.6f, 1.1f, 0.0f, 1.0f), white},
            {tcu::Vec4(0.8f, 1.1f, 0.0f, 1.0f), white, tcu::Vec4(0.95f, -1.1f, 0.0f, 1.0f), white,
             tcu::Vec4(3.0f, 0.0f, 0.0f, 1.0f), white},
        };
        const TriangleCase::TriangleData depthPolys[] = {
            // one vertex clipped to Z+
            {tcu::Vec4(-0.2f, 0.7f, 0.0f, 1.0f), white, tcu::Vec4(0.2f, 0.7f, 0.0f, 1.0f), white,
             tcu::Vec4(0.0f, 0.9f, 2.0f, 1.0f), white},

            // two vertices clipped to Z-
            {tcu::Vec4(0.9f, 0.4f, -1.5f, 1.0f), white, tcu::Vec4(0.9f, -0.4f, -1.5f, 1.0f), white,
             tcu::Vec4(0.6f, 0.0f, 0.0f, 1.0f), white},

            // three vertices clipped
            {tcu::Vec4(-0.9f, 0.6f, -2.0f, 1.0f), white, tcu::Vec4(-0.9f, -0.6f, -2.0f, 1.0f), white,
             tcu::Vec4(-0.4f, 0.0f, 2.0f, 1.0f), white},

            // three vertices clipped by X, Y and Z
            {tcu::Vec4(0.0f, -1.2f, 0.0f, 1.0f), white, tcu::Vec4(0.0f, 0.5f, -1.5f, 1.0f), white,
             tcu::Vec4(1.2f, -0.9f, 0.0f, 1.0f), white},
        };
        const TriangleCase::TriangleData largePolys[] = {
            // one vertex clipped
            {tcu::Vec4(-0.2f, -0.3f, 0.0f, 1.0f), white, tcu::Vec4(0.2f, -0.3f, 0.0f, 1.0f), white,
             tcu::Vec4(0.0f, -large, 2.0f, 1.0f), white},

            // two vertices clipped
            {tcu::Vec4(0.5f, 0.5f, 0.0f, 1.0f), white, tcu::Vec4(large, 0.5f, 0.0f, 1.0f), white,
             tcu::Vec4(0.5f, large, 0.0f, 1.0f), white},

            // three vertices clipped
            {tcu::Vec4(-0.9f, -large, 0.0f, 1.0f), white, tcu::Vec4(-1.1f, -large, 0.0f, 1.0f), white,
             tcu::Vec4(-0.9f, large, 0.0f, 1.0f), white},
        };
        const TriangleCase::TriangleData largeDepthPolys[] = {
            // one vertex clipped
            {tcu::Vec4(-0.2f, -0.3f, 0.0f, 1.0f), white, tcu::Vec4(0.2f, -0.3f, 0.0f, 1.0f), white,
             tcu::Vec4(0.0f, -large, large, 1.0f), white},

            // two vertices clipped
            {tcu::Vec4(0.5f, 0.5f, 0.0f, 1.0f), white, tcu::Vec4(0.9f, large / 2, -large, 1.0f), white,
             tcu::Vec4(large / 4, 0.0f, -large, 1.0f), white},

            // three vertices clipped
            {tcu::Vec4(-0.9f, large / 4, large, 1.0f), white, tcu::Vec4(-0.5f, -large / 4, -large, 1.0f), white,
             tcu::Vec4(-0.2f, large / 4, large, 1.0f), white},
        };
        const TriangleCase::TriangleData attribPolys[] = {
            // one vertex clipped to edge, large
            {tcu::Vec4(-0.2f, -0.3f, 0.0f, 1.0f), red, tcu::Vec4(0.2f, -0.3f, 0.0f, 1.0f), yellow,
             tcu::Vec4(0.0f, -large, 2.0f, 1.0f), blue},

            // two vertices clipped to edges
            {tcu::Vec4(0.6f, 1.2f, 0.0f, 1.0f), red, tcu::Vec4(1.2f, 0.6f, 0.0f, 1.0f), yellow,
             tcu::Vec4(0.6f, 0.6f, 0.0f, 1.0f), blue},

            // two vertices clipped to edges, with non-uniform w
            {tcu::Vec4(0.6f, -1.2f, 0.0f, 1.0f), red, tcu::Vec4(1.2f, -0.6f, 0.0f, 1.0f), yellow,
             16.0f * tcu::Vec4(0.6f, -0.6f, 0.0f, 1.0f), blue},

            // three vertices clipped, large, Z
            {tcu::Vec4(-0.9f, large / 4, large, 1.0f), red, tcu::Vec4(-0.5f, -large / 4, -large, 1.0f), yellow,
             tcu::Vec4(-0.2f, large / 4, large, 1.0f), blue},
        };

        addChild(new TriangleCase(m_context, "poly_clip_viewport_center", "polygon viewport clipping",
                                  DE_ARRAY_BEGIN(viewportPolys), DE_ARRAY_END(viewportPolys), VIEWPORT_CENTER));
        addChild(new TriangleCase(m_context, "poly_clip_viewport_corner", "polygon viewport clipping",
                                  DE_ARRAY_BEGIN(viewportPolys), DE_ARRAY_END(viewportPolys), VIEWPORT_CORNER));

        addChild(new TriangleCase(m_context, "poly_z_clip", "polygon z clipping", DE_ARRAY_BEGIN(depthPolys),
                                  DE_ARRAY_END(depthPolys), VIEWPORT_WHOLE));
        addChild(new TriangleCase(m_context, "poly_z_clip_viewport_center", "polygon z clipping",
                                  DE_ARRAY_BEGIN(depthPolys), DE_ARRAY_END(depthPolys), VIEWPORT_CENTER));
        addChild(new TriangleCase(m_context, "poly_z_clip_viewport_corner", "polygon z clipping",
                                  DE_ARRAY_BEGIN(depthPolys), DE_ARRAY_END(depthPolys), VIEWPORT_CORNER));

        addChild(new TriangleCase(m_context, "large_poly_clip_viewport_center", "polygon viewport clipping",
                                  DE_ARRAY_BEGIN(largePolys), DE_ARRAY_END(largePolys), VIEWPORT_CENTER));
        addChild(new TriangleCase(m_context, "large_poly_clip_viewport_corner", "polygon viewport clipping",
                                  DE_ARRAY_BEGIN(largePolys), DE_ARRAY_END(largePolys), VIEWPORT_CORNER));

        addChild(new TriangleCase(m_context, "large_poly_z_clip", "polygon z clipping", DE_ARRAY_BEGIN(largeDepthPolys),
                                  DE_ARRAY_END(largeDepthPolys), VIEWPORT_WHOLE));
        addChild(new TriangleCase(m_context, "large_poly_z_clip_viewport_center", "polygon z clipping",
                                  DE_ARRAY_BEGIN(largeDepthPolys), DE_ARRAY_END(largeDepthPolys), VIEWPORT_CENTER));
        addChild(new TriangleCase(m_context, "large_poly_z_clip_viewport_corner", "polygon z clipping",
                                  DE_ARRAY_BEGIN(largeDepthPolys), DE_ARRAY_END(largeDepthPolys), VIEWPORT_CORNER));

        addChild(new TriangleAttributeCase(m_context, "poly_attrib_clip", "polygon clipping",
                                           DE_ARRAY_BEGIN(attribPolys), DE_ARRAY_END(attribPolys), VIEWPORT_WHOLE));
        addChild(new TriangleAttributeCase(m_context, "poly_attrib_clip_viewport_center", "polygon clipping",
                                           DE_ARRAY_BEGIN(attribPolys), DE_ARRAY_END(attribPolys), VIEWPORT_CENTER));
        addChild(new TriangleAttributeCase(m_context, "poly_attrib_clip_viewport_corner", "polygon clipping",
                                           DE_ARRAY_BEGIN(attribPolys), DE_ARRAY_END(attribPolys), VIEWPORT_CORNER));
    }

    // multiple polygons
    {
        {
            const TriangleAttributeCase::TriangleData polys[] = {
                // one vertex clipped to edge
                {tcu::Vec4(-0.2f, -0.3f, 0.0f, 1.0f), red, tcu::Vec4(0.2f, -0.3f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.0f, -offset, 2.0f, 1.0f), blue},

                // two vertices clipped to edges
                {tcu::Vec4(0.6f, 1.2f, 0.0f, 1.0f), red, tcu::Vec4(1.2f, 0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.6f, 0.6f, 0.0f, 1.0f), blue},

                // two vertices clipped to edges, with non-uniform w
                {tcu::Vec4(0.6f, -1.2f, 0.0f, 1.0f), red, tcu::Vec4(1.2f, -0.6f, 0.0f, 1.0f), yellow,
                 16.0f * tcu::Vec4(0.6f, -0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(0.6f, 1.2f, 0.0f, 1.0f), red, tcu::Vec4(1.2f, 0.6f, 0.0f, 1.0f), yellow,
                 16.0f * tcu::Vec4(0.6f, 0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(-0.6f, 1.2f, 0.0f, 1.0f), red, tcu::Vec4(-1.2f, 0.6f, 0.0f, 1.0f), yellow,
                 16.0f * tcu::Vec4(-0.6f, 0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(-0.6f, -1.2f, 0.0f, 1.0f), red, tcu::Vec4(-1.2f, -0.6f, 0.0f, 1.0f), yellow,
                 16.0f * tcu::Vec4(-0.6f, -0.6f, 0.0f, 1.0f), blue},

                // three vertices clipped, Z
                {tcu::Vec4(-0.9f, offset / 4, offset, 1.0f), red, tcu::Vec4(-0.5f, -offset / 4, -offset, 1.0f), yellow,
                 tcu::Vec4(-0.2f, offset / 4, offset, 1.0f), blue},
            };

            addChild(new TriangleAttributeCase(m_context, "multiple_0", "polygon clipping", DE_ARRAY_BEGIN(polys),
                                               DE_ARRAY_END(polys), VIEWPORT_WHOLE));
            addChild(new TriangleAttributeCase(m_context, "multiple_0_viewport_center", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CENTER));
            addChild(new TriangleAttributeCase(m_context, "multiple_0_viewport_corner", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CORNER));
        }

        {
            const TriangleAttributeCase::TriangleData polys[] = {
                // one vertex clipped to z
                {tcu::Vec4(-0.2f, -0.3f, 0.0f, 1.0f), red, tcu::Vec4(0.2f, -0.3f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.0f, -offset, 2.0f, 1.0f), blue},

                // two vertices clipped to edges
                {tcu::Vec4(0.6f, 1.2f, 0.0f, 1.0f), red, tcu::Vec4(1.2f, 0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.6f, 0.6f, 0.0f, 1.0f), blue},

                // two vertices clipped to edges, with non-uniform w
                {tcu::Vec4(0.6f, -1.2f, 0.0f, 1.0f), red, tcu::Vec4(1.2f, -0.6f, 0.0f, 1.0f), yellow,
                 16.0f * tcu::Vec4(0.6f, -0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(0.6f, 1.2f, 0.0f, 1.0f), red, tcu::Vec4(1.2f, 0.6f, 0.0f, 1.0f), yellow,
                 16.0f * tcu::Vec4(0.6f, 0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(-0.6f, 1.2f, 0.0f, 1.0f), red, tcu::Vec4(-1.2f, 0.6f, 0.0f, 1.0f), yellow,
                 16.0f * tcu::Vec4(-0.6f, 0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(-0.6f, -1.2f, 0.0f, 1.0f), red, tcu::Vec4(-1.2f, -0.6f, 0.0f, 1.0f), yellow,
                 16.0f * tcu::Vec4(-0.6f, -0.6f, 0.0f, 1.0f), blue},
            };

            addChild(new TriangleAttributeCase(m_context, "multiple_1", "polygon clipping", DE_ARRAY_BEGIN(polys),
                                               DE_ARRAY_END(polys), VIEWPORT_WHOLE));
            addChild(new TriangleAttributeCase(m_context, "multiple_1_viewport_center", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CENTER));
            addChild(new TriangleAttributeCase(m_context, "multiple_1_viewport_corner", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CORNER));
        }

        {
            const TriangleAttributeCase::TriangleData polys[] = {
                // one vertex clipped to z
                {tcu::Vec4(-0.2f, -0.3f, 0.0f, 1.0f), red, tcu::Vec4(0.2f, -0.3f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.0f, -offset, 2.0f, 1.0f), blue},

                // two vertices clipped to edges
                {tcu::Vec4(0.6f, 1.2f, 0.0f, 1.0f), red, tcu::Vec4(1.2f, 0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.6f, 0.6f, 0.0f, 1.0f), blue},

                // two vertices clipped to edges
                {tcu::Vec4(0.6f, -1.2f, 0.0f, 1.0f), red, tcu::Vec4(1.2f, -0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.6f, -0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(0.6f, 1.2f, 0.0f, 1.0f), red, tcu::Vec4(1.2f, 0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.6f, 0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(-0.6f, 1.2f, 0.0f, 1.0f), red, tcu::Vec4(-1.2f, 0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(-0.6f, 0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(-0.6f, -1.2f, 0.0f, 1.0f), red, tcu::Vec4(-1.2f, -0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(-0.6f, -0.6f, 0.0f, 1.0f), blue},
            };

            addChild(new TriangleAttributeCase(m_context, "multiple_2", "polygon clipping", DE_ARRAY_BEGIN(polys),
                                               DE_ARRAY_END(polys), VIEWPORT_WHOLE));
            addChild(new TriangleAttributeCase(m_context, "multiple_2_viewport_center", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CENTER));
            addChild(new TriangleAttributeCase(m_context, "multiple_2_viewport_corner", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CORNER));
        }

        {
            const TriangleAttributeCase::TriangleData polys[] = {
                // one vertex clipped to z
                {tcu::Vec4(-0.2f, -0.3f, 0.0f, 1.0f), red, tcu::Vec4(0.2f, -0.3f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.0f, -offset, -2.0f, 1.0f), blue},

                // two vertices clipped to edges
                {tcu::Vec4(0.6f, -1.2f, 0.0f, 1.0f), red, tcu::Vec4(1.2f, -0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.6f, -0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(0.6f, 1.2f, 0.0f, 1.0f), red, tcu::Vec4(1.2f, 0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.6f, 0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(-0.6f, 1.2f, 0.0f, 1.0f), red, tcu::Vec4(-1.2f, 0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(-0.6f, 0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(-0.6f, -1.2f, 0.0f, 1.0f), red, tcu::Vec4(-1.2f, -0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(-0.6f, -0.6f, 0.0f, 1.0f), blue},
            };

            addChild(new TriangleAttributeCase(m_context, "multiple_3", "polygon clipping", DE_ARRAY_BEGIN(polys),
                                               DE_ARRAY_END(polys), VIEWPORT_WHOLE));
            addChild(new TriangleAttributeCase(m_context, "multiple_3_viewport_center", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CENTER));
            addChild(new TriangleAttributeCase(m_context, "multiple_3_viewport_corner", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CORNER));
        }

        {
            const TriangleAttributeCase::TriangleData polys[] = {
                // one vertex clipped to z
                {tcu::Vec4(0.3f, 0.2f, 0.0f, 1.0f), red, tcu::Vec4(0.3f, -0.2f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(offset, 0.0f, 2.0f, 1.0f), blue},

                // two vertices clipped to edges
                {tcu::Vec4(0.6f, -1.2f, 0.0f, 1.0f), red, tcu::Vec4(1.2f, -0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.6f, -0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(0.6f, 1.2f, 0.0f, 1.0f), red, tcu::Vec4(1.2f, 0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.6f, 0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(-0.6f, 1.2f, 0.0f, 1.0f), red, tcu::Vec4(-1.2f, 0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(-0.6f, 0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(-0.6f, -1.2f, 0.0f, 1.0f), red, tcu::Vec4(-1.2f, -0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(-0.6f, -0.6f, 0.0f, 1.0f), blue},
            };

            addChild(new TriangleAttributeCase(m_context, "multiple_4", "polygon clipping", DE_ARRAY_BEGIN(polys),
                                               DE_ARRAY_END(polys), VIEWPORT_WHOLE));
            addChild(new TriangleAttributeCase(m_context, "multiple_4_viewport_center", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CENTER));
            addChild(new TriangleAttributeCase(m_context, "multiple_4_viewport_corner", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CORNER));
        }

        {
            const TriangleAttributeCase::TriangleData polys[] = {
                // one vertex clipped to z
                {tcu::Vec4(-0.3f, 0.2f, 0.0f, 1.0f), red, tcu::Vec4(-0.3f, -0.2f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(-offset, 0.0f, 2.0f, 1.0f), blue},

                // two vertices clipped to edges
                {tcu::Vec4(0.6f, -1.2f, 0.0f, 1.0f), red, tcu::Vec4(1.2f, -0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.6f, -0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(0.6f, 1.2f, 0.0f, 1.0f), red, tcu::Vec4(1.2f, 0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.6f, 0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(-0.6f, 1.2f, 0.0f, 1.0f), red, tcu::Vec4(-1.2f, 0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(-0.6f, 0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(-0.6f, -1.2f, 0.0f, 1.0f), red, tcu::Vec4(-1.2f, -0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(-0.6f, -0.6f, 0.0f, 1.0f), blue},
            };

            addChild(new TriangleAttributeCase(m_context, "multiple_5", "polygon clipping", DE_ARRAY_BEGIN(polys),
                                               DE_ARRAY_END(polys), VIEWPORT_WHOLE));
            addChild(new TriangleAttributeCase(m_context, "multiple_5_viewport_center", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CENTER));
            addChild(new TriangleAttributeCase(m_context, "multiple_5_viewport_corner", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CORNER));
        }

        {
            const TriangleAttributeCase::TriangleData polys[] = {
                // one vertex clipped to z
                {tcu::Vec4(-0.2f, 0.3f, 0.0f, 1.0f), red, tcu::Vec4(0.2f, 0.3f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.0f, offset, 2.0f, 1.0f), blue},

                // two vertices clipped to edges
                {tcu::Vec4(0.6f, -1.2f, 0.0f, 1.0f), red, tcu::Vec4(1.2f, -0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.6f, -0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(0.6f, 1.2f, 0.0f, 1.0f), red, tcu::Vec4(1.2f, 0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.6f, 0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(-0.6f, 1.2f, 0.0f, 1.0f), red, tcu::Vec4(-1.2f, 0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(-0.6f, 0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(-0.6f, -1.2f, 0.0f, 1.0f), red, tcu::Vec4(-1.2f, -0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(-0.6f, -0.6f, 0.0f, 1.0f), blue},
            };

            addChild(new TriangleAttributeCase(m_context, "multiple_6", "polygon clipping", DE_ARRAY_BEGIN(polys),
                                               DE_ARRAY_END(polys), VIEWPORT_WHOLE));
            addChild(new TriangleAttributeCase(m_context, "multiple_6_viewport_center", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CENTER));
            addChild(new TriangleAttributeCase(m_context, "multiple_6_viewport_corner", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CORNER));
        }

        {
            const TriangleAttributeCase::TriangleData polys[] = {
                // two vertices clipped to edges
                {tcu::Vec4(0.6f, 1.2f, 0.0f, 1.0f), red, tcu::Vec4(1.2f, 0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.6f, 0.6f, 0.0f, 1.0f), blue},

                // two vertices clipped to edges
                {tcu::Vec4(0.6f, -1.2f, 0.0f, 1.0f), red, tcu::Vec4(1.2f, -0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.6f, -0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(0.6f, 1.2f, 0.0f, 1.0f), red, tcu::Vec4(1.2f, 0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.6f, 0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(-0.6f, 1.2f, 0.0f, 1.0f), red, tcu::Vec4(-1.2f, 0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(-0.6f, 0.6f, 0.0f, 1.0f), blue},
                {tcu::Vec4(-0.6f, -1.2f, 0.0f, 1.0f), red, tcu::Vec4(-1.2f, -0.6f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(-0.6f, -0.6f, 0.0f, 1.0f), blue},
            };

            addChild(new TriangleAttributeCase(m_context, "multiple_7", "polygon clipping", DE_ARRAY_BEGIN(polys),
                                               DE_ARRAY_END(polys), VIEWPORT_WHOLE));
            addChild(new TriangleAttributeCase(m_context, "multiple_7_viewport_center", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CENTER));
            addChild(new TriangleAttributeCase(m_context, "multiple_7_viewport_corner", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CORNER));
        }

        {
            const TriangleAttributeCase::TriangleData polys[] = {
                // one vertex clipped to z
                {tcu::Vec4(-0.2f, -0.3f, 0.0f, 1.0f), red, tcu::Vec4(0.2f, -0.3f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.0f, -offset, 2.0f, 1.0f), blue},

                // fill
                {tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), white, tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), white,
                 tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), white},
                {tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), blue, tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), blue,
                 tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f), blue},
            };

            addChild(new TriangleAttributeCase(m_context, "multiple_8", "polygon clipping", DE_ARRAY_BEGIN(polys),
                                               DE_ARRAY_END(polys), VIEWPORT_WHOLE));
            addChild(new TriangleAttributeCase(m_context, "multiple_8_viewport_center", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CENTER));
            addChild(new TriangleAttributeCase(m_context, "multiple_8_viewport_corner", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CORNER));
        }

        {
            const TriangleAttributeCase::TriangleData polys[] = {
                // one vertex clipped to z
                {tcu::Vec4(-0.2f, -0.3f, 0.0f, 1.0f), red, tcu::Vec4(0.2f, -0.3f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.0f, -offset, 2.0f, 1.0f), blue},

                // fill
                {tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), red, tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), red,
                 tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f), red},
                {tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), blue, tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), blue,
                 tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), blue},
            };

            addChild(new TriangleAttributeCase(m_context, "multiple_9", "polygon clipping", DE_ARRAY_BEGIN(polys),
                                               DE_ARRAY_END(polys), VIEWPORT_WHOLE));
            addChild(new TriangleAttributeCase(m_context, "multiple_9_viewport_center", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CENTER));
            addChild(new TriangleAttributeCase(m_context, "multiple_9_viewport_corner", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CORNER));
        }

        {
            const TriangleAttributeCase::TriangleData polys[] = {
                // one vertex clipped to z
                {tcu::Vec4(-0.2f, -0.3f, 0.0f, 1.0f), red, tcu::Vec4(0.2f, -0.3f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.0f, -offset, 2.0f, 1.0f), blue},

                // fill
                {tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), white, tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), white,
                 tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), white},
                {tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), red, tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), red,
                 tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f), red},
                {tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), blue, tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), blue,
                 tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), blue},
            };

            addChild(new TriangleAttributeCase(m_context, "multiple_10", "polygon clipping", DE_ARRAY_BEGIN(polys),
                                               DE_ARRAY_END(polys), VIEWPORT_WHOLE));
            addChild(new TriangleAttributeCase(m_context, "multiple_10_viewport_center", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CENTER));
            addChild(new TriangleAttributeCase(m_context, "multiple_10_viewport_corner", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CORNER));
        }

        {
            const TriangleAttributeCase::TriangleData polys[] = {
                // one vertex clipped to z
                {tcu::Vec4(-0.2f, -0.3f, 0.0f, 1.0f), red, tcu::Vec4(0.2f, -0.3f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(0.0f, -offset, 2.0f, 1.0f), blue},

                // fill
                {tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), white, tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), white,
                 tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), white},
                {tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), red, tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), red,
                 tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f), red},
                {tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), blue, tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), blue,
                 tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), blue},
                {tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), yellow, tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), yellow,
                 tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f), yellow},
            };

            addChild(new TriangleAttributeCase(m_context, "multiple_11", "polygon clipping", DE_ARRAY_BEGIN(polys),
                                               DE_ARRAY_END(polys), VIEWPORT_WHOLE));
            addChild(new TriangleAttributeCase(m_context, "multiple_11_viewport_center", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CENTER));
            addChild(new TriangleAttributeCase(m_context, "multiple_11_viewport_corner", "polygon clipping",
                                               DE_ARRAY_BEGIN(polys), DE_ARRAY_END(polys), VIEWPORT_CORNER));
        }
    }
}

class PolyEdgesTestGroup : public TestCaseGroup
{
public:
    PolyEdgesTestGroup(Context &context);

    void init(void);
};

PolyEdgesTestGroup::PolyEdgesTestGroup(Context &context)
    : TestCaseGroup(context, "polygon_edge", "Polygon clipping edge tests")
{
}

void PolyEdgesTestGroup::init(void)
{
    // Quads via origin
    const struct Quad
    {
        tcu::Vec3 d1; // tangent
        tcu::Vec3 d2; // bi-tangent
    } quads[] = {
        {tcu::Vec3(1, 1, 1), tcu::Vec3(1, -1, 1)},   {tcu::Vec3(1, 1, 1), tcu::Vec3(-1, 1.1f, 1)},
        {tcu::Vec3(1, 1, 0), tcu::Vec3(-1, 1, 0)},   {tcu::Vec3(0, 1, 0), tcu::Vec3(1, 0, 0)},
        {tcu::Vec3(0, 1, 0), tcu::Vec3(1, 0.1f, 0)},
    };

    // Quad near edge
    const struct EdgeQuad
    {
        tcu::Vec3 d1;     // tangent
        tcu::Vec3 d2;     // bi-tangent
        tcu::Vec3 center; // center
    } edgeQuads[] = {
        {tcu::Vec3(1, 0.01f, 0), tcu::Vec3(0, 0.01f, 0), tcu::Vec3(0, 0.99f, 0)},      // edge near x-plane
        {tcu::Vec3(0.01f, 1, 0), tcu::Vec3(0.01f, 0, 0), tcu::Vec3(0.99f, 0, 0)},      // edge near y-plane
        {tcu::Vec3(1, 1, 0.01f), tcu::Vec3(0.01f, -0.01f, 0), tcu::Vec3(0, 0, 0.99f)}, // edge near z-plane
    };

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(quads); ++ndx)
        addChild(new QuadFillTest(m_context, (std::string("quad_at_origin_") + de::toString(ndx)).c_str(),
                                  "polygon edge clipping", VIEWPORT_CENTER, quads[ndx].d1, quads[ndx].d2));
    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(edgeQuads); ++ndx)
        addChild(new QuadFillTest(m_context, (std::string("quad_near_edge_") + de::toString(ndx)).c_str(),
                                  "polygon edge clipping", VIEWPORT_CENTER, edgeQuads[ndx].d1, edgeQuads[ndx].d2,
                                  edgeQuads[ndx].center));

    // Polyfan
    addChild(new TriangleFanFillTest(m_context, "poly_fan", "polygon edge clipping", VIEWPORT_CENTER));
}

class PolyVertexClipTestGroup : public TestCaseGroup
{
public:
    PolyVertexClipTestGroup(Context &context);

    void init(void);
};

PolyVertexClipTestGroup::PolyVertexClipTestGroup(Context &context)
    : TestCaseGroup(context, "triangle_vertex", "Clip n vertices")
{
}

void PolyVertexClipTestGroup::init(void)
{
    const float far               = 30000.0f;
    const float farForThreeVertex = 20000.0f; // 3 vertex clipping tests use smaller triangles
    const tcu::IVec3 outside[]    = {
        // outside one clipping plane
        tcu::IVec3(-1, 0, 0),
        tcu::IVec3(1, 0, 0),
        tcu::IVec3(0, 1, 0),
        tcu::IVec3(0, -1, 0),
        tcu::IVec3(0, 0, 1),
        tcu::IVec3(0, 0, -1),

        // outside two clipping planes
        tcu::IVec3(-1, -1, 0),
        tcu::IVec3(1, -1, 0),
        tcu::IVec3(1, 1, 0),
        tcu::IVec3(-1, 1, 0),

        tcu::IVec3(-1, 0, -1),
        tcu::IVec3(1, 0, -1),
        tcu::IVec3(1, 0, 1),
        tcu::IVec3(-1, 0, 1),

        tcu::IVec3(0, -1, -1),
        tcu::IVec3(0, 1, -1),
        tcu::IVec3(0, 1, 1),
        tcu::IVec3(0, -1, 1),

        // outside three clipping planes
        tcu::IVec3(-1, -1, 1),
        tcu::IVec3(1, -1, 1),
        tcu::IVec3(1, 1, 1),
        tcu::IVec3(-1, 1, 1),

        tcu::IVec3(-1, -1, -1),
        tcu::IVec3(1, -1, -1),
        tcu::IVec3(1, 1, -1),
        tcu::IVec3(-1, 1, -1),
    };

    de::Random rnd(0xabcdef);

    TestCaseGroup *clipOne   = new TestCaseGroup(m_context, "clip_one", "Clip one vertex");
    TestCaseGroup *clipTwo   = new TestCaseGroup(m_context, "clip_two", "Clip two vertices");
    TestCaseGroup *clipThree = new TestCaseGroup(m_context, "clip_three", "Clip three vertices");

    addChild(clipOne);
    addChild(clipTwo);
    addChild(clipThree);

    // Test 1 point clipped
    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(outside); ++ndx)
    {
        const float w0        = rnd.getFloat(0.2f, 16.0f);
        const float w1        = rnd.getFloat(0.2f, 16.0f);
        const float w2        = rnd.getFloat(0.2f, 16.0f);
        const tcu::Vec4 white = tcu::Vec4(1, 1, 1, 1);
        const tcu::Vec3 r0    = tcu::Vec3(0.2f, 0.3f, 0);
        const tcu::Vec3 r1    = tcu::Vec3(-0.3f, -0.4f, 0);
        const tcu::Vec3 r2    = IVec3ToVec3(outside[ndx]) * far;
        const tcu::Vec4 p0    = tcu::Vec4(r0.x() * w0, r0.y() * w0, r0.z() * w0, w0);
        const tcu::Vec4 p1    = tcu::Vec4(r1.x() * w1, r1.y() * w1, r1.z() * w1, w1);
        const tcu::Vec4 p2    = tcu::Vec4(r2.x() * w2, r2.y() * w2, r2.z() * w2, w2);

        const std::string name = std::string("clip") +
                                 (outside[ndx].x() > 0 ? "_pos_x" : (outside[ndx].x() < 0 ? "_neg_x" : "")) +
                                 (outside[ndx].y() > 0 ? "_pos_y" : (outside[ndx].y() < 0 ? "_neg_y" : "")) +
                                 (outside[ndx].z() > 0 ? "_pos_z" : (outside[ndx].z() < 0 ? "_neg_z" : ""));

        const TriangleCase::TriangleData triangle = {p0, white, p1, white, p2, white};

        // don't try to test with degenerate (or almost degenerate) triangles
        if (outside[ndx].x() == 0 && outside[ndx].y() == 0)
            continue;

        clipOne->addChild(
            new TriangleCase(m_context, name.c_str(), "clip one vertex", &triangle, &triangle + 1, VIEWPORT_CENTER));
    }

    // Special triangles for "clip_z" cases, default triangles is not good, since it has very small visible area => problems with MSAA
    {
        const tcu::Vec4 white = tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f);

        const TriangleCase::TriangleData posZTriangle = {tcu::Vec4(0.0f, -0.7f, -0.9f, 1.0f), white,
                                                         tcu::Vec4(0.8f, 0.0f, -0.7f, 1.0f),  white,
                                                         tcu::Vec4(-0.9f, 0.9f, 3.0f, 1.0f),  white};
        const TriangleCase::TriangleData negZTriangle = {tcu::Vec4(0.0f, -0.7f, 0.9f, 1.0f),  white,
                                                         tcu::Vec4(0.4f, 0.0f, 0.7f, 1.0f),   white,
                                                         tcu::Vec4(-0.9f, 0.9f, -3.0f, 1.0f), white};

        clipOne->addChild(new TriangleCase(m_context, "clip_pos_z", "clip one vertex", &posZTriangle, &posZTriangle + 1,
                                           VIEWPORT_CENTER));
        clipOne->addChild(new TriangleCase(m_context, "clip_neg_z", "clip one vertex", &negZTriangle, &negZTriangle + 1,
                                           VIEWPORT_CENTER));
    }

    // Test 2 points clipped
    for (int ndx1 = 0; ndx1 < DE_LENGTH_OF_ARRAY(outside); ++ndx1)
        for (int ndx2 = ndx1 + 1; ndx2 < DE_LENGTH_OF_ARRAY(outside); ++ndx2)
        {
            const float w0        = rnd.getFloat(0.2f, 16.0f);
            const float w1        = rnd.getFloat(0.2f, 16.0f);
            const float w2        = rnd.getFloat(0.2f, 16.0f);
            const tcu::Vec4 white = tcu::Vec4(1, 1, 1, 1);
            const tcu::Vec3 r0    = tcu::Vec3(0.2f, 0.3f, 0);
            const tcu::IVec3 r1   = outside[ndx1];
            const tcu::IVec3 r2   = outside[ndx2];
            const tcu::Vec4 p0    = tcu::Vec4(r0.x() * w0, r0.y() * w0, r0.z() * w0, w0);
            const tcu::Vec4 p1 =
                tcu::Vec4(float(r1.x()) * far * w1, float(r1.y()) * far * w1, float(r1.z()) * far * w1, w1);
            const tcu::Vec4 p2 =
                tcu::Vec4(float(r2.x()) * far * w2, float(r2.y()) * far * w2, float(r2.z()) * far * w2, w2);

            const std::string name =
                std::string("clip") + (outside[ndx1].x() > 0 ? "_pos_x" : (outside[ndx1].x() < 0 ? "_neg_x" : "")) +
                (outside[ndx1].y() > 0 ? "_pos_y" : (outside[ndx1].y() < 0 ? "_neg_y" : "")) +
                (outside[ndx1].z() > 0 ? "_pos_z" : (outside[ndx1].z() < 0 ? "_neg_z" : "")) + "_and" +
                (outside[ndx2].x() > 0 ? "_pos_x" : (outside[ndx2].x() < 0 ? "_neg_x" : "")) +
                (outside[ndx2].y() > 0 ? "_pos_y" : (outside[ndx2].y() < 0 ? "_neg_y" : "")) +
                (outside[ndx2].z() > 0 ? "_pos_z" : (outside[ndx2].z() < 0 ? "_neg_z" : ""));

            const TriangleCase::TriangleData triangle = {p0, white, p1, white, p2, white};

            if (twoPointClippedTriangleInvisible(r0, r1, r2))
                continue;

            clipTwo->addChild(new TriangleCase(m_context, name.c_str(), "clip two vertices", &triangle, &triangle + 1,
                                               VIEWPORT_CENTER));
        }

    // Test 3 points clipped
    for (int ndx1 = 0; ndx1 < DE_LENGTH_OF_ARRAY(outside); ++ndx1)
        for (int ndx2 = ndx1 + 1; ndx2 < DE_LENGTH_OF_ARRAY(outside); ++ndx2)
            for (int ndx3 = ndx2 + 1; ndx3 < DE_LENGTH_OF_ARRAY(outside); ++ndx3)
            {
                const float w0        = rnd.getFloat(0.2f, 16.0f);
                const float w1        = rnd.getFloat(0.2f, 16.0f);
                const float w2        = rnd.getFloat(0.2f, 16.0f);
                const tcu::Vec4 white = tcu::Vec4(1, 1, 1, 1);
                const tcu::IVec3 r0   = outside[ndx1];
                const tcu::IVec3 r1   = outside[ndx2];
                const tcu::IVec3 r2   = outside[ndx3];
                const tcu::Vec4 p0 =
                    tcu::Vec4(float(r0.x()) * farForThreeVertex * w0, float(r0.y()) * farForThreeVertex * w0,
                              float(r0.z()) * farForThreeVertex * w0, w0);
                const tcu::Vec4 p1 =
                    tcu::Vec4(float(r1.x()) * farForThreeVertex * w1, float(r1.y()) * farForThreeVertex * w1,
                              float(r1.z()) * farForThreeVertex * w1, w1);
                const tcu::Vec4 p2 =
                    tcu::Vec4(float(r2.x()) * farForThreeVertex * w2, float(r2.y()) * farForThreeVertex * w2,
                              float(r2.z()) * farForThreeVertex * w2, w2);

                // ignore cases where polygon is along xz or yz planes
                if (pointsOnLine(r0.swizzle(0, 1), r1.swizzle(0, 1), r2.swizzle(0, 1)))
                    continue;

                // triangle is visible only if it intersects the origin
                if (pointOnTriangle(tcu::IVec3(0, 0, 0), r0, r1, r2))
                {
                    const TriangleCase::TriangleData triangle = {p0, white, p1, white, p2, white};
                    const std::string name =
                        std::string("clip") +
                        (outside[ndx1].x() > 0 ? "_pos_x" : (outside[ndx1].x() < 0 ? "_neg_x" : "")) +
                        (outside[ndx1].y() > 0 ? "_pos_y" : (outside[ndx1].y() < 0 ? "_neg_y" : "")) +
                        (outside[ndx1].z() > 0 ? "_pos_z" : (outside[ndx1].z() < 0 ? "_neg_z" : "")) + "_and" +
                        (outside[ndx2].x() > 0 ? "_pos_x" : (outside[ndx2].x() < 0 ? "_neg_x" : "")) +
                        (outside[ndx2].y() > 0 ? "_pos_y" : (outside[ndx2].y() < 0 ? "_neg_y" : "")) +
                        (outside[ndx2].z() > 0 ? "_pos_z" : (outside[ndx2].z() < 0 ? "_neg_z" : "")) + "_and" +
                        (outside[ndx3].x() > 0 ? "_pos_x" : (outside[ndx3].x() < 0 ? "_neg_x" : "")) +
                        (outside[ndx3].y() > 0 ? "_pos_y" : (outside[ndx3].y() < 0 ? "_neg_y" : "")) +
                        (outside[ndx3].z() > 0 ? "_pos_z" : (outside[ndx3].z() < 0 ? "_neg_z" : ""));

                    clipThree->addChild(new TriangleCase(m_context, name.c_str(), "clip three vertices", &triangle,
                                                         &triangle + 1, VIEWPORT_CENTER));
                }
            }
}

} // namespace

ClippingTests::ClippingTests(Context &context) : TestCaseGroup(context, "clipping", "Clipping tests")
{
}

ClippingTests::~ClippingTests(void)
{
}

void ClippingTests::init(void)
{
    addChild(new PointsTestGroup(m_context));
    addChild(new LinesTestGroup(m_context));
    addChild(new PolysTestGroup(m_context));
    addChild(new PolyEdgesTestGroup(m_context));
    addChild(new PolyVertexClipTestGroup(m_context));
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
