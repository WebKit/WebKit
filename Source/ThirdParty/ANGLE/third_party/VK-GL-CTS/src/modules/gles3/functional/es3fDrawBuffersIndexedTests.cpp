/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3 Module
 * -------------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief Indexed blend operation tests (GL_EXT_draw_buffers_indexed)
 *//*--------------------------------------------------------------------*/

#include "es3fDrawBuffersIndexedTests.hpp"

#include "gluContextInfo.hpp"
#include "gluDrawUtil.hpp"
#include "gluObjectWrapper.hpp"
#include "gluPixelTransfer.hpp"
#include "gluShaderProgram.hpp"
#include "gluStrUtil.hpp"
#include "gluTextureUtil.hpp"

#include "sglrReferenceUtils.hpp"

#include "rrMultisamplePixelBufferAccess.hpp"
#include "rrRenderer.hpp"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include "tcuEither.hpp"
#include "tcuImageCompare.hpp"
#include "tcuMaybe.hpp"
#include "tcuResultCollector.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVector.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuFloat.hpp"

#include "deRandom.hpp"
#include "deArrayUtil.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include "deInt32.h"

#include <string>
#include <vector>
#include <map>

using tcu::BVec4;
using tcu::Either;
using tcu::IVec2;
using tcu::IVec4;
using tcu::just;
using tcu::Maybe;
using tcu::TestLog;
using tcu::TextureFormat;
using tcu::TextureLevel;
using tcu::UVec4;
using tcu::Vec2;
using tcu::Vec4;

using std::map;
using std::string;
using std::vector;

using sglr::rr_util::mapGLBlendEquation;
using sglr::rr_util::mapGLBlendEquationAdvanced;
using sglr::rr_util::mapGLBlendFunc;

namespace deqp
{
namespace gles3
{
namespace Functional
{
namespace
{

typedef uint32_t BlendEq;

bool isAdvancedBlendEq(BlendEq eq)
{
    switch (eq)
    {
    case GL_MULTIPLY:
        return true;
    case GL_SCREEN:
        return true;
    case GL_OVERLAY:
        return true;
    case GL_DARKEN:
        return true;
    case GL_LIGHTEN:
        return true;
    case GL_COLORDODGE:
        return true;
    case GL_COLORBURN:
        return true;
    case GL_HARDLIGHT:
        return true;
    case GL_SOFTLIGHT:
        return true;
    case GL_DIFFERENCE:
        return true;
    case GL_EXCLUSION:
        return true;
    case GL_HSL_HUE:
        return true;
    case GL_HSL_SATURATION:
        return true;
    case GL_HSL_COLOR:
        return true;
    case GL_HSL_LUMINOSITY:
        return true;
    default:
        return false;
    }
}

struct SeparateBlendEq
{
    SeparateBlendEq(BlendEq rgb_, BlendEq alpha_) : rgb(rgb_), alpha(alpha_)
    {
    }

    BlendEq rgb;
    BlendEq alpha;
};

struct BlendFunc
{
    BlendFunc(uint32_t src_, uint32_t dst_) : src(src_), dst(dst_)
    {
    }

    uint32_t src;
    uint32_t dst;
};

struct SeparateBlendFunc
{
    SeparateBlendFunc(BlendFunc rgb_, BlendFunc alpha_) : rgb(rgb_), alpha(alpha_)
    {
    }

    BlendFunc rgb;
    BlendFunc alpha;
};

typedef uint32_t DrawBuffer;

struct BlendState
{
    BlendState(void)
    {
    }

    BlendState(const Maybe<bool> &enableBlend_, const Maybe<Either<BlendEq, SeparateBlendEq>> &blendEq_,
               const Maybe<Either<BlendFunc, SeparateBlendFunc>> &blendFunc_, const Maybe<BVec4> &colorMask_)
        : enableBlend(enableBlend_)
        , blendEq(blendEq_)
        , blendFunc(blendFunc_)
        , colorMask(colorMask_)
    {
    }

    bool isEmpty(void) const
    {
        return (!enableBlend) && (!blendEq) && (!blendFunc) && (!colorMask);
    }

