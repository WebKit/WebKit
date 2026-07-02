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
 * \brief Depth & stencil tests.
 *//*--------------------------------------------------------------------*/

#include "es2fDepthStencilTests.hpp"
#include "glsFragmentOpUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "gluStrUtil.hpp"
#include "tcuSurface.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTextureUtil.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deMemory.h"
#include "deString.h"
#include "rrFragmentOperations.hpp"
#include "sglrReferenceUtils.hpp"

#include <algorithm>
#include <sstream>

#include "glw.h"

namespace deqp
{
namespace gles2
{
namespace Functional
{

using std::ostringstream;
using std::vector;
using tcu::IVec2;
using tcu::TestLog;
using tcu::Vec2;
using tcu::Vec4;

enum
{
    VIEWPORT_WIDTH  = 4 * 3 * 4,
    VIEWPORT_HEIGHT = 4 * 12,

    NUM_RANDOM_CASES     = 25,
    NUM_RANDOM_SUB_CASES = 10
};

namespace DepthStencilCaseUtil
{

struct StencilParams
{
    uint32_t function;
    int reference;
    uint32_t compareMask;

    uint32_t stencilFailOp;
    uint32_t depthFailOp;
    uint32_t depthPassOp;

    uint32_t writeMask;

    StencilParams(void)
        : function(0)
        , reference(0)
        , compareMask(0)
        , stencilFailOp(0)
        , depthFailOp(0)
        , depthPassOp(0)
        , writeMask(0)
    {
    }
};

struct DepthStencilParams
{
    rr::FaceType visibleFace; //!< Quad visible face.

    bool stencilTestEnabled;
    StencilParams stencil[rr::FACETYPE_LAST];

    bool depthTestEnabled;
    uint32_t depthFunc;
    float depth;
    bool depthWriteMask;

    DepthStencilParams(void)
        : visibleFace(rr::FACETYPE_LAST)
        , stencilTestEnabled(false)
        , depthTestEnabled(false)
        , depthFunc(0)
        , depth(0.0f)
        , depthWriteMask(false)
    {
    }
};

tcu::TestLog &operator<<(tcu::TestLog &log, const StencilParams &params)
{
    log << TestLog::Message << "  func = " << glu::getCompareFuncStr(params.function) << "\n"
        << "  ref = " << params.reference << "\n"
        << "  compare mask = " << tcu::toHex(params.compareMask) << "\n"
        << "  stencil fail = " << glu::getStencilOpStr(params.stencilFailOp) << "\n"
        << "  depth fail = " << glu::getStencilOpStr(params.depthFailOp) << "\n"
        << "  depth pass = " << glu::getStencilOpStr(params.depthPassOp) << "\n"
        << "  write mask = " << tcu::toHex(params.writeMask) << "\n"
        << TestLog::EndMessage;
    return log;
}

tcu::TestLog &operator<<(tcu::TestLog &log, const DepthStencilParams &params)
{
    log << TestLog::Message << "Stencil test: " << (params.stencilTestEnabled ? "enabled" : "disabled")
        << TestLog::EndMessage;
    if (params.stencilTestEnabled)
    {
        log << TestLog::Message << "Front-face stencil state: " << TestLog::EndMessage;
        log << params.stencil[rr::FACETYPE_FRONT];

        log << TestLog::Message << "Back-face stencil state: " << TestLog::EndMessage;
        log << params.stencil[rr::FACETYPE_BACK];
    }

    log << TestLog::Message << "Depth test: " << (params.depthTestEnabled ? "enabled" : "disabled")
        << TestLog::EndMessage;
    if (params.depthTestEnabled)
    {
        log << TestLog::Message << "  func = " << glu::getCompareFuncStr(params.depthFunc)
            << "\n"
               "  depth value = "
            << params.depth
            << "\n"
               "  write mask = "
            << (params.depthWriteMask ? "true" : "false") << "\n"
            << TestLog::EndMessage;
    }

    log << TestLog::Message << "Triangles are " << (params.visibleFace == rr::FACETYPE_FRONT ? "front" : "back")
        << "-facing" << TestLog::EndMessage;

    return log;
}

struct ClearCommand
{
    rr::WindowRectangle rect;
    uint32_t buffers;
    tcu::Vec4 color;
    int stencil;
    // \note No depth here - don't use clears for setting depth values; use quad rendering instead. Cleared depths are in [0, 1] to begin with,
    //         whereas rendered depths are given in [-1, 1] and then mapped to [0, 1]; this discrepancy could cause precision issues in depth tests.

    ClearCommand(void) : rect(0, 0, 0, 0), buffers(0), stencil(0)
    {
    }

    ClearCommand(const rr::WindowRectangle &rect_, uint32_t buffers_, const tcu::Vec4 &color_, int stencil_)
        : rect(rect_)
        , buffers(buffers_)
        , color(color_)
        , stencil(stencil_)
    {
    }
};

struct RenderCommand
{
    DepthStencilParams params;
    rr::WindowRectangle rect;
    tcu::Vec4 color;
    tcu::BVec4 colorMask;

    RenderCommand(void) : rect(0, 0, 0, 0)
    {
    }
};

struct RefRenderCommand
{
    gls::FragmentOpUtil::IntegerQuad quad;
    rr::FragmentOperationState state;
};

struct TestRenderTarget
{
    int width;
    int height;
    int depthBits;
    int stencilBits;

    TestRenderTarget(int width_, int height_, int depthBits_, int stencilBits_)
        : width(width_)
        , height(height_)
        , depthBits(depthBits_)
        , stencilBits(stencilBits_)
    {
    }

