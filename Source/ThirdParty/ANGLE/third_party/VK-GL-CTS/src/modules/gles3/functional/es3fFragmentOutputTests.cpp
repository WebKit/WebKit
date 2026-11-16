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
 * \brief Fragment shader output tests.
 *
 * \todo [2012-04-10 pyry] Missing:
 *  + non-contiguous attachments in framebuffer
 *//*--------------------------------------------------------------------*/

#include "es3fFragmentOutputTests.hpp"
#include "gluShaderUtil.hpp"
#include "gluShaderProgram.hpp"
#include "gluTextureUtil.hpp"
#include "gluStrUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVector.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuImageCompare.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deMath.h"

// For getFormatName() \todo [pyry] Move to glu?
#include "es3fFboTestUtil.hpp"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

namespace deqp
{
namespace gles3
{
namespace Functional
{

using FboTestUtil::getFormatName;
using FboTestUtil::getFramebufferReadFormat;
using std::string;
using std::vector;
using tcu::BVec4;
using tcu::IVec2;
using tcu::IVec4;
using tcu::TestLog;
using tcu::UVec2;
using tcu::UVec4;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;

struct BufferSpec
{
    BufferSpec(void) : format(GL_NONE), width(0), height(0), samples(0)
    {
    }

    BufferSpec(uint32_t format_, int width_, int height_, int samples_)
        : format(format_)
        , width(width_)
        , height(height_)
        , samples(samples_)
    {
    }

    uint32_t format;
    int width;
    int height;
    int samples;
};

struct FragmentOutput
{
    FragmentOutput(void) : type(glu::TYPE_LAST), precision(glu::PRECISION_LAST), location(0), arrayLength(0)
    {
    }

    FragmentOutput(glu::DataType type_, glu::Precision precision_, int location_, int arrayLength_ = 0)
        : type(type_)
        , precision(precision_)
        , location(location_)
        , arrayLength(arrayLength_)
    {
    }

    glu::DataType type;
    glu::Precision precision;
    int location;
    int arrayLength; //!< 0 if not an array.
};

struct OutputVec
{
    vector<FragmentOutput> outputs;

    OutputVec &operator<<(const FragmentOutput &output)
    {
        outputs.push_back(output);
        return *this;
    }

    vector<FragmentOutput> toVec(void) const
    {
        return outputs;
    }
};

class FragmentOutputCase : public TestCase
{
public:
    FragmentOutputCase(Context &context, const char *name, const char *desc, const vector<BufferSpec> &fboSpec,
                       const vector<FragmentOutput> &outputs);
    ~FragmentOutputCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    FragmentOutputCase(const FragmentOutputCase &other);
    FragmentOutputCase &operator=(const FragmentOutputCase &other);

    vector<BufferSpec> m_fboSpec;
    vector<FragmentOutput> m_outputs;

    glu::ShaderProgram *m_program;
    uint32_t m_framebuffer;
    vector<uint32_t> m_renderbuffers;
};

FragmentOutputCase::FragmentOutputCase(Context &context, const char *name, const char *desc,
                                       const vector<BufferSpec> &fboSpec, const vector<FragmentOutput> &outputs)
    : TestCase(context, name, desc)
    , m_fboSpec(fboSpec)
    , m_outputs(outputs)
    , m_program(nullptr)
    , m_framebuffer(0)
{
}

FragmentOutputCase::~FragmentOutputCase(void)
{
    deinit();
}

static glu::ShaderProgram *createProgram(const glu::RenderContext &context, const vector<FragmentOutput> &outputs)
{
    std::ostringstream vtx;
    std::ostringstream frag;

    vtx << "#version 300 es\n"
        << "in highp vec4 a_position;\n";
    frag << "#version 300 es\n";

    // Input-output declarations.
    for (int outNdx = 0; outNdx < (int)outputs.size(); outNdx++)
    {
        const FragmentOutput &output = outputs[outNdx];
        bool isArray                 = output.arrayLength > 0;
        const char *typeName         = glu::getDataTypeName(output.type);
        const char *outputPrec       = glu::getPrecisionName(output.precision);
        bool isFloat                 = glu::isDataTypeFloatOrVec(output.type);
        const char *interp           = isFloat ? "smooth" : "flat";
        const char *interpPrec       = isFloat ? "highp" : outputPrec;

        if (isArray)
        {
            for (int elemNdx = 0; elemNdx < output.arrayLength; elemNdx++)
            {
                vtx << "in " << interpPrec << " " << typeName << " in" << outNdx << "_" << elemNdx << ";\n"
                    << interp << " out " << interpPrec << " " << typeName << " var" << outNdx << "_" << elemNdx
                    << ";\n";
                frag << interp << " in " << interpPrec << " " << typeName << " var" << outNdx << "_" << elemNdx
                     << ";\n";
            }
            frag << "layout(location = " << output.location << ") out " << outputPrec << " " << typeName << " out"
                 << outNdx << "[" << output.arrayLength << "];\n";
        }
        else
        {
            vtx << "in " << interpPrec << " " << typeName << " in" << outNdx << ";\n"
                << interp << " out " << interpPrec << " " << typeName << " var" << outNdx << ";\n";
            frag << interp << " in " << interpPrec << " " << typeName << " var" << outNdx << ";\n"
                 << "layout(location = " << output.location << ") out " << outputPrec << " " << typeName << " out"
                 << outNdx << ";\n";
        }
    }

    vtx << "\nvoid main()\n{\n";
    frag << "\nvoid main()\n{\n";

    vtx << "    gl_Position = a_position;\n";

    // Copy body
    for (int outNdx = 0; outNdx < (int)outputs.size(); outNdx++)
    {
        const FragmentOutput &output = outputs[outNdx];
        bool isArray                 = output.arrayLength > 0;

        if (isArray)
        {
            for (int elemNdx = 0; elemNdx < output.arrayLength; elemNdx++)
            {
                vtx << "\tvar" << outNdx << "_" << elemNdx << " = in" << outNdx << "_" << elemNdx << ";\n";
                frag << "\tout" << outNdx << "[" << elemNdx << "] = var" << outNdx << "_" << elemNdx << ";\n";
            }
        }
        else
        {
            vtx << "\tvar" << outNdx << " = in" << outNdx << ";\n";
            frag << "\tout" << outNdx << " = var" << outNdx << ";\n";
        }
    }

    vtx << "}\n";
    frag << "}\n";

    return new glu::ShaderProgram(context, glu::makeVtxFragSources(vtx.str(), frag.str()));
}

void FragmentOutputCase::init(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    TestLog &log             = m_testCtx.getLog();

    // Check that all attachments are supported
    for (std::vector<BufferSpec>::const_iterator bufIter = m_fboSpec.begin(); bufIter != m_fboSpec.end(); ++bufIter)
    {
        if (!glu::isSizedFormatColorRenderable(m_context.getRenderContext(), m_context.getContextInfo(),
                                               bufIter->format))
            throw tcu::NotSupportedError("Unsupported attachment format");
    }

    DE_ASSERT(!m_program);
    m_program = createProgram(m_context.getRenderContext(), m_outputs);

    log << *m_program;
    if (!m_program->isOk())
        TCU_FAIL("Compile failed");

    // Print render target info to log.
    log << TestLog::Section("Framebuffer", "Framebuffer configuration");

    for (int ndx = 0; ndx < (int)m_fboSpec.size(); ndx++)
        log << TestLog::Message << "COLOR_ATTACHMENT" << ndx << ": " << glu::getTextureFormatStr(m_fboSpec[ndx].format)
            << ", " << m_fboSpec[ndx].width << "x" << m_fboSpec[ndx].height << ", " << m_fboSpec[ndx].samples
            << " samples" << TestLog::EndMessage;

    log << TestLog::EndSection;

    // Create framebuffer.
    m_renderbuffers.resize(m_fboSpec.size(), 0);
    gl.genFramebuffers(1, &m_framebuffer);
    gl.genRenderbuffers((int)m_renderbuffers.size(), &m_renderbuffers[0]);

    gl.bindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

    for (int bufNdx = 0; bufNdx < (int)m_renderbuffers.size(); bufNdx++)
    {
        uint32_t rbo              = m_renderbuffers[bufNdx];
        const BufferSpec &bufSpec = m_fboSpec[bufNdx];
        uint32_t attachment       = GL_COLOR_ATTACHMENT0 + bufNdx;

        gl.bindRenderbuffer(GL_RENDERBUFFER, rbo);
        gl.renderbufferStorageMultisample(GL_RENDERBUFFER, bufSpec.samples, bufSpec.format, bufSpec.width,
                                          bufSpec.height);
        gl.framebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, rbo);
    }
    GLU_EXPECT_NO_ERROR(gl.getError(), "After framebuffer setup");