    Maybe<bool> enableBlend;
    Maybe<Either<BlendEq, SeparateBlendEq>> blendEq;
    Maybe<Either<BlendFunc, SeparateBlendFunc>> blendFunc;
    Maybe<BVec4> colorMask;
};

static bool checkES32orGL45Support(Context &ctx)
{
    auto ctxType = ctx.getRenderContext().getType();
    return contextSupports(ctxType, glu::ApiType::es(3, 2)) || contextSupports(ctxType, glu::ApiType::core(4, 5));
}

void setCommonBlendState(const glw::Functions &gl, const BlendState &blend)
{
    if (blend.enableBlend)
    {
        if (*blend.enableBlend)
            gl.enable(GL_BLEND);
        else
            gl.disable(GL_BLEND);
    }

    if (blend.colorMask)
    {
        const BVec4 &mask = *blend.colorMask;

        gl.colorMask(mask.x(), mask.y(), mask.z(), mask.w());
    }

    if (blend.blendEq)
    {
        const Either<BlendEq, SeparateBlendEq> &blendEq = *blend.blendEq;

        if (blendEq.is<BlendEq>())
            gl.blendEquation(blendEq.get<BlendEq>());
        else if (blendEq.is<SeparateBlendEq>())
            gl.blendEquationSeparate(blendEq.get<SeparateBlendEq>().rgb, blendEq.get<SeparateBlendEq>().alpha);
        else
            DE_ASSERT(false);
    }

    if (blend.blendFunc)
    {
        const Either<BlendFunc, SeparateBlendFunc> &blendFunc = *blend.blendFunc;

        if (blendFunc.is<BlendFunc>())
            gl.blendFunc(blendFunc.get<BlendFunc>().src, blendFunc.get<BlendFunc>().dst);
        else if (blendFunc.is<SeparateBlendFunc>())
            gl.blendFuncSeparate(blendFunc.get<SeparateBlendFunc>().rgb.src, blendFunc.get<SeparateBlendFunc>().rgb.dst,
                                 blendFunc.get<SeparateBlendFunc>().alpha.src,
                                 blendFunc.get<SeparateBlendFunc>().alpha.dst);
        else
            DE_ASSERT(false);
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to set common blend state.");
}

void setIndexedBlendState(const glw::Functions &gl, const BlendState &blend, uint32_t index)
{
    if (blend.enableBlend)
    {
        if (*blend.enableBlend)
            gl.enablei(GL_BLEND, index);
        else
            gl.disablei(GL_BLEND, index);
    }

    if (blend.colorMask)
    {
        const BVec4 mask = *blend.colorMask;

        gl.colorMaski(index, mask.x(), mask.y(), mask.z(), mask.w());
    }

    if (blend.blendEq)
    {
        const Either<BlendEq, SeparateBlendEq> &blendEq = *blend.blendEq;

        if (blendEq.is<BlendEq>())
            gl.blendEquationi(index, blendEq.get<BlendEq>());
        else if (blendEq.is<SeparateBlendEq>())
            gl.blendEquationSeparatei(index, blendEq.get<SeparateBlendEq>().rgb, blendEq.get<SeparateBlendEq>().alpha);
        else
            DE_ASSERT(false);
    }

    if (blend.blendFunc)
    {
        const Either<BlendFunc, SeparateBlendFunc> &blendFunc = *blend.blendFunc;

        if (blendFunc.is<BlendFunc>())
            gl.blendFunci(index, blendFunc.get<BlendFunc>().src, blendFunc.get<BlendFunc>().dst);
        else if (blendFunc.is<SeparateBlendFunc>())
            gl.blendFuncSeparatei(
                index, blendFunc.get<SeparateBlendFunc>().rgb.src, blendFunc.get<SeparateBlendFunc>().rgb.dst,
                blendFunc.get<SeparateBlendFunc>().alpha.src, blendFunc.get<SeparateBlendFunc>().alpha.dst);
        else
            DE_ASSERT(false);
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to set draw buffer specifig blend state.");
}

class DrawBufferInfo
{
public:
    DrawBufferInfo(bool render, const IVec2 &size, const BlendState &blendState, const TextureFormat &format);

    const TextureFormat &getFormat(void) const
    {
        return m_format;
    }
    const IVec2 &getSize(void) const
    {
        return m_size;
    }
    const BlendState &getBlendState(void) const
    {
        return m_blendState;
    }
    bool getRender(void) const
    {
        return m_render;
    }

private:
    bool m_render;
    IVec2 m_size;
    TextureFormat m_format;
    BlendState m_blendState;
};

DrawBufferInfo::DrawBufferInfo(bool render, const IVec2 &size, const BlendState &blendState,
                               const TextureFormat &format)
    : m_render(render)
    , m_size(size)
    , m_format(format)
    , m_blendState(blendState)
{
}

float getUnsignedFixedPointComponent(const float component, const int bits)
{
    // When converting the float to a normalized fixed-point integer value, it should be rounded
    // to the nearest integer. However, if the value is exactly between two integers, it could
    // be rounded to either one. To avoid this scenario, it is better to shift such a value to a
    // small extent.
    if (bits <= 1)
    {
        return component;
    }

    auto range       = static_cast<float>((1 << bits) - 1);
    float fixedValue = component * range;
    float eps        = 0.05f / range;
    // If the fractional part of the fixed value is close enough to 0.5, a small bias is applied
    // to the component value.
    if (abs(0.5f + floor(fixedValue) - fixedValue) < eps)
    {
        float bias = (component > 0.5f ? -1.0f : 1.0f) * eps;
        return component + bias;
    }
    return component;
}

void clearRenderbuffer(const glw::Functions &gl, const tcu::TextureFormat &format, int renderbufferNdx,
                       int renderbufferCount, tcu::TextureLevel &refRenderbuffer)
{
    const tcu::TextureFormatInfo info = tcu::getTextureFormatInfo(format);

    // Clear each buffer to different color
    const float redScale  = float(renderbufferNdx + 1) / float(renderbufferCount);
    const float blueScale = float(renderbufferCount - renderbufferNdx) / float(renderbufferCount);
    const float greenScale =
        float(((renderbufferCount / 2) + renderbufferNdx) % renderbufferCount) / float(renderbufferCount);
    // Alpha should never be zero as advanced blend equations assume premultiplied alpha.
    const float alphaScale =
        float(1 + (((renderbufferCount / 2) + renderbufferCount - renderbufferNdx) % renderbufferCount)) /
        float(renderbufferCount);

    switch (tcu::getTextureChannelClass(format.type))
    {
    case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
    {
        const float red   = -1000.0f + 2000.0f * redScale;
        const float green = -1000.0f + 2000.0f * greenScale;
        const float blue  = -1000.0f + 2000.0f * blueScale;
        const float alpha = -1000.0f + 2000.0f * alphaScale;
        const Vec4 color(red, green, blue, alpha);

        tcu::clear(refRenderbuffer, color);
        gl.clearBufferfv(GL_COLOR, renderbufferNdx, color.getPtr());
        break;
    }

    case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
    {
        const int32_t red   = int32_t(info.valueMin.x() + (info.valueMax.x() - info.valueMin.x()) * redScale);
        const int32_t green = int32_t(info.valueMin.y() + (info.valueMax.y() - info.valueMin.y()) * greenScale);
        const int32_t blue  = int32_t(info.valueMin.z() + (info.valueMax.z() - info.valueMin.z()) * blueScale);
        const int32_t alpha = int32_t(info.valueMin.w() + (info.valueMax.w() - info.valueMin.w()) * alphaScale);
        const IVec4 color(red, green, blue, alpha);

        tcu::clear(refRenderbuffer, color);
        gl.clearBufferiv(GL_COLOR, renderbufferNdx, color.getPtr());
        break;
    }

    case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
    {
        const uint32_t red   = uint32_t(info.valueMax.x() * redScale);
        const uint32_t green = uint32_t(info.valueMax.y() * greenScale);
        const uint32_t blue  = uint32_t(info.valueMax.z() * blueScale);
        const uint32_t alpha = uint32_t(info.valueMax.w() * alphaScale);
        const UVec4 color(red, green, blue, alpha);

        tcu::clear(refRenderbuffer, color);
        gl.clearBufferuiv(GL_COLOR, renderbufferNdx, color.getPtr());
        break;
    }

    case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
    {
        const float red   = info.valueMin.x() + (info.valueMax.x() - info.valueMin.x()) * redScale;
        const float green = info.valueMin.y() + (info.valueMax.y() - info.valueMin.y()) * greenScale;
        const float blue  = info.valueMin.z() + (info.valueMax.z() - info.valueMin.z()) * blueScale;
        const float alpha = info.valueMin.w() + (info.valueMax.w() - info.valueMin.w()) * alphaScale;
        const Vec4 color(red, green, blue, alpha);

        tcu::clear(refRenderbuffer, color);
        gl.clearBufferfv(GL_COLOR, renderbufferNdx, color.getPtr());
        break;
    }

    case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
    {
        const tcu::IVec4 formatBits = tcu::getTextureFormatBitDepth(format);
        const float red             = getUnsignedFixedPointComponent(info.valueMax.x() * redScale, formatBits.x());
        const float green           = getUnsignedFixedPointComponent(info.valueMax.y() * greenScale, formatBits.y());
        const float blue            = getUnsignedFixedPointComponent(info.valueMax.z() * blueScale, formatBits.z());
        const float alpha           = getUnsignedFixedPointComponent(info.valueMax.w() * alphaScale, formatBits.w());
        const Vec4 color(red, green, blue, alpha);

        tcu::clear(refRenderbuffer, color);
        gl.clearBufferfv(GL_COLOR, renderbufferNdx, color.getPtr());
        break;
    }

    default:
        DE_ASSERT(false);
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to clear renderbuffer.");
}

void genRenderbuffers(const glw::Functions &gl, const vector<DrawBufferInfo> &drawBuffers,
                      const glu::Framebuffer &framebuffer, const glu::RenderbufferVector &renderbuffers,
                      vector<TextureLevel> &refRenderbuffers)
{
    vector<uint32_t> bufs;

    bufs.resize(drawBuffers.size());

    DE_ASSERT(drawBuffers.size() == renderbuffers.size());
    DE_ASSERT(drawBuffers.size() == refRenderbuffers.size());

    gl.bindFramebuffer(GL_FRAMEBUFFER, *framebuffer);

    for (int renderbufferNdx = 0; renderbufferNdx < (int)drawBuffers.size(); renderbufferNdx++)
    {
        const DrawBufferInfo &drawBuffer = drawBuffers[renderbufferNdx];
        const TextureFormat &format      = drawBuffer.getFormat();
        const IVec2 &size                = drawBuffer.getSize();
        const uint32_t glFormat          = glu::getInternalFormat(format);

        bufs[renderbufferNdx]             = GL_COLOR_ATTACHMENT0 + renderbufferNdx;
        refRenderbuffers[renderbufferNdx] = TextureLevel(drawBuffer.getFormat(), size.x(), size.y());

        gl.bindRenderbuffer(GL_RENDERBUFFER, renderbuffers[renderbufferNdx]);
        gl.renderbufferStorage(GL_RENDERBUFFER, glFormat, size.x(), size.y());
        gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + renderbufferNdx, GL_RENDERBUFFER,
                                   renderbuffers[renderbufferNdx]);
        GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to create renderbuffer.");
    }

    gl.drawBuffers((glw::GLsizei)bufs.size(), &(bufs[0]));

    for (int renderbufferNdx = 0; renderbufferNdx < (int)drawBuffers.size(); renderbufferNdx++)
    {
        const DrawBufferInfo &drawBuffer = drawBuffers[renderbufferNdx];
        const TextureFormat &format      = drawBuffer.getFormat();

        clearRenderbuffer(gl, format, renderbufferNdx, (int)refRenderbuffers.size(), refRenderbuffers[renderbufferNdx]);
    }

    gl.bindRenderbuffer(GL_RENDERBUFFER, 0);
    gl.bindFramebuffer(GL_FRAMEBUFFER, 0);
}

Vec4 getFixedPointFormatThreshold(const tcu::TextureFormat &sourceFormat, const tcu::TextureFormat &readPixelsFormat)
{
    DE_ASSERT(tcu::getTextureChannelClass(sourceFormat.type) != tcu::TEXTURECHANNELCLASS_FLOATING_POINT);
    DE_ASSERT(tcu::getTextureChannelClass(readPixelsFormat.type) != tcu::TEXTURECHANNELCLASS_FLOATING_POINT);

    DE_ASSERT(tcu::getTextureChannelClass(sourceFormat.type) != tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER);
    DE_ASSERT(tcu::getTextureChannelClass(readPixelsFormat.type) != tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER);

    DE_ASSERT(tcu::getTextureChannelClass(sourceFormat.type) != tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER);
    DE_ASSERT(tcu::getTextureChannelClass(readPixelsFormat.type) != tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER);

    const tcu::IVec4 srcBits  = tcu::getTextureFormatBitDepth(sourceFormat);
    const tcu::IVec4 readBits = tcu::getTextureFormatBitDepth(readPixelsFormat);

    Vec4 threshold = Vec4(0.0f);

    for (int i = 0; i < 4; i++)
    {
        const int bits = de::min(srcBits[i], readBits[i]);

        if (bits > 0)
        {
            threshold[i] = 3.0f / static_cast<float>(((1ul << bits) - 1ul));
        }
    }

    return threshold;
}

UVec4 getFloatULPThreshold(const tcu::TextureFormat &sourceFormat, const tcu::TextureFormat &readPixelsFormat)
{
    const tcu::IVec4 srcMantissaBits  = tcu::getTextureFormatMantissaBitDepth(sourceFormat);
    const tcu::IVec4 readMantissaBits = tcu::getTextureFormatMantissaBitDepth(readPixelsFormat);
    tcu::IVec4 ULPDiff(0);

    for (int i = 0; i < 4; i++)
        if (readMantissaBits[i] >= srcMantissaBits[i])
            ULPDiff[i] = readMantissaBits[i] - srcMantissaBits[i];

    return UVec4(4) * (UVec4(1) << (ULPDiff.cast<uint32_t>()));
}

void verifyRenderbuffer(TestLog &log, tcu::ResultCollector &results, const tcu::TextureFormat &format,
                        int renderbufferNdx, const tcu::TextureLevel &refRenderbuffer, const tcu::TextureLevel &result)
{
    switch (tcu::getTextureChannelClass(format.type))
    {
    case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
    {
        const string name     = "Renderbuffer" + de::toString(renderbufferNdx);
        const string desc     = "Compare renderbuffer " + de::toString(renderbufferNdx);
        const UVec4 threshold = getFloatULPThreshold(format, result.getFormat());

        if (!tcu::floatUlpThresholdCompare(log, name.c_str(), desc.c_str(), refRenderbuffer, result, threshold,
                                           tcu::COMPARE_LOG_RESULT))
            results.fail("Verification of renderbuffer " + de::toString(renderbufferNdx) + " failed.");

        break;
    }

    case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
    case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
    {
        const string name = "Renderbuffer" + de::toString(renderbufferNdx);
        const string desc = "Compare renderbuffer " + de::toString(renderbufferNdx);
        const UVec4 threshold(1, 1, 1, 1);

        if (!tcu::intThresholdCompare(log, name.c_str(), desc.c_str(), refRenderbuffer, result, threshold,
                                      tcu::COMPARE_LOG_RESULT))
            results.fail("Verification of renderbuffer " + de::toString(renderbufferNdx) + " failed.");

        break;
    }

    case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
    case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
    {
        const string name    = "Renderbuffer" + de::toString(renderbufferNdx);
        const string desc    = "Compare renderbuffer " + de::toString(renderbufferNdx);
        const Vec4 threshold = getFixedPointFormatThreshold(format, result.getFormat());

        if (!tcu::floatThresholdCompare(log, name.c_str(), desc.c_str(), refRenderbuffer, result, threshold,
                                        tcu::COMPARE_LOG_RESULT))
            results.fail("Verification of renderbuffer " + de::toString(renderbufferNdx) + " failed.");

        break;
    }

    default:
        DE_ASSERT(false);
    }
}

TextureFormat getReadPixelFormat(const TextureFormat &format)
{
    switch (tcu::getTextureChannelClass(format.type))
    {
    case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::UNSIGNED_INT32);

    case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::SIGNED_INT32);

    case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
    case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8);

    case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::FLOAT);

