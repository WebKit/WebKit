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
 * \brief Screen clearing test.
 *//*--------------------------------------------------------------------*/

#include "es3fColorClearTest.hpp"
#include "tcuRGBA.hpp"
#include "tcuSurface.hpp"
#include "tcuTestLog.hpp"
#include "gluRenderContext.hpp"
#include "gluPixelTransfer.hpp"
#include "tcuVector.hpp"
#include "tcuRenderTarget.hpp"
#include "deRandom.hpp"
#include "deInt32.h"

#include "glwFunctions.hpp"
#include "glwEnums.hpp"

#include <vector>

using tcu::RGBA;
using tcu::Surface;
using tcu::TestLog;

using namespace std;

namespace deqp
{
namespace gles3
{
namespace Functional
{

class ColorClearCase : public TestCase
{
public:
    ColorClearCase(Context &context, const char *name, int numIters, int numClearsMin, int numClearsMax, bool testAlpha,
                   bool testScissoring, bool testColorMasks, bool firstClearFull)
        : TestCase(context, name, name)
        , m_numIters(numIters)
        , m_numClearsMin(numClearsMin)
        , m_numClearsMax(numClearsMax)
        , m_testAlpha(testAlpha)
        , m_testScissoring(testScissoring)
        , m_testColorMasks(testColorMasks)
        , m_firstClearFull(firstClearFull)
    {
        m_curIter = 0;
    }

    virtual ~ColorClearCase(void)
    {
    }

    virtual IterateResult iterate(void);

private:
    const int m_numIters;
    const int m_numClearsMin;
    const int m_numClearsMax;
    const bool m_testAlpha;
    const bool m_testScissoring;
    const bool m_testColorMasks;
    const bool m_firstClearFull;

    int m_curIter;
};

class ClearInfo
{
public:
    ClearInfo(const tcu::IVec4 &rect, uint8_t colorMask, tcu::RGBA color)
        : m_rect(rect)
        , m_colorMask(colorMask)
        , m_color(color)
    {
    }

