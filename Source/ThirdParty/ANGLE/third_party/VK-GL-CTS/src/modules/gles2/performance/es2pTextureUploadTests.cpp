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
 * \brief Texture upload performance tests.
 *
 * \todo [2012-10-01 pyry]
 *  - Test different pixel unpack alignments
 *  - Use multiple textures
 *  - Trash cache prior to uploading from data ptr
 *//*--------------------------------------------------------------------*/

#include "es2pTextureUploadTests.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuSurface.hpp"
#include "gluTextureUtil.hpp"
#include "gluShaderProgram.hpp"
#include "gluPixelTransfer.hpp"
#include "deStringUtil.hpp"
#include "deRandom.hpp"
#include "deClock.h"
#include "deString.h"

#include "glsCalibration.hpp"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include <algorithm>
#include <vector>

namespace deqp
{
namespace gles2
{
namespace Performance
{

using std::string;
using std::vector;
using tcu::IVec4;
using tcu::TestLog;
using tcu::TextureFormat;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;
using namespace glw; // GL types

static const int VIEWPORT_SIZE  = 64;
static const float quadCoords[] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};

class TextureUploadCase : public TestCase
{
public:
    TextureUploadCase(Context &context, const char *name, const char *description, UploadFunction uploadFunction,
                      uint32_t format, uint32_t type, int texSize);
    ~TextureUploadCase(void);

    virtual void init(void);
    void deinit(void);

    virtual IterateResult iterate(void) = 0;
    void logResults(void);

protected:
    UploadFunction m_uploadFunction;
    uint32_t m_format;
    uint32_t m_type;
    int m_texSize;
    int m_alignment;

    gls::TheilSenCalibrator m_calibrator;
    glu::ShaderProgram *m_program;
    uint32_t m_texture;
    de::Random m_rnd;
    TestLog &m_log;

    vector<uint8_t> m_texData;
};

TextureUploadCase::TextureUploadCase(Context &context, const char *name, const char *description,
                                     UploadFunction uploadFunction, uint32_t format, uint32_t type, int texSize)
    : TestCase(context, tcu::NODETYPE_PERFORMANCE, name, description)
    , m_uploadFunction(uploadFunction)
    , m_format(format)
    , m_type(type)
    , m_texSize(texSize)
    , m_alignment(4)
    , m_calibrator()
    , m_program(nullptr)
    , m_texture(0)
    , m_rnd(deStringHash(name))
    , m_log(context.getTestContext().getLog())
{
}

TextureUploadCase::~TextureUploadCase(void)
{
    TextureUploadCase::deinit();
}

void TextureUploadCase::deinit(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    if (m_program)
    {
        delete m_program;
        m_program = nullptr;
    }

    gl.deleteTextures(1, &m_texture);
    m_texture = 0;

    m_texData.clear();
}

void TextureUploadCase::init(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    int maxTextureSize;
    gl.getIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);

    if (m_texSize > maxTextureSize)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Unsupported texture size");
        return;
    }

    // Create program

    string vertexShaderSource   = "";
    string fragmentShaderSource = "";

    vertexShaderSource.append("precision mediump    float;\n"
                              "attribute vec2        a_pos;\n"
                              "varying   vec2        v_texCoord;\n"
                              "\n"
                              "void main (void)\n"
                              "{\n"
                              "    v_texCoord = a_pos;\n"
                              "    gl_Position = vec4(a_pos, 0.5, 1.0);\n"
                              "}\n");

    fragmentShaderSource.append("precision    mediump    float;\n"
                                "uniform    lowp sampler2D    u_sampler;\n"
                                "varying    vec2            v_texCoord;\n"
                                "\n"
                                "void main (void)\n"
                                "{\n"
                                "    gl_FragColor = texture2D(u_sampler, v_texCoord.xy);\n"
                                "}\n");