    default:
        DE_ASSERT(false);
        return TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8);
    }
}

void verifyRenderbuffers(TestLog &log, tcu::ResultCollector &results, glu::RenderContext &renderContext,
                         const glu::RenderbufferVector &renderbuffers, const glu::Framebuffer &framebuffer,
                         const vector<TextureLevel> &refRenderbuffers)
{
    const glw::Functions &gl = renderContext.getFunctions();

    DE_ASSERT(renderbuffers.size() == refRenderbuffers.size());

    gl.bindFramebuffer(GL_FRAMEBUFFER, *framebuffer);

    for (int renderbufferNdx = 0; renderbufferNdx < (int)renderbuffers.size(); renderbufferNdx++)
    {
        const TextureLevel &refRenderbuffer = refRenderbuffers[renderbufferNdx];
        const int width                     = refRenderbuffer.getWidth();
        const int height                    = refRenderbuffer.getHeight();
        const TextureFormat format          = refRenderbuffer.getFormat();

        tcu::TextureLevel result(getReadPixelFormat(format), width, height);

        gl.readBuffer(GL_COLOR_ATTACHMENT0 + renderbufferNdx);
        glu::readPixels(renderContext, 0, 0, result.getAccess());
        GLU_EXPECT_NO_ERROR(gl.getError(), "Reading pixels from renderbuffer failed.");

        verifyRenderbuffer(log, results, format, renderbufferNdx, refRenderbuffer, result);
    }

    gl.bindFramebuffer(GL_FRAMEBUFFER, 0);
}

static const float s_quadCoords[] = {-0.5f, -0.5f, 0.5f,  -0.5f, 0.5f,  0.5f,

                                     0.5f,  0.5f,  -0.5f, 0.5f,  -0.5f, -0.5f};

void setBlendState(rr::FragmentOperationState &fragOps, const BlendState &state)
{
    if (state.blendEq)
    {
        if (state.blendEq->is<BlendEq>())
        {
            if (isAdvancedBlendEq(state.blendEq->get<BlendEq>()))
            {
                const rr::BlendEquationAdvanced equation = mapGLBlendEquationAdvanced(state.blendEq->get<BlendEq>());

                fragOps.blendMode            = rr::BLENDMODE_ADVANCED;
                fragOps.blendEquationAdvaced = equation;
            }
            else
            {
                const rr::BlendEquation equation = mapGLBlendEquation(state.blendEq->get<BlendEq>());

                fragOps.blendMode              = rr::BLENDMODE_STANDARD;
                fragOps.blendRGBState.equation = equation;
                fragOps.blendAState.equation   = equation;
            }
        }
        else
        {
            DE_ASSERT(state.blendEq->is<SeparateBlendEq>());

            fragOps.blendMode              = rr::BLENDMODE_STANDARD;
            fragOps.blendRGBState.equation = mapGLBlendEquation(state.blendEq->get<SeparateBlendEq>().rgb);
            fragOps.blendAState.equation   = mapGLBlendEquation(state.blendEq->get<SeparateBlendEq>().alpha);
        }
    }

    if (state.blendFunc)
    {
        if (state.blendFunc->is<BlendFunc>())
        {
            const rr::BlendFunc srcFunction = mapGLBlendFunc(state.blendFunc->get<BlendFunc>().src);
            const rr::BlendFunc dstFunction = mapGLBlendFunc(state.blendFunc->get<BlendFunc>().dst);

            fragOps.blendRGBState.srcFunc = srcFunction;
            fragOps.blendRGBState.dstFunc = dstFunction;

            fragOps.blendAState.srcFunc = srcFunction;
            fragOps.blendAState.dstFunc = dstFunction;
        }
        else
        {
            DE_ASSERT(state.blendFunc->is<SeparateBlendFunc>());

            fragOps.blendRGBState.srcFunc = mapGLBlendFunc(state.blendFunc->get<SeparateBlendFunc>().rgb.src);
            fragOps.blendRGBState.dstFunc = mapGLBlendFunc(state.blendFunc->get<SeparateBlendFunc>().rgb.dst);

            fragOps.blendAState.srcFunc = mapGLBlendFunc(state.blendFunc->get<SeparateBlendFunc>().alpha.src);
            fragOps.blendAState.dstFunc = mapGLBlendFunc(state.blendFunc->get<SeparateBlendFunc>().alpha.dst);
        }
    }

    if (state.colorMask)
        fragOps.colorMask = *state.colorMask;
}

rr::RenderState createRenderState(const BlendState &preCommonBlendState, const BlendState &postCommonBlendState,
                                  const DrawBufferInfo &info, int subpixelBits)
{
    const IVec2 size = info.getSize();
    rr::RenderState state(rr::ViewportState(rr::WindowRectangle(0, 0, size.x(), size.y())), subpixelBits);

    state.fragOps.blendMode = rr::BLENDMODE_STANDARD;

    setBlendState(state.fragOps, preCommonBlendState);
    setBlendState(state.fragOps, info.getBlendState());
    setBlendState(state.fragOps, postCommonBlendState);

    if (postCommonBlendState.enableBlend)
        state.fragOps.blendMode = (*(postCommonBlendState.enableBlend) ? state.fragOps.blendMode : rr::BLENDMODE_NONE);
    else if (info.getBlendState().enableBlend)
        state.fragOps.blendMode = (*(info.getBlendState().enableBlend) ? state.fragOps.blendMode : rr::BLENDMODE_NONE);
    else if (preCommonBlendState.enableBlend)
        state.fragOps.blendMode = (*(preCommonBlendState.enableBlend) ? state.fragOps.blendMode : rr::BLENDMODE_NONE);
    else
        state.fragOps.blendMode = rr::BLENDMODE_NONE;

    if (tcu::getTextureChannelClass(info.getFormat().type) != tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT &&
        tcu::getTextureChannelClass(info.getFormat().type) != tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT &&
        tcu::getTextureChannelClass(info.getFormat().type) != tcu::TEXTURECHANNELCLASS_FLOATING_POINT)
        state.fragOps.blendMode = rr::BLENDMODE_NONE;

    return state;
}

class VertexShader : public rr::VertexShader
{
public:
    VertexShader(void);
    virtual void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets,
                               const int numPackets) const;
};

