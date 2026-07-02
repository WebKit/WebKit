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

#include "es3fFboCompletenessTests.hpp"

#include "glsFboCompletenessTests.hpp"
#include "deUniquePtr.hpp"
#include <sstream>

using namespace glw;
using deqp::gls::Range;
using namespace deqp::gls::FboUtil;
using namespace deqp::gls::FboUtil::config;
namespace fboc = deqp::gls::fboc;
typedef tcu::TestCase::IterateResult IterateResult;
using std::ostringstream;
using std::string;

namespace deqp
{
namespace gles3
{
namespace Functional
{

static const FormatKey s_es3ColorRenderables[] = {
    // GLES3, 4.4.4: "An internal format is color-renderable if it is one of
    // the formats from table 3.12 noted as color-renderable..."
    GL_R8,       GL_RG8,        GL_RGB8,         GL_RGB565,  GL_RGBA4,    GL_RGB5_A1, GL_RGBA8,
    GL_RGB10_A2, GL_RGB10_A2UI, GL_SRGB8_ALPHA8, GL_R8I,     GL_R8UI,     GL_R16I,    GL_R16UI,
    GL_R32I,     GL_R32UI,      GL_RG8I,         GL_RG8UI,   GL_RG16I,    GL_RG16UI,  GL_RG32I,
    GL_RG32UI,   GL_RGBA8I,     GL_RGBA8UI,      GL_RGBA16I, GL_RGBA16UI, GL_RGBA32I, GL_RGBA32UI,
};

static const FormatKey s_es3UnsizedColorRenderables[] = {
    // "...or if it is unsized format RGBA or RGB."
    // See Table 3.3 in GLES3.
    GLS_UNSIZED_FORMATKEY(GL_RGBA, GL_UNSIGNED_BYTE),
    GLS_UNSIZED_FORMATKEY(GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4),
    GLS_UNSIZED_FORMATKEY(GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1),
    GLS_UNSIZED_FORMATKEY(GL_RGB, GL_UNSIGNED_BYTE),
    GLS_UNSIZED_FORMATKEY(GL_RGB, GL_UNSIGNED_SHORT_5_6_5),
};

static const FormatKey s_es3DepthRenderables[] = {
    // GLES3, 4.4.4: "An internal format is depth-renderable if it is one of
    // the formats from table 3.13."
    GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT32F, GL_DEPTH24_STENCIL8, GL_DEPTH32F_STENCIL8,
};

static const FormatKey s_es3StencilRboRenderables[] = {
    // GLES3, 4.4.4: "An internal format is stencil-renderable if it is
    // STENCIL_INDEX8..."
    GL_STENCIL_INDEX8,
};

static const FormatKey s_es3StencilRenderables[] = {
    // "...or one of the formats from table 3.13 whose base internal format is
    // DEPTH_STENCIL."
    GL_DEPTH24_STENCIL8,
    GL_DEPTH32F_STENCIL8,
};

static const FormatKey s_es3TextureFloatFormats[] = {
    GL_RGBA32F, GL_RGBA16F, GL_R11F_G11F_B10F, GL_RG32F, GL_RG16F, GL_R32F,
    GL_R16F,    GL_RGBA16F, GL_RGB16F,         GL_RG16F, GL_R16F,
};

static const FormatKey s_es3NotRenderableTextureFormats[] = {
    GL_R8_SNORM, GL_RG8_SNORM, GL_RGB8_SNORM, GL_RGBA8_SNORM, GL_RGB9_E5, GL_SRGB8,
    GL_RGB8I,    GL_RGB16I,    GL_RGB32I,     GL_RGB8UI,      GL_RGB16UI, GL_RGB32UI,
};

static const FormatEntry s_es3Formats[] = {
    // Renderbuffers don't support unsized formats
    {REQUIRED_RENDERABLE | COLOR_RENDERABLE | TEXTURE_VALID, GLS_ARRAY_RANGE(s_es3UnsizedColorRenderables)},
    {REQUIRED_RENDERABLE | COLOR_RENDERABLE | RENDERBUFFER_VALID | TEXTURE_VALID,
     GLS_ARRAY_RANGE(s_es3ColorRenderables)},
    {REQUIRED_RENDERABLE | DEPTH_RENDERABLE | RENDERBUFFER_VALID | TEXTURE_VALID,
     GLS_ARRAY_RANGE(s_es3DepthRenderables)},
    {REQUIRED_RENDERABLE | STENCIL_RENDERABLE | RENDERBUFFER_VALID, GLS_ARRAY_RANGE(s_es3StencilRboRenderables)},
    {REQUIRED_RENDERABLE | STENCIL_RENDERABLE | RENDERBUFFER_VALID | TEXTURE_VALID,
     GLS_ARRAY_RANGE(s_es3StencilRenderables)},
    {TEXTURE_VALID, GLS_ARRAY_RANGE(s_es3NotRenderableTextureFormats)},

    // These are not color-renderable in vanilla ES3, but we need to mark them
    // as valid for textures, since EXT_color_buffer_(half_)float brings in
    // color-renderability and only renderbuffer-validity.
    {TEXTURE_VALID, GLS_ARRAY_RANGE(s_es3TextureFloatFormats)},
};

// GL_EXT_color_buffer_float
static const FormatKey s_extColorBufferFloatFormats[] = {
    GL_RGBA32F, GL_RGBA16F, GL_R11F_G11F_B10F, GL_RG32F, GL_RG16F, GL_R32F, GL_R16F,
};

// GL_QCOM_render_shared_exponent
static const FormatKey s_qcomRenderSharedExponent[] = {
    GL_RGB9_E5,
};
// GL_OES_texture_stencil8
static const FormatKey s_extOESTextureStencil8[] = {
    GL_STENCIL_INDEX8,
};

// GL_EXT_render_snorm
static const FormatKey s_extRenderSnorm[] = {
    GL_R8_SNORM,
    GL_RG8_SNORM,
    GL_RGBA8_SNORM,
};

static const FormatExtEntry s_es3ExtFormats[] = {
    {"GL_EXT_color_buffer_float",
     // These are already texture-valid in ES3, the extension just adds RBO
     // support and makes them color-renderable.
     (uint32_t)(REQUIRED_RENDERABLE | COLOR_RENDERABLE | RENDERBUFFER_VALID),
     GLS_ARRAY_RANGE(s_extColorBufferFloatFormats)},
    {"GL_OES_texture_stencil8",
     // \note: es3 RBO tests actually cover the first two requirements
     // - kept here for completeness
     (uint32_t)(REQUIRED_RENDERABLE | STENCIL_RENDERABLE | TEXTURE_VALID), GLS_ARRAY_RANGE(s_extOESTextureStencil8)},

    {"GL_EXT_render_snorm", (uint32_t)(REQUIRED_RENDERABLE | COLOR_RENDERABLE | TEXTURE_VALID | RENDERBUFFER_VALID),
     GLS_ARRAY_RANGE(s_extRenderSnorm)},

    {"GL_QCOM_render_shared_exponent",
     // This is already texture-valid in ES3, the extension just adds RBO
     // support to RGB9_E5 and make it color-renderable.
     (uint32_t)(REQUIRED_RENDERABLE | COLOR_RENDERABLE | RENDERBUFFER_VALID | TEXTURE_VALID),
     GLS_ARRAY_RANGE(s_qcomRenderSharedExponent)},
};

class ES3Checker : public Checker
{
public:
    ES3Checker(const glu::RenderContext &ctx, const FormatDB &formats)
        : Checker(ctx, formats)
        , m_ctxInfo(glu::ContextInfo::create(ctx))
        , m_numSamples(-1)
        , m_depthStencilImage(0)
        , m_depthStencilType(GL_NONE)
    {
    }
    void check(GLenum attPoint, const Attachment &att, const Image *image);

private:
    de::UniquePtr<glu::ContextInfo> m_ctxInfo;

