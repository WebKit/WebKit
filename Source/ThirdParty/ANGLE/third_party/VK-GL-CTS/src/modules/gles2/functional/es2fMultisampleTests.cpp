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
 * \brief Multisampling tests.
 *//*--------------------------------------------------------------------*/

#include "es2fMultisampleTests.hpp"
#include "gluPixelTransfer.hpp"
#include "gluShaderProgram.hpp"
#include "tcuSurface.hpp"
#include "tcuImageCompare.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuCommandLine.hpp"
#include "deStringUtil.hpp"
#include "deRandom.hpp"
#include "deMath.h"
#include "deString.h"

#include "glw.h"

#include <string>
#include <vector>

namespace deqp
{
namespace gles2
{
namespace Functional
{

using std::vector;
using tcu::IVec2;
using tcu::IVec4;
using tcu::TestLog;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;

static const float SQRT_HALF = 0.707107f;

namespace
{

struct QuadCorners
{
    Vec2 p0;
    Vec2 p1;
    Vec2 p2;
    Vec2 p3;

    QuadCorners(const Vec2 &p0_, const Vec2 &p1_, const Vec2 &p2_, const Vec2 &p3_) : p0(p0_), p1(p1_), p2(p2_), p3(p3_)
    {
    }
};

} // namespace

static inline int getIterationCount(const tcu::TestContext &ctx, int defaultCount)
{
    int cmdLineValue = ctx.getCommandLine().getTestIterationCount();
    return cmdLineValue > 0 ? cmdLineValue : defaultCount;
}

static inline int getGLInteger(GLenum name)
{
    int result;
    GLU_CHECK_CALL(glGetIntegerv(name, &result));
    return result;
}

template <typename T>
static inline T min4(T a, T b, T c, T d)
{
    return de::min(de::min(de::min(a, b), c), d);
}

template <typename T>
static inline T max4(T a, T b, T c, T d)
{
    return de::max(de::max(de::max(a, b), c), d);
}

static inline bool isInsideQuad(const IVec2 &point, const IVec2 &p0, const IVec2 &p1, const IVec2 &p2, const IVec2 &p3)
{
    int dot0 = (point.x() - p0.x()) * (p1.y() - p0.y()) + (point.y() - p0.y()) * (p0.x() - p1.x());
    int dot1 = (point.x() - p1.x()) * (p2.y() - p1.y()) + (point.y() - p1.y()) * (p1.x() - p2.x());
    int dot2 = (point.x() - p2.x()) * (p3.y() - p2.y()) + (point.y() - p2.y()) * (p2.x() - p3.x());
    int dot3 = (point.x() - p3.x()) * (p0.y() - p3.y()) + (point.y() - p3.y()) * (p3.x() - p0.x());

    return (dot0 > 0) == (dot1 > 0) && (dot1 > 0) == (dot2 > 0) && (dot2 > 0) == (dot3 > 0);
}

/*--------------------------------------------------------------------*//*!
 * \brief Check if a region in an image is unicolored.
 *
 * Checks if the pixels in img inside the convex quadilateral defined by
 * p0, p1, p2 and p3 are all (approximately) of the same color.
 *//*--------------------------------------------------------------------*/
static bool isPixelRegionUnicolored(const tcu::Surface &img, const IVec2 &p0, const IVec2 &p1, const IVec2 &p2,
                                    const IVec2 &p3)
{
    int xMin               = de::clamp(min4(p0.x(), p1.x(), p2.x(), p3.x()), 0, img.getWidth() - 1);
    int yMin               = de::clamp(min4(p0.y(), p1.y(), p2.y(), p3.y()), 0, img.getHeight() - 1);
    int xMax               = de::clamp(max4(p0.x(), p1.x(), p2.x(), p3.x()), 0, img.getWidth() - 1);
    int yMax               = de::clamp(max4(p0.y(), p1.y(), p2.y(), p3.y()), 0, img.getHeight() - 1);
    bool insideEncountered = false; //!< Whether we have already seen at least one pixel inside the region.
    tcu::RGBA insideColor;          //!< Color of the first pixel inside the region.

    for (int y = yMin; y <= yMax; y++)
        for (int x = xMin; x <= xMax; x++)
        {
            if (isInsideQuad(IVec2(x, y), p0, p1, p2, p3))
            {
                tcu::RGBA pixColor = img.getPixel(x, y);

                if (insideEncountered)
                {
                    if (!tcu::compareThreshold(
                            pixColor, insideColor,
                            tcu::RGBA(
                                3, 3, 3,
                                3))) // Pixel color differs from already-detected color inside same region - region not unicolored.
                        return false;
                }
                else
                {
                    insideEncountered = true;
                    insideColor       = pixColor;
                }
            }
        }

    return true;
}

static bool drawUnicolorTestErrors(tcu::Surface &img, const tcu::PixelBufferAccess &errorImg, const IVec2 &p0,
                                   const IVec2 &p1, const IVec2 &p2, const IVec2 &p3)
{
    int xMin           = de::clamp(min4(p0.x(), p1.x(), p2.x(), p3.x()), 0, img.getWidth() - 1);
    int yMin           = de::clamp(min4(p0.y(), p1.y(), p2.y(), p3.y()), 0, img.getHeight() - 1);
    int xMax           = de::clamp(max4(p0.x(), p1.x(), p2.x(), p3.x()), 0, img.getWidth() - 1);
    int yMax           = de::clamp(max4(p0.y(), p1.y(), p2.y(), p3.y()), 0, img.getHeight() - 1);
    tcu::RGBA refColor = img.getPixel((xMin + xMax) / 2, (yMin + yMax) / 2);

    for (int y = yMin; y <= yMax; y++)
        for (int x = xMin; x <= xMax; x++)
        {
            if (isInsideQuad(IVec2(x, y), p0, p1, p2, p3))
            {
                if (!tcu::compareThreshold(img.getPixel(x, y), refColor, tcu::RGBA(3, 3, 3, 3)))
                {
                    img.setPixel(x, y, tcu::RGBA::red());
                    errorImg.setPixel(Vec4(1.0f, 0.0f, 0.0f, 1.0f), x, y);
                }
            }
        }

    return true;
}

/*--------------------------------------------------------------------*//*!
 * \brief Abstract base class handling common stuff for multisample cases.
 *//*--------------------------------------------------------------------*/
class MultisampleCase : public TestCase
{
public:
    MultisampleCase(Context &context, const char *name, const char *desc);
    virtual ~MultisampleCase(void);

    virtual void init(void);
    virtual void deinit(void);

protected:
    virtual int getDesiredViewportSize(void) const = 0;

    void renderTriangle(const Vec3 &p0, const Vec3 &p1, const Vec3 &p2, const Vec4 &c0, const Vec4 &c1,
                        const Vec4 &c2) const;
    void renderTriangle(const Vec3 &p0, const Vec3 &p1, const Vec3 &p2, const Vec4 &color) const;
    void renderTriangle(const Vec2 &p0, const Vec2 &p1, const Vec2 &p2, const Vec4 &c0, const Vec4 &c1,
                        const Vec4 &c2) const;
    void renderTriangle(const Vec2 &p0, const Vec2 &p1, const Vec2 &p2, const Vec4 &color) const;
    void renderQuad(const Vec2 &p0, const Vec2 &p1, const Vec2 &p2, const Vec2 &p3, const Vec4 &c0, const Vec4 &c1,
                    const Vec4 &c2, const Vec4 &c3) const;
    void renderQuad(const Vec2 &p0, const Vec2 &p1, const Vec2 &p2, const Vec2 &p3, const Vec4 &color) const;
    void renderLine(const Vec2 &p0, const Vec2 &p1, const Vec4 &color) const;

    void randomizeViewport(void);
    void readImage(tcu::Surface &dst) const;

    int m_numSamples;

    int m_viewportSize;

private:
    MultisampleCase(const MultisampleCase &other);
    MultisampleCase &operator=(const MultisampleCase &other);

    glu::ShaderProgram *m_program;
    int m_attrPositionLoc;
    int m_attrColorLoc;

