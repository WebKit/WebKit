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
 * \brief Blend tests.
 *//*--------------------------------------------------------------------*/

#include "es2fBlendTests.hpp"
#include "glsFragmentOpUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "gluStrUtil.hpp"
#include "tcuSurface.hpp"
#include "tcuImageCompare.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "deRandom.hpp"
#include "rrFragmentOperations.hpp"
#include "sglrReferenceUtils.hpp"

#include "glw.h"

#include <string>
#include <vector>

namespace deqp
{

using gls::FragmentOpUtil::IntegerQuad;
using gls::FragmentOpUtil::Quad;
using gls::FragmentOpUtil::QuadRenderer;
using gls::FragmentOpUtil::ReferenceQuadRenderer;
using glu::getBlendEquationName;
using glu::getBlendFactorName;
using std::string;
using std::vector;
using tcu::Surface;
using tcu::TestLog;
using tcu::TextureFormat;
using tcu::TextureLevel;
using tcu::UVec4;
using tcu::Vec4;

namespace gles2
{
namespace Functional
{

static const int MAX_VIEWPORT_WIDTH  = 64;
static const int MAX_VIEWPORT_HEIGHT = 64;

struct BlendParams
{
    GLenum equationRGB;
    GLenum srcFuncRGB;
    GLenum dstFuncRGB;
    GLenum equationAlpha;
    GLenum srcFuncAlpha;
    GLenum dstFuncAlpha;
    Vec4 blendColor;

    BlendParams(GLenum equationRGB_, GLenum srcFuncRGB_, GLenum dstFuncRGB_, GLenum equationAlpha_,
                GLenum srcFuncAlpha_, GLenum dstFuncAlpha_, Vec4 blendColor_)
        : equationRGB(equationRGB_)
        , srcFuncRGB(srcFuncRGB_)
        , dstFuncRGB(dstFuncRGB_)
        , equationAlpha(equationAlpha_)
        , srcFuncAlpha(srcFuncAlpha_)
        , dstFuncAlpha(dstFuncAlpha_)
        , blendColor(blendColor_)
    {
    }
};

class BlendCase : public TestCase
{
public:
    BlendCase(Context &context, const char *name, const char *desc, const vector<BlendParams> &paramSets);

    ~BlendCase(void);

    void init(void);
    void deinit(void);

    IterateResult iterate(void);

private:
    BlendCase(const BlendCase &other);
    BlendCase &operator=(const BlendCase &other);

    vector<BlendParams> m_paramSets;
    int m_curParamSetNdx;

    QuadRenderer *m_renderer;
    ReferenceQuadRenderer *m_referenceRenderer;
    TextureLevel *m_refColorBuffer;
    Quad m_firstQuad;
    Quad m_secondQuad;
    IntegerQuad m_firstQuadInt;
    IntegerQuad m_secondQuadInt;

    int m_viewportW;
    int m_viewportH;
};

BlendCase::BlendCase(Context &context, const char *name, const char *desc, const vector<BlendParams> &paramSets)
    : TestCase(context, name, desc)
    , m_paramSets(paramSets)
    , m_curParamSetNdx(0)
    , m_renderer(nullptr)
    , m_referenceRenderer(nullptr)
    , m_refColorBuffer(nullptr)
    , m_viewportW(0)
    , m_viewportH(0)
{
    DE_ASSERT(!m_paramSets.empty());
    for (int i = 0; i < (int)m_paramSets.size(); i++)
        DE_ASSERT(m_paramSets[i].dstFuncRGB != GL_SRC_ALPHA_SATURATE &&
                  m_paramSets[i].dstFuncAlpha != GL_SRC_ALPHA_SATURATE);
}

void BlendCase::init(void)
{
    bool useRGB = m_context.getRenderTarget().getPixelFormat().alphaBits == 0;

    static const Vec4 baseGradientColors[4] = {Vec4(0.0f, 0.5f, 1.0f, 0.5f), Vec4(0.5f, 0.0f, 0.5f, 1.0f),
                                               Vec4(0.5f, 1.0f, 0.5f, 0.0f), Vec4(1.0f, 0.5f, 0.0f, 0.5f)};

    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(m_firstQuad.color) == DE_LENGTH_OF_ARRAY(m_firstQuadInt.color));
    for (int i = 0; i < DE_LENGTH_OF_ARRAY(m_firstQuad.color); i++)
    {
        m_firstQuad.color[i]    = (baseGradientColors[i] - 0.5f) * 0.2f + 0.5f;
        m_firstQuadInt.color[i] = m_firstQuad.color[i];

        m_secondQuad.color[i]    = (Vec4(1.0f) - baseGradientColors[i] - 0.5f) * 1.0f + 0.5f;
        m_secondQuadInt.color[i] = m_secondQuad.color[i];
    }