    TestRenderTarget(void) : width(0), height(0), depthBits(0), stencilBits(0)
    {
    }
};

void getStencilTestValues(int stencilBits, int numValues, int *values)
{
    int numLowest  = numValues / 2;
    int numHighest = numValues - numLowest;
    int maxVal     = (1 << stencilBits) - 1;

    for (int ndx = 0; ndx < numLowest; ndx++)
        values[ndx] = ndx;

    for (int ndx = 0; ndx < numHighest; ndx++)
        values[numValues - ndx - 1] = maxVal - ndx;
}

void generateBaseClearAndDepthCommands(const TestRenderTarget &target, vector<ClearCommand> &clearCommands,
                                       vector<RenderCommand> &renderCommands)
{
    DE_ASSERT(clearCommands.empty());
    DE_ASSERT(renderCommands.empty());

    const int numL0CellsX = 4;
    const int numL0CellsY = 4;
    const int numL1CellsX = 3;
    const int numL1CellsY = 1;
    int cellL0Width       = target.width / numL0CellsX;
    int cellL0Height      = target.height / numL0CellsY;
    int cellL1Width       = cellL0Width / numL1CellsX;
    int cellL1Height      = cellL0Height / numL1CellsY;

    int stencilValues[numL0CellsX * numL0CellsY];
    float depthValues[numL1CellsX * numL1CellsY];

    if (cellL0Width <= 0 || cellL1Width <= 0 || cellL0Height <= 0 || cellL1Height <= 0)
        throw tcu::NotSupportedError("Too small render target");

    // Fullscreen clear to black.
    clearCommands.push_back(ClearCommand(rr::WindowRectangle(0, 0, target.width, target.height), GL_COLOR_BUFFER_BIT,
                                         Vec4(0.0f, 0.0f, 0.0f, 1.0f), 0));

    // Compute stencil values: numL0CellsX*numL0CellsY combinations of lowest and highest bits.
    getStencilTestValues(target.stencilBits, numL0CellsX * numL0CellsY, &stencilValues[0]);

    // Compute depth values
    {
        int numValues   = DE_LENGTH_OF_ARRAY(depthValues);
        float depthStep = 2.0f / (float)(numValues - 1);

        for (int ndx = 0; ndx < numValues; ndx++)
            depthValues[ndx] = -1.0f + depthStep * (float)ndx;
    }

    for (int y0 = 0; y0 < numL0CellsY; y0++)
    {
        for (int x0 = 0; x0 < numL0CellsX; x0++)
        {
            int stencilValue = stencilValues[y0 * numL0CellsX + x0];

            for (int y1 = 0; y1 < numL1CellsY; y1++)
            {
                for (int x1 = 0; x1 < numL1CellsX; x1++)
                {
                    int x = x0 * cellL0Width + x1 * cellL1Width;
                    int y = y0 * cellL0Height + y1 * cellL1Height;
                    rr::WindowRectangle cellL1Rect(x, y, cellL1Width, cellL1Height);

                    clearCommands.push_back(ClearCommand(cellL1Rect, GL_STENCIL_BUFFER_BIT, Vec4(0), stencilValue));

                    RenderCommand renderCmd;
                    renderCmd.params.visibleFace      = rr::FACETYPE_FRONT;
                    renderCmd.params.depth            = depthValues[y1 * numL1CellsX + x1];
                    renderCmd.params.depthTestEnabled = true;
                    renderCmd.params.depthFunc        = GL_ALWAYS;
                    renderCmd.params.depthWriteMask   = true;
                    renderCmd.colorMask               = tcu::BVec4(false);
                    renderCmd.rect                    = cellL1Rect;

                    renderCommands.push_back(renderCmd);
                }
            }
        }
    }
}

void generateDepthVisualizeCommands(const TestRenderTarget &target, vector<RenderCommand> &commands)
{
    const float epsilon             = -0.05f;
    static const float depthSteps[] = {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f};
    int numSteps                    = DE_LENGTH_OF_ARRAY(depthSteps);
    const float colorStep           = 1.0f / (float)(numSteps - 1);

    for (int ndx = 0; ndx < numSteps; ndx++)
    {
        RenderCommand cmd;

        cmd.params.visibleFace      = rr::FACETYPE_FRONT;
        cmd.rect                    = rr::WindowRectangle(0, 0, target.width, target.height);
        cmd.color                   = Vec4(0.0f, 0.0f, colorStep * (float)ndx, 0.0f);
        cmd.colorMask               = tcu::BVec4(false, false, true, false);
        cmd.params.depth            = depthSteps[ndx] + epsilon;
        cmd.params.depthTestEnabled = true;
        cmd.params.depthFunc        = GL_LESS;
        cmd.params.depthWriteMask   = false;

        commands.push_back(cmd);
    }
}

void generateStencilVisualizeCommands(const TestRenderTarget &target, vector<RenderCommand> &commands)
{
    const int numValues = 4 * 4;
    float colorStep     = 1.0f / numValues; // 0 is reserved for non-matching.
    int stencilValues[numValues];

    getStencilTestValues(target.stencilBits, numValues, &stencilValues[0]);

    for (int ndx = 0; ndx < numValues; ndx++)
    {
        RenderCommand cmd;

        cmd.params.visibleFace        = rr::FACETYPE_FRONT;
        cmd.rect                      = rr::WindowRectangle(0, 0, target.width, target.height);
        cmd.color                     = Vec4(0.0f, colorStep * float(ndx + 1), 0.0f, 0.0f);
        cmd.colorMask                 = tcu::BVec4(false, true, false, false);
        cmd.params.stencilTestEnabled = true;

        cmd.params.stencil[rr::FACETYPE_FRONT].function      = GL_EQUAL;
        cmd.params.stencil[rr::FACETYPE_FRONT].reference     = stencilValues[ndx];
        cmd.params.stencil[rr::FACETYPE_FRONT].compareMask   = ~0u;
        cmd.params.stencil[rr::FACETYPE_FRONT].stencilFailOp = GL_KEEP;
        cmd.params.stencil[rr::FACETYPE_FRONT].depthFailOp   = GL_KEEP;
        cmd.params.stencil[rr::FACETYPE_FRONT].depthPassOp   = GL_KEEP;
        cmd.params.stencil[rr::FACETYPE_FRONT].writeMask     = 0u;

        cmd.params.stencil[rr::FACETYPE_BACK] = cmd.params.stencil[rr::FACETYPE_FRONT];

        commands.push_back(cmd);
    }
}

void translateStencilState(const StencilParams &src, rr::StencilState &dst)
{
    dst.func      = sglr::rr_util::mapGLTestFunc(src.function);
    dst.ref       = src.reference;
    dst.compMask  = src.compareMask;
    dst.sFail     = sglr::rr_util::mapGLStencilOp(src.stencilFailOp);
    dst.dpFail    = sglr::rr_util::mapGLStencilOp(src.depthFailOp);
    dst.dpPass    = sglr::rr_util::mapGLStencilOp(src.depthPassOp);
    dst.writeMask = src.writeMask;
}

void translateCommand(const RenderCommand &src, RefRenderCommand &dst, const TestRenderTarget &renderTarget)
{
    const float far    = 1.0f;
    const float near   = 0.0f;
    bool hasDepth      = renderTarget.depthBits > 0;
    bool hasStencil    = renderTarget.stencilBits > 0;
    bool isFrontFacing = src.params.visibleFace == rr::FACETYPE_FRONT;

    dst.quad.posA = IVec2(isFrontFacing ? src.rect.left : (src.rect.left + src.rect.width - 1), src.rect.bottom);
    dst.quad.posB = IVec2(isFrontFacing ? (src.rect.left + src.rect.width - 1) : src.rect.left,
                          src.rect.bottom + src.rect.height - 1);

    std::fill(DE_ARRAY_BEGIN(dst.quad.color), DE_ARRAY_END(dst.quad.color), src.color);
    std::fill(DE_ARRAY_BEGIN(dst.quad.depth), DE_ARRAY_END(dst.quad.depth),
              ((far - near) / 2.0f) * src.params.depth + (near + far) / 2.0f);

    dst.state.colorMask = src.colorMask;

    dst.state.scissorTestEnabled = false;
    dst.state.stencilTestEnabled = hasStencil && src.params.stencilTestEnabled;
    dst.state.depthTestEnabled   = hasDepth && src.params.depthTestEnabled;
    dst.state.blendMode          = rr::BLENDMODE_NONE;
    dst.state.numStencilBits     = renderTarget.stencilBits;

    if (dst.state.depthTestEnabled)
    {
        dst.state.depthFunc = sglr::rr_util::mapGLTestFunc(src.params.depthFunc);
        dst.state.depthMask = src.params.depthWriteMask;
    }

    if (dst.state.stencilTestEnabled)
    {
        translateStencilState(src.params.stencil[rr::FACETYPE_BACK], dst.state.stencilStates[rr::FACETYPE_BACK]);
        translateStencilState(src.params.stencil[rr::FACETYPE_FRONT], dst.state.stencilStates[rr::FACETYPE_FRONT]);
    }
}

void render(const vector<ClearCommand> &clears, int viewportX, int viewportY)
{
    glEnable(GL_SCISSOR_TEST);

    for (int ndx = 0; ndx < (int)clears.size(); ndx++)
    {
        const ClearCommand &clear = clears[ndx];

        if (clear.buffers & GL_COLOR_BUFFER_BIT)
            glClearColor(clear.color.x(), clear.color.y(), clear.color.z(), clear.color.w());
        if (clear.buffers & GL_STENCIL_BUFFER_BIT)
            glClearStencil(clear.stencil);

        DE_ASSERT(clear.buffers ==
                  (clear.buffers & (GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT))); // \note Don't use clear for depths.

        glScissor(clear.rect.left + viewportX, clear.rect.bottom + viewportY, clear.rect.width, clear.rect.height);
        glClear(clear.buffers);
    }

    glDisable(GL_SCISSOR_TEST);
}

void render(gls::FragmentOpUtil::QuadRenderer &renderer, const RenderCommand &command, int viewportX, int viewportY)
{
    if (command.params.stencilTestEnabled)
    {
        glEnable(GL_STENCIL_TEST);

        for (int face = 0; face < rr::FACETYPE_LAST; face++)
        {
            uint32_t glFace              = face == rr::FACETYPE_BACK ? GL_BACK : GL_FRONT;
            const StencilParams &sParams = command.params.stencil[face];

            glStencilFuncSeparate(glFace, sParams.function, sParams.reference, sParams.compareMask);
            glStencilOpSeparate(glFace, sParams.stencilFailOp, sParams.depthFailOp, sParams.depthPassOp);
            glStencilMaskSeparate(glFace, sParams.writeMask);
        }
    }
    else
        glDisable(GL_STENCIL_TEST);

    if (command.params.depthTestEnabled)
    {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(command.params.depthFunc);
        glDepthMask(command.params.depthWriteMask ? GL_TRUE : GL_FALSE);
    }
    else
        glDisable(GL_DEPTH_TEST);

    glColorMask(command.colorMask[0] ? GL_TRUE : GL_FALSE, command.colorMask[1] ? GL_TRUE : GL_FALSE,
                command.colorMask[2] ? GL_TRUE : GL_FALSE, command.colorMask[3] ? GL_TRUE : GL_FALSE);
    glViewport(command.rect.left + viewportX, command.rect.bottom + viewportY, command.rect.width, command.rect.height);

    gls::FragmentOpUtil::Quad quad;

    bool isFrontFacing = command.params.visibleFace == rr::FACETYPE_FRONT;
    quad.posA          = Vec2(isFrontFacing ? -1.0f : 1.0f, -1.0f);
    quad.posB          = Vec2(isFrontFacing ? 1.0f : -1.0f, 1.0f);

    std::fill(DE_ARRAY_BEGIN(quad.color), DE_ARRAY_END(quad.color), command.color);
    std::fill(DE_ARRAY_BEGIN(quad.depth), DE_ARRAY_END(quad.depth), command.params.depth);

    renderer.render(quad);
    GLU_CHECK();
}

void renderReference(const vector<ClearCommand> &clears, const tcu::PixelBufferAccess &dstColor,
                     const tcu::PixelBufferAccess &dstStencil, int stencilBits)
{
    for (int ndx = 0; ndx < (int)clears.size(); ndx++)
    {
        const ClearCommand &clear = clears[ndx];

        if (clear.buffers & GL_COLOR_BUFFER_BIT)
            tcu::clear(
                tcu::getSubregion(dstColor, clear.rect.left, clear.rect.bottom, clear.rect.width, clear.rect.height),
                clear.color);

        if (clear.buffers & GL_STENCIL_BUFFER_BIT && stencilBits > 0)
        {
            int maskedVal = clear.stencil & ((1 << stencilBits) - 1);
            tcu::clearStencil(
                tcu::getSubregion(dstStencil, clear.rect.left, clear.rect.bottom, clear.rect.width, clear.rect.height),
                maskedVal);
        }

        DE_ASSERT(clear.buffers ==
                  (clear.buffers & (GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT))); // \note Don't use clear for depths.
    }
}

} // namespace DepthStencilCaseUtil

using namespace DepthStencilCaseUtil;

class DepthStencilCase : public TestCase
{
public:
    DepthStencilCase(Context &context, const char *name, const char *desc,
                     const std::vector<DepthStencilParams> &cases);
    ~DepthStencilCase(void);

