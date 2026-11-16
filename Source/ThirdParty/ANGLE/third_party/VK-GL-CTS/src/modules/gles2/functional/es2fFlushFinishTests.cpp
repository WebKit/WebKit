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
 * \brief Flush and finish tests.
 *//*--------------------------------------------------------------------*/

#include "es2fFlushFinishTests.hpp"

#include "gluRenderContext.hpp"
#include "gluObjectWrapper.hpp"
#include "gluShaderProgram.hpp"
#include "gluDrawUtil.hpp"

#include "glsCalibration.hpp"

#include "tcuTestLog.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuCPUWarmup.hpp"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deClock.h"
#include "deThread.h"
#include "deMath.h"

#include <algorithm>

namespace deqp
{
namespace gles2
{
namespace Functional
{

using deqp::gls::LineParameters;
using deqp::gls::theilSenLinearRegression;
using std::string;
using std::vector;
using tcu::TestLog;
using tcu::Vec2;

namespace
{

enum
{
    MAX_VIEWPORT_SIZE      = 128,
    MAX_SAMPLE_DURATION_US = 1000 * 1000,
    WAIT_TIME_MS           = 1200,
    NUM_SAMPLES            = 25,
    MIN_DRAW_CALL_COUNT    = 10,
    MAX_DRAW_CALL_COUNT    = 1 << 20,
    NUM_ITERS_IN_SHADER    = 10
};

const float NO_CORR_COEF_THRESHOLD    = 0.1f;
const float FLUSH_COEF_THRESHOLD      = 0.2f;
const float CORRELATED_COEF_THRESHOLD = 0.5f;

static void busyWait(int milliseconds)
{
    const uint64_t startTime = deGetMicroseconds();
    float v                  = 2.0f;

    for (;;)
    {
        for (int i = 0; i < 10; i++)
            v = deFloatSin(v);

        if (deGetMicroseconds() - startTime >= uint64_t(1000 * milliseconds))
            break;
    }
}

class CalibrationFailedException : public std::runtime_error
{
public:
    CalibrationFailedException(const std::string &reason) : std::runtime_error(reason)
    {
    }
};

class FlushFinishCase : public TestCase
{
public:
    enum ExpectedBehavior
    {
        EXPECT_COEF_LESS_THAN = 0,
        EXPECT_COEF_GREATER_THAN,
    };

    FlushFinishCase(Context &context, const char *name, const char *description, ExpectedBehavior waitBehavior,
                    float waitThreshold, ExpectedBehavior readBehavior, float readThreshold);
    ~FlushFinishCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

    struct Sample
    {
        int numDrawCalls;
        uint64_t waitTime;
        uint64_t readPixelsTime;
    };

    struct CalibrationParams
    {
        int maxDrawCalls;
    };

protected:
    virtual void waitForGL(void) = 0;

private:
    FlushFinishCase(const FlushFinishCase &);
    FlushFinishCase &operator=(const FlushFinishCase &);

    CalibrationParams calibrate(void);
    void analyzeResults(const std::vector<Sample> &samples, const CalibrationParams &calibrationParams);

    void setupRenderState(void);
    void render(int numDrawCalls);
    void readPixels(void);

    const ExpectedBehavior m_waitBehavior;
    const float m_waitThreshold;
    const ExpectedBehavior m_readBehavior;
    const float m_readThreshold;

