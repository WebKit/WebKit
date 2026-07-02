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
 * \brief Shader built-in variable tests.
 *//*--------------------------------------------------------------------*/

#include "es3fShaderBuiltinVarTests.hpp"
#include "glsShaderRenderCase.hpp"
#include "glsShaderExecUtil.hpp"
#include "deRandom.hpp"
#include "deString.h"
#include "deMath.h"
#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuTestCase.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuImageCompare.hpp"
#include "gluPixelTransfer.hpp"
#include "gluDrawUtil.hpp"
#include "gluStrUtil.hpp"
#include "rrRenderer.hpp"
#include "rrFragmentOperations.hpp"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

using std::string;
using std::vector;
using tcu::TestLog;

namespace deqp
{
namespace gles3
{
namespace Functional
{

static int getInteger(const glw::Functions &gl, uint32_t pname)
{
    int value = -1;
    gl.getIntegerv(pname, &value);
    GLU_EXPECT_NO_ERROR(gl.getError(),
                        ("glGetIntegerv(" + glu::getGettableStateStr((int)pname).toString() + ")").c_str());
    return value;
}

template <uint32_t Pname>
static int getInteger(const glw::Functions &gl)
{
    return getInteger(gl, Pname);
}

static int getVectorsFromComps(const glw::Functions &gl, uint32_t pname)
{
    int value = -1;
    gl.getIntegerv(pname, &value);
    GLU_EXPECT_NO_ERROR(gl.getError(),
                        ("glGetIntegerv(" + glu::getGettableStateStr((int)pname).toString() + ")").c_str());
    // Accept truncated division. According to the spec, the number of vectors is number of components divided by four, plain and simple.
    return value / 4;
}

template <uint32_t Pname>
static int getVectorsFromComps(const glw::Functions &gl)
{
    return getVectorsFromComps(gl, Pname);
}

class ShaderBuiltinConstantCase : public TestCase
{
public:
    typedef int (*GetConstantValueFunc)(const glw::Functions &gl);

    ShaderBuiltinConstantCase(Context &context, const char *name, const char *desc, const char *varName,
                              GetConstantValueFunc getValue, glu::ShaderType shaderType);
    ~ShaderBuiltinConstantCase(void);

