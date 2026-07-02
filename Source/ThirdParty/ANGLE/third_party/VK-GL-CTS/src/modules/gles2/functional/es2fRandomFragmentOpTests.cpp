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
 * \brief Randomized per-fragment operation tests.
 *//*--------------------------------------------------------------------*/

#include "es2fRandomFragmentOpTests.hpp"
#include "glsFragmentOpUtil.hpp"
#include "glsInteractionTestUtil.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuTestLog.hpp"
#include "tcuSurface.hpp"
#include "tcuCommandLine.hpp"
#include "tcuImageCompare.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluRenderContext.hpp"
#include "deStringUtil.hpp"
#include "deRandom.hpp"
#include "deMath.h"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"
#include "rrFragmentOperations.hpp"
#include "sglrReferenceUtils.hpp"

#include <algorithm>

namespace deqp
{
namespace gles2
{
namespace Functional
{

using std::vector;
using tcu::BVec4;
using tcu::IVec2;
using tcu::TestLog;
using tcu::Vec2;
using tcu::Vec4;

enum
{
    VIEWPORT_WIDTH          = 64,
    VIEWPORT_HEIGHT         = 64,
    NUM_CALLS_PER_ITERATION = 3,
    NUM_ITERATIONS_PER_CASE = 10
};

static const tcu::Vec4 CLEAR_COLOR(0.25f, 0.5f, 0.75f, 1.0f);
static const float CLEAR_DEPTH    = 1.0f;
static const int CLEAR_STENCIL    = 0;
static const bool ENABLE_CALL_LOG = true;

using namespace gls::FragmentOpUtil;
using namespace gls::InteractionTestUtil;

void translateStencilState(const StencilState &src, rr::StencilState &dst)
{
    dst.func      = sglr::rr_util::mapGLTestFunc(src.function);
    dst.ref       = src.reference;
    dst.compMask  = src.compareMask;
    dst.sFail     = sglr::rr_util::mapGLStencilOp(src.stencilFailOp);
    dst.dpFail    = sglr::rr_util::mapGLStencilOp(src.depthFailOp);
    dst.dpPass    = sglr::rr_util::mapGLStencilOp(src.depthPassOp);
    dst.writeMask = src.writeMask;
}

void translateBlendState(const BlendState &src, rr::BlendState &dst)
{
    dst.equation = sglr::rr_util::mapGLBlendEquation(src.equation);
    dst.srcFunc  = sglr::rr_util::mapGLBlendFunc(src.srcFunc);
    dst.dstFunc  = sglr::rr_util::mapGLBlendFunc(src.dstFunc);
}

void translateState(const RenderState &src, rr::FragmentOperationState &dst, const tcu::RenderTarget &renderTarget)
{
    bool hasDepth   = renderTarget.getDepthBits() > 0;
    bool hasStencil = renderTarget.getStencilBits() > 0;

    dst.scissorTestEnabled = src.scissorTestEnabled;
    dst.scissorRectangle   = src.scissorRectangle;
    dst.stencilTestEnabled = hasStencil && src.stencilTestEnabled;
    dst.depthTestEnabled   = hasDepth && src.depthTestEnabled;
    dst.blendMode          = src.blendEnabled ? rr::BLENDMODE_STANDARD : rr::BLENDMODE_NONE;
    dst.numStencilBits     = renderTarget.getStencilBits();

    dst.colorMask = src.colorMask;

    if (dst.depthTestEnabled)
    {
        dst.depthFunc = sglr::rr_util::mapGLTestFunc(src.depthFunc);
        dst.depthMask = src.depthWriteMask;
    }

    if (dst.stencilTestEnabled)
    {
        translateStencilState(src.stencil[rr::FACETYPE_BACK], dst.stencilStates[rr::FACETYPE_BACK]);
        translateStencilState(src.stencil[rr::FACETYPE_FRONT], dst.stencilStates[rr::FACETYPE_FRONT]);
    }

    if (src.blendEnabled)
    {
        translateBlendState(src.blendRGBState, dst.blendRGBState);
        translateBlendState(src.blendAState, dst.blendAState);
        dst.blendColor = tcu::clamp(src.blendColor, Vec4(0.0f), Vec4(1.0f));
    }
}

static void renderQuad(const glw::Functions &gl, gls::FragmentOpUtil::QuadRenderer &renderer,
                       const gls::FragmentOpUtil::IntegerQuad &quad, int baseX, int baseY)
{
    gls::FragmentOpUtil::Quad translated;

    std::copy(DE_ARRAY_BEGIN(quad.color), DE_ARRAY_END(quad.color), DE_ARRAY_BEGIN(translated.color));

    bool flipX    = quad.posB.x() < quad.posA.x();
    bool flipY    = quad.posB.y() < quad.posA.y();
    int viewportX = de::min(quad.posA.x(), quad.posB.x());
    int viewportY = de::min(quad.posA.y(), quad.posB.y());
    int viewportW = de::abs(quad.posA.x() - quad.posB.x()) + 1;
    int viewportH = de::abs(quad.posA.y() - quad.posB.y()) + 1;

    translated.posA = Vec2(flipX ? 1.0f : -1.0f, flipY ? 1.0f : -1.0f);
    translated.posB = Vec2(flipX ? -1.0f : 1.0f, flipY ? -1.0f : 1.0f);

    // \todo [2012-12-18 pyry] Pass in DepthRange parameters.
    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(quad.depth); ndx++)
        translated.depth[ndx] = quad.depth[ndx] * 2.0f - 1.0f;

