/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief Test Log C++ Wrapper.
 *//*--------------------------------------------------------------------*/

#include "deCommandLine.h"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuSurface.hpp"
#include "deMath.h"

#include <limits>

namespace tcu
{

class LogWriteFailedError : public ResourceError
{
public:
    LogWriteFailedError(void) : ResourceError("Writing to test log failed")
    {
    }
};

enum
{
    MAX_IMAGE_SIZE_2D = 4096,
    MAX_IMAGE_SIZE_3D = 128
};

// LogImage

LogImage::LogImage(const std::string &name, const std::string &description, const Surface &surface,
                   qpImageCompressionMode compression)
    : m_name(name)
    , m_description(description)
    , m_access(surface.getAccess())
    , m_scale(1.0f, 1.0f, 1.0f, 1.0f)
    , m_bias(0.0f, 0.0f, 0.0f, 0.0f)
    , m_compression(compression)
{
}

LogImage::LogImage(const std::string &name, const std::string &description, const ConstPixelBufferAccess &access,
                   qpImageCompressionMode compression)
    : m_name(name)
    , m_description(description)
    , m_access(access)
    , m_scale(1.0f, 1.0f, 1.0f, 1.0f)
    , m_bias(0.0f, 0.0f, 0.0f, 0.0f)
    , m_compression(compression)
{
    // Simplify combined formats that only use a single channel
    if (tcu::isCombinedDepthStencilType(m_access.getFormat().type))
    {
        if (m_access.getFormat().order == tcu::TextureFormat::D)
            m_access = tcu::getEffectiveDepthStencilAccess(m_access, tcu::Sampler::MODE_DEPTH);
        else if (m_access.getFormat().order == tcu::TextureFormat::S)
            m_access = tcu::getEffectiveDepthStencilAccess(m_access, tcu::Sampler::MODE_STENCIL);
    }

    // Implicit scale and bias
    if (m_access.getFormat().order != tcu::TextureFormat::DS)
        computePixelScaleBias(m_access, m_scale, m_bias);
    else
    {
        // Pack D and S bias and scale to R and G
        const ConstPixelBufferAccess depthAccess =
            tcu::getEffectiveDepthStencilAccess(m_access, tcu::Sampler::MODE_DEPTH);
        const ConstPixelBufferAccess stencilAccess =
            tcu::getEffectiveDepthStencilAccess(m_access, tcu::Sampler::MODE_STENCIL);
        tcu::Vec4 depthScale;
        tcu::Vec4 depthBias;
        tcu::Vec4 stencilScale;
        tcu::Vec4 stencilBias;

        computePixelScaleBias(depthAccess, depthScale, depthBias);
        computePixelScaleBias(stencilAccess, stencilScale, stencilBias);

        m_scale = tcu::Vec4(depthScale.x(), stencilScale.x(), 0.0f, 0.0f);
        m_bias  = tcu::Vec4(depthBias.x(), stencilBias.x(), 0.0f, 0.0f);
    }
}

LogImage::LogImage(const std::string &name, const std::string &description, const ConstPixelBufferAccess &access,
                   const Vec4 &scale, const Vec4 &bias, qpImageCompressionMode compression)
    : m_name(name)
    , m_description(description)
    , m_access(access)
    , m_scale(scale)
    , m_bias(bias)
    , m_compression(compression)
{
    // Cannot set scale and bias of combined formats
    DE_ASSERT(access.getFormat().order != tcu::TextureFormat::DS);

    // Simplify access
    if (tcu::isCombinedDepthStencilType(access.getFormat().type))
    {
        if (access.getFormat().order == tcu::TextureFormat::D)
            m_access = tcu::getEffectiveDepthStencilAccess(access, tcu::Sampler::MODE_DEPTH);
        if (access.getFormat().order == tcu::TextureFormat::S)
            m_access = tcu::getEffectiveDepthStencilAccess(access, tcu::Sampler::MODE_STENCIL);
        else
        {
            // Cannot log a DS format
            DE_ASSERT(false);
            return;
        }
    }
}

void LogImage::write(TestLog &log) const
{
    if (m_access.getFormat().order != tcu::TextureFormat::DS)
        log.writeImage(m_name.c_str(), m_description.c_str(), m_access, m_scale, m_bias, m_compression);
    else
    {
        const ConstPixelBufferAccess depthAccess =
            tcu::getEffectiveDepthStencilAccess(m_access, tcu::Sampler::MODE_DEPTH);
        const ConstPixelBufferAccess stencilAccess =
            tcu::getEffectiveDepthStencilAccess(m_access, tcu::Sampler::MODE_STENCIL);

        log.startImageSet(m_name.c_str(), m_description.c_str());
        log.writeImage("Depth", "Depth channel", depthAccess, m_scale.swizzle(0, 0, 0, 0), m_bias.swizzle(0, 0, 0, 0),
                       m_compression);
        log.writeImage("Stencil", "Stencil channel", stencilAccess, m_scale.swizzle(1, 1, 1, 1),
                       m_bias.swizzle(1, 1, 1, 1), m_compression);
        log.endImageSet();
    }
}

// MessageBuilder

MessageBuilder::MessageBuilder(const MessageBuilder &other) : m_log(other.m_log)
{
    m_str.str(other.m_str.str());
}

MessageBuilder &MessageBuilder::operator=(const MessageBuilder &other)
{
    m_log = other.m_log;
    m_str.str(other.m_str.str());
    return *this;
}

TestLog &MessageBuilder::operator<<(const TestLog::EndMessageToken &)
{
    m_log->writeMessage(m_str.str().c_str());
    return *m_log;
}

// SampleBuilder

TestLog &SampleBuilder::operator<<(const TestLog::EndSampleToken &)
{
    m_log->startSample();

    for (std::vector<Value>::const_iterator val = m_values.begin(); val != m_values.end(); ++val)
    {
        if (val->type == Value::TYPE_FLOAT64)
            m_log->writeSampleValue(val->value.float64);
        else if (val->type == Value::TYPE_INT64)
            m_log->writeSampleValue(val->value.int64);
        else
            DE_ASSERT(false);
    }

    m_log->endSample();

    return *m_log;
}

// TestLog

TestLog::TestLog(const char *fileName, uint32_t flags)
    : m_log(qpTestLog_createFileLog(fileName, flags))
    , m_logSupressed(false)
{
    if (!m_log)
        throw ResourceError(std::string("Failed to open test log file '") + fileName + "'");
}

void TestLog::writeSessionInfo(std::string additionalInfo)
{
    qpTestLog_beginSession(m_log, additionalInfo.c_str());
}

TestLog::~TestLog(void)
{
    qpTestLog_destroy(m_log);
}

void TestLog::writeMessage(const char *msgStr)
{
    if (m_logSupressed)
        return;
    if (m_skipAdditionalDataInLog)
        return;
    if (qpTestLog_writeText(m_log, nullptr, nullptr, QP_KEY_TAG_NONE, msgStr) == false)
        throw LogWriteFailedError();
}

void TestLog::startImageSet(const char *name, const char *description)
{
    if (m_logSupressed)
        return;
    if (m_skipAdditionalDataInLog)
        return;
    if (qpTestLog_startImageSet(m_log, name, description) == false)
        throw LogWriteFailedError();
}

void TestLog::endImageSet(void)
{
    if (m_logSupressed)
        return;
    if (m_skipAdditionalDataInLog)
        return;
    if (qpTestLog_endImageSet(m_log) == false)
        throw LogWriteFailedError();
}

template <int Size>
static Vector<int, Size> computeScaledSize(const Vector<int, Size> &imageSize, int maxSize)
{
    bool allInRange = true;
    for (int i = 0; i < Size; i++)
        allInRange = allInRange && (imageSize[i] <= maxSize);

    if (allInRange)
        return imageSize;
    else
    {
        float d = 1.0f;
        for (int i = 0; i < Size; i++)
            d = de::max(d, (float)imageSize[i] / (float)maxSize);

        Vector<int, Size> res;
        for (int i = 0; i < Size; i++)
            res[i] = de::max(1, deRoundFloatToInt32((float)imageSize[i] / d));

        return res;
    }
}

void TestLog::writeImage(const char *name, const char *description, const ConstPixelBufferAccess &access,
                         const Vec4 &pixelScale, const Vec4 &pixelBias, qpImageCompressionMode compressionMode)
{
    if (m_logSupressed)
        return;
    if (m_skipAdditionalDataInLog)
        return;
    const TextureFormat &format = access.getFormat();
    int width                   = access.getWidth();
    int height                  = access.getHeight();
    int depth                   = access.getDepth();

    // Writing a combined image does not make sense
    DE_ASSERT(!tcu::isCombinedDepthStencilType(access.getFormat().type));

    // Do not bother with preprocessing if images are not stored
    if ((qpTestLog_getLogFlags(m_log) & QP_TEST_LOG_EXCLUDE_IMAGES) != 0)
        return;

    if (depth == 1 && (format.type == TextureFormat::UNORM_INT8 || format.type == TextureFormat::UNSIGNED_INT8) &&
        width <= MAX_IMAGE_SIZE_2D && height <= MAX_IMAGE_SIZE_2D &&
        (format.order == TextureFormat::RGB || format.order == TextureFormat::RGBA) &&
        access.getPixelPitch() == access.getFormat().getPixelSize() && pixelBias[0] == 0.0f && pixelBias[1] == 0.0f &&
        pixelBias[2] == 0.0f && pixelBias[3] == 0.0f && pixelScale[0] == 1.0f && pixelScale[1] == 1.0f &&
        pixelScale[2] == 1.0f && pixelScale[3] == 1.0f)
    {
        // Fast-path.
        bool isRGBA = format.order == TextureFormat::RGBA;

        writeImage(name, description, compressionMode, isRGBA ? QP_IMAGE_FORMAT_RGBA8888 : QP_IMAGE_FORMAT_RGB888,
                   width, height, access.getRowPitch(), access.getDataPtr());
    }
    else if (depth == 1 || isSeparateSlices())
    {
        Sampler sampler(Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE, Sampler::LINEAR,
                        Sampler::NEAREST);
        IVec2 logImageSize = computeScaledSize(IVec2(width, height), MAX_IMAGE_SIZE_2D);
        tcu::TextureLevel logImage(TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8), logImageSize.x(),
                                   logImageSize.y(), 1);
        PixelBufferAccess logImageAccess = logImage.getAccess();
        std::ostringstream longDesc;

        longDesc << description << " (p' = p * " << pixelScale << " + " << pixelBias << ")";

        for (int z = 0; z < depth; ++z)
        {
            for (int y = 0; y < logImage.getHeight(); y++)
            {
                for (int x = 0; x < logImage.getWidth(); x++)
                {
                    float yf = ((float)y + 0.5f) / (float)logImage.getHeight();
                    float xf = ((float)x + 0.5f) / (float)logImage.getWidth();
                    Vec4 s   = access.sample2D(sampler, sampler.minFilter, xf, yf, z) * pixelScale + pixelBias;

                    logImageAccess.setPixel(s, x, y);
                }
            }

            const auto loggedName = (depth == 1 ? name : name + std::string("-Slice") + std::to_string(z));
            const auto loggedDesc = longDesc.str() + (depth == 1 ? " [Slice " + std::to_string(z) + "]" : "");
            writeImage(loggedName.c_str(), loggedDesc.c_str(), compressionMode, QP_IMAGE_FORMAT_RGBA8888,
                       logImageAccess.getWidth(), logImageAccess.getHeight(), logImageAccess.getRowPitch(),
                       logImageAccess.getDataPtr());
        }
    }
    else
    {
        // Isometric splat volume rendering.
        const float blendFactor = 0.85f;
        IVec3 scaledSize        = computeScaledSize(IVec3(width, height, depth), MAX_IMAGE_SIZE_3D);
        int w                   = scaledSize.x();
        int h                   = scaledSize.y();
        int d                   = scaledSize.z();
        int logImageW           = w + d - 1;
        int logImageH           = w + d + h;
        std::vector<float> blendImage(logImageW * logImageH * 4, 0.0f);
        PixelBufferAccess blendImageAccess(TextureFormat(TextureFormat::RGBA, TextureFormat::FLOAT), logImageW,
                                           logImageH, 1, &blendImage[0]);
        tcu::TextureLevel logImage(TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8), logImageW, logImageH,
                                   1);
        PixelBufferAccess logImageAccess = logImage.getAccess();
        Sampler sampler(Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE, Sampler::NEAREST,
                        Sampler::NEAREST);
        std::ostringstream longDesc;

