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
 * \brief Shader built-in variable tests.
 *//*--------------------------------------------------------------------*/

#include "es2fShaderBuiltinVarTests.hpp"
#include "glsShaderRenderCase.hpp"
#include "deRandom.hpp"
#include "deString.h"
#include "deMath.h"
#include "deStringUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuTestCase.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuImageCompare.hpp"
#include "gluPixelTransfer.hpp"
#include "gluDrawUtil.hpp"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

using std::string;
using std::vector;
using tcu::TestLog;

namespace deqp
{
namespace gles2
{
namespace Functional
{

const float builtinConstScale = 4.0f;

void evalBuiltinConstant(gls::ShaderEvalContext &c)
{
    bool isOk = 0 == (int)(deFloatFloor(c.coords.x() * builtinConstScale) + 0.05f);
    c.color   = isOk ? tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f) : tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
}

class ShaderBuiltinConstantCase : public gls::ShaderRenderCase
{
public:
    ShaderBuiltinConstantCase(Context &context, const char *name, const char *desc, const char *varName,
                              uint32_t paramName, bool isVertexCase);
    ~ShaderBuiltinConstantCase(void);

    int getRefValue(void);
    void init(void);

private:
    const std::string m_varName;
    const uint32_t m_paramName;
};

ShaderBuiltinConstantCase::ShaderBuiltinConstantCase(Context &context, const char *name, const char *desc,
                                                     const char *varName, uint32_t paramName, bool isVertexCase)
    : ShaderRenderCase(context.getTestContext(), context.getRenderContext(), context.getContextInfo(), name, desc,
                       isVertexCase, evalBuiltinConstant)
    , m_varName(varName)
    , m_paramName(paramName)
{
}

ShaderBuiltinConstantCase::~ShaderBuiltinConstantCase(void)
{
}

int ShaderBuiltinConstantCase::getRefValue(void)
{
    if (m_varName == "gl_MaxDrawBuffers")
    {
        if (m_ctxInfo.isExtensionSupported("GL_EXT_draw_buffers") ||
            m_ctxInfo.isExtensionSupported("GL_NV_draw_buffers") || m_ctxInfo.isES3Compatible())
            return m_ctxInfo.getInt(GL_MAX_DRAW_BUFFERS);
        else
            return 1;
    }
    else
    {
        DE_ASSERT(m_paramName != GL_NONE);
        return m_ctxInfo.getInt(m_paramName);
    }
}

void ShaderBuiltinConstantCase::init(void)
{
    const int refValue = getRefValue();
    m_testCtx.getLog() << tcu::TestLog::Message << m_varName << " = " << refValue << tcu::TestLog::EndMessage;

    static const char *defaultVertSrc = "attribute highp vec4 a_position;\n"
                                        "attribute highp vec4 a_coords;\n"
                                        "varying mediump vec4 v_coords;\n\n"
                                        "void main (void)\n"
                                        "{\n"
                                        "    v_coords = a_coords;\n"
                                        "    gl_Position = a_position;\n"
                                        "}\n";
    static const char *defaultFragSrc = "varying mediump vec4 v_color;\n\n"
                                        "void main (void)\n"
                                        "{\n"
                                        "    gl_FragColor = v_color;\n"
                                        "}\n";

    // Construct shader.
    std::ostringstream src;
    if (m_isVertexCase)
    {
        src << "attribute highp vec4 a_position;\n"
            << "attribute highp vec4 a_coords;\n"
            << "varying mediump vec4 v_color;\n";
    }
    else
        src << "varying mediump vec4 v_coords;\n";

    src << "void main (void)\n{\n";

    src << "\tbool isOk = " << m_varName << " == (" << refValue << " + int(floor("
        << (m_isVertexCase ? "a_coords" : "v_coords") << ".x * " << de::floatToString(builtinConstScale, 1)
        << ") + 0.05));\n";
    src << "\t" << (m_isVertexCase ? "v_color" : "gl_FragColor")
        << " = isOk ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);\n";

    if (m_isVertexCase)
        src << "\tgl_Position = a_position;\n";

    src << "}\n";

    m_vertShaderSource = m_isVertexCase ? src.str() : defaultVertSrc;
    m_fragShaderSource = m_isVertexCase ? defaultFragSrc : src.str();

    gls::ShaderRenderCase::init();
}

namespace
{

struct DepthRangeParams
{
    DepthRangeParams(void) : zNear(0.0f), zFar(1.0f)
    {
    }