    gl.viewport(baseX + viewportX, baseY + viewportY, viewportW, viewportH);
    renderer.render(translated);
}

static void setGLState(glu::CallLogWrapper &wrapper, const RenderState &state, int viewportX, int viewportY)
{
    if (state.scissorTestEnabled)
    {
        wrapper.glEnable(GL_SCISSOR_TEST);
        wrapper.glScissor(viewportX + state.scissorRectangle.left, viewportY + state.scissorRectangle.bottom,
                          state.scissorRectangle.width, state.scissorRectangle.height);
    }
    else
        wrapper.glDisable(GL_SCISSOR_TEST);

    if (state.stencilTestEnabled)
    {
        wrapper.glEnable(GL_STENCIL_TEST);

        for (int face = 0; face < rr::FACETYPE_LAST; face++)
        {
            uint32_t glFace             = face == rr::FACETYPE_BACK ? GL_BACK : GL_FRONT;
            const StencilState &sParams = state.stencil[face];

            wrapper.glStencilFuncSeparate(glFace, sParams.function, sParams.reference, sParams.compareMask);
            wrapper.glStencilOpSeparate(glFace, sParams.stencilFailOp, sParams.depthFailOp, sParams.depthPassOp);
            wrapper.glStencilMaskSeparate(glFace, sParams.writeMask);
        }
    }
    else
        wrapper.glDisable(GL_STENCIL_TEST);

    if (state.depthTestEnabled)
    {
        wrapper.glEnable(GL_DEPTH_TEST);
        wrapper.glDepthFunc(state.depthFunc);
        wrapper.glDepthMask(state.depthWriteMask ? GL_TRUE : GL_FALSE);
    }
    else
        wrapper.glDisable(GL_DEPTH_TEST);

    if (state.blendEnabled)
    {
        wrapper.glEnable(GL_BLEND);
        wrapper.glBlendEquationSeparate(state.blendRGBState.equation, state.blendAState.equation);
        wrapper.glBlendFuncSeparate(state.blendRGBState.srcFunc, state.blendRGBState.dstFunc, state.blendAState.srcFunc,
                                    state.blendAState.dstFunc);
        wrapper.glBlendColor(state.blendColor.x(), state.blendColor.y(), state.blendColor.z(), state.blendColor.w());
    }
    else
        wrapper.glDisable(GL_BLEND);

    if (state.ditherEnabled)
        wrapper.glEnable(GL_DITHER);
    else
        wrapper.glDisable(GL_DITHER);

    wrapper.glColorMask(state.colorMask[0] ? GL_TRUE : GL_FALSE, state.colorMask[1] ? GL_TRUE : GL_FALSE,
                        state.colorMask[2] ? GL_TRUE : GL_FALSE, state.colorMask[3] ? GL_TRUE : GL_FALSE);
}

class RandomFragmentOpCase : public TestCase
{
public:
    RandomFragmentOpCase(Context &context, const char *name, const char *desc, uint32_t seed);
    ~RandomFragmentOpCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    tcu::UVec4 getCompareThreshold(void) const;

    uint32_t m_seed;

    glu::CallLogWrapper m_callLogWrapper;

    gls::FragmentOpUtil::QuadRenderer *m_renderer;
    tcu::TextureLevel *m_refColorBuffer;
    tcu::TextureLevel *m_refDepthBuffer;
    tcu::TextureLevel *m_refStencilBuffer;
    gls::FragmentOpUtil::ReferenceQuadRenderer *m_refRenderer;

