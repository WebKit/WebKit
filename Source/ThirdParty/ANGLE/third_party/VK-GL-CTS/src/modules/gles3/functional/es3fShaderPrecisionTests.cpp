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
 * \brief Shader precision tests.
 *
 * \note Floating-point case uses R32UI render target and uses
 *         floatBitsToUint() in shader to write out floating-point value bits.
 *         This is done since ES3 core doesn't support FP render targets.
 *//*--------------------------------------------------------------------*/

#include "es3fShaderPrecisionTests.hpp"
#include "tcuVector.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuFloat.hpp"
#include "tcuFormatUtil.hpp"
#include "gluRenderContext.hpp"
#include "gluShaderProgram.hpp"
#include "gluShaderUtil.hpp"
#include "gluDrawUtil.hpp"
#include "deRandom.hpp"
#include "deString.h"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include <algorithm>

namespace deqp
{
namespace gles3
{
namespace Functional
{

using std::ostringstream;
using std::string;
using std::vector;
using tcu::TestLog;

enum
{
    FRAMEBUFFER_WIDTH  = 32,
    FRAMEBUFFER_HEIGHT = 32
};

static glu::ShaderProgram *createFloatPrecisionEvalProgram(const glu::RenderContext &context, glu::Precision precision,
                                                           const char *evalOp, bool isVertexCase)
{
    glu::DataType type      = glu::TYPE_FLOAT;
    glu::DataType outType   = glu::TYPE_UINT;
    const char *typeName    = glu::getDataTypeName(type);
    const char *outTypeName = glu::getDataTypeName(outType);
    const char *precName    = glu::getPrecisionName(precision);
    ostringstream vtx;
    ostringstream frag;
    ostringstream &op = isVertexCase ? vtx : frag;

    vtx << "#version 300 es\n"
        << "in highp vec4 a_position;\n"
        << "in " << precName << " " << typeName << " a_in0;\n"
        << "in " << precName << " " << typeName << " a_in1;\n";
    frag << "#version 300 es\n"
         << "layout(location = 0) out highp " << outTypeName << " o_out;\n";

    if (isVertexCase)
    {
        vtx << "flat out " << precName << " " << typeName << " v_out;\n";
        frag << "flat in " << precName << " " << typeName << " v_out;\n";
    }
    else
    {
        vtx << "flat out " << precName << " " << typeName << " v_in0;\n"
            << "flat out " << precName << " " << typeName << " v_in1;\n";
        frag << "flat in " << precName << " " << typeName << " v_in0;\n"
             << "flat in " << precName << " " << typeName << " v_in1;\n";
    }

    vtx << "\nvoid main (void)\n{\n"
        << "    gl_Position = a_position;\n";
    frag << "\nvoid main (void)\n{\n";

    op << "\t" << precName << " " << typeName << " in0 = " << (isVertexCase ? "a_" : "v_") << "in0;\n"
       << "\t" << precName << " " << typeName << " in1 = " << (isVertexCase ? "a_" : "v_") << "in1;\n";

    if (!isVertexCase)
        op << "\t" << precName << " " << typeName << " res;\n";

    op << "\t" << (isVertexCase ? "v_out" : "res") << " = " << evalOp << ";\n";

    if (isVertexCase)
    {
        frag << "    o_out = floatBitsToUint(v_out);\n";
    }
    else
    {
        vtx << "    v_in0 = a_in0;\n"
            << "    v_in1 = a_in1;\n";
        frag << "    o_out = floatBitsToUint(res);\n";
    }

    vtx << "}\n";
    frag << "}\n";

    return new glu::ShaderProgram(context, glu::makeVtxFragSources(vtx.str(), frag.str()));
}

static glu::ShaderProgram *createIntUintPrecisionEvalProgram(const glu::RenderContext &context, glu::DataType type,
                                                             glu::Precision precision, const char *evalOp,
                                                             bool isVertexCase)
{
    const char *typeName = glu::getDataTypeName(type);
    const char *precName = glu::getPrecisionName(precision);
    ostringstream vtx;
    ostringstream frag;
    ostringstream &op = isVertexCase ? vtx : frag;

    vtx << "#version 300 es\n"
        << "in highp vec4 a_position;\n"
        << "in " << precName << " " << typeName << " a_in0;\n"
        << "in " << precName << " " << typeName << " a_in1;\n";
    frag << "#version 300 es\n"
         << "layout(location = 0) out " << precName << " " << typeName << " o_out;\n";

    if (isVertexCase)
    {
        vtx << "flat out " << precName << " " << typeName << " v_out;\n";
        frag << "flat in " << precName << " " << typeName << " v_out;\n";
    }
    else
    {
        vtx << "flat out " << precName << " " << typeName << " v_in0;\n"
            << "flat out " << precName << " " << typeName << " v_in1;\n";
        frag << "flat in " << precName << " " << typeName << " v_in0;\n"
             << "flat in " << precName << " " << typeName << " v_in1;\n";
    }

    vtx << "\nvoid main (void)\n{\n"
        << "    gl_Position = a_position;\n";
    frag << "\nvoid main (void)\n{\n";

    op << "\t" << precName << " " << typeName << " in0 = " << (isVertexCase ? "a_" : "v_") << "in0;\n"
       << "\t" << precName << " " << typeName << " in1 = " << (isVertexCase ? "a_" : "v_") << "in1;\n";

    op << "\t" << (isVertexCase ? "v_" : "o_") << "out = " << evalOp << ";\n";

    if (isVertexCase)
    {
        frag << "    o_out = v_out;\n";
    }
    else
    {
        vtx << "    v_in0 = a_in0;\n"
            << "    v_in1 = a_in1;\n";
    }

    vtx << "}\n";
    frag << "}\n";

    return new glu::ShaderProgram(context, glu::makeVtxFragSources(vtx.str(), frag.str()));
}

class ShaderFloatPrecisionCase : public TestCase
{
public:
    typedef double (*EvalFunc)(double in0, double in1);