    DepthRangeParams(float zNear_, float zFar_) : zNear(zNear_), zFar(zFar_)
    {
    }

    float zNear;
    float zFar;
};

class DepthRangeEvaluator : public gls::ShaderEvaluator
{
public:
    DepthRangeEvaluator(const DepthRangeParams &params) : m_params(params)
    {
    }

    void evaluate(gls::ShaderEvalContext &c)
    {
        float zNear   = deFloatClamp(m_params.zNear, 0.0f, 1.0f);
        float zFar    = deFloatClamp(m_params.zFar, 0.0f, 1.0f);
        float diff    = zFar - zNear;
        c.color.xyz() = tcu::Vec3(zNear, zFar, diff * 0.5f + 0.5f);
    }

private:
    const DepthRangeParams &m_params;
};

} // namespace

class ShaderDepthRangeTest : public gls::ShaderRenderCase
{
public:
    ShaderDepthRangeTest(Context &context, const char *name, const char *desc, bool isVertexCase)
        : ShaderRenderCase(context.getTestContext(), context.getRenderContext(), context.getContextInfo(), name, desc,
                           isVertexCase, m_evaluator)
        , m_evaluator(m_depthRange)
        , m_iterNdx(0)
    {
    }

    void init(void)
    {
        static const char *defaultVertSrc = "attribute highp vec4 a_position;\n"
                                            "void main (void)\n"
                                            "{\n"
                                            "    gl_Position = a_position;\n"
                                            "}\n";
        static const char *defaultFragSrc = "varying mediump vec4 v_color;\n\n"
                                            "void main (void)\n"
                                            "{\n"
                                            "    gl_FragColor = v_color;\n"
                                            "}\n";

        // Construct shader.
        std::ostringstream src;
        if (m_isVertexCase)
            src << "attribute highp vec4 a_position;\n"
                << "varying mediump vec4 v_color;\n";

        src << "void main (void)\n{\n";
        src << "\t" << (m_isVertexCase ? "v_color" : "gl_FragColor")
            << " = vec4(gl_DepthRange.near, gl_DepthRange.far, gl_DepthRange.diff*0.5 + 0.5, 1.0);\n";

        if (m_isVertexCase)
            src << "\tgl_Position = a_position;\n";

        src << "}\n";

        m_vertShaderSource = m_isVertexCase ? src.str() : defaultVertSrc;
        m_fragShaderSource = m_isVertexCase ? defaultFragSrc : src.str();

        gls::ShaderRenderCase::init();
    }

    IterateResult iterate(void)
    {
        const glw::Functions &gl = m_renderCtx.getFunctions();

        const DepthRangeParams cases[] = {DepthRangeParams(0.0f, 1.0f), DepthRangeParams(1.5f, -1.0f),
                                          DepthRangeParams(0.7f, 0.3f)};

        m_depthRange = cases[m_iterNdx];
        m_testCtx.getLog() << tcu::TestLog::Message << "glDepthRangef(" << m_depthRange.zNear << ", "
                           << m_depthRange.zFar << ")" << tcu::TestLog::EndMessage;
        gl.depthRangef(m_depthRange.zNear, m_depthRange.zFar);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDepthRangef()");

        gls::ShaderRenderCase::iterate();
        m_iterNdx += 1;

        if (m_iterNdx == DE_LENGTH_OF_ARRAY(cases) || m_testCtx.getTestResult() != QP_TEST_RESULT_PASS)
            return STOP;
        else
            return CONTINUE;
    }

private:
    DepthRangeParams m_depthRange;
    DepthRangeEvaluator m_evaluator;
    int m_iterNdx;
};

class FragCoordXYZCase : public TestCase
{
public:
    FragCoordXYZCase(Context &context) : TestCase(context, "fragcoord_xyz", "gl_FragCoord.xyz Test")
    {
    }