    uint32_t fboStatus = gl.checkFramebufferStatus(GL_FRAMEBUFFER);
    if (fboStatus == GL_FRAMEBUFFER_UNSUPPORTED)
        throw tcu::NotSupportedError("Framebuffer not supported", "", __FILE__, __LINE__);
    else if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
        throw tcu::TestError(
            (string("Incomplete framebuffer: ") + glu::getFramebufferStatusStr(fboStatus).toString()).c_str(), "",
            __FILE__, __LINE__);

    gl.bindFramebuffer(GL_FRAMEBUFFER, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "After init");
}

void FragmentOutputCase::deinit(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    if (m_framebuffer)
    {
        gl.deleteFramebuffers(1, &m_framebuffer);
        m_framebuffer = 0;
    }

    if (!m_renderbuffers.empty())
    {
        gl.deleteRenderbuffers((int)m_renderbuffers.size(), &m_renderbuffers[0]);
        m_renderbuffers.clear();
    }

    delete m_program;
    m_program = nullptr;
}

static IVec2 getMinSize(const vector<BufferSpec> &fboSpec)
{
    IVec2 minSize(0x7fffffff, 0x7fffffff);
    for (vector<BufferSpec>::const_iterator i = fboSpec.begin(); i != fboSpec.end(); i++)
    {
        minSize.x() = de::min(minSize.x(), i->width);
        minSize.y() = de::min(minSize.y(), i->height);
    }
    return minSize;
}

static int getNumInputVectors(const vector<FragmentOutput> &outputs)
{
    int numVecs = 0;
    for (vector<FragmentOutput>::const_iterator i = outputs.begin(); i != outputs.end(); i++)
        numVecs += (i->arrayLength > 0 ? i->arrayLength : 1);
    return numVecs;
}

static Vec2 getFloatRange(glu::Precision precision)
{
    // \todo [2012-04-09 pyry] Not quite the full ranges.
    static const Vec2 ranges[] = {Vec2(-2.0f, 2.0f), Vec2(-16000.0f, 16000.0f), Vec2(-1e35f, 1e35f)};
    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(ranges) == glu::PRECISION_LAST);
    DE_ASSERT(de::inBounds<int>(precision, 0, DE_LENGTH_OF_ARRAY(ranges)));
    return ranges[precision];
}

static IVec2 getIntRange(glu::Precision precision)
{
    static const IVec2 ranges[] = {IVec2(-(1 << 7), (1 << 7) - 1), IVec2(-(1 << 15), (1 << 15) - 1),
                                   IVec2(0x80000000, 0x7fffffff)};
    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(ranges) == glu::PRECISION_LAST);
    DE_ASSERT(de::inBounds<int>(precision, 0, DE_LENGTH_OF_ARRAY(ranges)));
    return ranges[precision];
}

static UVec2 getUintRange(glu::Precision precision)
{
    static const UVec2 ranges[] = {UVec2(0, (1 << 8) - 1), UVec2(0, (1 << 16) - 1), UVec2(0, 0xffffffffu)};
    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(ranges) == glu::PRECISION_LAST);
    DE_ASSERT(de::inBounds<int>(precision, 0, DE_LENGTH_OF_ARRAY(ranges)));
    return ranges[precision];
}

static inline Vec4 readVec4(const float *ptr, int numComponents)
{
    DE_ASSERT(numComponents >= 1);
    return Vec4(ptr[0], numComponents >= 2 ? ptr[1] : 0.0f, numComponents >= 3 ? ptr[2] : 0.0f,
                numComponents >= 4 ? ptr[3] : 0.0f);
}

static inline IVec4 readIVec4(const int *ptr, int numComponents)
{
    DE_ASSERT(numComponents >= 1);
    return IVec4(ptr[0], numComponents >= 2 ? ptr[1] : 0, numComponents >= 3 ? ptr[2] : 0,
                 numComponents >= 4 ? ptr[3] : 0);
}

static void renderFloatReference(const tcu::PixelBufferAccess &dst, int gridWidth, int gridHeight, int numComponents,
                                 const float *vertices)
{
    const bool isSRGB = tcu::isSRGB(dst.getFormat());
    const float cellW = (float)dst.getWidth() / (float)(gridWidth - 1);
    const float cellH = (float)dst.getHeight() / (float)(gridHeight - 1);

    for (int y = 0; y < dst.getHeight(); y++)
    {
        for (int x = 0; x < dst.getWidth(); x++)
        {
            const int cellX = de::clamp(deFloorFloatToInt32((float)x / cellW), 0, gridWidth - 2);
            const int cellY = de::clamp(deFloorFloatToInt32((float)y / cellH), 0, gridHeight - 2);
            const float xf  = ((float)x - (float)cellX * cellW + 0.5f) / cellW;
            const float yf  = ((float)y - (float)cellY * cellH + 0.5f) / cellH;
            const Vec4 v00  = readVec4(vertices + ((cellY + 0) * gridWidth + cellX + 0) * numComponents, numComponents);
            const Vec4 v01  = readVec4(vertices + ((cellY + 1) * gridWidth + cellX + 0) * numComponents, numComponents);
            const Vec4 v10  = readVec4(vertices + ((cellY + 0) * gridWidth + cellX + 1) * numComponents, numComponents);
            const Vec4 v11  = readVec4(vertices + ((cellY + 1) * gridWidth + cellX + 1) * numComponents, numComponents);
            const bool tri  = xf + yf >= 1.0f;
            const Vec4 &v0  = tri ? v11 : v00;
            const Vec4 &v1  = tri ? v01 : v10;
            const Vec4 &v2  = tri ? v10 : v01;
            const float s   = tri ? 1.0f - xf : xf;
            const float t   = tri ? 1.0f - yf : yf;
            const Vec4 color = v0 + (v1 - v0) * s + (v2 - v0) * t;

            dst.setPixel(isSRGB ? tcu::linearToSRGB(color) : color, x, y);
        }
    }
}

static void renderIntReference(const tcu::PixelBufferAccess &dst, int gridWidth, int gridHeight, int numComponents,
                               const int *vertices)
{
    float cellW = (float)dst.getWidth() / (float)(gridWidth - 1);
    float cellH = (float)dst.getHeight() / (float)(gridHeight - 1);

    for (int y = 0; y < dst.getHeight(); y++)
    {
        for (int x = 0; x < dst.getWidth(); x++)
        {
            int cellX = de::clamp(deFloorFloatToInt32((float)x / cellW), 0, gridWidth - 2);
            int cellY = de::clamp(deFloorFloatToInt32((float)y / cellH), 0, gridHeight - 2);
            IVec4 c   = readIVec4(vertices + (cellY * gridWidth + cellX + 1) * numComponents, numComponents);

            dst.setPixel(c, x, y);
        }
    }
}

