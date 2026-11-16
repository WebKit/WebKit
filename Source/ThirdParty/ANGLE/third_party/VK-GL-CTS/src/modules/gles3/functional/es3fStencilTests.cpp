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
 * \brief Stencil tests.
 *//*--------------------------------------------------------------------*/

#include "es3fStencilTests.hpp"

#include "tcuSurface.hpp"
#include "tcuVector.hpp"
#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuRenderTarget.hpp"

#include "sglrContextUtil.hpp"
#include "sglrGLContext.hpp"
#include "sglrReferenceContext.hpp"

#include "deRandom.hpp"
#include "deMath.h"
#include "deString.h"

#include <vector>

#include "glwEnums.hpp"
#include "glwDefs.hpp"

using std::vector;
using tcu::IVec2;
using tcu::IVec4;
using tcu::Vec3;
using namespace glw;

namespace deqp
{
namespace gles3
{
namespace Functional
{

class StencilShader : public sglr::ShaderProgram
{
public:
    StencilShader(void)
        : sglr::ShaderProgram(sglr::pdec::ShaderProgramDeclaration()
                              << sglr::pdec::VertexAttribute("a_position", rr::GENERICVECTYPE_FLOAT)
                              << sglr::pdec::VertexToFragmentVarying(rr::GENERICVECTYPE_FLOAT)
                              << sglr::pdec::FragmentOutput(rr::GENERICVECTYPE_FLOAT)
                              << sglr::pdec::Uniform("u_color", glu::TYPE_FLOAT_VEC4)
                              << sglr::pdec::VertexSource("#version 300 es\n"
                                                          "in highp vec4 a_position;\n"
                                                          "void main (void)\n"
                                                          "{\n"
                                                          "    gl_Position = a_position;\n"
                                                          "}\n")
                              << sglr::pdec::FragmentSource("#version 300 es\n"
                                                            "uniform highp vec4 u_color;\n"
                                                            "layout(location = 0) out mediump vec4 o_color;\n"
                                                            "void main (void)\n"
                                                            "{\n"
                                                            "    o_color = u_color;\n"
                                                            "}\n"))
        , u_color(getUniformByName("u_color"))
    {
    }

    void setColor(sglr::Context &ctx, uint32_t program, const tcu::Vec4 &color)
    {
        ctx.useProgram(program);
        ctx.uniform4fv(ctx.getUniformLocation(program, "u_color"), 1, color.getPtr());
    }

private:
    void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets, const int numPackets) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
        {
            rr::VertexPacket &packet = *packets[packetNdx];

            packet.position = rr::readVertexAttribFloat(inputs[0], packet.instanceNdx, packet.vertexNdx);
        }
    }

    void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                        const rr::FragmentShadingContext &context) const
    {
        const tcu::Vec4 color(u_color.value.f4);

        DE_UNREF(packets);

        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
            for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
    }

    const sglr::UniformSlot &u_color;
};

class StencilOp
{
public:
    enum Type
    {
        TYPE_CLEAR_STENCIL = 0,
        TYPE_CLEAR_DEPTH,
        TYPE_QUAD,

        TYPE_LAST
    };

    Type type;
    GLenum stencilTest;
    int stencil; //!< Ref for quad op, clear value for clears
    uint32_t stencilMask;
    GLenum depthTest;
    float depth; //!< Quad depth or clear value
    GLenum sFail;
    GLenum dFail;
    GLenum dPass;

    StencilOp(Type type_, GLenum stencilTest_ = GL_ALWAYS, int stencil_ = 0, GLenum depthTest_ = GL_ALWAYS,
              float depth_ = 1.0f, GLenum sFail_ = GL_KEEP, GLenum dFail_ = GL_KEEP, GLenum dPass_ = GL_KEEP)
        : type(type_)
        , stencilTest(stencilTest_)
        , stencil(stencil_)
        , stencilMask(0xffffffffu)
        , depthTest(depthTest_)
        , depth(depth_)
        , sFail(sFail_)
        , dFail(dFail_)
        , dPass(dPass_)
    {
    }

    static StencilOp clearStencil(int stencil)
    {
        StencilOp op(TYPE_CLEAR_STENCIL);
        op.stencil = stencil;
        return op;
    }

    static StencilOp clearDepth(float depth)
    {
        StencilOp op(TYPE_CLEAR_DEPTH);
        op.depth = depth;
        return op;
    }

    static StencilOp quad(GLenum stencilTest, int stencil, GLenum depthTest, float depth, GLenum sFail, GLenum dFail,
                          GLenum dPass)
    {
        return StencilOp(TYPE_QUAD, stencilTest, stencil, depthTest, depth, sFail, dFail, dPass);
    }
};