    int m_iterNdx;
};

RandomFragmentOpCase::RandomFragmentOpCase(Context &context, const char *name, const char *desc, uint32_t seed)
    : TestCase(context, name, desc)
    , m_seed(seed)
    , m_callLogWrapper(context.getRenderContext().getFunctions(), context.getTestContext().getLog())
    , m_renderer(nullptr)
    , m_refColorBuffer(nullptr)
    , m_refDepthBuffer(nullptr)
    , m_refStencilBuffer(nullptr)
    , m_refRenderer(nullptr)
    , m_iterNdx(0)
{
    m_callLogWrapper.enableLogging(ENABLE_CALL_LOG);
}

RandomFragmentOpCase::~RandomFragmentOpCase(void)
{
    delete m_renderer;
    delete m_refColorBuffer;
    delete m_refDepthBuffer;
    delete m_refStencilBuffer;
    delete m_refRenderer;
}

void RandomFragmentOpCase::init(void)
{
    DE_ASSERT(!m_renderer && !m_refColorBuffer && !m_refDepthBuffer && !m_refStencilBuffer && !m_refRenderer);

    int width   = de::min<int>(m_context.getRenderTarget().getWidth(), VIEWPORT_WIDTH);
    int height  = de::min<int>(m_context.getRenderTarget().getHeight(), VIEWPORT_HEIGHT);
    bool useRGB = m_context.getRenderTarget().getPixelFormat().alphaBits == 0;

    m_renderer       = new gls::FragmentOpUtil::QuadRenderer(m_context.getRenderContext(), glu::GLSL_VERSION_100_ES);
    m_refColorBuffer = new tcu::TextureLevel(
        tcu::TextureFormat(useRGB ? tcu::TextureFormat::RGB : tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8),
        width, height);
    m_refDepthBuffer =
        new tcu::TextureLevel(tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::FLOAT), width, height);
    m_refStencilBuffer = new tcu::TextureLevel(
        tcu::TextureFormat(tcu::TextureFormat::S, tcu::TextureFormat::UNSIGNED_INT32), width, height);
    m_refRenderer = new gls::FragmentOpUtil::ReferenceQuadRenderer();
    m_iterNdx     = 0;

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
}

void RandomFragmentOpCase::deinit(void)
{
    delete m_renderer;
    delete m_refColorBuffer;
    delete m_refDepthBuffer;
    delete m_refStencilBuffer;
    delete m_refRenderer;

    m_renderer         = nullptr;
    m_refColorBuffer   = nullptr;
    m_refDepthBuffer   = nullptr;
    m_refStencilBuffer = nullptr;
    m_refRenderer      = nullptr;
}

RandomFragmentOpCase::IterateResult RandomFragmentOpCase::iterate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    const bool isMSAA        = m_context.getRenderTarget().getNumSamples() > 1;
    const uint32_t iterSeed =
        deUint32Hash(m_seed) ^ deInt32Hash(m_iterNdx) ^ deInt32Hash(m_testCtx.getCommandLine().getBaseSeed());
    de::Random rnd(iterSeed);

    const int width     = m_refColorBuffer->getWidth();
    const int height    = m_refColorBuffer->getHeight();
    const int viewportX = rnd.getInt(0, m_context.getRenderTarget().getWidth() - width);
    const int viewportY = rnd.getInt(0, m_context.getRenderTarget().getHeight() - height);

    tcu::Surface renderedImg(width, height);
    tcu::Surface referenceImg(width, height);

    const Vec4 clearColor  = CLEAR_COLOR;
    const float clearDepth = CLEAR_DEPTH;
    const int clearStencil = CLEAR_STENCIL;

    bool gotError = false;

    const tcu::ScopedLogSection iterSection(m_testCtx.getLog(), std::string("Iteration") + de::toString(m_iterNdx),
                                            std::string("Iteration ") + de::toString(m_iterNdx));

    // Compute randomized rendering commands.
    vector<RenderCommand> commands;
    computeRandomRenderCommands(rnd, glu::ApiType::es(2, 0), NUM_CALLS_PER_ITERATION, width, height, commands);

    // Reset default fragment state.
    gl.disable(GL_SCISSOR_TEST);
    gl.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    gl.depthMask(GL_TRUE);
    gl.stencilMask(~0u);

    // Render using GL.
    m_callLogWrapper.glClearColor(clearColor.x(), clearColor.y(), clearColor.z(), clearColor.w());
    m_callLogWrapper.glClearDepthf(clearDepth);
    m_callLogWrapper.glClearStencil(clearStencil);
    m_callLogWrapper.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    m_callLogWrapper.glViewport(viewportX, viewportY, width, height);