    ShaderFloatPrecisionCase(Context &context, const char *name, const char *desc, const char *op, EvalFunc evalFunc,
                             glu::Precision precision, const tcu::Vec2 &rangeA, const tcu::Vec2 &rangeB,
                             bool isVertexCase);
    ~ShaderFloatPrecisionCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

protected:
    bool compare(float in0, float in1, double reference, float result);

private:
    ShaderFloatPrecisionCase(const ShaderFloatPrecisionCase &other);
    ShaderFloatPrecisionCase &operator=(const ShaderFloatPrecisionCase &other);

    // Case parameters.
    std::string m_op;
    EvalFunc m_evalFunc;
    glu::Precision m_precision;
    tcu::Vec2 m_rangeA;
    tcu::Vec2 m_rangeB;
    bool m_isVertexCase;

    int m_numTestsPerIter;
    int m_numIters;
    de::Random m_rnd;

    // Iteration state.
    glu::ShaderProgram *m_program;
    uint32_t m_framebuffer;
    uint32_t m_renderbuffer;
    int m_iterNdx;
};

ShaderFloatPrecisionCase::ShaderFloatPrecisionCase(Context &context, const char *name, const char *desc, const char *op,
                                                   EvalFunc evalFunc, glu::Precision precision, const tcu::Vec2 &rangeA,
                                                   const tcu::Vec2 &rangeB, bool isVertexCase)
    : TestCase(context, name, desc)
    , m_op(op)
    , m_evalFunc(evalFunc)
    , m_precision(precision)
    , m_rangeA(rangeA)
    , m_rangeB(rangeB)
    , m_isVertexCase(isVertexCase)
    , m_numTestsPerIter(32)
    , m_numIters(4)
    , m_rnd(deStringHash(name))
    , m_program(nullptr)
    , m_framebuffer(0)
    , m_renderbuffer(0)
    , m_iterNdx(0)
{
}

ShaderFloatPrecisionCase::~ShaderFloatPrecisionCase(void)
{
    ShaderFloatPrecisionCase::deinit();
}

void ShaderFloatPrecisionCase::init(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    TestLog &log             = m_testCtx.getLog();

    DE_ASSERT(!m_program && !m_framebuffer && !m_renderbuffer);

    // Create program.
    m_program =
        createFloatPrecisionEvalProgram(m_context.getRenderContext(), m_precision, m_op.c_str(), m_isVertexCase);
    log << *m_program;

    TCU_CHECK(m_program->isOk());

    // Create framebuffer.
    gl.genFramebuffers(1, &m_framebuffer);
    gl.genRenderbuffers(1, &m_renderbuffer);

    gl.bindRenderbuffer(GL_RENDERBUFFER, m_renderbuffer);
    gl.renderbufferStorage(GL_RENDERBUFFER, GL_R32UI, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);

    gl.bindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_renderbuffer);

