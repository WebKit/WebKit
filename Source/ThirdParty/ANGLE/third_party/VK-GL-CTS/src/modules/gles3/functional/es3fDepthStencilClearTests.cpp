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
 * \brief Depth and stencil clear tests.
 *//*--------------------------------------------------------------------*/

#include "es3fDepthStencilClearTests.hpp"

#include "gluShaderProgram.hpp"
#include "gluPixelTransfer.hpp"
#include "gluRenderContext.hpp"

#include "tcuTestLog.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuSurface.hpp"
#include "tcuRenderTarget.hpp"

#include "deRandom.hpp"
#include "deMath.h"
#include "deString.h"

#include "glwFunctions.hpp"
#include "glwEnums.hpp"

namespace deqp
{
namespace gles3
{
namespace Functional
{

using std::string;
using std::vector;
using tcu::TestLog;
using tcu::Vec3;
using tcu::Vec4;

namespace
{

enum
{
    STENCIL_STEPS = 32,
    DEPTH_STEPS   = 32
};

struct Clear
{
    Clear(void)
        : clearMask(0)
        , clearDepth(0.0f)
        , clearStencil(0)
        , useScissor(false)
        , scissor(0, 0, 0, 0)
        , depthMask(false)
        , stencilMask(0)
    {
    }

    uint32_t clearMask;
    float clearDepth;
    int clearStencil;

    bool useScissor;
    tcu::IVec4 scissor;

    bool depthMask;
    uint32_t stencilMask;
};

tcu::TextureFormat getDepthFormat(int depthBits)
{
    switch (depthBits)
    {
    case 8:
        return tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNORM_INT8);
    case 16:
        return tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNORM_INT16);
    case 24:
        return tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNORM_INT24);
    case 32:
        return tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::FLOAT);
    default:
        TCU_FAIL("Can't map depth buffer format");
    }
}

tcu::TextureFormat getStencilFormat(int stencilBits)
{
    switch (stencilBits)
    {
    case 8:
        return tcu::TextureFormat(tcu::TextureFormat::S, tcu::TextureFormat::UNSIGNED_INT8);
    case 16:
        return tcu::TextureFormat(tcu::TextureFormat::S, tcu::TextureFormat::UNSIGNED_INT16);
    case 24:
        return tcu::TextureFormat(tcu::TextureFormat::S, tcu::TextureFormat::UNSIGNED_INT24);
    case 32:
        return tcu::TextureFormat(tcu::TextureFormat::S, tcu::TextureFormat::UNSIGNED_INT32);
    default:
        TCU_FAIL("Can't map depth buffer format");
    }
}

} // namespace

class DepthStencilClearCase : public TestCase
{
public:
    DepthStencilClearCase(Context &context, const char *name, const char *description, int numIters, int numClears,
                          bool depth, bool stencil, bool scissor, bool masked);
    ~DepthStencilClearCase(void);

    void init(void);
    void deinit(void);

    IterateResult iterate(void);

private:
    void generateClears(vector<Clear> &dst, uint32_t seed);
    void renderGL(tcu::Surface &dst, const vector<Clear> &clears);
    void renderReference(tcu::Surface &dst, const vector<Clear> &clears);

    bool m_testDepth;
    bool m_testStencil;
    bool m_testScissor;
    bool m_masked;
    int m_numIters;
    int m_numClears;
    int m_curIter;

    glu::ShaderProgram *m_visProgram;
};

DepthStencilClearCase::DepthStencilClearCase(Context &context, const char *name, const char *description, int numIters,
                                             int numClears, bool depth, bool stencil, bool scissor, bool masked)
    : TestCase(context, name, description)
    , m_testDepth(depth)
    , m_testStencil(stencil)
    , m_testScissor(scissor)
    , m_masked(masked)
    , m_numIters(numIters)
    , m_numClears(numClears)
    , m_curIter(0)
    , m_visProgram(nullptr)
{
}

DepthStencilClearCase::~DepthStencilClearCase(void)
{
    DepthStencilClearCase::deinit();
}

void DepthStencilClearCase::init(void)
{
    TestLog &log = m_testCtx.getLog();

    m_visProgram =
        new glu::ShaderProgram(m_context.getRenderContext(), glu::makeVtxFragSources(
                                                                 // Vertex shader.
                                                                 "#version 300 es\n"
                                                                 "in highp vec4 a_position;\n"
                                                                 "void main (void)\n"
                                                                 "{\n"
                                                                 "    gl_Position = a_position;\n"
                                                                 "}\n",

                                                                 // Fragment shader.
                                                                 "#version 300 es\n"
                                                                 "uniform mediump vec4 u_color;\n"
                                                                 "layout(location = 0) out mediump vec4 o_color;\n"
                                                                 "void main (void)\n"
                                                                 "{\n"
                                                                 "    o_color = u_color;\n"
                                                                 "}\n"));

    if (!m_visProgram->isOk())
    {
        log << *m_visProgram;
        delete m_visProgram;
        m_visProgram = nullptr;
        TCU_FAIL("Compile failed");
    }

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
}

