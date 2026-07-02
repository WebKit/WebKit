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
 * \brief Depth tests.
 *//*--------------------------------------------------------------------*/

#include "es2fDepthTests.hpp"

#include "tcuTestLog.hpp"
#include "gluPixelTransfer.hpp"
#include "tcuImageCompare.hpp"
#include "tcuRenderTarget.hpp"
#include "gluStrUtil.hpp"

#include "sglrContextUtil.hpp"
#include "sglrReferenceContext.hpp"
#include "sglrGLContext.hpp"

#include "deRandom.hpp"

#include "glwEnums.hpp"

using tcu::RGBA;

namespace deqp
{
namespace gles2
{
namespace Functional
{

class DepthShader : public sglr::ShaderProgram
{
public:
    DepthShader(void);

    void setColor(sglr::Context &ctx, uint32_t programID, const tcu::Vec4 &color);

private:
    void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets, const int numPackets) const;
    void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                        const rr::FragmentShadingContext &context) const;

    const sglr::UniformSlot &u_color;
};

DepthShader::DepthShader(void)
    : sglr::ShaderProgram(sglr::pdec::ShaderProgramDeclaration()
                          << sglr::pdec::VertexAttribute("a_position", rr::GENERICVECTYPE_FLOAT)
                          << sglr::pdec::FragmentOutput(rr::GENERICVECTYPE_FLOAT)
                          << sglr::pdec::Uniform("u_color", glu::TYPE_FLOAT_VEC4)
                          << sglr::pdec::VertexSource("attribute highp vec4 a_position;\n"
                                                      "void main (void)\n"
                                                      "{\n"
                                                      "    gl_Position = a_position;\n"
                                                      "}\n")
                          << sglr::pdec::FragmentSource("uniform mediump vec4 u_color;\n"
                                                        "void main (void)\n"
                                                        "{\n"
                                                        "    gl_FragColor = u_color;\n"
                                                        "}\n"))
    , u_color(getUniformByName("u_color"))
{
}

void DepthShader::setColor(sglr::Context &ctx, uint32_t programID, const tcu::Vec4 &color)
{
    ctx.useProgram(programID);
    ctx.uniform4fv(ctx.getUniformLocation(programID, "u_color"), 1, color.getPtr());
}

void DepthShader::shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets,
                                const int numPackets) const
{
    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
        packets[packetNdx]->position =
            rr::readVertexAttribFloat(inputs[0], packets[packetNdx]->instanceNdx, packets[packetNdx]->vertexNdx);
}

void DepthShader::shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                                 const rr::FragmentShadingContext &context) const
{
    const tcu::Vec4 color(u_color.value.f4);

    DE_UNREF(packets);

    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
        for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
            rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
}

// \todo [2011-07-11 pyry] This code is duplicated in a quite many places. Move it to some utility?
class DepthCase : public TestCase
{
public:
    DepthCase(Context &context, const char *name, const char *description);
    virtual ~DepthCase(void)
    {
    }

    virtual IterateResult iterate(void);
    virtual void render(sglr::Context &context) = 0;
};

DepthCase::DepthCase(Context &context, const char *name, const char *description) : TestCase(context, name, description)
{
}

TestCase::IterateResult DepthCase::iterate(void)
{
    tcu::Vec4 clearColor                  = tcu::Vec4(0.125f, 0.25f, 0.5f, 1.0f);
    glu::RenderContext &renderCtx         = m_context.getRenderContext();
    const tcu::RenderTarget &renderTarget = renderCtx.getRenderTarget();
    tcu::TestLog &log                     = m_testCtx.getLog();
    const char *failReason                = nullptr;

    // Position & size for context
    de::Random rnd(deStringHash(getName()));

    int width  = deMin32(renderTarget.getWidth(), 128);
    int height = deMin32(renderTarget.getHeight(), 128);
    int x      = rnd.getInt(0, renderTarget.getWidth() - width);
    int y      = rnd.getInt(0, renderTarget.getHeight() - height);

    tcu::Surface gles2Frame(width, height);
    tcu::Surface refFrame(width, height);
    uint32_t gles2Error;
    uint32_t refError;

    // Render using GLES2
    {
        sglr::GLContext context(renderCtx, log, sglr::GLCONTEXT_LOG_CALLS, tcu::IVec4(x, y, width, height));

        context.clearColor(clearColor.x(), clearColor.y(), clearColor.z(), clearColor.w());
        context.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        render(context); // Call actual render func
        context.readPixels(gles2Frame, 0, 0, width, height);
        gles2Error = context.getError();
    }

    // Render reference image
    {
        sglr::ReferenceContextBuffers buffers(
            tcu::PixelFormat(8, 8, 8, renderTarget.getPixelFormat().alphaBits ? 8 : 0), renderTarget.getDepthBits(),
            renderTarget.getStencilBits(), width, height);
        sglr::ReferenceContext context(sglr::ReferenceContextLimits(renderCtx), buffers.getColorbuffer(),
                                       buffers.getDepthbuffer(), buffers.getStencilbuffer());

        context.clearColor(clearColor.x(), clearColor.y(), clearColor.z(), clearColor.w());
        context.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        render(context);
        context.readPixels(refFrame, 0, 0, width, height);
        refError = context.getError();
    }

    // Compare error codes
    bool errorCodesOk = (gles2Error == refError);

    if (!errorCodesOk)
    {
        log << tcu::TestLog::Message << "Error code mismatch: got " << glu::getErrorStr(gles2Error) << ", expected "
            << glu::getErrorStr(refError) << tcu::TestLog::EndMessage;
        failReason = "Got unexpected error";
    }

    // Compare images
    const float threshold = 0.02f;
    bool imagesOk         = tcu::fuzzyCompare(log, "ComparisonResult", "Image comparison result", refFrame, gles2Frame,
                                              threshold, tcu::COMPARE_LOG_RESULT);

    if (!imagesOk && !failReason)
        failReason = "Image comparison failed";

    // Store test result
    bool isOk = errorCodesOk && imagesOk;
    m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL, isOk ? "Pass" : failReason);

    return STOP;
}