        // \note Back-to-front.
        for (int z = d - 1; z >= 0; z--)
        {
            for (int y = 0; y < h; y++)
            {
                for (int x = 0; x < w; x++)
                {
                    int px = w - (x + 1) + z;
                    int py = (w + d + h) - (x + y + z + 1);

                    float xf = ((float)x + 0.5f) / (float)w;
                    float yf = ((float)y + 0.5f) / (float)h;
                    float zf = ((float)z + 0.5f) / (float)d;

                    Vec4 p = blendImageAccess.getPixel(px, py);
                    Vec4 s = access.sample3D(sampler, sampler.minFilter, xf, yf, zf);
                    Vec4 b = s + p * blendFactor;

                    blendImageAccess.setPixel(b, px, py);
                }
            }
        }

        // Scale blend image nicely.
        longDesc << description << " (p' = p * " << pixelScale << " + " << pixelBias << ")";

        // Write to final image.
        tcu::clear(logImageAccess, tcu::IVec4(0x33, 0x66, 0x99, 0xff));

        for (int z = 0; z < d; z++)
        {
            for (int y = 0; y < h; y++)
            {
                for (int x = 0; x < w; x++)
                {
                    if (z != 0 && !(x == 0 || y == h - 1 || y == h - 2))
                        continue;

                    int px = w - (x + 1) + z;
                    int py = (w + d + h) - (x + y + z + 1);
                    Vec4 s = blendImageAccess.getPixel(px, py) * pixelScale + pixelBias;

                    logImageAccess.setPixel(s, px, py);
                }
            }
        }