    IterateResult iterate(void);

private:
    const std::string m_varName;
    const GetConstantValueFunc m_getValue;
    const glu::ShaderType m_shaderType;
};

ShaderBuiltinConstantCase::ShaderBuiltinConstantCase(Context &context, const char *name, const char *desc,
                                                     const char *varName, GetConstantValueFunc getValue,
                                                     glu::ShaderType shaderType)
    : TestCase(context, name, desc)
    , m_varName(varName)
    , m_getValue(getValue)
    , m_shaderType(shaderType)
{
}

ShaderBuiltinConstantCase::~ShaderBuiltinConstantCase(void)
{
}

static gls::ShaderExecUtil::ShaderExecutor *createGetConstantExecutor(const glu::RenderContext &renderCtx,
                                                                      glu::ShaderType shaderType,
                                                                      const std::string &varName)
{
    using namespace gls::ShaderExecUtil;

    ShaderSpec shaderSpec;

    shaderSpec.version = glu::GLSL_VERSION_300_ES;
    shaderSpec.source  = string("result = ") + varName + ";\n";
    shaderSpec.outputs.push_back(Symbol("result", glu::VarType(glu::TYPE_INT, glu::PRECISION_HIGHP)));

    return createExecutor(renderCtx, shaderType, shaderSpec);
}

ShaderBuiltinConstantCase::IterateResult ShaderBuiltinConstantCase::iterate(void)
{
    using namespace gls::ShaderExecUtil;

    const de::UniquePtr<ShaderExecutor> shaderExecutor(
        createGetConstantExecutor(m_context.getRenderContext(), m_shaderType, m_varName));
    const int reference = m_getValue(m_context.getRenderContext().getFunctions());
    int result          = -1;
    void *const outputs = &result;

    if (!shaderExecutor->isOk())
    {
        shaderExecutor->log(m_testCtx.getLog());
        TCU_FAIL("Compile failed");
    }

    shaderExecutor->useProgram();
    shaderExecutor->execute(1, nullptr, &outputs);

    m_testCtx.getLog() << TestLog::Integer(m_varName, m_varName, "", QP_KEY_TAG_NONE, result);

    if (result != reference)
    {
        m_testCtx.getLog() << TestLog::Message << "ERROR: Expected " << m_varName << " = " << reference
                           << TestLog::EndMessage << TestLog::Message << "Test shader:" << TestLog::EndMessage;
        shaderExecutor->log(m_testCtx.getLog());
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid builtin constant value");
    }
    else
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

    return STOP;
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
        static const char *defaultVertSrc = "#version 300 es\n"
                                            "in highp vec4 a_position;\n"
                                            "void main (void)\n"
                                            "{\n"
                                            "    gl_Position = a_position;\n"
                                            "}\n";
        static const char *defaultFragSrc = "#version 300 es\n"
                                            "in mediump vec4 v_color;\n"
                                            "layout(location = 0) out mediump vec4 o_color;\n\n"
                                            "void main (void)\n"
                                            "{\n"
                                            "    o_color = v_color;\n"
                                            "}\n";

        // Construct shader.
        std::ostringstream src;
        src << "#version 300 es\n";
        if (m_isVertexCase)
            src << "in highp vec4 a_position;\n"
                << "out mediump vec4 v_color;\n";
        else
            src << "layout(location = 0) out mediump vec4 o_color;\n";

        src << "void main (void)\n{\n";
        src << "\t" << (m_isVertexCase ? "v_color" : "o_color")
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

        const glu::ShaderProgram program(m_context.getRenderContext(),
                                         glu::makeVtxFragSources("#version 300 es\n"
                                                                 "in highp vec4 a_position;\n"
                                                                 "void main (void)\n"
                                                                 "{\n"
                                                                 "    gl_Position = a_position;\n"
                                                                 "}\n",

                                                                 "#version 300 es\n"
                                                                 "uniform highp vec3 u_scale;\n"
                                                                 "layout(location = 0) out mediump vec4 o_color;\n"
                                                                 "void main (void)\n"
                                                                 "{\n"
                                                                 "    o_color = vec4(gl_FragCoord.xyz*u_scale, 1.0);\n"
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
            glu::makeVtxFragSources("#version 300 es\n"
                                    "in highp vec4 a_position;\n"
                                    "void main (void)\n"
                                    "{\n"
                                    "    gl_Position = a_position;\n"
                                    "}\n",

                                    "#version 300 es\n"
                                    "layout(location = 0) out mediump vec4 o_color;\n"
                                    "void main (void)\n"
                                    "{\n"
                                    "    o_color = vec4(0.0, 1.0/gl_FragCoord.w - 1.0, 0.0, 1.0);\n"
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
            glu::makeVtxFragSources("#version 300 es\n"
                                    "in highp vec3 a_positionSize;\n"
                                    "void main (void)\n"
                                    "{\n"
                                    "    gl_Position = vec4(a_positionSize.xy, 0.0, 1.0);\n"
                                    "    gl_PointSize = a_positionSize.z;\n"
                                    "}\n",

                                    "#version 300 es\n"
                                    "layout(location = 0) out mediump vec4 o_color;\n"
                                    "void main (void)\n"
                                    "{\n"
                                    "    o_color = vec4(gl_PointCoord, 0.0, 1.0);\n"
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
                                         glu::makeVtxFragSources("#version 300 es\n"
                                                                 "in highp vec4 a_position;\n"
                                                                 "void main (void)\n"
                                                                 "{\n"
                                                                 "    gl_Position = a_position;\n"
                                                                 "}\n",

                                                                 "#version 300 es\n"
                                                                 "layout(location = 0) out mediump vec4 o_color;\n"
                                                                 "void main (void)\n"
                                                                 "{\n"
                                                                 "    if (gl_FrontFacing)\n"
                                                                 "        o_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
                                                                 "    else\n"
                                                                 "        o_color = vec4(0.0, 0.0, 1.0, 1.0);\n"
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

// VertexIDCase

class VertexIDCase : public TestCase
{
public:
    VertexIDCase(Context &context);
    ~VertexIDCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    enum
    {
        MAX_VERTICES = 8 * 3 //!< 8 triangles, totals 24 vertices
    };

    void renderReference(const tcu::PixelBufferAccess &dst, const int numVertices, const uint16_t *const indices,
                         const tcu::Vec4 *const positions, const tcu::Vec4 *const colors, const int subpixelBits);

    glu::ShaderProgram *m_program;
    uint32_t m_positionBuffer;
    uint32_t m_elementBuffer;

    vector<tcu::Vec4> m_positions;
    vector<tcu::Vec4> m_colors;
    int m_viewportW;
    int m_viewportH;

    int m_iterNdx;
};

VertexIDCase::VertexIDCase(Context &context)
    : TestCase(context, "vertex_id", "gl_VertexID Test")
    , m_program(nullptr)
    , m_positionBuffer(0)
    , m_elementBuffer(0)
    , m_viewportW(0)
    , m_viewportH(0)
    , m_iterNdx(0)
{
}

VertexIDCase::~VertexIDCase(void)
{
    VertexIDCase::deinit();
}

void VertexIDCase::init(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    const int width          = m_context.getRenderTarget().getWidth();
    const int height         = m_context.getRenderTarget().getHeight();

    const int quadWidth  = 32;
    const int quadHeight = 32;

    if (width < quadWidth)
        throw tcu::NotSupportedError("Too small render target");

    const int maxQuadsX   = width / quadWidth;
    const int numVertices = MAX_VERTICES;

    const int numQuads  = numVertices / 6 + (numVertices % 6 != 0 ? 1 : 0);
    const int viewportW = de::min(numQuads, maxQuadsX) * quadWidth;
    const int viewportH = (numQuads / maxQuadsX + (numQuads % maxQuadsX != 0 ? 1 : 0)) * quadHeight;

    if (viewportH > height)
        throw tcu::NotSupportedError("Too small render target");

    DE_ASSERT(viewportW <= width && viewportH <= height);

    DE_ASSERT(!m_program);
    m_program = new glu::ShaderProgram(m_context.getRenderContext(),
                                       glu::makeVtxFragSources("#version 300 es\n"
                                                               "in highp vec4 a_position;\n"
                                                               "out mediump vec4 v_color;\n"
                                                               "uniform highp vec4 u_colors[24];\n"
                                                               "void main (void)\n"
                                                               "{\n"
                                                               "    gl_Position = a_position;\n"
                                                               "    v_color = u_colors[gl_VertexID];\n"
                                                               "}\n",

                                                               "#version 300 es\n"
                                                               "in mediump vec4 v_color;\n"
                                                               "layout(location = 0) out mediump vec4 o_color;\n"
                                                               "void main (void)\n"
                                                               "{\n"
                                                               "    o_color = v_color;\n"
                                                               "}\n"));

    m_testCtx.getLog() << *m_program;

    if (!m_program->isOk())
    {
        delete m_program;
        m_program = nullptr;
        throw tcu::TestError("Compile failed");
    }

    gl.genBuffers(1, &m_positionBuffer);
    gl.genBuffers(1, &m_elementBuffer);

    // Set colors (in dynamic memory to save static data space).
    m_colors.resize(numVertices);
    m_colors[0]  = tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    m_colors[1]  = tcu::Vec4(0.5f, 1.0f, 0.5f, 1.0f);
    m_colors[2]  = tcu::Vec4(0.0f, 0.5f, 1.0f, 1.0f);
    m_colors[3]  = tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);
    m_colors[4]  = tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f);
    m_colors[5]  = tcu::Vec4(0.5f, 0.0f, 0.0f, 1.0f);
    m_colors[6]  = tcu::Vec4(0.5f, 0.0f, 1.0f, 1.0f);
    m_colors[7]  = tcu::Vec4(0.5f, 0.0f, 0.5f, 1.0f);
    m_colors[8]  = tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
    m_colors[9]  = tcu::Vec4(0.5f, 1.0f, 0.0f, 1.0f);
    m_colors[10] = tcu::Vec4(0.0f, 0.5f, 0.0f, 1.0f);
    m_colors[11] = tcu::Vec4(0.5f, 1.0f, 1.0f, 1.0f);
    m_colors[12] = tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    m_colors[13] = tcu::Vec4(1.0f, 0.0f, 0.5f, 1.0f);
    m_colors[14] = tcu::Vec4(0.0f, 0.5f, 0.5f, 1.0f);
    m_colors[15] = tcu::Vec4(1.0f, 1.0f, 0.5f, 1.0f);
    m_colors[16] = tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f);
    m_colors[17] = tcu::Vec4(1.0f, 0.5f, 0.0f, 1.0f);
    m_colors[18] = tcu::Vec4(0.0f, 1.0f, 0.5f, 1.0f);
    m_colors[19] = tcu::Vec4(1.0f, 0.5f, 1.0f, 1.0f);
    m_colors[20] = tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f);
    m_colors[21] = tcu::Vec4(1.0f, 0.5f, 0.5f, 1.0f);
    m_colors[22] = tcu::Vec4(0.0f, 0.0f, 0.5f, 1.0f);
    m_colors[23] = tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f);

    // Compute positions.
    m_positions.resize(numVertices);
    DE_ASSERT(numVertices % 3 == 0);
    for (int vtxNdx = 0; vtxNdx < numVertices; vtxNdx += 3)
    {
        const float h = 2.0f * float(quadHeight) / float(viewportH);
        const float w = 2.0f * float(quadWidth) / float(viewportW);

        const int triNdx  = vtxNdx / 3;
        const int quadNdx = triNdx / 2;
        const int quadY   = quadNdx / maxQuadsX;
        const int quadX   = quadNdx % maxQuadsX;

        const float x0 = -1.0f + float(quadX) * w;
        const float y0 = -1.0f + float(quadY) * h;

        if (triNdx % 2 == 0)
        {
            m_positions[vtxNdx + 0] = tcu::Vec4(x0, y0, 0.0f, 1.0f);
            m_positions[vtxNdx + 1] = tcu::Vec4(x0 + w, y0 + h, 0.0f, 1.0f);
            m_positions[vtxNdx + 2] = tcu::Vec4(x0, y0 + h, 0.0f, 1.0f);
        }
        else
        {
            m_positions[vtxNdx + 0] = tcu::Vec4(x0 + w, y0 + h, 0.0f, 1.0f);
            m_positions[vtxNdx + 1] = tcu::Vec4(x0, y0, 0.0f, 1.0f);
            m_positions[vtxNdx + 2] = tcu::Vec4(x0 + w, y0, 0.0f, 1.0f);
        }
    }

    m_viewportW = viewportW;
    m_viewportH = viewportH;
    m_iterNdx   = 0;

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
}

void VertexIDCase::deinit(void)
{
    delete m_program;
    m_program = nullptr;

    if (m_positionBuffer)
    {
        m_context.getRenderContext().getFunctions().deleteBuffers(1, &m_positionBuffer);
        m_positionBuffer = 0;
    }

    if (m_elementBuffer)
    {
        m_context.getRenderContext().getFunctions().deleteBuffers(1, &m_elementBuffer);
        m_elementBuffer = 0;
    }

    m_positions.clear();
    m_colors.clear();
}

class VertexIDReferenceShader : public rr::VertexShader, public rr::FragmentShader
{
public:
    enum
    {
        VARYINGLOC_COLOR = 0
    };

    VertexIDReferenceShader()
        : rr::VertexShader(2, 1)   // color and pos in => color out
        , rr::FragmentShader(1, 1) // color in => color out
    {
        this->rr::VertexShader::m_inputs[0].type = rr::GENERICVECTYPE_FLOAT;
        this->rr::VertexShader::m_inputs[1].type = rr::GENERICVECTYPE_FLOAT;

        this->rr::VertexShader::m_outputs[0].type      = rr::GENERICVECTYPE_FLOAT;
        this->rr::VertexShader::m_outputs[0].flatshade = false;

        this->rr::FragmentShader::m_inputs[0].type      = rr::GENERICVECTYPE_FLOAT;
        this->rr::FragmentShader::m_inputs[0].flatshade = false;

        this->rr::FragmentShader::m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
    }

    void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets, const int numPackets) const
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

    void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
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
};

void VertexIDCase::renderReference(const tcu::PixelBufferAccess &dst, const int numVertices,
                                   const uint16_t *const indices, const tcu::Vec4 *const positions,
                                   const tcu::Vec4 *const colors, const int subpixelBits)
{
    const rr::Renderer referenceRenderer;
    const rr::RenderState referenceState(
        (rr::ViewportState)(rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(dst)), subpixelBits);
    const rr::RenderTarget referenceTarget(rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(dst));
    const VertexIDReferenceShader referenceShader;
    rr::VertexAttrib attribs[2];

    attribs[0].type            = rr::VERTEXATTRIBTYPE_FLOAT;
    attribs[0].size            = 4;
    attribs[0].stride          = 0;
    attribs[0].instanceDivisor = 0;
    attribs[0].pointer         = positions;

    attribs[1].type            = rr::VERTEXATTRIBTYPE_FLOAT;
    attribs[1].size            = 4;
    attribs[1].stride          = 0;
    attribs[1].instanceDivisor = 0;
    attribs[1].pointer         = colors;

    referenceRenderer.draw(
        rr::DrawCommand(referenceState, referenceTarget, rr::Program(&referenceShader, &referenceShader), 2, attribs,
                        rr::PrimitiveList(rr::PRIMITIVETYPE_TRIANGLES, numVertices, rr::DrawIndices(indices))));
}

VertexIDCase::IterateResult VertexIDCase::iterate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    const int width          = m_context.getRenderTarget().getWidth();
    const int height         = m_context.getRenderTarget().getHeight();
    const int viewportW      = m_viewportW;
    const int viewportH      = m_viewportH;