static const IVec4 s_swizzles[] = {IVec4(0, 1, 2, 3), IVec4(1, 2, 3, 0), IVec4(2, 3, 0, 1), IVec4(3, 0, 1, 2),
                                   IVec4(3, 2, 1, 0), IVec4(2, 1, 0, 3), IVec4(1, 0, 3, 2), IVec4(0, 3, 2, 1)};

template <typename T>
inline tcu::Vector<T, 4> swizzleVec(const tcu::Vector<T, 4> &vec, int swzNdx)
{
    const IVec4 &swz = s_swizzles[swzNdx % DE_LENGTH_OF_ARRAY(s_swizzles)];
    return vec.swizzle(swz[0], swz[1], swz[2], swz[3]);
}

namespace
{

struct AttachmentData
{
    tcu::TextureFormat format;          //!< Actual format of attachment.
    tcu::TextureFormat referenceFormat; //!< Used for reference rendering.
    tcu::TextureFormat readFormat;
    int numWrittenChannels;
    glu::Precision outPrecision;
    vector<uint8_t> renderedData;
    vector<uint8_t> referenceData;
};

template <typename Type>
string valueRangeToString(int numValidChannels, const tcu::Vector<Type, 4> &minValue,
                          const tcu::Vector<Type, 4> &maxValue)
{
    std::ostringstream stream;

    stream << "(";

    for (int i = 0; i < 4; i++)
    {
        if (i != 0)
            stream << ", ";

        if (i < numValidChannels)
            stream << minValue[i] << " -> " << maxValue[i];
        else
            stream << "Undef";
    }

    stream << ")";

    return stream.str();
}

void clearUndefined(const tcu::PixelBufferAccess &access, int numValidChannels)
{
    for (int y = 0; y < access.getHeight(); y++)
        for (int x = 0; x < access.getWidth(); x++)
        {
            switch (tcu::getTextureChannelClass(access.getFormat().type))
            {
            case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
            {
                const Vec4 srcPixel = access.getPixel(x, y);
                Vec4 dstPixel(0.0f, 0.0f, 0.0f, 1.0f);

                for (int channelNdx = 0; channelNdx < numValidChannels; channelNdx++)
                    dstPixel[channelNdx] = srcPixel[channelNdx];

                access.setPixel(dstPixel, x, y);
                break;
            }

            case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
            case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
            case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
            case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
            {
                const IVec4 bitDepth = tcu::getTextureFormatBitDepth(access.getFormat());
                const IVec4 srcPixel = access.getPixelInt(x, y);
                IVec4 dstPixel(0, 0, 0, (int)(((int64_t)0x1u << bitDepth.w()) - 1));

                for (int channelNdx = 0; channelNdx < numValidChannels; channelNdx++)
                    dstPixel[channelNdx] = srcPixel[channelNdx];

                access.setPixel(dstPixel, x, y);
                break;
            }

            default:
                DE_ASSERT(false);
            }
        }
}

} // namespace

