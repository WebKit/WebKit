/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
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
 * \brief Framebuffer completeness tests.
 *//*--------------------------------------------------------------------*/

#include "glsFboCompletenessTests.hpp"

#include "gluStrUtil.hpp"
#include "gluObjectWrapper.hpp"
#include "deStringUtil.hpp"

#include <cctype>
#include <iterator>
#include <algorithm>

using namespace glw;
using de::toLower;
using de::toString;
using glu::Framebuffer;
using glu::getErrorName;
using glu::getFramebufferStatusName;
using glu::getTextureFormatName;
using glu::getTypeName;
using glu::RenderContext;
using std::string;
using tcu::MessageBuilder;
using tcu::TestCase;
using tcu::TestCaseGroup;
using tcu::TestLog;
using tcu::TestNode;
using namespace deqp::gls::FboUtil;
using namespace deqp::gls::FboUtil::config;
typedef TestCase::IterateResult IterateResult;

namespace deqp
{
namespace gls
{
namespace fboc
{

namespace details
{

// The following extensions are applicable both to ES2 and ES3.

// GL_OES_depth_texture
static const FormatKey s_oesDepthTextureFormats[] = {
    GLS_UNSIZED_FORMATKEY(GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT),
    GLS_UNSIZED_FORMATKEY(GL_DEPTH_COMPONENT, GL_UNSIGNED_INT),
};

// GL_OES_packed_depth_stencil
static const FormatKey s_oesPackedDepthStencilSizedFormats[] = {
    GL_DEPTH24_STENCIL8,
};

static const FormatKey s_oesPackedDepthStencilTexFormats[] = {
    GLS_UNSIZED_FORMATKEY(GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8),
};

// GL_OES_required_internalformat
static const FormatKey s_oesRequiredInternalFormatColorFormats[] = {
    // Same as ES2 RBO formats, plus RGBA8 (even without OES_rgb8_rgba8)
    GL_RGB5_A1, GL_RGBA8, GL_RGBA4, GL_RGB565};

static const FormatKey s_oesRequiredInternalFormatDepthFormats[] = {
    GL_DEPTH_COMPONENT16,
};

// GL_EXT_color_buffer_half_float
static const FormatKey s_extColorBufferHalfFloatFormats[] = {
    GL_RGBA16F,
    GL_RGB16F,
    GL_RG16F,
    GL_R16F,
};

static const FormatKey s_oesDepth24SizedFormats[] = {GL_DEPTH_COMPONENT24};

static const FormatKey s_oesDepth32SizedFormats[] = {GL_DEPTH_COMPONENT32};

static const FormatKey s_oesRgb8Rgba8RboFormats[] = {
    GL_RGB8,
    GL_RGBA8,
};

static const FormatKey s_oesRequiredInternalFormatRgb8ColorFormat[] = {
    GL_RGB8,
};

static const FormatKey s_extTextureType2101010RevFormats[] = {
    GLS_UNSIZED_FORMATKEY(GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV),
    GLS_UNSIZED_FORMATKEY(GL_RGB, GL_UNSIGNED_INT_2_10_10_10_REV),
};

static const FormatKey s_oesRequiredInternalFormat10bitColorFormats[] = {
    GL_RGB10_A2,
    GL_RGB10,
};

static const FormatKey s_extTextureRgRboFormats[] = {
    GL_R8,
    GL_RG8,
};

static const FormatKey s_extTextureRgTexFormats[] = {
    GLS_UNSIZED_FORMATKEY(GL_RED, GL_UNSIGNED_BYTE),
    GLS_UNSIZED_FORMATKEY(GL_RG, GL_UNSIGNED_BYTE),
};

static const FormatKey s_extTextureRgFloatTexFormats[] = {
    GLS_UNSIZED_FORMATKEY(GL_RED, GL_FLOAT),
    GLS_UNSIZED_FORMATKEY(GL_RG, GL_FLOAT),
};

static const FormatKey s_extTextureRgHalfFloatTexFormats[] = {
    GLS_UNSIZED_FORMATKEY(GL_RED, GL_HALF_FLOAT_OES),
    GLS_UNSIZED_FORMATKEY(GL_RG, GL_HALF_FLOAT_OES),
};

static const FormatKey s_nvPackedFloatRboFormats[] = {
    GL_R11F_G11F_B10F,
};

static const FormatKey s_nvPackedFloatTexFormats[] = {
    GLS_UNSIZED_FORMATKEY(GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV),
};

static const FormatKey s_extSrgbRboFormats[] = {
    GL_SRGB8_ALPHA8,
};

static const FormatKey s_extSrgbRenderableTexFormats[] = {
    GLS_UNSIZED_FORMATKEY(GL_SRGB_ALPHA, GL_UNSIGNED_BYTE),
};

static const FormatKey s_extSrgbNonRenderableTexFormats[] = {
    GLS_UNSIZED_FORMATKEY(GL_SRGB, GL_UNSIGNED_BYTE),
};

static const FormatKey s_nvSrgbFormatsRboFormats[] = {
    GL_SRGB8,
};

static const FormatKey s_nvSrgbFormatsTextureFormats[] = {
    GL_SRGB8,

    // The extension does not actually require any unsized format
    // to be renderable. However, the renderablility of unsized
    // SRGB,UBYTE internalformat-type pair is implied.
    GLS_UNSIZED_FORMATKEY(GL_SRGB, GL_UNSIGNED_BYTE),
};

static const FormatKey s_oesRgb8Rgba8TexFormats[] = {
    GLS_UNSIZED_FORMATKEY(GL_RGB, GL_UNSIGNED_BYTE),
    GLS_UNSIZED_FORMATKEY(GL_RGBA, GL_UNSIGNED_BYTE),
};

static const FormatKey s_extTextureSRGBR8Formats[] = {
    GL_SR8_EXT,
};

static const FormatKey s_extTextureSRGBRG8Formats[] = {
    GL_SRG8_EXT,
};

static const FormatKey s_qcomRenderSRGBR8RG8Formats[] = {
    GL_SR8_EXT,
    GL_SRG8_EXT,
};

static const FormatKey s_extTextureFormatBGRA8888Formats[] = {
    GL_BGRA,
    GL_BGRA8_EXT,
};

static const FormatExtEntry s_esExtFormats[] = {
    {
        "GL_OES_depth_texture",
        (uint32_t)(REQUIRED_RENDERABLE | DEPTH_RENDERABLE | TEXTURE_VALID),
        GLS_ARRAY_RANGE(s_oesDepthTextureFormats),
    },
    {"GL_OES_packed_depth_stencil",
     (uint32_t)(REQUIRED_RENDERABLE | DEPTH_RENDERABLE | STENCIL_RENDERABLE | RENDERBUFFER_VALID),
     GLS_ARRAY_RANGE(s_oesPackedDepthStencilSizedFormats)},
    {"GL_OES_packed_depth_stencil GL_OES_required_internalformat", (uint32_t)TEXTURE_VALID,
     GLS_ARRAY_RANGE(s_oesPackedDepthStencilSizedFormats)},
    {"GL_OES_packed_depth_stencil GL_OES_depth_texture",
     (uint32_t)(DEPTH_RENDERABLE | STENCIL_RENDERABLE | TEXTURE_VALID),
     GLS_ARRAY_RANGE(s_oesPackedDepthStencilTexFormats)},
    // The ANGLE extension incorporates GL_OES_depth_texture/GL_OES_packed_depth_stencil.
    {
        "GL_ANGLE_depth_texture",
        (uint32_t)(REQUIRED_RENDERABLE | DEPTH_RENDERABLE | TEXTURE_VALID),
        GLS_ARRAY_RANGE(s_oesDepthTextureFormats),
    },
    {
        "GL_OES_packed_depth_stencil GL_ANGLE_depth_texture",
        (uint32_t)(DEPTH_RENDERABLE | STENCIL_RENDERABLE | TEXTURE_VALID),
        GLS_ARRAY_RANGE(s_oesPackedDepthStencilTexFormats),
    },
    // \todo [2013-12-10 lauri] Find out if OES_texture_half_float is really a
    // requirement on ES3 also. Or is color_buffer_half_float applicatble at
    // all on ES3, since there's also EXT_color_buffer_float?
    {"GL_OES_texture_half_float GL_EXT_color_buffer_half_float",
     (uint32_t)(REQUIRED_RENDERABLE | COLOR_RENDERABLE | RENDERBUFFER_VALID),
     GLS_ARRAY_RANGE(s_extColorBufferHalfFloatFormats)},

    // OES_required_internalformat doesn't actually specify that these are renderable,
    // since it was written against ES 1.1.
    {"GL_OES_required_internalformat",
     // Allow but don't require RGBA8 to be color-renderable if
     // OES_rgb8_rgba8 is not present.
     (uint32_t)(COLOR_RENDERABLE | TEXTURE_VALID), GLS_ARRAY_RANGE(s_oesRequiredInternalFormatColorFormats)},
    {"GL_OES_required_internalformat", (uint32_t)(DEPTH_RENDERABLE | TEXTURE_VALID),
     GLS_ARRAY_RANGE(s_oesRequiredInternalFormatDepthFormats)},
    {"GL_EXT_texture_rg", (uint32_t)(REQUIRED_RENDERABLE | COLOR_RENDERABLE | RENDERBUFFER_VALID | TEXTURE_VALID),
     GLS_ARRAY_RANGE(s_extTextureRgRboFormats)},
    // These are not specified to be color-renderable, but the wording is
    // exactly as ambiguous as the wording in the ES2 spec.
    {"GL_EXT_texture_rg", (uint32_t)(COLOR_RENDERABLE | TEXTURE_VALID), GLS_ARRAY_RANGE(s_extTextureRgTexFormats)},
    {"GL_EXT_texture_rg GL_OES_texture_float", (uint32_t)(COLOR_RENDERABLE | TEXTURE_VALID),
     GLS_ARRAY_RANGE(s_extTextureRgFloatTexFormats)},
    {"GL_EXT_texture_rg GL_OES_texture_half_float", (uint32_t)(COLOR_RENDERABLE | TEXTURE_VALID),
     GLS_ARRAY_RANGE(s_extTextureRgHalfFloatTexFormats)},

    {"GL_NV_packed_float", (uint32_t)(COLOR_RENDERABLE | TEXTURE_VALID), GLS_ARRAY_RANGE(s_nvPackedFloatTexFormats)},
    {"GL_NV_packed_float GL_EXT_color_buffer_half_float",
     (uint32_t)(REQUIRED_RENDERABLE | COLOR_RENDERABLE | RENDERBUFFER_VALID),
     GLS_ARRAY_RANGE(s_nvPackedFloatRboFormats)},

    {"GL_EXT_sRGB", (uint32_t)(COLOR_RENDERABLE | TEXTURE_VALID), GLS_ARRAY_RANGE(s_extSrgbRenderableTexFormats)},
    {"GL_EXT_sRGB", (uint32_t)TEXTURE_VALID, GLS_ARRAY_RANGE(s_extSrgbNonRenderableTexFormats)},
    {"GL_EXT_sRGB", (uint32_t)(REQUIRED_RENDERABLE | COLOR_RENDERABLE | RENDERBUFFER_VALID),
     GLS_ARRAY_RANGE(s_extSrgbRboFormats)},
    {"GL_NV_sRGB_formats", (uint32_t)(REQUIRED_RENDERABLE | COLOR_RENDERABLE | RENDERBUFFER_VALID),
     GLS_ARRAY_RANGE(s_nvSrgbFormatsRboFormats)},
    {"GL_NV_sRGB_formats", (uint32_t)(REQUIRED_RENDERABLE | COLOR_RENDERABLE | TEXTURE_VALID),
     GLS_ARRAY_RANGE(s_nvSrgbFormatsTextureFormats)},

    // In Khronos bug 7333 discussion, the consensus is that these texture
    // formats, at least, should be color-renderable. Still, that cannot be
    // found in any extension specs, so only allow it, not require it.
    {"GL_OES_rgb8_rgba8", (uint32_t)(COLOR_RENDERABLE | TEXTURE_VALID), GLS_ARRAY_RANGE(s_oesRgb8Rgba8TexFormats)},
    {"GL_OES_rgb8_rgba8", (uint32_t)(REQUIRED_RENDERABLE | COLOR_RENDERABLE | RENDERBUFFER_VALID),
     GLS_ARRAY_RANGE(s_oesRgb8Rgba8RboFormats)},
    {"GL_OES_rgb8_rgba8 GL_OES_required_internalformat", (uint32_t)TEXTURE_VALID,
     GLS_ARRAY_RANGE(s_oesRequiredInternalFormatRgb8ColorFormat)},

    // The depth-renderability of the depth RBO formats is not explicitly
    // spelled out, but all renderbuffer formats are meant to be renderable.
    {"GL_OES_depth24", (uint32_t)(REQUIRED_RENDERABLE | DEPTH_RENDERABLE | RENDERBUFFER_VALID),
     GLS_ARRAY_RANGE(s_oesDepth24SizedFormats)},
    {"GL_OES_depth24 GL_OES_required_internalformat GL_OES_depth_texture", (uint32_t)TEXTURE_VALID,
     GLS_ARRAY_RANGE(s_oesDepth24SizedFormats)},

    {"GL_OES_depth32", (uint32_t)(REQUIRED_RENDERABLE | DEPTH_RENDERABLE | RENDERBUFFER_VALID),
     GLS_ARRAY_RANGE(s_oesDepth32SizedFormats)},
    {"GL_OES_depth32 GL_OES_required_internalformat GL_OES_depth_texture", (uint32_t)TEXTURE_VALID,
     GLS_ARRAY_RANGE(s_oesDepth32SizedFormats)},

    {"GL_EXT_texture_type_2_10_10_10_REV",
     (uint32_t)TEXTURE_VALID, // explicitly unrenderable
     GLS_ARRAY_RANGE(s_extTextureType2101010RevFormats)},
    {"GL_EXT_texture_type_2_10_10_10_REV GL_OES_required_internalformat",
     (uint32_t)TEXTURE_VALID, // explicitly unrenderable
     GLS_ARRAY_RANGE(s_oesRequiredInternalFormat10bitColorFormats)},

    {"GL_EXT_texture_sRGB_R8", (uint32_t)TEXTURE_VALID, GLS_ARRAY_RANGE(s_extTextureSRGBR8Formats)},
    {"GL_EXT_texture_sRGB_RG8", (uint32_t)TEXTURE_VALID, GLS_ARRAY_RANGE(s_extTextureSRGBRG8Formats)},

    {"GL_QCOM_render_sRGB_R8_RG8",
     (uint32_t)(REQUIRED_RENDERABLE | COLOR_RENDERABLE | RENDERBUFFER_VALID | TEXTURE_VALID),
     GLS_ARRAY_RANGE(s_qcomRenderSRGBR8RG8Formats)},

    {"GL_EXT_texture_format_BGRA8888",
     (uint32_t)(REQUIRED_RENDERABLE | COLOR_RENDERABLE | RENDERBUFFER_VALID | TEXTURE_VALID),
     GLS_ARRAY_RANGE(s_extTextureFormatBGRA8888Formats)},
};

Context::Context(TestContext &testCtx, RenderContext &renderCtx, CheckerFactory &factory)
    : m_testCtx(testCtx)
    , m_renderCtx(renderCtx)
    , m_verifier(m_ctxFormats, factory, renderCtx)
    , m_haveMultiColorAtts(false)
{
    FormatExtEntries extRange = GLS_ARRAY_RANGE(s_esExtFormats);
    addExtFormats(extRange);
}

void Context::addFormats(FormatEntries fmtRange)
{
    FboUtil::addFormats(m_coreFormats, fmtRange);
    FboUtil::addFormats(m_ctxFormats, fmtRange);
    FboUtil::addFormats(m_allFormats, fmtRange);
}

void Context::addExtFormats(FormatExtEntries extRange)
{
    FboUtil::addExtFormats(m_ctxFormats, extRange, &m_renderCtx);
    FboUtil::addExtFormats(m_allFormats, extRange, nullptr);
}

void TestBase::pass(void)
{
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
}

void TestBase::qualityWarning(const char *msg)
{
    m_testCtx.setTestResult(QP_TEST_RESULT_QUALITY_WARNING, msg);
}

void TestBase::fail(const char *msg)
{
    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, msg);
}

const glw::Functions &gl(const TestBase &test)
{
    return test.getContext().getRenderContext().getFunctions();
}

static bool isFormatFeatureSupported(const FormatDB &db, const ImageFormat &format, FormatFlags feature)
{
    return db.isKnownFormat(format) && ((db.getFormatInfo(format) & feature) == feature);
}

static void logAffectingExtensions(const char *prefix, const FormatDB &db, const ImageFormat &format,
                                   FormatFlags feature, tcu::MessageBuilder &msg)
{
    const std::set<std::set<std::string>> rows = db.getFormatFeatureExtensions(format, feature);

    for (std::set<std::set<std::string>>::const_iterator rowIt = rows.begin(); rowIt != rows.end(); ++rowIt)
    {
        const std::set<std::string> &requiredExtensions = *rowIt;
        std::set<std::string>::const_iterator it        = requiredExtensions.begin();
        std::string extName;

        msg << prefix;

        extName = *it++;
        while (it != requiredExtensions.end())
        {
            msg << getExtensionDescription(extName);
            extName = *it++;
            msg << (it == requiredExtensions.end() ? " and " : ", ");
        }

        msg << getExtensionDescription(extName) << '\n';
    }
}

static void logFormatInfo(const config::Framebuffer &fbo, const FormatDB &ctxFormats, const FormatDB &coreFormats,
                          const FormatDB &allFormats, tcu::TestLog &log)
{
    static const struct
    {
        const char *name;
        const FormatFlags flag;
    } s_renderability[] = {
        {"color-renderable", COLOR_RENDERABLE},
        {"depth-renderable", DEPTH_RENDERABLE},
        {"stencil-renderable", STENCIL_RENDERABLE},
    };

    std::set<ImageFormat> formats;

    for (config::TextureMap::const_iterator it = fbo.textures.begin(); it != fbo.textures.end(); ++it)
        formats.insert(it->second->internalFormat);
    for (config::RboMap::const_iterator it = fbo.rbos.begin(); it != fbo.rbos.end(); ++it)
        formats.insert(it->second->internalFormat);

    if (!formats.empty())
    {
        const tcu::ScopedLogSection supersection(log, "Format", "Format info");

        for (std::set<ImageFormat>::const_iterator it = formats.begin(); it != formats.end(); ++it)
        {
            const tcu::ScopedLogSection section(log, "FormatInfo", de::toString(*it));

            // texture validity
            if (isFormatFeatureSupported(ctxFormats, *it, TEXTURE_VALID))
            {
                tcu::MessageBuilder msg(&log);
                msg << "* Valid texture format\n";

                if (isFormatFeatureSupported(coreFormats, *it, TEXTURE_VALID))
                    msg << "\t* core feature";
                else
                {
                    msg << "\t* defined in supported extension(s):\n";
                    logAffectingExtensions("\t\t- ", ctxFormats, *it, TEXTURE_VALID, msg);
                }

                msg << tcu::TestLog::EndMessage;
            }
            else
            {
                tcu::MessageBuilder msg(&log);
                msg << "* Unsupported texture format\n";

                if (isFormatFeatureSupported(allFormats, *it, TEXTURE_VALID))
                {
                    msg << "\t* requires any of the extensions or combinations:\n";
                    logAffectingExtensions("\t\t- ", allFormats, *it, TEXTURE_VALID, msg);
                }
                else
                    msg << "\t* no extension can make this format valid";

                msg << tcu::TestLog::EndMessage;
            }

            // RBO validity
            if (isFormatFeatureSupported(ctxFormats, *it, RENDERBUFFER_VALID))
            {
                tcu::MessageBuilder msg(&log);
                msg << "* Valid renderbuffer format\n";

                if (isFormatFeatureSupported(coreFormats, *it, RENDERBUFFER_VALID))
                    msg << "\t* core feature";
                else
                {
                    msg << "\t* defined in supported extension(s):\n";
                    logAffectingExtensions("\t\t- ", ctxFormats, *it, RENDERBUFFER_VALID, msg);
                }

                msg << tcu::TestLog::EndMessage;
            }
            else
            {
                tcu::MessageBuilder msg(&log);
                msg << "* Unsupported renderbuffer format\n";

                if (isFormatFeatureSupported(allFormats, *it, RENDERBUFFER_VALID))
                {
                    msg << "\t* requires any of the extensions or combinations:\n";
                    logAffectingExtensions("\t\t- ", allFormats, *it, RENDERBUFFER_VALID, msg);
                }
                else
                    msg << "\t* no extension can make this format valid";

                msg << tcu::TestLog::EndMessage;
            }

            // renderability
            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_renderability); ++ndx)
            {
                if (isFormatFeatureSupported(ctxFormats, *it, s_renderability[ndx].flag | REQUIRED_RENDERABLE))
                {
                    tcu::MessageBuilder msg(&log);
                    msg << "* Format is " << s_renderability[ndx].name << "\n";

                    if (isFormatFeatureSupported(coreFormats, *it, s_renderability[ndx].flag | REQUIRED_RENDERABLE))
                        msg << "\t* core feature";
                    else
                    {
                        msg << "\t* defined in supported extension(s):\n";
                        logAffectingExtensions("\t\t- ", ctxFormats, *it,
                                               s_renderability[ndx].flag | REQUIRED_RENDERABLE, msg);
                    }

                    msg << tcu::TestLog::EndMessage;
                }
                else if (isFormatFeatureSupported(ctxFormats, *it, s_renderability[ndx].flag))
                {
                    tcu::MessageBuilder msg(&log);
                    msg << "* Format is allowed to be " << s_renderability[ndx].name << " but not required\n";

                    if (isFormatFeatureSupported(coreFormats, *it, s_renderability[ndx].flag))
                        msg << "\t* core feature";
                    else if (isFormatFeatureSupported(allFormats, *it, s_renderability[ndx].flag))
                    {
                        msg << "\t* extensions that would make format " << s_renderability[ndx].name << ":\n";
                        logAffectingExtensions("\t\t- ", allFormats, *it, s_renderability[ndx].flag, msg);
                    }
                    else
                        msg << "\t* no extension can make this format " << s_renderability[ndx].name;

                    msg << tcu::TestLog::EndMessage;
                }
                else
                {
                    tcu::MessageBuilder msg(&log);
                    msg << "* Format is NOT " << s_renderability[ndx].name << "\n";

                    if (isFormatFeatureSupported(allFormats, *it, s_renderability[ndx].flag))
                    {
                        if (isFormatFeatureSupported(allFormats, *it, s_renderability[ndx].flag | REQUIRED_RENDERABLE))
                        {
                            msg << "\t* extensions that would make format " << s_renderability[ndx].name << ":\n";
                            logAffectingExtensions("\t\t- ", allFormats, *it,
                                                   s_renderability[ndx].flag | REQUIRED_RENDERABLE, msg);
                        }
                        else
                        {
                            msg << "\t* extensions that are allowed to make format " << s_renderability[ndx].name
                                << ":\n";
                            logAffectingExtensions("\t\t- ", allFormats, *it, s_renderability[ndx].flag, msg);
                        }
                    }
                    else
                        msg << "\t* no extension can make this format " << s_renderability[ndx].name;

                    msg << tcu::TestLog::EndMessage;
                }
            }
        }
    }
}

IterateResult TestBase::iterate(void)
{
    glu::Framebuffer fbo(m_ctx.getRenderContext());
    FboBuilder builder(*fbo, GL_FRAMEBUFFER, gl(*this));
    const IterateResult ret          = build(builder);
    const ValidStatusCodes reference = m_ctx.getVerifier().validStatusCodes(builder);
    const GLenum errorCode           = builder.getError();

    logFramebufferConfig(builder, m_testCtx.getLog());
    logFormatInfo(builder, m_ctx.getCtxFormats(), m_ctx.getCoreFormats(), m_ctx.getAllFormats(), m_testCtx.getLog());
    reference.logRules(m_testCtx.getLog());
    reference.logLegalResults(m_testCtx.getLog());

    // \todo [2013-12-04 lauri] Check if drawing operations succeed.

    if (errorCode != GL_NO_ERROR)
    {
        m_testCtx.getLog() << TestLog::Message << "Received " << glu::getErrorStr(errorCode)
                           << " (during FBO initialization)." << TestLog::EndMessage;

        if (reference.isErrorCodeValid(errorCode))
            pass();
        else if (reference.isErrorCodeRequired(GL_NO_ERROR))
            fail(("Expected no error but got " + de::toString(glu::getErrorStr(errorCode))).c_str());
        else
            fail("Got wrong error code");
    }
    else
    {
        const GLenum fboStatus = gl(*this).checkFramebufferStatus(GL_FRAMEBUFFER);
        const bool validStatus = reference.isFBOStatusValid(fboStatus);

        m_testCtx.getLog() << TestLog::Message << "Received " << glu::getFramebufferStatusStr(fboStatus) << "."
                           << TestLog::EndMessage;

        if (!validStatus)
        {
            if (fboStatus == GL_FRAMEBUFFER_COMPLETE)
                fail("Framebuffer checked as complete, expected incomplete");
            else if (reference.isFBOStatusRequired(GL_FRAMEBUFFER_COMPLETE))
                fail("Framebuffer checked is incomplete, expected complete");
            else
                // An incomplete status is allowed, but not _this_ incomplete status.
                fail("Framebuffer checked as incomplete, but with wrong status");
        }
        else if (fboStatus == GL_FRAMEBUFFER_UNSUPPORTED)
        {
            // The spec requires
            //     "when both depth and stencil attachments are present,implementations are only required
            //      to support framebuffer objects where both attachments refer to the same image."
            //
            // Thus, it is acceptable for an implementation returning GL_FRAMEBUFFER_UNSUPPORTED,
            // and the test cannot be marked as failed.
            pass();
        }
        else if (fboStatus != GL_FRAMEBUFFER_COMPLETE && reference.isFBOStatusValid(GL_FRAMEBUFFER_COMPLETE))
            qualityWarning("Framebuffer object could have checked as complete but did not.");
        else
            pass();
    }

    return ret;
}

IterateResult TestBase::build(FboBuilder &builder)
{
    DE_UNREF(builder);
    return STOP;
}

ImageFormat TestBase::getDefaultFormat(GLenum attPoint, GLenum bufType) const
{
    if (bufType == GL_NONE)
    {
        return ImageFormat::none();
    }

    // Prefer a standard format, if there is one, but if not, use a format
    // provided by an extension.
    Formats formats            = m_ctx.getCoreFormats().getFormats(formatFlag(attPoint) | formatFlag(bufType));
    Formats::const_iterator it = formats.begin();
    if (it == formats.end())
    {
        formats = m_ctx.getCtxFormats().getFormats(formatFlag(attPoint) | formatFlag(bufType));
        it      = formats.begin();
    }
    if (it == formats.end())
        throw tcu::NotSupportedError("Unsupported attachment kind for attachment point", "", __FILE__, __LINE__);
    return *it;
}

Image *makeImage(GLenum bufType, ImageFormat format, GLsizei width, GLsizei height, FboBuilder &builder)
{
    Image *image = nullptr;
    switch (bufType)
    {
    case GL_NONE:
        return nullptr;
    case GL_RENDERBUFFER:
        image = &builder.makeConfig<Renderbuffer>();
        break;
    case GL_TEXTURE:
        image = &builder.makeConfig<Texture2D>();
        break;
    default:
        DE_FATAL("Impossible case");
    }
    image->internalFormat = format;
    image->width          = width;
    image->height         = height;
    return image;
}

Attachment *makeAttachment(GLenum bufType, ImageFormat format, GLsizei width, GLsizei height, FboBuilder &builder)
{
    Image *const imgCfg = makeImage(bufType, format, width, height, builder);
    Attachment *att     = nullptr;
    GLuint img          = 0;

    if (Renderbuffer *rboCfg = dynamic_cast<Renderbuffer *>(imgCfg))
    {
        img = builder.glCreateRbo(*rboCfg);
        att = &builder.makeConfig<RenderbufferAttachment>();
    }
    else if (Texture2D *texCfg = dynamic_cast<Texture2D *>(imgCfg))
    {
        img                           = builder.glCreateTexture(*texCfg);
        TextureFlatAttachment &texAtt = builder.makeConfig<TextureFlatAttachment>();
        texAtt.texTarget              = GL_TEXTURE_2D;
        att                           = &texAtt;
    }
    else
    {
        DE_ASSERT(imgCfg == nullptr);
        return nullptr;
    }
    att->imageName = img;
    return att;
}

void TestBase::attachTargetToNew(GLenum target, GLenum bufType, ImageFormat format, GLsizei width, GLsizei height,
                                 FboBuilder &builder)
{
    ImageFormat imgFmt = format;
    if (imgFmt.format == GL_NONE)
        imgFmt = getDefaultFormat(target, bufType);

    const Attachment *const att = makeAttachment(bufType, imgFmt, width, height, builder);
    builder.glAttach(target, att);
}

static string formatName(ImageFormat format)
{
    const string s      = getTextureFormatName(format.format);
    const string fmtStr = toLower(s.substr(3));

    if (format.unsizedType != GL_NONE)
    {
        const string typeStr = getTypeName(format.unsizedType);
        return fmtStr + "_" + toLower(typeStr.substr(3));
    }

    return fmtStr;
}

static string formatDesc(ImageFormat format)
{
    const string fmtStr = getTextureFormatName(format.format);

    if (format.unsizedType != GL_NONE)
    {
        const string typeStr = getTypeName(format.unsizedType);
        return fmtStr + " with type " + typeStr;
    }

    return fmtStr;
}

struct RenderableParams
{
    GLenum attPoint;
    GLenum bufType;
    ImageFormat format;
    static string getName(const RenderableParams &params)
    {
        return formatName(params.format);
    }
    static string getDescription(const RenderableParams &params)
    {
        return formatDesc(params.format);
    }
};

class RenderableTest : public ParamTest<RenderableParams>
{
public:
    RenderableTest(Context &group, const Params &params) : ParamTest<RenderableParams>(group, params)
    {
    }
    IterateResult build(FboBuilder &builder);
};

IterateResult RenderableTest::build(FboBuilder &builder)
{
    attachTargetToNew(m_params.attPoint, m_params.bufType, m_params.format, 64, 64, builder);
    return STOP;
}

string attTypeName(GLenum bufType)
{
    switch (bufType)
    {
    case GL_NONE:
        return "none";
    case GL_RENDERBUFFER:
        return "rbo";
    case GL_TEXTURE:
        return "tex";
    default:
        DE_FATAL("Impossible case");
    }
    return ""; // Shut up compiler
}

struct AttachmentParams
{
    GLenum color0Kind;
    GLenum colornKind;
    GLenum depthKind;
    GLenum stencilKind;