VertexShader::VertexShader(void) : rr::VertexShader(1, 1)
{
    m_inputs[0].type  = rr::GENERICVECTYPE_FLOAT;
    m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
}

void VertexShader::shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets,
                                 const int numPackets) const
{
    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
    {
        rr::VertexPacket &packet = *packets[packetNdx];

        packet.position = rr::readVertexAttribFloat(inputs[0], packet.instanceNdx, packet.vertexNdx);
        packet.outputs[0] =
            0.5f * (Vec4(1.0f) + rr::readVertexAttribFloat(inputs[0], packet.instanceNdx, packet.vertexNdx));
    }
}

class FragmentShader : public rr::FragmentShader
{
public:
    FragmentShader(int drawBufferNdx, const DrawBufferInfo &info);
    void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                        const rr::FragmentShadingContext &context) const;

private:
    const int m_drawBufferNdx;
    const DrawBufferInfo m_info;
};

FragmentShader::FragmentShader(int drawBufferNdx, const DrawBufferInfo &info)
    : rr::FragmentShader(1, 1)
    , m_drawBufferNdx(drawBufferNdx)
    , m_info(info)
{
    m_inputs[0].type = rr::GENERICVECTYPE_FLOAT;

    switch (tcu::getTextureChannelClass(m_info.getFormat().type))
    {
    case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
    case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
    case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
        m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
        break;

    case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
        m_outputs[0].type = rr::GENERICVECTYPE_UINT32;
        break;

    case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
        m_outputs[0].type = rr::GENERICVECTYPE_INT32;
        break;

    default:
        DE_ASSERT(false);
    }
}

void FragmentShader::shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                                    const rr::FragmentShadingContext &context) const
{
    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
    {
        rr::FragmentPacket &packet = packets[packetNdx];

        DE_ASSERT(m_drawBufferNdx >= 0);
        DE_UNREF(m_info);

        for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
        {
            const Vec2 vColor    = rr::readVarying<float>(packet, context, 0, fragNdx).xy();
            const float values[] = {vColor.x(), vColor.y(), (1.0f - vColor.x()), (1.0f - vColor.y())};

            switch (tcu::getTextureChannelClass(m_info.getFormat().type))
            {
            case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
            case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
            case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
            {
                const Vec4 color(values[(m_drawBufferNdx + 0) % 4], values[(m_drawBufferNdx + 1) % 4],
                                 values[(m_drawBufferNdx + 2) % 4], values[(m_drawBufferNdx + 3) % 4]);

                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
                break;
            }

            case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
            {
                const UVec4 color(
                    (uint32_t)(values[(m_drawBufferNdx + 0) % 4]), (uint32_t)(values[(m_drawBufferNdx + 1) % 4]),
                    (uint32_t)(values[(m_drawBufferNdx + 2) % 4]), (uint32_t)(values[(m_drawBufferNdx + 3) % 4]));

                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
                break;
            }

            case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
            {
                const IVec4 color(
                    (int32_t)(values[(m_drawBufferNdx + 0) % 4]), (int32_t)(values[(m_drawBufferNdx + 1) % 4]),
                    (int32_t)(values[(m_drawBufferNdx + 2) % 4]), (int32_t)(values[(m_drawBufferNdx + 3) % 4]));

                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
                break;
            }

            default:
                DE_ASSERT(false);
            }
        }
    }
}

rr::VertexAttrib createVertexAttrib(const float *coords)
{
    rr::VertexAttrib attrib;

    attrib.type    = rr::VERTEXATTRIBTYPE_FLOAT;
    attrib.size    = 2;
    attrib.pointer = coords;

    return attrib;
}

void renderRefQuad(const BlendState &preCommonBlendState, const BlendState &postCommonBlendState,
                   const vector<DrawBufferInfo> &drawBuffers, const int subpixelBits,
                   vector<TextureLevel> &refRenderbuffers)
{
    const rr::Renderer renderer;
    const rr::PrimitiveList primitives(rr::PRIMITIVETYPE_TRIANGLES, 6, 0);
    const rr::VertexAttrib vertexAttribs[] = {createVertexAttrib(s_quadCoords)};

    for (int drawBufferNdx = 0; drawBufferNdx < (int)drawBuffers.size(); drawBufferNdx++)
    {
        if (drawBuffers[drawBufferNdx].getRender())
        {
            const rr::RenderState renderState(
                createRenderState(preCommonBlendState, postCommonBlendState, drawBuffers[drawBufferNdx], subpixelBits));
            const rr::RenderTarget renderTarget(
                rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(refRenderbuffers[drawBufferNdx].getAccess()));
            const VertexShader vertexShader;
            const FragmentShader fragmentShader(drawBufferNdx, drawBuffers[drawBufferNdx]);
            const rr::Program program(&vertexShader, &fragmentShader);
            const rr::DrawCommand command(renderState, renderTarget, program, DE_LENGTH_OF_ARRAY(vertexAttribs),
                                          vertexAttribs, primitives);

            renderer.draw(command);
        }
    }
}

bool requiresAdvancedBlendEq(const BlendState &pre, const BlendState post, const vector<DrawBufferInfo> &drawBuffers)
{
    bool requiresAdvancedBlendEq = false;

    if (pre.blendEq && pre.blendEq->is<BlendEq>())
        requiresAdvancedBlendEq |= isAdvancedBlendEq(pre.blendEq->get<BlendEq>());

    if (post.blendEq && post.blendEq->is<BlendEq>())
        requiresAdvancedBlendEq |= isAdvancedBlendEq(post.blendEq->get<BlendEq>());

    for (int drawBufferNdx = 0; drawBufferNdx < (int)drawBuffers.size(); drawBufferNdx++)
    {
        const BlendState &drawBufferBlendState = drawBuffers[drawBufferNdx].getBlendState();

        if (drawBufferBlendState.blendEq && drawBufferBlendState.blendEq->is<BlendEq>())
            requiresAdvancedBlendEq |= isAdvancedBlendEq(drawBufferBlendState.blendEq->get<BlendEq>());
    }

    return requiresAdvancedBlendEq;
}

glu::VertexSource genVertexSource(glu::RenderContext &renderContext)
{
    const bool supportsES32 = glu::contextSupports(renderContext.getType(), glu::ApiType::es(3, 2));

    const char *const vertexSource = "${GLSL_VERSION_DECL}\n"
                                     "layout(location=0) in highp vec2 i_coord;\n"
                                     "out highp vec2 v_color;\n"
                                     "void main (void)\n"
                                     "{\n"
                                     "\tv_color = 0.5 * (vec2(1.0) + i_coord);\n"
                                     "\tgl_Position = vec4(i_coord, 0.0, 1.0);\n"
                                     "}";

    map<string, string> args;
    args["GLSL_VERSION_DECL"] = supportsES32 ? getGLSLVersionDeclaration(glu::GLSL_VERSION_320_ES) :
                                               getGLSLVersionDeclaration(glu::GLSL_VERSION_300_ES);

    return glu::VertexSource(tcu::StringTemplate(vertexSource).specialize(args));
}