    DE_ASSERT(!m_program);
    m_program = new glu::ShaderProgram(m_context.getRenderContext(),
                                       glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));

    if (!m_program->isOk())
    {
        m_log << *m_program;
        TCU_FAIL("Failed to create shader program (m_programRender)");
    }

    gl.useProgram(m_program->getProgram());

    // Init GL state

    gl.viewport(0, 0, VIEWPORT_SIZE, VIEWPORT_SIZE);
    gl.disable(GL_DEPTH_TEST);
    gl.disable(GL_CULL_FACE);
    gl.enable(GL_BLEND);
    gl.blendFunc(GL_ONE, GL_ONE);
    gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl.clear(GL_COLOR_BUFFER_BIT);

    uint32_t uSampler = gl.getUniformLocation(m_program->getProgram(), "u_sampler");
    uint32_t aPos     = gl.getAttribLocation(m_program->getProgram(), "a_pos");
    gl.enableVertexAttribArray(aPos);
    gl.vertexAttribPointer(aPos, 2, GL_FLOAT, GL_FALSE, 0, &quadCoords[0]);
    gl.uniform1i(uSampler, 0);

    // Create texture

    gl.activeTexture(GL_TEXTURE0);
    gl.genTextures(1, &m_texture);
    gl.bindTexture(GL_TEXTURE_2D, m_texture);
    gl.pixelStorei(GL_UNPACK_ALIGNMENT, m_alignment);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Prepare texture data

    {
        const tcu::TextureFormat &texFmt = glu::mapGLTransferFormat(m_format, m_type);
        int pixelSize                    = texFmt.getPixelSize();
        int stride                       = deAlign32(pixelSize * m_texSize, m_alignment);

        m_texData.resize(stride * m_texSize);

        tcu::PixelBufferAccess access(texFmt, m_texSize, m_texSize, 1, stride, 0, &m_texData[0]);

        tcu::fillWithComponentGradients(access, tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    // Do a dry-run to ensure the pipes are hot

    gl.texImage2D(GL_TEXTURE_2D, 0, m_format, m_texSize, m_texSize, 0, m_format, m_type, &m_texData[0]);
    gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl.finish();
}

void TextureUploadCase::logResults(void)
{
    const gls::MeasureState &measureState = m_calibrator.getMeasureState();

    // Log measurement details

    m_log << TestLog::Section("Measurement details", "Measurement details");
    m_log << TestLog::Message << "Uploading texture with "
          << (m_uploadFunction == UPLOAD_TEXIMAGE2D ? "glTexImage2D" : "glTexSubImage2D") << "."
          << TestLog::EndMessage; // \todo [arttu] Change enum to struct with name included
    m_log << TestLog::Message << "Texture size = " << m_texSize << "x" << m_texSize << "." << TestLog::EndMessage;
    m_log << TestLog::Message << "Viewport size = " << VIEWPORT_SIZE << "x" << VIEWPORT_SIZE << "."
          << TestLog::EndMessage;
    m_log << TestLog::Message << measureState.numDrawCalls << " upload calls / iteration" << TestLog::EndMessage;
    m_log << TestLog::EndSection;

    // Log results

    TestLog &log = m_testCtx.getLog();
    log << TestLog::Section("Results", "Results");

    // Log individual frame durations
    //for (int i = 0; i < m_calibrator.measureState.numFrames; i++)
    // m_log << TestLog::Message << "Frame " << i+1 << " duration: \t" << m_calibrator.measureState.frameTimes[i] << " us."<< TestLog::EndMessage;

    std::vector<uint64_t> sortedFrameTimes(measureState.frameTimes.begin(), measureState.frameTimes.end());
    std::sort(sortedFrameTimes.begin(), sortedFrameTimes.end());
    vector<uint64_t>::const_iterator first  = sortedFrameTimes.begin();
    vector<uint64_t>::const_iterator last   = sortedFrameTimes.end();
    vector<uint64_t>::const_iterator middle = first + (last - first) / 2;

    uint64_t medianFrameTime = *middle;
    double medianMTexelsPerSeconds =
        (double)(m_texSize * m_texSize * measureState.numDrawCalls) / (double)medianFrameTime;
    double medianTexelDrawDurationNs =
        (double)medianFrameTime * 1000.0 / (double)(m_texSize * m_texSize * measureState.numDrawCalls);

    uint64_t totalTime       = measureState.getTotalTime();
    int numFrames            = (int)measureState.frameTimes.size();
    int64_t numTexturesDrawn = measureState.numDrawCalls * numFrames;
    int64_t numPixels        = (int64_t)m_texSize * (int64_t)m_texSize * numTexturesDrawn;

    double framesPerSecond        = (double)numFrames / ((double)totalTime / 1000000.0);
    double avgFrameTime           = (double)totalTime / (double)numFrames;
    double avgMTexelsPerSeconds   = (double)numPixels / (double)totalTime;
    double avgTexelDrawDurationNs = (double)totalTime * 1000.0 / (double)numPixels;

    log << TestLog::Float("FramesPerSecond", "Frames per second in measurement\t\t", "Frames/s", QP_KEY_TAG_PERFORMANCE,
                          (float)framesPerSecond);
    log << TestLog::Float("AverageFrameTime", "Average frame duration in measurement\t", "us", QP_KEY_TAG_PERFORMANCE,
                          (float)avgFrameTime);
    log << TestLog::Float("AverageTexelPerf", "Average texel upload performance\t\t", "MTex/s", QP_KEY_TAG_PERFORMANCE,
                          (float)avgMTexelsPerSeconds);
    log << TestLog::Float("AverageTexelTime", "Average texel upload duration\t\t", "ns", QP_KEY_TAG_PERFORMANCE,
                          (float)avgTexelDrawDurationNs);
    log << TestLog::Float("MedianTexelPerf", "Median texel upload performance\t\t", "MTex/s", QP_KEY_TAG_PERFORMANCE,
                          (float)medianMTexelsPerSeconds);
    log << TestLog::Float("MedianTexelTime", "Median texel upload duration\t\t", "ns", QP_KEY_TAG_PERFORMANCE,
                          (float)medianTexelDrawDurationNs);

    log << TestLog::EndSection;

    gls::logCalibrationInfo(log, m_calibrator); // Log calibration details
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, de::floatToString((float)avgMTexelsPerSeconds, 2).c_str());
}