    glu::ShaderProgram *m_program;
};

FlushFinishCase::FlushFinishCase(Context &context, const char *name, const char *description,
                                 ExpectedBehavior waitBehavior, float waitThreshold, ExpectedBehavior readBehavior,
                                 float readThreshold)
    : TestCase(context, name, description)
    , m_waitBehavior(waitBehavior)
    , m_waitThreshold(waitThreshold)
    , m_readBehavior(readBehavior)
    , m_readThreshold(readThreshold)
    , m_program(nullptr)
{
}

FlushFinishCase::~FlushFinishCase(void)
{
    FlushFinishCase::deinit();
}

void FlushFinishCase::init(void)
{
    DE_ASSERT(!m_program);

    m_program =
        new glu::ShaderProgram(m_context.getRenderContext(),
                               glu::ProgramSources() << glu::VertexSource("attribute highp vec4 a_position;\n"
                                                                          "varying highp vec4 v_coord;\n"
                                                                          "void main (void)\n"
                                                                          "{\n"
                                                                          "    gl_Position = a_position;\n"
                                                                          "    v_coord = a_position;\n"
                                                                          "}\n")
                                                     << glu::FragmentSource("uniform mediump int u_numIters;\n"
                                                                            "varying mediump vec4 v_coord;\n"
                                                                            "void main (void)\n"
                                                                            "{\n"
                                                                            "    highp vec4 color = v_coord;\n"
                                                                            "    for (int i = 0; i < " +
                                                                            de::toString(int(NUM_ITERS_IN_SHADER)) +
                                                                            "; i++)\n"
                                                                            "        color = sin(color);\n"
                                                                            "    gl_FragColor = color;\n"
                                                                            "}\n"));

    if (!m_program->isOk())
    {
        m_testCtx.getLog() << *m_program;
        delete m_program;
        m_program = nullptr;
        TCU_FAIL("Compile failed");
    }
}

void FlushFinishCase::deinit(void)
{
    delete m_program;
    m_program = nullptr;
}

tcu::TestLog &operator<<(tcu::TestLog &log, const FlushFinishCase::Sample &sample)
{
    log << TestLog::Message << sample.numDrawCalls << " calls:\t" << sample.waitTime << " us wait,\t"
        << sample.readPixelsTime << " us read" << TestLog::EndMessage;
    return log;
}

void FlushFinishCase::setupRenderState(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    const int posLoc         = gl.getAttribLocation(m_program->getProgram(), "a_position");
    const int viewportW      = de::min<int>(m_context.getRenderTarget().getWidth(), MAX_VIEWPORT_SIZE);
    const int viewportH      = de::min<int>(m_context.getRenderTarget().getHeight(), MAX_VIEWPORT_SIZE);

    static const float s_positions[] = {-1.0f, -1.0f, +1.0f, -1.0f, -1.0f, +1.0f, +1.0f, +1.0f};

    TCU_CHECK(posLoc >= 0);

    gl.viewport(0, 0, viewportW, viewportH);
    gl.useProgram(m_program->getProgram());
    gl.enableVertexAttribArray(posLoc);
    gl.vertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 0, &s_positions[0]);
    gl.enable(GL_BLEND);
    gl.blendFunc(GL_ONE, GL_ONE);
    gl.blendEquation(GL_FUNC_ADD);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to set up render state");
}

void FlushFinishCase::render(int numDrawCalls)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    const uint8_t indices[] = {0, 1, 2, 2, 1, 3};

    gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    for (int ndx = 0; ndx < numDrawCalls; ndx++)
        gl.drawElements(GL_TRIANGLES, DE_LENGTH_OF_ARRAY(indices), GL_UNSIGNED_BYTE, &indices[0]);
}

void FlushFinishCase::readPixels(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    uint8_t tmp[4];

    gl.readPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &tmp);
}

FlushFinishCase::CalibrationParams FlushFinishCase::calibrate(void)
{
    tcu::ScopedLogSection section(m_testCtx.getLog(), "CalibrationInfo", "Calibration info");
    CalibrationParams params;

    // Find draw call count that results in desired maximum time.
    {
        uint64_t prevDuration = 0;
        int prevDrawCount     = 1;
        int curDrawCount      = 1;

        m_testCtx.getLog() << TestLog::Message
                           << "Calibrating maximum draw call count, target duration = " << int(MAX_SAMPLE_DURATION_US)
                           << " us" << TestLog::EndMessage;

        for (;;)
        {
            uint64_t curDuration;

            {
                const uint64_t startTime = deGetMicroseconds();
                render(curDrawCount);
                readPixels();
                curDuration = deGetMicroseconds() - startTime;
            }

            m_testCtx.getLog() << TestLog::Message << "Duration with " << curDrawCount
                               << " draw calls = " << curDuration << " us" << TestLog::EndMessage;

            if (curDuration > MAX_SAMPLE_DURATION_US)
            {
                if (curDrawCount > 1)
                {
                    // Compute final count by using linear estimation.
                    const float a   = float(curDuration - prevDuration) / float(curDrawCount - prevDrawCount);
                    const float b   = float(prevDuration) - a * float(prevDrawCount);
                    const float est = (float(MAX_SAMPLE_DURATION_US) - b) / a;

                    curDrawCount = de::clamp(deFloorFloatToInt32(est), 1, int(MAX_DRAW_CALL_COUNT));
                }
                // else: Settle on 1.

                break;
            }
            else if (curDrawCount >= MAX_DRAW_CALL_COUNT)
                break; // Settle on maximum.
            else
            {
                prevDrawCount = curDrawCount;
                prevDuration  = curDuration;
                curDrawCount  = curDrawCount * 2;
            }
        }

        params.maxDrawCalls = curDrawCount;

        m_testCtx.getLog() << TestLog::Integer("MaxDrawCalls", "Maximum number of draw calls", "", QP_KEY_TAG_NONE,
                                               params.maxDrawCalls);
    }

    // Quick check.
    if (params.maxDrawCalls < MIN_DRAW_CALL_COUNT)
        throw CalibrationFailedException("Calibration failed, maximum draw call count is too low");

    return params;
}

struct CompareSampleDrawCount
{
    bool operator()(const FlushFinishCase::Sample &a, const FlushFinishCase::Sample &b) const
    {
        return a.numDrawCalls < b.numDrawCalls;
    }
};

std::vector<Vec2> getPointsFromSamples(const std::vector<FlushFinishCase::Sample> &samples,
                                       const uint64_t FlushFinishCase::Sample::*field)
{
    vector<Vec2> points(samples.size());

    for (size_t ndx = 0; ndx < samples.size(); ndx++)
        points[ndx] = Vec2(float(samples[ndx].numDrawCalls), float(samples[ndx].*field));

    return points;
}

template <typename T>
T getMaximumValue(const std::vector<FlushFinishCase::Sample> &samples, const T FlushFinishCase::Sample::*field)
{
    DE_ASSERT(!samples.empty());

    T maxVal = samples[0].*field;

    for (size_t ndx = 1; ndx < samples.size(); ndx++)
        maxVal = de::max(maxVal, samples[ndx].*field);

    return maxVal;
}

void FlushFinishCase::analyzeResults(const std::vector<Sample> &samples, const CalibrationParams &calibrationParams)
{
    const vector<Vec2> waitTimes  = getPointsFromSamples(samples, &Sample::waitTime);
    const vector<Vec2> readTimes  = getPointsFromSamples(samples, &Sample::readPixelsTime);
    const LineParameters waitLine = theilSenLinearRegression(waitTimes);
    const LineParameters readLine = theilSenLinearRegression(readTimes);
    const float normWaitCoef =
        waitLine.coefficient * float(calibrationParams.maxDrawCalls) / float(MAX_SAMPLE_DURATION_US);
    const float normReadCoef =
        readLine.coefficient * float(calibrationParams.maxDrawCalls) / float(MAX_SAMPLE_DURATION_US);
    bool allOk = true;

    {
        tcu::ScopedLogSection section(m_testCtx.getLog(), "Samples", "Samples");
        vector<Sample> sortedSamples(samples.begin(), samples.end());

        std::sort(sortedSamples.begin(), sortedSamples.end(), CompareSampleDrawCount());

        for (vector<Sample>::const_iterator iter = sortedSamples.begin(); iter != sortedSamples.end(); ++iter)
            m_testCtx.getLog() << *iter;
    }

    m_testCtx.getLog() << TestLog::Float("WaitCoefficient", "Wait coefficient", "", QP_KEY_TAG_NONE,
                                         waitLine.coefficient)
                       << TestLog::Float("ReadCoefficient", "Read coefficient", "", QP_KEY_TAG_NONE,
                                         readLine.coefficient)
                       << TestLog::Float("NormalizedWaitCoefficient", "Normalized wait coefficient", "",
                                         QP_KEY_TAG_NONE, normWaitCoef)
                       << TestLog::Float("NormalizedReadCoefficient", "Normalized read coefficient", "",
                                         QP_KEY_TAG_NONE, normReadCoef);

    {
        const bool waitCorrelated = normWaitCoef > CORRELATED_COEF_THRESHOLD;
        const bool readCorrelated = normReadCoef > CORRELATED_COEF_THRESHOLD;
        const bool waitNotCorr    = normWaitCoef < NO_CORR_COEF_THRESHOLD;
        const bool readNotCorr    = normReadCoef < NO_CORR_COEF_THRESHOLD;

        if (waitCorrelated || waitNotCorr)
            m_testCtx.getLog() << TestLog::Message << "Wait time is" << (waitCorrelated ? "" : " NOT")
                               << " correlated to rendering workload size." << TestLog::EndMessage;
        else
            m_testCtx.getLog() << TestLog::Message
                               << "Warning: Wait time correlation to rendering workload size is unclear."
                               << TestLog::EndMessage;

        if (readCorrelated || readNotCorr)
            m_testCtx.getLog() << TestLog::Message << "Read time is" << (readCorrelated ? "" : " NOT")
                               << " correlated to rendering workload size." << TestLog::EndMessage;
        else
            m_testCtx.getLog() << TestLog::Message
                               << "Warning: Read time correlation to rendering workload size is unclear."
                               << TestLog::EndMessage;
    }

    for (int ndx = 0; ndx < 2; ndx++)
    {
        const float coef                = ndx == 0 ? normWaitCoef : normReadCoef;
        const char *name                = ndx == 0 ? "wait" : "read";
        const ExpectedBehavior behavior = ndx == 0 ? m_waitBehavior : m_readBehavior;
        const float threshold           = ndx == 0 ? m_waitThreshold : m_readThreshold;
        const bool isOk                 = behavior == EXPECT_COEF_GREATER_THAN ? coef > threshold :
                                          behavior == EXPECT_COEF_LESS_THAN    ? coef < threshold :
                                                                                 false;
        const char *cmpName             = behavior == EXPECT_COEF_GREATER_THAN ? "greater than" :
                                          behavior == EXPECT_COEF_LESS_THAN    ? "less than" :
                                                                                 nullptr;

        if (!isOk)
        {
            m_testCtx.getLog() << TestLog::Message << "ERROR: Expected " << name << " coefficient to be " << cmpName
                               << " " << threshold << TestLog::EndMessage;
            allOk = false;
        }
    }

    m_testCtx.setTestResult(allOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_COMPATIBILITY_WARNING,
                            allOk ? "Pass" : "Suspicious performance behavior");
}

FlushFinishCase::IterateResult FlushFinishCase::iterate(void)
{
    vector<Sample> samples(NUM_SAMPLES);
    CalibrationParams params;

    tcu::warmupCPU();

    setupRenderState();

    // Do one full render cycle.
    {
        render(1);
        readPixels();
    }

    // Calibrate.
    try
    {
        params = calibrate();
    }
    catch (const CalibrationFailedException &e)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_COMPATIBILITY_WARNING, e.what());
        return STOP;
    }

    // Do measurement.
    {
        de::Random rnd(123);

        for (size_t ndx = 0; ndx < samples.size(); ndx++)
        {
            const int drawCallCount = rnd.getInt(1, params.maxDrawCalls);
            uint64_t waitStartTime;
            uint64_t readStartTime;
            uint64_t readFinishTime;

            render(drawCallCount);

            waitStartTime = deGetMicroseconds();
            waitForGL();

            readStartTime = deGetMicroseconds();
            readPixels();
            readFinishTime = deGetMicroseconds();

            samples[ndx].numDrawCalls   = drawCallCount;
            samples[ndx].waitTime       = readStartTime - waitStartTime;
            samples[ndx].readPixelsTime = readFinishTime - readStartTime;

            if (m_testCtx.getWatchDog())
                qpWatchDog_touch(m_testCtx.getWatchDog());
        }
    }

    // Analyze - sets test case result.
    analyzeResults(samples, params);

    return STOP;
}