    GLU_EXPECT_NO_ERROR(gl.getError(), "Post framebuffer setup");
    TCU_CHECK(gl.checkFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    gl.bindFramebuffer(GL_FRAMEBUFFER, m_context.getRenderContext().getDefaultFramebuffer());

    // Initialize test result to pass.
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    m_iterNdx = 0;
}

void ShaderFloatPrecisionCase::deinit(void)
{
    delete m_program;

    if (m_framebuffer)
        m_context.getRenderContext().getFunctions().deleteFramebuffers(1, &m_framebuffer);

    if (m_renderbuffer)
        m_context.getRenderContext().getFunctions().deleteRenderbuffers(1, &m_renderbuffer);

    m_program      = nullptr;
    m_framebuffer  = 0;
    m_renderbuffer = 0;
}

bool ShaderFloatPrecisionCase::compare(float in0, float in1, double reference, float result)
{
    // Comparison is done using 64-bit reference value to accurately evaluate rounding mode error.
    // If 32-bit reference value is used, 2 bits of rounding error must be allowed.

    // For mediump and lowp types the comparison currently allows 3 bits of rounding error:
    // two bits from conversions and one from actual operation.

    // \todo [2013-09-30 pyry] Make this more strict: determine if rounding can actually happen.

    const int mantissaBits = m_precision == glu::PRECISION_HIGHP ? 23 : 10;
    const int numPrecBits  = 52 - mantissaBits;

    const int in0Exp      = tcu::Float32(in0).exponent();
    const int in1Exp      = tcu::Float32(in1).exponent();
    const int resExp      = tcu::Float32(result).exponent();
    const int numLostBits = de::max(de::max(in0Exp - resExp, in1Exp - resExp), 0); // Lost due to mantissa shift.

    const int roundingUlpError = m_precision == glu::PRECISION_HIGHP ? 1 : 3;
    const int maskBits         = numLostBits + numPrecBits;

    m_testCtx.getLog() << TestLog::Message << "Assuming " << mantissaBits << " mantissa bits, " << numLostBits
                       << " bits lost in operation, and " << roundingUlpError << " ULP rounding error."
                       << TestLog::EndMessage;

    {
        const uint64_t refBits         = tcu::Float64(reference).bits();
        const uint64_t resBits         = tcu::Float64(result).bits();
        const uint64_t accurateRefBits = maskBits < 64 ? refBits >> (uint64_t)maskBits : 0u;
        const uint64_t accurateResBits = maskBits < 64 ? resBits >> (uint64_t)maskBits : 0u;
        const uint64_t ulpDiff         = (uint64_t)de::abs((int64_t)accurateRefBits - (int64_t)accurateResBits);

        if (ulpDiff > (uint64_t)roundingUlpError)
        {
            m_testCtx.getLog() << TestLog::Message
                               << "ERROR: comparison failed! ULP diff (ignoring lost/undefined bits) = " << ulpDiff
                               << TestLog::EndMessage;
            return false;
        }
        else
            return true;
    }
}

ShaderFloatPrecisionCase::IterateResult ShaderFloatPrecisionCase::iterate(void)
{
    // Constant data.
    const float position[]   = {-1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f,
                                1.0f,  -1.0f, 0.0f, 1.0f, 1.0f,  1.0f, 0.0f, 1.0f};
    const uint16_t indices[] = {0, 1, 2, 2, 1, 3};

    const int numVertices = 4;
    float in0Arr[4]       = {0.0f};
    float in1Arr[4]       = {0.0f};

    TestLog &log             = m_testCtx.getLog();
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    vector<glu::VertexArrayBinding> vertexArrays;

    // Image read from GL.
    std::vector<float> pixels(FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT * 4);

    // \todo [2012-05-03 pyry] Could be cached.
    uint32_t prog = m_program->getProgram();

    gl.useProgram(prog);
    gl.bindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

    vertexArrays.push_back(glu::va::Float("a_position", 4, numVertices, 0, &position[0]));
    vertexArrays.push_back(glu::va::Float("a_in0", 1, numVertices, 0, &in0Arr[0]));
    vertexArrays.push_back(glu::va::Float("a_in1", 1, numVertices, 0, &in1Arr[0]));

    GLU_EXPECT_NO_ERROR(gl.getError(), "After program setup");

    // Compute values and reference.
    for (int testNdx = 0; testNdx < m_numTestsPerIter; testNdx++)
    {
        const float in0   = m_rnd.getFloat(m_rangeA.x(), m_rangeA.y());
        const float in1   = m_rnd.getFloat(m_rangeB.x(), m_rangeB.y());
        const double refD = m_evalFunc((double)in0, (double)in1);
        const float refF  = tcu::Float64(refD).asFloat(); // Uses RTE rounding mode.

        log << TestLog::Message << "iter " << m_iterNdx << ", test " << testNdx << ": "
            << "in0 = " << in0 << " / " << tcu::toHex(tcu::Float32(in0).bits()) << ", in1 = " << in1 << " / "
            << tcu::toHex(tcu::Float32(in1).bits()) << TestLog::EndMessage << TestLog::Message
            << "  reference = " << refF << " / " << tcu::toHex(tcu::Float32(refF).bits()) << TestLog::EndMessage;

        std::fill(&in0Arr[0], &in0Arr[0] + DE_LENGTH_OF_ARRAY(in0Arr), in0);
        std::fill(&in1Arr[0], &in1Arr[0] + DE_LENGTH_OF_ARRAY(in1Arr), in1);

        glu::draw(m_context.getRenderContext(), prog, (int)vertexArrays.size(), &vertexArrays[0],
                  glu::pr::Triangles(DE_LENGTH_OF_ARRAY(indices), &indices[0]));
        gl.readPixels(0, 0, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT, GL_RGBA_INTEGER, GL_UNSIGNED_INT, &pixels[0]);
        GLU_EXPECT_NO_ERROR(gl.getError(), "After render");

        log << TestLog::Message << "  result = " << pixels[0] << " / " << tcu::toHex(tcu::Float32(pixels[0]).bits())
            << TestLog::EndMessage;

        // Verify results
        {
            const bool firstPixelOk = compare(in0, in1, refD, pixels[0]);

            if (firstPixelOk)
            {
                // Check that rest of pixels match to first one.
                const uint32_t firstPixelBits = tcu::Float32(pixels[0]).bits();
                bool allPixelsOk              = true;

                for (int y = 0; y < FRAMEBUFFER_HEIGHT; y++)
                {
                    for (int x = 0; x < FRAMEBUFFER_WIDTH; x++)
                    {
                        const uint32_t pixelBits = tcu::Float32(pixels[(y * FRAMEBUFFER_WIDTH + x) * 4]).bits();

                        if (pixelBits != firstPixelBits)
                        {
                            log << TestLog::Message << "ERROR: Inconsistent results, got " << tcu::toHex(pixelBits)
                                << " at (" << x << ", " << y << ")" << TestLog::EndMessage;
                            allPixelsOk = false;
                        }
                    }

                    if (!allPixelsOk)
                        break;
                }

                if (!allPixelsOk)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Inconsistent values in framebuffer");
            }
            else
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Result comparison failed");
        }

        if (m_testCtx.getTestResult() != QP_TEST_RESULT_PASS)
            break;
    }

    gl.bindFramebuffer(GL_FRAMEBUFFER, m_context.getRenderContext().getDefaultFramebuffer());
    GLU_EXPECT_NO_ERROR(gl.getError(), "After iteration");

    m_iterNdx += 1;
    return (m_iterNdx < m_numIters && m_testCtx.getTestResult() == QP_TEST_RESULT_PASS) ? CONTINUE : STOP;
}

class ShaderIntPrecisionCase : public TestCase
{
public:
    typedef int64_t (*EvalFunc)(int64_t a, int64_t b);