        writeImage(name, longDesc.str().c_str(), compressionMode, QP_IMAGE_FORMAT_RGBA8888, logImageAccess.getWidth(),
                   logImageAccess.getHeight(), logImageAccess.getRowPitch(), logImageAccess.getDataPtr());
    }
}

void TestLog::writeImage(const char *name, const char *description, qpImageCompressionMode compressionMode,
                         qpImageFormat format, int width, int height, int stride, const void *data)
{
    if (m_logSupressed)
        return;
    if (m_skipAdditionalDataInLog)
        return;
    if (qpTestLog_writeImage(m_log, name, description, compressionMode, format, width, height, stride, data) == false)
        throw LogWriteFailedError();
}

void TestLog::startSection(const char *name, const char *description)
{
    if (m_logSupressed)
        return;
    if (m_skipAdditionalDataInLog)
        return;
    if (qpTestLog_startSection(m_log, name, description) == false)
        throw LogWriteFailedError();
}

void TestLog::endSection(void)
{
    if (m_logSupressed)
        return;
    if (m_skipAdditionalDataInLog)
        return;
    if (qpTestLog_endSection(m_log) == false)
        throw LogWriteFailedError();
}

void TestLog::startShaderProgram(bool linkOk, const char *linkInfoLog)
{
    if (m_logSupressed)
        return;
    if (m_skipAdditionalDataInLog)
        return;
    if (qpTestLog_startShaderProgram(m_log, linkOk ? true : false, linkInfoLog) == false)
        throw LogWriteFailedError();
}