    IterateResult iterate(void)
    {
        TestLog &log             = m_testCtx.getLog();
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        const int width          = m_context.getRenderTarget().getWidth();
        const int height         = m_context.getRenderTarget().getHeight();
        const tcu::RGBA threshold =
            tcu::RGBA(1, 1, 1, 1) + m_context.getRenderTarget().getPixelFormat().getColorThreshold();
        const tcu::Vec3 scale(1.f / float(width), 1.f / float(height), 1.0f);

        tcu::Surface testImg(width, height);
        tcu::Surface refImg(width, height);

        const glu::ShaderProgram program(
            m_context.getRenderContext(),
            glu::makeVtxFragSources("attribute highp vec4 a_position;\n"
                                    "void main (void)\n"
                                    "{\n"
                                    "    gl_Position = a_position;\n"
                                    "}\n",

                                    "uniform mediump vec3 u_scale;\n"
                                    "void main (void)\n"
                                    "{\n"
                                    "    gl_FragColor = vec4(gl_FragCoord.xyz*u_scale, 1.0);\n"
                                    "}\n"));

        log << program;

        if (!program.isOk())
            throw tcu::TestError("Compile failed");

        // Draw with GL.
        {
            const float positions[]  = {-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 1.0f,
                                        1.0f,  1.0f, 0.0f,  1.0f, 1.0f,  -1.0f, 1.0f, 1.0f};
            const uint16_t indices[] = {0, 1, 2, 2, 1, 3};

            const int scaleLoc                 = gl.getUniformLocation(program.getProgram(), "u_scale");
            glu::VertexArrayBinding posBinding = glu::va::Float("a_position", 4, 4, 0, &positions[0]);

            gl.useProgram(program.getProgram());
            gl.uniform3fv(scaleLoc, 1, scale.getPtr());

            glu::draw(m_context.getRenderContext(), program.getProgram(), 1, &posBinding,
                      glu::pr::Triangles(DE_LENGTH_OF_ARRAY(indices), &indices[0]));

            glu::readPixels(m_context.getRenderContext(), 0, 0, testImg.getAccess());
            GLU_EXPECT_NO_ERROR(gl.getError(), "Draw");
        }

        // Draw reference
        for (int y = 0; y < refImg.getHeight(); y++)
        {
            for (int x = 0; x < refImg.getWidth(); x++)
            {
                const float xf = (float(x) + .5f) / float(refImg.getWidth());
                const float yf = (float(refImg.getHeight() - y - 1) + .5f) / float(refImg.getHeight());
                const float z  = (xf + yf) / 2.0f;
                const tcu::Vec3 fragCoord(float(x) + .5f, float(y) + .5f, z);
                const tcu::Vec3 scaledFC = fragCoord * scale;
                const tcu::Vec4 color(scaledFC.x(), scaledFC.y(), scaledFC.z(), 1.0f);

                refImg.setPixel(x, y, tcu::RGBA(color));
            }
        }

        // Compare
        {
            bool isOk = tcu::pixelThresholdCompare(log, "Result", "Image comparison result", refImg, testImg, threshold,
                                                   tcu::COMPARE_LOG_RESULT);
            m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                    isOk ? "Pass" : "Image comparison failed");
        }

        return STOP;
    }
};

static inline float projectedTriInterpolate(const tcu::Vec3 &s, const tcu::Vec3 &w, float nx, float ny)
{
    return (s[0] * (1.0f - nx - ny) / w[0] + s[1] * ny / w[1] + s[2] * nx / w[2]) /
           ((1.0f - nx - ny) / w[0] + ny / w[1] + nx / w[2]);
}

class FragCoordWCase : public TestCase
{
public:
    FragCoordWCase(Context &context) : TestCase(context, "fragcoord_w", "gl_FragCoord.w Test")
    {
    }