    ShaderIntPrecisionCase(Context &context, const char *name, const char *desc, const char *op, EvalFunc evalFunc,
                           glu::Precision precision, int bits, const tcu::IVec2 &rangeA, const tcu::IVec2 &rangeB,
                           bool isVertexCase);
    ~ShaderIntPrecisionCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    ShaderIntPrecisionCase(const ShaderIntPrecisionCase &other);
    ShaderIntPrecisionCase &operator=(const ShaderIntPrecisionCase &other);

    // Case parameters.
    std::string m_op;
    EvalFunc m_evalFunc;
    glu::Precision m_precision;
    int m_bits;
    tcu::IVec2 m_rangeA;
    tcu::IVec2 m_rangeB;
    bool m_isVertexCase;

    int m_numTestsPerIter;
    int m_numIters;
    de::Random m_rnd;

    // Iteration state.
    glu::ShaderProgram *m_program;
    uint32_t m_framebuffer;
    uint32_t m_renderbuffer;
    int m_iterNdx;
};

ShaderIntPrecisionCase::ShaderIntPrecisionCase(Context &context, const char *name, const char *desc, const char *op,
                                               EvalFunc evalFunc, glu::Precision precision, int bits,
                                               const tcu::IVec2 &rangeA, const tcu::IVec2 &rangeB, bool isVertexCase)
    : TestCase(context, name, desc)
    , m_op(op)
    , m_evalFunc(evalFunc)
    , m_precision(precision)
    , m_bits(bits)
    , m_rangeA(rangeA)
    , m_rangeB(rangeB)
    , m_isVertexCase(isVertexCase)
    , m_numTestsPerIter(32)
    , m_numIters(4)
    , m_rnd(deStringHash(name))
    , m_program(nullptr)
    , m_framebuffer(0)
    , m_renderbuffer(0)
    , m_iterNdx(0)
{
}

ShaderIntPrecisionCase::~ShaderIntPrecisionCase(void)
{
    ShaderIntPrecisionCase::deinit();
}

void ShaderIntPrecisionCase::init(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    TestLog &log             = m_testCtx.getLog();

    DE_ASSERT(!m_program && !m_framebuffer && !m_renderbuffer);

    // Create program.
    m_program = createIntUintPrecisionEvalProgram(m_context.getRenderContext(), glu::TYPE_INT, m_precision,
                                                  m_op.c_str(), m_isVertexCase);
    log << *m_program;

    TCU_CHECK(m_program->isOk());

    // Create framebuffer.
    gl.genFramebuffers(1, &m_framebuffer);
    gl.genRenderbuffers(1, &m_renderbuffer);

    gl.bindRenderbuffer(GL_RENDERBUFFER, m_renderbuffer);
    gl.renderbufferStorage(GL_RENDERBUFFER, GL_R32I, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);

    gl.bindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_renderbuffer);