// Texture upload call case

class TextureUploadCallCase : public TextureUploadCase
{
public:
    TextureUploadCallCase(Context &context, const char *name, const char *description, UploadFunction uploadFunction,
                          uint32_t format, uint32_t type, int texSize);
    ~TextureUploadCallCase(void);

    IterateResult iterate(void);
    void render(void);
};

TextureUploadCallCase::TextureUploadCallCase(Context &context, const char *name, const char *description,
                                             UploadFunction uploadFunction, uint32_t format, uint32_t type, int texSize)
    : TextureUploadCase(context, name, description, uploadFunction, format, type, texSize)
{
}

TextureUploadCallCase::~TextureUploadCallCase(void)
{
    TextureUploadCase::deinit();
}

void TextureUploadCallCase::render(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    // Draw multiple quads to ensure enough workload

    switch (m_uploadFunction)
    {
    case UPLOAD_TEXIMAGE2D:
        gl.texImage2D(GL_TEXTURE_2D, 0, m_format, m_texSize, m_texSize, 0, m_format, m_type, &m_texData[0]);
        break;
    case UPLOAD_TEXSUBIMAGE2D:
        gl.texSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_texSize, m_texSize, m_format, m_type, &m_texData[0]);
        break;
    default:
        DE_ASSERT(false);
    }
}