void DepthStencilClearCase::deinit(void)
{
    delete m_visProgram;
    m_visProgram = nullptr;
}

DepthStencilClearCase::IterateResult DepthStencilClearCase::iterate(void)
{
    const tcu::RenderTarget &renderTarget = m_context.getRenderTarget();
    int width                             = renderTarget.getWidth();
    int height                            = renderTarget.getHeight();
    tcu::Surface result(width, height);
    tcu::Surface reference(width, height);
    tcu::RGBA threshold = renderTarget.getPixelFormat().getColorThreshold() + tcu::RGBA(1, 1, 1, 1);
    vector<Clear> clears;

    if ((m_testDepth && renderTarget.getDepthBits() == 0) || (m_testStencil && renderTarget.getStencilBits() == 0))
        throw tcu::NotSupportedError("No depth/stencil buffers", "", __FILE__, __LINE__);

    generateClears(clears, deStringHash(getName()) ^ deInt32Hash(m_curIter));
    renderGL(result, clears);
    renderReference(reference, clears);

    bool isLastIter = m_curIter + 1 == m_numIters;
    bool isOk = tcu::pixelThresholdCompare(m_testCtx.getLog(), "Result", "Image comparison result", reference, result,
                                           threshold, isLastIter ? tcu::COMPARE_LOG_RESULT : tcu::COMPARE_LOG_ON_ERROR);

    if (!isOk)
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");

    m_curIter += 1;
    return isLastIter || !isOk ? STOP : CONTINUE;
}

void DepthStencilClearCase::generateClears(vector<Clear> &clears, uint32_t seed)
{
    const tcu::RenderTarget &renderTarget = m_context.getRenderContext().getRenderTarget();
    int width                             = renderTarget.getWidth();
    int height                            = renderTarget.getHeight();
    de::Random rnd(seed);

    clears.resize(m_numClears);

    for (vector<Clear>::iterator clear = clears.begin(); clear != clears.end(); clear++)
    {
        if (m_testScissor)
        {
            int w = rnd.getInt(1, width);
            int h = rnd.getInt(1, height);
            int x = rnd.getInt(0, width - w);
            int y = rnd.getInt(0, height - h);

            clear->useScissor = true; // \todo [pyry] Should we randomize?
            clear->scissor    = tcu::IVec4(x, y, w, h);
        }
        else
            clear->useScissor = false;

        clear->clearDepth   = rnd.getFloat(-0.2f, 1.2f);
        clear->clearStencil = rnd.getUint32();

        clear->depthMask   = m_masked ? rnd.getBool() : true;
        clear->stencilMask = m_masked ? rnd.getUint32() : 0xffffffffu;

        if (m_testDepth && m_testStencil)
        {
            switch (rnd.getInt(0, 2))
            {
            case 0:
                clear->clearMask = GL_DEPTH_BUFFER_BIT;
                break;
            case 1:
                clear->clearMask = GL_STENCIL_BUFFER_BIT;
                break;
            case 2:
                clear->clearMask = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
                break;
            }
        }
        else if (m_testDepth)
            clear->clearMask = GL_DEPTH_BUFFER_BIT;
        else
        {
            DE_ASSERT(m_testStencil);
            clear->clearMask = GL_STENCIL_BUFFER_BIT;
        }
    }
}