class WaitOnlyCase : public FlushFinishCase
{
public:
    WaitOnlyCase(Context &context)
        : FlushFinishCase(context, "wait", "Wait only", EXPECT_COEF_LESS_THAN, NO_CORR_COEF_THRESHOLD,
                          EXPECT_COEF_GREATER_THAN, -1000.0f /* practically nothing is expected */)
    {
    }

    void init(void)
    {
        m_testCtx.getLog() << TestLog::Message << int(WAIT_TIME_MS) << " ms busy wait" << TestLog::EndMessage;
        FlushFinishCase::init();
    }

protected:
    void waitForGL(void)
    {
        busyWait(WAIT_TIME_MS);
    }
};

class FlushOnlyCase : public FlushFinishCase
{
public:
    FlushOnlyCase(Context &context)
        : FlushFinishCase(context, "flush", "Flush only", EXPECT_COEF_LESS_THAN, FLUSH_COEF_THRESHOLD,
                          EXPECT_COEF_GREATER_THAN, CORRELATED_COEF_THRESHOLD)
    {
    }

    void init(void)
    {
        m_testCtx.getLog() << TestLog::Message << "Single call to glFlush()" << TestLog::EndMessage;
        FlushFinishCase::init();
    }

protected:
    void waitForGL(void)
    {
        m_context.getRenderContext().getFunctions().flush();
    }
};