FragmentOutputCase::IterateResult FragmentOutputCase::iterate(void)
{
    TestLog &log             = m_testCtx.getLog();
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    // Compute grid size & index list.
    const int minCellSize  = 8;
    const IVec2 minBufSize = getMinSize(m_fboSpec);
    const int gridWidth    = de::clamp(minBufSize.x() / minCellSize, 1, 255) + 1;
    const int gridHeight   = de::clamp(minBufSize.y() / minCellSize, 1, 255) + 1;
    const int numVertices  = gridWidth * gridHeight;
    const int numQuads     = (gridWidth - 1) * (gridHeight - 1);
    const int numIndices   = numQuads * 6;

    const int numInputVecs = getNumInputVectors(m_outputs);
    vector<vector<uint32_t>> inputs(numInputVecs);
    vector<float> positions(numVertices * 4);
    vector<uint16_t> indices(numIndices);

    const int readAlignment  = 4;
    const int viewportW      = minBufSize.x();
    const int viewportH      = minBufSize.y();
    const int numAttachments = (int)m_fboSpec.size();

    vector<uint32_t> drawBuffers(numAttachments);
    vector<AttachmentData> attachments(numAttachments);

    // Initialize attachment data.
    for (int ndx = 0; ndx < numAttachments; ndx++)
    {
        const tcu::TextureFormat texFmt         = glu::mapGLInternalFormat(m_fboSpec[ndx].format);
        const tcu::TextureChannelClass chnClass = tcu::getTextureChannelClass(texFmt.type);
        const bool isFixedPoint                 = chnClass == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT ||
                                  chnClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT;

        // \note Fixed-point formats use float reference to enable more accurate result verification.
        const tcu::TextureFormat refFmt =
            isFixedPoint ? tcu::TextureFormat(texFmt.order, tcu::TextureFormat::FLOAT) : texFmt;
        const tcu::TextureFormat readFmt = getFramebufferReadFormat(texFmt);
        const int attachmentW            = m_fboSpec[ndx].width;
        const int attachmentH            = m_fboSpec[ndx].height;

        drawBuffers[ndx]                 = GL_COLOR_ATTACHMENT0 + ndx;
        attachments[ndx].format          = texFmt;
        attachments[ndx].readFormat      = readFmt;
        attachments[ndx].referenceFormat = refFmt;
        attachments[ndx].renderedData.resize(readFmt.getPixelSize() * attachmentW * attachmentH);
        attachments[ndx].referenceData.resize(refFmt.getPixelSize() * attachmentW * attachmentH);
    }

    // Initialize indices.
    for (int quadNdx = 0; quadNdx < numQuads; quadNdx++)
    {
        int quadY = quadNdx / (gridWidth - 1);
        int quadX = quadNdx - quadY * (gridWidth - 1);

        indices[quadNdx * 6 + 0] = uint16_t(quadX + quadY * gridWidth);
        indices[quadNdx * 6 + 1] = uint16_t(quadX + (quadY + 1) * gridWidth);
        indices[quadNdx * 6 + 2] = uint16_t(quadX + quadY * gridWidth + 1);
        indices[quadNdx * 6 + 3] = indices[quadNdx * 6 + 1];
        indices[quadNdx * 6 + 4] = uint16_t(quadX + (quadY + 1) * gridWidth + 1);
        indices[quadNdx * 6 + 5] = indices[quadNdx * 6 + 2];
    }

    for (int y = 0; y < gridHeight; y++)
    {
        for (int x = 0; x < gridWidth; x++)
        {
            float xf = (float)x / (float)(gridWidth - 1);
            float yf = (float)y / (float)(gridHeight - 1);

            positions[(y * gridWidth + x) * 4 + 0] = 2.0f * xf - 1.0f;
            positions[(y * gridWidth + x) * 4 + 1] = 2.0f * yf - 1.0f;
            positions[(y * gridWidth + x) * 4 + 2] = 0.0f;
            positions[(y * gridWidth + x) * 4 + 3] = 1.0f;
        }
    }

    // Initialize input vectors.
    {
        int curInVec = 0;
        for (int outputNdx = 0; outputNdx < (int)m_outputs.size(); outputNdx++)
        {
            const FragmentOutput &output = m_outputs[outputNdx];
            bool isFloat                 = glu::isDataTypeFloatOrVec(output.type);
            bool isInt                   = glu::isDataTypeIntOrIVec(output.type);
            bool isUint                  = glu::isDataTypeUintOrUVec(output.type);
            int numVecs                  = output.arrayLength > 0 ? output.arrayLength : 1;
            int numScalars               = glu::getDataTypeScalarSize(output.type);

            for (int vecNdx = 0; vecNdx < numVecs; vecNdx++)
            {
                inputs[curInVec].resize(numVertices * numScalars);

                // Record how many outputs are written in attachment.
                DE_ASSERT(output.location + vecNdx < (int)attachments.size());
                attachments[output.location + vecNdx].numWrittenChannels = numScalars;
                attachments[output.location + vecNdx].outPrecision       = output.precision;

                if (isFloat)
                {
                    Vec2 range = getFloatRange(output.precision);
                    Vec4 minVal(range.x());
                    Vec4 maxVal(range.y());
                    float *dst = (float *)&inputs[curInVec][0];

                    if (de::inBounds(output.location + vecNdx, 0, (int)attachments.size()))
                    {
                        // \note Floating-point precision conversion is not well-defined. For that reason we must
                        //       limit value range to intersection of both data type and render target value ranges.
                        const tcu::TextureFormatInfo fmtInfo =
                            tcu::getTextureFormatInfo(attachments[output.location + vecNdx].format);
                        minVal = tcu::max(minVal, fmtInfo.valueMin);
                        maxVal = tcu::min(maxVal, fmtInfo.valueMax);
                    }

                    m_testCtx.getLog() << TestLog::Message << "out" << curInVec
                                       << " value range: " << valueRangeToString(numScalars, minVal, maxVal)
                                       << TestLog::EndMessage;

                    for (int y = 0; y < gridHeight; y++)
                    {
                        for (int x = 0; x < gridWidth; x++)
                        {
                            float xf = (float)x / (float)(gridWidth - 1);
                            float yf = (float)y / (float)(gridHeight - 1);

                            float f0 = (xf + yf) * 0.5f;
                            float f1 = 0.5f + (xf - yf) * 0.5f;
                            Vec4 f   = swizzleVec(Vec4(f0, f1, 1.0f - f0, 1.0f - f1), curInVec);
                            Vec4 c   = minVal + (maxVal - minVal) * f;
                            float *v = dst + (y * gridWidth + x) * numScalars;

                            for (int ndx = 0; ndx < numScalars; ndx++)
                                v[ndx] = c[ndx];
                        }
                    }
                }
                else if (isInt)
                {
                    const IVec2 range = getIntRange(output.precision);
                    IVec4 minVal(range.x());
                    IVec4 maxVal(range.y());

                    if (de::inBounds(output.location + vecNdx, 0, (int)attachments.size()))
                    {
                        // Limit to range of output format as conversion mode is not specified.
                        const IVec4 fmtBits =
                            tcu::getTextureFormatBitDepth(attachments[output.location + vecNdx].format);
                        const BVec4 isZero    = lessThanEqual(fmtBits, IVec4(0));
                        const IVec4 shift     = tcu::clamp(fmtBits - 1, tcu::IVec4(0), tcu::IVec4(256));
                        const IVec4 fmtMinVal = (-(tcu::Vector<int64_t, 4>(1) << shift.cast<int64_t>())).asInt();
                        const IVec4 fmtMaxVal =
                            ((tcu::Vector<int64_t, 4>(1) << shift.cast<int64_t>()) - int64_t(1)).asInt();

                        minVal = select(minVal, tcu::max(minVal, fmtMinVal), isZero);
                        maxVal = select(maxVal, tcu::min(maxVal, fmtMaxVal), isZero);
                    }

                    m_testCtx.getLog() << TestLog::Message << "out" << curInVec
                                       << " value range: " << valueRangeToString(numScalars, minVal, maxVal)
                                       << TestLog::EndMessage;

                    const IVec4 rangeDiv =
                        swizzleVec((IVec4(gridWidth, gridHeight, gridWidth, gridHeight) - 1), curInVec);
                    const IVec4 step =
                        ((maxVal.cast<int64_t>() - minVal.cast<int64_t>()) / (rangeDiv.cast<int64_t>())).asInt();
                    int32_t *dst = (int32_t *)&inputs[curInVec][0];

                    for (int y = 0; y < gridHeight; y++)
                    {
                        for (int x = 0; x < gridWidth; x++)
                        {
                            int ix     = gridWidth - x - 1;
                            int iy     = gridHeight - y - 1;
                            IVec4 c    = minVal + step * swizzleVec(IVec4(x, y, ix, iy), curInVec);
                            int32_t *v = dst + (y * gridWidth + x) * numScalars;

                            DE_ASSERT(boolAll(logicalAnd(greaterThanEqual(c, minVal), lessThanEqual(c, maxVal))));

                            for (int ndx = 0; ndx < numScalars; ndx++)
                                v[ndx] = c[ndx];
                        }
                    }
                }
                else if (isUint)
                {
                    const UVec2 range = getUintRange(output.precision);
                    UVec4 maxVal(range.y());

                    if (de::inBounds(output.location + vecNdx, 0, (int)attachments.size()))
                    {
                        // Limit to range of output format as conversion mode is not specified.
                        const IVec4 fmtBits =
                            tcu::getTextureFormatBitDepth(attachments[output.location + vecNdx].format);
                        const UVec4 fmtMaxVal =
                            ((tcu::Vector<uint64_t, 4>(1) << fmtBits.cast<uint64_t>()) - uint64_t(1)).asUint();

                        maxVal = tcu::min(maxVal, fmtMaxVal);
                    }

                    m_testCtx.getLog() << TestLog::Message << "out" << curInVec
                                       << " value range: " << valueRangeToString(numScalars, UVec4(0), maxVal)
                                       << TestLog::EndMessage;

                    const IVec4 rangeDiv =
                        swizzleVec((IVec4(gridWidth, gridHeight, gridWidth, gridHeight) - 1), curInVec);
                    const UVec4 step = maxVal / rangeDiv.asUint();
                    uint32_t *dst    = &inputs[curInVec][0];

                    DE_ASSERT(range.x() == 0);

                    for (int y = 0; y < gridHeight; y++)
                    {
                        for (int x = 0; x < gridWidth; x++)
                        {
                            int ix      = gridWidth - x - 1;
                            int iy      = gridHeight - y - 1;
                            UVec4 c     = step * swizzleVec(IVec4(x, y, ix, iy).asUint(), curInVec);
                            uint32_t *v = dst + (y * gridWidth + x) * numScalars;

                            DE_ASSERT(boolAll(lessThanEqual(c, maxVal)));

                            for (int ndx = 0; ndx < numScalars; ndx++)
                                v[ndx] = c[ndx];
                        }
                    }
                }
                else
                    DE_ASSERT(false);

                curInVec += 1;
            }
        }
    }

    // Render using gl.
    gl.useProgram(m_program->getProgram());
    gl.bindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    gl.viewport(0, 0, viewportW, viewportH);
    gl.drawBuffers((int)drawBuffers.size(), &drawBuffers[0]);
    gl.disable(
        GL_DITHER); // Dithering causes issues with unorm formats. Those issues could be worked around in threshold, but it makes validation less accurate.
    GLU_EXPECT_NO_ERROR(gl.getError(), "After program setup");

    {
        int curInVec = 0;
        for (int outputNdx = 0; outputNdx < (int)m_outputs.size(); outputNdx++)
        {
            const FragmentOutput &output = m_outputs[outputNdx];
            bool isArray                 = output.arrayLength > 0;
            bool isFloat                 = glu::isDataTypeFloatOrVec(output.type);
            bool isInt                   = glu::isDataTypeIntOrIVec(output.type);
            bool isUint                  = glu::isDataTypeUintOrUVec(output.type);
            int scalarSize               = glu::getDataTypeScalarSize(output.type);
            uint32_t glScalarType        = isFloat ? GL_FLOAT : isInt ? GL_INT : isUint ? GL_UNSIGNED_INT : GL_NONE;
            int numVecs                  = isArray ? output.arrayLength : 1;

            for (int vecNdx = 0; vecNdx < numVecs; vecNdx++)
            {
                string name =
                    string("in") + de::toString(outputNdx) + (isArray ? string("_") + de::toString(vecNdx) : string());
                int loc = gl.getAttribLocation(m_program->getProgram(), name.c_str());

                if (loc >= 0)
                {
                    gl.enableVertexAttribArray(loc);
                    if (isFloat)
                        gl.vertexAttribPointer(loc, scalarSize, glScalarType, GL_FALSE, 0, &inputs[curInVec][0]);
                    else
                        gl.vertexAttribIPointer(loc, scalarSize, glScalarType, 0, &inputs[curInVec][0]);
                }
                else
                    log << TestLog::Message << "Warning: No location for attribute '" << name << "' found."
                        << TestLog::EndMessage;

                curInVec += 1;
            }
        }
    }
    {
        int posLoc = gl.getAttribLocation(m_program->getProgram(), "a_position");
        TCU_CHECK(posLoc >= 0);
        gl.enableVertexAttribArray(posLoc);
        gl.vertexAttribPointer(posLoc, 4, GL_FLOAT, GL_FALSE, 0, &positions[0]);
    }
    GLU_EXPECT_NO_ERROR(gl.getError(), "After attribute setup");

    gl.drawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT, &indices[0]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDrawElements");

    // Read all attachment points.
    for (int ndx = 0; ndx < numAttachments; ndx++)
    {
        const glu::TransferFormat transferFmt = glu::getTransferFormat(attachments[ndx].readFormat);
        void *dst                             = &attachments[ndx].renderedData[0];
        const int attachmentW                 = m_fboSpec[ndx].width;
        const int attachmentH                 = m_fboSpec[ndx].height;
        const int numValidChannels            = attachments[ndx].numWrittenChannels;
        const tcu::PixelBufferAccess rendered(
            attachments[ndx].readFormat, attachmentW, attachmentH, 1,
            deAlign32(attachments[ndx].readFormat.getPixelSize() * attachmentW, readAlignment), 0,
            &attachments[ndx].renderedData[0]);

        gl.readBuffer(GL_COLOR_ATTACHMENT0 + ndx);
        gl.readPixels(0, 0, minBufSize.x(), minBufSize.y(), transferFmt.format, transferFmt.dataType, dst);

        clearUndefined(rendered, numValidChannels);
    }

    // Render reference images.
    {
        int curInNdx = 0;
        for (int outputNdx = 0; outputNdx < (int)m_outputs.size(); outputNdx++)
        {
            const FragmentOutput &output = m_outputs[outputNdx];
            const bool isArray           = output.arrayLength > 0;
            const bool isFloat           = glu::isDataTypeFloatOrVec(output.type);
            const bool isInt             = glu::isDataTypeIntOrIVec(output.type);
            const bool isUint            = glu::isDataTypeUintOrUVec(output.type);
            const int scalarSize         = glu::getDataTypeScalarSize(output.type);
            const int numVecs            = isArray ? output.arrayLength : 1;

            for (int vecNdx = 0; vecNdx < numVecs; vecNdx++)
            {
                const int location    = output.location + vecNdx;
                const void *inputData = &inputs[curInNdx][0];

                DE_ASSERT(de::inBounds(location, 0, (int)m_fboSpec.size()));

                const int bufW = m_fboSpec[location].width;
                const int bufH = m_fboSpec[location].height;
                const tcu::PixelBufferAccess buf(attachments[location].referenceFormat, bufW, bufH, 1,
                                                 &attachments[location].referenceData[0]);
                const tcu::PixelBufferAccess viewportBuf = getSubregion(buf, 0, 0, 0, viewportW, viewportH, 1);

                if (isInt || isUint)
                    renderIntReference(viewportBuf, gridWidth, gridHeight, scalarSize, (const int *)inputData);
                else if (isFloat)
                    renderFloatReference(viewportBuf, gridWidth, gridHeight, scalarSize, (const float *)inputData);
                else
                    DE_ASSERT(false);

                curInNdx += 1;
            }
        }
    }

    // Compare all images.
    bool allLevelsOk = true;
    for (int attachNdx = 0; attachNdx < numAttachments; attachNdx++)
    {
        const int attachmentW      = m_fboSpec[attachNdx].width;
        const int attachmentH      = m_fboSpec[attachNdx].height;
        const int numValidChannels = attachments[attachNdx].numWrittenChannels;
        const tcu::BVec4 cmpMask(numValidChannels >= 1, numValidChannels >= 2, numValidChannels >= 3,
                                 numValidChannels >= 4);
        const glu::Precision outPrecision = attachments[attachNdx].outPrecision;
        const tcu::TextureFormat &format  = attachments[attachNdx].format;
        tcu::ConstPixelBufferAccess rendered(
            attachments[attachNdx].readFormat, attachmentW, attachmentH, 1,
            deAlign32(attachments[attachNdx].readFormat.getPixelSize() * attachmentW, readAlignment), 0,
            &attachments[attachNdx].renderedData[0]);
        tcu::ConstPixelBufferAccess reference(attachments[attachNdx].referenceFormat, attachmentW, attachmentH, 1,
                                              &attachments[attachNdx].referenceData[0]);
        tcu::TextureChannelClass texClass = tcu::getTextureChannelClass(format.type);
        bool isOk                         = true;
        const string name                 = string("Attachment") + de::toString(attachNdx);
        const string desc                 = string("Color attachment ") + de::toString(attachNdx);

        log << TestLog::Message << "Attachment " << attachNdx << ": " << numValidChannels
            << " channels have defined values and used for comparison" << TestLog::EndMessage;

        switch (texClass)
        {
        case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
        {
            const uint32_t interpThreshold = 4; //!< 4 ULP interpolation threshold (interpolation always in highp)
            uint32_t outTypeThreshold      = 0; //!< Threshold based on output type
            UVec4 formatThreshold;              //!< Threshold computed based on format.
            UVec4 finalThreshold;

            // 1 ULP rounding error is allowed for smaller floating-point formats
            switch (format.type)
            {
            case tcu::TextureFormat::FLOAT:
                formatThreshold = UVec4(0);
                break;
            case tcu::TextureFormat::HALF_FLOAT:
                formatThreshold = UVec4((1 << (23 - 10)));
                break;
            case tcu::TextureFormat::UNSIGNED_INT_11F_11F_10F_REV:
                formatThreshold = UVec4((1 << (23 - 6)), (1 << (23 - 6)), (1 << (23 - 5)), 0);
                break;
            default:
                DE_ASSERT(false);
                break;
            }

            // 1 ULP rounding error for highp -> output precision cast
            switch (outPrecision)
            {
            case glu::PRECISION_LOWP:
                outTypeThreshold = (1 << (23 - 8));
                break;
            case glu::PRECISION_MEDIUMP:
                outTypeThreshold = (1 << (23 - 10));
                break;
            case glu::PRECISION_HIGHP:
                outTypeThreshold = 0;
                break;
            default:
                DE_ASSERT(false);
            }

            finalThreshold =
                select(max(formatThreshold, UVec4(deMax32(interpThreshold, outTypeThreshold))), UVec4(~0u), cmpMask);

            isOk = tcu::floatUlpThresholdCompare(log, name.c_str(), desc.c_str(), reference, rendered, finalThreshold,
                                                 tcu::COMPARE_LOG_RESULT);
            break;
        }

        case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
        {
            // \note glReadPixels() allows only 8 bits to be read. This means that RGB10_A2 will loose some
            // bits in the process and it must be taken into account when computing threshold.
            const IVec4 bits         = min(IVec4(8), tcu::getTextureFormatBitDepth(format));
            const Vec4 baseThreshold = 1.0f / ((IVec4(1) << bits) - 1).asFloat();
            const Vec4 threshold     = select(baseThreshold, Vec4(2.0f), cmpMask);

            isOk = tcu::floatThresholdCompare(log, name.c_str(), desc.c_str(), reference, rendered, threshold,
                                              tcu::COMPARE_LOG_RESULT);
            break;
        }

        case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
        case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
        {
            const tcu::UVec4 threshold = select(UVec4(0u), UVec4(~0u), cmpMask);
            isOk = tcu::intThresholdCompare(log, name.c_str(), desc.c_str(), reference, rendered, threshold,
                                            tcu::COMPARE_LOG_RESULT);
            break;
        }

        default:
            TCU_FAIL("Unsupported comparison");
        }

        if (!isOk)
            allLevelsOk = false;
    }

    m_testCtx.setTestResult(allLevelsOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                            allLevelsOk ? "Pass" : "Image comparison failed");
    return STOP;
}