void DepthStencilClearCase::renderGL(tcu::Surface &dst, const vector<Clear> &clears)
{
    const glw::Functions &gl       = m_context.getRenderContext().getFunctions();
    int colorLoc                   = gl.getUniformLocation(m_visProgram->getProgram(), "u_color");
    int positionLoc                = gl.getAttribLocation(m_visProgram->getProgram(), "a_position");
    static const uint8_t indices[] = {0, 1, 2, 2, 1, 3};

    // Clear with default values.
    gl.clearDepthf(1.0f);
    gl.clearStencil(0);
    gl.clearColor(1.0f, 0.0f, 0.0f, 1.0f);
    gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    GLU_EXPECT_NO_ERROR(gl.getError(), "Before clears");

    for (vector<Clear>::const_iterator clear = clears.begin(); clear != clears.end(); clear++)
    {
        if (clear->useScissor)
        {
            gl.enable(GL_SCISSOR_TEST);
            gl.scissor(clear->scissor.x(), clear->scissor.y(), clear->scissor.z(), clear->scissor.w());
        }

        // Clear values.
        gl.clearDepthf(clear->clearDepth);
        gl.clearStencil(clear->clearStencil);

        // Masks.
        gl.depthMask(clear->depthMask ? GL_TRUE : GL_FALSE);
        gl.stencilMask(clear->stencilMask);

        // Execute clear.
        gl.clear(clear->clearMask);

        if (clear->useScissor)
            gl.disable(GL_SCISSOR_TEST);
    }

    // Restore default masks.
    gl.depthMask(GL_TRUE);
    gl.stencilMask(0xffffffffu);

    GLU_EXPECT_NO_ERROR(gl.getError(), "After clears");

    gl.useProgram(m_visProgram->getProgram());
    gl.enableVertexAttribArray(positionLoc);

    // Visualize depth / stencil buffers.
    if (m_testDepth)
    {
        int numSteps = DEPTH_STEPS;
        float step   = 2.0f / (float)numSteps;

        gl.enable(GL_DEPTH_TEST);
        gl.depthFunc(GL_LESS);
        gl.depthMask(GL_FALSE);
        gl.colorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);

        for (int ndx = 0; ndx < numSteps; ndx++)
        {
            float d     = -1.0f + step * (float)ndx;
            float c     = (float)ndx / (float)(numSteps - 1);
            float pos[] = {-1.0f, -1.0f, d, -1.0f, 1.0f, d, 1.0f, -1.0f, d, 1.0f, 1.0f, d};

            gl.uniform4f(colorLoc, 0.0f, 0.0f, c, 1.0f);
            gl.vertexAttribPointer(positionLoc, 3, GL_FLOAT, GL_FALSE, 0, &pos[0]);
            gl.drawElements(GL_TRIANGLES, DE_LENGTH_OF_ARRAY(indices), GL_UNSIGNED_BYTE, &indices[0]);
        }

        gl.disable(GL_DEPTH_TEST);
        gl.depthMask(GL_TRUE);

        GLU_EXPECT_NO_ERROR(gl.getError(), "After depth visualization");
    }

    if (m_testStencil)
    {
        int numSteps  = STENCIL_STEPS;
        int numValues = (1 << TestCase::m_context.getRenderContext().getRenderTarget().getStencilBits()); // 2^bits
        int step      = numValues / numSteps;

        gl.enable(GL_STENCIL_TEST);
        gl.stencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        gl.colorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);

        static const float pos[] = {-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f};
        gl.vertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, &pos[0]);

        for (int ndx = 0; ndx < numSteps; ndx++)
        {
            int s   = step * ndx;
            float c = (float)ndx / (float)(numSteps - 1);

            gl.stencilFunc(GL_LEQUAL, s, 0xffu);
            gl.uniform4f(colorLoc, 0.0f, c, 0.0f, 1.0f);
            gl.drawElements(GL_TRIANGLES, DE_LENGTH_OF_ARRAY(indices), GL_UNSIGNED_BYTE, &indices[0]);
        }

        gl.disable(GL_STENCIL_TEST);

        GLU_EXPECT_NO_ERROR(gl.getError(), "After stencil visualization");
    }

    // Restore color mask (changed by visualization).
    gl.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glu::readPixels(m_context.getRenderContext(), 0, 0, dst.getAccess());
}