    static string getName(const AttachmentParams &params);
    static string getDescription(const AttachmentParams &params)
    {
        return getName(params);
    }
};

string AttachmentParams::getName(const AttachmentParams &params)
{
    return (attTypeName(params.color0Kind) + "_" + attTypeName(params.colornKind) + "_" +
            attTypeName(params.depthKind) + "_" + attTypeName(params.stencilKind));
}

//! Test for combinations of different kinds of attachments
class AttachmentTest : public ParamTest<AttachmentParams>
{
public:
    AttachmentTest(Context &group, Params &params) : ParamTest<AttachmentParams>(group, params)
    {
    }

protected:
    IterateResult build(FboBuilder &builder);
    void makeDepthAndStencil(FboBuilder &builder);
};

void AttachmentTest::makeDepthAndStencil(FboBuilder &builder)
{
    if (m_params.stencilKind == m_params.depthKind)
    {
        // If there is a common stencil+depth -format, try to use a common
        // image for both attachments.
        const FormatFlags flags    = DEPTH_RENDERABLE | STENCIL_RENDERABLE | formatFlag(m_params.stencilKind);
        const Formats &formats     = m_ctx.getCoreFormats().getFormats(flags);
        Formats::const_iterator it = formats.begin();
        if (it != formats.end())
        {
            const ImageFormat format = *it;
            Attachment *att          = makeAttachment(m_params.depthKind, format, 64, 64, builder);
            builder.glAttach(GL_DEPTH_ATTACHMENT, att);
            builder.glAttach(GL_STENCIL_ATTACHMENT, att);
            return;
        }
    }
    // Either the kinds were separate, or a suitable format was not found.
    // Create separate images.
    attachTargetToNew(GL_STENCIL_ATTACHMENT, m_params.stencilKind, ImageFormat::none(), 64, 64, builder);
    attachTargetToNew(GL_DEPTH_ATTACHMENT, m_params.depthKind, ImageFormat::none(), 64, 64, builder);
}

IterateResult AttachmentTest::build(FboBuilder &builder)
{
    attachTargetToNew(GL_COLOR_ATTACHMENT0, m_params.color0Kind, ImageFormat::none(), 64, 64, builder);

    if (m_params.colornKind != GL_NONE)
    {
        TCU_CHECK_AND_THROW(NotSupportedError, m_ctx.haveMultiColorAtts(), "Multiple attachments not supported");
        GLint maxAttachments = 1;
        gl(*this).getIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxAttachments);
        GLU_EXPECT_NO_ERROR(gl(*this).getError(), "Couldn't read GL_MAX_COLOR_ATTACHMENTS");

        for (int i = 1; i < maxAttachments; i++)
        {
            attachTargetToNew(GL_COLOR_ATTACHMENT0 + i, m_params.colornKind, ImageFormat::none(), 64, 64, builder);
        }
    }

