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
 * \brief Invariance tests.
 *//*--------------------------------------------------------------------*/

#include "es2fShaderInvarianceTests.hpp"
#include "deStringUtil.hpp"
#include "deRandom.hpp"
#include "gluContextInfo.hpp"
#include "gluRenderContext.hpp"
#include "gluShaderProgram.hpp"
#include "gluPixelTransfer.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuTestLog.hpp"
#include "tcuSurface.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuStringTemplate.hpp"

namespace deqp
{
namespace gles2
{
namespace Functional
{
namespace
{

class FormatArgumentList;

static tcu::Vec4 genRandomVector(de::Random &rnd)
{
    tcu::Vec4 retVal;

    retVal.x() = rnd.getFloat(-1.0f, 1.0f);
    retVal.y() = rnd.getFloat(-1.0f, 1.0f);
    retVal.z() = rnd.getFloat(-1.0f, 1.0f);
    retVal.w() = rnd.getFloat(0.2f, 1.0f);

    return retVal;
}

class FormatArgument
{
public:
    FormatArgument(const char *name, const std::string &value);

private:
    friend class FormatArgumentList;

    const char *const m_name;
    const std::string m_value;
};

FormatArgument::FormatArgument(const char *name, const std::string &value) : m_name(name), m_value(value)
{
}

class FormatArgumentList
{
public:
    FormatArgumentList(void);

    FormatArgumentList &operator<<(const FormatArgument &);
    const std::map<std::string, std::string> &getArguments(void) const;

private:
    std::map<std::string, std::string> m_formatArguments;
};

FormatArgumentList::FormatArgumentList(void)
{
}

FormatArgumentList &FormatArgumentList::operator<<(const FormatArgument &arg)
{
    m_formatArguments[arg.m_name] = arg.m_value;
    return *this;
}

const std::map<std::string, std::string> &FormatArgumentList::getArguments(void) const
{
    return m_formatArguments;
}

static std::string formatGLSL(const char *templateString, const FormatArgumentList &args)
{
    const std::map<std::string, std::string> &params = args.getArguments();

    return tcu::StringTemplate(std::string(templateString)).specialize(params);
}

/*--------------------------------------------------------------------*//*!
 * \brief Vertex shader invariance test
 *
 * Test vertex shader invariance by drawing a test pattern two times, each
 * time with a different shader. Shaders have set identical values to
 * invariant gl_Position using identical expressions. No fragments from the
 * first pass using should remain visible.
 *//*--------------------------------------------------------------------*/
class InvarianceTest : public TestCase
{
public:
    struct ShaderPair
    {
        std::string vertexShaderSource0;
        std::string fragmentShaderSource0;
        std::string vertexShaderSource1;
        std::string fragmentShaderSource1;
    };