    tcu::IVec4 m_rect;
    uint8_t m_colorMask;
    tcu::RGBA m_color;
};

TestCase::IterateResult ColorClearCase::iterate(void)
{
    TestLog &log                          = m_testCtx.getLog();
    const glw::Functions &gl              = m_context.getRenderContext().getFunctions();
    const tcu::RenderTarget &renderTarget = m_context.getRenderTarget();
    const tcu::PixelFormat &pixelFormat   = renderTarget.getPixelFormat();
    const int targetWidth                 = renderTarget.getWidth();
    const int targetHeight                = renderTarget.getHeight();
    const int numPixels                   = targetWidth * targetHeight;

    de::Random rnd(deInt32Hash(m_curIter));
    vector<uint8_t> pixelKnownChannelMask(numPixels, 0);
    Surface refImage(targetWidth, targetHeight);
    Surface resImage(targetWidth, targetHeight);
    Surface diffImage(targetWidth, targetHeight);
    int numClears = rnd.getUint32() % (m_numClearsMax + 1 - m_numClearsMin) + m_numClearsMin;
    std::vector<ClearInfo> clearOps;

    if (m_testScissoring)
        gl.enable(GL_SCISSOR_TEST);

    for (int clearNdx = 0; clearNdx < numClears; clearNdx++)
    {
        // Rectangle.
        int clearX;
        int clearY;
        int clearWidth;
        int clearHeight;
        if (!m_testScissoring || (clearNdx == 0 && m_firstClearFull))
        {
            clearX      = 0;
            clearY      = 0;
            clearWidth  = targetWidth;
            clearHeight = targetHeight;
        }
        else
        {
            clearX      = (rnd.getUint32() % (2 * targetWidth)) - targetWidth;
            clearY      = (rnd.getUint32() % (2 * targetHeight)) - targetHeight;
            clearWidth  = (rnd.getUint32() % targetWidth);
            clearHeight = (rnd.getUint32() % targetHeight);
        }
        gl.scissor(clearX, clearY, clearWidth, clearHeight);

        // Color.
        int r = (int)(rnd.getUint32() & 0xFF);
        int g = (int)(rnd.getUint32() & 0xFF);
        int b = (int)(rnd.getUint32() & 0xFF);
        int a = m_testAlpha ? (int)(rnd.getUint32() & 0xFF) : 0xFF;
        RGBA clearCol(r, g, b, a);
        gl.clearColor(float(r) / 255.0f, float(g) / 255.0f, float(b) / 255.0f, float(a) / 255.0f);

        // Mask.
        uint8_t clearMask;
        if (!m_testColorMasks || (clearNdx == 0 && m_firstClearFull))
            clearMask = 0xF;
        else
            clearMask = (rnd.getUint32() & 0xF);
        gl.colorMask((clearMask & 0x1) != 0, (clearMask & 0x2) != 0, (clearMask & 0x4) != 0, (clearMask & 0x8) != 0);

        // Clear & store op.
        gl.clear(GL_COLOR_BUFFER_BIT);
        clearOps.push_back(ClearInfo(tcu::IVec4(clearX, clearY, clearWidth, clearHeight), clearMask, clearCol));

        // Let watchdog know we're alive.
        m_testCtx.touchWatchdog();
    }

    // Compute reference image.
    {
        for (int y = 0; y < targetHeight; y++)
        {
            std::vector<ClearInfo> scanlineClearOps;

            // Find all rectangles affecting this scanline.
            for (int opNdx = 0; opNdx < (int)clearOps.size(); opNdx++)
            {
                ClearInfo &op = clearOps[opNdx];
                if (de::inBounds(y, op.m_rect.y(), op.m_rect.y() + op.m_rect.w()))
                    scanlineClearOps.push_back(op);
            }

            // Compute reference for scanline.
            int x = 0;
            while (x < targetWidth)
            {
                tcu::RGBA spanColor;
                uint8_t spanKnownMask = 0;
                int spanLength        = (targetWidth - x);

                for (int opNdx = (int)scanlineClearOps.size() - 1; opNdx >= 0; opNdx--)
                {
                    const ClearInfo &op = scanlineClearOps[opNdx];

                    if (de::inBounds(x, op.m_rect.x(), op.m_rect.x() + op.m_rect.z()) &&
                        de::inBounds(y, op.m_rect.y(), op.m_rect.y() + op.m_rect.w()))
                    {
                        // Compute span length until end of given rectangle.
                        spanLength = deMin32(spanLength, op.m_rect.x() + op.m_rect.z() - x);

                        tcu::RGBA clearCol = op.m_color;
                        uint8_t clearMask  = op.m_colorMask;
                        if ((clearMask & 0x1) && !(spanKnownMask & 0x1))
                            spanColor.setRed(clearCol.getRed());
                        if ((clearMask & 0x2) && !(spanKnownMask & 0x2))
                            spanColor.setGreen(clearCol.getGreen());
                        if ((clearMask & 0x4) && !(spanKnownMask & 0x4))
                            spanColor.setBlue(clearCol.getBlue());
                        if ((clearMask & 0x8) && !(spanKnownMask & 0x8))
                            spanColor.setAlpha(clearCol.getAlpha());
                        spanKnownMask |= clearMask;

                        // Break if have all colors.
                        if (spanKnownMask == 0xF)
                            break;
                    }
                    else if (op.m_rect.x() > x)
                        spanLength = deMin32(spanLength, op.m_rect.x() - x);
                }

                // Set reference alpha channel to 0xFF, if no alpha in target.
                if (pixelFormat.alphaBits == 0)
                    spanColor.setAlpha(0xFF);

                // Fill the span.
                for (int ndx = 0; ndx < spanLength; ndx++)
                {
                    refImage.setPixel(x + ndx, y, spanColor);
                    pixelKnownChannelMask[y * targetWidth + x + ndx] |= spanKnownMask;
                }

                x += spanLength;
            }
        }
    }

    glu::readPixels(m_context.getRenderContext(), 0, 0, resImage.getAccess());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glReadPixels");

    // Compute difference image.
    RGBA colorThreshold = pixelFormat.getColorThreshold();
    RGBA matchColor(0, 255, 0, 255);
    RGBA diffColor(255, 0, 0, 255);
    RGBA maxDiff(0, 0, 0, 0);
    bool isImageOk = true;

    if (gl.isEnabled(GL_DITHER))
    {
        colorThreshold.setRed(colorThreshold.getRed() + 1);
        colorThreshold.setGreen(colorThreshold.getGreen() + 1);
        colorThreshold.setBlue(colorThreshold.getBlue() + 1);
        colorThreshold.setAlpha(colorThreshold.getAlpha() + 1);
    }

    for (int y = 0; y < targetHeight; y++)
        for (int x = 0; x < targetWidth; x++)
        {
            int offset      = (y * targetWidth + x);
            RGBA refRGBA    = refImage.getPixel(x, y);
            RGBA resRGBA    = resImage.getPixel(x, y);
            uint8_t colMask = pixelKnownChannelMask[offset];
            RGBA diff       = computeAbsDiffMasked(refRGBA, resRGBA, colMask);
            bool isPixelOk  = diff.isBelowThreshold(colorThreshold);

            diffImage.setPixel(x, y, isPixelOk ? matchColor : diffColor);

            isImageOk = isImageOk && isPixelOk;
            maxDiff   = max(maxDiff, diff);
        }

    if (isImageOk)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    }
    else
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");