    int m_viewportX;
    int m_viewportY;
    de::Random m_rnd;
};

MultisampleCase::MultisampleCase(Context &context, const char *name, const char *desc)
    : TestCase(context, name, desc)
    , m_numSamples(0)
    , m_viewportSize(0)
    , m_program(nullptr)
    , m_attrPositionLoc(-1)
    , m_attrColorLoc(-1)
    , m_viewportX(0)
    , m_viewportY(0)
    , m_rnd(deStringHash(name))
{
}

void MultisampleCase::renderTriangle(const Vec3 &p0, const Vec3 &p1, const Vec3 &p2, const Vec4 &c0, const Vec4 &c1,
                                     const Vec4 &c2) const
{
    float vertexPositions[] = {p0.x(), p0.y(), p0.z(), 1.0f,   p1.x(), p1.y(),
                               p1.z(), 1.0f,   p2.x(), p2.y(), p2.z(), 1.0f};
    float vertexColors[]    = {
        c0.x(), c0.y(), c0.z(), c0.w(), c1.x(), c1.y(), c1.z(), c1.w(), c2.x(), c2.y(), c2.z(), c2.w(),
    };

    GLU_CHECK_CALL(glEnableVertexAttribArray(m_attrPositionLoc));
    GLU_CHECK_CALL(glVertexAttribPointer(m_attrPositionLoc, 4, GL_FLOAT, false, 0, &vertexPositions[0]));

    GLU_CHECK_CALL(glEnableVertexAttribArray(m_attrColorLoc));
    GLU_CHECK_CALL(glVertexAttribPointer(m_attrColorLoc, 4, GL_FLOAT, false, 0, &vertexColors[0]));

    GLU_CHECK_CALL(glUseProgram(m_program->getProgram()));
    GLU_CHECK_CALL(glDrawArrays(GL_TRIANGLES, 0, 3));
}

void MultisampleCase::renderTriangle(const Vec3 &p0, const Vec3 &p1, const Vec3 &p2, const Vec4 &color) const
{
    renderTriangle(p0, p1, p2, color, color, color);
}

void MultisampleCase::renderTriangle(const Vec2 &p0, const Vec2 &p1, const Vec2 &p2, const Vec4 &c0, const Vec4 &c1,
                                     const Vec4 &c2) const
{
    renderTriangle(Vec3(p0.x(), p0.y(), 0.0f), Vec3(p1.x(), p1.y(), 0.0f), Vec3(p2.x(), p2.y(), 0.0f), c0, c1, c2);
}

void MultisampleCase::renderTriangle(const Vec2 &p0, const Vec2 &p1, const Vec2 &p2, const Vec4 &color) const
{
    renderTriangle(p0, p1, p2, color, color, color);
}

void MultisampleCase::renderQuad(const Vec2 &p0, const Vec2 &p1, const Vec2 &p2, const Vec2 &p3, const Vec4 &c0,
                                 const Vec4 &c1, const Vec4 &c2, const Vec4 &c3) const
{
    renderTriangle(p0, p1, p2, c0, c1, c2);
    renderTriangle(p2, p1, p3, c2, c1, c3);
}

void MultisampleCase::renderQuad(const Vec2 &p0, const Vec2 &p1, const Vec2 &p2, const Vec2 &p3,
                                 const Vec4 &color) const
{
    renderQuad(p0, p1, p2, p3, color, color, color, color);
}

void MultisampleCase::renderLine(const Vec2 &p0, const Vec2 &p1, const Vec4 &color) const
{
    float vertexPositions[] = {p0.x(), p0.y(), 0.0f, 1.0f, p1.x(), p1.y(), 0.0f, 1.0f};
    float vertexColors[]    = {color.x(), color.y(), color.z(), color.w(), color.x(), color.y(), color.z(), color.w()};

    GLU_CHECK_CALL(glEnableVertexAttribArray(m_attrPositionLoc));
    GLU_CHECK_CALL(glVertexAttribPointer(m_attrPositionLoc, 4, GL_FLOAT, false, 0, &vertexPositions[0]));

    GLU_CHECK_CALL(glEnableVertexAttribArray(m_attrColorLoc));
    GLU_CHECK_CALL(glVertexAttribPointer(m_attrColorLoc, 4, GL_FLOAT, false, 0, &vertexColors[0]));

    GLU_CHECK_CALL(glUseProgram(m_program->getProgram()));
    GLU_CHECK_CALL(glDrawArrays(GL_LINES, 0, 2));
}

void MultisampleCase::randomizeViewport(void)
{
    m_viewportX = m_rnd.getInt(0, m_context.getRenderTarget().getWidth() - m_viewportSize);
    m_viewportY = m_rnd.getInt(0, m_context.getRenderTarget().getHeight() - m_viewportSize);

    GLU_CHECK_CALL(glViewport(m_viewportX, m_viewportY, m_viewportSize, m_viewportSize));
}

void MultisampleCase::readImage(tcu::Surface &dst) const
{
    glu::readPixels(m_context.getRenderContext(), m_viewportX, m_viewportY, dst.getAccess());
}

void MultisampleCase::init(void)
{
    static const char *vertShaderSource = "attribute highp vec4 a_position;\n"
                                          "attribute mediump vec4 a_color;\n"
                                          "varying mediump vec4 v_color;\n"
                                          "void main()\n"
                                          "{\n"
                                          "    gl_Position = a_position;\n"
                                          "    v_color = a_color;\n"
                                          "}\n";

    static const char *fragShaderSource = "varying mediump vec4 v_color;\n"
                                          "void main()\n"
                                          "{\n"
                                          "    gl_FragColor = v_color;\n"
                                          "}\n";

    // Check multisample support.

    if (m_context.getRenderTarget().getNumSamples() <= 1)
        throw tcu::NotSupportedError("No multisample buffers");

    // Prepare program.

    DE_ASSERT(!m_program);

    m_program = new glu::ShaderProgram(m_context.getRenderContext(),
                                       glu::makeVtxFragSources(vertShaderSource, fragShaderSource));
    if (!m_program->isOk())
        throw tcu::TestError("Failed to compile program", nullptr, __FILE__, __LINE__);

    GLU_CHECK_CALL(m_attrPositionLoc = glGetAttribLocation(m_program->getProgram(), "a_position"));
    GLU_CHECK_CALL(m_attrColorLoc = glGetAttribLocation(m_program->getProgram(), "a_color"));

    if (m_attrPositionLoc < 0 || m_attrColorLoc < 0)
    {
        delete m_program;
        throw tcu::TestError("Invalid attribute locations", nullptr, __FILE__, __LINE__);
    }

    // Get suitable viewport size.

    m_viewportSize = de::min<int>(getDesiredViewportSize(), de::min(m_context.getRenderTarget().getWidth(),
                                                                    m_context.getRenderTarget().getHeight()));
    randomizeViewport();

    // Query and log number of samples per pixel.

    m_numSamples = getGLInteger(GL_SAMPLES);
    m_testCtx.getLog() << TestLog::Message << "GL_SAMPLES = " << m_numSamples << TestLog::EndMessage;
}

MultisampleCase::~MultisampleCase(void)
{
    delete m_program;
}

void MultisampleCase::deinit(void)
{
    delete m_program;

    m_program = nullptr;
}

/*--------------------------------------------------------------------*//*!
 * \brief Base class for cases testing the value of GL_SAMPLES.
 *
 * Draws a test pattern (defined by renderPattern() of an inheriting class)
 * and counts the number of distinct colors in the resulting image. That
 * number should be at least the value of GL_SAMPLES plus one. This is
 * repeated with increased values of m_currentIteration until this correct
 * number of colors is detected or m_currentIteration reaches
 * m_maxNumIterations.
 *//*--------------------------------------------------------------------*/
class NumSamplesCase : public MultisampleCase
{
public:
    NumSamplesCase(Context &context, const char *name, const char *description);
    ~NumSamplesCase(void)
    {
    }

    IterateResult iterate(void);

protected:
    int getDesiredViewportSize(void) const
    {
        return 256;
    }
    virtual void renderPattern(void) const = 0;

    int m_currentIteration;

private:
    enum
    {
        DEFAULT_MAX_NUM_ITERATIONS = 16
    };

