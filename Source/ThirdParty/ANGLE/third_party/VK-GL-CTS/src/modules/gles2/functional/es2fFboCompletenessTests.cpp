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

#include "es2fFboCompletenessTests.hpp"

#include "glsFboCompletenessTests.hpp"
#include "gluObjectWrapper.hpp"

using namespace glw;
using deqp::gls::Range;
using namespace deqp::gls::FboUtil;
using namespace deqp::gls::FboUtil::config;
namespace fboc = deqp::gls::fboc;
typedef tcu::TestCase::IterateResult IterateResult;

namespace deqp
{
namespace gles2
{
namespace Functional
{

static const FormatKey s_es2ColorRenderables[] = {
    GL_RGBA4,
    GL_RGB5_A1,
    GL_RGB565,
};

// GLES2 does not strictly allow these, but this seems to be a bug in the
// specification. For now, let's assume the unsized formats corresponding to
// the color-renderable sized formats are allowed.
// See https://cvs.khronos.org/bugzilla/show_bug.cgi?id=7333

static const FormatKey s_es2UnsizedColorRenderables[] = {GLS_UNSIZED_FORMATKEY(GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4),
                                                         GLS_UNSIZED_FORMATKEY(GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1),
                                                         GLS_UNSIZED_FORMATKEY(GL_RGB, GL_UNSIGNED_SHORT_5_6_5)};

static const FormatKey s_es2DepthRenderables[] = {
    GL_DEPTH_COMPONENT16,
};

static const FormatKey s_es2StencilRenderables[] = {
    GL_STENCIL_INDEX8,
};

static const FormatEntry s_es2Formats[] = {
    {COLOR_RENDERABLE | TEXTURE_VALID, GLS_ARRAY_RANGE(s_es2UnsizedColorRenderables)},
    {REQUIRED_RENDERABLE | COLOR_RENDERABLE | RENDERBUFFER_VALID, GLS_ARRAY_RANGE(s_es2ColorRenderables)},
    {REQUIRED_RENDERABLE | DEPTH_RENDERABLE | RENDERBUFFER_VALID, GLS_ARRAY_RANGE(s_es2DepthRenderables)},
    {REQUIRED_RENDERABLE | STENCIL_RENDERABLE | RENDERBUFFER_VALID, GLS_ARRAY_RANGE(s_es2StencilRenderables)},
};

// We have here only the extensions that are redundant in vanilla GLES3. Those
// that are applicable both to GLES2 and GLES3 are in glsFboCompletenessTests.cpp.

// GL_OES_texture_float
static const FormatKey s_oesTextureFloatFormats[] = {
    GLS_UNSIZED_FORMATKEY(GL_RGBA, GL_FLOAT),
    GLS_UNSIZED_FORMATKEY(GL_RGB, GL_FLOAT),
};

// GL_OES_texture_half_float
static const FormatKey s_oesTextureHalfFloatFormats[] = {
    GLS_UNSIZED_FORMATKEY(GL_RGBA, GL_HALF_FLOAT_OES),
    GLS_UNSIZED_FORMATKEY(GL_RGB, GL_HALF_FLOAT_OES),
};

// GL_EXT_color_buffer_half_float
static const FormatKey s_extColorBufferHalfFloatUnsized[] = {
    GLS_UNSIZED_FORMATKEY(GL_RGBA, GL_HALF_FLOAT_OES),
};

// GL_EXT_sRGB_write_control
static const FormatKey s_extSrgbWriteControlFormats[] = {GL_SRGB8_ALPHA8};

// DEQP_gles3_core_no_extension_features
static const FormatKey s_es3NoExtRboFormats[] = {
    GL_RGB10_A2,
    GL_SRGB8_ALPHA8,
};
static const FormatKey s_es3NoExtTextureFormats[] = {
    GL_R16F, GL_RG16F, GL_RGB16F, GL_RGBA16F, GL_R11F_G11F_B10F,
};
static const FormatKey s_es3NoExtTextureColorRenderableFormats[] = {
    GL_R8, GL_RG8, GL_RGB8, GL_RGBA4, GL_RGB5_A1, GL_RGBA8, GL_RGB10_A2, GL_RGB565, GL_SRGB8_ALPHA8,
};

// with ES3 core and GL_EXT_color_buffer_float
static const FormatKey s_es3NoExtExtColorBufferFloatFormats[] = {
    // \note Only the GLES2+exts subset of formats
    GL_R11F_G11F_B10F,
    GL_RGBA16F,
    GL_RG16F,
    GL_R16F,
};

// with ES3 core with OES_texture_stencil8
static const FormatKey s_es3NoExtOesTextureStencil8Formats[] = {
    GL_STENCIL_INDEX8,
};

// DEQP_gles3_core_changed_features
static const FormatKey s_es3NoExtDepthRenderable[] = {
    GL_DEPTH_COMPONENT16,
    GL_DEPTH_COMPONENT24,
    GL_DEPTH24_STENCIL8,
};

static const FormatKey s_es3NoExtStencilRenderable[] = {
    GL_DEPTH24_STENCIL8,
};

static const FormatExtEntry s_es2ExtFormats[] = {
    // The extension does not specify these to be color-renderable.
    {"GL_OES_texture_float", (uint32_t)TEXTURE_VALID, GLS_ARRAY_RANGE(s_oesTextureFloatFormats)},
    {"GL_OES_texture_half_float", (uint32_t)TEXTURE_VALID, GLS_ARRAY_RANGE(s_oesTextureHalfFloatFormats)},
    // However GL_EXT_color_buffer_half_float does say explicitly
    // that the RGBA variant should be renderable.
    {"GL_OES_texture_half_float GL_EXT_color_buffer_half_float", (uint32_t)(REQUIRED_RENDERABLE | COLOR_RENDERABLE),
     GLS_ARRAY_RANGE(s_extColorBufferHalfFloatUnsized)},

    // GL_EXT_sRGB_write_control makes SRGB8_ALPHA8 color-renderable
    {"GL_EXT_sRGB_write_control",
     (uint32_t)(REQUIRED_RENDERABLE | TEXTURE_VALID | COLOR_RENDERABLE | RENDERBUFFER_VALID),
     GLS_ARRAY_RANGE(s_extSrgbWriteControlFormats)},

    // Since GLES3 is "backwards compatible" to GLES2, we might actually be running on a GLES3
    // context. Since GLES3 added some features to core with no corresponding GLES2 extension,
    // some tests might produce wrong results (since they are using rules of GLES2 & extensions)
    //
    // To avoid this, require new features of GLES3 that have no matching GLES2 extension if
    // context is GLES3. This can be done with a DEQP_* extensions.
    //
    // \note Not all feature changes are listed here but only those that alter GLES2 subset of
    //       the formats
    {"DEQP_gles3_core_compatible", (uint32_t)(REQUIRED_RENDERABLE | COLOR_RENDERABLE | RENDERBUFFER_VALID),
     GLS_ARRAY_RANGE(s_es3NoExtRboFormats)},
    {"DEQP_gles3_core_compatible", (uint32_t)TEXTURE_VALID, GLS_ARRAY_RANGE(s_es3NoExtTextureFormats)},
    {"DEQP_gles3_core_compatible",
     (uint32_t)(REQUIRED_RENDERABLE | TEXTURE_VALID | COLOR_RENDERABLE | RENDERBUFFER_VALID),
     GLS_ARRAY_RANGE(s_es3NoExtTextureColorRenderableFormats)},
    {"DEQP_gles3_core_compatible GL_EXT_color_buffer_float",
     (uint32_t)(REQUIRED_RENDERABLE | COLOR_RENDERABLE | RENDERBUFFER_VALID),
     GLS_ARRAY_RANGE(s_es3NoExtExtColorBufferFloatFormats)},
    {"DEQP_gles3_core_compatible GL_OES_texture_stencil8",
     (uint32_t)(REQUIRED_RENDERABLE | STENCIL_RENDERABLE | TEXTURE_VALID),
     GLS_ARRAY_RANGE(s_es3NoExtOesTextureStencil8Formats)},
    {"DEQP_gles3_core_compatible GL_OES_texture_half_float GL_EXT_color_buffer_half_float",
     (uint32_t)(REQUIRED_RENDERABLE | COLOR_RENDERABLE), GLS_ARRAY_RANGE(s_oesTextureHalfFloatFormats)},
    {"DEQP_gles3_core_compatible",
     (uint32_t)(REQUIRED_RENDERABLE | DEPTH_RENDERABLE | RENDERBUFFER_VALID | TEXTURE_VALID),
     GLS_ARRAY_RANGE(s_es3NoExtDepthRenderable)},
    {"DEQP_gles3_core_compatible",
     (uint32_t)(REQUIRED_RENDERABLE | STENCIL_RENDERABLE | RENDERBUFFER_VALID | TEXTURE_VALID),
     GLS_ARRAY_RANGE(s_es3NoExtStencilRenderable)},
};

class ES2Checker : public Checker
{
public:
    ES2Checker(const glu::RenderContext &ctx, const FormatDB &formats);
    void check(GLenum attPoint, const Attachment &att, const Image *image);

private:
    GLsizei m_width;  //< The common width of images
    GLsizei m_height; //< The common height of images
};

ES2Checker::ES2Checker(const glu::RenderContext &ctx, const FormatDB &formats)
    : Checker(ctx, formats)
    , m_width(-1)
    , m_height(-1)
{
}

void ES2Checker::check(GLenum attPoint, const Attachment &att, const Image *image)
{
    DE_UNREF(attPoint);
    DE_UNREF(att);
    // GLES2: "All attached images have the same width and height."
    if (m_width == -1)
    {
        m_width  = image->width;
        m_height = image->height;
    }
    else if (image->width != m_width || image->height != m_height)
    {
        // Since GLES3 is "backwards compatible" to GLES2, we might actually be running
        // on a GLES3 context. On GLES3, FRAMEBUFFER_INCOMPLETE_DIMENSIONS is not generated
        // if attachments have different sizes.
        if (!gls::FboUtil::checkExtensionSupport(m_renderCtx, "DEQP_gles3_core_compatible"))
        {
            // running on GLES2
            addFBOStatus(GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS, "Sizes of attachments differ");
        }
    }
    // GLES2, 4.4.5: "some implementations may not support rendering to
    // particular combinations of internal formats. If the combination of
    // formats of the images attached to a framebuffer object are not
    // supported by the implementation, then the framebuffer is not complete
    // under the clause labeled FRAMEBUFFER_UNSUPPORTED."
    //
    // Hence it is _always_ allowed to report FRAMEBUFFER_UNSUPPORTED.
    addPotentialFBOStatus(GL_FRAMEBUFFER_UNSUPPORTED, "Particular format combinations need not to be supported");
}

struct FormatCombination
{
    GLenum colorKind;
    ImageFormat colorFmt;
    GLenum depthKind;
    ImageFormat depthFmt;
    GLenum stencilKind;
    ImageFormat stencilFmt;
};

class SupportedCombinationTest : public fboc::TestBase
{
public:
    SupportedCombinationTest(fboc::Context &ctx, const char *name, const char *desc) : TestBase(ctx, name, desc)
    {
    }