        m_testCtx.getLog() << tcu::TestLog::Message << "Image comparison failed, max diff = " << maxDiff
                           << ", threshold = " << colorThreshold << tcu::TestLog::EndMessage;

        log << TestLog::ImageSet("Result", "Resulting framebuffer")
            << TestLog::Image("Result", "Resulting framebuffer", resImage)
            << TestLog::Image("Reference", "Reference image", refImage)
            << TestLog::Image("DiffMask", "Failing pixels", diffImage) << TestLog::EndImageSet;
        return TestCase::STOP;
    }

    bool isFinal = (++m_curIter == m_numIters);

    // On final frame, dump images.
    if (isFinal)
    {
        log << TestLog::ImageSet("Result", "Resulting framebuffer")
            << TestLog::Image("Result", "Resulting framebuffer", resImage) << TestLog::EndImageSet;
    }

    return isFinal ? TestCase::STOP : TestCase::CONTINUE;
}

ColorClearTest::ColorClearTest(Context &context) : TestCaseGroup(context, "color_clear", "Color Clear Tests")
{
}

ColorClearTest::~ColorClearTest(void)
{
}

void ColorClearTest::init(void)
{
    //                                        name                    iters, #..#, alpha?, scissor,masks, 1stfull?
    addChild(new ColorClearCase(m_context, "single_rgb", 30, 1, 3, false, false, false, true));
    addChild(new ColorClearCase(m_context, "single_rgba", 30, 1, 3, true, false, false, true));
    addChild(new ColorClearCase(m_context, "multiple_rgb", 15, 4, 20, false, false, false, true));
    addChild(new ColorClearCase(m_context, "multiple_rgba", 15, 4, 20, true, false, false, true));
    addChild(new ColorClearCase(m_context, "long_rgb", 2, 100, 500, false, false, false, true));
    addChild(new ColorClearCase(m_context, "long_rgba", 2, 100, 500, true, false, false, true));
    addChild(new ColorClearCase(m_context, "subclears_rgb", 15, 4, 30, false, false, false, false));
    addChild(new ColorClearCase(m_context, "subclears_rgba", 15, 4, 30, true, false, false, false));
    addChild(new ColorClearCase(m_context, "short_scissored_rgb", 30, 2, 4, false, true, false, true));
    addChild(new ColorClearCase(m_context, "scissored_rgb", 15, 4, 30, false, true, false, true));
    addChild(new ColorClearCase(m_context, "scissored_rgba", 15, 4, 30, true, true, false, true));
    addChild(new ColorClearCase(m_context, "masked_rgb", 15, 4, 30, false, false, true, true));
    addChild(new ColorClearCase(m_context, "masked_rgba", 15, 4, 30, true, false, true, true));
    addChild(new ColorClearCase(m_context, "masked_scissored_rgb", 15, 4, 30, false, true, true, true));
    addChild(new ColorClearCase(m_context, "masked_scissored_rgba", 15, 4, 30, true, true, true, true));
    addChild(new ColorClearCase(m_context, "complex_rgb", 15, 5, 50, false, true, true, false));
    addChild(new ColorClearCase(m_context, "complex_rgba", 15, 5, 50, true, true, true, false));
    addChild(new ColorClearCase(m_context, "long_masked_rgb", 2, 100, 500, false, true, true, true));
    addChild(new ColorClearCase(m_context, "long_masked_rgba", 2, 100, 500, true, true, true, true));
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