FragmentOutputTests::FragmentOutputTests(Context &context)
    : TestCaseGroup(context, "fragment_out", "Fragment output tests")
{
}

FragmentOutputTests::~FragmentOutputTests(void)
{
}

static FragmentOutputCase *createRandomCase(Context &context, int minRenderTargets, int maxRenderTargets, uint32_t seed)
{
    static const glu::DataType outputTypes[] = {glu::TYPE_FLOAT,      glu::TYPE_FLOAT_VEC2, glu::TYPE_FLOAT_VEC3,
                                                glu::TYPE_FLOAT_VEC4, glu::TYPE_INT,        glu::TYPE_INT_VEC2,
                                                glu::TYPE_INT_VEC3,   glu::TYPE_INT_VEC4,   glu::TYPE_UINT,
                                                glu::TYPE_UINT_VEC2,  glu::TYPE_UINT_VEC3,  glu::TYPE_UINT_VEC4};
    static const glu::Precision precisions[] = {glu::PRECISION_LOWP, glu::PRECISION_MEDIUMP, glu::PRECISION_HIGHP};
    static const uint32_t floatFormats[]     = {
        GL_RGBA32F,      GL_RGBA16F,  GL_R11F_G11F_B10F, GL_RG32F,   GL_RG16F, GL_R32F,   GL_R16F, GL_RGBA8,
        GL_SRGB8_ALPHA8, GL_RGB10_A2, GL_RGBA4,          GL_RGB5_A1, GL_RGB8,  GL_RGB565, GL_RG8,  GL_R8};
    static const uint32_t intFormats[]  = {GL_RGBA32I, GL_RGBA16I, GL_RGBA8I, GL_RG32I, GL_RG16I,
                                           GL_RG8I,    GL_R32I,    GL_R16I,   GL_R8I};
    static const uint32_t uintFormats[] = {GL_RGBA32UI, GL_RGBA16UI, GL_RGBA8UI, GL_RGB10_A2UI, GL_RG32UI,
                                           GL_RG16UI,   GL_RG8UI,    GL_R32UI,   GL_R16UI,      GL_R8UI};

    de::Random rnd(seed);
    vector<FragmentOutput> outputs;
    vector<BufferSpec> targets;
    vector<glu::DataType> outTypes;

    int numTargets    = rnd.getInt(minRenderTargets, maxRenderTargets);
    const int width   = 128; // \todo [2012-04-10 pyry] Separate randomized sizes per target?
    const int height  = 64;
    const int samples = 0;

    // Compute outputs.
    int curLoc = 0;
    while (curLoc < numTargets)
    {
        bool useArray   = rnd.getFloat() < 0.3f;
        int maxArrayLen = numTargets - curLoc;
        int arrayLen    = useArray ? rnd.getInt(1, maxArrayLen) : 0;
        glu::DataType basicType =
            rnd.choose<glu::DataType>(&outputTypes[0], &outputTypes[0] + DE_LENGTH_OF_ARRAY(outputTypes));
        glu::Precision precision =
            rnd.choose<glu::Precision>(&precisions[0], &precisions[0] + DE_LENGTH_OF_ARRAY(precisions));
        int numLocations = useArray ? arrayLen : 1;

        outputs.push_back(FragmentOutput(basicType, precision, curLoc, arrayLen));

        for (int ndx = 0; ndx < numLocations; ndx++)
            outTypes.push_back(basicType);

        curLoc += numLocations;
    }
    DE_ASSERT(curLoc == numTargets);
    DE_ASSERT((int)outTypes.size() == numTargets);

    // Compute buffers.
    while ((int)targets.size() < numTargets)
    {
        glu::DataType outType = outTypes[targets.size()];
        bool isFloat          = glu::isDataTypeFloatOrVec(outType);
        bool isInt            = glu::isDataTypeIntOrIVec(outType);
        bool isUint           = glu::isDataTypeUintOrUVec(outType);
        uint32_t format       = 0;

        if (isFloat)
            format = rnd.choose<uint32_t>(&floatFormats[0], &floatFormats[0] + DE_LENGTH_OF_ARRAY(floatFormats));
        else if (isInt)
            format = rnd.choose<uint32_t>(&intFormats[0], &intFormats[0] + DE_LENGTH_OF_ARRAY(intFormats));
        else if (isUint)
            format = rnd.choose<uint32_t>(&uintFormats[0], &uintFormats[0] + DE_LENGTH_OF_ARRAY(uintFormats));
        else
            DE_ASSERT(false);

        targets.push_back(BufferSpec(format, width, height, samples));
    }

    return new FragmentOutputCase(context, de::toString(seed).c_str(), "", targets, outputs);
}