class FlushWaitCase : public FlushFinishCase
{
public:
    FlushWaitCase(Context &context)
        : FlushFinishCase(context, "flush_wait", "Wait after flushing", EXPECT_COEF_LESS_THAN, FLUSH_COEF_THRESHOLD,
                          EXPECT_COEF_LESS_THAN, NO_CORR_COEF_THRESHOLD)
    {
    }

    void init(void)
    {
        m_testCtx.getLog() << TestLog::Message << "glFlush() followed by " << int(WAIT_TIME_MS) << " ms busy wait"
                           << TestLog::EndMessage;
        FlushFinishCase::init();
    }

protected:
    void waitForGL(void)
    {
        m_context.getRenderContext().getFunctions().flush();
        busyWait(WAIT_TIME_MS);
    }
};

class FinishOnlyCase : public FlushFinishCase
{
public:
    FinishOnlyCase(Context &context)
        : FlushFinishCase(context, "finish", "Finish only", EXPECT_COEF_GREATER_THAN, CORRELATED_COEF_THRESHOLD,
                          EXPECT_COEF_LESS_THAN, NO_CORR_COEF_THRESHOLD)
    {
    }

    void init(void)
    {
        m_testCtx.getLog() << TestLog::Message << "Single call to glFinish()" << TestLog::EndMessage;
        FlushFinishCase::init();
    }

protected:
    void waitForGL(void)
    {
        m_context.getRenderContext().getFunctions().finish();
    }
};

class FinishWaitCase : public FlushFinishCase
{
public:
    FinishWaitCase(Context &context)
        : FlushFinishCase(context, "finish_wait", "Finish and wait", EXPECT_COEF_GREATER_THAN,
                          CORRELATED_COEF_THRESHOLD, EXPECT_COEF_LESS_THAN, NO_CORR_COEF_THRESHOLD)
    {
    }

    void init(void)
    {
        m_testCtx.getLog() << TestLog::Message << "glFinish() followed by " << int(WAIT_TIME_MS) << " ms busy wait"
                           << TestLog::EndMessage;
        FlushFinishCase::init();
    }

protected:
    void waitForGL(void)
    {
        m_context.getRenderContext().getFunctions().finish();
        busyWait(WAIT_TIME_MS);
    }
};

} // namespace

FlushFinishTests::FlushFinishTests(Context &context) : TestCaseGroup(context, "flush_finish", "Flush and Finish tests")
{
}

FlushFinishTests::~FlushFinishTests(void)
{
}

void FlushFinishTests::init(void)
{
    addChild(new WaitOnlyCase(m_context));
    addChild(new FlushOnlyCase(m_context));
    addChild(new FlushWaitCase(m_context));
    addChild(new FinishOnlyCase(m_context));
    addChild(new FinishWaitCase(m_context));
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