    //! The common number of samples of images.
    GLsizei m_numSamples;

    //! The common image for depth and stencil attachments.
    GLuint m_depthStencilImage;
    GLenum m_depthStencilType;
    ImageFormat m_depthStencilFormat;
};

void ES3Checker::check(GLenum attPoint, const Attachment &att, const Image *image)
{
    GLsizei imgSamples = imageNumSamples(*image);

    if (m_numSamples == -1)
    {
        m_numSamples = imgSamples;
    }
    else
    {
        // GLES3: "The value of RENDERBUFFER_SAMPLES is the same for all attached
        // renderbuffers and, if the attached images are a mix of renderbuffers
        // and textures, the value of RENDERBUFFER_SAMPLES is zero."
        //
        // On creating a renderbuffer: "If _samples_ is zero, then
        // RENDERBUFFER_SAMPLES is set to zero. Otherwise [...] the resulting
        // value for RENDERBUFFER_SAMPLES is guaranteed to be greater than or
        // equal to _samples_ and no more than the next larger sample count
        // supported by the implementation."

        // Either all attachments are zero-sample renderbuffers and/or
        // textures, or none of them are.
        if ((m_numSamples == 0) != (imgSamples == 0))
            addFBOStatus(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE, "Mixed multi- and single-sampled attachments");

        // If the attachments requested a different number of samples, the
        // implementation is allowed to report this as incomplete. However, it
        // is also possible that despite the different requests, the
        // implementation allocated the same number of samples to both. Hence
        // reporting the framebuffer as complete is also legal.
        if (m_numSamples != imgSamples)
            addPotentialFBOStatus(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE, "Number of samples differ");
    }

    if (attPoint == GL_DEPTH_ATTACHMENT || attPoint == GL_STENCIL_ATTACHMENT)
    {
        if (m_depthStencilImage == 0)
        {
            m_depthStencilImage  = att.imageName;
            m_depthStencilType   = attachmentType(att);
            m_depthStencilFormat = image->internalFormat;
        }
        else if (m_depthStencilImage != att.imageName || m_depthStencilType != attachmentType(att))
        {
            // "Depth and stencil attachments, if present, are the same image."
            if (!m_ctxInfo->isExtensionSupported("GL_EXT_separate_depth_stencil"))
                addFBOStatus(GL_FRAMEBUFFER_UNSUPPORTED, "Depth and stencil attachments are not the same image");

            // "The combination of internal formats of the attached images does not violate
            //  an implementation-dependent set of restrictions."
            ImageFormat depthFormat = attPoint == GL_DEPTH_ATTACHMENT ? image->internalFormat : m_depthStencilFormat;
            ImageFormat stencilFormat =
                attPoint == GL_STENCIL_ATTACHMENT ? image->internalFormat : m_depthStencilFormat;
            if (m_formats.getFormatInfo(depthFormat) & STENCIL_RENDERABLE)
                addPotentialFBOStatus(GL_FRAMEBUFFER_UNSUPPORTED,
                                      "Separate depth attachment has combined depth and stencil format");
            if (m_formats.getFormatInfo(stencilFormat) & DEPTH_RENDERABLE)
                addPotentialFBOStatus(GL_FRAMEBUFFER_UNSUPPORTED,
                                      "Separate stencil attachment has combined depth and stencil format");
        }
    }
}

struct NumLayersParams
{
    GLenum textureKind;      //< GL_TEXTURE_3D or GL_TEXTURE_2D_ARRAY
    GLsizei numLayers;       //< Number of layers in texture
    GLsizei attachmentLayer; //< Layer referenced by attachment