    IterateResult iterate(void)
    {
        TestLog &log             = m_testCtx.getLog();
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        const int width          = m_context.getRenderTarget().getWidth();
        const int height         = m_context.getRenderTarget().getHeight();
        const tcu::RGBA threshold =
            tcu::RGBA(1, 1, 1, 1) + m_context.getRenderTarget().getPixelFormat().getColorThreshold();

        tcu::Surface testImg(width, height);
        tcu::Surface refImg(width, height);

        const float w[4] = {1.7f, 2.0f, 1.2f, 1.0f};

        const glu::ShaderProgram program(
            m_context.getRenderContext(),
            glu::makeVtxFragSources("attribute highp vec4 a_position;\n"
                                    "void main (void)\n"
                                    "{\n"
                                    "    gl_Position = a_position;\n"
                                    "}\n",

                                    "void main (void)\n"
                                    "{\n"
                                    "    gl_FragColor = vec4(0.0, 1.0/gl_FragCoord.w - 1.0, 0.0, 1.0);\n"
                                    "}\n"));

        log << program;

        if (!program.isOk())
            throw tcu::TestError("Compile failed");

        // Draw with GL.
        {
            const float positions[]  = {-w[0], w[0], 0.0f, w[0], -w[1], -w[1], 0.0f, w[1],
                                        w[2],  w[2], 0.0f, w[2], w[3],  -w[3], 0.0f, w[3]};
            const uint16_t indices[] = {0, 1, 2, 2, 1, 3};

            glu::VertexArrayBinding posBinding = glu::va::Float("a_position", 4, 4, 0, &positions[0]);

            gl.useProgram(program.getProgram());

            glu::draw(m_context.getRenderContext(), program.getProgram(), 1, &posBinding,
                      glu::pr::Triangles(DE_LENGTH_OF_ARRAY(indices), &indices[0]));

            glu::readPixels(m_context.getRenderContext(), 0, 0, testImg.getAccess());
            GLU_EXPECT_NO_ERROR(gl.getError(), "Draw");
        }

        // Draw reference
        for (int y = 0; y < refImg.getHeight(); y++)
        {
            for (int x = 0; x < refImg.getWidth(); x++)
            {
                const float xf = (float(x) + .5f) / float(refImg.getWidth());
                const float yf = (float(refImg.getHeight() - y - 1) + .5f) / float(refImg.getHeight());
                const float oow =
                    ((xf + yf) < 1.0f) ?
                        projectedTriInterpolate(tcu::Vec3(w[0], w[1], w[2]), tcu::Vec3(w[0], w[1], w[2]), xf, yf) :
                        projectedTriInterpolate(tcu::Vec3(w[3], w[2], w[1]), tcu::Vec3(w[3], w[2], w[1]), 1.0f - xf,
                                                1.0f - yf);
                const tcu::Vec4 color(0.0f, oow - 1.0f, 0.0f, 1.0f);

                refImg.setPixel(x, y, tcu::RGBA(color));
            }
        }

        // Compare
        {
            bool isOk = tcu::pixelThresholdCompare(log, "Result", "Image comparison result", refImg, testImg, threshold,
                                                   tcu::COMPARE_LOG_RESULT);
            m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                    isOk ? "Pass" : "Image comparison failed");
        }

        return STOP;
    }
};

class PointCoordCase : public TestCase
{
public:
    PointCoordCase(Context &context) : TestCase(context, "pointcoord", "gl_PointCoord Test")
    {
    }

    IterateResult iterate(void)
    {
        TestLog &log             = m_testCtx.getLog();
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        const int width          = de::min(256, m_context.getRenderTarget().getWidth());
        const int height         = de::min(256, m_context.getRenderTarget().getHeight());
        const float threshold    = 0.02f;

        const int numPoints = 8;

        vector<tcu::Vec3> coords(numPoints);
        float pointSizeRange[2] = {0.0f, 0.0f};

        de::Random rnd(0x145fa);
        tcu::Surface testImg(width, height);
        tcu::Surface refImg(width, height);

        gl.getFloatv(GL_ALIASED_POINT_SIZE_RANGE, &pointSizeRange[0]);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE)");