class StencilCase : public TestCase
{
public:
    StencilCase(Context &context, const char *name, const char *description);
    virtual ~StencilCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

    virtual void genOps(vector<StencilOp> &dst, int stencilBits, int depthBits, int targetStencil) = 0;

private:
    void executeOps(sglr::Context &context, const IVec4 &cell, const vector<StencilOp> &ops);
    void visualizeStencil(sglr::Context &context, int stencilBits, int stencilStep);

    StencilShader m_shader;
    uint32_t m_shaderID;
};

StencilCase::StencilCase(Context &context, const char *name, const char *description)
    : TestCase(context, name, description)
    , m_shaderID(0)
{
}

StencilCase::~StencilCase(void)
{
}

void StencilCase::init(void)
{
}

void StencilCase::deinit(void)
{
}

void StencilCase::executeOps(sglr::Context &context, const IVec4 &cell, const vector<StencilOp> &ops)
{
    // For quadOps
    float x0 = 2.0f * ((float)cell.x() / (float)context.getWidth()) - 1.0f;
    float y0 = 2.0f * ((float)cell.y() / (float)context.getHeight()) - 1.0f;
    float x1 = x0 + 2.0f * ((float)cell.z() / (float)context.getWidth());
    float y1 = y0 + 2.0f * ((float)cell.w() / (float)context.getHeight());

    m_shader.setColor(context, m_shaderID, tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    for (vector<StencilOp>::const_iterator i = ops.begin(); i != ops.end(); i++)
    {
        const StencilOp &op = *i;

        switch (op.type)
        {
        case StencilOp::TYPE_CLEAR_DEPTH:
            context.enable(GL_SCISSOR_TEST);
            context.scissor(cell.x(), cell.y(), cell.z(), cell.w());
            context.clearDepthf(op.depth);
            context.clear(GL_DEPTH_BUFFER_BIT);
            context.disable(GL_SCISSOR_TEST);
            break;

        case StencilOp::TYPE_CLEAR_STENCIL:
            context.enable(GL_SCISSOR_TEST);
            context.scissor(cell.x(), cell.y(), cell.z(), cell.w());
            context.clearStencil(op.stencil);
            context.clear(GL_STENCIL_BUFFER_BIT);
            context.disable(GL_SCISSOR_TEST);
            break;

        case StencilOp::TYPE_QUAD:
            context.enable(GL_DEPTH_TEST);
            context.enable(GL_STENCIL_TEST);
            context.depthFunc(op.depthTest);
            context.stencilFunc(op.stencilTest, op.stencil, op.stencilMask);
            context.stencilOp(op.sFail, op.dFail, op.dPass);
            sglr::drawQuad(context, m_shaderID, Vec3(x0, y0, op.depth), Vec3(x1, y1, op.depth));
            context.disable(GL_STENCIL_TEST);
            context.disable(GL_DEPTH_TEST);
            break;

        default:
            DE_ASSERT(false);
        }
    }
}

void StencilCase::visualizeStencil(sglr::Context &context, int stencilBits, int stencilStep)
{
    int endVal           = 1 << stencilBits;
    int numStencilValues = endVal / stencilStep + 1;

    context.enable(GL_STENCIL_TEST);
    context.stencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    for (int ndx = 0; ndx < numStencilValues; ndx++)
    {
        int value      = deMin32(ndx * stencilStep, endVal - 1);
        float colorMix = (float)value / (float)de::max(1, endVal - 1);
        tcu::Vec4 color(0.0f, 1.0f - colorMix, colorMix, 1.0f);

        m_shader.setColor(context, m_shaderID, color);
        context.stencilFunc(GL_EQUAL, value, 0xffffffffu);
        sglr::drawQuad(context, m_shaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(+1.0f, +1.0f, 0.0f));
    }
}

TestCase::IterateResult StencilCase::iterate(void)
{
    const tcu::RenderTarget &renderTarget = m_context.getRenderContext().getRenderTarget();
    int depthBits                         = renderTarget.getDepthBits();
    int stencilBits                       = renderTarget.getStencilBits();

    int stencilStep      = stencilBits == 8 ? 8 : 1;
    int numStencilValues = (1 << stencilBits) / stencilStep + 1;

    int gridSize = (int)deFloatCeil(deFloatSqrt((float)(numStencilValues + 2)));

    int width  = deMin32(128, renderTarget.getWidth());
    int height = deMin32(128, renderTarget.getHeight());

    tcu::TestLog &log = m_testCtx.getLog();
    de::Random rnd(deStringHash(m_name.c_str()));
    int viewportX  = rnd.getInt(0, renderTarget.getWidth() - width);
    int viewportY  = rnd.getInt(0, renderTarget.getHeight() - height);
    IVec4 viewport = IVec4(viewportX, viewportY, width, height);

    tcu::Surface gles2Frame(width, height);
    tcu::Surface refFrame(width, height);
    GLenum gles2Error;

    const char *failReason = nullptr;

    // Get ops for stencil values
    vector<vector<StencilOp>> ops(numStencilValues + 2);
    {
        // Values from 0 to max
        for (int ndx = 0; ndx < numStencilValues; ndx++)
            genOps(ops[ndx], stencilBits, depthBits, deMin32(ndx * stencilStep, (1 << stencilBits) - 1));

        // -1 and max+1
        genOps(ops[numStencilValues + 0], stencilBits, depthBits, 1 << stencilBits);
        genOps(ops[numStencilValues + 1], stencilBits, depthBits, -1);
    }

    // Compute cells: (x, y, w, h)
    vector<IVec4> cells;
    int cellWidth  = width / gridSize;
    int cellHeight = height / gridSize;
    for (int y = 0; y < gridSize; y++)
        for (int x = 0; x < gridSize; x++)
            cells.push_back(IVec4(x * cellWidth, y * cellHeight, cellWidth, cellHeight));

    DE_ASSERT(ops.size() <= cells.size());

    // Execute for gles3 context
    {
        sglr::GLContext context(m_context.getRenderContext(), log, 0 /* don't log calls or program */, viewport);

        m_shaderID = context.createProgram(&m_shader);

        context.clearColor(1.0f, 0.0f, 0.0f, 1.0f);
        context.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        for (int ndx = 0; ndx < (int)ops.size(); ndx++)
            executeOps(context, cells[ndx], ops[ndx]);

        visualizeStencil(context, stencilBits, stencilStep);

        gles2Error = context.getError();
        context.readPixels(gles2Frame, 0, 0, width, height);
    }

    // Execute for reference context
    {
        sglr::ReferenceContextBuffers buffers(
            tcu::PixelFormat(8, 8, 8, renderTarget.getPixelFormat().alphaBits ? 8 : 0), renderTarget.getDepthBits(),
            renderTarget.getStencilBits(), width, height);
        sglr::ReferenceContext context(sglr::ReferenceContextLimits(m_context.getRenderContext()),
                                       buffers.getColorbuffer(), buffers.getDepthbuffer(), buffers.getStencilbuffer());

        m_shaderID = context.createProgram(&m_shader);

        context.clearColor(1.0f, 0.0f, 0.0f, 1.0f);
        context.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        for (int ndx = 0; ndx < (int)ops.size(); ndx++)
            executeOps(context, cells[ndx], ops[ndx]);

        visualizeStencil(context, stencilBits, stencilStep);

        context.readPixels(refFrame, 0, 0, width, height);
    }

    // Check error
    bool errorCodeOk = (gles2Error == GL_NO_ERROR);
    if (!errorCodeOk && !failReason)
        failReason = "Got unexpected error";

    // Compare images
    const float threshold = 0.02f;
    bool imagesOk         = tcu::fuzzyCompare(log, "ComparisonResult", "Image comparison result", refFrame, gles2Frame,
                                              threshold, tcu::COMPARE_LOG_RESULT);

    if (!imagesOk && !failReason)
        failReason = "Image comparison failed";

    // Store test result
    bool isOk = errorCodeOk && imagesOk;
    m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL, isOk ? "Pass" : failReason);

    return STOP;
}