    IterateResult iterate(void);
    bool tryCombination(const FormatCombination &comb);
    GLenum formatKind(ImageFormat fmt);
};

bool SupportedCombinationTest::tryCombination(const FormatCombination &comb)
{
    glu::Framebuffer fbo(m_ctx.getRenderContext());
    FboBuilder builder(*fbo, GL_FRAMEBUFFER, fboc::gl(*this));

    attachTargetToNew(GL_COLOR_ATTACHMENT0, comb.colorKind, comb.colorFmt, 64, 64, builder);
    attachTargetToNew(GL_DEPTH_ATTACHMENT, comb.depthKind, comb.depthFmt, 64, 64, builder);
    attachTargetToNew(GL_STENCIL_ATTACHMENT, comb.stencilKind, comb.stencilFmt, 64, 64, builder);

    const GLenum glStatus = fboc::gl(*this).checkFramebufferStatus(GL_FRAMEBUFFER);

    return (glStatus == GL_FRAMEBUFFER_COMPLETE);
}

GLenum SupportedCombinationTest::formatKind(ImageFormat fmt)
{
    if (fmt.format == GL_NONE)
        return GL_NONE;

    const FormatFlags flags = m_ctx.getCoreFormats().getFormatInfo(fmt);
    const bool rbo          = (flags & RENDERBUFFER_VALID) != 0;
    // exactly one of renderbuffer and texture is supported by vanilla GLES2 formats
    DE_ASSERT(rbo != ((flags & TEXTURE_VALID) != 0));

    return rbo ? GL_RENDERBUFFER : GL_TEXTURE;
}

IterateResult SupportedCombinationTest::iterate(void)
{
    const FormatDB &db     = m_ctx.getCoreFormats();
    const ImageFormat none = ImageFormat::none();
    Formats colorFmts      = db.getFormats(COLOR_RENDERABLE);
    Formats depthFmts      = db.getFormats(DEPTH_RENDERABLE);
    Formats stencilFmts    = db.getFormats(STENCIL_RENDERABLE);
    FormatCombination comb;
    bool succ = false;

    colorFmts.insert(none);
    depthFmts.insert(none);
    stencilFmts.insert(none);

    for (Formats::const_iterator col = colorFmts.begin(); col != colorFmts.end(); col++)
    {
        comb.colorFmt  = *col;
        comb.colorKind = formatKind(*col);
        for (Formats::const_iterator dep = depthFmts.begin(); dep != depthFmts.end(); dep++)
        {
            comb.depthFmt  = *dep;
            comb.depthKind = formatKind(*dep);
            for (Formats::const_iterator stc = stencilFmts.begin(); stc != stencilFmts.end(); stc++)
            {
                comb.stencilFmt  = *stc;
                comb.stencilKind = formatKind(*stc);
                succ             = tryCombination(comb);
                if (succ)
                    break;
            }
        }
    }

    if (succ)
        pass();
    else
        fail("No supported format combination found");

    return STOP;
}

class ES2CheckerFactory : public CheckerFactory
{
public:
    Checker *createChecker(const glu::RenderContext &ctx, const FormatDB &formats)
    {
        return new ES2Checker(ctx, formats);
    }
};

class TestGroup : public TestCaseGroup
{
public:
    TestGroup(Context &ctx);
    void init(void);

private:
    ES2CheckerFactory m_checkerFactory;
    fboc::Context m_fboc;
};

TestGroup::TestGroup(Context &ctx)
    : TestCaseGroup(ctx, "completeness", "Completeness tests")
    , m_checkerFactory()
    , m_fboc(ctx.getTestContext(), ctx.getRenderContext(), m_checkerFactory)
{
    const FormatEntries stdRange    = GLS_ARRAY_RANGE(s_es2Formats);
    const FormatExtEntries extRange = GLS_ARRAY_RANGE(s_es2ExtFormats);

    m_fboc.addFormats(stdRange);
    m_fboc.addExtFormats(extRange);
    m_fboc.setHaveMulticolorAtts(ctx.getContextInfo().isExtensionSupported("GL_NV_fbo_color_attachments"));
}

void TestGroup::init(void)
{
    tcu::TestCaseGroup *attCombTests = m_fboc.createAttachmentTests();
    addChild(m_fboc.createRenderableTests());
    attCombTests->addChild(new SupportedCombinationTest(m_fboc, "exists_supported",
                                                        "Test for existence of a supported combination of formats"));
    addChild(attCombTests);
    addChild(m_fboc.createSizeTests());
}

tcu::TestCaseGroup *createFboCompletenessTests(Context &context)
{
    return new TestGroup(context);
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