    const float threshold = 0.02f;

    de::Random rnd(0xcf23ab1 ^ deInt32Hash(m_iterNdx));
    tcu::Surface refImg(viewportW, viewportH);
    tcu::Surface testImg(viewportW, viewportH);

    const int viewportX = rnd.getInt(0, width - viewportW);
    const int viewportY = rnd.getInt(0, height - viewportH);

    const int posLoc    = gl.getAttribLocation(m_program->getProgram(), "a_position");
    const int colorsLoc = gl.getUniformLocation(m_program->getProgram(), "u_colors[0]");
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Setup common state.
    gl.viewport(viewportX, viewportY, viewportW, viewportH);
    gl.useProgram(m_program->getProgram());
    gl.bindBuffer(GL_ARRAY_BUFFER, m_positionBuffer);
    gl.enableVertexAttribArray(posLoc);
    gl.vertexAttribPointer(posLoc, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    gl.uniform4fv(colorsLoc, (int)m_colors.size(), (const float *)&m_colors[0]);

    // Clear render target to black.
    gl.clearColor(clearColor.x(), clearColor.y(), clearColor.z(), clearColor.w());
    gl.clear(GL_COLOR_BUFFER_BIT);

    tcu::clear(refImg.getAccess(), clearColor);

    int subpixelBits = 0;
    gl.getIntegerv(GL_SUBPIXEL_BITS, &subpixelBits);

    if (m_iterNdx == 0)
    {
        tcu::ScopedLogSection logSection(m_testCtx.getLog(), "Iter0", "glDrawArrays()");
        vector<uint16_t> indices(m_positions.size());

        gl.bufferData(GL_ARRAY_BUFFER, (int)(m_positions.size() * sizeof(tcu::Vec4)), &m_positions[0], GL_DYNAMIC_DRAW);
        gl.drawArrays(GL_TRIANGLES, 0, (int)m_positions.size());

        glu::readPixels(m_context.getRenderContext(), viewportX, viewportY, testImg.getAccess());
        GLU_EXPECT_NO_ERROR(gl.getError(), "Draw");

        // Reference indices
        for (int ndx = 0; ndx < (int)indices.size(); ndx++)
            indices[ndx] = (uint16_t)ndx;

        renderReference(refImg.getAccess(), (int)m_positions.size(), &indices[0], &m_positions[0], &m_colors[0],
                        subpixelBits);
    }
    else if (m_iterNdx == 1)
    {
        tcu::ScopedLogSection logSection(m_testCtx.getLog(), "Iter1", "glDrawElements(), indices in client-side array");
        vector<uint16_t> indices(m_positions.size());
        vector<tcu::Vec4> mappedPos(m_positions.size());

        // Compute initial indices and suffle
        for (int ndx = 0; ndx < (int)indices.size(); ndx++)
            indices[ndx] = (uint16_t)ndx;
        rnd.shuffle(indices.begin(), indices.end());

        // Use indices to re-map positions.
        for (int ndx = 0; ndx < (int)indices.size(); ndx++)
            mappedPos[indices[ndx]] = m_positions[ndx];

        gl.bufferData(GL_ARRAY_BUFFER, (int)(m_positions.size() * sizeof(tcu::Vec4)), &mappedPos[0], GL_DYNAMIC_DRAW);
        gl.drawElements(GL_TRIANGLES, (int)indices.size(), GL_UNSIGNED_SHORT, &indices[0]);

        glu::readPixels(m_context.getRenderContext(), viewportX, viewportY, testImg.getAccess());
        GLU_EXPECT_NO_ERROR(gl.getError(), "Draw");

        renderReference(refImg.getAccess(), (int)indices.size(), &indices[0], &mappedPos[0], &m_colors[0],
                        subpixelBits);
    }
    else if (m_iterNdx == 2)
    {
        tcu::ScopedLogSection logSection(m_testCtx.getLog(), "Iter2", "glDrawElements(), indices in buffer");
        vector<uint16_t> indices(m_positions.size());
        vector<tcu::Vec4> mappedPos(m_positions.size());

        // Compute initial indices and suffle
        for (int ndx = 0; ndx < (int)indices.size(); ndx++)
            indices[ndx] = (uint16_t)ndx;
        rnd.shuffle(indices.begin(), indices.end());

        // Use indices to re-map positions.
        for (int ndx = 0; ndx < (int)indices.size(); ndx++)
            mappedPos[indices[ndx]] = m_positions[ndx];

        gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_elementBuffer);
        gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, (int)(indices.size() * sizeof(uint16_t)), &indices[0], GL_DYNAMIC_DRAW);