    GLU_EXPECT_NO_ERROR(gl.getError(), "Post framebuffer setup");
    TCU_CHECK(gl.checkFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    gl.bindFramebuffer(GL_FRAMEBUFFER, m_context.getRenderContext().getDefaultFramebuffer());

    // Initialize test result to pass.
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    m_iterNdx = 0;

    log << TestLog::Message << "Number of accurate bits assumed = " << m_bits << TestLog::EndMessage;
}

void ShaderIntPrecisionCase::deinit(void)
{
    delete m_program;

    if (m_framebuffer)
        m_context.getRenderContext().getFunctions().deleteFramebuffers(1, &m_framebuffer);

    if (m_renderbuffer)
        m_context.getRenderContext().getFunctions().deleteRenderbuffers(1, &m_renderbuffer);

    m_program      = nullptr;
    m_framebuffer  = 0;
    m_renderbuffer = 0;
}

ShaderIntPrecisionCase::IterateResult ShaderIntPrecisionCase::iterate(void)
{
    // Constant data.
    const float position[]   = {-1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f,
                                1.0f,  -1.0f, 0.0f, 1.0f, 1.0f,  1.0f, 0.0f, 1.0f};
    const uint16_t indices[] = {0, 1, 2, 2, 1, 3};

    const int numVertices = 4;
    int in0Arr[4]         = {0};
    int in1Arr[4]         = {0};

    TestLog &log             = m_testCtx.getLog();
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    uint32_t mask            = m_bits == 32 ? 0xffffffffu : ((1u << m_bits) - 1);
    vector<int> pixels(FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT * 4);
    vector<glu::VertexArrayBinding> vertexArrays;

    uint32_t prog = m_program->getProgram();

    // \todo [2012-05-03 pyry] A bit hacky. getInt() should work fine with ranges like this.
    bool isMaxRangeA = m_rangeA.x() == (int)0x80000000 && m_rangeA.y() == (int)0x7fffffff;
    bool isMaxRangeB = m_rangeB.x() == (int)0x80000000 && m_rangeB.y() == (int)0x7fffffff;

    gl.useProgram(prog);
    gl.bindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

    vertexArrays.push_back(glu::va::Float("a_position", 4, numVertices, 0, &position[0]));
    vertexArrays.push_back(glu::va::Int32("a_in0", 1, numVertices, 0, &in0Arr[0]));
    vertexArrays.push_back(glu::va::Int32("a_in1", 1, numVertices, 0, &in1Arr[0]));

    GLU_EXPECT_NO_ERROR(gl.getError(), "After program setup");

    // Compute values and reference.
    for (int testNdx = 0; testNdx < m_numTestsPerIter; testNdx++)
    {
        int in0 = deSignExtendTo32(
            ((isMaxRangeA ? (int)m_rnd.getUint32() : m_rnd.getInt(m_rangeA.x(), m_rangeA.y())) & mask), m_bits);
        int in1 = deSignExtendTo32(
            ((isMaxRangeB ? (int)m_rnd.getUint32() : m_rnd.getInt(m_rangeB.x(), m_rangeB.y())) & mask), m_bits);
        int refMasked = static_cast<int>(m_evalFunc(in0, in1) & static_cast<uint64_t>(mask));
        int refOut    = deSignExtendTo32(refMasked, m_bits);

        log << TestLog::Message << "iter " << m_iterNdx << ", test " << testNdx << ": "
            << "in0 = " << in0 << ", in1 = " << in1 << ", ref out = " << refOut << " / " << tcu::toHex(refMasked)
            << TestLog::EndMessage;

        std::fill(&in0Arr[0], &in0Arr[0] + DE_LENGTH_OF_ARRAY(in0Arr), in0);
        std::fill(&in1Arr[0], &in1Arr[0] + DE_LENGTH_OF_ARRAY(in1Arr), in1);

        glu::draw(m_context.getRenderContext(), prog, (int)vertexArrays.size(), &vertexArrays[0],
                  glu::pr::Triangles(DE_LENGTH_OF_ARRAY(indices), &indices[0]));
        gl.readPixels(0, 0, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT, GL_RGBA_INTEGER, GL_INT, &pixels[0]);
        GLU_EXPECT_NO_ERROR(gl.getError(), "After render");

        // Compare pixels.
        for (int y = 0; y < FRAMEBUFFER_HEIGHT; y++)
        {
            for (int x = 0; x < FRAMEBUFFER_WIDTH; x++)
            {
                int cmpOut    = pixels[(y * FRAMEBUFFER_WIDTH + x) * 4];
                int cmpMasked = cmpOut & mask;

                if (cmpMasked != refMasked)
                {
                    log << TestLog::Message << "Comparison failed (at " << x << ", " << y << "): "
                        << "got " << cmpOut << " / " << tcu::toHex(cmpOut) << TestLog::EndMessage;
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
                    return STOP;
                }
            }
        }
    }

    gl.bindFramebuffer(GL_FRAMEBUFFER, m_context.getRenderContext().getDefaultFramebuffer());
    GLU_EXPECT_NO_ERROR(gl.getError(), "After iteration");

    m_iterNdx += 1;
    return (m_iterNdx < m_numIters) ? CONTINUE : STOP;
}

class ShaderUintPrecisionCase : public TestCase
{
public:
    typedef uint32_t (*EvalFunc)(uint32_t a, uint32_t b);

    ShaderUintPrecisionCase(Context &context, const char *name, const char *desc, const char *op, EvalFunc evalFunc,
                            glu::Precision precision, int bits, const tcu::UVec2 &rangeA, const tcu::UVec2 &rangeB,
                            bool isVertexCase);
    ~ShaderUintPrecisionCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    ShaderUintPrecisionCase(const ShaderUintPrecisionCase &other);
    ShaderUintPrecisionCase &operator=(const ShaderUintPrecisionCase &other);

    // Case parameters.
    std::string m_op;
    EvalFunc m_evalFunc;
    glu::Precision m_precision;
    int m_bits;
    tcu::UVec2 m_rangeA;
    tcu::UVec2 m_rangeB;
    bool m_isVertexCase;

    int m_numTestsPerIter;
    int m_numIters;
    de::Random m_rnd;

    // Iteration state.
    glu::ShaderProgram *m_program;
    uint32_t m_framebuffer;
    uint32_t m_renderbuffer;
    int m_iterNdx;
};

ShaderUintPrecisionCase::ShaderUintPrecisionCase(Context &context, const char *name, const char *desc, const char *op,
                                                 EvalFunc evalFunc, glu::Precision precision, int bits,
                                                 const tcu::UVec2 &rangeA, const tcu::UVec2 &rangeB, bool isVertexCase)
    : TestCase(context, name, desc)
    , m_op(op)
    , m_evalFunc(evalFunc)
    , m_precision(precision)
    , m_bits(bits)
    , m_rangeA(rangeA)
    , m_rangeB(rangeB)
    , m_isVertexCase(isVertexCase)
    , m_numTestsPerIter(32)
    , m_numIters(4)
    , m_rnd(deStringHash(name))
    , m_program(nullptr)
    , m_framebuffer(0)
    , m_renderbuffer(0)
    , m_iterNdx(0)
{
}

ShaderUintPrecisionCase::~ShaderUintPrecisionCase(void)
{
    ShaderUintPrecisionCase::deinit();
}

void ShaderUintPrecisionCase::init(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    TestLog &log             = m_testCtx.getLog();

    DE_ASSERT(!m_program && !m_framebuffer && !m_renderbuffer);

    // Create program.
    m_program = createIntUintPrecisionEvalProgram(m_context.getRenderContext(), glu::TYPE_UINT, m_precision,
                                                  m_op.c_str(), m_isVertexCase);
    log << *m_program;

    TCU_CHECK(m_program->isOk());

    // Create framebuffer.
    gl.genFramebuffers(1, &m_framebuffer);
    gl.genRenderbuffers(1, &m_renderbuffer);

    gl.bindRenderbuffer(GL_RENDERBUFFER, m_renderbuffer);
    gl.renderbufferStorage(GL_RENDERBUFFER, GL_R32UI, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);

    gl.bindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_renderbuffer);