void TestLog::endShaderProgram(void)
{
    if (m_logSupressed)
        return;
    if (m_skipAdditionalDataInLog)
        return;
    if (qpTestLog_endShaderProgram(m_log) == false)
        throw LogWriteFailedError();
}

void TestLog::writeShader(qpShaderType type, const char *source, bool compileOk, const char *infoLog)
{
    if (m_logSupressed)
        return;
    if (m_skipAdditionalDataInLog)
        return;
    if (qpTestLog_writeShader(m_log, type, source, compileOk ? true : false, infoLog) == false)
        throw LogWriteFailedError();
}

void TestLog::writeSpirVAssemblySource(const char *source)
{
    if (m_logSupressed)
        return;
    if (m_skipAdditionalDataInLog)
        return;
    if (qpTestLog_writeSpirVAssemblySource(m_log, source) == false)
        throw LogWriteFailedError();
}

void TestLog::writeKernelSource(const char *source)
{
    if (m_logSupressed)
        return;
    if (m_skipAdditionalDataInLog)
        return;
    if (qpTestLog_writeKernelSource(m_log, source) == false)
        throw LogWriteFailedError();
}

void TestLog::writeCompileInfo(const char *name, const char *description, bool compileOk, const char *infoLog)
{
    if (m_logSupressed)
        return;
    if (m_skipAdditionalDataInLog)
        return;
    if (qpTestLog_writeCompileInfo(m_log, name, description, compileOk ? true : false, infoLog) == false)
        throw LogWriteFailedError();
}