StencilTests::StencilTests(Context &context) : TestCaseGroup(context, "stencil", "Stencil Tests")
{
}

StencilTests::~StencilTests(void)
{
}

typedef void (*GenStencilOpsFunc)(vector<StencilOp> &dst, int stencilBits, int depthBits, int targetStencil);

class SimpleStencilCase : public StencilCase
{
public:
    SimpleStencilCase(Context &context, const char *name, const char *description, GenStencilOpsFunc genOpsFunc)
        : StencilCase(context, name, description)
        , m_genOps(genOpsFunc)
    {
    }

    void genOps(vector<StencilOp> &dst, int stencilBits, int depthBits, int targetStencil)
    {
        m_genOps(dst, stencilBits, depthBits, targetStencil);
    }

private:
    GenStencilOpsFunc m_genOps;
};

void StencilTests::init(void)
{
#define STENCIL_CASE(NAME, DESCRIPTION, GEN_OPS_BODY)                                                     \
    do                                                                                                    \
    {                                                                                                     \
        struct Gen_##NAME                                                                                 \
        {                                                                                                 \
            static void genOps(vector<StencilOp> &dst, int stencilBits, int depthBits, int targetStencil) \
            {                                                                                             \
                DE_UNREF(stencilBits &&depthBits);                                                        \
                GEN_OPS_BODY                                                                              \
            }                                                                                             \
        };                                                                                                \
        addChild(new SimpleStencilCase(m_context, #NAME, DESCRIPTION, Gen_##NAME::genOps));               \
    } while (false);

    STENCIL_CASE(clear, "Stencil clear", {
        // \note Unused bits are set to 1, clear should mask them out
        int mask = (1 << stencilBits) - 1;
        dst.push_back(StencilOp::clearStencil(targetStencil | ~mask));
    })

    // Replace in different points
    STENCIL_CASE(stencil_fail_replace, "Set stencil on stencil fail", {
        dst.push_back(StencilOp::quad(GL_NEVER, targetStencil, GL_ALWAYS, 0.0f, GL_REPLACE, GL_KEEP, GL_KEEP));
    })
    STENCIL_CASE(depth_fail_replace, "Set stencil on depth fail", {
        dst.push_back(StencilOp::clearDepth(0.0f));
        dst.push_back(StencilOp::quad(GL_ALWAYS, targetStencil, GL_LESS, 0.5f, GL_KEEP, GL_REPLACE, GL_KEEP));
    })
    STENCIL_CASE(depth_pass_replace, "Set stencil on depth pass", {
        dst.push_back(StencilOp::quad(GL_ALWAYS, targetStencil, GL_LESS, 0.0f, GL_KEEP, GL_KEEP, GL_REPLACE));
    })

    // Increment, decrement
    STENCIL_CASE(incr_stencil_fail, "Increment on stencil fail", {
        if (targetStencil > 0)
        {
            dst.push_back(StencilOp::clearStencil(targetStencil - 1));
            dst.push_back(StencilOp::quad(GL_EQUAL, targetStencil, GL_ALWAYS, 0.0f, GL_INCR, GL_KEEP, GL_KEEP));
        }
        else
            dst.push_back(StencilOp::clearStencil(targetStencil));
    })
    STENCIL_CASE(decr_stencil_fail, "Decrement on stencil fail", {
        int maxStencil = (1 << stencilBits) - 1;
        if (targetStencil < maxStencil)
        {
            dst.push_back(StencilOp::clearStencil(targetStencil + 1));
            dst.push_back(StencilOp::quad(GL_EQUAL, targetStencil, GL_ALWAYS, 0.0f, GL_DECR, GL_KEEP, GL_KEEP));
        }
        else
            dst.push_back(StencilOp::clearStencil(targetStencil));
    })
    STENCIL_CASE(incr_wrap_stencil_fail, "Increment (wrap) on stencil fail", {
        int maxStencil = (1 << stencilBits) - 1;
        dst.push_back(StencilOp::clearStencil((targetStencil - 1) & maxStencil));
        dst.push_back(StencilOp::quad(GL_EQUAL, targetStencil, GL_ALWAYS, 0.0f, GL_INCR_WRAP, GL_KEEP, GL_KEEP));
    })
    STENCIL_CASE(decr_wrap_stencil_fail, "Decrement (wrap) on stencil fail", {
        int maxStencil = (1 << stencilBits) - 1;
        dst.push_back(StencilOp::clearStencil((targetStencil + 1) & maxStencil));
        dst.push_back(StencilOp::quad(GL_EQUAL, targetStencil, GL_ALWAYS, 0.0f, GL_DECR_WRAP, GL_KEEP, GL_KEEP));
    })

    // Zero, Invert
    STENCIL_CASE(zero_stencil_fail, "Zero on stencil fail", {
        dst.push_back(StencilOp::clearStencil(targetStencil));
        dst.push_back(StencilOp::quad(GL_NOTEQUAL, targetStencil, GL_ALWAYS, 0.0f, GL_ZERO, GL_KEEP, GL_KEEP));
        dst.push_back(StencilOp::quad(GL_EQUAL, targetStencil, GL_ALWAYS, 0.0f, GL_REPLACE, GL_KEEP, GL_KEEP));
    })
    STENCIL_CASE(invert_stencil_fail, "Invert on stencil fail", {
        int mask = (1 << stencilBits) - 1;
        dst.push_back(StencilOp::clearStencil((~targetStencil) & mask));
        dst.push_back(StencilOp::quad(GL_EQUAL, targetStencil, GL_ALWAYS, 0.0f, GL_INVERT, GL_KEEP, GL_KEEP));
    })

    // Comparison modes
    STENCIL_CASE(cmp_equal, "Equality comparison", {
        int mask = (1 << stencilBits) - 1;
        int inv  = (~targetStencil) & mask;
        dst.push_back(StencilOp::clearStencil(inv));
        dst.push_back(StencilOp::quad(GL_EQUAL, inv, GL_ALWAYS, 0.0f, GL_KEEP, GL_KEEP, GL_INVERT));
        dst.push_back(StencilOp::quad(GL_EQUAL, inv, GL_ALWAYS, 0.0f, GL_KEEP, GL_KEEP, GL_INVERT));
    })
    STENCIL_CASE(cmp_not_equal, "Equality comparison", {
        int mask = (1 << stencilBits) - 1;
        int inv  = (~targetStencil) & mask;
        dst.push_back(StencilOp::clearStencil(inv));
        dst.push_back(StencilOp::quad(GL_NOTEQUAL, targetStencil, GL_ALWAYS, 0.0f, GL_KEEP, GL_KEEP, GL_INVERT));
        dst.push_back(StencilOp::quad(GL_NOTEQUAL, targetStencil, GL_ALWAYS, 0.0f, GL_KEEP, GL_KEEP, GL_INVERT));
    })
    STENCIL_CASE(cmp_less_than, "Less than comparison", {
        int maxStencil = (1 << stencilBits) - 1;
        if (targetStencil < maxStencil)
        {
            dst.push_back(StencilOp::clearStencil(targetStencil + 1));
            dst.push_back(StencilOp::quad(GL_LESS, targetStencil, GL_ALWAYS, 0.0f, GL_KEEP, GL_KEEP, GL_DECR));
            dst.push_back(StencilOp::quad(GL_LESS, targetStencil, GL_ALWAYS, 0.0f, GL_KEEP, GL_KEEP, GL_DECR));
        }
        else
            dst.push_back(StencilOp::clearStencil(targetStencil));
    })
    STENCIL_CASE(cmp_less_or_equal, "Less or equal comparison", {
        int maxStencil = (1 << stencilBits) - 1;
        if (targetStencil < maxStencil)
        {
            dst.push_back(StencilOp::clearStencil(targetStencil + 1));
            dst.push_back(StencilOp::quad(GL_LEQUAL, targetStencil + 1, GL_ALWAYS, 0.0f, GL_KEEP, GL_KEEP, GL_DECR));
            dst.push_back(StencilOp::quad(GL_LEQUAL, targetStencil + 1, GL_ALWAYS, 0.0f, GL_KEEP, GL_KEEP, GL_DECR));
        }
        else
            dst.push_back(StencilOp::clearStencil(targetStencil));
    })
    STENCIL_CASE(cmp_greater_than, "Greater than comparison", {
        if (targetStencil > 0)
        {
            dst.push_back(StencilOp::clearStencil(targetStencil - 1));
            dst.push_back(StencilOp::quad(GL_GREATER, targetStencil, GL_ALWAYS, 0.0f, GL_KEEP, GL_KEEP, GL_INCR));
            dst.push_back(StencilOp::quad(GL_GREATER, targetStencil, GL_ALWAYS, 0.0f, GL_KEEP, GL_KEEP, GL_INCR));
        }
        else
            dst.push_back(StencilOp::clearStencil(targetStencil));
    })
    STENCIL_CASE(cmp_greater_or_equal, "Greater or equal comparison", {
        if (targetStencil > 0)
        {
            dst.push_back(StencilOp::clearStencil(targetStencil - 1));
            dst.push_back(StencilOp::quad(GL_GEQUAL, targetStencil - 1, GL_ALWAYS, 0.0f, GL_KEEP, GL_KEEP, GL_INCR));
            dst.push_back(StencilOp::quad(GL_GEQUAL, targetStencil - 1, GL_ALWAYS, 0.0f, GL_KEEP, GL_KEEP, GL_INCR));
        }
        else
            dst.push_back(StencilOp::clearStencil(targetStencil));
    })
    STENCIL_CASE(cmp_mask_equal, "Equality comparison with mask", {
        int valMask = (1 << stencilBits) - 1;
        int mask    = (1 << 7) | (1 << 5) | (1 << 3) | (1 << 1);
        dst.push_back(StencilOp::clearStencil(~targetStencil));
        StencilOp op =
            StencilOp::quad(GL_EQUAL, (~targetStencil | ~mask) & valMask, GL_ALWAYS, 0.0f, GL_KEEP, GL_KEEP, GL_INVERT);
        op.stencilMask = mask;
        dst.push_back(op);
    })
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