    GLU_EXPECT_NO_ERROR(gl.getError(), "Post framebuffer setup");
    TCU_CHECK(gl.checkFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    gl.bindFramebuffer(GL_FRAMEBUFFER, m_context.getRenderContext().getDefaultFramebuffer());

    // Initialize test result to pass.
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    m_iterNdx = 0;

    log << TestLog::Message << "Number of accurate bits assumed = " << m_bits << TestLog::EndMessage;
}

void ShaderUintPrecisionCase::deinit(void)
{
    delete m_program;

    if (m_framebuffer)
        m_context.getRenderContext().getFunctions().deleteFramebuffers(1, &m_framebuffer);

    if (m_renderbuffer)
        m_context.getRenderContext().getFunctions().deleteRenderbuffers(1, &m_renderbuffer);

    m_program      = nullptr;
    m_framebuffer  = 0;
    m_renderbuffer = 0;
}

ShaderUintPrecisionCase::IterateResult ShaderUintPrecisionCase::iterate(void)
{
    // Constant data.
    const float position[]   = {-1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f,
                                1.0f,  -1.0f, 0.0f, 1.0f, 1.0f,  1.0f, 0.0f, 1.0f};
    const uint16_t indices[] = {0, 1, 2, 2, 1, 3};

    const int numVertices = 4;
    uint32_t in0Arr[4]    = {0};
    uint32_t in1Arr[4]    = {0};

    TestLog &log             = m_testCtx.getLog();
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    uint32_t mask            = m_bits == 32 ? 0xffffffffu : ((1u << m_bits) - 1);
    vector<uint32_t> pixels(FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT * 4);
    vector<glu::VertexArrayBinding> vertexArrays;

    uint32_t prog = m_program->getProgram();

    // \todo [2012-05-03 pyry] A bit hacky.
    bool isMaxRangeA = m_rangeA.x() == 0 && m_rangeA.y() == 0xffffffff;
    bool isMaxRangeB = m_rangeB.x() == 0 && m_rangeB.y() == 0xffffffff;

    gl.useProgram(prog);
    gl.bindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

    vertexArrays.push_back(glu::va::Float("a_position", 4, numVertices, 0, &position[0]));
    vertexArrays.push_back(glu::va::Uint32("a_in0", 1, numVertices, 0, &in0Arr[0]));
    vertexArrays.push_back(glu::va::Uint32("a_in1", 1, numVertices, 0, &in1Arr[0]));

    GLU_EXPECT_NO_ERROR(gl.getError(), "After program setup");

    // Compute values and reference.
    for (int testNdx = 0; testNdx < m_numTestsPerIter; testNdx++)
    {
        uint32_t in0 =
            (isMaxRangeA ? m_rnd.getUint32() : (m_rangeA.x() + m_rnd.getUint32() % (m_rangeA.y() - m_rangeA.x() + 1))) &
            mask;
        uint32_t in1 =
            (isMaxRangeB ? m_rnd.getUint32() : (m_rangeB.x() + m_rnd.getUint32() % (m_rangeB.y() - m_rangeB.x() + 1))) &
            mask;
        uint32_t refOut = m_evalFunc(in0, in1) & mask;

        log << TestLog::Message << "iter " << m_iterNdx << ", test " << testNdx << ": "
            << "in0 = " << tcu::toHex(in0) << ", in1 = " << tcu::toHex(in1) << ", ref out = " << tcu::toHex(refOut)
            << TestLog::EndMessage;

        std::fill(&in0Arr[0], &in0Arr[0] + DE_LENGTH_OF_ARRAY(in0Arr), in0);
        std::fill(&in1Arr[0], &in1Arr[0] + DE_LENGTH_OF_ARRAY(in1Arr), in1);

        glu::draw(m_context.getRenderContext(), prog, (int)vertexArrays.size(), &vertexArrays[0],
                  glu::pr::Triangles(DE_LENGTH_OF_ARRAY(indices), &indices[0]));
        gl.readPixels(0, 0, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT, GL_RGBA_INTEGER, GL_UNSIGNED_INT, &pixels[0]);
        GLU_EXPECT_NO_ERROR(gl.getError(), "After render");

        // Compare pixels.
        for (int y = 0; y < FRAMEBUFFER_HEIGHT; y++)
        {
            for (int x = 0; x < FRAMEBUFFER_WIDTH; x++)
            {
                uint32_t cmpOut    = pixels[(y * FRAMEBUFFER_WIDTH + x) * 4];
                uint32_t cmpMasked = cmpOut & mask;

                if (cmpMasked != refOut)
                {
                    log << TestLog::Message << "Comparison failed (at " << x << ", " << y << "): "
                        << "got " << tcu::toHex(cmpOut) << TestLog::EndMessage;
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
                    return STOP;
                }
            }
        }
    }

    gl.bindFramebuffer(GL_FRAMEBUFFER, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "After iteration");

    m_iterNdx += 1;
    return (m_iterNdx < m_numIters) ? CONTINUE : STOP;
}

ShaderPrecisionTests::ShaderPrecisionTests(Context &context)
    : TestCaseGroup(context, "precision", "Shader precision requirements validation tests")
{
}

ShaderPrecisionTests::~ShaderPrecisionTests(void)
{
}

void ShaderPrecisionTests::init(void)
{
    using tcu::add;
    using tcu::div;
    using tcu::IVec2;
    using tcu::mul;
    using tcu::sub;
    using tcu::UVec2;
    using tcu::Vec2;

    // Exp = Emax-2, Mantissa = 0
    float minF32 = tcu::Float32((1u << 31) | (0xfdu << 23) | 0x0u).asFloat();
    float maxF32 = tcu::Float32((0u << 31) | (0xfdu << 23) | 0x0u).asFloat();
    float minF16 = tcu::Float16((uint16_t)((1u << 15) | (0x1du << 10) | 0x0u)).asFloat();
    float maxF16 = tcu::Float16((uint16_t)((0u << 15) | (0x1du << 10) | 0x0u)).asFloat();
    tcu::Vec2 fullRange32F(minF32, maxF32);
    tcu::Vec2 fullRange16F(minF16, maxF16);
    tcu::IVec2 fullRange32I(0x80000000, 0x7fffffff);
    tcu::IVec2 fullRange16I(-(1 << 15), (1 << 15) - 1);
    tcu::IVec2 fullRange8I(-(1 << 7), (1 << 7) - 1);
    tcu::UVec2 fullRange32U(0u, 0xffffffffu);
    tcu::UVec2 fullRange16U(0u, 0xffffu);
    tcu::UVec2 fullRange8U(0u, 0xffu);

    // \note Right now it is not programmatically verified that the results shouldn't end up being inf/nan but
    //       actual values used are ok.

    static const struct
    {
        const char *name;
        const char *op;
        ShaderFloatPrecisionCase::EvalFunc evalFunc;
        glu::Precision precision;
        tcu::Vec2 rangeA;
        tcu::Vec2 rangeB;
    } floatCases[] = {
        // Name                Op                Eval            Precision                RangeA                RangeB
        {"highp_add", "in0 + in1", add<double>, glu::PRECISION_HIGHP, fullRange32F, fullRange32F},
        {"highp_sub", "in0 - in1", sub<double>, glu::PRECISION_HIGHP, fullRange32F, fullRange32F},
        {"highp_mul", "in0 * in1", mul<double>, glu::PRECISION_HIGHP, Vec2(-1e5f, 1e5f), Vec2(-1e5f, 1e5f)},
        {"highp_div", "in0 / in1", div<double>, glu::PRECISION_HIGHP, Vec2(-1e5f, 1e5f), Vec2(-1e5f, 1e5f)},
        {"mediump_add", "in0 + in1", add<double>, glu::PRECISION_MEDIUMP, fullRange16F, fullRange16F},
        {"mediump_sub", "in0 - in1", sub<double>, glu::PRECISION_MEDIUMP, fullRange16F, fullRange16F},
        {"mediump_mul", "in0 * in1", mul<double>, glu::PRECISION_MEDIUMP, Vec2(-1e2f, 1e2f), Vec2(-1e2f, 1e2f)},
        {"mediump_div", "in0 / in1", div<double>, glu::PRECISION_MEDIUMP, Vec2(-1e2f, 1e2f), Vec2(-1e2f, 1e2f)}};

    static const struct
    {
        const char *name;
        const char *op;
        ShaderIntPrecisionCase::EvalFunc evalFunc;
        glu::Precision precision;
        int bits;
        tcu::IVec2 rangeA;
        tcu::IVec2 rangeB;
    } intCases[] = {
        // Name                Op                Eval                Precision                Bits    RangeA            RangeB
        {"highp_add", "in0 + in1", add<int64_t>, glu::PRECISION_HIGHP, 32, fullRange32I, fullRange32I},
        {"highp_sub", "in0 - in1", sub<int64_t>, glu::PRECISION_HIGHP, 32, fullRange32I, fullRange32I},
        {"highp_mul", "in0 * in1", mul<int64_t>, glu::PRECISION_HIGHP, 32, fullRange32I, fullRange32I},
        {"highp_div", "in0 / in1", div<int64_t>, glu::PRECISION_HIGHP, 32, fullRange32I, IVec2(-10000, -1)},
        {"mediump_add", "in0 + in1", add<int64_t>, glu::PRECISION_MEDIUMP, 16, fullRange16I, fullRange16I},
        {"mediump_sub", "in0 - in1", sub<int64_t>, glu::PRECISION_MEDIUMP, 16, fullRange16I, fullRange16I},
        {"mediump_mul", "in0 * in1", mul<int64_t>, glu::PRECISION_MEDIUMP, 16, fullRange16I, fullRange16I},
        {"mediump_div", "in0 / in1", div<int64_t>, glu::PRECISION_MEDIUMP, 16, fullRange16I, IVec2(1, 1000)},
        {"lowp_add", "in0 + in1", add<int64_t>, glu::PRECISION_LOWP, 8, fullRange8I, fullRange8I},
        {"lowp_sub", "in0 - in1", sub<int64_t>, glu::PRECISION_LOWP, 8, fullRange8I, fullRange8I},
        {"lowp_mul", "in0 * in1", mul<int64_t>, glu::PRECISION_LOWP, 8, fullRange8I, fullRange8I},
        {"lowp_div", "in0 / in1", div<int64_t>, glu::PRECISION_LOWP, 8, fullRange8I, IVec2(-50, -1)}};

    static const struct
    {
        const char *name;
        const char *op;
        ShaderUintPrecisionCase::EvalFunc evalFunc;
        glu::Precision precision;
        int bits;
        tcu::UVec2 rangeA;
        tcu::UVec2 rangeB;
    } uintCases[] = {
        // Name                Op                Eval                Precision                Bits    RangeA            RangeB
        {"highp_add", "in0 + in1", add<uint32_t>, glu::PRECISION_HIGHP, 32, fullRange32U, fullRange32U},
        {"highp_sub", "in0 - in1", sub<uint32_t>, glu::PRECISION_HIGHP, 32, fullRange32U, fullRange32U},
        {"highp_mul", "in0 * in1", mul<uint32_t>, glu::PRECISION_HIGHP, 32, fullRange32U, fullRange32U},
        {"highp_div", "in0 / in1", div<uint32_t>, glu::PRECISION_HIGHP, 32, fullRange32U, UVec2(1u, 10000u)},
        {"mediump_add", "in0 + in1", add<uint32_t>, glu::PRECISION_MEDIUMP, 16, fullRange16U, fullRange16U},
        {"mediump_sub", "in0 - in1", sub<uint32_t>, glu::PRECISION_MEDIUMP, 16, fullRange16U, fullRange16U},
        {"mediump_mul", "in0 * in1", mul<uint32_t>, glu::PRECISION_MEDIUMP, 16, fullRange16U, fullRange16U},
        {"mediump_div", "in0 / in1", div<uint32_t>, glu::PRECISION_MEDIUMP, 16, fullRange16U, UVec2(1, 1000u)},
        {"lowp_add", "in0 + in1", add<uint32_t>, glu::PRECISION_LOWP, 8, fullRange8U, fullRange8U},
        {"lowp_sub", "in0 - in1", sub<uint32_t>, glu::PRECISION_LOWP, 8, fullRange8U, fullRange8U},
        {"lowp_mul", "in0 * in1", mul<uint32_t>, glu::PRECISION_LOWP, 8, fullRange8U, fullRange8U},
        {"lowp_div", "in0 / in1", div<uint32_t>, glu::PRECISION_LOWP, 8, fullRange8U, UVec2(1, 50u)}};

    tcu::TestCaseGroup *floatGroup = new tcu::TestCaseGroup(m_testCtx, "float", "Floating-point precision tests");
    addChild(floatGroup);
    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(floatCases); ndx++)
    {
        floatGroup->addChild(new ShaderFloatPrecisionCase(
            m_context, (string(floatCases[ndx].name) + "_vertex").c_str(), "", floatCases[ndx].op,
            floatCases[ndx].evalFunc, floatCases[ndx].precision, floatCases[ndx].rangeA, floatCases[ndx].rangeB, true));
        floatGroup->addChild(
            new ShaderFloatPrecisionCase(m_context, (string(floatCases[ndx].name) + "_fragment").c_str(), "",
                                         floatCases[ndx].op, floatCases[ndx].evalFunc, floatCases[ndx].precision,
                                         floatCases[ndx].rangeA, floatCases[ndx].rangeB, false));
    }