    InvarianceTest(Context &ctx, const char *name, const char *desc);
    ~InvarianceTest(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    virtual ShaderPair genShaders(void) const = 0;
    bool checkImage(const tcu::Surface &) const;

    glu::ShaderProgram *m_shader0;
    glu::ShaderProgram *m_shader1;
    glw::GLuint m_arrayBuf;
    int m_verticesInPattern;

    const int m_renderSize;
};

InvarianceTest::InvarianceTest(Context &ctx, const char *name, const char *desc)
    : TestCase(ctx, name, desc)
    , m_shader0(nullptr)
    , m_shader1(nullptr)
    , m_arrayBuf(0)
    , m_verticesInPattern(0)
    , m_renderSize(256)
{
}

InvarianceTest::~InvarianceTest(void)
{
    deinit();
}

void InvarianceTest::init(void)
{
    // Invariance tests require drawing to the screen and reading back results.
    // Tests results are not reliable if the resolution is too small
    {
        if (m_context.getRenderTarget().getWidth() < m_renderSize ||
            m_context.getRenderTarget().getHeight() < m_renderSize)
            throw tcu::NotSupportedError(std::string("Render target size must be at least ") +
                                         de::toString(m_renderSize) + "x" + de::toString(m_renderSize));
    }

    // Gen shaders
    {
        ShaderPair vertexShaders = genShaders();

        m_shader0 =
            new glu::ShaderProgram(m_context.getRenderContext(),
                                   glu::ProgramSources() << glu::VertexSource(vertexShaders.vertexShaderSource0)
                                                         << glu::FragmentSource(vertexShaders.fragmentShaderSource0));
        if (!m_shader0->isOk())
        {
            m_testCtx.getLog() << *m_shader0;
            throw tcu::TestError("Test shader compile failed.");
        }

        m_shader1 =
            new glu::ShaderProgram(m_context.getRenderContext(),
                                   glu::ProgramSources() << glu::VertexSource(vertexShaders.vertexShaderSource1)
                                                         << glu::FragmentSource(vertexShaders.fragmentShaderSource1));
        if (!m_shader1->isOk())
        {
            m_testCtx.getLog() << *m_shader1;
            throw tcu::TestError("Test shader compile failed.");
        }

        // log
        m_testCtx.getLog() << tcu::TestLog::Message << "Shader 1:" << tcu::TestLog::EndMessage << *m_shader0
                           << tcu::TestLog::Message << "Shader 2:" << tcu::TestLog::EndMessage << *m_shader1;
    }

    // Gen test pattern
    {
        const int numTriangles = 72;
        de::Random rnd(123);
        std::vector<tcu::Vec4> triangles(numTriangles * 3 * 2);
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();

        // Narrow triangle pattern
        for (int triNdx = 0; triNdx < numTriangles; ++triNdx)
        {
            const tcu::Vec4 vertex1 = genRandomVector(rnd);
            const tcu::Vec4 vertex2 = genRandomVector(rnd);
            const tcu::Vec4 vertex3 = vertex2 + genRandomVector(rnd) * 0.01f; // generate narrow triangles

            triangles[triNdx * 3 + 0] = vertex1;
            triangles[triNdx * 3 + 1] = vertex2;
            triangles[triNdx * 3 + 2] = vertex3;
        }

        // Normal triangle pattern
        for (int triNdx = 0; triNdx < numTriangles; ++triNdx)
        {
            triangles[(numTriangles + triNdx) * 3 + 0] = genRandomVector(rnd);
            triangles[(numTriangles + triNdx) * 3 + 1] = genRandomVector(rnd);
            triangles[(numTriangles + triNdx) * 3 + 2] = genRandomVector(rnd);
        }

        // upload
        gl.genBuffers(1, &m_arrayBuf);
        gl.bindBuffer(GL_ARRAY_BUFFER, m_arrayBuf);
        gl.bufferData(GL_ARRAY_BUFFER, (int)(triangles.size() * sizeof(tcu::Vec4)), &triangles[0], GL_STATIC_DRAW);
        GLU_EXPECT_NO_ERROR(gl.getError(), "buffer gen");

        m_verticesInPattern = numTriangles * 3;
    }
}

void InvarianceTest::deinit(void)
{
    delete m_shader0;
    delete m_shader1;

    m_shader0 = nullptr;
    m_shader1 = nullptr;

    if (m_arrayBuf)
    {
        m_context.getRenderContext().getFunctions().deleteBuffers(1, &m_arrayBuf);
        m_arrayBuf = 0;
    }
}

InvarianceTest::IterateResult InvarianceTest::iterate(void)
{
    const glw::Functions &gl     = m_context.getRenderContext().getFunctions();
    const bool depthBufferExists = m_context.getRenderTarget().getDepthBits() != 0;
    tcu::Surface resultSurface(m_renderSize, m_renderSize);
    bool error = false;

    // Prepare draw
    gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl.clear(GL_COLOR_BUFFER_BIT);
    gl.viewport(0, 0, m_renderSize, m_renderSize);
    gl.bindBuffer(GL_ARRAY_BUFFER, m_arrayBuf);
    GLU_EXPECT_NO_ERROR(gl.getError(), "setup draw");

    m_testCtx.getLog() << tcu::TestLog::Message << "Testing position invariance." << tcu::TestLog::EndMessage;

    // Draw position check passes
    for (int passNdx = 0; passNdx < 2; ++passNdx)
    {
        const glu::ShaderProgram &shader = (passNdx == 0) ? (*m_shader0) : (*m_shader1);
        const glw::GLint positionLoc     = gl.getAttribLocation(shader.getProgram(), "a_input");
        const glw::GLint colorLoc        = gl.getUniformLocation(shader.getProgram(), "u_color");
        const tcu::Vec4 red              = tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
        const tcu::Vec4 green            = tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);
        const tcu::Vec4 color            = (passNdx == 0) ? (red) : (green);
        const char *const colorStr       = (passNdx == 0) ? ("red - purple") : ("green");

        m_testCtx.getLog() << tcu::TestLog::Message << "Drawing position test pattern using shader " << (passNdx + 1)
                           << ". Primitive color: " << colorStr << "." << tcu::TestLog::EndMessage;

        gl.useProgram(shader.getProgram());
        gl.uniform4fv(colorLoc, 1, color.getPtr());
        gl.enableVertexAttribArray(positionLoc);
        gl.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, sizeof(tcu::Vec4), nullptr);
        gl.drawArrays(GL_TRIANGLES, 0, m_verticesInPattern);
        gl.disableVertexAttribArray(positionLoc);
        GLU_EXPECT_NO_ERROR(gl.getError(), "draw pass");
    }

    // Read result
    glu::readPixels(m_context.getRenderContext(), 0, 0, resultSurface.getAccess());

    // Check there are no red pixels
    m_testCtx.getLog() << tcu::TestLog::Message
                       << "Verifying output. Expecting only green or background colored pixels."
                       << tcu::TestLog::EndMessage;
    error |= !checkImage(resultSurface);