    static string getName(const NumLayersParams &params);
    static string getDescription(const NumLayersParams &params);
};

string NumLayersParams::getName(const NumLayersParams &params)
{
    ostringstream os;
    const string kindStr = params.textureKind == GL_TEXTURE_3D ? "3d" : "2darr";
    os << kindStr << "_" << params.numLayers << "_" << params.attachmentLayer;
    return os.str();
}

string NumLayersParams::getDescription(const NumLayersParams &params)
{
    ostringstream os;
    const string kindStr = (params.textureKind == GL_TEXTURE_3D ? "3D Texture" : "2D Array Texture");
    os << kindStr + ", " << params.numLayers << " layers, "
       << "attached layer " << params.attachmentLayer << ".";
    return os.str();
}

class NumLayersTest : public fboc::ParamTest<NumLayersParams>
{
public:
    NumLayersTest(fboc::Context &ctx, NumLayersParams param) : fboc::ParamTest<NumLayersParams>(ctx, param)
    {
    }

    IterateResult build(FboBuilder &builder);
};

IterateResult NumLayersTest::build(FboBuilder &builder)
{
    TextureLayered *texCfg = nullptr;
    const GLenum target    = GL_COLOR_ATTACHMENT0;

    switch (m_params.textureKind)
    {
    case GL_TEXTURE_3D:
        texCfg = &builder.makeConfig<Texture3D>();
        break;
    case GL_TEXTURE_2D_ARRAY:
        texCfg = &builder.makeConfig<Texture2DArray>();
        break;
    default:
        DE_FATAL("Impossible case");
    }
    texCfg->internalFormat = getDefaultFormat(target, GL_TEXTURE);
    texCfg->width          = 64;
    texCfg->height         = 64;
    texCfg->numLayers      = m_params.numLayers;
    const GLuint tex       = builder.glCreateTexture(*texCfg);

    TextureLayerAttachment *att = &builder.makeConfig<TextureLayerAttachment>();
    att->layer                  = m_params.attachmentLayer;
    att->imageName              = tex;

    builder.glAttach(target, att);

    return STOP;
}

enum
{
    SAMPLES_NONE    = -2,
    SAMPLES_TEXTURE = -1
};
struct NumSamplesParams
{
    // >= 0: renderbuffer with N samples, -1: texture, -2: no attachment
    GLsizei numSamples[3];