glu::FragmentSource genFragmentSource(const BlendState &preCommonBlendState, const BlendState &postCommonBlendState,
                                      const vector<DrawBufferInfo> &drawBuffers, glu::RenderContext &renderContext)
{
    std::ostringstream stream;
    const bool supportsES32 = glu::contextSupports(renderContext.getType(), glu::ApiType::es(3, 2));

    stream << "${GLSL_VERSION_DECL}\n";

    if (requiresAdvancedBlendEq(preCommonBlendState, postCommonBlendState, drawBuffers))
    {
        stream << "${GLSL_EXTENSION}"
               << "layout(blend_support_all_equations) out;\n";
    }

    stream << "in highp vec2 v_color;\n";

    for (int drawBufferNdx = 0; drawBufferNdx < (int)drawBuffers.size(); drawBufferNdx++)
    {
        const DrawBufferInfo &drawBuffer = drawBuffers[drawBufferNdx];
        const TextureFormat &format      = drawBuffer.getFormat();

        stream << "layout(location=" << drawBufferNdx << ") out highp ";

        switch (tcu::getTextureChannelClass(format.type))
        {
        case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
        case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
        case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
            stream << "vec4";
            break;

        case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
            stream << "uvec4";
            break;

        case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
            stream << "ivec4";
            break;

        default:
            DE_ASSERT(false);
        }

        stream << " o_drawBuffer" << drawBufferNdx << ";\n";
    }

    stream << "void main (void)\n"
           << "{\n";

    for (int drawBufferNdx = 0; drawBufferNdx < (int)drawBuffers.size(); drawBufferNdx++)
    {
        const DrawBufferInfo &drawBuffer = drawBuffers[drawBufferNdx];
        const TextureFormat &format      = drawBuffer.getFormat();
        const char *const values[]       = {"v_color.x", "v_color.y", "(1.0 - v_color.x)", "(1.0 - v_color.y)"};

        stream << "\to_drawBuffer" << drawBufferNdx;

        switch (tcu::getTextureChannelClass(format.type))
        {
        case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
        case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
        case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
            stream << " = vec4(" << values[(drawBufferNdx + 0) % 4] << ", " << values[(drawBufferNdx + 1) % 4] << ", "
                   << values[(drawBufferNdx + 2) % 4] << ", " << values[(drawBufferNdx + 3) % 4] << ");\n";
            break;

        case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
            stream << " = uvec4(uint(" << values[(drawBufferNdx + 0) % 4] << "), uint("
                   << values[(drawBufferNdx + 1) % 4] << "), uint(" << values[(drawBufferNdx + 2) % 4] << "), uint("
                   << values[(drawBufferNdx + 3) % 4] << "));\n";
            break;

        case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
            stream << " = ivec4(int(" << values[(drawBufferNdx + 0) % 4] << "), int(" << values[(drawBufferNdx + 1) % 4]
                   << "), int(" << values[(drawBufferNdx + 2) % 4] << "), int(" << values[(drawBufferNdx + 3) % 4]
                   << "));\n";
            break;

        default:
            DE_ASSERT(false);
        }
    }

    stream << "}";

    map<string, string> args;
    args["GLSL_VERSION_DECL"] = supportsES32 ? getGLSLVersionDeclaration(glu::GLSL_VERSION_320_ES) :
                                               getGLSLVersionDeclaration(glu::GLSL_VERSION_300_ES);
    args["GLSL_EXTENSION"]    = supportsES32 ? "\n" : "#extension GL_KHR_blend_equation_advanced : require\n";

    return glu::FragmentSource(tcu::StringTemplate(stream.str()).specialize(args));
}

glu::ProgramSources genShaderSources(const BlendState &preCommonBlendState, const BlendState &postCommonBlendState,
                                     const vector<DrawBufferInfo> &drawBuffers, glu::RenderContext &renderContext)
{
    return glu::ProgramSources() << genVertexSource(renderContext)
                                 << genFragmentSource(preCommonBlendState, postCommonBlendState, drawBuffers,
                                                      renderContext);
}

void renderGLQuad(glu::RenderContext &renderContext, const glu::ShaderProgram &program)
{
    const glu::VertexArrayBinding vertexArrays[] = {glu::VertexArrayBinding(
        glu::BindingPoint(0),
        glu::VertexArrayPointer(glu::VTX_COMP_FLOAT, glu::VTX_COMP_CONVERT_NONE, 2, 6, 0, s_quadCoords))};

    glu::draw(renderContext, program.getProgram(), 1, vertexArrays, glu::pr::Triangles(6));
}

void renderQuad(TestLog &log, glu::RenderContext &renderContext, const BlendState &preCommonBlendState,
                const BlendState &postCommonBlendState, const vector<DrawBufferInfo> &drawBuffers,
                const glu::Framebuffer &framebuffer, vector<TextureLevel> &refRenderbuffers)
{
    const glw::Functions &gl = renderContext.getFunctions();
    const glu::ShaderProgram program(
        gl, genShaderSources(preCommonBlendState, postCommonBlendState, drawBuffers, renderContext));
    const IVec2 size                 = drawBuffers[0].getSize();
    const bool requiresBlendBarriers = requiresAdvancedBlendEq(preCommonBlendState, postCommonBlendState, drawBuffers);

    vector<uint32_t> bufs;

    bufs.resize(drawBuffers.size());

    for (int bufNdx = 0; bufNdx < (int)bufs.size(); bufNdx++)
        bufs[bufNdx] = (drawBuffers[bufNdx].getRender() ? GL_COLOR_ATTACHMENT0 + bufNdx : GL_NONE);

    log << program;

    gl.viewport(0, 0, size.x(), size.y());
    gl.useProgram(program.getProgram());
    gl.bindFramebuffer(GL_FRAMEBUFFER, *framebuffer);

    setCommonBlendState(gl, preCommonBlendState);

    for (int renderbufferNdx = 0; renderbufferNdx < (int)drawBuffers.size(); renderbufferNdx++)
        setIndexedBlendState(gl, drawBuffers[renderbufferNdx].getBlendState(), renderbufferNdx);

    setCommonBlendState(gl, postCommonBlendState);

    gl.drawBuffers((glw::GLsizei)bufs.size(), &(bufs[0]));

    if (requiresBlendBarriers)
        gl.blendBarrier();

    renderGLQuad(renderContext, program);

    if (requiresBlendBarriers)
        gl.blendBarrier();

    gl.drawBuffers(0, 0);
    gl.bindFramebuffer(GL_FRAMEBUFFER, 0);
    gl.useProgram(0);

    GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to render");

    int subpixelBits = 0;
    gl.getIntegerv(GL_SUBPIXEL_BITS, &subpixelBits);

    renderRefQuad(preCommonBlendState, postCommonBlendState, drawBuffers, subpixelBits, refRenderbuffers);
}

void logBlendState(TestLog &log, const BlendState &blend)
{
    if (blend.enableBlend)
    {
        if (*blend.enableBlend)
            log << TestLog::Message << "Enable blending." << TestLog::EndMessage;
        else
            log << TestLog::Message << "Disable blending." << TestLog::EndMessage;
    }

    if (blend.colorMask)
    {
        const BVec4 mask = *blend.colorMask;

        log << TestLog::Message << "Set color mask: " << mask << "." << TestLog::EndMessage;
    }

    if (blend.blendEq)
    {
        const Either<BlendEq, SeparateBlendEq> &blendEq = *blend.blendEq;

        if (blendEq.is<BlendEq>())
            log << TestLog::Message << "Set blend equation: " << glu::getBlendEquationStr(blendEq.get<BlendEq>()) << "."
                << TestLog::EndMessage;
        else if (blendEq.is<SeparateBlendEq>())
            log << TestLog::Message
                << "Set blend equation rgb: " << glu::getBlendEquationStr(blendEq.get<SeparateBlendEq>().rgb)
                << ", alpha: " << glu::getBlendEquationStr(blendEq.get<SeparateBlendEq>().alpha) << "."
                << TestLog::EndMessage;
        else
            DE_ASSERT(false);
    }

    if (blend.blendFunc)
    {
        const Either<BlendFunc, SeparateBlendFunc> &blendFunc = *blend.blendFunc;

        if (blendFunc.is<BlendFunc>())
            log << TestLog::Message
                << "Set blend function source: " << glu::getBlendFactorStr(blendFunc.get<BlendFunc>().src)
                << ", destination: " << glu::getBlendFactorStr(blendFunc.get<BlendFunc>().dst) << "."
                << TestLog::EndMessage;
        else if (blendFunc.is<SeparateBlendFunc>())
        {
            log << TestLog::Message << "Set blend function rgb source: "
                << glu::getBlendFactorStr(blendFunc.get<SeparateBlendFunc>().rgb.src)
                << ", destination: " << glu::getBlendFactorStr(blendFunc.get<SeparateBlendFunc>().rgb.dst) << "."
                << TestLog::EndMessage;
            log << TestLog::Message << "Set blend function alpha source: "
                << glu::getBlendFactorStr(blendFunc.get<SeparateBlendFunc>().alpha.src)
                << ", destination: " << glu::getBlendFactorStr(blendFunc.get<SeparateBlendFunc>().alpha.dst) << "."
                << TestLog::EndMessage;
        }
        else
            DE_ASSERT(false);
    }
}