    m_viewportW = de::min<int>(m_context.getRenderTarget().getWidth(), MAX_VIEWPORT_WIDTH);
    m_viewportH = de::min<int>(m_context.getRenderTarget().getHeight(), MAX_VIEWPORT_HEIGHT);

    m_firstQuadInt.posA  = tcu::IVec2(0, 0);
    m_secondQuadInt.posA = tcu::IVec2(0, 0);
    m_firstQuadInt.posB  = tcu::IVec2(m_viewportW - 1, m_viewportH - 1);
    m_secondQuadInt.posB = tcu::IVec2(m_viewportW - 1, m_viewportH - 1);

    DE_ASSERT(!m_renderer);
    DE_ASSERT(!m_referenceRenderer);
    DE_ASSERT(!m_refColorBuffer);

    m_renderer          = new QuadRenderer(m_context.getRenderContext(), glu::GLSL_VERSION_100_ES);
    m_referenceRenderer = new ReferenceQuadRenderer;
    m_refColorBuffer =
        new TextureLevel(TextureFormat(useRGB ? TextureFormat::RGB : TextureFormat::RGBA, TextureFormat::UNORM_INT8),
                         m_viewportW, m_viewportH);

    m_curParamSetNdx = 0;
}

BlendCase::~BlendCase(void)
{
    delete m_renderer;
    delete m_referenceRenderer;
    delete m_refColorBuffer;
}

void BlendCase::deinit(void)
{
    delete m_renderer;
    delete m_referenceRenderer;
    delete m_refColorBuffer;

    m_renderer          = nullptr;
    m_referenceRenderer = nullptr;
    m_refColorBuffer    = nullptr;
}

BlendCase::IterateResult BlendCase::iterate(void)
{
    de::Random rnd(deStringHash(getName()) ^ deInt32Hash(m_curParamSetNdx));
    int viewportX = rnd.getInt(0, m_context.getRenderTarget().getWidth() - m_viewportW);
    int viewportY = rnd.getInt(0, m_context.getRenderTarget().getHeight() - m_viewportH);
    tcu::Surface renderedImg(m_viewportW, m_viewportH);
    tcu::Surface referenceImg(m_viewportH, m_viewportH);
    TestLog &log(m_testCtx.getLog());
    const BlendParams &paramSet = m_paramSets[m_curParamSetNdx];
    rr::FragmentOperationState referenceState;

    // Log the blend parameters.

    log << TestLog::Message << "RGB equation = " << getBlendEquationName(paramSet.equationRGB) << TestLog::EndMessage;
    log << TestLog::Message << "RGB src func = " << getBlendFactorName(paramSet.srcFuncRGB) << TestLog::EndMessage;
    log << TestLog::Message << "RGB dst func = " << getBlendFactorName(paramSet.dstFuncRGB) << TestLog::EndMessage;
    log << TestLog::Message << "Alpha equation = " << getBlendEquationName(paramSet.equationAlpha)
        << TestLog::EndMessage;
    log << TestLog::Message << "Alpha src func = " << getBlendFactorName(paramSet.srcFuncAlpha) << TestLog::EndMessage;
    log << TestLog::Message << "Alpha dst func = " << getBlendFactorName(paramSet.dstFuncAlpha) << TestLog::EndMessage;
    log << TestLog::Message << "Blend color = (" << paramSet.blendColor.x() << ", " << paramSet.blendColor.y() << ", "
        << paramSet.blendColor.z() << ", " << paramSet.blendColor.w() << ")" << TestLog::EndMessage;

    // Set GL state.

    GLU_CHECK_CALL(glBlendEquationSeparate(paramSet.equationRGB, paramSet.equationAlpha));
    GLU_CHECK_CALL(
        glBlendFuncSeparate(paramSet.srcFuncRGB, paramSet.dstFuncRGB, paramSet.srcFuncAlpha, paramSet.dstFuncAlpha));
    GLU_CHECK_CALL(glBlendColor(paramSet.blendColor.x(), paramSet.blendColor.y(), paramSet.blendColor.z(),
                                paramSet.blendColor.w()));

    // Set reference state.

    referenceState.blendRGBState.equation = sglr::rr_util::mapGLBlendEquation(paramSet.equationRGB);
    referenceState.blendRGBState.srcFunc  = sglr::rr_util::mapGLBlendFunc(paramSet.srcFuncRGB);
    referenceState.blendRGBState.dstFunc  = sglr::rr_util::mapGLBlendFunc(paramSet.dstFuncRGB);
    referenceState.blendAState.equation   = sglr::rr_util::mapGLBlendEquation(paramSet.equationAlpha);
    referenceState.blendAState.srcFunc    = sglr::rr_util::mapGLBlendFunc(paramSet.srcFuncAlpha);
    referenceState.blendAState.dstFunc    = sglr::rr_util::mapGLBlendFunc(paramSet.dstFuncAlpha);
    referenceState.blendColor             = paramSet.blendColor;

    // Render with GL.

    glDisable(GL_BLEND);
    glViewport(viewportX, viewportY, m_viewportW, m_viewportH);
    m_renderer->render(m_firstQuad);
    glEnable(GL_BLEND);
    m_renderer->render(m_secondQuad);
    glFlush();

    // Render reference.

    const tcu::PixelBufferAccess nullAccess = tcu::PixelBufferAccess();

    referenceState.blendMode = rr::BLENDMODE_NONE;
    m_referenceRenderer->render(gls::FragmentOpUtil::getMultisampleAccess(m_refColorBuffer->getAccess()),
                                nullAccess /* no depth */, nullAccess /* no stencil */, m_firstQuadInt, referenceState);
    referenceState.blendMode = rr::BLENDMODE_STANDARD;
    m_referenceRenderer->render(gls::FragmentOpUtil::getMultisampleAccess(m_refColorBuffer->getAccess()),
                                nullAccess /* no depth */, nullAccess /* no stencil */, m_secondQuadInt,
                                referenceState);

    // Expand reference color buffer to RGBA8
    copy(referenceImg.getAccess(), m_refColorBuffer->getAccess());

    // Read GL image.

    glu::readPixels(m_context.getRenderContext(), viewportX, viewportY, renderedImg.getAccess());

    // Compare images.

    UVec4 compareThreshold =
        m_context.getRenderTarget().getPixelFormat().getColorThreshold().toIVec().asUint() * UVec4(5) / UVec4(2) +
        UVec4(
            3); // \note Non-scientific ad hoc formula. Need big threshold when few color bits; blending brings extra inaccuracy.

    bool comparePass = tcu::intThresholdCompare(m_testCtx.getLog(), "CompareResult", "Image Comparison Result",
                                                referenceImg.getAccess(), renderedImg.getAccess(), compareThreshold,
                                                tcu::COMPARE_LOG_RESULT);

    // Fail now if images don't match.

    if (!comparePass)
    {
        m_context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Image compare failed");
        return STOP;
    }

    // Continue if param sets still remain in m_paramSets; otherwise stop.

    m_curParamSetNdx++;

    if (m_curParamSetNdx < (int)m_paramSets.size())
        return CONTINUE;
    else
    {
        m_context.getTestContext().setTestResult(QP_TEST_RESULT_PASS, "Passed");
        return STOP;
    }
}

BlendTests::BlendTests(Context &context) : TestCaseGroup(context, "blend", "Blend tests")
{
}

BlendTests::~BlendTests(void)
{
}

void BlendTests::init(void)
{
    struct EnumGL
    {
        GLenum glValue;
        const char *nameStr;
    };

    static const EnumGL blendEquations[] = {
        {GL_FUNC_ADD, "add"}, {GL_FUNC_SUBTRACT, "subtract"}, {GL_FUNC_REVERSE_SUBTRACT, "reverse_subtract"}};

    static const EnumGL blendFunctions[] = {{GL_ZERO, "zero"},
                                            {GL_ONE, "one"},
                                            {GL_SRC_COLOR, "src_color"},
                                            {GL_ONE_MINUS_SRC_COLOR, "one_minus_src_color"},
                                            {GL_DST_COLOR, "dst_color"},
                                            {GL_ONE_MINUS_DST_COLOR, "one_minus_dst_color"},
                                            {GL_SRC_ALPHA, "src_alpha"},
                                            {GL_ONE_MINUS_SRC_ALPHA, "one_minus_src_alpha"},
                                            {GL_DST_ALPHA, "dst_alpha"},
                                            {GL_ONE_MINUS_DST_ALPHA, "one_minus_dst_alpha"},
                                            {GL_CONSTANT_COLOR, "constant_color"},
                                            {GL_ONE_MINUS_CONSTANT_COLOR, "one_minus_constant_color"},
                                            {GL_CONSTANT_ALPHA, "constant_alpha"},
                                            {GL_ONE_MINUS_CONSTANT_ALPHA, "one_minus_constant_alpha"},
                                            {GL_SRC_ALPHA_SATURATE, "src_alpha_saturate"}};

    const Vec4 defaultBlendColor(0.2f, 0.4f, 0.6f, 0.8f);

    // Test all blend equation, src blend function, dst blend function combinations. RGB and alpha modes are the same.

    {
        TestCaseGroup *group =
            new TestCaseGroup(m_context, "equation_src_func_dst_func", "Combinations of Blend Equations and Functions");
        addChild(group);

        for (int equationNdx = 0; equationNdx < DE_LENGTH_OF_ARRAY(blendEquations); equationNdx++)
            for (int srcFuncNdx = 0; srcFuncNdx < DE_LENGTH_OF_ARRAY(blendFunctions); srcFuncNdx++)
                for (int dstFuncNdx = 0; dstFuncNdx < DE_LENGTH_OF_ARRAY(blendFunctions); dstFuncNdx++)
                {
                    const EnumGL &eq  = blendEquations[equationNdx];
                    const EnumGL &src = blendFunctions[srcFuncNdx];
                    const EnumGL &dst = blendFunctions[dstFuncNdx];

                    if (dst.glValue == GL_SRC_ALPHA_SATURATE) // SRC_ALPHA_SATURATE is only valid for src func.
                        continue;

                    string name        = string("") + eq.nameStr + "_" + src.nameStr + "_" + dst.nameStr;
                    string description = string("") + "Equations " + getBlendEquationName(eq.glValue) + ", src funcs " +
                                         getBlendFactorName(src.glValue) + ", dst funcs " +
                                         getBlendFactorName(dst.glValue);

                    vector<BlendParams> paramSets;
                    paramSets.push_back(BlendParams(eq.glValue, src.glValue, dst.glValue, eq.glValue, src.glValue,
                                                    dst.glValue, defaultBlendColor));

                    group->addChild(new BlendCase(m_context, name.c_str(), description.c_str(), paramSets));
                }
    }

    // Test all RGB src, alpha src and RGB dst, alpha dst combinations. Equations are ADD.
    // \note For all RGB src, alpha src combinations, also test a couple of different RGBA dst functions, and vice versa.

    {
        TestCaseGroup *mainGroup =
            new TestCaseGroup(m_context, "rgb_func_alpha_func", "Combinations of RGB and Alpha Functions");
        addChild(mainGroup);
        TestCaseGroup *srcGroup = new TestCaseGroup(m_context, "src", "Source functions");
        TestCaseGroup *dstGroup = new TestCaseGroup(m_context, "dst", "Destination functions");
        mainGroup->addChild(srcGroup);
        mainGroup->addChild(dstGroup);

        for (int isDstI = 0; isDstI <= 1; isDstI++)
            for (int rgbFuncNdx = 0; rgbFuncNdx < DE_LENGTH_OF_ARRAY(blendFunctions); rgbFuncNdx++)
                for (int alphaFuncNdx = 0; alphaFuncNdx < DE_LENGTH_OF_ARRAY(blendFunctions); alphaFuncNdx++)
                {
                    bool isSrc              = isDstI == 0;
                    TestCaseGroup *curGroup = isSrc ? srcGroup : dstGroup;
                    const EnumGL &funcRGB   = blendFunctions[rgbFuncNdx];
                    const EnumGL &funcAlpha = blendFunctions[alphaFuncNdx];
                    const char *dstOrSrcStr = isSrc ? "src" : "dst";

                    if (!isSrc &&
                        (funcRGB.glValue == GL_SRC_ALPHA_SATURATE ||
                         funcAlpha.glValue == GL_SRC_ALPHA_SATURATE)) // SRC_ALPHA_SATURATE is only valid for src func.
                        continue;

                    string name        = string("") + funcRGB.nameStr + "_" + funcAlpha.nameStr;
                    string description = string("") + "RGB " + dstOrSrcStr + " func " +
                                         getBlendFactorName(funcRGB.glValue) + ", alpha " + dstOrSrcStr + " func " +
                                         getBlendFactorName(funcAlpha.glValue);

                    // First, make param sets as if this was a src case.

                    vector<BlendParams> paramSets;
                    paramSets.push_back(BlendParams(GL_FUNC_ADD, funcRGB.glValue, GL_ONE, GL_FUNC_ADD,
                                                    funcAlpha.glValue, GL_ONE, defaultBlendColor));
                    paramSets.push_back(BlendParams(GL_FUNC_ADD, funcRGB.glValue, GL_ZERO, GL_FUNC_ADD,
                                                    funcAlpha.glValue, GL_ZERO, defaultBlendColor));
                    paramSets.push_back(BlendParams(GL_FUNC_ADD, funcRGB.glValue, GL_SRC_COLOR, GL_FUNC_ADD,
                                                    funcAlpha.glValue, GL_SRC_COLOR, defaultBlendColor));
                    paramSets.push_back(BlendParams(GL_FUNC_ADD, funcRGB.glValue, GL_DST_COLOR, GL_FUNC_ADD,
                                                    funcAlpha.glValue, GL_DST_COLOR, defaultBlendColor));

                    // Swap src and dst params if this is a dst case.

                    if (!isSrc)
                    {
                        for (int i = 0; i < (int)paramSets.size(); i++)
                        {
                            std::swap(paramSets[i].srcFuncRGB, paramSets[i].dstFuncRGB);
                            std::swap(paramSets[i].srcFuncAlpha, paramSets[i].dstFuncAlpha);
                        }
                    }

                    curGroup->addChild(new BlendCase(m_context, name.c_str(), description.c_str(), paramSets));
                }
    }

    // Test all RGB and alpha equation combinations. Src and dst funcs are ONE for both.

    {
        TestCaseGroup *group = new TestCaseGroup(m_context, "rgb_equation_alpha_equation",
                                                 "Combinations of RGB and Alpha Equation Combinations");
        addChild(group);

        for (int equationRGBNdx = 0; equationRGBNdx < DE_LENGTH_OF_ARRAY(blendEquations); equationRGBNdx++)
            for (int equationAlphaNdx = 0; equationAlphaNdx < DE_LENGTH_OF_ARRAY(blendEquations); equationAlphaNdx++)
            {
                const EnumGL &eqRGB   = blendEquations[equationRGBNdx];
                const EnumGL &eqAlpha = blendEquations[equationAlphaNdx];

                string name        = string("") + eqRGB.nameStr + "_" + eqAlpha.nameStr;
                string description = string("") + "RGB equation " + getBlendEquationName(eqRGB.glValue) +
                                     ", alpha equation " + getBlendEquationName(eqAlpha.glValue);

                vector<BlendParams> paramSets;
                paramSets.push_back(
                    BlendParams(eqRGB.glValue, GL_ONE, GL_ONE, eqAlpha.glValue, GL_ONE, GL_ONE, defaultBlendColor));

                group->addChild(new BlendCase(m_context, name.c_str(), description.c_str(), paramSets));
            }
    }
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