        if (pointSizeRange[0] <= 0.0f || pointSizeRange[1] <= 0.0f || pointSizeRange[1] < pointSizeRange[0])
            throw tcu::TestError("Invalid GL_ALIASED_POINT_SIZE_RANGE");

        // Compute coordinates.
        {

            for (vector<tcu::Vec3>::iterator coord = coords.begin(); coord != coords.end(); ++coord)
            {
                coord->x() = rnd.getFloat(-0.9f, 0.9f);
                coord->y() = rnd.getFloat(-0.9f, 0.9f);
                coord->z() = rnd.getFloat(pointSizeRange[0], pointSizeRange[1]);
            }
        }

        const glu::ShaderProgram program(
            m_context.getRenderContext(),
            glu::makeVtxFragSources("attribute highp vec3 a_positionSize;\n"
                                    "void main (void)\n"
                                    "{\n"
                                    "    gl_Position = vec4(a_positionSize.xy, 0.0, 1.0);\n"
                                    "    gl_PointSize = a_positionSize.z;\n"
                                    "}\n",

                                    "void main (void)\n"
                                    "{\n"
                                    "    gl_FragColor = vec4(gl_PointCoord, 0.0, 1.0);\n"
                                    "}\n"));

        log << program;

        if (!program.isOk())
            throw tcu::TestError("Compile failed");

        // Draw with GL.
        {
            glu::VertexArrayBinding posBinding =
                glu::va::Float("a_positionSize", 3, (int)coords.size(), 0, (const float *)&coords[0]);
            const int viewportX = rnd.getInt(0, m_context.getRenderTarget().getWidth() - width);
            const int viewportY = rnd.getInt(0, m_context.getRenderTarget().getHeight() - height);

            gl.viewport(viewportX, viewportY, width, height);
            gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
            gl.clear(GL_COLOR_BUFFER_BIT);

            gl.useProgram(program.getProgram());

            glu::draw(m_context.getRenderContext(), program.getProgram(), 1, &posBinding,
                      glu::pr::Points((int)coords.size()));

            glu::readPixels(m_context.getRenderContext(), viewportX, viewportY, testImg.getAccess());
            GLU_EXPECT_NO_ERROR(gl.getError(), "Draw");
        }

        // Draw reference
        tcu::clear(refImg.getAccess(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
        for (vector<tcu::Vec3>::const_iterator pointIter = coords.begin(); pointIter != coords.end(); ++pointIter)
        {
            const int x0 = deRoundFloatToInt32(float(width) * (pointIter->x() * 0.5f + 0.5f) - pointIter->z() * 0.5f);
            const int y0 = deRoundFloatToInt32(float(height) * (pointIter->y() * 0.5f + 0.5f) - pointIter->z() * 0.5f);
            const int x1 = deRoundFloatToInt32(float(width) * (pointIter->x() * 0.5f + 0.5f) + pointIter->z() * 0.5f);
            const int y1 = deRoundFloatToInt32(float(height) * (pointIter->y() * 0.5f + 0.5f) + pointIter->z() * 0.5f);
            const int w  = x1 - x0;
            const int h  = y1 - y0;

            for (int yo = 0; yo < h; yo++)
            {
                for (int xo = 0; xo < w; xo++)
                {
                    const float xf = (float(xo) + 0.5f) / float(w);
                    const float yf = (float(h - yo - 1) + 0.5f) / float(h);
                    const tcu::Vec4 color(xf, yf, 0.0f, 1.0f);
                    const int dx = x0 + xo;
                    const int dy = y0 + yo;

                    if (de::inBounds(dx, 0, refImg.getWidth()) && de::inBounds(dy, 0, refImg.getHeight()))
                        refImg.setPixel(dx, dy, tcu::RGBA(color));
                }
            }
        }

        // Compare
        {
            bool isOk = tcu::fuzzyCompare(log, "Result", "Image comparison result", refImg, testImg, threshold,
                                          tcu::COMPARE_LOG_RESULT);
            m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                    isOk ? "Pass" : "Image comparison failed");
        }

        return STOP;
    }
};