    void init(void);
    void deinit(void);

    IterateResult iterate(void);

private:
    DepthStencilCase(const DepthStencilCase &other);
    DepthStencilCase &operator=(const DepthStencilCase &other);

    std::vector<DepthStencilParams> m_cases;

    TestRenderTarget m_renderTarget;
    std::vector<ClearCommand> m_baseClears;
    std::vector<RenderCommand> m_baseDepthRenders;
    std::vector<RenderCommand> m_visualizeCommands;
    std::vector<RefRenderCommand> m_refBaseDepthRenders;
    std::vector<RefRenderCommand> m_refVisualizeCommands;

    gls::FragmentOpUtil::QuadRenderer *m_renderer;
    tcu::Surface *m_refColorBuffer;
    tcu::TextureLevel *m_refDepthBuffer;
    tcu::TextureLevel *m_refStencilBuffer;
    gls::FragmentOpUtil::ReferenceQuadRenderer *m_refRenderer;

    int m_iterNdx;
};

DepthStencilCase::DepthStencilCase(Context &context, const char *name, const char *desc,
                                   const std::vector<DepthStencilParams> &cases)
    : TestCase(context, name, desc)
    , m_cases(cases)
    , m_renderer(nullptr)
    , m_refColorBuffer(nullptr)
    , m_refDepthBuffer(nullptr)
    , m_refStencilBuffer(nullptr)
    , m_refRenderer(nullptr)
    , m_iterNdx(0)
{
}

DepthStencilCase::~DepthStencilCase(void)
{
    delete m_renderer;
    delete m_refColorBuffer;
    delete m_refDepthBuffer;
    delete m_refStencilBuffer;
    delete m_refRenderer;
}

void DepthStencilCase::init(void)
{
    DE_ASSERT(!m_renderer && !m_refColorBuffer && !m_refDepthBuffer && !m_refStencilBuffer && !m_refRenderer);

    // Compute render target.
    int viewportW  = de::min<int>(m_context.getRenderTarget().getWidth(), VIEWPORT_WIDTH);
    int viewportH  = de::min<int>(m_context.getRenderTarget().getHeight(), VIEWPORT_HEIGHT);
    m_renderTarget = TestRenderTarget(viewportW, viewportH, m_context.getRenderTarget().getDepthBits(),
                                      m_context.getRenderTarget().getStencilBits());

    // Compute base clears & visualization commands.
    generateBaseClearAndDepthCommands(m_renderTarget, m_baseClears, m_baseDepthRenders);
    generateDepthVisualizeCommands(m_renderTarget, m_visualizeCommands);
    generateStencilVisualizeCommands(m_renderTarget, m_visualizeCommands);

    // Translate to ref commands.
    m_refBaseDepthRenders.resize(m_baseDepthRenders.size());
    for (int ndx = 0; ndx < (int)m_baseDepthRenders.size(); ndx++)
        translateCommand(m_baseDepthRenders[ndx], m_refBaseDepthRenders[ndx], m_renderTarget);

    m_refVisualizeCommands.resize(m_visualizeCommands.size());
    for (int ndx = 0; ndx < (int)m_visualizeCommands.size(); ndx++)
        translateCommand(m_visualizeCommands[ndx], m_refVisualizeCommands[ndx], m_renderTarget);

    m_renderer         = new gls::FragmentOpUtil::QuadRenderer(m_context.getRenderContext(), glu::GLSL_VERSION_100_ES);
    m_refColorBuffer   = new tcu::Surface(viewportW, viewportH);
    m_refDepthBuffer   = new tcu::TextureLevel(tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::FLOAT),
                                               viewportW, viewportH);
    m_refStencilBuffer = new tcu::TextureLevel(
        tcu::TextureFormat(tcu::TextureFormat::S, tcu::TextureFormat::UNSIGNED_INT32), viewportW, viewportH);
    m_refRenderer = new gls::FragmentOpUtil::ReferenceQuadRenderer();