void TestLog::writeFloat(const char *name, const char *description, const char *unit, qpKeyValueTag tag, float value)
{
    if (m_logSupressed)
        return;
    if (m_skipAdditionalDataInLog)
        return;
    if (qpTestLog_writeFloat(m_log, name, description, unit, tag, value) == false)
        throw LogWriteFailedError();
}

void TestLog::writeInteger(const char *name, const char *description, const char *unit, qpKeyValueTag tag,
                           int64_t value)
{
    if (m_logSupressed)
        return;
    if (m_skipAdditionalDataInLog)
        return;
    if (qpTestLog_writeInteger(m_log, name, description, unit, tag, value) == false)
        throw LogWriteFailedError();
}

void TestLog::startEglConfigSet(const char *name, const char *description)
{
    if (m_logSupressed)
        return;
    if (qpTestLog_startEglConfigSet(m_log, name, description) == false)
        throw LogWriteFailedError();
}

void TestLog::writeEglConfig(const qpEglConfigInfo *config)
{
    if (m_logSupressed)
        return;
    if (qpTestLog_writeEglConfig(m_log, config) == false)
        throw LogWriteFailedError();
}

void TestLog::endEglConfigSet(void)
{
    if (m_logSupressed)
        return;
    if (qpTestLog_endEglConfigSet(m_log) == false)
        throw LogWriteFailedError();
}

void TestLog::startCase(const char *testCasePath, qpTestCaseType testCaseType, const char *testCaseSource)
{
    if (m_logSupressed)
        return;
    if (qpTestLog_startCase(m_log, testCasePath, testCaseType, testCaseSource) == false)
        throw LogWriteFailedError();
    // Check if the test is one of those we want to print fully in the log
    m_skipAdditionalDataInLog = false;
    if (qpTestLog_isCompact(m_log))
    {
        const std::string testCasePathStr = testCasePath;
        if (testCasePathStr.rfind("dEQP-VK.info.") != 0 && testCasePathStr.rfind("dEQP-VK.api.info.") != 0 &&
            testCasePathStr.rfind("dEQP-VK.api.version_check.") != 0)
        {
            // We can skip writing text, numbers, imagesets, etc.
            m_skipAdditionalDataInLog = true;
        }
    }
}

void TestLog::endCase(qpTestResult result, const char *description)
{
    if (m_logSupressed)
        return;
    if (qpTestLog_endCase(m_log, result, description) == false)
        throw LogWriteFailedError();
}

void TestLog::terminateCase(qpTestResult result)
{
    if (m_logSupressed)
        return;
    if (qpTestLog_terminateCase(m_log, result) == false)
        throw LogWriteFailedError();
}

void TestLog::startTestsCasesTime(void)
{
    if (m_logSupressed)
        return;
    if (qpTestLog_startTestsCasesTime(m_log) == false)
        throw LogWriteFailedError();
}