class FrontFacingCase : public TestCase
{
public:
    FrontFacingCase(Context &context) : TestCase(context, "frontfacing", "gl_FrontFacing Test")
    {
    }

    IterateResult iterate(void)
    {
        // Test case renders two adjecent quads, where left is has front-facing
        // triagles and right back-facing. Color is selected based on gl_FrontFacing
        // value.

        TestLog &log             = m_testCtx.getLog();
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        de::Random rnd(0x89f2c);
        const int width     = de::min(64, m_context.getRenderTarget().getWidth());
        const int height    = de::min(64, m_context.getRenderTarget().getHeight());
        const int viewportX = rnd.getInt(0, m_context.getRenderTarget().getWidth() - width);
        const int viewportY = rnd.getInt(0, m_context.getRenderTarget().getHeight() - height);
        const tcu::RGBA threshold =
            tcu::RGBA(1, 1, 1, 1) + m_context.getRenderTarget().getPixelFormat().getColorThreshold();

        tcu::Surface testImg(width, height);
        tcu::Surface refImg(width, height);

        const glu::ShaderProgram program(m_context.getRenderContext(),
                                         glu::makeVtxFragSources("attribute highp vec4 a_position;\n"
                                                                 "void main (void)\n"
                                                                 "{\n"
                                                                 "    gl_Position = a_position;\n"
                                                                 "}\n",

                                                                 "void main (void)\n"
                                                                 "{\n"
                                                                 "    if (gl_FrontFacing)\n"
                                                                 "        gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
                                                                 "    else\n"
                                                                 "        gl_FragColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
                                                                 "}\n"));

        log << program;

        if (!program.isOk())
            throw tcu::TestError("Compile failed");

        // Draw with GL.
        {
            const float positions[]     = {-1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 1.0f,
                                           1.0f,  1.0f, 0.0f, 1.0f, 1.0f,  -1.0f, 0.0f, 1.0f};
            const uint16_t indicesCCW[] = {0, 1, 2, 2, 1, 3};
            const uint16_t indicesCW[]  = {2, 1, 0, 3, 1, 2};

            glu::VertexArrayBinding posBinding = glu::va::Float("a_position", 4, 4, 0, &positions[0]);

            gl.useProgram(program.getProgram());

            gl.frontFace(GL_CCW);

            gl.viewport(viewportX, viewportY, width / 2, height / 2);
            glu::draw(m_context.getRenderContext(), program.getProgram(), 1, &posBinding,
                      glu::pr::Triangles(DE_LENGTH_OF_ARRAY(indicesCCW), &indicesCCW[0]));

            gl.viewport(viewportX + width / 2, viewportY, width - width / 2, height / 2);
            glu::draw(m_context.getRenderContext(), program.getProgram(), 1, &posBinding,
                      glu::pr::Triangles(DE_LENGTH_OF_ARRAY(indicesCW), &indicesCW[0]));

            gl.frontFace(GL_CW);

            gl.viewport(viewportX, viewportY + height / 2, width / 2, height - height / 2);
            glu::draw(m_context.getRenderContext(), program.getProgram(), 1, &posBinding,
                      glu::pr::Triangles(DE_LENGTH_OF_ARRAY(indicesCCW), &indicesCCW[0]));

            gl.viewport(viewportX + width / 2, viewportY + height / 2, width - width / 2, height - height / 2);
            glu::draw(m_context.getRenderContext(), program.getProgram(), 1, &posBinding,
                      glu::pr::Triangles(DE_LENGTH_OF_ARRAY(indicesCW), &indicesCW[0]));

            glu::readPixels(m_context.getRenderContext(), viewportX, viewportY, testImg.getAccess());
            GLU_EXPECT_NO_ERROR(gl.getError(), "Draw");
        }

        // Draw reference
        {
            for (int y = 0; y < refImg.getHeight() / 2; y++)
                for (int x = 0; x < refImg.getWidth() / 2; x++)
                    refImg.setPixel(x, y, tcu::RGBA::green());

            for (int y = 0; y < refImg.getHeight() / 2; y++)
                for (int x = refImg.getWidth() / 2; x < refImg.getWidth(); x++)
                    refImg.setPixel(x, y, tcu::RGBA::blue());

            for (int y = refImg.getHeight() / 2; y < refImg.getHeight(); y++)
                for (int x = 0; x < refImg.getWidth() / 2; x++)
                    refImg.setPixel(x, y, tcu::RGBA::blue());

            for (int y = refImg.getHeight() / 2; y < refImg.getHeight(); y++)
                for (int x = refImg.getWidth() / 2; x < refImg.getWidth(); x++)
                    refImg.setPixel(x, y, tcu::RGBA::green());
        }

        // Compare
        {
            bool isOk = tcu::pixelThresholdCompare(log, "Result", "Image comparison result", refImg, testImg, threshold,
                                                   tcu::COMPARE_LOG_RESULT);
            m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                    isOk ? "Pass" : "Image comparison failed");
        }