    static string getName(const NumSamplesParams &params);
    static string getDescription(const NumSamplesParams &params);
};

string NumSamplesParams::getName(const NumSamplesParams &params)
{
    ostringstream os;
    bool first = true;
    for (const GLsizei *ns = DE_ARRAY_BEGIN(params.numSamples); ns != DE_ARRAY_END(params.numSamples); ns++)
    {
        if (first)
            first = false;
        else
            os << "_";

        if (*ns == SAMPLES_NONE)
            os << "none";
        else if (*ns == SAMPLES_TEXTURE)
            os << "tex";
        else
            os << "rbo" << *ns;
    }
    return os.str();
}

string NumSamplesParams::getDescription(const NumSamplesParams &params)
{
    ostringstream os;
    bool first                         = true;
    static const char *const s_names[] = {"color", "depth", "stencil"};
    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_names) == DE_LENGTH_OF_ARRAY(params.numSamples));

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(s_names); i++)
    {
        GLsizei ns = params.numSamples[i];

        if (ns == SAMPLES_NONE)
            continue;

        if (first)
            first = false;
        else
            os << ", ";

        if (ns == SAMPLES_TEXTURE)
            os << "texture " << s_names[i] << " attachment";
        else
            os << ns << "-sample renderbuffer " << s_names[i] << " attachment";
    }
    return os.str();
}

class NumSamplesTest : public fboc::ParamTest<NumSamplesParams>
{
public:
    NumSamplesTest(fboc::Context &ctx, NumSamplesParams param) : fboc::ParamTest<NumSamplesParams>(ctx, param)
    {
    }

    IterateResult build(FboBuilder &builder);
};

IterateResult NumSamplesTest::build(FboBuilder &builder)
{
    static const GLenum s_targets[] = {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_DEPTH_ATTACHMENT,
    };
    // Non-integer formats for each attachment type.
    // \todo [2013-12-17 lauri] Add fixed/floating/integer metadata for formats so
    // we can pick one smartly or maybe try several.
    static const GLenum s_formats[] = {
        GL_RGBA8,
        GL_RGB565,
        GL_DEPTH_COMPONENT24,
    };
    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_targets) == DE_LENGTH_OF_ARRAY(m_params.numSamples));

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(s_targets); i++)
    {
        const GLenum target   = s_targets[i];
        const ImageFormat fmt = {s_formats[i], GL_NONE};

        const GLsizei ns = m_params.numSamples[i];
        if (ns == -2)
            continue;

        if (ns == -1)
        {
            attachTargetToNew(target, GL_TEXTURE, fmt, 64, 64, builder);
        }
        else
        {
            Renderbuffer &rboCfg  = builder.makeConfig<Renderbuffer>();
            rboCfg.internalFormat = fmt;
            rboCfg.width = rboCfg.height = 64;
            rboCfg.numSamples            = ns;

            const GLuint rbo = builder.glCreateRbo(rboCfg);
            // Implementations do not necessarily support sample sizes greater than 1.
            TCU_CHECK_AND_THROW(NotSupportedError, builder.getError() != GL_INVALID_OPERATION,
                                "Unsupported number of samples");
            RenderbufferAttachment &att = builder.makeConfig<RenderbufferAttachment>();
            att.imageName               = rbo;
            builder.glAttach(target, &att);
        }
    }

    return STOP;
}