    if (!depthBufferExists)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Depth buffer not available, skipping z-test."
                           << tcu::TestLog::EndMessage;
    }
    else
    {
        // Test with Z-test
        gl.clearDepthf(1.0f);
        gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        gl.enable(GL_DEPTH_TEST);

        m_testCtx.getLog() << tcu::TestLog::Message
                           << "Testing position invariance with z-test. Enabling GL_DEPTH_TEST."
                           << tcu::TestLog::EndMessage;

        // Draw position check passes
        for (int passNdx = 0; passNdx < 2; ++passNdx)
        {
            const glu::ShaderProgram &shader = (passNdx == 0) ? (*m_shader0) : (*m_shader1);
            const glw::GLint positionLoc     = gl.getAttribLocation(shader.getProgram(), "a_input");
            const glw::GLint colorLoc        = gl.getUniformLocation(shader.getProgram(), "u_color");
            const tcu::Vec4 red              = tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
            const tcu::Vec4 green            = tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);
            const tcu::Vec4 color            = (passNdx == 0) ? (red) : (green);
            const glw::GLenum depthFunc      = (passNdx == 0) ? (GL_ALWAYS) : (GL_EQUAL);
            const char *const depthFuncStr   = (passNdx == 0) ? ("GL_ALWAYS") : ("GL_EQUAL");
            const char *const colorStr       = (passNdx == 0) ? ("red - purple") : ("green");

            m_testCtx.getLog() << tcu::TestLog::Message << "Drawing Z-test pattern using shader " << (passNdx + 1)
                               << ". Primitive color: " << colorStr << ". DepthFunc: " << depthFuncStr
                               << tcu::TestLog::EndMessage;

            gl.useProgram(shader.getProgram());
            gl.uniform4fv(colorLoc, 1, color.getPtr());
            gl.depthFunc(depthFunc);
            gl.enableVertexAttribArray(positionLoc);
            gl.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, sizeof(tcu::Vec4), nullptr);
            gl.drawArrays(GL_TRIANGLES, m_verticesInPattern,
                          m_verticesInPattern); // !< buffer contains 2 m_verticesInPattern-sized patterns
            gl.disableVertexAttribArray(positionLoc);
            GLU_EXPECT_NO_ERROR(gl.getError(), "draw pass");
        }

        // Read result
        glu::readPixels(m_context.getRenderContext(), 0, 0, resultSurface.getAccess());

        // Check there are no red pixels
        m_testCtx.getLog() << tcu::TestLog::Message
                           << "Verifying output. Expecting only green or background colored pixels."
                           << tcu::TestLog::EndMessage;
        error |= !checkImage(resultSurface);
    }

    // Report result
    if (error)
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Detected variance between two invariant values");
    else
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

    return STOP;
}

bool InvarianceTest::checkImage(const tcu::Surface &surface) const
{
    const tcu::IVec4 okColor = tcu::IVec4(0, 255, 0, 255);
    const tcu::RGBA errColor = tcu::RGBA(255, 0, 0, 255);
    bool error               = false;
    tcu::Surface errorMask(m_renderSize, m_renderSize);

    tcu::clear(errorMask.getAccess(), okColor);

    for (int y = 0; y < m_renderSize; ++y)
        for (int x = 0; x < m_renderSize; ++x)
        {
            const tcu::RGBA col = surface.getPixel(x, y);

            if (col.getRed() != 0)
            {
                errorMask.setPixel(x, y, errColor);
                error = true;
            }
        }

    // report error
    if (error)
    {
        m_testCtx.getLog() << tcu::TestLog::Message
                           << "Invalid pixels found (fragments from first render pass found). Variance detected."
                           << tcu::TestLog::EndMessage;
        m_testCtx.getLog() << tcu::TestLog::ImageSet("Results", "Result verification")
                           << tcu::TestLog::Image("Result", "Result", surface)
                           << tcu::TestLog::Image("Error mask", "Error mask", errorMask) << tcu::TestLog::EndImageSet;

        return false;
    }
    else
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "No variance found." << tcu::TestLog::EndMessage;
        m_testCtx.getLog() << tcu::TestLog::ImageSet("Results", "Result verification")
                           << tcu::TestLog::Image("Result", "Result", surface) << tcu::TestLog::EndImageSet;

        return true;
    }
}

class BasicInvarianceTest : public InvarianceTest
{
public:
    BasicInvarianceTest(Context &ctx, const char *name, const char *desc, const std::string &vertexShader1,
                        const std::string &vertexShader2);
    ShaderPair genShaders(void) const;

private:
    const std::string m_vertexShader1;
    const std::string m_vertexShader2;
    const std::string m_fragmentShader;
    static const char *const s_basicFragmentShader;
};

const char *const BasicInvarianceTest::s_basicFragmentShader =
    "uniform mediump vec4 u_color;\n"
    "varying mediump vec4 v_unrelated;\n"
    "void main ()\n"
    "{\n"
    "    mediump float blue = dot(v_unrelated, vec4(1.0, 1.0, 1.0, 1.0));\n"
    "    gl_FragColor = vec4(u_color.r, u_color.g, blue, u_color.a);\n"
    "}\n";

BasicInvarianceTest::BasicInvarianceTest(Context &ctx, const char *name, const char *desc,
                                         const std::string &vertexShader1, const std::string &vertexShader2)
    : InvarianceTest(ctx, name, desc)
    , m_vertexShader1(vertexShader1)
    , m_vertexShader2(vertexShader2)
    , m_fragmentShader(s_basicFragmentShader)
{
}

BasicInvarianceTest::ShaderPair BasicInvarianceTest::genShaders(void) const
{
    ShaderPair retVal;

    retVal.vertexShaderSource0   = m_vertexShader1;
    retVal.vertexShaderSource1   = m_vertexShader2;
    retVal.fragmentShaderSource0 = m_fragmentShader;
    retVal.fragmentShaderSource1 = m_fragmentShader;

    return retVal;
}

} // namespace

ShaderInvarianceTests::ShaderInvarianceTests(Context &context)
    : TestCaseGroup(context, "invariance", "Invariance tests")
{
}

ShaderInvarianceTests::~ShaderInvarianceTests(void)
{
}

