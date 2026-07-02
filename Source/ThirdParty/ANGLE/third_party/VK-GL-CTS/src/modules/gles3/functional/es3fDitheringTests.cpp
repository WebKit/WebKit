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
 * \brief Dithering tests.
 *//*--------------------------------------------------------------------*/

#include "es3fDitheringTests.hpp"
#include "gluRenderContext.hpp"
#include "gluDefs.hpp"
#include "glsFragmentOpUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuRGBA.hpp"
#include "tcuVector.hpp"
#include "tcuPixelFormat.hpp"
#include "tcuTestLog.hpp"
#include "tcuSurface.hpp"
#include "tcuCommandLine.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deString.h"
#include "deMath.h"

#include <string>
#include <algorithm>

#include "glw.h"

namespace deqp
{

using de::Random;
using gls::FragmentOpUtil::Quad;
using gls::FragmentOpUtil::QuadRenderer;
using std::string;
using std::vector;
using tcu::IVec4;
using tcu::PixelFormat;
using tcu::Surface;
using tcu::TestLog;
using tcu::Vec4;

namespace gles3
{
namespace Functional
{

static const char *const s_channelNames[4] = {"red", "green", "blue", "alpha"};

static inline IVec4 pixelFormatToIVec4(const PixelFormat &format)
{
    return IVec4(format.redBits, format.greenBits, format.blueBits, format.alphaBits);
}

template <typename T>
static inline string choiceListStr(const vector<T> &choices)
{
    string result;
    for (int i = 0; i < (int)choices.size(); i++)
    {
        if (i == (int)choices.size() - 1)
            result += " or ";
        else if (i > 0)
            result += ", ";
        result += de::toString(choices[i]);
    }
    return result;
}

class DitheringCase : public tcu::TestCase
{
public:
    enum PatternType
    {
        PATTERNTYPE_GRADIENT = 0,
        PATTERNTYPE_UNICOLORED_QUAD,

        PATTERNTYPE_LAST
    };

    DitheringCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name, const char *description,
                  bool isEnabled, PatternType patternType, const tcu::Vec4 &color);
    ~DitheringCase(void);

    IterateResult iterate(void);
    void init(void);
    void deinit(void);

    static const char *getPatternTypeName(PatternType type);

private:
    bool checkColor(const tcu::Vec4 &inputClr, const tcu::RGBA &renderedClr, bool logErrors, const bool incTol) const;

    bool drawAndCheckGradient(bool isVerticallyIncreasing, const tcu::Vec4 &highColor) const;
    bool drawAndCheckUnicoloredQuad(const tcu::Vec4 &color) const;

    const glu::RenderContext &m_renderCtx;

    const bool m_ditheringEnabled;
    const PatternType m_patternType;
    const tcu::Vec4 m_color;

    const tcu::PixelFormat m_renderFormat;