class ES3CheckerFactory : public CheckerFactory
{
public:
    Checker *createChecker(const glu::RenderContext &ctx, const FormatDB &formats)
    {
        return new ES3Checker(ctx, formats);
    }
};

class TestGroup : public TestCaseGroup
{
public:
    TestGroup(Context &context);
    void init(void);

private:
    ES3CheckerFactory m_checkerFactory;
    fboc::Context m_fboc;
};

void TestGroup::init(void)
{
    addChild(m_fboc.createRenderableTests());
    addChild(m_fboc.createAttachmentTests());
    addChild(m_fboc.createSizeTests());

    TestCaseGroup *layerTests = new TestCaseGroup(getContext(), "layer", "Tests for layer attachments");

    static const NumLayersParams s_layersParams[] = {
        //  textureKind            numLayers    attachmentKind
        {GL_TEXTURE_2D_ARRAY, 1, 0},  {GL_TEXTURE_2D_ARRAY, 1, 3}, {GL_TEXTURE_2D_ARRAY, 4, 3},
        {GL_TEXTURE_2D_ARRAY, 4, 15}, {GL_TEXTURE_3D, 1, 0},       {GL_TEXTURE_3D, 1, 15},
        {GL_TEXTURE_3D, 4, 15},       {GL_TEXTURE_3D, 64, 15},
    };

    for (const NumLayersParams *lp = DE_ARRAY_BEGIN(s_layersParams); lp != DE_ARRAY_END(s_layersParams); ++lp)
        layerTests->addChild(new NumLayersTest(m_fboc, *lp));

    addChild(layerTests);

    TestCaseGroup *sampleTests = new TestCaseGroup(getContext(), "samples", "Tests for multisample attachments");

    static const NumSamplesParams s_samplesParams[] = {
        {{0, SAMPLES_NONE, SAMPLES_NONE}},
        {{1, SAMPLES_NONE, SAMPLES_NONE}},
        {{2, SAMPLES_NONE, SAMPLES_NONE}},
        {{0, SAMPLES_TEXTURE, SAMPLES_NONE}},
        {{1, SAMPLES_TEXTURE, SAMPLES_NONE}},
        {{2, SAMPLES_TEXTURE, SAMPLES_NONE}},
        {{2, 1, SAMPLES_NONE}},
        {{2, 2, SAMPLES_NONE}},
        {{0, 0, SAMPLES_TEXTURE}},
        {{1, 2, 0}},
        {{2, 2, 0}},
        {{1, 1, 1}},
        {{1, 2, 4}},
    };

    for (const NumSamplesParams *lp = DE_ARRAY_BEGIN(s_samplesParams); lp != DE_ARRAY_END(s_samplesParams); ++lp)
        sampleTests->addChild(new NumSamplesTest(m_fboc, *lp));

    addChild(sampleTests);
}

TestGroup::TestGroup(Context &ctx)
    : TestCaseGroup(ctx, "completeness", "Completeness tests")
    , m_checkerFactory()
    , m_fboc(ctx.getTestContext(), ctx.getRenderContext(), m_checkerFactory)
{
    const FormatEntries stdRange    = GLS_ARRAY_RANGE(s_es3Formats);
    const FormatExtEntries extRange = GLS_ARRAY_RANGE(s_es3ExtFormats);

    m_fboc.addFormats(stdRange);
    m_fboc.addExtFormats(extRange);
    m_fboc.setHaveMulticolorAtts(true); // Vanilla ES3 has multiple color attachments
}

tcu::TestCaseGroup *createFboCompletenessTests(Context &context)
{
    return new TestGroup(context);
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