    const int m_maxNumIterations;
    vector<tcu::RGBA> m_detectedColors;
};

NumSamplesCase::NumSamplesCase(Context &context, const char *name, const char *description)
    : MultisampleCase(context, name, description)
    , m_currentIteration(0)
    , m_maxNumIterations(getIterationCount(m_testCtx, DEFAULT_MAX_NUM_ITERATIONS))
{
}

NumSamplesCase::IterateResult NumSamplesCase::iterate(void)
{
    TestLog &log = m_testCtx.getLog();
    tcu::Surface renderedImg(m_viewportSize, m_viewportSize);

    randomizeViewport();

    GLU_CHECK_CALL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
    GLU_CHECK_CALL(glClear(GL_COLOR_BUFFER_BIT));

    renderPattern();

    // Read and log rendered image.

    readImage(renderedImg);

    log << TestLog::Image("RenderedImage", "Rendered image", renderedImg, QP_IMAGE_COMPRESSION_MODE_PNG);

    // Detect new, previously unseen colors from image.

    int requiredNumDistinctColors = m_numSamples + 1;

    for (int y = 0; y < renderedImg.getHeight() && (int)m_detectedColors.size() < requiredNumDistinctColors; y++)
        for (int x = 0; x < renderedImg.getWidth() && (int)m_detectedColors.size() < requiredNumDistinctColors; x++)
        {
            tcu::RGBA color = renderedImg.getPixel(x, y);

            int i;
            for (i = 0; i < (int)m_detectedColors.size(); i++)
            {
                if (tcu::compareThreshold(color, m_detectedColors[i], tcu::RGBA(3, 3, 3, 3)))
                    break;
            }

            if (i == (int)m_detectedColors.size())
                m_detectedColors.push_back(color); // Color not previously detected.
        }

    // Log results.

    log << TestLog::Message << "Number of distinct colors detected so far: "
        << ((int)m_detectedColors.size() >= requiredNumDistinctColors ? "at least " : "")
        << de::toString(m_detectedColors.size()) << TestLog::EndMessage;

    if ((int)m_detectedColors.size() < requiredNumDistinctColors)
    {
        // Haven't detected enough different colors yet.

        m_currentIteration++;

        if (m_currentIteration >= m_maxNumIterations)
        {
            log << TestLog::Message << "Failure: Number of distinct colors detected is lower than GL_SAMPLES+1"
                << TestLog::EndMessage;
            m_context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Failed");
            return STOP;
        }
        else
        {
            log << TestLog::Message
                << "The number of distinct colors detected is lower than GL_SAMPLES+1 - trying again with a slightly "
                   "altered pattern"
                << TestLog::EndMessage;
            return CONTINUE;
        }
    }
    else
    {
        log << TestLog::Message << "Success: The number of distinct colors detected is at least GL_SAMPLES+1"
            << TestLog::EndMessage;
        m_context.getTestContext().setTestResult(QP_TEST_RESULT_PASS, "Passed");
        return STOP;
    }
}

class PolygonNumSamplesCase : public NumSamplesCase
{
public:
    PolygonNumSamplesCase(Context &context, const char *name, const char *description);
    ~PolygonNumSamplesCase(void)
    {
    }

protected:
    void renderPattern(void) const;
};

PolygonNumSamplesCase::PolygonNumSamplesCase(Context &context, const char *name, const char *description)
    : NumSamplesCase(context, name, description)
{
}

void PolygonNumSamplesCase::renderPattern(void) const
{
    // The test pattern consists of several triangles with edges at different angles.

    const int numTriangles = 25;
    for (int i = 0; i < numTriangles; i++)
    {
        float angle0 = 2.0f * DE_PI * (float)i / (float)numTriangles + 0.001f * (float)m_currentIteration;
        float angle1 = 2.0f * DE_PI * ((float)i + 0.5f) / (float)numTriangles + 0.001f * (float)m_currentIteration;

        renderTriangle(Vec2(0.0f, 0.0f), Vec2(deFloatCos(angle0) * 0.95f, deFloatSin(angle0) * 0.95f),
                       Vec2(deFloatCos(angle1) * 0.95f, deFloatSin(angle1) * 0.95f), Vec4(1.0f));
    }
}

class LineNumSamplesCase : public NumSamplesCase
{
public:
    LineNumSamplesCase(Context &context, const char *name, const char *description);
    ~LineNumSamplesCase(void)
    {
    }

protected:
    void renderPattern(void) const;
};

LineNumSamplesCase::LineNumSamplesCase(Context &context, const char *name, const char *description)
    : NumSamplesCase(context, name, description)
{
}

void LineNumSamplesCase::renderPattern(void) const
{
    // The test pattern consists of several lines at different angles.

    // We scale the number of lines based on the viewport size. This is because a gl line's thickness is
    // constant in pixel units, i.e. they get relatively thicker as viewport size decreases. Thus we must
    // decrease the number of lines in order to decrease the extent of overlap among the lines in the
    // center of the pattern.
    const int numLines = (int)(100.0f * deFloatSqrt((float)m_viewportSize / 256.0f));

    for (int i = 0; i < numLines; i++)
    {
        float angle = 2.0f * DE_PI * (float)i / (float)numLines + 0.001f * (float)m_currentIteration;
        renderLine(Vec2(0.0f, 0.0f), Vec2(deFloatCos(angle) * 0.95f, deFloatSin(angle) * 0.95f), Vec4(1.0f));
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Case testing behaviour of common edges when multisampling.
 *
 * Draws a number of test patterns, each with a number of quads, each made
 * of two triangles, rotated at different angles. The inner edge inside the
 * quad (i.e. the common edge of the two triangles) still should not be
 * visible, despite multisampling - i.e. the two triangles forming the quad
 * should never get any common coverage bits in any pixel.
 *//*--------------------------------------------------------------------*/
class CommonEdgeCase : public MultisampleCase
{
public:
    enum CaseType
    {
        CASETYPE_SMALL_QUADS = 0,           //!< Draw several small quads per iteration.
        CASETYPE_BIGGER_THAN_VIEWPORT_QUAD, //!< Draw one bigger-than-viewport quad per iteration.
        CASETYPE_FIT_VIEWPORT_QUAD,         //!< Draw one exactly viewport-sized, axis aligned quad per iteration.

        CASETYPE_LAST
    };

    CommonEdgeCase(Context &context, const char *name, const char *description, CaseType caseType);
    ~CommonEdgeCase(void)
    {
    }

    void init(void);

    IterateResult iterate(void);

protected:
    int getDesiredViewportSize(void) const
    {
        return m_caseType == CASETYPE_SMALL_QUADS ? 128 : 32;
    }

private:
    enum
    {
        DEFAULT_SMALL_QUADS_ITERATIONS               = 16,
        DEFAULT_BIGGER_THAN_VIEWPORT_QUAD_ITERATIONS = 8 * 8
        // \note With CASETYPE_FIT_VIEWPORT_QUAD, we don't do rotations other than multiples of 90 deg -> constant number of iterations.
    };

    const CaseType m_caseType;

    const int m_numIterations;
    int m_currentIteration;
};

CommonEdgeCase::CommonEdgeCase(Context &context, const char *name, const char *description, CaseType caseType)
    : MultisampleCase(context, name, description)
    , m_caseType(caseType)
    , m_numIterations(caseType == CASETYPE_SMALL_QUADS ?
                          getIterationCount(m_testCtx, DEFAULT_SMALL_QUADS_ITERATIONS) :
                      caseType == CASETYPE_BIGGER_THAN_VIEWPORT_QUAD ?
                          getIterationCount(m_testCtx, DEFAULT_BIGGER_THAN_VIEWPORT_QUAD_ITERATIONS) :
                          8)
    , m_currentIteration(0)
{
}

void CommonEdgeCase::init(void)
{
    MultisampleCase::init();

    if (m_caseType == CASETYPE_SMALL_QUADS)
    {
        // Check for a big enough viewport. With too small viewports the test case can't analyze the resulting image well enough.

        const int minViewportSize = 32;

        DE_ASSERT(minViewportSize <= getDesiredViewportSize());

        if (m_viewportSize < minViewportSize)
            throw tcu::InternalError("Render target width or height too low (is " + de::toString(m_viewportSize) +
                                     ", should be at least " + de::toString(minViewportSize) + ")");
    }

    GLU_CHECK_CALL(glEnable(GL_BLEND));
    GLU_CHECK_CALL(glBlendEquation(GL_FUNC_ADD));
    GLU_CHECK_CALL(glBlendFunc(GL_ONE, GL_ONE));

    m_testCtx.getLog() << TestLog::Message
                       << "Additive blending enabled in order to detect (erroneously) overlapping samples"
                       << TestLog::EndMessage;
}

CommonEdgeCase::IterateResult CommonEdgeCase::iterate(void)
{
    TestLog &log = m_testCtx.getLog();
    tcu::Surface renderedImg(m_viewportSize, m_viewportSize);
    tcu::Surface errorImg(m_viewportSize, m_viewportSize);

    randomizeViewport();

    GLU_CHECK_CALL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
    GLU_CHECK_CALL(glClear(GL_COLOR_BUFFER_BIT));

    // Draw test pattern. Test patterns consist of quads formed with two triangles.
    // After drawing the pattern, we check that the interior pixels of each quad are
    // all the same color - this is meant to verify that there are no artifacts on the inner edge.

    vector<QuadCorners> unicoloredRegions;

    if (m_caseType == CASETYPE_SMALL_QUADS)
    {
        // Draw several quads, rotated at different angles.

        const float quadDiagLen = 2.0f / 3.0f * 0.9f; // \note Fit 3 quads in both x and y directions.
        float angleCos;
        float angleSin;

        // \note First and second iteration get exact 0 (and 90, 180, 270) and 45 (and 135, 225, 315) angle quads, as they are kind of a special case.

        if (m_currentIteration == 0)
        {
            angleCos = 1.0f;
            angleSin = 0.0f;
        }
        else if (m_currentIteration == 1)
        {
            angleCos = SQRT_HALF;
            angleSin = SQRT_HALF;
        }
        else
        {
            float angle = 0.5f * DE_PI * (float)(m_currentIteration - 1) / (float)(m_numIterations - 1);
            angleCos    = deFloatCos(angle);
            angleSin    = deFloatSin(angle);
        }

        Vec2 corners[4] = {
            0.5f * quadDiagLen * Vec2(angleCos, angleSin), 0.5f * quadDiagLen * Vec2(-angleSin, angleCos),
            0.5f * quadDiagLen * Vec2(-angleCos, -angleSin), 0.5f * quadDiagLen * Vec2(angleSin, -angleCos)};

        unicoloredRegions.reserve(8);

        // Draw 8 quads.
        // First four are rotated at angles angle+0, angle+90, angle+180 and angle+270.
        // Last four are rotated the same angles as the first four, but the ordering of the last triangle's vertices is reversed.

        for (int quadNdx = 0; quadNdx < 8; quadNdx++)
        {
            Vec2 center = (2.0f - quadDiagLen) * Vec2((float)(quadNdx % 3), (float)(quadNdx / 3)) / 2.0f -
                          0.5f * (2.0f - quadDiagLen);

            renderTriangle(corners[(0 + quadNdx) % 4] + center, corners[(1 + quadNdx) % 4] + center,
                           corners[(2 + quadNdx) % 4] + center, Vec4(0.5f, 0.5f, 0.5f, 1.0f));

            if (quadNdx >= 4)
            {
                renderTriangle(corners[(3 + quadNdx) % 4] + center, corners[(2 + quadNdx) % 4] + center,
                               corners[(0 + quadNdx) % 4] + center, Vec4(0.5f, 0.5f, 0.5f, 1.0f));
            }
            else
            {
                renderTriangle(corners[(0 + quadNdx) % 4] + center, corners[(2 + quadNdx) % 4] + center,
                               corners[(3 + quadNdx) % 4] + center, Vec4(0.5f, 0.5f, 0.5f, 1.0f));
            }

            // The size of the "interior" of a quad is assumed to be approximately unicolorRegionScale*<actual size of quad>.
            // By "interior" we here mean the region of non-boundary pixels of the rendered quad for which we can safely assume
            // that it has all coverage bits set to 1, for every pixel.
            float unicolorRegionScale = 1.0f - 6.0f * 2.0f / (float)m_viewportSize / quadDiagLen;
            unicoloredRegions.push_back(
                QuadCorners((center + corners[0] * unicolorRegionScale), (center + corners[1] * unicolorRegionScale),
                            (center + corners[2] * unicolorRegionScale), (center + corners[3] * unicolorRegionScale)));
        }
    }
    else if (m_caseType == CASETYPE_BIGGER_THAN_VIEWPORT_QUAD)
    {
        // Draw a bigger-than-viewport quad, rotated at an angle depending on m_currentIteration.

        int quadBaseAngleNdx = m_currentIteration / 8;
        int quadSubAngleNdx  = m_currentIteration % 8;
        float angleCos;
        float angleSin;

        if (quadBaseAngleNdx == 0)
        {
            angleCos = 1.0f;
            angleSin = 0.0f;
        }
        else if (quadBaseAngleNdx == 1)
        {
            angleCos = SQRT_HALF;
            angleSin = SQRT_HALF;
        }
        else
        {
            float angle = 0.5f * DE_PI * (float)(m_currentIteration - 1) / (float)(m_numIterations - 1);
            angleCos    = deFloatCos(angle);
            angleSin    = deFloatSin(angle);
        }

        float quadDiagLen = 2.5f / de::max(angleCos, angleSin);

        Vec2 corners[4] = {
            0.5f * quadDiagLen * Vec2(angleCos, angleSin), 0.5f * quadDiagLen * Vec2(-angleSin, angleCos),
            0.5f * quadDiagLen * Vec2(-angleCos, -angleSin), 0.5f * quadDiagLen * Vec2(angleSin, -angleCos)};

        renderTriangle(corners[(0 + quadSubAngleNdx) % 4], corners[(1 + quadSubAngleNdx) % 4],
                       corners[(2 + quadSubAngleNdx) % 4], Vec4(0.5f, 0.5f, 0.5f, 1.0f));

        if (quadSubAngleNdx >= 4)
        {
            renderTriangle(corners[(3 + quadSubAngleNdx) % 4], corners[(2 + quadSubAngleNdx) % 4],
                           corners[(0 + quadSubAngleNdx) % 4], Vec4(0.5f, 0.5f, 0.5f, 1.0f));
        }
        else
        {
            renderTriangle(corners[(0 + quadSubAngleNdx) % 4], corners[(2 + quadSubAngleNdx) % 4],
                           corners[(3 + quadSubAngleNdx) % 4], Vec4(0.5f, 0.5f, 0.5f, 1.0f));
        }

        float unicolorRegionScale = 1.0f - 6.0f * 2.0f / (float)m_viewportSize / quadDiagLen;
        unicoloredRegions.push_back(QuadCorners((corners[0] * unicolorRegionScale), (corners[1] * unicolorRegionScale),
                                                (corners[2] * unicolorRegionScale),
                                                (corners[3] * unicolorRegionScale)));
    }
    else if (m_caseType == CASETYPE_FIT_VIEWPORT_QUAD)
    {
        // Draw an exactly viewport-sized quad, rotated by multiples of 90 degrees angle depending on m_currentIteration.

        int quadSubAngleNdx = m_currentIteration % 8;

        Vec2 corners[4] = {Vec2(1.0f, 1.0f), Vec2(-1.0f, 1.0f), Vec2(-1.0f, -1.0f), Vec2(1.0f, -1.0f)};

        renderTriangle(corners[(0 + quadSubAngleNdx) % 4], corners[(1 + quadSubAngleNdx) % 4],
                       corners[(2 + quadSubAngleNdx) % 4], Vec4(0.5f, 0.5f, 0.5f, 1.0f));

        if (quadSubAngleNdx >= 4)
        {
            renderTriangle(corners[(3 + quadSubAngleNdx) % 4], corners[(2 + quadSubAngleNdx) % 4],
                           corners[(0 + quadSubAngleNdx) % 4], Vec4(0.5f, 0.5f, 0.5f, 1.0f));
        }
        else
        {
            renderTriangle(corners[(0 + quadSubAngleNdx) % 4], corners[(2 + quadSubAngleNdx) % 4],
                           corners[(3 + quadSubAngleNdx) % 4], Vec4(0.5f, 0.5f, 0.5f, 1.0f));
        }

        unicoloredRegions.push_back(QuadCorners(corners[0], corners[1], corners[2], corners[3]));
    }
    else
        DE_ASSERT(false);

    // Read pixels and check unicolored regions.

    readImage(renderedImg);

    tcu::clear(errorImg.getAccess(), Vec4(0.0f, 1.0f, 0.0f, 1.0f));

    log << TestLog::Image("RenderedImage", "Rendered image", renderedImg, QP_IMAGE_COMPRESSION_MODE_PNG);

    bool errorsDetected = false;
    for (int i = 0; i < (int)unicoloredRegions.size(); i++)
    {
        const QuadCorners &region  = unicoloredRegions[i];
        IVec2 p0Win                = ((region.p0 + 1.0f) * 0.5f * (float)(m_viewportSize - 1) + 0.5f).asInt();
        IVec2 p1Win                = ((region.p1 + 1.0f) * 0.5f * (float)(m_viewportSize - 1) + 0.5f).asInt();
        IVec2 p2Win                = ((region.p2 + 1.0f) * 0.5f * (float)(m_viewportSize - 1) + 0.5f).asInt();
        IVec2 p3Win                = ((region.p3 + 1.0f) * 0.5f * (float)(m_viewportSize - 1) + 0.5f).asInt();
        bool errorsInCurrentRegion = !isPixelRegionUnicolored(renderedImg, p0Win, p1Win, p2Win, p3Win);

        if (errorsInCurrentRegion)
            drawUnicolorTestErrors(renderedImg, errorImg.getAccess(), p0Win, p1Win, p2Win, p3Win);

        errorsDetected = errorsDetected || errorsInCurrentRegion;
    }

    m_currentIteration++;

    if (errorsDetected)
    {
        log << TestLog::Message << "Failure: Not all quad interiors seem unicolored - common-edge artifacts?"
            << TestLog::EndMessage;
        log << TestLog::Message << "Erroneous pixels are drawn red in the following image" << TestLog::EndMessage;
        log << TestLog::Image("RenderedImageWithErrors", "Rendered image with errors marked", renderedImg,
                              QP_IMAGE_COMPRESSION_MODE_PNG);
        log << TestLog::Image("ErrorsOnly", "Image with error pixels only", errorImg, QP_IMAGE_COMPRESSION_MODE_PNG);
        m_context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Failed");
        return STOP;
    }
    else if (m_currentIteration < m_numIterations)
    {
        log << TestLog::Message << "Quads seem OK - moving on to next pattern" << TestLog::EndMessage;
        return CONTINUE;
    }
    else
    {
        log << TestLog::Message << "Success: All quad interiors seem unicolored (no common-edge artifacts)"
            << TestLog::EndMessage;
        m_context.getTestContext().setTestResult(QP_TEST_RESULT_PASS, "Passed");
        return STOP;
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Test that depth values are per-sample.
 *
 * Draws intersecting, differently-colored polygons and checks that there
 * are at least GL_SAMPLES+1 distinct colors present, due to some of the
 * samples at the intersection line belonging to one and some to another
 * polygon.
 *//*--------------------------------------------------------------------*/
class SampleDepthCase : public NumSamplesCase
{
public:
    SampleDepthCase(Context &context, const char *name, const char *description);
    ~SampleDepthCase(void)
    {
    }

    void init(void);

protected:
    void renderPattern(void) const;
};

SampleDepthCase::SampleDepthCase(Context &context, const char *name, const char *description)
    : NumSamplesCase(context, name, description)
{
}

void SampleDepthCase::init(void)
{
    TestLog &log = m_testCtx.getLog();

    if (m_context.getRenderTarget().getDepthBits() == 0)
        TCU_THROW(NotSupportedError, "Test requires depth buffer");

    MultisampleCase::init();

    GLU_CHECK_CALL(glEnable(GL_DEPTH_TEST));
    GLU_CHECK_CALL(glDepthFunc(GL_LESS));

    log << TestLog::Message << "Depth test enabled, depth func is GL_LESS" << TestLog::EndMessage;
    log << TestLog::Message << "Drawing several bigger-than-viewport black or white polygons intersecting each other"
        << TestLog::EndMessage;
}

void SampleDepthCase::renderPattern(void) const
{
    GLU_CHECK_CALL(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
    GLU_CHECK_CALL(glClearDepthf(1.0f));
    GLU_CHECK_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    {
        const int numPolygons = 50;

        for (int i = 0; i < numPolygons; i++)
        {
            Vec4 color  = i % 2 == 0 ? Vec4(1.0f, 1.0f, 1.0f, 1.0f) : Vec4(0.0f, 0.0f, 0.0f, 1.0f);
            float angle = 2.0f * DE_PI * (float)i / (float)numPolygons + 0.001f * (float)m_currentIteration;
            Vec3 pt0(3.0f * deFloatCos(angle + 2.0f * DE_PI * 0.0f / 3.0f),
                     3.0f * deFloatSin(angle + 2.0f * DE_PI * 0.0f / 3.0f), 1.0f);
            Vec3 pt1(3.0f * deFloatCos(angle + 2.0f * DE_PI * 1.0f / 3.0f),
                     3.0f * deFloatSin(angle + 2.0f * DE_PI * 1.0f / 3.0f), 0.0f);
            Vec3 pt2(3.0f * deFloatCos(angle + 2.0f * DE_PI * 2.0f / 3.0f),
                     3.0f * deFloatSin(angle + 2.0f * DE_PI * 2.0f / 3.0f), 0.0f);

            renderTriangle(pt0, pt1, pt2, color);
        }
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Test that stencil buffer values are per-sample.
 *
 * Draws a unicolored pattern and marks drawn samples in stencil buffer;
 * then clears and draws a viewport-size quad with that color and with
 * proper stencil test such that the resulting image should be exactly the
 * same as after the pattern was first drawn.
 *//*--------------------------------------------------------------------*/
class SampleStencilCase : public MultisampleCase
{
public:
    SampleStencilCase(Context &context, const char *name, const char *description);
    ~SampleStencilCase(void)
    {
    }

    void init(void);
    IterateResult iterate(void);

protected:
    int getDesiredViewportSize(void) const
    {
        return 256;
    }
};

SampleStencilCase::SampleStencilCase(Context &context, const char *name, const char *description)
    : MultisampleCase(context, name, description)
{
}

void SampleStencilCase::init(void)
{
    if (m_context.getRenderTarget().getStencilBits() == 0)
        TCU_THROW(NotSupportedError, "Test requires stencil buffer");

    MultisampleCase::init();
}

SampleStencilCase::IterateResult SampleStencilCase::iterate(void)
{
    TestLog &log = m_testCtx.getLog();
    tcu::Surface renderedImgFirst(m_viewportSize, m_viewportSize);
    tcu::Surface renderedImgSecond(m_viewportSize, m_viewportSize);

    randomizeViewport();

    GLU_CHECK_CALL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
    GLU_CHECK_CALL(glClearStencil(0));
    GLU_CHECK_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
    GLU_CHECK_CALL(glEnable(GL_STENCIL_TEST));
    GLU_CHECK_CALL(glStencilFunc(GL_ALWAYS, 1, 1));
    GLU_CHECK_CALL(glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE));

    log << TestLog::Message
        << "Drawing a pattern with glStencilFunc(GL_ALWAYS, 1, 1) and glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE)"
        << TestLog::EndMessage;

    {
        const int numTriangles = 25;
        for (int i = 0; i < numTriangles; i++)
        {
            float angle0 = 2.0f * DE_PI * (float)i / (float)numTriangles;
            float angle1 = 2.0f * DE_PI * ((float)i + 0.5f) / (float)numTriangles;

            renderTriangle(Vec2(0.0f, 0.0f), Vec2(deFloatCos(angle0) * 0.95f, deFloatSin(angle0) * 0.95f),
                           Vec2(deFloatCos(angle1) * 0.95f, deFloatSin(angle1) * 0.95f), Vec4(1.0f));
        }
    }

    readImage(renderedImgFirst);
    log << TestLog::Image("RenderedImgFirst", "First image rendered", renderedImgFirst, QP_IMAGE_COMPRESSION_MODE_PNG);

    log << TestLog::Message << "Clearing color buffer to black" << TestLog::EndMessage;

    GLU_CHECK_CALL(glClear(GL_COLOR_BUFFER_BIT));
    GLU_CHECK_CALL(glStencilFunc(GL_EQUAL, 1, 1));
    GLU_CHECK_CALL(glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP));

    {
        log << TestLog::Message << "Checking that color buffer was actually cleared to black" << TestLog::EndMessage;

        tcu::Surface clearedImg(m_viewportSize, m_viewportSize);
        readImage(clearedImg);

        for (int y = 0; y < clearedImg.getHeight(); y++)
            for (int x = 0; x < clearedImg.getWidth(); x++)
            {
                const tcu::RGBA &clr = clearedImg.getPixel(x, y);
                if (clr != tcu::RGBA::black())
                {
                    log << TestLog::Message << "Failure: first non-black pixel, color " << clr
                        << ", detected at coordinates (" << x << ", " << y << ")" << TestLog::EndMessage;
                    log << TestLog::Image("ClearedImg", "Image after clearing, erroneously non-black", clearedImg);
                    m_context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Failed");
                    return STOP;
                }
            }
    }

    log << TestLog::Message
        << "Drawing a viewport-sized quad with glStencilFunc(GL_EQUAL, 1, 1) and glStencilOp(GL_KEEP, GL_KEEP, "
           "GL_KEEP) - should result in same image as the first"
        << TestLog::EndMessage;

    renderQuad(Vec2(-1.0f, -1.0f), Vec2(1.0f, -1.0f), Vec2(-1.0f, 1.0f), Vec2(1.0f, 1.0f), Vec4(1.0f));

    readImage(renderedImgSecond);
    log << TestLog::Image("RenderedImgSecond", "Second image rendered", renderedImgSecond,
                          QP_IMAGE_COMPRESSION_MODE_PNG);

    bool passed = tcu::pixelThresholdCompare(log, "ImageCompare", "Image comparison", renderedImgFirst,
                                             renderedImgSecond, tcu::RGBA(0), tcu::COMPARE_LOG_ON_ERROR);

    if (passed)
        log << TestLog::Message << "Success: The two images rendered are identical" << TestLog::EndMessage;

    m_context.getTestContext().setTestResult(passed ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                             passed ? "Passed" : "Failed");

    return STOP;
}

/*--------------------------------------------------------------------*//*!
 * \brief Tests coverage mask generation proportionality property.
 *
 * Tests that the number of coverage bits in a coverage mask created by
 * GL_SAMPLE_ALPHA_TO_COVERAGE or GL_SAMPLE_COVERAGE is, on average,
 * proportional to the alpha or coverage value, respectively. Draws
 * multiple frames, each time increasing the alpha or coverage value used,
 * and checks that the average color is changing appropriately.
 *//*--------------------------------------------------------------------*/
class MaskProportionalityCase : public MultisampleCase
{
public:
    enum CaseType
    {
        CASETYPE_ALPHA_TO_COVERAGE = 0,
        CASETYPE_SAMPLE_COVERAGE,
        CASETYPE_SAMPLE_COVERAGE_INVERTED,

        CASETYPE_LAST
    };

    MaskProportionalityCase(Context &context, const char *name, const char *description, CaseType type);
    ~MaskProportionalityCase(void)
    {
    }

    void init(void);

    IterateResult iterate(void);

protected:
    int getDesiredViewportSize(void) const
    {
        return 32;
    }

private:
    const CaseType m_type;

    int m_numIterations;
    int m_currentIteration;

    int32_t m_previousIterationColorSum;
};

MaskProportionalityCase::MaskProportionalityCase(Context &context, const char *name, const char *description,
                                                 CaseType type)
    : MultisampleCase(context, name, description)
    , m_type(type)
    , m_numIterations(0)
    , m_currentIteration(0)
    , m_previousIterationColorSum(-1)
{
}

void MaskProportionalityCase::init(void)
{
    TestLog &log = m_testCtx.getLog();

    MultisampleCase::init();

    if (m_type == CASETYPE_ALPHA_TO_COVERAGE)
    {
        GLU_CHECK_CALL(glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE));
        log << TestLog::Message << "GL_SAMPLE_ALPHA_TO_COVERAGE is enabled" << TestLog::EndMessage;
    }
    else
    {
        DE_ASSERT(m_type == CASETYPE_SAMPLE_COVERAGE || m_type == CASETYPE_SAMPLE_COVERAGE_INVERTED);

        GLU_CHECK_CALL(glEnable(GL_SAMPLE_COVERAGE));
        log << TestLog::Message << "GL_SAMPLE_COVERAGE is enabled" << TestLog::EndMessage;
    }

    m_numIterations = de::max(2, getIterationCount(m_testCtx, m_numSamples * 5));

    randomizeViewport(); // \note Using the same viewport for every iteration since coverage mask may depend on window-relative pixel coordinate.
}

MaskProportionalityCase::IterateResult MaskProportionalityCase::iterate(void)
{
    TestLog &log = m_testCtx.getLog();
    tcu::Surface renderedImg(m_viewportSize, m_viewportSize);
    int32_t numPixels = (int32_t)renderedImg.getWidth() * (int32_t)renderedImg.getHeight();

    log << TestLog::Message << "Clearing color to black" << TestLog::EndMessage;
    GLU_CHECK_CALL(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));
    GLU_CHECK_CALL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
    GLU_CHECK_CALL(glClear(GL_COLOR_BUFFER_BIT));

    if (m_type == CASETYPE_ALPHA_TO_COVERAGE)
    {
        GLU_CHECK_CALL(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE));
        log << TestLog::Message << "Using color mask TRUE, TRUE, TRUE, FALSE" << TestLog::EndMessage;
    }

    // Draw quad.

    {
        const Vec2 pt0(-1.0f, -1.0f);
        const Vec2 pt1(1.0f, -1.0f);
        const Vec2 pt2(-1.0f, 1.0f);
        const Vec2 pt3(1.0f, 1.0f);
        Vec4 quadColor(1.0f, 0.0f, 0.0f, 1.0f);
        float alphaOrCoverageValue = (float)m_currentIteration / (float)(m_numIterations - 1);

        if (m_type == CASETYPE_ALPHA_TO_COVERAGE)
        {
            log << TestLog::Message
                << "Drawing a red quad using alpha value " + de::floatToString(alphaOrCoverageValue, 2)
                << TestLog::EndMessage;
            quadColor.w() = alphaOrCoverageValue;
        }
        else
        {
            DE_ASSERT(m_type == CASETYPE_SAMPLE_COVERAGE || m_type == CASETYPE_SAMPLE_COVERAGE_INVERTED);

            bool isInverted     = m_type == CASETYPE_SAMPLE_COVERAGE_INVERTED;
            float coverageValue = isInverted ? 1.0f - alphaOrCoverageValue : alphaOrCoverageValue;
            log << TestLog::Message
                << "Drawing a red quad using sample coverage value " + de::floatToString(coverageValue, 2)
                << (isInverted ? " (inverted)" : "") << TestLog::EndMessage;
            GLU_CHECK_CALL(glSampleCoverage(coverageValue, isInverted ? GL_TRUE : GL_FALSE));
        }

        renderQuad(pt0, pt1, pt2, pt3, quadColor);
    }

    // Read ang log image.

    readImage(renderedImg);

    log << TestLog::Image("RenderedImage", "Rendered image", renderedImg, QP_IMAGE_COMPRESSION_MODE_PNG);

    // Compute average red component in rendered image.

    int32_t sumRed = 0;

    for (int y = 0; y < renderedImg.getHeight(); y++)
        for (int x = 0; x < renderedImg.getWidth(); x++)
            sumRed += renderedImg.getPixel(x, y).getRed();

    log << TestLog::Message
        << "Average red color component: " << de::floatToString((float)sumRed / 255.0f / (float)numPixels, 2)
        << TestLog::EndMessage;

    // Check if average color has decreased from previous frame's color.

    if (sumRed < m_previousIterationColorSum)
    {
        log << TestLog::Message << "Failure: Current average red color component is lower than previous"
            << TestLog::EndMessage;
        m_context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Failed");
        return STOP;
    }

    // Check if coverage mask is not all-zeros if alpha or coverage value is 0 (or 1, if inverted).

    if (m_currentIteration == 0 && sumRed != 0)
    {
        log << TestLog::Message << "Failure: Image should be completely black" << TestLog::EndMessage;
        m_context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Failed");
        return STOP;
    }

    if (m_currentIteration == m_numIterations - 1 && sumRed != 0xff * numPixels)
    {
        log << TestLog::Message << "Failure: Image should be completely red" << TestLog::EndMessage;

        m_context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Failed");
        return STOP;
    }

    m_previousIterationColorSum = sumRed;

    m_currentIteration++;

    if (m_currentIteration >= m_numIterations)
    {
        log << TestLog::Message
            << "Success: Number of coverage mask bits set appears to be, on average, proportional to "
            << (m_type == CASETYPE_ALPHA_TO_COVERAGE ? "alpha" :
                m_type == CASETYPE_SAMPLE_COVERAGE   ? "sample coverage value" :
                                                       "inverted sample coverage value")
            << TestLog::EndMessage;

        m_context.getTestContext().setTestResult(QP_TEST_RESULT_PASS, "Passed");
        return STOP;
    }
    else
        return CONTINUE;
}

/*--------------------------------------------------------------------*//*!
 * \brief Tests coverage mask generation constancy property.
 *
 * Tests that the coverage mask created by GL_SAMPLE_ALPHA_TO_COVERAGE or
 * GL_SAMPLE_COVERAGE is constant at given pixel coordinates, with a given
 * alpha component or coverage value, respectively. Draws two quads, with
 * the second one fully overlapping the first one such that at any given
 * pixel, both quads have the same alpha or coverage value. This way, if
 * the constancy property is fulfilled, only the second quad should be
 * visible.
 *//*--------------------------------------------------------------------*/
class MaskConstancyCase : public MultisampleCase
{
public:
    enum CaseType
    {
        CASETYPE_ALPHA_TO_COVERAGE = 0,    //!< Use only alpha-to-coverage.
        CASETYPE_SAMPLE_COVERAGE,          //!< Use only sample coverage.
        CASETYPE_SAMPLE_COVERAGE_INVERTED, //!< Use only inverted sample coverage.
        CASETYPE_BOTH,                     //!< Use both alpha-to-coverage and sample coverage.
        CASETYPE_BOTH_INVERTED,            //!< Use both alpha-to-coverage and inverted sample coverage.

        CASETYPE_LAST
    };

    MaskConstancyCase(Context &context, const char *name, const char *description, CaseType type);
    ~MaskConstancyCase(void)
    {
    }

    IterateResult iterate(void);

protected:
    int getDesiredViewportSize(void) const
    {
        return 256;
    }

private:
    const bool m_isAlphaToCoverageCase;
    const bool m_isSampleCoverageCase;
    const bool m_isInvertedSampleCoverageCase;
};

MaskConstancyCase::MaskConstancyCase(Context &context, const char *name, const char *description, CaseType type)
    : MultisampleCase(context, name, description)
    , m_isAlphaToCoverageCase(type == CASETYPE_ALPHA_TO_COVERAGE || type == CASETYPE_BOTH ||
                              type == CASETYPE_BOTH_INVERTED)
    , m_isSampleCoverageCase(type == CASETYPE_SAMPLE_COVERAGE || type == CASETYPE_SAMPLE_COVERAGE_INVERTED ||
                             type == CASETYPE_BOTH || type == CASETYPE_BOTH_INVERTED)
    , m_isInvertedSampleCoverageCase(type == CASETYPE_SAMPLE_COVERAGE_INVERTED || type == CASETYPE_BOTH_INVERTED)
{
}

MaskConstancyCase::IterateResult MaskConstancyCase::iterate(void)
{
    TestLog &log = m_testCtx.getLog();
    tcu::Surface renderedImg(m_viewportSize, m_viewportSize);

    randomizeViewport();

    log << TestLog::Message << "Clearing color to black" << TestLog::EndMessage;
    GLU_CHECK_CALL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
    GLU_CHECK_CALL(glClear(GL_COLOR_BUFFER_BIT));

    if (m_isAlphaToCoverageCase)
    {
        GLU_CHECK_CALL(glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE));
        GLU_CHECK_CALL(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE));
        log << TestLog::Message << "GL_SAMPLE_ALPHA_TO_COVERAGE is enabled" << TestLog::EndMessage;
        log << TestLog::Message << "Color mask is TRUE, TRUE, TRUE, FALSE" << TestLog::EndMessage;
    }

    if (m_isSampleCoverageCase)
    {
        GLU_CHECK_CALL(glEnable(GL_SAMPLE_COVERAGE));
        log << TestLog::Message << "GL_SAMPLE_COVERAGE is enabled" << TestLog::EndMessage;
    }

    log << TestLog::Message << "Drawing several green quads, each fully overlapped by a red quad with the same "
        << (m_isAlphaToCoverageCase ? "alpha" : "")
        << (m_isAlphaToCoverageCase && m_isSampleCoverageCase ? " and " : "")
        << (m_isInvertedSampleCoverageCase ? "inverted " : "") << (m_isSampleCoverageCase ? "sample coverage" : "")
        << " values" << TestLog::EndMessage;

    const int numQuadRowsCols = m_numSamples * 4;

    for (int row = 0; row < numQuadRowsCols; row++)
    {
        for (int col = 0; col < numQuadRowsCols; col++)
        {
            float x0 = (float)(col + 0) / (float)numQuadRowsCols * 2.0f - 1.0f;
            float x1 = (float)(col + 1) / (float)numQuadRowsCols * 2.0f - 1.0f;
            float y0 = (float)(row + 0) / (float)numQuadRowsCols * 2.0f - 1.0f;
            float y1 = (float)(row + 1) / (float)numQuadRowsCols * 2.0f - 1.0f;
            const Vec4 baseGreen(0.0f, 1.0f, 0.0f, 0.0f);
            const Vec4 baseRed(1.0f, 0.0f, 0.0f, 0.0f);
            Vec4 alpha0(0.0f, 0.0f, 0.0f, m_isAlphaToCoverageCase ? (float)col / (float)(numQuadRowsCols - 1) : 1.0f);
            Vec4 alpha1(0.0f, 0.0f, 0.0f, m_isAlphaToCoverageCase ? (float)row / (float)(numQuadRowsCols - 1) : 1.0f);

            if (m_isSampleCoverageCase)
            {
                float value = (float)(row * numQuadRowsCols + col) / (float)(numQuadRowsCols * numQuadRowsCols - 1);
                GLU_CHECK_CALL(glSampleCoverage(m_isInvertedSampleCoverageCase ? 1.0f - value : value,
                                                m_isInvertedSampleCoverageCase ? GL_TRUE : GL_FALSE));
            }

            renderQuad(Vec2(x0, y0), Vec2(x1, y0), Vec2(x0, y1), Vec2(x1, y1), baseGreen + alpha0, baseGreen + alpha1,
                       baseGreen + alpha0, baseGreen + alpha1);
            renderQuad(Vec2(x0, y0), Vec2(x1, y0), Vec2(x0, y1), Vec2(x1, y1), baseRed + alpha0, baseRed + alpha1,
                       baseRed + alpha0, baseRed + alpha1);
        }
    }

    readImage(renderedImg);

    log << TestLog::Image("RenderedImage", "Rendered image", renderedImg, QP_IMAGE_COMPRESSION_MODE_PNG);

    for (int y = 0; y < renderedImg.getHeight(); y++)
        for (int x = 0; x < renderedImg.getWidth(); x++)
        {
            if (renderedImg.getPixel(x, y).getGreen() > 0)
            {
                log << TestLog::Message
                    << "Failure: Non-zero green color component detected - should have been completely overwritten by "
                       "red quad"
                    << TestLog::EndMessage;
                m_context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Failed");
                return STOP;
            }
        }

    log << TestLog::Message << "Success: Coverage mask appears to be constant at a given pixel coordinate with a given "
        << (m_isAlphaToCoverageCase ? "alpha" : "")
        << (m_isAlphaToCoverageCase && m_isSampleCoverageCase ? " and " : "")
        << (m_isSampleCoverageCase ? "coverage value" : "") << TestLog::EndMessage;

    m_context.getTestContext().setTestResult(QP_TEST_RESULT_PASS, "Passed");

    return STOP;
}

/*--------------------------------------------------------------------*//*!
 * \brief Tests coverage mask inversion validity.
 *
 * Tests that the coverage masks obtained by glSampleCoverage(..., GL_TRUE)
 * and glSampleCoverage(..., GL_FALSE) are indeed each others' inverses.
 * This is done by drawing a pattern, with varying coverage values,
 * overlapped by a pattern that has inverted masks and is otherwise
 * identical. The resulting image is compared to one obtained by drawing
 * the same pattern but with all-ones coverage masks.
 *//*--------------------------------------------------------------------*/
class CoverageMaskInvertCase : public MultisampleCase
{
public:
    CoverageMaskInvertCase(Context &context, const char *name, const char *description);
    ~CoverageMaskInvertCase(void)
    {
    }