        gl.bufferData(GL_ARRAY_BUFFER, (int)(m_positions.size() * sizeof(tcu::Vec4)), &mappedPos[0], GL_DYNAMIC_DRAW);
        gl.drawElements(GL_TRIANGLES, (int)indices.size(), GL_UNSIGNED_SHORT, nullptr);

        glu::readPixels(m_context.getRenderContext(), viewportX, viewportY, testImg.getAccess());
        GLU_EXPECT_NO_ERROR(gl.getError(), "Draw");

        tcu::clear(refImg.getAccess(), clearColor);
        renderReference(refImg.getAccess(), (int)indices.size(), &indices[0], &mappedPos[0], &m_colors[0],
                        subpixelBits);
    }
    else
        DE_ASSERT(false);

    if (!tcu::fuzzyCompare(m_testCtx.getLog(), "Result", "Image comparison result", refImg, testImg, threshold,
                           tcu::COMPARE_LOG_RESULT))
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");

    m_iterNdx += 1;
    return (m_iterNdx < 3) ? CONTINUE : STOP;
}

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
        ShaderBuiltinConstantCase::GetConstantValueFunc getValue;
    } builtinConstants[] = {
        // GLES 2.

        {"max_vertex_attribs", "gl_MaxVertexAttribs", getInteger<GL_MAX_VERTEX_ATTRIBS>},
        {"max_vertex_uniform_vectors", "gl_MaxVertexUniformVectors", getInteger<GL_MAX_VERTEX_UNIFORM_VECTORS>},
        {"max_fragment_uniform_vectors", "gl_MaxFragmentUniformVectors", getInteger<GL_MAX_FRAGMENT_UNIFORM_VECTORS>},
        {"max_texture_image_units", "gl_MaxTextureImageUnits", getInteger<GL_MAX_TEXTURE_IMAGE_UNITS>},
        {"max_vertex_texture_image_units", "gl_MaxVertexTextureImageUnits",
         getInteger<GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS>},
        {"max_combined_texture_image_units", "gl_MaxCombinedTextureImageUnits",
         getInteger<GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS>},
        {"max_draw_buffers", "gl_MaxDrawBuffers", getInteger<GL_MAX_DRAW_BUFFERS>},

        // GLES 3.

        {"max_vertex_output_vectors", "gl_MaxVertexOutputVectors",
         getVectorsFromComps<GL_MAX_VERTEX_OUTPUT_COMPONENTS>},
        {"max_fragment_input_vectors", "gl_MaxFragmentInputVectors",
         getVectorsFromComps<GL_MAX_FRAGMENT_INPUT_COMPONENTS>},
        {"min_program_texel_offset", "gl_MinProgramTexelOffset", getInteger<GL_MIN_PROGRAM_TEXEL_OFFSET>},
        {"max_program_texel_offset", "gl_MaxProgramTexelOffset", getInteger<GL_MAX_PROGRAM_TEXEL_OFFSET>}};

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(builtinConstants); ndx++)
    {
        const char *const caseName                                     = builtinConstants[ndx].caseName;
        const char *const varName                                      = builtinConstants[ndx].varName;
        const ShaderBuiltinConstantCase::GetConstantValueFunc getValue = builtinConstants[ndx].getValue;

        addChild(new ShaderBuiltinConstantCase(m_context, (string(caseName) + "_vertex").c_str(), varName, varName,
                                               getValue, glu::SHADERTYPE_VERTEX));
        addChild(new ShaderBuiltinConstantCase(m_context, (string(caseName) + "_fragment").c_str(), varName, varName,
                                               getValue, glu::SHADERTYPE_FRAGMENT));
    }

    addChild(new ShaderDepthRangeTest(m_context, "depth_range_vertex", "gl_DepthRange", true));
    addChild(new ShaderDepthRangeTest(m_context, "depth_range_fragment", "gl_DepthRange", false));

    // Vertex shader builtin variables.
    addChild(new VertexIDCase(m_context));
    // \todo [2013-03-20 pyry] gl_InstanceID -- tested in instancing tests quite thoroughly.

    // Fragment shader builtin variables.

    addChild(new FragCoordXYZCase(m_context));
    addChild(new FragCoordWCase(m_context));
    addChild(new PointCoordCase(m_context));
    addChild(new FrontFacingCase(m_context));
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