        return STOP;
    }
};

ShaderBuiltinVarTests::ShaderBuiltinVarTests(Context &context)
    : TestCaseGroup(context, "builtin_variable", "Built-in Variable Tests")
{
}

ShaderBuiltinVarTests::~ShaderBuiltinVarTests(void)
{
}

void ShaderBuiltinVarTests::init(void)
{
    // Builtin constants.

    static const struct
    {
        const char *caseName;
        const char *varName;
        uint32_t paramName;
    } builtinConstants[] = {
        {"max_vertex_attribs", "gl_MaxVertexAttribs", GL_MAX_VERTEX_ATTRIBS},
        {"max_vertex_uniform_vectors", "gl_MaxVertexUniformVectors", GL_MAX_VERTEX_UNIFORM_VECTORS},
        {"max_fragment_uniform_vectors", "gl_MaxFragmentUniformVectors", GL_MAX_FRAGMENT_UNIFORM_VECTORS},
        {"max_varying_vectors", "gl_MaxVaryingVectors", GL_MAX_VARYING_VECTORS},
        {"max_texture_image_units", "gl_MaxTextureImageUnits", GL_MAX_TEXTURE_IMAGE_UNITS},
        {"max_vertex_texture_image_units", "gl_MaxVertexTextureImageUnits", GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS},
        {"max_combined_texture_image_units", "gl_MaxCombinedTextureImageUnits", GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS},
        {"max_draw_buffers", "gl_MaxDrawBuffers", GL_NONE}};

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(builtinConstants); ndx++)
    {
        const char *caseName = builtinConstants[ndx].caseName;
        const char *varName  = builtinConstants[ndx].varName;
        uint32_t paramName   = builtinConstants[ndx].paramName;

        addChild(new ShaderBuiltinConstantCase(m_context, (string(caseName) + "_vertex").c_str(), varName, varName,
                                               paramName, true));
        addChild(new ShaderBuiltinConstantCase(m_context, (string(caseName) + "_fragment").c_str(), varName, varName,
                                               paramName, false));
    }

    addChild(new ShaderDepthRangeTest(m_context, "depth_range_vertex", "gl_DepthRange", true));
    addChild(new ShaderDepthRangeTest(m_context, "depth_range_fragment", "gl_DepthRange", false));

    // Fragment shader builtin variables.

    addChild(new FragCoordXYZCase(m_context));
    addChild(new FragCoordWCase(m_context));
    addChild(new PointCoordCase(m_context));
    addChild(new FrontFacingCase(m_context));
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