    IterateResult iterate(void);

protected:
    int getDesiredViewportSize(void) const
    {
        return 256;
    }

private:
    void drawPattern(bool invertSampleCoverage) const;
};

CoverageMaskInvertCase::CoverageMaskInvertCase(Context &context, const char *name, const char *description)
    : MultisampleCase(context, name, description)
{
}

void CoverageMaskInvertCase::drawPattern(bool invertSampleCoverage) const
{
    const int numTriangles = 25;
    for (int i = 0; i < numTriangles; i++)
    {
        GLU_CHECK_CALL(
            glSampleCoverage((float)i / (float)(numTriangles - 1), invertSampleCoverage ? GL_TRUE : GL_FALSE));

        float angle0 = 2.0f * DE_PI * (float)i / (float)numTriangles;
        float angle1 = 2.0f * DE_PI * ((float)i + 0.5f) / (float)numTriangles;

        renderTriangle(Vec2(0.0f, 0.0f), Vec2(deFloatCos(angle0) * 0.95f, deFloatSin(angle0) * 0.95f),
                       Vec2(deFloatCos(angle1) * 0.95f, deFloatSin(angle1) * 0.95f),
                       Vec4(0.4f + (float)i / (float)numTriangles * 0.6f, 0.5f + (float)i / (float)numTriangles * 0.3f,
                            0.6f - (float)i / (float)numTriangles * 0.5f,
                            0.7f - (float)i / (float)numTriangles * 0.7f));
    }
}

CoverageMaskInvertCase::IterateResult CoverageMaskInvertCase::iterate(void)
{
    TestLog &log = m_testCtx.getLog();
    tcu::Surface renderedImgNoSampleCoverage(m_viewportSize, m_viewportSize);
    tcu::Surface renderedImgSampleCoverage(m_viewportSize, m_viewportSize);

    randomizeViewport();

    GLU_CHECK_CALL(glEnable(GL_BLEND));
    GLU_CHECK_CALL(glBlendEquation(GL_FUNC_ADD));
    GLU_CHECK_CALL(glBlendFunc(GL_ONE, GL_ONE));
    log << TestLog::Message << "Additive blending enabled in order to detect (erroneously) overlapping samples"
        << TestLog::EndMessage;

    log << TestLog::Message << "Clearing color to all-zeros" << TestLog::EndMessage;
    GLU_CHECK_CALL(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
    GLU_CHECK_CALL(glClear(GL_COLOR_BUFFER_BIT));
    log << TestLog::Message << "Drawing the pattern with GL_SAMPLE_COVERAGE disabled" << TestLog::EndMessage;
    drawPattern(false);
    readImage(renderedImgNoSampleCoverage);

    log << TestLog::Image("RenderedImageNoSampleCoverage", "Rendered image with GL_SAMPLE_COVERAGE disabled",
                          renderedImgNoSampleCoverage, QP_IMAGE_COMPRESSION_MODE_PNG);

    log << TestLog::Message << "Clearing color to all-zeros" << TestLog::EndMessage;
    GLU_CHECK_CALL(glClear(GL_COLOR_BUFFER_BIT));
    GLU_CHECK_CALL(glEnable(GL_SAMPLE_COVERAGE));
    log << TestLog::Message << "Drawing the pattern with GL_SAMPLE_COVERAGE enabled, using non-inverted masks"
        << TestLog::EndMessage;
    drawPattern(false);
    log << TestLog::Message
        << "Drawing the pattern with GL_SAMPLE_COVERAGE enabled, using same sample coverage values but inverted masks"
        << TestLog::EndMessage;
    drawPattern(true);
    readImage(renderedImgSampleCoverage);

    log << TestLog::Image("RenderedImageSampleCoverage", "Rendered image with GL_SAMPLE_COVERAGE enabled",
                          renderedImgSampleCoverage, QP_IMAGE_COMPRESSION_MODE_PNG);

    bool passed = tcu::pixelThresholdCompare(
        log, "CoverageVsNoCoverage", "Comparison of same pattern with GL_SAMPLE_COVERAGE disabled and enabled",
        renderedImgNoSampleCoverage, renderedImgSampleCoverage, tcu::RGBA(0), tcu::COMPARE_LOG_ON_ERROR);

    if (passed)
        log << TestLog::Message << "Success: The two images rendered are identical" << TestLog::EndMessage;

    m_context.getTestContext().setTestResult(passed ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                             passed ? "Passed" : "Failed");

    return STOP;
}

MultisampleTests::MultisampleTests(Context &context) : TestCaseGroup(context, "multisample", "Multisampling tests")
{
}

MultisampleTests::~MultisampleTests(void)
{
}

void MultisampleTests::init(void)
{
    addChild(new PolygonNumSamplesCase(m_context, "num_samples_polygon",
                                       "Test sanity of the value of GL_SAMPLES, with polygons"));
    addChild(
        new LineNumSamplesCase(m_context, "num_samples_line", "Test sanity of the value of GL_SAMPLES, with lines"));
    addChild(new CommonEdgeCase(m_context, "common_edge_small_quads", "Test polygons' common edges with small quads",
                                CommonEdgeCase::CASETYPE_SMALL_QUADS));
    addChild(new CommonEdgeCase(m_context, "common_edge_big_quad",
                                "Test polygons' common edges with bigger-than-viewport quads",
                                CommonEdgeCase::CASETYPE_BIGGER_THAN_VIEWPORT_QUAD));
    addChild(new CommonEdgeCase(m_context, "common_edge_viewport_quad",
                                "Test polygons' common edges with exactly viewport-sized quads",
                                CommonEdgeCase::CASETYPE_FIT_VIEWPORT_QUAD));
    addChild(new SampleDepthCase(m_context, "depth", "Test that depth values are per-sample"));
    addChild(new SampleStencilCase(m_context, "stencil", "Test that stencil values are per-sample"));
    addChild(new CoverageMaskInvertCase(
        m_context, "sample_coverage_invert",
        "Test that non-inverted and inverted sample coverage masks are each other's negations"));

    addChild(new MaskProportionalityCase(m_context, "proportionality_alpha_to_coverage",
                                         "Test the proportionality property of GL_SAMPLE_ALPHA_TO_COVERAGE",
                                         MaskProportionalityCase::CASETYPE_ALPHA_TO_COVERAGE));
    addChild(new MaskProportionalityCase(m_context, "proportionality_sample_coverage",
                                         "Test the proportionality property of GL_SAMPLE_COVERAGE",
                                         MaskProportionalityCase::CASETYPE_SAMPLE_COVERAGE));
    addChild(new MaskProportionalityCase(m_context, "proportionality_sample_coverage_inverted",
                                         "Test the proportionality property of inverted-mask GL_SAMPLE_COVERAGE",
                                         MaskProportionalityCase::CASETYPE_SAMPLE_COVERAGE_INVERTED));

    addChild(new MaskConstancyCase(m_context, "constancy_alpha_to_coverage",
                                   "Test that coverage mask is constant at given coordinates with a given alpha or "
                                   "coverage value, using GL_SAMPLE_ALPHA_TO_COVERAGE",
                                   MaskConstancyCase::CASETYPE_ALPHA_TO_COVERAGE));
    addChild(new MaskConstancyCase(m_context, "constancy_sample_coverage",
                                   "Test that coverage mask is constant at given coordinates with a given alpha or "
                                   "coverage value, using GL_SAMPLE_COVERAGE",
                                   MaskConstancyCase::CASETYPE_SAMPLE_COVERAGE));
    addChild(new MaskConstancyCase(m_context, "constancy_sample_coverage_inverted",
                                   "Test that coverage mask is constant at given coordinates with a given alpha or "
                                   "coverage value, using inverted-mask GL_SAMPLE_COVERAGE",
                                   MaskConstancyCase::CASETYPE_SAMPLE_COVERAGE_INVERTED));
    addChild(new MaskConstancyCase(m_context, "constancy_both",
                                   "Test that coverage mask is constant at given coordinates with a given alpha or "
                                   "coverage value, using GL_SAMPLE_ALPHA_TO_COVERAGE and GL_SAMPLE_COVERAGE",
                                   MaskConstancyCase::CASETYPE_BOTH));
    addChild(
        new MaskConstancyCase(m_context, "constancy_both_inverted",
                              "Test that coverage mask is constant at given coordinates with a given alpha or coverage "
                              "value, using GL_SAMPLE_ALPHA_TO_COVERAGE and inverted-mask GL_SAMPLE_COVERAGE",
                              MaskConstancyCase::CASETYPE_BOTH_INVERTED));
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