tcu::TestNode::IterateResult TextureUploadCallCase::iterate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    if (m_testCtx.getTestResult() == QP_TEST_RESULT_NOT_SUPPORTED)
        return STOP;

    for (;;)
    {
        gls::TheilSenCalibrator::State state = m_calibrator.getState();

        if (state == gls::TheilSenCalibrator::STATE_MEASURE)
        {
            int numCalls       = m_calibrator.getCallCount();
            uint64_t startTime = deGetMicroseconds();

            for (int i = 0; i < numCalls; i++)
                render();

            gl.finish();

            uint64_t endTime  = deGetMicroseconds();
            uint64_t duration = endTime - startTime;

            m_calibrator.recordIteration(duration);
        }
        else if (state == gls::TheilSenCalibrator::STATE_RECOMPUTE_PARAMS)
        {
            m_calibrator.recomputeParameters();
        }
        else
        {
            DE_ASSERT(state == gls::TheilSenCalibrator::STATE_FINISHED);
            break;
        }

        // Touch watchdog between iterations to avoid timeout.
        {
            qpWatchDog *dog = m_testCtx.getWatchDog();
            if (dog)
                qpWatchDog_touch(dog);
        }
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "iterate");
    logResults();
    return STOP;
}

// Texture upload and draw case

class TextureUploadAndDrawCase : public TextureUploadCase
{
public:
    TextureUploadAndDrawCase(Context &context, const char *name, const char *description, UploadFunction uploadFunction,
                             uint32_t format, uint32_t type, int texSize);
    ~TextureUploadAndDrawCase(void);

    IterateResult iterate(void);
    void render(void);

private:
    bool m_lastIterationRender;
    uint64_t m_renderStart;
};

TextureUploadAndDrawCase::TextureUploadAndDrawCase(Context &context, const char *name, const char *description,
                                                   UploadFunction uploadFunction, uint32_t format, uint32_t type,
                                                   int texSize)
    : TextureUploadCase(context, name, description, uploadFunction, format, type, texSize)
    , m_lastIterationRender(false)
    , m_renderStart(0)
{
}

TextureUploadAndDrawCase::~TextureUploadAndDrawCase(void)
{
    TextureUploadCase::deinit();
}

void TextureUploadAndDrawCase::render(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    // Draw multiple quads to ensure enough workload

    switch (m_uploadFunction)
    {
    case UPLOAD_TEXIMAGE2D:
        gl.texImage2D(GL_TEXTURE_2D, 0, m_format, m_texSize, m_texSize, 0, m_format, m_type, &m_texData[0]);
        break;
    case UPLOAD_TEXSUBIMAGE2D:
        gl.texSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_texSize, m_texSize, m_format, m_type, &m_texData[0]);
        break;
    default:
        DE_ASSERT(false);
    }

    gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

tcu::TestNode::IterateResult TextureUploadAndDrawCase::iterate(void)
{
    if (m_testCtx.getTestResult() == QP_TEST_RESULT_NOT_SUPPORTED)
        return STOP;

    if (m_lastIterationRender && (m_calibrator.getState() == gls::TheilSenCalibrator::STATE_MEASURE))
    {
        uint64_t curTime = deGetMicroseconds();
        m_calibrator.recordIteration(curTime - m_renderStart);
    }

    gls::TheilSenCalibrator::State state = m_calibrator.getState();

    if (state == gls::TheilSenCalibrator::STATE_MEASURE)
    {
        // Render
        int numCalls = m_calibrator.getCallCount();

        m_renderStart         = deGetMicroseconds();
        m_lastIterationRender = true;

        for (int i = 0; i < numCalls; i++)
            render();

        return CONTINUE;
    }
    else if (state == gls::TheilSenCalibrator::STATE_RECOMPUTE_PARAMS)
    {
        m_calibrator.recomputeParameters();
        m_lastIterationRender = false;
        return CONTINUE;
    }
    else
    {
        DE_ASSERT(state == gls::TheilSenCalibrator::STATE_FINISHED);
        GLU_EXPECT_NO_ERROR(m_context.getRenderContext().getFunctions().getError(), "finish");
        logResults();
        return STOP;
    }
}

// Texture upload tests

TextureUploadTests::TextureUploadTests(Context &context) : TestCaseGroup(context, "upload", "Texture upload tests")
{
}

TextureUploadTests::~TextureUploadTests(void)
{
    TextureUploadTests::deinit();
}

