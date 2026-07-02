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
 * \brief Internal Format Query tests.
 *//*--------------------------------------------------------------------*/

#include "es3fInternalFormatQueryTests.hpp"
#include "glsStateQueryUtil.hpp"
#include "es3fApiCase.hpp"
#include "gluRenderContext.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "deMath.h"

using namespace glw; // GLint and other GL types
using deqp::gls::StateQueryUtil::StateQueryMemoryWriteGuard;

namespace deqp
{
namespace gles3
{
namespace Functional
{
namespace
{

class SamplesCase : public ApiCase
{
public:
    SamplesCase(Context &context, const char *name, const char *description, GLenum internalFormat,
                bool isIntegerInternalFormat)
        : ApiCase(context, name, description)
        , m_internalFormat(internalFormat)
        , m_isIntegerInternalFormat(isIntegerInternalFormat)
    {
    }

    void test(void)
    {
        using tcu::TestLog;

        StateQueryMemoryWriteGuard<GLint> sampleCounts;
        glGetInternalformativ(GL_RENDERBUFFER, m_internalFormat, GL_NUM_SAMPLE_COUNTS, 1, &sampleCounts);
        expectError(GL_NO_ERROR);

        if (!sampleCounts.verifyValidity(m_testCtx))
            return;

        m_testCtx.getLog() << TestLog::Message << "// sample counts is " << sampleCounts << TestLog::EndMessage;

        if (sampleCounts == 0)
            return;

        std::vector<GLint> samples;
        samples.resize(sampleCounts, -1);
        glGetInternalformativ(GL_RENDERBUFFER, m_internalFormat, GL_SAMPLES, sampleCounts, &samples[0]);
        expectError(GL_NO_ERROR);

        GLint prevSampleCount = 0;
        GLint sampleCount     = 0;
        for (size_t ndx = 0; ndx < samples.size(); ++ndx, prevSampleCount = sampleCount)
        {
            sampleCount = samples[ndx];

            // sample count must be > 0
            if (sampleCount <= 0)
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected sample count to be at least one; got "
                                   << sampleCount << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid value");
            }

            // samples must be ordered descending
            if (ndx != 0 && !(sampleCount < prevSampleCount))
            {
                m_testCtx.getLog() << TestLog::Message
                                   << "// ERROR: Expected sample count to be ordered in descending order;"
                                   << "got " << prevSampleCount << " at index " << (ndx - 1) << ", and " << sampleCount
                                   << " at index " << ndx << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid order");
            }
        }

        if (!m_isIntegerInternalFormat)
        {
            // the maximum value in SAMPLES is guaranteed to be at least the value of MAX_SAMPLES
            StateQueryMemoryWriteGuard<GLint> maxSamples;
            glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
            expectError(GL_NO_ERROR);

            if (maxSamples.verifyValidity(m_testCtx))
            {
                const GLint maximumFormatSampleCount = samples[0];
                if (!(maximumFormatSampleCount >= maxSamples))
                {
                    m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected maximum value in SAMPLES ("
                                       << maximumFormatSampleCount << ") to be at least the value of MAX_SAMPLES ("
                                       << maxSamples << ")" << TestLog::EndMessage;
                    if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid maximum sample count");
                }
            }
        }
    }

private:
    const GLenum m_internalFormat;
    const bool m_isIntegerInternalFormat;
};

class SamplesBufferSizeCase : public ApiCase
{
public:
    SamplesBufferSizeCase(Context &context, const char *name, const char *description, GLenum internalFormat)
        : ApiCase(context, name, description)
        , m_internalFormat(internalFormat)
    {
    }

    void test(void)
    {
        using tcu::TestLog;

        StateQueryMemoryWriteGuard<GLint> sampleCounts;
        glGetInternalformativ(GL_RENDERBUFFER, m_internalFormat, GL_NUM_SAMPLE_COUNTS, 1, &sampleCounts);
        expectError(GL_NO_ERROR);

        if (!sampleCounts.verifyValidity(m_testCtx))
            return;

        // test with bufSize = 0
        GLint queryTargetValue = -1;
        glGetInternalformativ(GL_RENDERBUFFER, m_internalFormat, GL_NUM_SAMPLE_COUNTS, 0, &queryTargetValue);
        expectError(GL_NO_ERROR);

        if (queryTargetValue != -1)
        {
            m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected output variable not to be written to."
                               << TestLog::EndMessage;
            if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid write");
        }
    }

private:
    GLenum m_internalFormat;
};

} // namespace

InternalFormatQueryTests::InternalFormatQueryTests(Context &context)
    : TestCaseGroup(context, "internal_format", "Internal Format Query tests.")
{
}

void InternalFormatQueryTests::init(void)
{
    const struct InternalFormat
    {
        const char *name;
        GLenum format;
        bool isIntegerFormat;
    } internalFormats[] = {
        // color renderable
        {"r8", GL_R8, false},
        {"rg8", GL_RG8, false},
        {"rgb8", GL_RGB8, false},
        {"rgb565", GL_RGB565, false},
        {"rgba4", GL_RGBA4, false},
        {"rgb5_a1", GL_RGB5_A1, false},
        {"rgba8", GL_RGBA8, false},
        {"rgb10_a2", GL_RGB10_A2, false},
        {"rgb10_a2ui", GL_RGB10_A2UI, true},
        {"srgb8_alpha8", GL_SRGB8_ALPHA8, false},
        {"r8i", GL_R8I, true},
        {"r8ui", GL_R8UI, true},
        {"r16i", GL_R16I, true},
        {"r16ui", GL_R16UI, true},
        {"r32i", GL_R32I, true},
        {"r32ui", GL_R32UI, true},
        {"rg8i", GL_RG8I, true},
        {"rg8ui", GL_RG8UI, true},
        {"rg16i", GL_RG16I, true},
        {"rg16ui", GL_RG16UI, true},
        {"rg32i", GL_RG32I, true},
        {"rg32ui", GL_RG32UI, true},
        {"rgba8i", GL_RGBA8I, true},
        {"rgba8ui", GL_RGBA8UI, true},
        {"rgba16i", GL_RGBA16I, true},
        {"rgba16ui", GL_RGBA16UI, true},
        {"rgba32i", GL_RGBA32I, true},
        {"rgba32ui", GL_RGBA32UI, true},

        // depth renderable
        {"depth_component16", GL_DEPTH_COMPONENT16, false},
        {"depth_component24", GL_DEPTH_COMPONENT24, false},
        {"depth_component32f", GL_DEPTH_COMPONENT32F, false},
        {"depth24_stencil8", GL_DEPTH24_STENCIL8, false},
        {"depth32f_stencil8", GL_DEPTH32F_STENCIL8, false},

        // stencil renderable
        {"stencil_index8", GL_STENCIL_INDEX8, false}
        // DEPTH24_STENCIL8,  duplicate
        // DEPTH32F_STENCIL8  duplicate
    };

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(internalFormats); ++ndx)
    {
        const InternalFormat internalFormat = internalFormats[ndx];

        addChild(new SamplesCase(m_context, (std::string(internalFormat.name) + "_samples").c_str(),
                                 "SAMPLES and NUM_SAMPLE_COUNTS", internalFormat.format,
                                 internalFormat.isIntegerFormat));
    }

    addChild(new SamplesBufferSizeCase(m_context, "rgba8_samples_buffer", "SAMPLES bufSize parameter", GL_RGBA8));
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