void TestLog::endTestsCasesTime(void)
{
    if (m_logSupressed)
        return;
    if (qpTestLog_endTestsCasesTime(m_log) == false)
        throw LogWriteFailedError();
}

void TestLog::startSampleList(const std::string &name, const std::string &description)
{
    if (m_logSupressed)
        return;
    if (qpTestLog_startSampleList(m_log, name.c_str(), description.c_str()) == false)
        throw LogWriteFailedError();
}

void TestLog::startSampleInfo(void)
{
    if (m_logSupressed)
        return;
    if (qpTestLog_startSampleInfo(m_log) == false)
        throw LogWriteFailedError();
}

void TestLog::writeValueInfo(const std::string &name, const std::string &description, const std::string &unit,
                             qpSampleValueTag tag)
{
    if (m_logSupressed)
        return;
    if (qpTestLog_writeValueInfo(m_log, name.c_str(), description.c_str(), unit.empty() ? nullptr : unit.c_str(),
                                 tag) == false)
        throw LogWriteFailedError();
}

void TestLog::endSampleInfo(void)
{
    if (m_logSupressed)
        return;
    if (qpTestLog_endSampleInfo(m_log) == false)
        throw LogWriteFailedError();
}

void TestLog::startSample(void)
{
    if (m_logSupressed)
        return;
    if (qpTestLog_startSample(m_log) == false)
        throw LogWriteFailedError();
}

void TestLog::writeSampleValue(double value)
{
    if (m_logSupressed)
        return;
    if (qpTestLog_writeValueFloat(m_log, value) == false)
        throw LogWriteFailedError();
}

void TestLog::writeSampleValue(int64_t value)
{
    if (m_logSupressed)
        return;
    if (qpTestLog_writeValueInteger(m_log, value) == false)
        throw LogWriteFailedError();
}

void TestLog::endSample(void)
{
    if (m_logSupressed)
        return;
    if (qpTestLog_endSample(m_log) == false)
        throw LogWriteFailedError();
}

void TestLog::endSampleList(void)
{
    if (m_logSupressed)
        return;
    if (qpTestLog_endSampleList(m_log) == false)
        throw LogWriteFailedError();
}

void TestLog::writeRaw(const char *rawContents)
{
    if (m_logSupressed)
        return;
    qpTestLog_writeRaw(m_log, rawContents);
}

bool TestLog::isShaderLoggingEnabled(void)
{
    return (qpTestLog_getLogFlags(m_log) & QP_TEST_LOG_EXCLUDE_SHADER_SOURCES) == 0;
}

void TestLog::supressLogging(bool value)
{
    m_logSupressed = value;
}

bool TestLog::isSupressLogging(void)
{
    return m_logSupressed;
}

void TestLog::separateSlices(bool value)
{
    qpTestLog_setSplitSlices(m_log, value);
}

bool TestLog::isSeparateSlices(void) const
{
    return ((qpTestLog_getLogFlags(m_log) & QP_TEST_LOG_SPLIT_SLICES) != 0u);
}

const TestLog::BeginMessageToken TestLog::Message              = TestLog::BeginMessageToken();
const TestLog::EndMessageToken TestLog::EndMessage             = TestLog::EndMessageToken();
const TestLog::EndImageSetToken TestLog::EndImageSet           = TestLog::EndImageSetToken();
const TestLog::EndSectionToken TestLog::EndSection             = TestLog::EndSectionToken();
const TestLog::EndShaderProgramToken TestLog::EndShaderProgram = TestLog::EndShaderProgramToken();
const TestLog::SampleInfoToken TestLog::SampleInfo             = TestLog::SampleInfoToken();
const TestLog::EndSampleInfoToken TestLog::EndSampleInfo       = TestLog::EndSampleInfoToken();
const TestLog::BeginSampleToken TestLog::Sample                = TestLog::BeginSampleToken();
const TestLog::EndSampleToken TestLog::EndSample               = TestLog::EndSampleToken();
const TestLog::EndSampleListToken TestLog::EndSampleList       = TestLog::EndSampleListToken();

} // namespace tcu