void FragmentOutputTests::init(void)
{
    static const uint32_t requiredFloatFormats[] = {GL_RGBA32F, GL_RGBA16F, GL_R11F_G11F_B10F, GL_RG32F, GL_RG16F,
                                                    GL_R32F,    GL_R16F};
    static const uint32_t requiredFixedFormats[] = {GL_RGBA8, GL_SRGB8_ALPHA8, GL_RGB10_A2, GL_RGBA4, GL_RGB5_A1,
                                                    GL_RGB8,  GL_RGB565,       GL_RG8,      GL_R8};
    static const uint32_t requiredIntFormats[]   = {GL_RGBA32I, GL_RGBA16I, GL_RGBA8I, GL_RG32I, GL_RG16I,
                                                    GL_RG8I,    GL_R32I,    GL_R16I,   GL_R8I};
    static const uint32_t requiredUintFormats[]  = {GL_RGBA32UI, GL_RGBA16UI, GL_RGBA8UI, GL_RGB10_A2UI, GL_RG32UI,
                                                    GL_RG16UI,   GL_RG8UI,    GL_R32UI,   GL_R16UI,      GL_R8UI};

    static const glu::Precision precisions[] = {glu::PRECISION_LOWP, glu::PRECISION_MEDIUMP, glu::PRECISION_HIGHP};

    // .basic.
    {
        tcu::TestCaseGroup *basicGroup = new tcu::TestCaseGroup(m_testCtx, "basic", "Basic fragment output tests");
        addChild(basicGroup);

        const int width   = 64;
        const int height  = 64;
        const int samples = 0;

        // .float
        tcu::TestCaseGroup *floatGroup = new tcu::TestCaseGroup(m_testCtx, "float", "Floating-point output tests");
        basicGroup->addChild(floatGroup);
        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(requiredFloatFormats); fmtNdx++)
        {
            uint32_t format = requiredFloatFormats[fmtNdx];
            string fmtName  = getFormatName(format);
            vector<BufferSpec> fboSpec;

            fboSpec.push_back(BufferSpec(format, width, height, samples));

            for (int precNdx = 0; precNdx < DE_LENGTH_OF_ARRAY(precisions); precNdx++)
            {
                glu::Precision prec = precisions[precNdx];
                string precName     = glu::getPrecisionName(prec);

                floatGroup->addChild(
                    new FragmentOutputCase(m_context, (fmtName + "_" + precName + "_float").c_str(), "", fboSpec,
                                           (OutputVec() << FragmentOutput(glu::TYPE_FLOAT, prec, 0)).toVec()));
                floatGroup->addChild(
                    new FragmentOutputCase(m_context, (fmtName + "_" + precName + "_vec2").c_str(), "", fboSpec,
                                           (OutputVec() << FragmentOutput(glu::TYPE_FLOAT_VEC2, prec, 0)).toVec()));
                floatGroup->addChild(
                    new FragmentOutputCase(m_context, (fmtName + "_" + precName + "_vec3").c_str(), "", fboSpec,
                                           (OutputVec() << FragmentOutput(glu::TYPE_FLOAT_VEC3, prec, 0)).toVec()));
                floatGroup->addChild(
                    new FragmentOutputCase(m_context, (fmtName + "_" + precName + "_vec4").c_str(), "", fboSpec,
                                           (OutputVec() << FragmentOutput(glu::TYPE_FLOAT_VEC4, prec, 0)).toVec()));
            }
        }

        // .fixed
        tcu::TestCaseGroup *fixedGroup = new tcu::TestCaseGroup(m_testCtx, "fixed", "Fixed-point output tests");
        basicGroup->addChild(fixedGroup);
        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(requiredFixedFormats); fmtNdx++)
        {
            uint32_t format = requiredFixedFormats[fmtNdx];
            string fmtName  = getFormatName(format);
            vector<BufferSpec> fboSpec;

            fboSpec.push_back(BufferSpec(format, width, height, samples));

            for (int precNdx = 0; precNdx < DE_LENGTH_OF_ARRAY(precisions); precNdx++)
            {
                glu::Precision prec = precisions[precNdx];
                string precName     = glu::getPrecisionName(prec);

                fixedGroup->addChild(
                    new FragmentOutputCase(m_context, (fmtName + "_" + precName + "_float").c_str(), "", fboSpec,
                                           (OutputVec() << FragmentOutput(glu::TYPE_FLOAT, prec, 0)).toVec()));
                fixedGroup->addChild(
                    new FragmentOutputCase(m_context, (fmtName + "_" + precName + "_vec2").c_str(), "", fboSpec,
                                           (OutputVec() << FragmentOutput(glu::TYPE_FLOAT_VEC2, prec, 0)).toVec()));
                fixedGroup->addChild(
                    new FragmentOutputCase(m_context, (fmtName + "_" + precName + "_vec3").c_str(), "", fboSpec,
                                           (OutputVec() << FragmentOutput(glu::TYPE_FLOAT_VEC3, prec, 0)).toVec()));
                fixedGroup->addChild(
                    new FragmentOutputCase(m_context, (fmtName + "_" + precName + "_vec4").c_str(), "", fboSpec,
                                           (OutputVec() << FragmentOutput(glu::TYPE_FLOAT_VEC4, prec, 0)).toVec()));
            }
        }

        // .int
        tcu::TestCaseGroup *intGroup = new tcu::TestCaseGroup(m_testCtx, "int", "Integer output tests");
        basicGroup->addChild(intGroup);
        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(requiredIntFormats); fmtNdx++)
        {
            uint32_t format = requiredIntFormats[fmtNdx];
            string fmtName  = getFormatName(format);
            vector<BufferSpec> fboSpec;

            fboSpec.push_back(BufferSpec(format, width, height, samples));

            for (int precNdx = 0; precNdx < DE_LENGTH_OF_ARRAY(precisions); precNdx++)
            {
                glu::Precision prec = precisions[precNdx];
                string precName     = glu::getPrecisionName(prec);

                intGroup->addChild(
                    new FragmentOutputCase(m_context, (fmtName + "_" + precName + "_int").c_str(), "", fboSpec,
                                           (OutputVec() << FragmentOutput(glu::TYPE_INT, prec, 0)).toVec()));
                intGroup->addChild(
                    new FragmentOutputCase(m_context, (fmtName + "_" + precName + "_ivec2").c_str(), "", fboSpec,
                                           (OutputVec() << FragmentOutput(glu::TYPE_INT_VEC2, prec, 0)).toVec()));
                intGroup->addChild(
                    new FragmentOutputCase(m_context, (fmtName + "_" + precName + "_ivec3").c_str(), "", fboSpec,
                                           (OutputVec() << FragmentOutput(glu::TYPE_INT_VEC3, prec, 0)).toVec()));
                intGroup->addChild(
                    new FragmentOutputCase(m_context, (fmtName + "_" + precName + "_ivec4").c_str(), "", fboSpec,
                                           (OutputVec() << FragmentOutput(glu::TYPE_INT_VEC4, prec, 0)).toVec()));
            }
        }

        // .uint
        tcu::TestCaseGroup *uintGroup = new tcu::TestCaseGroup(m_testCtx, "uint", "Usigned integer output tests");
        basicGroup->addChild(uintGroup);
        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(requiredUintFormats); fmtNdx++)
        {
            uint32_t format = requiredUintFormats[fmtNdx];
            string fmtName  = getFormatName(format);
            vector<BufferSpec> fboSpec;

            fboSpec.push_back(BufferSpec(format, width, height, samples));

            for (int precNdx = 0; precNdx < DE_LENGTH_OF_ARRAY(precisions); precNdx++)
            {
                glu::Precision prec = precisions[precNdx];
                string precName     = glu::getPrecisionName(prec);

                uintGroup->addChild(
                    new FragmentOutputCase(m_context, (fmtName + "_" + precName + "_uint").c_str(), "", fboSpec,
                                           (OutputVec() << FragmentOutput(glu::TYPE_UINT, prec, 0)).toVec()));
                uintGroup->addChild(
                    new FragmentOutputCase(m_context, (fmtName + "_" + precName + "_uvec2").c_str(), "", fboSpec,
                                           (OutputVec() << FragmentOutput(glu::TYPE_UINT_VEC2, prec, 0)).toVec()));
                uintGroup->addChild(
                    new FragmentOutputCase(m_context, (fmtName + "_" + precName + "_uvec3").c_str(), "", fboSpec,
                                           (OutputVec() << FragmentOutput(glu::TYPE_UINT_VEC3, prec, 0)).toVec()));
                uintGroup->addChild(
                    new FragmentOutputCase(m_context, (fmtName + "_" + precName + "_uvec4").c_str(), "", fboSpec,
                                           (OutputVec() << FragmentOutput(glu::TYPE_UINT_VEC4, prec, 0)).toVec()));
            }
        }
    }

    // .array
    {
        tcu::TestCaseGroup *arrayGroup = new tcu::TestCaseGroup(m_testCtx, "array", "Array outputs");
        addChild(arrayGroup);

        const int width      = 64;
        const int height     = 64;
        const int samples    = 0;
        const int numTargets = 3;

        // .float
        tcu::TestCaseGroup *floatGroup = new tcu::TestCaseGroup(m_testCtx, "float", "Floating-point output tests");
        arrayGroup->addChild(floatGroup);
        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(requiredFloatFormats); fmtNdx++)
        {
            uint32_t format = requiredFloatFormats[fmtNdx];
            string fmtName  = getFormatName(format);
            vector<BufferSpec> fboSpec;

            for (int ndx = 0; ndx < numTargets; ndx++)
                fboSpec.push_back(BufferSpec(format, width, height, samples));

            for (int precNdx = 0; precNdx < DE_LENGTH_OF_ARRAY(precisions); precNdx++)
            {
                glu::Precision prec = precisions[precNdx];
                string precName     = glu::getPrecisionName(prec);

                floatGroup->addChild(new FragmentOutputCase(
                    m_context, (fmtName + "_" + precName + "_float").c_str(), "", fboSpec,
                    (OutputVec() << FragmentOutput(glu::TYPE_FLOAT, prec, 0, numTargets)).toVec()));
                floatGroup->addChild(new FragmentOutputCase(
                    m_context, (fmtName + "_" + precName + "_vec2").c_str(), "", fboSpec,
                    (OutputVec() << FragmentOutput(glu::TYPE_FLOAT_VEC2, prec, 0, numTargets)).toVec()));
                floatGroup->addChild(new FragmentOutputCase(
                    m_context, (fmtName + "_" + precName + "_vec3").c_str(), "", fboSpec,
                    (OutputVec() << FragmentOutput(glu::TYPE_FLOAT_VEC3, prec, 0, numTargets)).toVec()));
                floatGroup->addChild(new FragmentOutputCase(
                    m_context, (fmtName + "_" + precName + "_vec4").c_str(), "", fboSpec,
                    (OutputVec() << FragmentOutput(glu::TYPE_FLOAT_VEC4, prec, 0, numTargets)).toVec()));
            }
        }

        // .fixed
        tcu::TestCaseGroup *fixedGroup = new tcu::TestCaseGroup(m_testCtx, "fixed", "Fixed-point output tests");
        arrayGroup->addChild(fixedGroup);
        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(requiredFixedFormats); fmtNdx++)
        {
            uint32_t format = requiredFixedFormats[fmtNdx];
            string fmtName  = getFormatName(format);
            vector<BufferSpec> fboSpec;

            for (int ndx = 0; ndx < numTargets; ndx++)
                fboSpec.push_back(BufferSpec(format, width, height, samples));

            for (int precNdx = 0; precNdx < DE_LENGTH_OF_ARRAY(precisions); precNdx++)
            {
                glu::Precision prec = precisions[precNdx];
                string precName     = glu::getPrecisionName(prec);

                fixedGroup->addChild(new FragmentOutputCase(
                    m_context, (fmtName + "_" + precName + "_float").c_str(), "", fboSpec,
                    (OutputVec() << FragmentOutput(glu::TYPE_FLOAT, prec, 0, numTargets)).toVec()));
                fixedGroup->addChild(new FragmentOutputCase(
                    m_context, (fmtName + "_" + precName + "_vec2").c_str(), "", fboSpec,
                    (OutputVec() << FragmentOutput(glu::TYPE_FLOAT_VEC2, prec, 0, numTargets)).toVec()));
                fixedGroup->addChild(new FragmentOutputCase(
                    m_context, (fmtName + "_" + precName + "_vec3").c_str(), "", fboSpec,
                    (OutputVec() << FragmentOutput(glu::TYPE_FLOAT_VEC3, prec, 0, numTargets)).toVec()));
                fixedGroup->addChild(new FragmentOutputCase(
                    m_context, (fmtName + "_" + precName + "_vec4").c_str(), "", fboSpec,
                    (OutputVec() << FragmentOutput(glu::TYPE_FLOAT_VEC4, prec, 0, numTargets)).toVec()));
            }
        }

        // .int
        tcu::TestCaseGroup *intGroup = new tcu::TestCaseGroup(m_testCtx, "int", "Integer output tests");
        arrayGroup->addChild(intGroup);
        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(requiredIntFormats); fmtNdx++)
        {
            uint32_t format = requiredIntFormats[fmtNdx];
            string fmtName  = getFormatName(format);
            vector<BufferSpec> fboSpec;

            for (int ndx = 0; ndx < numTargets; ndx++)
                fboSpec.push_back(BufferSpec(format, width, height, samples));

            for (int precNdx = 0; precNdx < DE_LENGTH_OF_ARRAY(precisions); precNdx++)
            {
                glu::Precision prec = precisions[precNdx];
                string precName     = glu::getPrecisionName(prec);

                intGroup->addChild(new FragmentOutputCase(
                    m_context, (fmtName + "_" + precName + "_int").c_str(), "", fboSpec,
                    (OutputVec() << FragmentOutput(glu::TYPE_INT, prec, 0, numTargets)).toVec()));
                intGroup->addChild(new FragmentOutputCase(
                    m_context, (fmtName + "_" + precName + "_ivec2").c_str(), "", fboSpec,
                    (OutputVec() << FragmentOutput(glu::TYPE_INT_VEC2, prec, 0, numTargets)).toVec()));
                intGroup->addChild(new FragmentOutputCase(
                    m_context, (fmtName + "_" + precName + "_ivec3").c_str(), "", fboSpec,
                    (OutputVec() << FragmentOutput(glu::TYPE_INT_VEC3, prec, 0, numTargets)).toVec()));
                intGroup->addChild(new FragmentOutputCase(
                    m_context, (fmtName + "_" + precName + "_ivec4").c_str(), "", fboSpec,
                    (OutputVec() << FragmentOutput(glu::TYPE_INT_VEC4, prec, 0, numTargets)).toVec()));
            }
        }

        // .uint
        tcu::TestCaseGroup *uintGroup = new tcu::TestCaseGroup(m_testCtx, "uint", "Usigned integer output tests");
        arrayGroup->addChild(uintGroup);
        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(requiredUintFormats); fmtNdx++)
        {
            uint32_t format = requiredUintFormats[fmtNdx];
            string fmtName  = getFormatName(format);
            vector<BufferSpec> fboSpec;

            for (int ndx = 0; ndx < numTargets; ndx++)
                fboSpec.push_back(BufferSpec(format, width, height, samples));

            for (int precNdx = 0; precNdx < DE_LENGTH_OF_ARRAY(precisions); precNdx++)
            {
                glu::Precision prec = precisions[precNdx];
                string precName     = glu::getPrecisionName(prec);

                uintGroup->addChild(new FragmentOutputCase(
                    m_context, (fmtName + "_" + precName + "_uint").c_str(), "", fboSpec,
                    (OutputVec() << FragmentOutput(glu::TYPE_UINT, prec, 0, numTargets)).toVec()));
                uintGroup->addChild(new FragmentOutputCase(
                    m_context, (fmtName + "_" + precName + "_uvec2").c_str(), "", fboSpec,
                    (OutputVec() << FragmentOutput(glu::TYPE_UINT_VEC2, prec, 0, numTargets)).toVec()));
                uintGroup->addChild(new FragmentOutputCase(
                    m_context, (fmtName + "_" + precName + "_uvec3").c_str(), "", fboSpec,
                    (OutputVec() << FragmentOutput(glu::TYPE_UINT_VEC3, prec, 0, numTargets)).toVec()));
                uintGroup->addChild(new FragmentOutputCase(
                    m_context, (fmtName + "_" + precName + "_uvec4").c_str(), "", fboSpec,
                    (OutputVec() << FragmentOutput(glu::TYPE_UINT_VEC4, prec, 0, numTargets)).toVec()));
            }
        }
    }

    // .random
    {
        tcu::TestCaseGroup *randomGroup = new tcu::TestCaseGroup(m_testCtx, "random", "Random fragment output cases");
        addChild(randomGroup);

        for (uint32_t seed = 0; seed < 100; seed++)
            randomGroup->addChild(createRandomCase(m_context, 2, 4, seed));
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