    m_iterNdx = 0;
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
}

void DepthStencilCase::deinit(void)
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

    m_baseClears.clear();
    m_baseDepthRenders.clear();
    m_visualizeCommands.clear();
    m_refBaseDepthRenders.clear();
    m_refVisualizeCommands.clear();
}

DepthStencilCase::IterateResult DepthStencilCase::iterate(void)
{
    de::Random rnd(deStringHash(getName()) ^ deInt32Hash(m_iterNdx));
    int viewportX = rnd.getInt(0, m_context.getRenderTarget().getWidth() - m_renderTarget.width);
    int viewportY = rnd.getInt(0, m_context.getRenderTarget().getHeight() - m_renderTarget.height);
    RenderCommand testCmd;

    tcu::Surface renderedImg(m_renderTarget.width, m_renderTarget.height);
    tcu::RGBA threshold = m_context.getRenderTarget().getPixelFormat().getColorThreshold();

    // Fill in test command for this iteration.
    testCmd.color     = Vec4(1.0f, 0.0f, 0.0f, 1.0f);
    testCmd.colorMask = tcu::BVec4(true);
    testCmd.rect      = rr::WindowRectangle(0, 0, m_renderTarget.width, m_renderTarget.height);
    testCmd.params    = m_cases[m_iterNdx];

    if (m_iterNdx == 0)
    {
        m_testCtx.getLog() << TestLog::Message
                           << "Channels:\n"
                              "  RED: passing pixels\n"
                              "  GREEN: stencil values\n"
                              "  BLUE: depth values"
                           << TestLog::EndMessage;
    }

    if (m_cases.size() > 1)
        m_testCtx.getLog() << TestLog::Message << "Iteration " << m_iterNdx << "..." << TestLog::EndMessage;

    m_testCtx.getLog() << m_cases[m_iterNdx];

    // Submit render commands to gl GL.

    // Base clears.
    render(m_baseClears, viewportX, viewportY);

    // Base depths.
    for (vector<RenderCommand>::const_iterator cmd = m_baseDepthRenders.begin(); cmd != m_baseDepthRenders.end(); ++cmd)
        render(*m_renderer, *cmd, viewportX, viewportY);

    // Test command.
    render(*m_renderer, testCmd, viewportX, viewportY);

    // Visualization commands.
    for (vector<RenderCommand>::const_iterator cmd = m_visualizeCommands.begin(); cmd != m_visualizeCommands.end();
         ++cmd)
        render(*m_renderer, *cmd, viewportX, viewportY);

    // Re-enable all write masks.
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glStencilMask(~0u);

    // Ask GPU to start rendering.
    glFlush();

    // Render reference while GPU is doing work.
    {
        RefRenderCommand refTestCmd;
        translateCommand(testCmd, refTestCmd, m_renderTarget);

        // Base clears.
        renderReference(m_baseClears, m_refColorBuffer->getAccess(), m_refStencilBuffer->getAccess(),
                        m_renderTarget.stencilBits);

        // Base depths.
        for (vector<RefRenderCommand>::const_iterator cmd = m_refBaseDepthRenders.begin();
             cmd != m_refBaseDepthRenders.end(); ++cmd)
            m_refRenderer->render(gls::FragmentOpUtil::getMultisampleAccess(m_refColorBuffer->getAccess()),
                                  gls::FragmentOpUtil::getMultisampleAccess(m_refDepthBuffer->getAccess()),
                                  gls::FragmentOpUtil::getMultisampleAccess(m_refStencilBuffer->getAccess()), cmd->quad,
                                  cmd->state);

        // Test command.
        m_refRenderer->render(gls::FragmentOpUtil::getMultisampleAccess(m_refColorBuffer->getAccess()),
                              gls::FragmentOpUtil::getMultisampleAccess(m_refDepthBuffer->getAccess()),
                              gls::FragmentOpUtil::getMultisampleAccess(m_refStencilBuffer->getAccess()),
                              refTestCmd.quad, refTestCmd.state);

        // Visualization commands.
        for (vector<RefRenderCommand>::const_iterator cmd = m_refVisualizeCommands.begin();
             cmd != m_refVisualizeCommands.end(); ++cmd)
            m_refRenderer->render(gls::FragmentOpUtil::getMultisampleAccess(m_refColorBuffer->getAccess()),
                                  gls::FragmentOpUtil::getMultisampleAccess(m_refDepthBuffer->getAccess()),
                                  gls::FragmentOpUtil::getMultisampleAccess(m_refStencilBuffer->getAccess()), cmd->quad,
                                  cmd->state);
    }

    // Read rendered image.
    glu::readPixels(m_context.getRenderContext(), viewportX, viewportY, renderedImg.getAccess());

    m_iterNdx += 1;

    // Compare to reference.
    bool isLastIter = m_iterNdx >= (int)m_cases.size();
    bool compareOk  = tcu::pixelThresholdCompare(m_testCtx.getLog(), "CompareResult", "Image Comparison Result",
                                                 *m_refColorBuffer, renderedImg, threshold, tcu::COMPARE_LOG_RESULT);

    m_testCtx.getLog() << TestLog::Message << (compareOk ? "  Passed." : "  FAILED!") << TestLog::EndMessage;
    if (!compareOk)
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");

    if (compareOk && !isLastIter)
        return CONTINUE;
    else
        return STOP;
}

DepthStencilTests::DepthStencilTests(Context &context)
    : TestCaseGroup(context, "depth_stencil", "Depth and Stencil Op Tests")
{
}

DepthStencilTests::~DepthStencilTests(void)
{
}

static void randomDepthStencilState(de::Random &rnd, DepthStencilParams &params)
{
    const float stencilTestProbability = 0.8f;
    const float depthTestProbability   = 0.7f;

    static const uint32_t compareFuncs[] = {GL_NEVER, GL_ALWAYS, GL_LESS,    GL_LEQUAL,
                                            GL_EQUAL, GL_GEQUAL, GL_GREATER, GL_NOTEQUAL};

    static const uint32_t stencilOps[] = {GL_KEEP, GL_ZERO,   GL_REPLACE,   GL_INCR,
                                          GL_DECR, GL_INVERT, GL_INCR_WRAP, GL_DECR_WRAP};

    static const float depthValues[] = {-1.0f, -0.8f, -0.6f, -0.4f, -0.2f, 0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};

    params.visibleFace        = rnd.getBool() ? rr::FACETYPE_FRONT : rr::FACETYPE_BACK;
    params.stencilTestEnabled = rnd.getFloat() < stencilTestProbability;
    params.depthTestEnabled   = !params.stencilTestEnabled || (rnd.getFloat() < depthTestProbability);

    if (params.stencilTestEnabled)
    {
        for (int ndx = 0; ndx < 2; ndx++)
        {
            params.stencil[ndx].function =
                rnd.choose<uint32_t>(DE_ARRAY_BEGIN(compareFuncs), DE_ARRAY_END(compareFuncs));
            params.stencil[ndx].reference   = rnd.getInt(-2, 260);
            params.stencil[ndx].compareMask = rnd.getUint32();
            params.stencil[ndx].stencilFailOp =
                rnd.choose<uint32_t>(DE_ARRAY_BEGIN(stencilOps), DE_ARRAY_END(stencilOps));
            params.stencil[ndx].depthFailOp =
                rnd.choose<uint32_t>(DE_ARRAY_BEGIN(stencilOps), DE_ARRAY_END(stencilOps));
            params.stencil[ndx].depthPassOp =
                rnd.choose<uint32_t>(DE_ARRAY_BEGIN(stencilOps), DE_ARRAY_END(stencilOps));
            params.stencil[ndx].writeMask = rnd.getUint32();
        }
    }

    if (params.depthTestEnabled)
    {
        params.depthFunc      = rnd.choose<uint32_t>(DE_ARRAY_BEGIN(compareFuncs), DE_ARRAY_END(compareFuncs));
        params.depth          = rnd.choose<float>(DE_ARRAY_BEGIN(depthValues), DE_ARRAY_END(depthValues));
        params.depthWriteMask = rnd.getBool();
    }
}

void DepthStencilTests::init(void)
{
    static const struct
    {
        const char *name;
        uint32_t func;
    } compareFuncs[] = {{"never", GL_NEVER}, {"always", GL_ALWAYS}, {"less", GL_LESS},       {"lequal", GL_LEQUAL},
                        {"equal", GL_EQUAL}, {"gequal", GL_GEQUAL}, {"greater", GL_GREATER}, {"notequal", GL_NOTEQUAL}};

    static const struct
    {
        const char *name;
        uint32_t op;
    } stencilOps[] = {{"keep", GL_KEEP},           {"zero", GL_ZERO},          {"replace", GL_REPLACE},
                      {"incr", GL_INCR},           {"decr", GL_DECR},          {"invert", GL_INVERT},
                      {"incr_wrap", GL_INCR_WRAP}, {"decr_wrap", GL_DECR_WRAP}};

    static const struct
    {
        rr::FaceType visibleFace;
        uint32_t sFail;
        uint32_t dFail;
        uint32_t dPass;
        int stencilRef;
        uint32_t compareMask;
        uint32_t writeMask;
        float depth;
    } functionCases[] = {{rr::FACETYPE_BACK, GL_DECR, GL_INCR, GL_INVERT, 4, ~0u, ~0u, -0.7f},
                         {rr::FACETYPE_FRONT, GL_DECR, GL_INCR, GL_INVERT, 2, ~0u, ~0u, 0.0f},
                         {rr::FACETYPE_BACK, GL_DECR, GL_INCR, GL_INVERT, 1, ~0u, ~0u, 0.2f},
                         {rr::FACETYPE_FRONT, GL_DECR_WRAP, GL_INVERT, GL_REPLACE, 4, ~0u, ~0u, 1.0f}};

    // All combinations of depth stencil functions.
    {
        tcu::TestCaseGroup *functionsGroup =
            new tcu::TestCaseGroup(m_testCtx, "stencil_depth_funcs", "Combinations of Depth and Stencil Functions");
        addChild(functionsGroup);

        for (int stencilFunc = 0; stencilFunc < DE_LENGTH_OF_ARRAY(compareFuncs) + 1; stencilFunc++)
        {
            // One extra: depth test disabled.
            for (int depthFunc = 0; depthFunc < DE_LENGTH_OF_ARRAY(compareFuncs) + 1; depthFunc++)
            {
                DepthStencilParams params;
                ostringstream name;
                bool hasStencilFunc = de::inBounds(stencilFunc, 0, DE_LENGTH_OF_ARRAY(compareFuncs));
                bool hasDepthFunc   = de::inBounds(depthFunc, 0, DE_LENGTH_OF_ARRAY(compareFuncs));

                if (hasStencilFunc)
                    name << "stencil_" << compareFuncs[stencilFunc].name << "_";
                else
                    name << "no_stencil_";

                if (hasDepthFunc)
                    name << "depth_" << compareFuncs[depthFunc].name;
                else
                    name << "no_depth";

                params.depthFunc        = hasDepthFunc ? compareFuncs[depthFunc].func : 0;
                params.depthTestEnabled = hasDepthFunc;
                params.depthWriteMask   = true;

                params.stencilTestEnabled = hasStencilFunc;

                vector<DepthStencilParams> cases;
                for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(functionCases); ndx++)
                {
                    rr::FaceType visible    = functionCases[ndx].visibleFace;
                    rr::FaceType notVisible = visible == rr::FACETYPE_FRONT ? rr::FACETYPE_BACK : rr::FACETYPE_FRONT;

                    params.depth       = functionCases[ndx].depth;
                    params.visibleFace = visible;

                    params.stencil[visible].function      = hasStencilFunc ? compareFuncs[stencilFunc].func : 0;
                    params.stencil[visible].reference     = functionCases[ndx].stencilRef;
                    params.stencil[visible].stencilFailOp = functionCases[ndx].sFail;
                    params.stencil[visible].depthFailOp   = functionCases[ndx].dFail;
                    params.stencil[visible].depthPassOp   = functionCases[ndx].dPass;
                    params.stencil[visible].compareMask   = functionCases[ndx].compareMask;
                    params.stencil[visible].writeMask     = functionCases[ndx].writeMask;

                    params.stencil[notVisible].function      = GL_ALWAYS;
                    params.stencil[notVisible].reference     = 0;
                    params.stencil[notVisible].stencilFailOp = GL_REPLACE;
                    params.stencil[notVisible].depthFailOp   = GL_REPLACE;
                    params.stencil[notVisible].depthPassOp   = GL_REPLACE;
                    params.stencil[notVisible].compareMask   = 0u;
                    params.stencil[notVisible].writeMask     = ~0u;

                    cases.push_back(params);
                }

                functionsGroup->addChild(new DepthStencilCase(m_context, name.str().c_str(), "", cases));
            }
        }
    }