class DepthCompareCase : public DepthCase
{
public:
    DepthCompareCase(Context &context, const char *name, const char *description, uint32_t compareOp)
        : DepthCase(context, name, description)
        , m_compareOp(compareOp)
    {
    }

    void render(sglr::Context &context)
    {
        using tcu::Vec3;

        DepthShader shader;
        uint32_t shaderID = context.createProgram(&shader);

        tcu::Vec4 red(1.0f, 0.0f, 0.0f, 1.0);
        tcu::Vec4 green(0.0f, 1.0f, 0.0f, 1.0f);

        // Clear depth to 1
        context.clearDepthf(1.0f);
        context.clear(GL_DEPTH_BUFFER_BIT);

        // Enable depth test.
        context.enable(GL_DEPTH_TEST);

        // Upper left: two quads with same depth
        context.depthFunc(GL_ALWAYS);
        shader.setColor(context, shaderID, red);
        sglr::drawQuad(context, shaderID, Vec3(-1.0f, -1.0f, 0.2f), Vec3(0.0f, 0.0f, 0.2f));
        context.depthFunc(m_compareOp);
        shader.setColor(context, shaderID, green);
        sglr::drawQuad(context, shaderID, Vec3(-1.0f, -1.0f, 0.2f), Vec3(0.0f, 0.0f, 0.2f));

        // Lower left: two quads, d1 < d2
        context.depthFunc(GL_ALWAYS);
        shader.setColor(context, shaderID, red);
        sglr::drawQuad(context, shaderID, Vec3(-1.0f, 0.0f, -0.4f), Vec3(0.0f, 1.0f, -0.4f));
        context.depthFunc(m_compareOp);
        shader.setColor(context, shaderID, green);
        sglr::drawQuad(context, shaderID, Vec3(-1.0f, 0.0f, -0.1f), Vec3(0.0f, 1.0f, -0.1f));

        // Upper right: two quads, d1 > d2
        context.depthFunc(GL_ALWAYS);
        shader.setColor(context, shaderID, red);
        sglr::drawQuad(context, shaderID, Vec3(0.0f, -1.0f, 0.5f), Vec3(1.0f, 0.0f, 0.5f));
        context.depthFunc(m_compareOp);
        shader.setColor(context, shaderID, green);
        sglr::drawQuad(context, shaderID, Vec3(0.0f, -1.0f, 0.3f), Vec3(1.0f, 0.0f, 0.3f));

        // Lower right: two quads, d1 = 0, d2 = [-1..1]
        context.depthFunc(GL_ALWAYS);
        shader.setColor(context, shaderID, red);
        sglr::drawQuad(context, shaderID, Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
        context.depthFunc(m_compareOp);
        shader.setColor(context, shaderID, green);
        sglr::drawQuad(context, shaderID, Vec3(0.0f, 0.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));
    }

private:
    uint32_t m_compareOp;
};

DepthTests::DepthTests(Context &context) : TestCaseGroup(context, "depth", "Depth Tests")
{
}

DepthTests::~DepthTests(void)
{
}

void DepthTests::init(void)
{
    addChild(new DepthCompareCase(m_context, "cmp_always", "Always pass depth test", GL_ALWAYS));
    addChild(new DepthCompareCase(m_context, "cmp_never", "Never pass depth test", GL_NEVER));
    addChild(new DepthCompareCase(m_context, "cmp_equal", "Depth compare: equal", GL_EQUAL));
    addChild(new DepthCompareCase(m_context, "cmp_not_equal", "Depth compare: not equal", GL_NOTEQUAL));
    addChild(new DepthCompareCase(m_context, "cmp_less_than", "Depth compare: less than", GL_LESS));
    addChild(new DepthCompareCase(m_context, "cmp_less_or_equal", "Depth compare: less than or equal", GL_LEQUAL));
    addChild(new DepthCompareCase(m_context, "cmp_greater_than", "Depth compare: greater than", GL_GREATER));
    addChild(
        new DepthCompareCase(m_context, "cmp_greater_or_equal", "Depth compare: greater than or equal", GL_GEQUAL));
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