    makeDepthAndStencil(builder);

    return STOP;
}

class EmptyImageTest : public TestBase
{
public:
    EmptyImageTest(Context &group, const char *name, const char *desc) : TestBase(group, name, desc)
    {
    }

    IterateResult build(FboBuilder &builder);
};

IterateResult EmptyImageTest::build(FboBuilder &builder)
{
    attachTargetToNew(GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, ImageFormat::none(), 0, 0, builder);
    return STOP;
}

class DistinctSizeTest : public TestBase
{
public:
    DistinctSizeTest(Context &group, const char *name, const char *desc) : TestBase(group, name, desc)
    {
    }

    IterateResult build(FboBuilder &builder);
};

IterateResult DistinctSizeTest::build(FboBuilder &builder)
{
    attachTargetToNew(GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, ImageFormat::none(), 64, 64, builder);
    attachTargetToNew(GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, ImageFormat::none(), 128, 128, builder);
    return STOP;
}

TestCaseGroup *Context::createRenderableTests(void)
{
    TestCaseGroup *const renderableTests =
        new TestCaseGroup(m_testCtx, "renderable", "Tests for support of renderable image formats");

    TestCaseGroup *const rbRenderableTests =
        new TestCaseGroup(m_testCtx, "renderbuffer", "Tests for renderbuffer formats");

    TestCaseGroup *const texRenderableTests = new TestCaseGroup(m_testCtx, "texture", "Tests for texture formats");

    static const struct AttPoint
    {
        GLenum attPoint;
        const char *name;
        const char *desc;
    } attPoints[] = {
        {GL_COLOR_ATTACHMENT0, "color0", "Tests for color attachments"},
        {GL_STENCIL_ATTACHMENT, "stencil", "Tests for stencil attachments"},
        {GL_DEPTH_ATTACHMENT, "depth", "Tests for depth attachments"},
    };

    // At each attachment point, iterate through all the possible formats to
    // detect both false positives and false negatives.
    const Formats rboFmts = m_allFormats.getFormats(ANY_FORMAT);
    const Formats texFmts = m_allFormats.getFormats(ANY_FORMAT);

    for (const AttPoint *it = DE_ARRAY_BEGIN(attPoints); it != DE_ARRAY_END(attPoints); it++)
    {
        TestCaseGroup *const rbAttTests  = new TestCaseGroup(m_testCtx, it->name, it->desc);
        TestCaseGroup *const texAttTests = new TestCaseGroup(m_testCtx, it->name, it->desc);

        for (Formats::const_iterator it2 = rboFmts.begin(); it2 != rboFmts.end(); it2++)
        {
            const RenderableParams params = {it->attPoint, GL_RENDERBUFFER, *it2};
            rbAttTests->addChild(new RenderableTest(*this, params));
        }
        rbRenderableTests->addChild(rbAttTests);

        for (Formats::const_iterator it2 = texFmts.begin(); it2 != texFmts.end(); it2++)
        {
            const RenderableParams params = {it->attPoint, GL_TEXTURE, *it2};
            texAttTests->addChild(new RenderableTest(*this, params));
        }
        texRenderableTests->addChild(texAttTests);
    }
    renderableTests->addChild(rbRenderableTests);
    renderableTests->addChild(texRenderableTests);

    return renderableTests;
}