void TextureUploadTests::deinit(void)
{
}

void TextureUploadTests::init(void)
{
    TestCaseGroup *uploadCall = new TestCaseGroup(m_context, "upload", "Texture upload");
    TestCaseGroup *uploadAndDraw =
        new TestCaseGroup(m_context, "upload_draw_swap", "Texture upload, draw & buffer swap");

    addChild(uploadCall);
    addChild(uploadAndDraw);

    static const struct
    {
        const char *name;
        const char *nameLower;
        UploadFunction func;
    } uploadFunctions[] = {{"texImage2D", "teximage2d", UPLOAD_TEXIMAGE2D},
                           {"texSubImage2D", "texsubimage2d", UPLOAD_TEXSUBIMAGE2D}};

    static const struct
    {
        const char *name;
        uint32_t format;
        uint32_t type;
    } textureCombinations[] = {
        {"rgb_ubyte", GL_RGB, GL_UNSIGNED_BYTE},
        {"rgba_ubyte", GL_RGBA, GL_UNSIGNED_BYTE},
        {"alpha_ubyte", GL_ALPHA, GL_UNSIGNED_BYTE},
        {"luminance_ubyte", GL_LUMINANCE, GL_UNSIGNED_BYTE},
        {"luminance-alpha_ubyte", GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE},
        {"rgb_ushort565", GL_RGB, GL_UNSIGNED_SHORT_5_6_5},
        {"rgba_ushort4444", GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4},
        {"rgba_ushort5551", GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1},
    };

    static const struct
    {
        int size;
        TestCaseGroup *uploadCallGroup;
        TestCaseGroup *uploadAndDrawGroup;
    } textureSizes[] = {
        {16, new TestCaseGroup(m_context, "16x16", "Texture size 16x16"),
         new TestCaseGroup(m_context, "16x16", "Texture size 16x16")},
        {256, new TestCaseGroup(m_context, "256x256", "Texture size 256x256"),
         new TestCaseGroup(m_context, "256x256", "Texture size 256x256")},
        {257, new TestCaseGroup(m_context, "257x257", "Texture size 257x257"),
         new TestCaseGroup(m_context, "257x257", "Texture size 257x257")},
        {1024, new TestCaseGroup(m_context, "1024x1024", "Texture size 1024x1024"),
         new TestCaseGroup(m_context, "1024x1024", "Texture size 1024x1024")},
        {2048, new TestCaseGroup(m_context, "2048x2048", "Texture size 2048x2048"),
         new TestCaseGroup(m_context, "2048x2048", "Texture size 2048x2048")},
    };

#define FOR_EACH(ITERATOR, ARRAY, BODY)                                      \
    for (int ITERATOR = 0; ITERATOR < DE_LENGTH_OF_ARRAY(ARRAY); ITERATOR++) \
    BODY

    FOR_EACH(uploadFunc, uploadFunctions,
             FOR_EACH(texSize, textureSizes, FOR_EACH(texCombination, textureCombinations, {
                          string caseName = string("") + uploadFunctions[uploadFunc].nameLower + "_" +
                                            textureCombinations[texCombination].name;
                          UploadFunction function = uploadFunctions[uploadFunc].func;
                          uint32_t format         = textureCombinations[texCombination].format;
                          uint32_t type           = textureCombinations[texCombination].type;
                          int size                = textureSizes[texSize].size;

                          textureSizes[texSize].uploadCallGroup->addChild(
                              new TextureUploadCallCase(m_context, caseName.c_str(), "", function, format, type, size));
                          textureSizes[texSize].uploadAndDrawGroup->addChild(new TextureUploadAndDrawCase(
                              m_context, caseName.c_str(), "", function, format, type, size));
                      })))

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(textureSizes); i++)
    {
        uploadCall->addChild(textureSizes[i].uploadCallGroup);
        uploadAndDraw->addChild(textureSizes[i].uploadAndDrawGroup);
    }
}

} // namespace Performance
} // namespace gles2
} // namespace deqp