void DepthStencilClearCase::renderReference(tcu::Surface &dst, const vector<Clear> &clears)
{
    glu::RenderContext &renderCtx         = TestCase::m_context.getRenderContext();
    const tcu::RenderTarget &renderTarget = renderCtx.getRenderTarget();

    // Clear surface to red.
    tcu::clear(dst.getAccess(), tcu::RGBA::red().toVec());

    if (m_testDepth)
    {
        // Simulated depth buffer span.
        tcu::TextureLevel depthBufRow(getDepthFormat(renderTarget.getDepthBits()), dst.getWidth(), 1, 1);
        tcu::PixelBufferAccess rowAccess = depthBufRow.getAccess();

        for (int y = 0; y < dst.getHeight(); y++)
        {
            // Clear to default value.
            for (int x = 0; x < rowAccess.getWidth(); x++)
                rowAccess.setPixel(Vec4(1.0f), x, 0);

            // Execute clears.
            for (vector<Clear>::const_iterator clear = clears.begin(); clear != clears.end(); clear++)
            {
                // Clear / mask test.
                if ((clear->clearMask & GL_DEPTH_BUFFER_BIT) == 0 || !clear->depthMask)
                    continue;

                tcu::IVec4 clearRect =
                    clear->useScissor ? clear->scissor : tcu::IVec4(0, 0, dst.getWidth(), dst.getHeight());

                // Intersection test.
                if (!de::inBounds(y, clearRect.y(), clearRect.y() + clearRect.w()))
                    continue;

                for (int x = clearRect.x(); x < clearRect.x() + clearRect.z(); x++)
                    rowAccess.setPixDepth(de::clamp(clear->clearDepth, 0.0f, 1.0f), x, 0);
            }

            // Map to colors.
            for (int x = 0; x < dst.getWidth(); x++)
            {
                float depth        = rowAccess.getPixDepth(x, 0);
                float step         = deFloatFloor(depth * (float)DEPTH_STEPS) / (float)(DEPTH_STEPS - 1);
                tcu::RGBA oldColor = dst.getPixel(x, y);
                tcu::RGBA newColor =
                    tcu::RGBA(oldColor.getRed(), oldColor.getGreen(),
                              deClamp32(deRoundFloatToInt32(step * 255.0f), 0, 255), oldColor.getAlpha());

                dst.setPixel(x, y, newColor);
            }
        }
    }

    if (m_testStencil)
    {
        // Simulated stencil buffer span.
        int stencilBits = renderTarget.getStencilBits();
        tcu::TextureLevel depthBufRow(getStencilFormat(stencilBits), dst.getWidth(), 1, 1);
        tcu::PixelBufferAccess rowAccess = depthBufRow.getAccess();
        uint32_t bufMask                 = (1u << stencilBits) - 1;

        for (int y = 0; y < dst.getHeight(); y++)
        {
            // Clear to default value.
            for (int x = 0; x < rowAccess.getWidth(); x++)
                rowAccess.setPixel(tcu::UVec4(0), x, 0);

            // Execute clears.
            for (vector<Clear>::const_iterator clear = clears.begin(); clear != clears.end(); clear++)
            {
                // Clear / mask test.
                if ((clear->clearMask & GL_STENCIL_BUFFER_BIT) == 0 || clear->stencilMask == 0)
                    continue;

                tcu::IVec4 clearRect =
                    clear->useScissor ? clear->scissor : tcu::IVec4(0, 0, dst.getWidth(), dst.getHeight());

                // Intersection test.
                if (!de::inBounds(y, clearRect.y(), clearRect.y() + clearRect.w()))
                    continue;

                for (int x = clearRect.x(); x < clearRect.x() + clearRect.z(); x++)
                {
                    uint32_t oldVal = rowAccess.getPixStencil(x, 0);
                    uint32_t newVal =
                        ((oldVal & ~clear->stencilMask) | (clear->clearStencil & clear->stencilMask)) & bufMask;
                    rowAccess.setPixStencil(newVal, x, 0);
                }
            }

            // Map to colors.
            for (int x = 0; x < dst.getWidth(); x++)
            {
                uint32_t stencil = rowAccess.getPixStencil(x, 0);
                float step =
                    (float)(stencil / ((1u << stencilBits) / (uint32_t)STENCIL_STEPS)) / (float)(STENCIL_STEPS - 1);
                tcu::RGBA oldColor = dst.getPixel(x, y);
                tcu::RGBA newColor = tcu::RGBA(oldColor.getRed(), deClamp32(deRoundFloatToInt32(step * 255.0f), 0, 255),
                                               oldColor.getBlue(), oldColor.getAlpha());

                dst.setPixel(x, y, newColor);
            }
        }
    }
}

DepthStencilClearTests::DepthStencilClearTests(Context &context)
    : TestCaseGroup(context, "depth_stencil_clear", "Depth and stencil clear tests")
{
}

void DepthStencilClearTests::init(void)
{
    //                                                                                    iters    clears    depth    stencil    scissor    masked
    addChild(new DepthStencilClearCase(m_context, "depth", "", 4, 2, true, false, false, false));
    addChild(new DepthStencilClearCase(m_context, "depth_scissored", "", 4, 16, true, false, true, false));
    addChild(new DepthStencilClearCase(m_context, "depth_scissored_masked", "", 4, 16, true, false, true, true));

    addChild(new DepthStencilClearCase(m_context, "stencil", "", 4, 2, false, true, false, false));
    addChild(new DepthStencilClearCase(m_context, "stencil_masked", "", 4, 8, false, true, false, true));
    addChild(new DepthStencilClearCase(m_context, "stencil_scissored", "", 4, 16, false, true, true, false));
    addChild(new DepthStencilClearCase(m_context, "stencil_scissored_masked", "", 4, 16, false, true, true, true));

    addChild(new DepthStencilClearCase(m_context, "depth_stencil", "", 4, 2, true, true, false, false));
    addChild(new DepthStencilClearCase(m_context, "depth_stencil_masked", "", 4, 8, true, true, false, true));
    addChild(new DepthStencilClearCase(m_context, "depth_stencil_scissored", "", 4, 16, true, true, true, false));
    addChild(new DepthStencilClearCase(m_context, "depth_stencil_scissored_masked", "", 4, 16, true, true, true, true));
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