void ShaderInvarianceTests::init(void)
{
    static const struct PrecisionCase
    {
        glu::Precision prec;
        const char *name;

        // set literals in the glsl to be in the representable range
        const char *highValue; // !< highValue < maxValue
        const char *invHighValue;
        const char *mediumValue; // !< mediumValue^2 < maxValue
        const char *lowValue;    // !< lowValue^4 < maxValue
        const char *invlowValue;
        int loopIterations;
        int loopPartialIterations;
        int loopNormalizationExponent;
        const char *loopNormalizationConstantLiteral;
        const char *loopMultiplier;
        const char *sumLoopNormalizationConstantLiteral;
    } precisions[] = {
        {glu::PRECISION_HIGHP, "highp", "1.0e20", "1.0e-20", "1.0e14", "1.0e9", "1.0e-9", 14, 11, 2, "1.0e4", "1.9",
         "1.0e3"},
        {glu::PRECISION_MEDIUMP, "mediump", "1.0e4", "1.0e-4", "1.0e2", "1.0e1", "1.0e-1", 13, 11, 2, "1.0e4", "1.9",
         "1.0e3"},
        {glu::PRECISION_LOWP, "lowp", "0.9", "1.1", "1.1", "1.15", "0.87", 6, 2, 0, "2.0", "1.1", "1.0"},
    };

    for (int precNdx = 0; precNdx < DE_LENGTH_OF_ARRAY(precisions); ++precNdx)
    {
        const char *const precisionName = precisions[precNdx].name;
        const glu::Precision precision  = precisions[precNdx].prec;
        tcu::TestCaseGroup *const group =
            new tcu::TestCaseGroup(m_testCtx, precisionName, "Invariance tests using the given precision.");

        const FormatArgumentList args =
            FormatArgumentList() << FormatArgument("VERSION", "") << FormatArgument("IN", "attribute")
                                 << FormatArgument("OUT", "varying") << FormatArgument("IN_PREC", precisionName)
                                 << FormatArgument("HIGH_VALUE", de::toString(precisions[precNdx].highValue))
                                 << FormatArgument("HIGH_VALUE_INV", de::toString(precisions[precNdx].invHighValue))
                                 << FormatArgument("MEDIUM_VALUE", de::toString(precisions[precNdx].mediumValue))
                                 << FormatArgument("LOW_VALUE", de::toString(precisions[precNdx].lowValue))
                                 << FormatArgument("LOW_VALUE_INV", de::toString(precisions[precNdx].invlowValue))
                                 << FormatArgument("LOOP_ITERS", de::toString(precisions[precNdx].loopIterations))
                                 << FormatArgument("LOOP_ITERS_PARTIAL",
                                                   de::toString(precisions[precNdx].loopPartialIterations))
                                 << FormatArgument("LOOP_NORM_FRACT_EXP",
                                                   de::toString(precisions[precNdx].loopNormalizationExponent))
                                 << FormatArgument("LOOP_NORM_LITERAL",
                                                   precisions[precNdx].loopNormalizationConstantLiteral)
                                 << FormatArgument("LOOP_MULTIPLIER", precisions[precNdx].loopMultiplier)
                                 << FormatArgument("SUM_LOOP_NORM_LITERAL",
                                                   precisions[precNdx].sumLoopNormalizationConstantLiteral);

        addChild(group);

        // subexpression cases
        {
            // First shader shares "${HIGH_VALUE}*a_input.x*a_input.xxxx + ${HIGH_VALUE}*a_input.y*a_input.yyyy" with unrelated output variable. Reordering might result in accuracy loss
            // due to the high exponent. In the second shader, the high exponent may be removed during compilation.

            group->addChild(new BasicInvarianceTest(
                m_context, "common_subexpression_0", "Shader shares a subexpression with an unrelated variable.",
                formatGLSL(
                    "${VERSION}"
                    "${IN} ${IN_PREC} vec4 a_input;\n"
                    "${OUT} mediump vec4 v_unrelated;\n"
                    "invariant gl_Position;\n"
                    "void main ()\n"
                    "{\n"
                    "    v_unrelated = a_input.xzxz + (${HIGH_VALUE}*a_input.x*a_input.xxxx + "
                    "${HIGH_VALUE}*a_input.y*a_input.yyyy) * (1.08 * a_input.zyzy * a_input.xzxz) * ${HIGH_VALUE_INV} "
                    "* (a_input.z * a_input.zzxz - a_input.z * a_input.zzxz) + (${HIGH_VALUE}*a_input.x*a_input.xxxx + "
                    "${HIGH_VALUE}*a_input.y*a_input.yyyy) / ${HIGH_VALUE};\n"
                    "    gl_Position = a_input + (${HIGH_VALUE}*a_input.x*a_input.xxxx + "
                    "${HIGH_VALUE}*a_input.y*a_input.yyyy) * ${HIGH_VALUE_INV};\n"
                    "}\n",
                    args),
                formatGLSL("${VERSION}"
                           "${IN} ${IN_PREC} vec4 a_input;\n"
                           "${OUT} mediump vec4 v_unrelated;\n"
                           "invariant gl_Position;\n"
                           "void main ()\n"
                           "{\n"
                           "    v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
                           "    gl_Position = a_input + (${HIGH_VALUE}*a_input.x*a_input.xxxx + "
                           "${HIGH_VALUE}*a_input.y*a_input.yyyy) * ${HIGH_VALUE_INV};\n"
                           "}\n",
                           args)));

            // In the first shader, the unrelated variable "d" has mathematically the same expression as "e", but the different
            // order of calculation might cause different results.

            group->addChild(new BasicInvarianceTest(
                m_context, "common_subexpression_1", "Shader shares a subexpression with an unrelated variable.",
                formatGLSL("${VERSION}"
                           "${IN} ${IN_PREC} vec4 a_input;\n"
                           "${OUT} mediump vec4 v_unrelated;\n"
                           "invariant gl_Position;\n"
                           "void main ()\n"
                           "{\n"
                           "    ${IN_PREC} vec4 a = ${HIGH_VALUE} * a_input.zzxx + a_input.xzxy - ${HIGH_VALUE} * "
                           "a_input.zzxx;\n"
                           "    ${IN_PREC} vec4 b = ${HIGH_VALUE} * a_input.zzxx;\n"
                           "    ${IN_PREC} vec4 c = b - ${HIGH_VALUE} * a_input.zzxx + a_input.xzxy;\n"
                           "    ${IN_PREC} vec4 d = (${LOW_VALUE} * a_input.yzxx) * (${LOW_VALUE} * a_input.yzzw) * "
                           "(1.1*${LOW_VALUE_INV} * a_input.yzxx) * (${LOW_VALUE_INV} * a_input.xzzy);\n"
                           "    ${IN_PREC} vec4 e = ((${LOW_VALUE} * a_input.yzxx) * (1.1*${LOW_VALUE_INV} * "
                           "a_input.yzxx)) * ((${LOW_VALUE_INV} * a_input.xzzy) * (${LOW_VALUE} * a_input.yzzw));\n"
                           "    v_unrelated = a + b + c + d + e;\n"
                           "    gl_Position = a_input + fract(c) + e;\n"
                           "}\n",
                           args),
                formatGLSL("${VERSION}"
                           "${IN} ${IN_PREC} vec4 a_input;\n"
                           "${OUT} mediump vec4 v_unrelated;\n"
                           "invariant gl_Position;\n"
                           "void main ()\n"
                           "{\n"
                           "    ${IN_PREC} vec4 b = ${HIGH_VALUE} * a_input.zzxx;\n"
                           "    ${IN_PREC} vec4 c = b - ${HIGH_VALUE} * a_input.zzxx + a_input.xzxy;\n"
                           "    ${IN_PREC} vec4 e = ((${LOW_VALUE} * a_input.yzxx) * (1.1*${LOW_VALUE_INV} * "
                           "a_input.yzxx)) * ((${LOW_VALUE_INV} * a_input.xzzy) * (${LOW_VALUE} * a_input.yzzw));\n"
                           "    v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
                           "    gl_Position = a_input + fract(c) + e;\n"
                           "}\n",
                           args)));

            // Intermediate values used by an unrelated output variable

            group->addChild(new BasicInvarianceTest(
                m_context, "common_subexpression_2", "Shader shares a subexpression with an unrelated variable.",
                formatGLSL("${VERSION}"
                           "${IN} ${IN_PREC} vec4 a_input;\n"
                           "${OUT} mediump vec4 v_unrelated;\n"
                           "invariant gl_Position;\n"
                           "void main ()\n"
                           "{\n"
                           "    ${IN_PREC} vec4 a = ${MEDIUM_VALUE} * (a_input.xxxx + a_input.yyyy);\n"
                           "    ${IN_PREC} vec4 b = (${MEDIUM_VALUE} * (a_input.xxxx + a_input.yyyy)) * "
                           "(${MEDIUM_VALUE} * (a_input.xxxx + a_input.yyyy)) / ${MEDIUM_VALUE} / ${MEDIUM_VALUE};\n"
                           "    ${IN_PREC} vec4 c = a * a;\n"
                           "    ${IN_PREC} vec4 d = c / ${MEDIUM_VALUE} / ${MEDIUM_VALUE};\n"
                           "    v_unrelated = a + b + c + d;\n"
                           "    gl_Position = a_input + d;\n"
                           "}\n",
                           args),
                formatGLSL("${VERSION}"
                           "${IN} ${IN_PREC} vec4 a_input;\n"
                           "${OUT} mediump vec4 v_unrelated;\n"
                           "invariant gl_Position;\n"
                           "void main ()\n"
                           "{\n"
                           "    ${IN_PREC} vec4 a = ${MEDIUM_VALUE} * (a_input.xxxx + a_input.yyyy);\n"
                           "    ${IN_PREC} vec4 c = a * a;\n"
                           "    ${IN_PREC} vec4 d = c / ${MEDIUM_VALUE} / ${MEDIUM_VALUE};\n"
                           "    v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
                           "    gl_Position = a_input + d;\n"
                           "}\n",
                           args)));

            // Invariant value can be calculated using unrelated value

            group->addChild(new BasicInvarianceTest(m_context, "common_subexpression_3",
                                                    "Shader shares a subexpression with an unrelated variable.",
                                                    formatGLSL("${VERSION}"
                                                               "${IN} ${IN_PREC} vec4 a_input;\n"
                                                               "${OUT} mediump vec4 v_unrelated;\n"
                                                               "invariant gl_Position;\n"
                                                               "void main ()\n"
                                                               "{\n"
                                                               "    ${IN_PREC} float x = a_input.x * 0.2;\n"
                                                               "    ${IN_PREC} vec4 a = a_input.xxyx * 0.7;\n"
                                                               "    ${IN_PREC} vec4 b = a_input.yxyz * 0.7;\n"
                                                               "    ${IN_PREC} vec4 c = a_input.zxyx * 0.5;\n"
                                                               "    ${IN_PREC} vec4 f = x*a + x*b + x*c;\n"
                                                               "    v_unrelated = f;\n"
                                                               "    ${IN_PREC} vec4 g = x * (a + b + c);\n"
                                                               "    gl_Position = a_input + g;\n"
                                                               "}\n",
                                                               args),
                                                    formatGLSL("${VERSION}"
                                                               "${IN} ${IN_PREC} vec4 a_input;\n"
                                                               "${OUT} mediump vec4 v_unrelated;\n"
                                                               "invariant gl_Position;\n"
                                                               "void main ()\n"
                                                               "{\n"
                                                               "    ${IN_PREC} float x = a_input.x * 0.2;\n"
                                                               "    ${IN_PREC} vec4 a = a_input.xxyx * 0.7;\n"
                                                               "    ${IN_PREC} vec4 b = a_input.yxyz * 0.7;\n"
                                                               "    ${IN_PREC} vec4 c = a_input.zxyx * 0.5;\n"
                                                               "    v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
                                                               "    ${IN_PREC} vec4 g = x * (a + b + c);\n"
                                                               "    gl_Position = a_input + g;\n"
                                                               "}\n",
                                                               args)));
        }

        // shared subexpression of different precision
        {
            for (int precisionOther = glu::PRECISION_LOWP; precisionOther != glu::PRECISION_LAST; ++precisionOther)
            {
                const char *const unrelatedPrec = glu::getPrecisionName((glu::Precision)precisionOther);
                const glu::Precision minPrecision =
                    (precisionOther < (int)precision) ? ((glu::Precision)precisionOther) : (precision);
                const char *const multiplierStr =
                    (minPrecision == glu::PRECISION_LOWP) ? ("0.8, 0.4, -0.2, 0.3") : ("1.0e1, 5.0e2, 2.0e2, 1.0");
                const char *const normalizationStrUsed =
                    (minPrecision == glu::PRECISION_LOWP) ?
                        ("vec4(fract(used2).xyz, 0.0)") :
                        ("vec4(fract(used2 / 1.0e2).xyz - fract(used2 / 1.0e3).xyz, 0.0)");
                const char *const normalizationStrUnrelated =
                    (minPrecision == glu::PRECISION_LOWP) ?
                        ("vec4(fract(unrelated2).xyz, 0.0)") :
                        ("vec4(fract(unrelated2 / 1.0e2).xyz - fract(unrelated2 / 1.0e3).xyz, 0.0)");

                group->addChild(new BasicInvarianceTest(
                    m_context, ("subexpression_precision_" + std::string(unrelatedPrec)).c_str(),
                    "Shader shares subexpression of different precision with an unrelated variable.",
                    formatGLSL(
                        "${VERSION}"
                        "${IN} ${IN_PREC} vec4 a_input;\n"
                        "${OUT} ${UNRELATED_PREC} vec4 v_unrelated;\n"
                        "invariant gl_Position;\n"
                        "void main ()\n"
                        "{\n"
                        "    ${UNRELATED_PREC} vec4 unrelated0 = a_input + vec4(0.1, 0.2, 0.3, 0.4);\n"
                        "    ${UNRELATED_PREC} vec4 unrelated1 = vec4(${MULTIPLIER}) * unrelated0.xywz + unrelated0;\n"
                        "    ${UNRELATED_PREC} vec4 unrelated2 = refract(unrelated1, unrelated0, distance(unrelated0, "
                        "unrelated1));\n"
                        "    v_unrelated = a_input + 0.02 * ${NORMALIZE_UNRELATED};\n"
                        "    ${IN_PREC} vec4 used0 = a_input + vec4(0.1, 0.2, 0.3, 0.4);\n"
                        "    ${IN_PREC} vec4 used1 = vec4(${MULTIPLIER}) * used0.xywz + used0;\n"
                        "    ${IN_PREC} vec4 used2 = refract(used1, used0, distance(used0, used1));\n"
                        "    gl_Position = a_input + 0.02 * ${NORMALIZE_USED};\n"
                        "}\n",
                        FormatArgumentList(args) << FormatArgument("UNRELATED_PREC", unrelatedPrec)
                                                 << FormatArgument("MULTIPLIER", multiplierStr)
                                                 << FormatArgument("NORMALIZE_USED", normalizationStrUsed)
                                                 << FormatArgument("NORMALIZE_UNRELATED", normalizationStrUnrelated)),
                    formatGLSL("${VERSION}"
                               "${IN} ${IN_PREC} vec4 a_input;\n"
                               "${OUT} ${UNRELATED_PREC} vec4 v_unrelated;\n"
                               "invariant gl_Position;\n"
                               "void main ()\n"
                               "{\n"
                               "    v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
                               "    ${IN_PREC} vec4 used0 = a_input + vec4(0.1, 0.2, 0.3, 0.4);\n"
                               "    ${IN_PREC} vec4 used1 = vec4(${MULTIPLIER}) * used0.xywz + used0;\n"
                               "    ${IN_PREC} vec4 used2 = refract(used1, used0, distance(used0, used1));\n"
                               "    gl_Position = a_input + 0.02 * ${NORMALIZE_USED};\n"
                               "}\n",
                               FormatArgumentList(args)
                                   << FormatArgument("UNRELATED_PREC", unrelatedPrec)
                                   << FormatArgument("MULTIPLIER", multiplierStr)
                                   << FormatArgument("NORMALIZE_USED", normalizationStrUsed)
                                   << FormatArgument("NORMALIZE_UNRELATED", normalizationStrUnrelated))));
            }
        }

        // loops
        {
            group->addChild(new BasicInvarianceTest(
                m_context, "loop_0", "Invariant value set using a loop",
                formatGLSL("${VERSION}"
                           "${IN} ${IN_PREC} vec4 a_input;\n"
                           "${OUT} highp vec4 v_unrelated;\n"
                           "invariant gl_Position;\n"
                           "void main ()\n"
                           "{\n"
                           "    ${IN_PREC} vec4 value = a_input;\n"
                           "    v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
                           "    for (mediump int i = 0; i < ${LOOP_ITERS}; ++i)\n"
                           "    {\n"
                           "        value *= ${LOOP_MULTIPLIER};\n"
                           "        v_unrelated += value;\n"
                           "    }\n"
                           "    gl_Position = vec4(value.xyz / ${LOOP_NORM_LITERAL} + a_input.xyz * 0.1, 1.0);\n"
                           "}\n",
                           args),
                formatGLSL("${VERSION}"
                           "${IN} ${IN_PREC} vec4 a_input;\n"
                           "${OUT} highp vec4 v_unrelated;\n"
                           "invariant gl_Position;\n"
                           "void main ()\n"
                           "{\n"
                           "    ${IN_PREC} vec4 value = a_input;\n"
                           "    v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
                           "    for (mediump int i = 0; i < ${LOOP_ITERS}; ++i)\n"
                           "    {\n"
                           "        value *= ${LOOP_MULTIPLIER};\n"
                           "    }\n"
                           "    gl_Position = vec4(value.xyz / ${LOOP_NORM_LITERAL} + a_input.xyz * 0.1, 1.0);\n"
                           "}\n",
                           args)));

            group->addChild(new BasicInvarianceTest(
                m_context, "loop_1", "Invariant value set using a loop",
                formatGLSL("${VERSION}"
                           "${IN} ${IN_PREC} vec4 a_input;\n"
                           "${OUT} mediump vec4 v_unrelated;\n"
                           "invariant gl_Position;\n"
                           "void main ()\n"
                           "{\n"
                           "    ${IN_PREC} vec4 value = a_input;\n"
                           "    for (mediump int i = 0; i < ${LOOP_ITERS}; ++i)\n"
                           "    {\n"
                           "        value *= ${LOOP_MULTIPLIER};\n"
                           "        if (i == ${LOOP_ITERS_PARTIAL})\n"
                           "            v_unrelated = value;\n"
                           "    }\n"
                           "    gl_Position = vec4(value.xyz / ${LOOP_NORM_LITERAL} + a_input.xyz * 0.1, 1.0);\n"
                           "}\n",
                           args),
                formatGLSL("${VERSION}"
                           "${IN} ${IN_PREC} vec4 a_input;\n"
                           "${OUT} mediump vec4 v_unrelated;\n"
                           "invariant gl_Position;\n"
                           "void main ()\n"
                           "{\n"
                           "    ${IN_PREC} vec4 value = a_input;\n"
                           "    v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
                           "    for (mediump int i = 0; i < ${LOOP_ITERS}; ++i)\n"
                           "    {\n"
                           "        value *= ${LOOP_MULTIPLIER};\n"
                           "    }\n"
                           "    gl_Position = vec4(value.xyz / ${LOOP_NORM_LITERAL} + a_input.xyz * 0.1, 1.0);\n"
                           "}\n",
                           args)));

            group->addChild(
                new BasicInvarianceTest(m_context, "loop_2", "Invariant value set using a loop",
                                        formatGLSL("${VERSION}"
                                                   "${IN} ${IN_PREC} vec4 a_input;\n"
                                                   "${OUT} mediump vec4 v_unrelated;\n"
                                                   "invariant gl_Position;\n"
                                                   "void main ()\n"
                                                   "{\n"
                                                   "    ${IN_PREC} vec4 value = a_input;\n"
                                                   "    v_unrelated = vec4(0.0, 0.0, -1.0, 1.0);\n"
                                                   "    for (mediump int i = 0; i < ${LOOP_ITERS}; ++i)\n"
                                                   "    {\n"
                                                   "        value *= ${LOOP_MULTIPLIER};\n"
                                                   "        if (i == ${LOOP_ITERS_PARTIAL})\n"
                                                   "            gl_Position = a_input + 0.05 * vec4(fract(value.xyz / "
                                                   "1.0e${LOOP_NORM_FRACT_EXP}), 1.0);\n"
                                                   "        else\n"
                                                   "            v_unrelated = value + a_input;\n"
                                                   "    }\n"
                                                   "}\n",
                                                   args),
                                        formatGLSL("${VERSION}"
                                                   "${IN} ${IN_PREC} vec4 a_input;\n"
                                                   "${OUT} mediump vec4 v_unrelated;\n"
                                                   "invariant gl_Position;\n"
                                                   "void main ()\n"
                                                   "{\n"
                                                   "    ${IN_PREC} vec4 value = a_input;\n"
                                                   "    v_unrelated = vec4(0.0, 0.0, -1.0, 1.0);\n"
                                                   "    for (mediump int i = 0; i < ${LOOP_ITERS}; ++i)\n"
                                                   "    {\n"
                                                   "        value *= ${LOOP_MULTIPLIER};\n"
                                                   "        if (i == ${LOOP_ITERS_PARTIAL})\n"
                                                   "            gl_Position = a_input + 0.05 * vec4(fract(value.xyz / "
                                                   "1.0e${LOOP_NORM_FRACT_EXP}), 1.0);\n"
                                                   "        else\n"
                                                   "            v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
                                                   "    }\n"
                                                   "}\n",
                                                   args)));

            group->addChild(new BasicInvarianceTest(
                m_context, "loop_3", "Invariant value set using a loop",
                formatGLSL(
                    "${VERSION}"
                    "${IN} ${IN_PREC} vec4 a_input;\n"
                    "${OUT} mediump vec4 v_unrelated;\n"
                    "invariant gl_Position;\n"
                    "void main ()\n"
                    "{\n"
                    "    ${IN_PREC} vec4 value = a_input;\n"
                    "    gl_Position = vec4(0.0, 0.0, 0.0, 0.0);\n"
                    "    v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
                    "    for (mediump int i = 0; i < ${LOOP_ITERS}; ++i)\n"
                    "    {\n"
                    "        value *= ${LOOP_MULTIPLIER};\n"
                    "        gl_Position += vec4(value.xyz / ${SUM_LOOP_NORM_LITERAL} + a_input.xyz * 0.1, 1.0);\n"
                    "        v_unrelated = gl_Position.xyzx * a_input;\n"
                    "    }\n"
                    "}\n",
                    args),
                formatGLSL(
                    "${VERSION}"
                    "${IN} ${IN_PREC} vec4 a_input;\n"
                    "${OUT} mediump vec4 v_unrelated;\n"
                    "invariant gl_Position;\n"
                    "void main ()\n"
                    "{\n"
                    "    ${IN_PREC} vec4 value = a_input;\n"
                    "    gl_Position = vec4(0.0, 0.0, 0.0, 0.0);\n"
                    "    v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
                    "    for (mediump int i = 0; i < ${LOOP_ITERS}; ++i)\n"
                    "    {\n"
                    "        value *= ${LOOP_MULTIPLIER};\n"
                    "        gl_Position += vec4(value.xyz / ${SUM_LOOP_NORM_LITERAL} + a_input.xyz * 0.1, 1.0);\n"
                    "    }\n"
                    "}\n",
                    args)));

            group->addChild(new BasicInvarianceTest(
                m_context, "loop_4", "Invariant value set using a loop",
                formatGLSL(
                    "${VERSION}"
                    "${IN} ${IN_PREC} vec4 a_input;\n"
                    "${OUT} mediump vec4 v_unrelated;\n"
                    "invariant gl_Position;\n"
                    "void main ()\n"
                    "{\n"
                    "    ${IN_PREC} vec4 position = vec4(0.0, 0.0, 0.0, 0.0);\n"
                    "    ${IN_PREC} vec4 value1 = a_input;\n"
                    "    ${IN_PREC} vec4 value2 = a_input;\n"
                    "    v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
                    "    for (mediump int i = 0; i < ${LOOP_ITERS}; ++i)\n"
                    "    {\n"
                    "        value1 *= ${LOOP_MULTIPLIER};\n"
                    "        v_unrelated = v_unrelated*1.3 + a_input.xyzx * value1.xyxw;\n"
                    "    }\n"
                    "    for (mediump int i = 0; i < ${LOOP_ITERS}; ++i)\n"
                    "    {\n"
                    "        value2 *= ${LOOP_MULTIPLIER};\n"
                    "        position = position*1.3 + a_input.xyzx * value2.xyxw;\n"
                    "    }\n"
                    "    gl_Position = a_input + 0.05 * vec4(fract(position.xyz / 1.0e${LOOP_NORM_FRACT_EXP}), 1.0);\n"
                    "}\n",
                    args),
                formatGLSL(
                    "${VERSION}"
                    "${IN} ${IN_PREC} vec4 a_input;\n"
                    "${OUT} mediump vec4 v_unrelated;\n"
                    "invariant gl_Position;\n"
                    "void main ()\n"
                    "{\n"
                    "    ${IN_PREC} vec4 position = vec4(0.0, 0.0, 0.0, 0.0);\n"
                    "    ${IN_PREC} vec4 value2 = a_input;\n"
                    "    v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
                    "    for (mediump int i = 0; i < ${LOOP_ITERS}; ++i)\n"
                    "    {\n"
                    "        value2 *= ${LOOP_MULTIPLIER};\n"
                    "        position = position*1.3 + a_input.xyzx * value2.xyxw;\n"
                    "    }\n"
                    "    gl_Position = a_input + 0.05 * vec4(fract(position.xyz / 1.0e${LOOP_NORM_FRACT_EXP}), 1.0);\n"
                    "}\n",
                    args)));
        }
    }
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