    static const struct
    {
        rr::FaceType visibleFace;
        uint32_t func;
        int ref;
        uint32_t compareMask;
        uint32_t writeMask;
    } opCombinationCases[] = {{rr::FACETYPE_BACK, GL_LESS, 4, ~0u, ~0u},
                              {rr::FACETYPE_FRONT, GL_GREATER, 2, ~0u, ~0u},
                              {rr::FACETYPE_BACK, GL_EQUAL, 3, ~2u, ~0u},
                              {rr::FACETYPE_FRONT, GL_NOTEQUAL, 1, ~0u, ~1u}};

    // All combinations of stencil ops.
    {
        tcu::TestCaseGroup *opCombinationGroup =
            new tcu::TestCaseGroup(m_testCtx, "stencil_ops", "Stencil Op Combinations");
        addChild(opCombinationGroup);

        for (int sFail = 0; sFail < DE_LENGTH_OF_ARRAY(stencilOps); sFail++)
        {
            for (int dFail = 0; dFail < DE_LENGTH_OF_ARRAY(stencilOps); dFail++)
            {
                for (int dPass = 0; dPass < DE_LENGTH_OF_ARRAY(stencilOps); dPass++)
                {
                    DepthStencilParams params;
                    ostringstream name;

                    name << stencilOps[sFail].name << "_" << stencilOps[dFail].name << "_" << stencilOps[dPass].name;

                    params.depthFunc        = GL_LEQUAL;
                    params.depth            = 0.0f;
                    params.depthTestEnabled = true;
                    params.depthWriteMask   = true;

                    params.stencilTestEnabled = true;

                    vector<DepthStencilParams> cases;
                    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(opCombinationCases); ndx++)
                    {
                        rr::FaceType visible = opCombinationCases[ndx].visibleFace;
                        rr::FaceType notVisible =
                            visible == rr::FACETYPE_FRONT ? rr::FACETYPE_BACK : rr::FACETYPE_FRONT;

                        params.visibleFace = visible;

                        params.stencil[visible].function      = opCombinationCases[ndx].func;
                        params.stencil[visible].reference     = opCombinationCases[ndx].ref;
                        params.stencil[visible].stencilFailOp = stencilOps[sFail].op;
                        params.stencil[visible].depthFailOp   = stencilOps[dFail].op;
                        params.stencil[visible].depthPassOp   = stencilOps[dPass].op;
                        params.stencil[visible].compareMask   = opCombinationCases[ndx].compareMask;
                        params.stencil[visible].writeMask     = opCombinationCases[ndx].writeMask;

                        params.stencil[notVisible].function      = GL_ALWAYS;
                        params.stencil[notVisible].reference     = 0;
                        params.stencil[notVisible].stencilFailOp = GL_REPLACE;
                        params.stencil[notVisible].depthFailOp   = GL_REPLACE;
                        params.stencil[notVisible].depthPassOp   = GL_REPLACE;
                        params.stencil[notVisible].compareMask   = 0u;
                        params.stencil[notVisible].writeMask     = ~0u;

                        cases.push_back(params);
                    }

                    opCombinationGroup->addChild(new DepthStencilCase(m_context, name.str().c_str(), "", cases));
                }
            }
        }
    }

    // Write masks
    {
        tcu::TestCaseGroup *writeMaskGroup =
            new tcu::TestCaseGroup(m_testCtx, "write_mask", "Depth and Stencil Write Masks");
        addChild(writeMaskGroup);

        // Depth mask
        {
            DepthStencilParams params;

            params.depthFunc          = GL_LEQUAL;
            params.depth              = 0.0f;
            params.depthTestEnabled   = true;
            params.stencilTestEnabled = true;

            params.stencil[rr::FACETYPE_FRONT].function      = GL_NOTEQUAL;
            params.stencil[rr::FACETYPE_FRONT].reference     = 1;
            params.stencil[rr::FACETYPE_FRONT].stencilFailOp = GL_INVERT;
            params.stencil[rr::FACETYPE_FRONT].depthFailOp   = GL_INCR;
            params.stencil[rr::FACETYPE_FRONT].depthPassOp   = GL_DECR;
            params.stencil[rr::FACETYPE_FRONT].compareMask   = ~0u;
            params.stencil[rr::FACETYPE_FRONT].writeMask     = ~0u;

            params.stencil[rr::FACETYPE_BACK].function      = GL_ALWAYS;
            params.stencil[rr::FACETYPE_BACK].reference     = 0;
            params.stencil[rr::FACETYPE_BACK].stencilFailOp = GL_REPLACE;
            params.stencil[rr::FACETYPE_BACK].depthFailOp   = GL_INVERT;
            params.stencil[rr::FACETYPE_BACK].depthPassOp   = GL_INCR;
            params.stencil[rr::FACETYPE_BACK].compareMask   = ~0u;
            params.stencil[rr::FACETYPE_BACK].writeMask     = ~0u;

            vector<DepthStencilParams> cases;

            // Case 1: front, depth write enabled
            params.visibleFace    = rr::FACETYPE_FRONT;
            params.depthWriteMask = true;
            cases.push_back(params);

            // Case 2: front, depth write disabled
            params.visibleFace    = rr::FACETYPE_FRONT;
            params.depthWriteMask = false;
            cases.push_back(params);

            // Case 3: back, depth write enabled
            params.visibleFace    = rr::FACETYPE_BACK;
            params.depthWriteMask = true;
            cases.push_back(params);

            // Case 4: back, depth write disabled
            params.visibleFace    = rr::FACETYPE_BACK;
            params.depthWriteMask = false;
            cases.push_back(params);

            writeMaskGroup->addChild(new DepthStencilCase(m_context, "depth", "Depth Write Mask", cases));
        }

        // Stencil write masks.
        {
            static const struct
            {
                rr::FaceType visibleFace;
                uint32_t frontWriteMask;
                uint32_t backWriteMask;
            } stencilWmaskCases[] = {{rr::FACETYPE_FRONT, ~0u, 0u},     {rr::FACETYPE_FRONT, 0u, ~0u},
                                     {rr::FACETYPE_FRONT, 0xfu, 0xf0u}, {rr::FACETYPE_FRONT, 0x2u, 0x4u},
                                     {rr::FACETYPE_BACK, 0u, ~0u},      {rr::FACETYPE_BACK, ~0u, 0u},
                                     {rr::FACETYPE_BACK, 0xf0u, 0xfu},  {rr::FACETYPE_BACK, 0x4u, 0x2u}};

            DepthStencilParams params;

            params.depthFunc          = GL_LEQUAL;
            params.depth              = 0.0f;
            params.depthTestEnabled   = true;
            params.depthWriteMask     = true;
            params.stencilTestEnabled = true;

            params.stencil[rr::FACETYPE_FRONT].function      = GL_NOTEQUAL;
            params.stencil[rr::FACETYPE_FRONT].reference     = 1;
            params.stencil[rr::FACETYPE_FRONT].stencilFailOp = GL_INVERT;
            params.stencil[rr::FACETYPE_FRONT].depthFailOp   = GL_INCR;
            params.stencil[rr::FACETYPE_FRONT].depthPassOp   = GL_DECR;
            params.stencil[rr::FACETYPE_FRONT].compareMask   = ~0u;

            params.stencil[rr::FACETYPE_BACK].function      = GL_ALWAYS;
            params.stencil[rr::FACETYPE_BACK].reference     = 0;
            params.stencil[rr::FACETYPE_BACK].stencilFailOp = GL_REPLACE;
            params.stencil[rr::FACETYPE_BACK].depthFailOp   = GL_INVERT;
            params.stencil[rr::FACETYPE_BACK].depthPassOp   = GL_INCR;
            params.stencil[rr::FACETYPE_BACK].compareMask   = ~0u;

            vector<DepthStencilParams> cases;
            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(stencilWmaskCases); ndx++)
            {
                params.visibleFace                           = stencilWmaskCases[ndx].visibleFace;
                params.stencil[rr::FACETYPE_FRONT].writeMask = stencilWmaskCases[ndx].frontWriteMask;
                params.stencil[rr::FACETYPE_BACK].writeMask  = stencilWmaskCases[ndx].backWriteMask;
                cases.push_back(params);
            }

            writeMaskGroup->addChild(new DepthStencilCase(m_context, "stencil", "Stencil Write Mask", cases));
        }

        // Depth & stencil write masks.
        {
            static const struct
            {
                bool depthWriteMask;
                rr::FaceType visibleFace;
                uint32_t frontWriteMask;
                uint32_t backWriteMask;
            } depthStencilWmaskCases[] = {
                {false, rr::FACETYPE_FRONT, ~0u, 0u},     {false, rr::FACETYPE_FRONT, 0u, ~0u},
                {false, rr::FACETYPE_FRONT, 0xfu, 0xf0u}, {true, rr::FACETYPE_FRONT, ~0u, 0u},
                {true, rr::FACETYPE_FRONT, 0u, ~0u},      {true, rr::FACETYPE_FRONT, 0xfu, 0xf0u},
                {false, rr::FACETYPE_BACK, 0u, ~0u},      {false, rr::FACETYPE_BACK, ~0u, 0u},
                {false, rr::FACETYPE_BACK, 0xf0u, 0xfu},  {true, rr::FACETYPE_BACK, 0u, ~0u},
                {true, rr::FACETYPE_BACK, ~0u, 0u},       {true, rr::FACETYPE_BACK, 0xf0u, 0xfu}};

            DepthStencilParams params;

            params.depthFunc          = GL_LEQUAL;
            params.depth              = 0.0f;
            params.depthTestEnabled   = true;
            params.depthWriteMask     = true;
            params.stencilTestEnabled = true;

            params.stencil[rr::FACETYPE_FRONT].function      = GL_NOTEQUAL;
            params.stencil[rr::FACETYPE_FRONT].reference     = 1;
            params.stencil[rr::FACETYPE_FRONT].stencilFailOp = GL_INVERT;
            params.stencil[rr::FACETYPE_FRONT].depthFailOp   = GL_INCR;
            params.stencil[rr::FACETYPE_FRONT].depthPassOp   = GL_DECR;
            params.stencil[rr::FACETYPE_FRONT].compareMask   = ~0u;

            params.stencil[rr::FACETYPE_BACK].function      = GL_ALWAYS;
            params.stencil[rr::FACETYPE_BACK].reference     = 0;
            params.stencil[rr::FACETYPE_BACK].stencilFailOp = GL_REPLACE;
            params.stencil[rr::FACETYPE_BACK].depthFailOp   = GL_INVERT;
            params.stencil[rr::FACETYPE_BACK].depthPassOp   = GL_INCR;
            params.stencil[rr::FACETYPE_BACK].compareMask   = ~0u;

            vector<DepthStencilParams> cases;
            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(depthStencilWmaskCases); ndx++)
            {
                params.depthWriteMask                        = depthStencilWmaskCases[ndx].depthWriteMask;
                params.visibleFace                           = depthStencilWmaskCases[ndx].visibleFace;
                params.stencil[rr::FACETYPE_FRONT].writeMask = depthStencilWmaskCases[ndx].frontWriteMask;
                params.stencil[rr::FACETYPE_BACK].writeMask  = depthStencilWmaskCases[ndx].backWriteMask;
                cases.push_back(params);
            }

            writeMaskGroup->addChild(new DepthStencilCase(m_context, "both", "Depth and Stencil Write Masks", cases));
        }
    }

    // Randomized cases
    {
        tcu::TestCaseGroup *randomGroup =
            new tcu::TestCaseGroup(m_testCtx, "random", "Randomized Depth and Stencil Test Cases");
        addChild(randomGroup);

        for (int caseNdx = 0; caseNdx < NUM_RANDOM_CASES; caseNdx++)
        {
            vector<DepthStencilParams> subCases(NUM_RANDOM_SUB_CASES);
            de::Random rnd(deInt32Hash(caseNdx) ^ deInt32Hash(m_testCtx.getCommandLine().getBaseSeed()));

            for (vector<DepthStencilParams>::iterator iter = subCases.begin(); iter != subCases.end(); ++iter)
                randomDepthStencilState(rnd, *iter);

            randomGroup->addChild(new DepthStencilCase(m_context, de::toString(caseNdx).c_str(), "", subCases));
        }
    }
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