TestCaseGroup *Context::createAttachmentTests(void)
{
    TestCaseGroup *const attCombTests =
        new TestCaseGroup(m_testCtx, "attachment_combinations", "Tests for attachment combinations");

    static const GLenum s_bufTypes[]   = {GL_NONE, GL_RENDERBUFFER, GL_TEXTURE};
    static const Range<GLenum> s_kinds = GLS_ARRAY_RANGE(s_bufTypes);

    for (const GLenum *col0 = s_kinds.begin(); col0 != s_kinds.end(); ++col0)
        for (const GLenum *coln = s_kinds.begin(); coln != s_kinds.end(); ++coln)
            for (const GLenum *dep = s_kinds.begin(); dep != s_kinds.end(); ++dep)
                for (const GLenum *stc = s_kinds.begin(); stc != s_kinds.end(); ++stc)
                {
                    AttachmentParams params = {*col0, *coln, *dep, *stc};
                    attCombTests->addChild(new AttachmentTest(*this, params));
                }

    return attCombTests;
}

TestCaseGroup *Context::createSizeTests(void)
{
    TestCaseGroup *const sizeTests = new TestCaseGroup(m_testCtx, "size", "Tests for attachment sizes");
    sizeTests->addChild(new EmptyImageTest(*this, "zero", "Test for zero-sized image attachment"));
    sizeTests->addChild(new DistinctSizeTest(*this, "distinct", "Test for attachments with different sizes"));

    return sizeTests;
}

} // namespace details

} // namespace fboc
} // namespace gls
} // namespace deqp