    tcu::TestCaseGroup *intGroup = new tcu::TestCaseGroup(m_testCtx, "int", "Integer precision tests");
    addChild(intGroup);
    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(intCases); ndx++)
    {
        intGroup->addChild(new ShaderIntPrecisionCase(
            m_context, (string(intCases[ndx].name) + "_vertex").c_str(), "", intCases[ndx].op, intCases[ndx].evalFunc,
            intCases[ndx].precision, intCases[ndx].bits, intCases[ndx].rangeA, intCases[ndx].rangeB, true));
        intGroup->addChild(new ShaderIntPrecisionCase(
            m_context, (string(intCases[ndx].name) + "_fragment").c_str(), "", intCases[ndx].op, intCases[ndx].evalFunc,
            intCases[ndx].precision, intCases[ndx].bits, intCases[ndx].rangeA, intCases[ndx].rangeB, false));
    }

    tcu::TestCaseGroup *uintGroup = new tcu::TestCaseGroup(m_testCtx, "uint", "Unsigned integer precision tests");
    addChild(uintGroup);
    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(uintCases); ndx++)
    {
        uintGroup->addChild(new ShaderUintPrecisionCase(m_context, (string(uintCases[ndx].name) + "_vertex").c_str(),
                                                        "", uintCases[ndx].op, uintCases[ndx].evalFunc,
                                                        uintCases[ndx].precision, uintCases[ndx].bits,
                                                        uintCases[ndx].rangeA, uintCases[ndx].rangeB, true));
        uintGroup->addChild(new ShaderUintPrecisionCase(m_context, (string(uintCases[ndx].name) + "_fragment").c_str(),
                                                        "", uintCases[ndx].op, uintCases[ndx].evalFunc,
                                                        uintCases[ndx].precision, uintCases[ndx].bits,
                                                        uintCases[ndx].rangeA, uintCases[ndx].rangeB, false));
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