void logTestCaseInfo(TestLog &log, const BlendState &preCommonBlendState, const BlendState &postCommonBlendState,
                     const vector<DrawBufferInfo> &drawBuffers)
{
    {
        tcu::ScopedLogSection drawBuffersSection(log, "DrawBuffers", "Draw buffers");

        for (int drawBufferNdx = 0; drawBufferNdx < (int)drawBuffers.size(); drawBufferNdx++)
        {
            const tcu::ScopedLogSection drawBufferSection(log, "DrawBuffer" + de::toString(drawBufferNdx),
                                                          "Draw Buffer " + de::toString(drawBufferNdx));
            const DrawBufferInfo &drawBuffer = drawBuffers[drawBufferNdx];

            log << TestLog::Message << "Format: " << drawBuffer.getFormat() << TestLog::EndMessage;
            log << TestLog::Message << "Size: " << drawBuffer.getSize() << TestLog::EndMessage;
            log << TestLog::Message << "Render: " << (drawBuffer.getRender() ? "true" : "false") << TestLog::EndMessage;
        }
    }

    if (!preCommonBlendState.isEmpty())
    {
        tcu::ScopedLogSection s(log, "PreCommonState", "First set common blend state");
        logBlendState(log, preCommonBlendState);
    }

    for (int drawBufferNdx = 0; drawBufferNdx < (int)drawBuffers.size(); drawBufferNdx++)
    {
        if (!drawBuffers[drawBufferNdx].getBlendState().isEmpty())
        {
            const tcu::ScopedLogSection s(log, "DrawBufferState" + de::toString(drawBufferNdx),
                                          "Set DrawBuffer " + de::toString(drawBufferNdx) + " state to");

            logBlendState(log, drawBuffers[drawBufferNdx].getBlendState());
        }
    }

    if (!postCommonBlendState.isEmpty())
    {
        tcu::ScopedLogSection s(log, "PostCommonState", "After set common blend state");
        logBlendState(log, postCommonBlendState);
    }
}

void runTest(TestLog &log, tcu::ResultCollector &results, glu::RenderContext &renderContext,

             const BlendState &preCommonBlendState, const BlendState &postCommonBlendState,
             const vector<DrawBufferInfo> &drawBuffers)
{
    const glw::Functions &gl = renderContext.getFunctions();
    glu::RenderbufferVector renderbuffers(gl, drawBuffers.size());
    glu::Framebuffer framebuffer(gl);
    vector<TextureLevel> refRenderbuffers(drawBuffers.size());

    logTestCaseInfo(log, preCommonBlendState, postCommonBlendState, drawBuffers);

    genRenderbuffers(gl, drawBuffers, framebuffer, renderbuffers, refRenderbuffers);

    renderQuad(log, renderContext, preCommonBlendState, postCommonBlendState, drawBuffers, framebuffer,
               refRenderbuffers);

    verifyRenderbuffers(log, results, renderContext, renderbuffers, framebuffer, refRenderbuffers);
}

class DrawBuffersIndexedTest : public TestCase
{
public:
    DrawBuffersIndexedTest(Context &context, const BlendState &preCommonBlendState,
                           const BlendState &postCommonBlendState, const vector<DrawBufferInfo> &drawBuffers,
                           const string &name, const string &description);

    void init(void);
    IterateResult iterate(void);

private:
    const BlendState m_preCommonBlendState;
    const BlendState m_postCommonBlendState;
    const vector<DrawBufferInfo> m_drawBuffers;
};

DrawBuffersIndexedTest::DrawBuffersIndexedTest(Context &context, const BlendState &preCommonBlendState,
                                               const BlendState &postCommonBlendState,
                                               const vector<DrawBufferInfo> &drawBuffers, const string &name,
                                               const string &description)
    : TestCase(context, name.c_str(), description.c_str())
    , m_preCommonBlendState(preCommonBlendState)
    , m_postCommonBlendState(postCommonBlendState)
    , m_drawBuffers(drawBuffers)
{
}

void DrawBuffersIndexedTest::init(void)
{
    const bool supportsES32orGL45 = checkES32orGL45Support(m_context);

    if (!supportsES32orGL45)
    {
        if (requiresAdvancedBlendEq(m_preCommonBlendState, m_postCommonBlendState, m_drawBuffers) &&
            !m_context.getContextInfo().isExtensionSupported("GL_KHR_blend_equation_advanced"))
            TCU_THROW(NotSupportedError, "Extension GL_KHR_blend_equation_advanced not supported");

        if (!m_context.getContextInfo().isExtensionSupported("GL_EXT_draw_buffers_indexed"))
            TCU_THROW(NotSupportedError, "Extension GL_EXT_draw_buffers_indexed not supported");
    }
}

TestCase::IterateResult DrawBuffersIndexedTest::iterate(void)
{
    TestLog &log = m_testCtx.getLog();
    tcu::ResultCollector results(log);

    runTest(log, results, m_context.getRenderContext(), m_preCommonBlendState, m_postCommonBlendState, m_drawBuffers);

    results.setTestContextResult(m_testCtx);

    return STOP;
}

BlendEq getRandomBlendEq(de::Random &rng)
{
    const BlendEq eqs[] = {GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, GL_MIN, GL_MAX};

    return de::getSizedArrayElement<DE_LENGTH_OF_ARRAY(eqs)>(eqs, rng.getUint32() % DE_LENGTH_OF_ARRAY(eqs));
}

BlendFunc getRandomBlendFunc(de::Random &rng)
{
    const uint32_t funcs[] = {GL_ZERO,
                              GL_ONE,
                              GL_SRC_COLOR,
                              GL_ONE_MINUS_SRC_COLOR,
                              GL_DST_COLOR,
                              GL_ONE_MINUS_DST_COLOR,
                              GL_SRC_ALPHA,
                              GL_ONE_MINUS_SRC_ALPHA,
                              GL_DST_ALPHA,
                              GL_ONE_MINUS_DST_ALPHA,
                              GL_CONSTANT_COLOR,
                              GL_ONE_MINUS_CONSTANT_COLOR,
                              GL_CONSTANT_ALPHA,
                              GL_ONE_MINUS_CONSTANT_ALPHA,
                              GL_SRC_ALPHA_SATURATE};

    const uint32_t src =
        de::getSizedArrayElement<DE_LENGTH_OF_ARRAY(funcs)>(funcs, rng.getUint32() % DE_LENGTH_OF_ARRAY(funcs));
    const uint32_t dst =
        de::getSizedArrayElement<DE_LENGTH_OF_ARRAY(funcs)>(funcs, rng.getUint32() % DE_LENGTH_OF_ARRAY(funcs));

    return BlendFunc(src, dst);
}

void genRandomBlendState(de::Random &rng, BlendState &blendState)
{
    if (rng.getBool())
        blendState.enableBlend = rng.getBool();

    if (rng.getBool())
    {
        if (rng.getBool())
            blendState.blendEq = getRandomBlendEq(rng);
        else
        {
            const BlendEq rgb   = getRandomBlendEq(rng);
            const BlendEq alpha = getRandomBlendEq(rng);

            blendState.blendEq = SeparateBlendEq(rgb, alpha);
        }
    }

    if (rng.getBool())
    {
        if (rng.getBool())
            blendState.blendFunc = getRandomBlendFunc(rng);
        else
        {
            const BlendFunc rgb   = getRandomBlendFunc(rng);
            const BlendFunc alpha = getRandomBlendFunc(rng);

            blendState.blendFunc = SeparateBlendFunc(rgb, alpha);
        }
    }

    if (rng.getBool())
    {
        const bool red   = rng.getBool();
        const bool green = rng.getBool();
        const bool blue  = rng.getBool();
        const bool alpha = rng.getBool();

        blendState.colorMask = BVec4(red, blue, green, alpha);
    }
}

TextureFormat getRandomFormat(de::Random &rng, Context &context)
{
    const bool supportsES32orGL45 = checkES32orGL45Support(context);

    const uint32_t glFormats[] = {
        GL_R8,         GL_RG8,     GL_RGB8,     GL_RGB565,  GL_RGBA4,  GL_RGB5_A1, GL_RGBA8,   GL_RGB10_A2,
        GL_RGB10_A2UI, GL_R8I,     GL_R8UI,     GL_R16I,    GL_R16UI,  GL_R32I,    GL_R32UI,   GL_RG8I,
        GL_RG8UI,      GL_RG16I,   GL_RG16UI,   GL_RG32I,   GL_RG32UI, GL_RGBA8I,  GL_RGBA8UI, GL_RGBA16I,
        GL_RGBA16UI,   GL_RGBA32I, GL_RGBA32UI, GL_RGBA16F, GL_R32F,   GL_RG32F,   GL_RGBA32F, GL_R11F_G11F_B10F};

    if (supportsES32orGL45)
        return glu::mapGLInternalFormat(
            de::getArrayElement(glFormats, rng.getUint32() % DE_LENGTH_OF_ARRAY(glFormats)));
    else
    {
        DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(glFormats) == 32);
        return glu::mapGLInternalFormat(
            de::getArrayElement(glFormats, rng.getUint32() % (DE_LENGTH_OF_ARRAY(glFormats) - 5)));
    }
}