    for (vector<RenderCommand>::const_iterator cmd = commands.begin(); cmd != commands.end(); cmd++)
    {
        setGLState(m_callLogWrapper, cmd->state, viewportX, viewportY);

        if (ENABLE_CALL_LOG)
            m_testCtx.getLog() << TestLog::Message << "// Quad: " << cmd->quad.posA << " -> " << cmd->quad.posB
                               << ", color: [" << cmd->quad.color[0] << ", " << cmd->quad.color[1] << ", "
                               << cmd->quad.color[2] << ", " << cmd->quad.color[3] << "]"
                               << ", depth: [" << cmd->quad.depth[0] << ", " << cmd->quad.depth[1] << ", "
                               << cmd->quad.depth[2] << ", " << cmd->quad.depth[3] << "]" << TestLog::EndMessage;

        renderQuad(gl, *m_renderer, cmd->quad, viewportX, viewportY);
    }

    // Check error.
    if (m_callLogWrapper.glGetError() != GL_NO_ERROR)
        gotError = true;

    gl.flush();

    // Render reference while GPU is doing work.
    tcu::clear(m_refColorBuffer->getAccess(), clearColor);
    tcu::clearDepth(m_refDepthBuffer->getAccess(), clearDepth);
    tcu::clearStencil(m_refStencilBuffer->getAccess(), clearStencil);

    for (vector<RenderCommand>::const_iterator cmd = commands.begin(); cmd != commands.end(); cmd++)
    {
        rr::FragmentOperationState refState;
        translateState(cmd->state, refState, m_context.getRenderTarget());
        m_refRenderer->render(gls::FragmentOpUtil::getMultisampleAccess(m_refColorBuffer->getAccess()),
                              gls::FragmentOpUtil::getMultisampleAccess(m_refDepthBuffer->getAccess()),
                              gls::FragmentOpUtil::getMultisampleAccess(m_refStencilBuffer->getAccess()), cmd->quad,
                              refState);
    }

    // Expand reference color buffer to RGBA8
    copy(referenceImg.getAccess(), m_refColorBuffer->getAccess());

    // Read rendered image.
    glu::readPixels(m_context.getRenderContext(), viewportX, viewportY, renderedImg.getAccess());

    m_iterNdx += 1;

    // Compare to reference.
    bool isLastIter            = m_iterNdx >= NUM_ITERATIONS_PER_CASE;
    const tcu::UVec4 threshold = getCompareThreshold();
    bool compareOk;

    if (isMSAA)
    {
        // in MSAA cases, the sampling points could be anywhere in the pixel and we could
        // even have multiple samples that are combined in resolve. Allow arbitrary sample
        // positions by using bilinearCompare.
        compareOk = tcu::bilinearCompare(m_testCtx.getLog(), "CompareResult", "Image Comparison Result",
                                         referenceImg.getAccess(), renderedImg.getAccess(),
                                         tcu::RGBA(threshold.x(), threshold.y(), threshold.z(), threshold.w()),
                                         tcu::COMPARE_LOG_RESULT);
    }
    else
        compareOk = tcu::intThresholdCompare(m_testCtx.getLog(), "CompareResult", "Image Comparison Result",
                                             referenceImg.getAccess(), renderedImg.getAccess(), threshold,
                                             tcu::COMPARE_LOG_RESULT);

    m_testCtx.getLog() << TestLog::Message << (compareOk ? "  Passed." : "  FAILED!") << TestLog::EndMessage;

    if (!compareOk)
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
    else if (gotError)
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "GL error");

    if (compareOk && !gotError && !isLastIter)
        return CONTINUE;
    else
        return STOP;
}

tcu::UVec4 RandomFragmentOpCase::getCompareThreshold(void) const
{
    tcu::PixelFormat format = m_context.getRenderTarget().getPixelFormat();

    if (format == tcu::PixelFormat(8, 8, 8, 8) || format == tcu::PixelFormat(8, 8, 8, 0))
        return format.getColorThreshold().toIVec().asUint() + tcu::UVec4(6); // Default threshold.
    else
        return format.getColorThreshold().toIVec().asUint() * tcu::UVec4(5) +
               tcu::UVec4(
                   2); // \note Non-scientific ad hoc formula. Need big threshold when few color bits; especially multiple blendings bring extra inaccuracy.
}

RandomFragmentOpTests::RandomFragmentOpTests(Context &context)
    : TestCaseGroup(context, "random", "Randomized Per-Fragment Operation Tests")
{
}

RandomFragmentOpTests::~RandomFragmentOpTests(void)
{
}

void RandomFragmentOpTests::init(void)
{
    for (int ndx = 0; ndx < 100; ndx++)
        addChild(new RandomFragmentOpCase(m_context, de::toString(ndx).c_str(), "",
                                          (uint32_t)(ndx * NUM_ITERATIONS_PER_CASE)));
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