    const QuadRenderer *m_renderer;
    int m_iteration;
};

const char *DitheringCase::getPatternTypeName(const PatternType type)
{
    switch (type)
    {
    case PATTERNTYPE_GRADIENT:
        return "gradient";
    case PATTERNTYPE_UNICOLORED_QUAD:
        return "unicolored_quad";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

DitheringCase::DitheringCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *const name,
                             const char *const description, const bool ditheringEnabled, const PatternType patternType,
                             const Vec4 &color)
    : TestCase(testCtx, name, description)
    , m_renderCtx(renderCtx)
    , m_ditheringEnabled(ditheringEnabled)
    , m_patternType(patternType)
    , m_color(color)
    , m_renderFormat(renderCtx.getRenderTarget().getPixelFormat())
    , m_renderer(nullptr)
    , m_iteration(0)
{
}

DitheringCase::~DitheringCase(void)
{
    DitheringCase::deinit();
}

void DitheringCase::init(void)
{
    DE_ASSERT(!m_renderer);
    m_renderer  = new QuadRenderer(m_renderCtx, glu::GLSL_VERSION_300_ES);
    m_iteration = 0;
}

void DitheringCase::deinit(void)
{
    delete m_renderer;
    m_renderer = nullptr;
}

bool DitheringCase::checkColor(const Vec4 &inputClr, const tcu::RGBA &renderedClr, const bool logErrors,
                               const bool incTol) const
{
    const IVec4 channelBits = pixelFormatToIVec4(m_renderFormat);
    bool allChannelsOk      = true;

    for (int chanNdx = 0; chanNdx < 4; chanNdx++)
    {
        if (channelBits[chanNdx] == 0)
            continue;

        const int channelMax         = (1 << channelBits[chanNdx]) - 1;
        const float scaledInput      = inputClr[chanNdx] * (float)channelMax;
        const bool useRoundingMargin = deFloatAbs(scaledInput - deFloatRound(scaledInput)) < 0.0001f;
        vector<int> channelChoices;

        channelChoices.push_back(de::min(channelMax, (int)deFloatCeil(scaledInput)));
        channelChoices.push_back(de::max(0, (int)deFloatCeil(scaledInput) - 1));
        // Allow for more tolerance for small dimension render targets
        if (incTol)
        {
            channelChoices.push_back(de::max(0, (int)deFloatCeil(scaledInput) - 2));
            channelChoices.push_back(de::max(0, (int)deFloatCeil(scaledInput) + 1));
        }

        // If the input color results in a scaled value that is very close to an integer, account for a little bit of possible inaccuracy.
        if (useRoundingMargin)
        {
            if (scaledInput > deFloatRound(scaledInput))
                channelChoices.push_back((int)deFloatCeil(scaledInput) - 2);
            else
                channelChoices.push_back((int)deFloatCeil(scaledInput) + 1);
        }

        std::sort(channelChoices.begin(), channelChoices.end());

        {
            const int renderedClrInFormat =
                (int)deFloatRound((float)(renderedClr.toIVec()[chanNdx] * channelMax) / 255.0f);
            bool goodChannel = false;

            for (int i = 0; i < (int)channelChoices.size(); i++)
            {
                if (renderedClrInFormat == channelChoices[i])
                {
                    goodChannel = true;
                    break;
                }
            }

            if (!goodChannel)
            {
                if (logErrors)
                {
                    m_testCtx.getLog() << TestLog::Message << "Failure: " << channelBits[chanNdx] << "-bit "
                                       << s_channelNames[chanNdx] << " channel is " << renderedClrInFormat
                                       << ", should be " << choiceListStr(channelChoices)
                                       << " (corresponding fragment color channel is " << inputClr[chanNdx] << ")"
                                       << TestLog::EndMessage << TestLog::Message << "Note: " << inputClr[chanNdx]
                                       << " * (" << channelMax + 1 << "-1) = " << scaledInput << TestLog::EndMessage;

                    if (useRoundingMargin)
                    {
                        m_testCtx.getLog() << TestLog::Message
                                           << "Note: one extra color candidate was allowed because "
                                              "fragmentColorChannel * (2^bits-1) is close to an integer"
                                           << TestLog::EndMessage;
                    }
                }

                allChannelsOk = false;
            }
        }
    }

    return allChannelsOk;
}

bool DitheringCase::drawAndCheckGradient(const bool isVerticallyIncreasing, const Vec4 &highColor) const
{
    TestLog &log = m_testCtx.getLog();
    Random rnd(deStringHash(getName()));
    const int maxViewportWid = 256;
    const int maxViewportHei = 256;
    const int viewportWid    = de::min(m_renderCtx.getRenderTarget().getWidth(), maxViewportWid);
    const int viewportHei    = de::min(m_renderCtx.getRenderTarget().getHeight(), maxViewportHei);
    const int viewportX      = rnd.getInt(0, m_renderCtx.getRenderTarget().getWidth() - viewportWid);
    const int viewportY      = rnd.getInt(0, m_renderCtx.getRenderTarget().getHeight() - viewportHei);
    const Vec4 quadClr0(0.0f, 0.0f, 0.0f, 0.0f);
    const Vec4 &quadClr1 = highColor;
    Quad quad;
    Surface renderedImg(viewportWid, viewportHei);

    GLU_CHECK_CALL(glViewport(viewportX, viewportY, viewportWid, viewportHei));

    log << TestLog::Message << "Dithering is " << (m_ditheringEnabled ? "enabled" : "disabled") << TestLog::EndMessage;

    if (m_ditheringEnabled)
        GLU_CHECK_CALL(glEnable(GL_DITHER));
    else
        GLU_CHECK_CALL(glDisable(GL_DITHER));

    log << TestLog::Message << "Drawing a " << (isVerticallyIncreasing ? "vertically" : "horizontally")
        << " increasing gradient" << TestLog::EndMessage;

    quad.color[0] = quadClr0;
    quad.color[1] = isVerticallyIncreasing ? quadClr1 : quadClr0;
    quad.color[2] = isVerticallyIncreasing ? quadClr0 : quadClr1;
    quad.color[3] = quadClr1;

    m_renderer->render(quad);

    glu::readPixels(m_renderCtx, viewportX, viewportY, renderedImg.getAccess());
    GLU_CHECK_MSG("glReadPixels()");

    log << TestLog::Image(isVerticallyIncreasing ? "VerGradient" : "HorGradient",
                          isVerticallyIncreasing ? "Vertical gradient" : "Horizontal gradient", renderedImg);

    // Validate, at each pixel, that each color channel is one of its two allowed values.

    {
        Surface errorMask(viewportWid, viewportHei);
        bool colorChoicesOk = true;

        for (int y = 0; y < renderedImg.getHeight(); y++)
        {
            for (int x = 0; x < renderedImg.getWidth(); x++)
            {
                const float inputF = ((float)(isVerticallyIncreasing ? y : x) + 0.5f) /
                                     (float)(isVerticallyIncreasing ? renderedImg.getHeight() : renderedImg.getWidth());
                const Vec4 inputClr = (1.0f - inputF) * quadClr0 + inputF * quadClr1;
                const bool increaseTol =
                    ((renderedImg.getWidth() < 300) || (renderedImg.getHeight() < 300)) ? true : false;

                if (!checkColor(inputClr, renderedImg.getPixel(x, y), colorChoicesOk, increaseTol))
                {
                    errorMask.setPixel(x, y, tcu::RGBA::red());

                    if (colorChoicesOk)
                    {
                        log << TestLog::Message << "First failure at pixel (" << x << ", " << y
                            << ") (not printing further errors)" << TestLog::EndMessage;
                        colorChoicesOk = false;
                    }
                }
                else
                    errorMask.setPixel(x, y, tcu::RGBA::green());
            }
        }

        if (!colorChoicesOk)
        {
            log << TestLog::Image("ColorChoiceErrorMask", "Error mask for color choices", errorMask);
            return false;
        }
    }

    // When dithering is disabled, the color selection must be coordinate-independent - i.e. the colors must be constant in the gradient's constant direction.

    if (!m_ditheringEnabled)
    {
        const int increasingDirectionSize = isVerticallyIncreasing ? renderedImg.getHeight() : renderedImg.getWidth();
        const int constantDirectionSize   = isVerticallyIncreasing ? renderedImg.getWidth() : renderedImg.getHeight();

        for (int incrPos = 0; incrPos < increasingDirectionSize; incrPos++)
        {
            bool colorHasChanged = false;
            tcu::RGBA prevConstantDirectionPix;

            for (int constPos = 0; constPos < constantDirectionSize; constPos++)
            {
                const int x         = isVerticallyIncreasing ? constPos : incrPos;
                const int y         = isVerticallyIncreasing ? incrPos : constPos;
                const tcu::RGBA clr = renderedImg.getPixel(x, y);

                if (constPos > 0 && clr != prevConstantDirectionPix)
                {
                    if (colorHasChanged)
                    {
                        log << TestLog::Message << "Failure: colors should be constant per "
                            << (isVerticallyIncreasing ? "row" : "column")
                            << " (since dithering is disabled), but the color at position (" << x << ", " << y
                            << ") is " << clr << " and does not equal the color at ("
                            << (isVerticallyIncreasing ? x - 1 : x) << ", " << (isVerticallyIncreasing ? y : y - 1)
                            << "), which is " << prevConstantDirectionPix << TestLog::EndMessage;

                        return false;
                    }
                    else
                        colorHasChanged = true;
                }

                prevConstantDirectionPix = clr;
            }
        }
    }

    return true;
}

bool DitheringCase::drawAndCheckUnicoloredQuad(const Vec4 &quadColor) const
{
    TestLog &log = m_testCtx.getLog();
    Random rnd(deStringHash(getName()));
    const int maxViewportWid = 32;
    const int maxViewportHei = 32;
    const int viewportWid    = de::min(m_renderCtx.getRenderTarget().getWidth(), maxViewportWid);
    const int viewportHei    = de::min(m_renderCtx.getRenderTarget().getHeight(), maxViewportHei);
    const int viewportX      = rnd.getInt(0, m_renderCtx.getRenderTarget().getWidth() - viewportWid);
    const int viewportY      = rnd.getInt(0, m_renderCtx.getRenderTarget().getHeight() - viewportHei);
    Quad quad;
    Surface renderedImg(viewportWid, viewportHei);

    GLU_CHECK_CALL(glViewport(viewportX, viewportY, viewportWid, viewportHei));

    log << TestLog::Message << "Dithering is " << (m_ditheringEnabled ? "enabled" : "disabled") << TestLog::EndMessage;

    if (m_ditheringEnabled)
        GLU_CHECK_CALL(glEnable(GL_DITHER));
    else
        GLU_CHECK_CALL(glDisable(GL_DITHER));

    log << TestLog::Message << "Drawing an unicolored quad with color " << quadColor << TestLog::EndMessage;

    quad.color[0] = quadColor;
    quad.color[1] = quadColor;
    quad.color[2] = quadColor;
    quad.color[3] = quadColor;

    m_renderer->render(quad);

    glu::readPixels(m_renderCtx, viewportX, viewportY, renderedImg.getAccess());
    GLU_CHECK_MSG("glReadPixels()");

    log << TestLog::Image(("Quad" + de::toString(m_iteration)).c_str(), ("Quad " + de::toString(m_iteration)).c_str(),
                          renderedImg);

    // Validate, at each pixel, that each color channel is one of its two allowed values.

    {
        Surface errorMask(viewportWid, viewportHei);
        bool colorChoicesOk = true;

        for (int y = 0; y < renderedImg.getHeight(); y++)
        {
            for (int x = 0; x < renderedImg.getWidth(); x++)
            {
                if (!checkColor(quadColor, renderedImg.getPixel(x, y), colorChoicesOk, false))
                {
                    errorMask.setPixel(x, y, tcu::RGBA::red());

                    if (colorChoicesOk)
                    {
                        log << TestLog::Message << "First failure at pixel (" << x << ", " << y
                            << ") (not printing further errors)" << TestLog::EndMessage;
                        colorChoicesOk = false;
                    }
                }
                else
                    errorMask.setPixel(x, y, tcu::RGBA::green());
            }
        }

        if (!colorChoicesOk)
        {
            log << TestLog::Image("ColorChoiceErrorMask", "Error mask for color choices", errorMask);
            return false;
        }
    }

    // When dithering is disabled, the color selection must be coordinate-independent - i.e. the entire rendered image must be unicolored.

    if (!m_ditheringEnabled)
    {
        const tcu::RGBA renderedClr00 = renderedImg.getPixel(0, 0);

        for (int y = 0; y < renderedImg.getHeight(); y++)
        {
            for (int x = 0; x < renderedImg.getWidth(); x++)
            {
                const tcu::RGBA curClr = renderedImg.getPixel(x, y);

                if (curClr != renderedClr00)
                {
                    log << TestLog::Message << "Failure: color at (" << x << ", " << y << ") is " << curClr
                        << " and does not equal the color at (0, 0), which is " << renderedClr00 << TestLog::EndMessage;

                    return false;
                }
            }
        }
    }

    return true;
}

DitheringCase::IterateResult DitheringCase::iterate(void)
{
    if (m_patternType == PATTERNTYPE_GRADIENT)
    {
        // Draw horizontal and vertical gradients.

        DE_ASSERT(m_iteration < 2);

        const bool success = drawAndCheckGradient(m_iteration == 1, m_color);

        if (!success)
        {
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
            return STOP;
        }

        if (m_iteration == 1)
        {
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
            return STOP;
        }
    }
    else if (m_patternType == PATTERNTYPE_UNICOLORED_QUAD)
    {
        const int numQuads = m_testCtx.getCommandLine().getTestIterationCount() > 0 ?
                                 m_testCtx.getCommandLine().getTestIterationCount() :
                                 30;

        DE_ASSERT(m_iteration < numQuads);

        const Vec4 quadColor = (float)m_iteration / (float)(numQuads - 1) * m_color;
        const bool success   = drawAndCheckUnicoloredQuad(quadColor);

        if (!success)
        {
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
            return STOP;
        }

        if (m_iteration == numQuads - 1)
        {
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
            return STOP;
        }
    }
    else
        DE_ASSERT(false);

    m_iteration++;

    return CONTINUE;
}

DitheringTests::DitheringTests(Context &context) : TestCaseGroup(context, "dither", "Dithering tests")
{
}

DitheringTests::~DitheringTests(void)
{
}

void DitheringTests::init(void)
{
    static const struct
    {
        const char *name;
        Vec4 color;
    } caseColors[] = {{"white", Vec4(1.0f, 1.0f, 1.0f, 1.0f)},
                      {"red", Vec4(1.0f, 0.0f, 0.0f, 1.0f)},
                      {"green", Vec4(0.0f, 1.0f, 0.0f, 1.0f)},
                      {"blue", Vec4(0.0f, 0.0f, 1.0f, 1.0f)},
                      {"alpha", Vec4(0.0f, 0.0f, 0.0f, 1.0f)}};

    for (int ditheringEnabledI = 0; ditheringEnabledI <= 1; ditheringEnabledI++)
    {
        const bool ditheringEnabled = ditheringEnabledI != 0;
        TestCaseGroup *const group  = new TestCaseGroup(m_context, ditheringEnabled ? "enabled" : "disabled", "");
        addChild(group);

        for (int patternTypeI = 0; patternTypeI < DitheringCase::PATTERNTYPE_LAST; patternTypeI++)
        {
            for (int caseColorNdx = 0; caseColorNdx < DE_LENGTH_OF_ARRAY(caseColors); caseColorNdx++)
            {
                const DitheringCase::PatternType patternType = (DitheringCase::PatternType)patternTypeI;
                const string caseName =
                    string("") + DitheringCase::getPatternTypeName(patternType) + "_" + caseColors[caseColorNdx].name;

                group->addChild(new DitheringCase(m_context.getTestContext(), m_context.getRenderContext(),
                                                  caseName.c_str(), "", ditheringEnabled, patternType,
                                                  caseColors[caseColorNdx].color));
            }
        }
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