void genRandomTest(de::Random &rng, BlendState &preCommon, BlendState &postCommon, vector<DrawBufferInfo> &drawBuffers,
                   int maxDrawBufferCount, Context &context)
{
    genRandomBlendState(rng, preCommon);
    genRandomBlendState(rng, postCommon);

    for (int drawBufferNdx = 0; drawBufferNdx < maxDrawBufferCount; drawBufferNdx++)
    {
        const bool render = rng.getFloat() > 0.1f;
        const IVec2 size(64, 64);
        const TextureFormat format(getRandomFormat(rng, context));
        BlendState blendState;

        genRandomBlendState(rng, blendState);

        // 32bit float formats don't support blending in GLES32
        if (format.type == tcu::TextureFormat::FLOAT)
        {
            // If format is 32bit float post common can't enable blending
            if (postCommon.enableBlend && *postCommon.enableBlend)
            {
                // Either don't set enable blend or disable blending
                if (rng.getBool())
                    postCommon.enableBlend = tcu::Nothing;
                else
                    postCommon.enableBlend = tcu::just(false);
            }

            // If post common doesn't disable blending, per attachment state or
            // pre common must.
            if (!postCommon.enableBlend)
            {
                // If pre common enables blend per attachment must disable it
                // If per attachment state changes blend state it must disable it
                if ((preCommon.enableBlend && *preCommon.enableBlend) || blendState.enableBlend)
                    blendState.enableBlend = tcu::just(false);
            }
        }

        drawBuffers.push_back(DrawBufferInfo(render, size, blendState, format));
    }
}

class MaxDrawBuffersIndexedTest : public TestCase
{
public:
    MaxDrawBuffersIndexedTest(Context &contet, int seed);

    void init(void);
    IterateResult iterate(void);

private:
    const int m_seed;
};

MaxDrawBuffersIndexedTest::MaxDrawBuffersIndexedTest(Context &context, int seed)
    : TestCase(context, de::toString(seed).c_str(), de::toString(seed).c_str())
    , m_seed(deInt32Hash(seed) ^ 1558001307u)
{
}

void MaxDrawBuffersIndexedTest::init(void)
{
    const bool supportsES32orGL45 = checkES32orGL45Support(m_context);

    if (!supportsES32orGL45 && !m_context.getContextInfo().isExtensionSupported("GL_EXT_draw_buffers_indexed"))
        TCU_THROW(NotSupportedError, "Extension GL_EXT_draw_buffers_indexed not supported");
}

TestCase::IterateResult MaxDrawBuffersIndexedTest::iterate(void)
{
    TestLog &log = m_testCtx.getLog();
    tcu::ResultCollector results(log);
    de::Random rng(m_seed);
    BlendState preCommonBlendState;
    BlendState postCommonBlendState;
    vector<DrawBufferInfo> drawBuffers;

    genRandomTest(rng, preCommonBlendState, postCommonBlendState, drawBuffers, 4, m_context);

    runTest(log, results, m_context.getRenderContext(), preCommonBlendState, postCommonBlendState, drawBuffers);

    results.setTestContextResult(m_testCtx);

    return STOP;
}

class ImplMaxDrawBuffersIndexedTest : public TestCase
{
public:
    ImplMaxDrawBuffersIndexedTest(Context &contet, int seed);

    void init(void);
    IterateResult iterate(void);

private:
    const int m_seed;
};

ImplMaxDrawBuffersIndexedTest::ImplMaxDrawBuffersIndexedTest(Context &context, int seed)
    : TestCase(context, de::toString(seed).c_str(), de::toString(seed).c_str())
    , m_seed(deInt32Hash(seed) ^ 2686315738u)
{
}

void ImplMaxDrawBuffersIndexedTest::init(void)
{
    const bool supportsES32orGL45 = checkES32orGL45Support(m_context);

    if (!supportsES32orGL45 && !m_context.getContextInfo().isExtensionSupported("GL_EXT_draw_buffers_indexed"))
        TCU_THROW(NotSupportedError, "Extension GL_EXT_draw_buffers_indexed not supported");
}

TestCase::IterateResult ImplMaxDrawBuffersIndexedTest::iterate(void)
{
    TestLog &log = m_testCtx.getLog();
    tcu::ResultCollector results(log);
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    de::Random rng(m_seed);
    int32_t maxDrawBuffers = 0;
    BlendState preCommonBlendState;
    BlendState postCommonBlendState;
    vector<DrawBufferInfo> drawBuffers;

    gl.getIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv(GL_MAX_DRAW_BUFFERS) failed");

    TCU_CHECK(maxDrawBuffers > 0);

    genRandomTest(rng, preCommonBlendState, postCommonBlendState, drawBuffers, maxDrawBuffers, m_context);

    runTest(log, results, m_context.getRenderContext(), preCommonBlendState, postCommonBlendState, drawBuffers);

    results.setTestContextResult(m_testCtx);

    return STOP;
}

enum PrePost
{
    PRE,
    POST
};

TestCase *createDiffTest(Context &context, PrePost prepost, const char *name, const BlendState &commonState,
                         const BlendState &drawBufferState)
{
    const BlendState emptyState = BlendState(tcu::Nothing, tcu::Nothing, tcu::Nothing, tcu::Nothing);

    if (prepost == PRE)
    {
        const BlendState preState =
            BlendState((commonState.enableBlend ? commonState.enableBlend : just(true)), commonState.blendEq,
                       (commonState.blendFunc ? commonState.blendFunc :
                                                just(Either<BlendFunc, SeparateBlendFunc>(BlendFunc(GL_ONE, GL_ONE)))),
                       tcu::Nothing);
        vector<DrawBufferInfo> drawBuffers;

        drawBuffers.push_back(DrawBufferInfo(true, IVec2(64, 64), emptyState,
                                             TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8)));
        drawBuffers.push_back(DrawBufferInfo(true, IVec2(64, 64), drawBufferState,
                                             TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8)));

        return new DrawBuffersIndexedTest(context, preState, emptyState, drawBuffers, name, name);
    }
    else if (prepost == POST)
    {
        const BlendState preState =
            BlendState(just(true), tcu::Nothing, Maybe<Either<BlendFunc, SeparateBlendFunc>>(BlendFunc(GL_ONE, GL_ONE)),
                       tcu::Nothing);
        vector<DrawBufferInfo> drawBuffers;

        drawBuffers.push_back(DrawBufferInfo(true, IVec2(64, 64), emptyState,
                                             TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8)));
        drawBuffers.push_back(DrawBufferInfo(true, IVec2(64, 64), drawBufferState,
                                             TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8)));

        return new DrawBuffersIndexedTest(context, preState, commonState, drawBuffers, name, name);
    }
    else
    {
        DE_ASSERT(false);
        return nullptr;
    }
}

TestCase *createAdvancedEqDiffTest(Context &context, PrePost prepost, const char *name, const BlendState &commonState,
                                   const BlendState &drawBufferState)
{
    const BlendState emptyState = BlendState(tcu::Nothing, tcu::Nothing, tcu::Nothing, tcu::Nothing);

    if (prepost == PRE)
    {
        const BlendState preState =
            BlendState((commonState.enableBlend ? commonState.enableBlend : just(true)), commonState.blendEq,
                       (commonState.blendFunc ? commonState.blendFunc :
                                                just(Either<BlendFunc, SeparateBlendFunc>(BlendFunc(GL_ONE, GL_ONE)))),
                       tcu::Nothing);
        vector<DrawBufferInfo> drawBuffers;

        drawBuffers.push_back(DrawBufferInfo(true, IVec2(64, 64), drawBufferState,
                                             TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8)));

        return new DrawBuffersIndexedTest(context, preState, emptyState, drawBuffers, name, name);
    }
    else if (prepost == POST)
    {
        const BlendState preState =
            BlendState(just(true), tcu::Nothing, Maybe<Either<BlendFunc, SeparateBlendFunc>>(BlendFunc(GL_ONE, GL_ONE)),
                       tcu::Nothing);
        vector<DrawBufferInfo> drawBuffers;

        drawBuffers.push_back(DrawBufferInfo(true, IVec2(64, 64), drawBufferState,
                                             TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8)));

        return new DrawBuffersIndexedTest(context, preState, commonState, drawBuffers, name, name);
    }
    else
    {
        DE_ASSERT(false);
        return nullptr;
    }
}

