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
 * \brief Prerequisite tests.
 *//*--------------------------------------------------------------------*/

#include "es3fPrerequisiteTests.hpp"
#include "deRandom.h"
#include "tcuRGBA.hpp"
#include "tcuSurface.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuRenderTarget.hpp"
#include "gluPixelTransfer.hpp"
#include "gluStateReset.hpp"

#include "glw.h"

using tcu::RGBA;
using tcu::Surface;
using tcu::TestLog;

namespace deqp
{
namespace gles3
{
namespace Functional
{

class StateResetCase : public TestCase
{
public:
    StateResetCase(Context &context);
    virtual ~StateResetCase(void);
    virtual TestCase::IterateResult iterate(void);
};

StateResetCase::StateResetCase(Context &context) : TestCase(context, "state_reset", "State Reset Test")
{
}

StateResetCase::~StateResetCase(void)
{
}

TestCase::IterateResult StateResetCase::iterate(void)
{
    try
    {
        glu::resetState(m_context.getRenderContext(), m_context.getContextInfo());
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    }
    catch (const tcu::TestError &e)
    {
        m_testCtx.getLog() << e;
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
    }

    return TestCase::STOP;
}

class ClearColorCase : public TestCase
{
public:
    ClearColorCase(Context &context);
    virtual ~ClearColorCase(void);
    virtual TestCase::IterateResult iterate(void);

private:
    RGBA m_clearColor;
    int m_numIters;
    int m_curIter;
};

ClearColorCase::ClearColorCase(Context &context)
    : TestCase(context, "clear_color", "glClearColor test")
    , m_numIters(10)
    , m_curIter(0)
{
}

ClearColorCase::~ClearColorCase(void)
{
}

TestCase::IterateResult ClearColorCase::iterate(void)
{
    int r = 0;
    int g = 0;
    int b = 0;
    int a = 255;

    switch (m_curIter)
    {
    case 0:
        // Black, skip
        break;
    case 1:
        r = 255;
        g = 255;
        b = 255;
        break;
    case 2:
        r = 255;
        break;
    case 3:
        g = 255;
        break;
    case 4:
        b = 255;
        break;
    default:
        deRandom rnd;
        deRandom_init(&rnd, deInt32Hash(m_curIter));
        r = (int)(deRandom_getUint32(&rnd) & 0xFF);
        g = (int)(deRandom_getUint32(&rnd) & 0xFF);
        b = (int)(deRandom_getUint32(&rnd) & 0xFF);
        a = (int)(deRandom_getUint32(&rnd) & 0xFF);
        break;
    }

    glClearColor(float(r) / 255.0f, float(g) / 255.0f, float(b) / 255.0f, float(a) / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    GLU_CHECK_MSG("CLES2 ClearColor failed.");

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

    return (++m_curIter < m_numIters) ? CONTINUE : STOP;
}

class ReadPixelsCase : public TestCase
{
public:
    ReadPixelsCase(Context &context);
    virtual ~ReadPixelsCase(void);
    virtual TestCase::IterateResult iterate(void);

private:
    int m_numIters;
    int m_curIter;
};

ReadPixelsCase::ReadPixelsCase(Context &context)
    : TestCase(context, "read_pixels", "Read pixels test")
    , m_numIters(20)
    , m_curIter(0)
{
}

ReadPixelsCase::~ReadPixelsCase(void)
{
}

TestCase::IterateResult ReadPixelsCase::iterate(void)
{
    const tcu::RenderTarget &renderTarget = m_context.getRenderTarget();
    tcu::PixelFormat pixelFormat          = renderTarget.getPixelFormat();
    int targetWidth                       = renderTarget.getWidth();
    int targetHeight                      = renderTarget.getHeight();
    int x                                 = 0;
    int y                                 = 0;
    int imageWidth                        = 0;
    int imageHeight                       = 0;

    deRandom rnd;
    deRandom_init(&rnd, deInt32Hash(m_curIter));

    switch (m_curIter)
    {
    case 0:
        // Fullscreen
        x           = 0;
        y           = 0;
        imageWidth  = targetWidth;
        imageHeight = targetHeight;
        break;
    case 1:
        // Upper left corner
        x           = 0;
        y           = 0;
        imageWidth  = targetWidth / 2;
        imageHeight = targetHeight / 2;
        break;
    case 2:
        // Lower right corner
        x           = targetWidth / 2;
        y           = targetHeight / 2;
        imageWidth  = targetWidth - x;
        imageHeight = targetHeight - y;
        break;
    default:
        x           = deRandom_getUint32(&rnd) % (targetWidth - 1);
        y           = deRandom_getUint32(&rnd) % (targetHeight - 1);
        imageWidth  = 1 + (deRandom_getUint32(&rnd) % (targetWidth - x - 1));
        imageHeight = 1 + (deRandom_getUint32(&rnd) % (targetHeight - y - 1));
        break;
    }

    Surface resImage(imageWidth, imageHeight);
    Surface refImage(imageWidth, imageHeight);
    Surface diffImage(imageWidth, imageHeight);

    int r = (int)(deRandom_getUint32(&rnd) & 0xFF);
    int g = (int)(deRandom_getUint32(&rnd) & 0xFF);
    int b = (int)(deRandom_getUint32(&rnd) & 0xFF);

    tcu::clear(refImage.getAccess(), tcu::IVec4(r, g, b, 255));
    glClearColor(float(r) / 255.0f, float(g) / 255.0f, float(b) / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glu::readPixels(m_context.getRenderContext(), x, y, resImage.getAccess());
    GLU_CHECK_MSG("glReadPixels() failed.");

    RGBA colorThreshold = pixelFormat.getColorThreshold();
    RGBA matchColor(0, 255, 0, 255);
    RGBA diffColor(255, 0, 0, 255);
    bool isImageOk = true;

    for (int j = 0; j < imageHeight; j++)
    {
        for (int i = 0; i < imageWidth; i++)
        {
            RGBA resRGBA   = resImage.getPixel(i, j);
            RGBA refRGBA   = refImage.getPixel(i, j);
            bool isPixelOk = compareThreshold(refRGBA, resRGBA, colorThreshold);
            diffImage.setPixel(i, j, isPixelOk ? matchColor : diffColor);

            isImageOk = isImageOk && isPixelOk;
        }
    }

    if (isImageOk)
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    else
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");

        m_testCtx.getLog() << TestLog::ImageSet("Result", "Resulting framebuffer")
                           << TestLog::Image("Result", "Resulting framebuffer", resImage)
                           << TestLog::Image("Reference", "Reference image", refImage)
                           << TestLog::Image("DiffMask", "Failing pixels", diffImage) << TestLog::EndImageSet;
    }

    return (++m_curIter < m_numIters) ? CONTINUE : STOP;
}

PrerequisiteTests::PrerequisiteTests(Context &context)
    : TestCaseGroup(context, "prerequisite", "Prerequisite Test Cases")
{
}

PrerequisiteTests::~PrerequisiteTests(void)
{
}

void PrerequisiteTests::init(void)
{
    addChild(new StateResetCase(m_context));
    addChild(new ClearColorCase(m_context));
    addChild(new ReadPixelsCase(m_context));
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