void addDrawBufferCommonTests(TestCaseGroup *root, PrePost prepost)
{
    const BlendState emptyState = BlendState(Maybe<bool>(), Maybe<Either<BlendEq, SeparateBlendEq>>(),
                                             Maybe<Either<BlendFunc, SeparateBlendFunc>>(), Maybe<BVec4>());

    {
        const BlendState disableState = BlendState(just(false), Maybe<Either<BlendEq, SeparateBlendEq>>(),
                                                   Maybe<Either<BlendFunc, SeparateBlendFunc>>(), Maybe<BVec4>());
        const BlendState enableState  = BlendState(just(true), Maybe<Either<BlendEq, SeparateBlendEq>>(),
                                                   Maybe<Either<BlendFunc, SeparateBlendFunc>>(), Maybe<BVec4>());

        root->addChild(
            createDiffTest(root->getContext(), prepost, "common_enable_buffer_enable", enableState, enableState));
        root->addChild(
            createDiffTest(root->getContext(), prepost, "common_disable_buffer_disable", disableState, disableState));
        root->addChild(
            createDiffTest(root->getContext(), prepost, "common_disable_buffer_enable", disableState, enableState));
        root->addChild(
            createDiffTest(root->getContext(), prepost, "common_enable_buffer_disable", enableState, disableState));
    }

    {
        const BlendState eqStateA = BlendState(tcu::Nothing, Maybe<Either<BlendEq, SeparateBlendEq>>(GL_FUNC_ADD),
                                               Maybe<Either<BlendFunc, SeparateBlendFunc>>(), Maybe<BVec4>());
        const BlendState eqStateB = BlendState(tcu::Nothing, Maybe<Either<BlendEq, SeparateBlendEq>>(GL_FUNC_SUBTRACT),
                                               Maybe<Either<BlendFunc, SeparateBlendFunc>>(), Maybe<BVec4>());

        const BlendState separateEqStateA = BlendState(
            tcu::Nothing, Maybe<Either<BlendEq, SeparateBlendEq>>(SeparateBlendEq(GL_FUNC_ADD, GL_FUNC_SUBTRACT)),
            Maybe<Either<BlendFunc, SeparateBlendFunc>>(), Maybe<BVec4>());
        const BlendState separateEqStateB = BlendState(
            tcu::Nothing, Maybe<Either<BlendEq, SeparateBlendEq>>(SeparateBlendEq(GL_FUNC_SUBTRACT, GL_FUNC_ADD)),
            Maybe<Either<BlendFunc, SeparateBlendFunc>>(), Maybe<BVec4>());

        const BlendState advancedEqStateA =
            BlendState(tcu::Nothing, Maybe<Either<BlendEq, SeparateBlendEq>>(GL_DIFFERENCE),
                       Maybe<Either<BlendFunc, SeparateBlendFunc>>(), Maybe<BVec4>());
        const BlendState advancedEqStateB = BlendState(tcu::Nothing, Maybe<Either<BlendEq, SeparateBlendEq>>(GL_SCREEN),
                                                       Maybe<Either<BlendFunc, SeparateBlendFunc>>(), Maybe<BVec4>());

        root->addChild(
            createDiffTest(root->getContext(), prepost, "common_blend_eq_buffer_blend_eq", eqStateA, eqStateB));
        root->addChild(createDiffTest(root->getContext(), prepost, "common_blend_eq_buffer_separate_blend_eq", eqStateA,
                                      separateEqStateB));
        root->addChild(createAdvancedEqDiffTest(root->getContext(), prepost, "common_blend_eq_buffer_advanced_blend_eq",
                                                eqStateA, advancedEqStateB));

        root->addChild(createDiffTest(root->getContext(), prepost, "common_separate_blend_eq_buffer_blend_eq",
                                      separateEqStateA, eqStateB));
        root->addChild(createDiffTest(root->getContext(), prepost, "common_separate_blend_eq_buffer_separate_blend_eq",
                                      separateEqStateA, separateEqStateB));
        root->addChild(createAdvancedEqDiffTest(root->getContext(), prepost,
                                                "common_separate_blend_eq_buffer_advanced_blend_eq", separateEqStateA,
                                                advancedEqStateB));

        root->addChild(createAdvancedEqDiffTest(root->getContext(), prepost, "common_advanced_blend_eq_buffer_blend_eq",
                                                advancedEqStateA, eqStateB));
        root->addChild(createAdvancedEqDiffTest(root->getContext(), prepost,
                                                "common_advanced_blend_eq_buffer_separate_blend_eq", advancedEqStateA,
                                                separateEqStateB));
        root->addChild(createAdvancedEqDiffTest(root->getContext(), prepost,
                                                "common_advanced_blend_eq_buffer_advanced_blend_eq", advancedEqStateA,
                                                advancedEqStateB));
    }

    {
        const BlendState funcStateA = BlendState(
            tcu::Nothing, Maybe<Either<BlendEq, SeparateBlendEq>>(),
            Maybe<Either<BlendFunc, SeparateBlendFunc>>(BlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA)), Maybe<BVec4>());
        const BlendState funcStateB = BlendState(
            tcu::Nothing, Maybe<Either<BlendEq, SeparateBlendEq>>(),
            Maybe<Either<BlendFunc, SeparateBlendFunc>>(BlendFunc(GL_DST_ALPHA, GL_SRC_ALPHA)), Maybe<BVec4>());
        const BlendState separateFuncStateA = BlendState(
            tcu::Nothing, Maybe<Either<BlendEq, SeparateBlendEq>>(),
            Maybe<Either<BlendFunc, SeparateBlendFunc>>(SeparateBlendFunc(
                BlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA), BlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_DST_ALPHA))),
            Maybe<BVec4>());
        const BlendState separateFuncStateB = BlendState(
            tcu::Nothing, Maybe<Either<BlendEq, SeparateBlendEq>>(),
            Maybe<Either<BlendFunc, SeparateBlendFunc>>(SeparateBlendFunc(
                BlendFunc(GL_DST_ALPHA, GL_SRC_ALPHA), BlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA))),
            Maybe<BVec4>());

        root->addChild(
            createDiffTest(root->getContext(), prepost, "common_blend_func_buffer_blend_func", funcStateA, funcStateB));
        root->addChild(createDiffTest(root->getContext(), prepost, "common_blend_func_buffer_separate_blend_func",
                                      funcStateA, separateFuncStateB));
        root->addChild(createDiffTest(root->getContext(), prepost, "common_separate_blend_func_buffer_blend_func",
                                      separateFuncStateA, funcStateB));
        root->addChild(createDiffTest(root->getContext(), prepost,
                                      "common_separate_blend_func_buffer_separate_blend_func", separateFuncStateA,
                                      separateFuncStateB));
    }

    {
        const BlendState commonColorMaskState =
            BlendState(tcu::Nothing, Maybe<Either<BlendEq, SeparateBlendEq>>(),
                       Maybe<Either<BlendFunc, SeparateBlendFunc>>(), Maybe<BVec4>(BVec4(true, false, true, false)));
        const BlendState bufferColorMaskState =
            BlendState(tcu::Nothing, Maybe<Either<BlendEq, SeparateBlendEq>>(),
                       Maybe<Either<BlendFunc, SeparateBlendFunc>>(), Maybe<BVec4>(BVec4(false, true, false, true)));

        root->addChild(createDiffTest(root->getContext(), prepost, "common_color_mask_buffer_color_mask",
                                      commonColorMaskState, bufferColorMaskState));
    }
}

void addRandomMaxTest(TestCaseGroup *root)
{
    for (int i = 0; i < 20; i++)
        root->addChild(new MaxDrawBuffersIndexedTest(root->getContext(), i));
}

void addRandomImplMaxTest(TestCaseGroup *root)
{
    for (int i = 0; i < 20; i++)
        root->addChild(new ImplMaxDrawBuffersIndexedTest(root->getContext(), i));
}

} // namespace

TestCaseGroup *createDrawBuffersIndexedTests(Context &context)
{
    const BlendState emptyState = BlendState(Maybe<bool>(), Maybe<Either<BlendEq, SeparateBlendEq>>(),
                                             Maybe<Either<BlendFunc, SeparateBlendFunc>>(), Maybe<BVec4>());
    TestCaseGroup *const group  = new TestCaseGroup(context, "draw_buffers_indexed",
                                                    "Test for indexed draw buffers. GL_EXT_draw_buffers_indexed.");

    TestCaseGroup *const preGroup = new TestCaseGroup(
        context, "overwrite_common", "Set common state and overwrite it with draw buffer blend state.");
    TestCaseGroup *const postGroup =
        new TestCaseGroup(context, "overwrite_indexed", "Set indexed blend state and overwrite it with common state.");
    TestCaseGroup *const randomGroup = new TestCaseGroup(context, "random", "Random indexed blend state tests.");
    TestCaseGroup *const maxGroup    = new TestCaseGroup(context, "max_required_draw_buffers",
                                                         "Random tests using minimum maximum number of draw buffers.");
    TestCaseGroup *const maxImplGroup =
        new TestCaseGroup(context, "max_implementation_draw_buffers",
                          "Random tests using maximum number of draw buffers reported by implementation.");

    group->addChild(preGroup);
    group->addChild(postGroup);
    group->addChild(randomGroup);

    randomGroup->addChild(maxGroup);
    randomGroup->addChild(maxImplGroup);

    addDrawBufferCommonTests(preGroup, PRE);
    addDrawBufferCommonTests(postGroup, POST);
    addRandomMaxTest(maxGroup);
    addRandomImplMaxTest(maxImplGroup);

    return group;
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
