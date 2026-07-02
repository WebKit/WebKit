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
 * \brief Texture specification tests.
 *
 * \todo [pyry] Following tests are missing:
 *  - Specify mipmap incomplete texture, use without mipmaps, re-specify
 *    as complete and render.
 *  - Randomly re-specify levels to eventually reach mipmap-complete texture.
 *//*--------------------------------------------------------------------*/

#include "es2fTextureSpecificationTests.hpp"
#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "gluTextureUtil.hpp"
#include "sglrContextUtil.hpp"
#include "sglrContextWrapper.hpp"
#include "sglrGLContext.hpp"
#include "sglrReferenceContext.hpp"
#include "glsTextureTestUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

namespace deqp
{
namespace gles2
{
namespace Functional
{

using std::pair;
using std::string;
using std::vector;
using tcu::IVec4;
using tcu::TestLog;
using tcu::UVec4;
using tcu::Vec4;

tcu::TextureFormat mapGLUnsizedInternalFormat(uint32_t internalFormat)
{
    using tcu::TextureFormat;
    switch (internalFormat)
    {
    case GL_ALPHA:
        return TextureFormat(TextureFormat::A, TextureFormat::UNORM_INT8);
    case GL_LUMINANCE:
        return TextureFormat(TextureFormat::L, TextureFormat::UNORM_INT8);
    case GL_LUMINANCE_ALPHA:
        return TextureFormat(TextureFormat::LA, TextureFormat::UNORM_INT8);
    case GL_RGB:
        return TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8);
    case GL_RGBA:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8);
    default:
        throw tcu::InternalError(string("Can't map GL unsized internal format (") +
                                 tcu::toHex(internalFormat).toString() + ") to texture format");
    }
}

template <int Size>
static tcu::Vector<float, Size> randomVector(de::Random &rnd,
                                             const tcu::Vector<float, Size> &minVal = tcu::Vector<float, Size>(0.0f),
                                             const tcu::Vector<float, Size> &maxVal = tcu::Vector<float, Size>(1.0f))
{
    tcu::Vector<float, Size> res;
    for (int ndx = 0; ndx < Size; ndx++)
        res[ndx] = rnd.getFloat(minVal[ndx], maxVal[ndx]);
    return res;
}

static tcu::IVec4 getPixelFormatCompareDepth(const tcu::PixelFormat &pixelFormat, tcu::TextureFormat textureFormat)
{
    switch (textureFormat.order)
    {
    case tcu::TextureFormat::L:
    case tcu::TextureFormat::LA:
        return tcu::IVec4(pixelFormat.redBits, pixelFormat.redBits, pixelFormat.redBits, pixelFormat.alphaBits);
    default:
        return tcu::IVec4(pixelFormat.redBits, pixelFormat.greenBits, pixelFormat.blueBits, pixelFormat.alphaBits);
    }
}

static tcu::UVec4 computeCompareThreshold(const tcu::PixelFormat &pixelFormat, tcu::TextureFormat textureFormat)
{
    const IVec4 texFormatBits   = tcu::getTextureFormatBitDepth(textureFormat);
    const IVec4 pixelFormatBits = getPixelFormatCompareDepth(pixelFormat, textureFormat);
    const IVec4 accurateFmtBits = min(pixelFormatBits, texFormatBits);
    const IVec4 compareBits     = select(accurateFmtBits, IVec4(8), greaterThan(accurateFmtBits, IVec4(0))) - 1;

    return (IVec4(1) << (8 - compareBits)).asUint();
}

class GradientShader : public sglr::ShaderProgram
{
public:
    GradientShader(void)
        : ShaderProgram(sglr::pdec::ShaderProgramDeclaration()
                        << sglr::pdec::VertexAttribute("a_position", rr::GENERICVECTYPE_FLOAT)
                        << sglr::pdec::VertexAttribute("a_coord", rr::GENERICVECTYPE_FLOAT)
                        << sglr::pdec::VertexToFragmentVarying(rr::GENERICVECTYPE_FLOAT)
                        << sglr::pdec::FragmentOutput(rr::GENERICVECTYPE_FLOAT)
                        << sglr::pdec::VertexSource("attribute highp vec4 a_position;\n"
                                                    "attribute mediump vec2 a_coord;\n"
                                                    "varying mediump vec2 v_coord;\n"
                                                    "void main (void)\n"
                                                    "{\n"
                                                    "    gl_Position = a_position;\n"
                                                    "    v_coord = a_coord;\n"
                                                    "}\n")
                        << sglr::pdec::FragmentSource("varying mediump vec2 v_coord;\n"
                                                      "void main (void)\n"
                                                      "{\n"
                                                      "    mediump float x = v_coord.x;\n"
                                                      "    mediump float y = v_coord.y;\n"
                                                      "    mediump float f0 = (x + y) * 0.5;\n"
                                                      "    mediump float f1 = 0.5 + (x - y) * 0.5;\n"
                                                      "    gl_FragColor = vec4(f0, f1, 1.0-f0, 1.0-f1);\n"
                                                      "}\n"))
    {
    }

    void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets, const int numPackets) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
        {
            rr::VertexPacket &packet = *packets[packetNdx];

            packet.position   = rr::readVertexAttribFloat(inputs[0], packet.instanceNdx, packet.vertexNdx);
            packet.outputs[0] = rr::readVertexAttribFloat(inputs[1], packet.instanceNdx, packet.vertexNdx);
        }
    }

    void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                        const rr::FragmentShadingContext &context) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
            for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
            {
                const tcu::Vec4 coord = rr::readTriangleVarying<float>(packets[packetNdx], context, 0, fragNdx);
                const float x         = coord.x();
                const float y         = coord.y();
                const float f0        = (x + y) * 0.5f;
                const float f1        = 0.5f + (x - y) * 0.5f;

                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, tcu::Vec4(f0, f1, 1.0f - f0, 1.0f - f1));
            }
    }
};

class Tex2DShader : public sglr::ShaderProgram
{
public:
    Tex2DShader(void)
        : ShaderProgram(sglr::pdec::ShaderProgramDeclaration()
                        << sglr::pdec::VertexAttribute("a_position", rr::GENERICVECTYPE_FLOAT)
                        << sglr::pdec::VertexAttribute("a_coord", rr::GENERICVECTYPE_FLOAT)
                        << sglr::pdec::VertexToFragmentVarying(rr::GENERICVECTYPE_FLOAT)
                        << sglr::pdec::FragmentOutput(rr::GENERICVECTYPE_FLOAT)
                        << sglr::pdec::Uniform("u_sampler0", glu::TYPE_SAMPLER_2D)
                        << sglr::pdec::VertexSource("attribute highp vec4 a_position;\n"
                                                    "attribute mediump vec2 a_coord;\n"
                                                    "varying mediump vec2 v_coord;\n"
                                                    "void main (void)\n"
                                                    "{\n"
                                                    "    gl_Position = a_position;\n"
                                                    "    v_coord = a_coord;\n"
                                                    "}\n")
                        << sglr::pdec::FragmentSource("uniform sampler2D u_sampler0;\n"
                                                      "varying mediump vec2 v_coord;\n"
                                                      "void main (void)\n"
                                                      "{\n"
                                                      "    gl_FragColor = texture2D(u_sampler0, v_coord);\n"
                                                      "}\n"))
    {
    }

    void setUniforms(sglr::Context &ctx, uint32_t program) const
    {
        ctx.useProgram(program);
        ctx.uniform1i(ctx.getUniformLocation(program, "u_sampler0"), 0);
    }

    void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets, const int numPackets) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
        {
            rr::VertexPacket &packet = *packets[packetNdx];

            packet.position   = rr::readVertexAttribFloat(inputs[0], packet.instanceNdx, packet.vertexNdx);
            packet.outputs[0] = rr::readVertexAttribFloat(inputs[1], packet.instanceNdx, packet.vertexNdx);
        }
    }

    void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                        const rr::FragmentShadingContext &context) const
    {
        tcu::Vec2 texCoords[4];
        tcu::Vec4 colors[4];

        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
        {
            // setup tex coords
            for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
            {
                const tcu::Vec4 coord = rr::readTriangleVarying<float>(packets[packetNdx], context, 0, fragNdx);
                texCoords[fragNdx]    = tcu::Vec2(coord.x(), coord.y());
            }

            // Sample
            m_uniforms[0].sampler.tex2D->sample4(colors, texCoords);

            // Write out
            for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, colors[fragNdx]);
        }
    }
};

static const char *s_cubeSwizzles[] = {"vec3(-1, -y, +x)", "vec3(+1, -y, -x)", "vec3(+x, -1, -y)",
                                       "vec3(+x, +1, +y)", "vec3(-x, -y, -1)", "vec3(+x, -y, +1)"};

class TexCubeShader : public sglr::ShaderProgram
{
public:
    TexCubeShader(tcu::CubeFace face)
        : ShaderProgram(sglr::pdec::ShaderProgramDeclaration()
                        << sglr::pdec::VertexAttribute("a_position", rr::GENERICVECTYPE_FLOAT)
                        << sglr::pdec::VertexAttribute("a_coord", rr::GENERICVECTYPE_FLOAT)
                        << sglr::pdec::VertexToFragmentVarying(rr::GENERICVECTYPE_FLOAT)
                        << sglr::pdec::FragmentOutput(rr::GENERICVECTYPE_FLOAT)
                        << sglr::pdec::Uniform("u_sampler0", glu::TYPE_SAMPLER_CUBE)
                        << sglr::pdec::VertexSource("attribute highp vec4 a_position;\n"
                                                    "attribute mediump vec2 a_coord;\n"
                                                    "varying mediump vec2 v_coord;\n"
                                                    "void main (void)\n"
                                                    "{\n"
                                                    "    gl_Position = a_position;\n"
                                                    "    v_coord = a_coord;\n"
                                                    "}\n")
                        << sglr::pdec::FragmentSource(string("") +
                                                      "uniform samplerCube u_sampler0;\n"
                                                      "varying mediump vec2 v_coord;\n"
                                                      "void main (void)\n"
                                                      "{\n"
                                                      "    mediump float x = v_coord.x*2.0 - 1.0;\n"
                                                      "    mediump float y = v_coord.y*2.0 - 1.0;\n"
                                                      "    gl_FragColor = textureCube(u_sampler0, " +
                                                      s_cubeSwizzles[face] +
                                                      ");\n"
                                                      "}\n"))
        , m_face(face)
    {
    }

    void setUniforms(sglr::Context &ctx, uint32_t program) const
    {
        ctx.useProgram(program);
        ctx.uniform1i(ctx.getUniformLocation(program, "u_sampler0"), 0);
    }

    void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets, const int numPackets) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
        {
            rr::VertexPacket &packet = *packets[packetNdx];

            packet.position   = rr::readVertexAttribFloat(inputs[0], packet.instanceNdx, packet.vertexNdx);
            packet.outputs[0] = rr::readVertexAttribFloat(inputs[1], packet.instanceNdx, packet.vertexNdx);
        }
    }

    void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                        const rr::FragmentShadingContext &context) const
    {
        tcu::Vec3 texCoords[4];
        tcu::Vec4 colors[4];

        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
        {
            // setup tex coords
            for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
            {
                const tcu::Vec4 coord = rr::readTriangleVarying<float>(packets[packetNdx], context, 0, fragNdx);
                const float x         = coord.x() * 2.0f - 1.0f;
                const float y         = coord.y() * 2.0f - 1.0f;

                // Swizzle tex coords
                switch (m_face)
                {
                case tcu::CUBEFACE_NEGATIVE_X:
                    texCoords[fragNdx] = tcu::Vec3(-1.0f, -y, +x);
                    break;
                case tcu::CUBEFACE_POSITIVE_X:
                    texCoords[fragNdx] = tcu::Vec3(+1.0f, -y, -x);
                    break;
                case tcu::CUBEFACE_NEGATIVE_Y:
                    texCoords[fragNdx] = tcu::Vec3(+x, -1.0f, -y);
                    break;
                case tcu::CUBEFACE_POSITIVE_Y:
                    texCoords[fragNdx] = tcu::Vec3(+x, +1.0f, +y);
                    break;
                case tcu::CUBEFACE_NEGATIVE_Z:
                    texCoords[fragNdx] = tcu::Vec3(-x, -y, -1.0f);
                    break;
                case tcu::CUBEFACE_POSITIVE_Z:
                    texCoords[fragNdx] = tcu::Vec3(+x, -y, +1.0f);
                    break;
                default:
                    DE_ASSERT(false);
                }
            }

            // Sample
            m_uniforms[0].sampler.texCube->sample4(colors, texCoords);

            // Write out
            for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, colors[fragNdx]);
        }
    }

private:
    tcu::CubeFace m_face;
};

enum TextureType
{
    TEXTURETYPE_2D = 0,
    TEXTURETYPE_CUBE,

    TEXTURETYPE_LAST
};

enum Flags
{
    MIPMAPS = (1 << 0)
};

static const uint32_t s_cubeMapFaces[] = {
    GL_TEXTURE_CUBE_MAP_NEGATIVE_X, GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
};

class TextureSpecCase : public TestCase, public sglr::ContextWrapper
{
public:
    TextureSpecCase(Context &context, const char *name, const char *desc, const TextureType type,
                    const tcu::TextureFormat format, const uint32_t flags, int width, int height);
    ~TextureSpecCase(void);

    IterateResult iterate(void);

protected:
    virtual void createTexture(void) = 0;

    const TextureType m_texType;
    const tcu::TextureFormat m_texFormat;
    const uint32_t m_flags;
    const int m_width;
    const int m_height;

    bool m_half_float_oes;

private:
    TextureSpecCase(const TextureSpecCase &other);
    TextureSpecCase &operator=(const TextureSpecCase &other);

    void verifyTex2D(sglr::GLContext &gles2Context, sglr::ReferenceContext &refContext);
    void verifyTexCube(sglr::GLContext &gles2Context, sglr::ReferenceContext &refContext);

    void renderTex2D(tcu::Surface &dst, int width, int height);
    void renderTexCube(tcu::Surface &dst, int width, int height, tcu::CubeFace face);

    void readPixels(tcu::Surface &dst, int x, int y, int width, int height);

    // \todo [2012-03-27 pyry] Renderer should be extended to allow custom attributes, that would clean up this cubemap mess.
    Tex2DShader m_tex2DShader;
    TexCubeShader m_texCubeNegXShader;
    TexCubeShader m_texCubePosXShader;
    TexCubeShader m_texCubeNegYShader;
    TexCubeShader m_texCubePosYShader;
    TexCubeShader m_texCubeNegZShader;
    TexCubeShader m_texCubePosZShader;
};

TextureSpecCase::TextureSpecCase(Context &context, const char *name, const char *desc, const TextureType type,
                                 const tcu::TextureFormat format, const uint32_t flags, int width, int height)
    : TestCase(context, name, desc)
    , m_texType(type)
    , m_texFormat(format)
    , m_flags(flags)
    , m_width(width)
    , m_height(height)
    , m_texCubeNegXShader(tcu::CUBEFACE_NEGATIVE_X)
    , m_texCubePosXShader(tcu::CUBEFACE_POSITIVE_X)
    , m_texCubeNegYShader(tcu::CUBEFACE_NEGATIVE_Y)
    , m_texCubePosYShader(tcu::CUBEFACE_POSITIVE_Y)
    , m_texCubeNegZShader(tcu::CUBEFACE_NEGATIVE_Z)
    , m_texCubePosZShader(tcu::CUBEFACE_POSITIVE_Z)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    m_half_float_oes         = glu::hasExtension(gl, glu::ApiType::es(2, 0), "GL_OES_texture_half_float");
}

TextureSpecCase::~TextureSpecCase(void)
{
}

TextureSpecCase::IterateResult TextureSpecCase::iterate(void)
{
    glu::RenderContext &renderCtx         = TestCase::m_context.getRenderContext();
    const tcu::RenderTarget &renderTarget = renderCtx.getRenderTarget();
    tcu::TestLog &log                     = m_testCtx.getLog();

    DE_ASSERT(m_width <= 256 && m_height <= 256);
    if (renderTarget.getWidth() < m_width || renderTarget.getHeight() < m_height)
        throw tcu::NotSupportedError("Too small viewport", "", __FILE__, __LINE__);

    // Context size, and viewport for GLES2
    de::Random rnd(deStringHash(getName()));
    int width  = deMin32(renderTarget.getWidth(), 256);
    int height = deMin32(renderTarget.getHeight(), 256);
    int x      = rnd.getInt(0, renderTarget.getWidth() - width);
    int y      = rnd.getInt(0, renderTarget.getHeight() - height);

    // Contexts.
    sglr::GLContext gles2Context(renderCtx, log, sglr::GLCONTEXT_LOG_CALLS, tcu::IVec4(x, y, width, height));
    sglr::ReferenceContextBuffers refBuffers(tcu::PixelFormat(8, 8, 8, renderTarget.getPixelFormat().alphaBits ? 8 : 0),
                                             0 /* depth */, 0 /* stencil */, width, height);
    sglr::ReferenceContext refContext(sglr::ReferenceContextLimits(renderCtx), refBuffers.getColorbuffer(),
                                      refBuffers.getDepthbuffer(), refBuffers.getStencilbuffer());

    // Clear color buffer.
    for (int ndx = 0; ndx < 2; ndx++)
    {
        setContext(ndx ? (sglr::Context *)&refContext : (sglr::Context *)&gles2Context);
        glClearColor(0.125f, 0.25f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

    // Construct texture using both GLES2 and reference contexts.
    for (int ndx = 0; ndx < 2; ndx++)
    {
        setContext(ndx ? (sglr::Context *)&refContext : (sglr::Context *)&gles2Context);
        createTexture();
        TCU_CHECK(glGetError() == GL_NO_ERROR);
    }

    // Setup texture filtering state.
    for (int ndx = 0; ndx < 2; ndx++)
    {
        setContext(ndx ? (sglr::Context *)&refContext : (sglr::Context *)&gles2Context);

        uint32_t texTarget = m_texType == TEXTURETYPE_2D ? GL_TEXTURE_2D : GL_TEXTURE_CUBE_MAP;
        glTexParameteri(texTarget, GL_TEXTURE_MIN_FILTER, (m_flags & MIPMAPS) ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST);
        glTexParameteri(texTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(texTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(texTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // Initialize case result to pass.
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

    // Disable logging.
    gles2Context.enableLogging(0);

    // Verify results.
    switch (m_texType)
    {
    case TEXTURETYPE_2D:
        verifyTex2D(gles2Context, refContext);
        break;
    case TEXTURETYPE_CUBE:
        verifyTexCube(gles2Context, refContext);
        break;
    default:
        DE_ASSERT(false);
    }

    return STOP;
}

void TextureSpecCase::verifyTex2D(sglr::GLContext &gles2Context, sglr::ReferenceContext &refContext)
{
    int numLevels = (m_flags & MIPMAPS) ? de::max(deLog2Floor32(m_width), deLog2Floor32(m_height)) + 1 : 1;

    DE_ASSERT(m_texType == TEXTURETYPE_2D);

    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        int levelW = de::max(1, m_width >> levelNdx);
        int levelH = de::max(1, m_height >> levelNdx);
        tcu::Surface reference;
        tcu::Surface result;

        if (levelW <= 2 || levelH <= 2)
            continue; // Don't bother checking.

        // Render with GLES2
        setContext(&gles2Context);
        renderTex2D(result, levelW, levelH);

        // Render reference.
        setContext(&refContext);
        renderTex2D(reference, levelW, levelH);

        {
            tcu::UVec4 threshold = computeCompareThreshold(m_context.getRenderTarget().getPixelFormat(), m_texFormat);
            bool isOk            = tcu::intThresholdCompare(m_testCtx.getLog(), "Result", "Image comparison result",
                                                            reference.getAccess(), result.getAccess(), threshold,
                                                 levelNdx == 0 ? tcu::COMPARE_LOG_RESULT : tcu::COMPARE_LOG_ON_ERROR);

            if (!isOk)
            {
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
                break;
            }
        }
    }
}

void TextureSpecCase::verifyTexCube(sglr::GLContext &gles2Context, sglr::ReferenceContext &refContext)
{
    int numLevels = (m_flags & MIPMAPS) ? de::max(deLog2Floor32(m_width), deLog2Floor32(m_height)) + 1 : 1;

    DE_ASSERT(m_texType == TEXTURETYPE_CUBE);

    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        int levelW = de::max(1, m_width >> levelNdx);
        int levelH = de::max(1, m_height >> levelNdx);
        bool isOk  = true;

        for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
        {
            tcu::Surface reference;
            tcu::Surface result;

            if (levelW <= 2 || levelH <= 2)
                continue; // Don't bother checking.

            // Render with GLES2
            setContext(&gles2Context);
            renderTexCube(result, levelW, levelH, (tcu::CubeFace)face);

            // Render reference.
            setContext(&refContext);
            renderTexCube(reference, levelW, levelH, (tcu::CubeFace)face);

            const float threshold = 0.02f;
            isOk                  = tcu::fuzzyCompare(m_testCtx.getLog(), "Result",
                                                      (string("Image comparison result: ") + de::toString((tcu::CubeFace)face)).c_str(),
                                                      reference, result, threshold,
                                     levelNdx == 0 ? tcu::COMPARE_LOG_RESULT : tcu::COMPARE_LOG_ON_ERROR);

            if (!isOk)
            {
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
                break;
            }
        }

        if (!isOk)
            break;
    }
}

void TextureSpecCase::renderTex2D(tcu::Surface &dst, int width, int height)
{
    int targetW = getWidth();
    int targetH = getHeight();

    float w = (float)width / (float)targetW;
    float h = (float)height / (float)targetH;

    uint32_t shaderID = getCurrentContext()->createProgram(&m_tex2DShader);

    m_tex2DShader.setUniforms(*getCurrentContext(), shaderID);
    sglr::drawQuad(*getCurrentContext(), shaderID, tcu::Vec3(-1.0f, -1.0f, 0.0f),
                   tcu::Vec3(-1.0f + w * 2.0f, -1.0f + h * 2.0f, 0.0f));

    // Read pixels back.
    readPixels(dst, 0, 0, width, height);
}

void TextureSpecCase::renderTexCube(tcu::Surface &dst, int width, int height, tcu::CubeFace face)
{
    int targetW = getWidth();
    int targetH = getHeight();

    float w = (float)width / (float)targetW;
    float h = (float)height / (float)targetH;

    TexCubeShader *shaders[] = {&m_texCubeNegXShader, &m_texCubePosXShader, &m_texCubeNegYShader,
                                &m_texCubePosYShader, &m_texCubeNegZShader, &m_texCubePosZShader};

    uint32_t shaderID = getCurrentContext()->createProgram(shaders[face]);

    shaders[face]->setUniforms(*getCurrentContext(), shaderID);
    sglr::drawQuad(*getCurrentContext(), shaderID, tcu::Vec3(-1.0f, -1.0f, 0.0f),
                   tcu::Vec3(-1.0f + w * 2.0f, -1.0f + h * 2.0f, 0.0f));

    // Read pixels back.
    readPixels(dst, 0, 0, width, height);
}

void TextureSpecCase::readPixels(tcu::Surface &dst, int x, int y, int width, int height)
{
    dst.setSize(width, height);
    glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, dst.getAccess().getDataPtr());
}

// Basic TexImage2D() with 2D texture usage
class BasicTexImage2DCase : public TextureSpecCase
{
public:
    BasicTexImage2DCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t dataType,
                        uint32_t flags, int width, int height)
        : TextureSpecCase(context, name, desc, TEXTURETYPE_2D, glu::mapGLTransferFormat(format, dataType), flags, width,
                          height)
        , m_format(format)
        , m_dataType(dataType)
    {
    }

protected:
    void createTexture(void)
    {
        tcu::TextureFormat fmt = m_texFormat;
        int numLevels          = (m_flags & MIPMAPS) ? de::max(deLog2Floor32(m_width), deLog2Floor32(m_height)) + 1 : 1;
        uint32_t tex           = 0;
        tcu::TextureLevel levelData(fmt);
        de::Random rnd(deStringHash(getName()));

        if (m_dataType == GL_HALF_FLOAT_OES && !m_half_float_oes)
            throw tcu::NotSupportedError("GL_OES_texture_half_float is not supported", "", __FILE__, __LINE__);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (int ndx = 0; ndx < numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);
            Vec4 gMin  = randomVector<4>(rnd);
            Vec4 gMax  = randomVector<4>(rnd);

            levelData.setSize(levelW, levelH);
            tcu::fillWithComponentGradients(levelData.getAccess(), gMin, gMax);

            glTexImage2D(GL_TEXTURE_2D, ndx, m_format, levelW, levelH, 0, m_format, m_dataType,
                         levelData.getAccess().getDataPtr());
        }
    }

    uint32_t m_format;
    uint32_t m_dataType;
};

// Basic TexImage2D() with cubemap usage
class BasicTexImageCubeCase : public TextureSpecCase
{
public:
    BasicTexImageCubeCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t dataType,
                          uint32_t flags, int width, int height)
        : TextureSpecCase(context, name, desc, TEXTURETYPE_CUBE, glu::mapGLTransferFormat(format, dataType), flags,
                          width, height)
        , m_format(format)
        , m_dataType(dataType)
    {
    }

protected:
    void createTexture(void)
    {
        tcu::TextureFormat fmt = m_texFormat;
        int numLevels          = (m_flags & MIPMAPS) ? de::max(deLog2Floor32(m_width), deLog2Floor32(m_height)) + 1 : 1;
        uint32_t tex           = 0;
        tcu::TextureLevel levelData(fmt);
        de::Random rnd(deStringHash(getName()));

        DE_ASSERT(m_width == m_height); // Non-square cubemaps are not supported by GLES2.

        if (m_dataType == GL_HALF_FLOAT_OES && !m_half_float_oes)
            throw tcu::NotSupportedError("GL_OES_texture_half_float is not supported", "", __FILE__, __LINE__);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (int ndx = 0; ndx < numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);

            levelData.setSize(levelW, levelH);

            for (int face = 0; face < DE_LENGTH_OF_ARRAY(s_cubeMapFaces); face++)
            {
                Vec4 gMin = randomVector<4>(rnd);
                Vec4 gMax = randomVector<4>(rnd);

                tcu::fillWithComponentGradients(levelData.getAccess(), gMin, gMax);

                glTexImage2D(s_cubeMapFaces[face], ndx, m_format, levelW, levelH, 0, m_format, m_dataType,
                             levelData.getAccess().getDataPtr());
            }
        }
    }

    uint32_t m_format;
    uint32_t m_dataType;
};

// Randomized 2D texture specification using TexImage2D
class RandomOrderTexImage2DCase : public TextureSpecCase
{
public:
    RandomOrderTexImage2DCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t dataType,
                              uint32_t flags, int width, int height)
        : TextureSpecCase(context, name, desc, TEXTURETYPE_2D, glu::mapGLTransferFormat(format, dataType), flags, width,
                          height)
        , m_format(format)
        , m_dataType(dataType)
    {
    }

protected:
    void createTexture(void)
    {
        tcu::TextureFormat fmt = m_texFormat;
        int numLevels          = (m_flags & MIPMAPS) ? de::max(deLog2Floor32(m_width), deLog2Floor32(m_height)) + 1 : 1;
        uint32_t tex           = 0;
        tcu::TextureLevel levelData(fmt);
        de::Random rnd(deStringHash(getName()));

        if (m_dataType == GL_HALF_FLOAT_OES && !m_half_float_oes)
            throw tcu::NotSupportedError("GL_OES_texture_half_float is not supported", "", __FILE__, __LINE__);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        vector<int> levels(numLevels);

        for (int i = 0; i < numLevels; i++)
            levels[i] = i;
        rnd.shuffle(levels.begin(), levels.end());

        for (int ndx = 0; ndx < numLevels; ndx++)
        {
            int levelNdx = levels[ndx];
            int levelW   = de::max(1, m_width >> levelNdx);
            int levelH   = de::max(1, m_height >> levelNdx);
            Vec4 gMin    = randomVector<4>(rnd);
            Vec4 gMax    = randomVector<4>(rnd);

            levelData.setSize(levelW, levelH);
            tcu::fillWithComponentGradients(levelData.getAccess(), gMin, gMax);

            glTexImage2D(GL_TEXTURE_2D, levelNdx, m_format, levelW, levelH, 0, m_format, m_dataType,
                         levelData.getAccess().getDataPtr());
        }
    }

    uint32_t m_format;
    uint32_t m_dataType;
};

// Randomized cubemap texture specification using TexImage2D
class RandomOrderTexImageCubeCase : public TextureSpecCase
{
public:
    RandomOrderTexImageCubeCase(Context &context, const char *name, const char *desc, uint32_t format,
                                uint32_t dataType, uint32_t flags, int width, int height)
        : TextureSpecCase(context, name, desc, TEXTURETYPE_CUBE, glu::mapGLTransferFormat(format, dataType), flags,
                          width, height)
        , m_format(format)
        , m_dataType(dataType)
    {
    }

protected:
    void createTexture(void)
    {
        tcu::TextureFormat fmt = m_texFormat;
        int numLevels          = (m_flags & MIPMAPS) ? de::max(deLog2Floor32(m_width), deLog2Floor32(m_height)) + 1 : 1;
        uint32_t tex           = 0;
        tcu::TextureLevel levelData(fmt);
        de::Random rnd(deStringHash(getName()));

        DE_ASSERT(m_width == m_height); // Non-square cubemaps are not supported by GLES2.

        if (m_dataType == GL_HALF_FLOAT_OES && !m_half_float_oes)
            throw tcu::NotSupportedError("GL_OES_texture_half_float is not supported", "", __FILE__, __LINE__);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // Level-face pairs.
        vector<pair<int, tcu::CubeFace>> images(numLevels * 6);

        for (int ndx = 0; ndx < numLevels; ndx++)
            for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
                images[ndx * 6 + face] = std::make_pair(ndx, (tcu::CubeFace)face);

        rnd.shuffle(images.begin(), images.end());

        for (int ndx = 0; ndx < (int)images.size(); ndx++)
        {
            int levelNdx       = images[ndx].first;
            tcu::CubeFace face = images[ndx].second;
            int levelW         = de::max(1, m_width >> levelNdx);
            int levelH         = de::max(1, m_height >> levelNdx);
            Vec4 gMin          = randomVector<4>(rnd);
            Vec4 gMax          = randomVector<4>(rnd);

            levelData.setSize(levelW, levelH);
            tcu::fillWithComponentGradients(levelData.getAccess(), gMin, gMax);

            glTexImage2D(s_cubeMapFaces[face], levelNdx, m_format, levelW, levelH, 0, m_format, m_dataType,
                         levelData.getAccess().getDataPtr());
        }
    }

    uint32_t m_format;
    uint32_t m_dataType;
};

static inline int getRowPitch(const tcu::TextureFormat &transferFmt, int rowLen, int alignment)
{
    int basePitch = transferFmt.getPixelSize() * rowLen;
    return alignment * (basePitch / alignment + ((basePitch % alignment) ? 1 : 0));
}

// TexImage2D() unpack alignment case.
class TexImage2DAlignCase : public TextureSpecCase
{
public:
    TexImage2DAlignCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t dataType,
                        uint32_t flags, int width, int height, int alignment)
        : TextureSpecCase(context, name, desc, TEXTURETYPE_2D, glu::mapGLTransferFormat(format, dataType), flags, width,
                          height)
        , m_format(format)
        , m_dataType(dataType)
        , m_alignment(alignment)
    {
    }

protected:
    void createTexture(void)
    {
        tcu::TextureFormat fmt = m_texFormat;
        int numLevels          = (m_flags & MIPMAPS) ? de::max(deLog2Floor32(m_width), deLog2Floor32(m_height)) + 1 : 1;
        uint32_t tex           = 0;
        vector<uint8_t> data;

        if (m_dataType == GL_HALF_FLOAT_OES && !m_half_float_oes)
            throw tcu::NotSupportedError("GL_OES_texture_half_float is not supported", "", __FILE__, __LINE__);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, m_alignment);

        for (int ndx = 0; ndx < numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);
            Vec4 colorA(1.0f, 0.0f, 0.0f, 1.0f);
            Vec4 colorB(0.0f, 1.0f, 0.0f, 1.0f);
            int rowPitch = getRowPitch(fmt, levelW, m_alignment);
            int cellSize = de::max(1, de::min(levelW >> 2, levelH >> 2));

            data.resize(rowPitch * levelH);
            tcu::fillWithGrid(tcu::PixelBufferAccess(fmt, levelW, levelH, 1, rowPitch, 0, &data[0]), cellSize, colorA,
                              colorB);

            glTexImage2D(GL_TEXTURE_2D, ndx, m_format, levelW, levelH, 0, m_format, m_dataType, &data[0]);
        }
    }

    uint32_t m_format;
    uint32_t m_dataType;
    int m_alignment;
};

// TexImage2D() unpack alignment case.
class TexImageCubeAlignCase : public TextureSpecCase
{
public:
    TexImageCubeAlignCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t dataType,
                          uint32_t flags, int width, int height, int alignment)
        : TextureSpecCase(context, name, desc, TEXTURETYPE_CUBE, glu::mapGLTransferFormat(format, dataType), flags,
                          width, height)
        , m_format(format)
        , m_dataType(dataType)
        , m_alignment(alignment)
    {
    }

protected:
    void createTexture(void)
    {
        tcu::TextureFormat fmt = m_texFormat;
        int numLevels          = (m_flags & MIPMAPS) ? de::max(deLog2Floor32(m_width), deLog2Floor32(m_height)) + 1 : 1;
        uint32_t tex           = 0;
        vector<uint8_t> data;

        DE_ASSERT(m_width == m_height); // Non-square cubemaps are not supported by GLES2.

        if (m_dataType == GL_HALF_FLOAT_OES && !m_half_float_oes)
            throw tcu::NotSupportedError("GL_OES_texture_half_float is not supported", "", __FILE__, __LINE__);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, m_alignment);

        for (int ndx = 0; ndx < numLevels; ndx++)
        {
            int levelW   = de::max(1, m_width >> ndx);
            int levelH   = de::max(1, m_height >> ndx);
            int rowPitch = getRowPitch(fmt, levelW, m_alignment);
            Vec4 colorA(1.0f, 0.0f, 0.0f, 1.0f);
            Vec4 colorB(0.0f, 1.0f, 0.0f, 1.0f);
            int cellSize = de::max(1, de::min(levelW >> 2, levelH >> 2));

            data.resize(rowPitch * levelH);
            tcu::fillWithGrid(tcu::PixelBufferAccess(fmt, levelW, levelH, 1, rowPitch, 0, &data[0]), cellSize, colorA,
                              colorB);

            for (int face = 0; face < DE_LENGTH_OF_ARRAY(s_cubeMapFaces); face++)
                glTexImage2D(s_cubeMapFaces[face], ndx, m_format, levelW, levelH, 0, m_format, m_dataType, &data[0]);
        }
    }

    uint32_t m_format;
    uint32_t m_dataType;
    int m_alignment;
};

// Basic TexSubImage2D() with 2D texture usage
class BasicTexSubImage2DCase : public TextureSpecCase
{
public:
    BasicTexSubImage2DCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t dataType,
                           uint32_t flags, int width, int height)
        : TextureSpecCase(context, name, desc, TEXTURETYPE_2D, glu::mapGLTransferFormat(format, dataType), flags, width,
                          height)
        , m_format(format)
        , m_dataType(dataType)
    {
    }

protected:
    void createTexture(void)
    {
        tcu::TextureFormat fmt = m_texFormat;
        int numLevels          = (m_flags & MIPMAPS) ? de::max(deLog2Floor32(m_width), deLog2Floor32(m_height)) + 1 : 1;
        uint32_t tex           = 0;
        tcu::TextureLevel data(fmt);
        de::Random rnd(deStringHash(getName()));

        if (m_dataType == GL_HALF_FLOAT_OES && !m_half_float_oes)
            throw tcu::NotSupportedError("GL_OES_texture_half_float is not supported", "", __FILE__, __LINE__);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // First specify full texture.
        for (int ndx = 0; ndx < numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);
            Vec4 gMin  = randomVector<4>(rnd);
            Vec4 gMax  = randomVector<4>(rnd);

            data.setSize(levelW, levelH);
            tcu::fillWithComponentGradients(data.getAccess(), gMin, gMax);

            glTexImage2D(GL_TEXTURE_2D, ndx, m_format, levelW, levelH, 0, m_format, m_dataType,
                         data.getAccess().getDataPtr());
        }

        // Re-specify parts of each level.
        for (int ndx = 0; ndx < numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);

            int w = rnd.getInt(1, levelW);
            int h = rnd.getInt(1, levelH);
            int x = rnd.getInt(0, levelW - w);
            int y = rnd.getInt(0, levelH - h);

            Vec4 colorA  = randomVector<4>(rnd);
            Vec4 colorB  = randomVector<4>(rnd);
            int cellSize = rnd.getInt(2, 16);

            data.setSize(w, h);
            tcu::fillWithGrid(data.getAccess(), cellSize, colorA, colorB);

            glTexSubImage2D(GL_TEXTURE_2D, ndx, x, y, w, h, m_format, m_dataType, data.getAccess().getDataPtr());
        }
    }

    uint32_t m_format;
    uint32_t m_dataType;
};

// Basic TexSubImage2D() with cubemap usage
class BasicTexSubImageCubeCase : public TextureSpecCase
{
public:
    BasicTexSubImageCubeCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t dataType,
                             uint32_t flags, int width, int height)
        : TextureSpecCase(context, name, desc, TEXTURETYPE_CUBE, glu::mapGLTransferFormat(format, dataType), flags,
                          width, height)
        , m_format(format)
        , m_dataType(dataType)
    {
    }

protected:
    void createTexture(void)
    {
        tcu::TextureFormat fmt = m_texFormat;
        int numLevels          = (m_flags & MIPMAPS) ? de::max(deLog2Floor32(m_width), deLog2Floor32(m_height)) + 1 : 1;
        uint32_t tex           = 0;
        tcu::TextureLevel data(fmt);
        de::Random rnd(deStringHash(getName()));

        DE_ASSERT(m_width == m_height); // Non-square cubemaps are not supported by GLES2.

        if (m_dataType == GL_HALF_FLOAT_OES && !m_half_float_oes)
            throw tcu::NotSupportedError("GL_OES_texture_half_float is not supported", "", __FILE__, __LINE__);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (int ndx = 0; ndx < numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);

            data.setSize(levelW, levelH);

            for (int face = 0; face < DE_LENGTH_OF_ARRAY(s_cubeMapFaces); face++)
            {
                Vec4 gMin = randomVector<4>(rnd);
                Vec4 gMax = randomVector<4>(rnd);

                tcu::fillWithComponentGradients(data.getAccess(), gMin, gMax);

                glTexImage2D(s_cubeMapFaces[face], ndx, m_format, levelW, levelH, 0, m_format, m_dataType,
                             data.getAccess().getDataPtr());
            }
        }

        // Re-specify parts of each face and level.
        for (int ndx = 0; ndx < numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);

            for (int face = 0; face < DE_LENGTH_OF_ARRAY(s_cubeMapFaces); face++)
            {
                int w = rnd.getInt(1, levelW);
                int h = rnd.getInt(1, levelH);
                int x = rnd.getInt(0, levelW - w);
                int y = rnd.getInt(0, levelH - h);

                Vec4 colorA  = randomVector<4>(rnd);
                Vec4 colorB  = randomVector<4>(rnd);
                int cellSize = rnd.getInt(2, 16);

                data.setSize(w, h);
                tcu::fillWithGrid(data.getAccess(), cellSize, colorA, colorB);

                glTexSubImage2D(s_cubeMapFaces[face], ndx, x, y, w, h, m_format, m_dataType,
                                data.getAccess().getDataPtr());
            }
        }
    }

    uint32_t m_format;
    uint32_t m_dataType;
};

// TexSubImage2D() to texture initialized with empty data
class TexSubImage2DEmptyTexCase : public TextureSpecCase
{
public:
    TexSubImage2DEmptyTexCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t dataType,
                              uint32_t flags, int width, int height)
        : TextureSpecCase(context, name, desc, TEXTURETYPE_2D, glu::mapGLTransferFormat(format, dataType), flags, width,
                          height)
        , m_format(format)
        , m_dataType(dataType)
    {
    }

protected:
    void createTexture(void)
    {
        tcu::TextureFormat fmt = m_texFormat;
        int numLevels          = (m_flags & MIPMAPS) ? de::max(deLog2Floor32(m_width), deLog2Floor32(m_height)) + 1 : 1;
        uint32_t tex           = 0;
        tcu::TextureLevel data(fmt);
        de::Random rnd(deStringHash(getName()));

        if (m_dataType == GL_HALF_FLOAT_OES && !m_half_float_oes)
            throw tcu::NotSupportedError("GL_OES_texture_half_float is not supported", "", __FILE__, __LINE__);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // First allocate storage for each level.
        for (int ndx = 0; ndx < numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);

            glTexImage2D(GL_TEXTURE_2D, ndx, m_format, levelW, levelH, 0, m_format, m_dataType, nullptr);
        }

        // Specify pixel data to all levels using glTexSubImage2D()
        for (int ndx = 0; ndx < numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);
            Vec4 gMin  = randomVector<4>(rnd);
            Vec4 gMax  = randomVector<4>(rnd);

            data.setSize(levelW, levelH);
            tcu::fillWithComponentGradients(data.getAccess(), gMin, gMax);

            glTexSubImage2D(GL_TEXTURE_2D, ndx, 0, 0, levelW, levelH, m_format, m_dataType,
                            data.getAccess().getDataPtr());
        }
    }

    uint32_t m_format;
    uint32_t m_dataType;
};

// TexSubImage2D() to empty cubemap texture
class TexSubImageCubeEmptyTexCase : public TextureSpecCase
{
public:
    TexSubImageCubeEmptyTexCase(Context &context, const char *name, const char *desc, uint32_t format,
                                uint32_t dataType, uint32_t flags, int width, int height)
        : TextureSpecCase(context, name, desc, TEXTURETYPE_CUBE, glu::mapGLTransferFormat(format, dataType), flags,
                          width, height)
        , m_format(format)
        , m_dataType(dataType)
    {
    }

protected:
    void createTexture(void)
    {
        tcu::TextureFormat fmt = m_texFormat;
        int numLevels          = (m_flags & MIPMAPS) ? de::max(deLog2Floor32(m_width), deLog2Floor32(m_height)) + 1 : 1;
        uint32_t tex           = 0;
        tcu::TextureLevel data(fmt);
        de::Random rnd(deStringHash(getName()));

        DE_ASSERT(m_width == m_height); // Non-square cubemaps are not supported by GLES2.

        if (m_dataType == GL_HALF_FLOAT_OES && !m_half_float_oes)
            throw tcu::NotSupportedError("GL_OES_texture_half_float is not supported", "", __FILE__, __LINE__);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // Specify storage for each level.
        for (int ndx = 0; ndx < numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);

            for (int face = 0; face < DE_LENGTH_OF_ARRAY(s_cubeMapFaces); face++)
                glTexImage2D(s_cubeMapFaces[face], ndx, m_format, levelW, levelH, 0, m_format, m_dataType, nullptr);
        }

        // Specify data using glTexSubImage2D()
        for (int ndx = 0; ndx < numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);

            data.setSize(levelW, levelH);

            for (int face = 0; face < DE_LENGTH_OF_ARRAY(s_cubeMapFaces); face++)
            {
                Vec4 gMin = randomVector<4>(rnd);
                Vec4 gMax = randomVector<4>(rnd);

                tcu::fillWithComponentGradients(data.getAccess(), gMin, gMax);

                glTexSubImage2D(s_cubeMapFaces[face], ndx, 0, 0, levelW, levelH, m_format, m_dataType,
                                data.getAccess().getDataPtr());
            }
        }
    }

    uint32_t m_format;
    uint32_t m_dataType;
};

// TexSubImage2D() unpack alignment with 2D texture
class TexSubImage2DAlignCase : public TextureSpecCase
{
public:
    TexSubImage2DAlignCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t dataType,
                           int width, int height, int subX, int subY, int subW, int subH, int alignment)
        : TextureSpecCase(context, name, desc, TEXTURETYPE_2D, glu::mapGLTransferFormat(format, dataType),
                          0 /* Mipmaps are never used */, width, height)
        , m_format(format)
        , m_dataType(dataType)
        , m_subX(subX)
        , m_subY(subY)
        , m_subW(subW)
        , m_subH(subH)
        , m_alignment(alignment)
    {
    }

protected:
    void createTexture(void)
    {
        tcu::TextureFormat fmt = m_texFormat;
        uint32_t tex           = 0;
        vector<uint8_t> data;

        if (m_dataType == GL_HALF_FLOAT_OES && !m_half_float_oes)
            throw tcu::NotSupportedError("GL_OES_texture_half_float is not supported", "", __FILE__, __LINE__);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        // Specify base level.
        data.resize(fmt.getPixelSize() * m_width * m_height);
        tcu::fillWithComponentGradients(tcu::PixelBufferAccess(fmt, m_width, m_height, 1, &data[0]), Vec4(0.0f),
                                        Vec4(1.0f));

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, m_format, m_width, m_height, 0, m_format, m_dataType, &data[0]);

        // Re-specify subrectangle.
        int rowPitch = getRowPitch(fmt, m_subW, m_alignment);
        data.resize(rowPitch * m_subH);
        tcu::fillWithGrid(tcu::PixelBufferAccess(fmt, m_subW, m_subH, 1, rowPitch, 0, &data[0]), 4,
                          Vec4(1.0f, 0.0f, 0.0f, 1.0f), Vec4(0.0f, 1.0f, 0.0f, 1.0f));

        glPixelStorei(GL_UNPACK_ALIGNMENT, m_alignment);
        glTexSubImage2D(GL_TEXTURE_2D, 0, m_subX, m_subY, m_subW, m_subH, m_format, m_dataType, &data[0]);
    }

    uint32_t m_format;
    uint32_t m_dataType;
    int m_subX;
    int m_subY;
    int m_subW;
    int m_subH;
    int m_alignment;
};

// TexSubImage2D() unpack alignment with cubemap texture
class TexSubImageCubeAlignCase : public TextureSpecCase
{
public:
    TexSubImageCubeAlignCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t dataType,
                             int width, int height, int subX, int subY, int subW, int subH, int alignment)
        : TextureSpecCase(context, name, desc, TEXTURETYPE_CUBE, glu::mapGLTransferFormat(format, dataType),
                          0 /* Mipmaps are never used */, width, height)
        , m_format(format)
        , m_dataType(dataType)
        , m_subX(subX)
        , m_subY(subY)
        , m_subW(subW)
        , m_subH(subH)
        , m_alignment(alignment)
    {
    }

protected:
    void createTexture(void)
    {
        tcu::TextureFormat fmt = m_texFormat;
        uint32_t tex           = 0;
        vector<uint8_t> data;

        DE_ASSERT(m_width == m_height);

        if (m_dataType == GL_HALF_FLOAT_OES && !m_half_float_oes)
            throw tcu::NotSupportedError("GL_OES_texture_half_float is not supported", "", __FILE__, __LINE__);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

        // Specify base level.
        data.resize(fmt.getPixelSize() * m_width * m_height);
        tcu::fillWithComponentGradients(tcu::PixelBufferAccess(fmt, m_width, m_height, 1, &data[0]), Vec4(0.0f),
                                        Vec4(1.0f));

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
            glTexImage2D(s_cubeMapFaces[face], 0, m_format, m_width, m_height, 0, m_format, m_dataType, &data[0]);

        // Re-specify subrectangle.
        int rowPitch = getRowPitch(fmt, m_subW, m_alignment);
        data.resize(rowPitch * m_subH);
        tcu::fillWithGrid(tcu::PixelBufferAccess(fmt, m_subW, m_subH, 1, rowPitch, 0, &data[0]), 4,
                          Vec4(1.0f, 0.0f, 0.0f, 1.0f), Vec4(0.0f, 1.0f, 0.0f, 1.0f));

        glPixelStorei(GL_UNPACK_ALIGNMENT, m_alignment);
        for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
            glTexSubImage2D(s_cubeMapFaces[face], 0, m_subX, m_subY, m_subW, m_subH, m_format, m_dataType, &data[0]);
    }

    uint32_t m_format;
    uint32_t m_dataType;
    int m_subX;
    int m_subY;
    int m_subW;
    int m_subH;
    int m_alignment;
};

// Basic CopyTexImage2D() with 2D texture usage
class BasicCopyTexImage2DCase : public TextureSpecCase
{
public:
    BasicCopyTexImage2DCase(Context &context, const char *name, const char *desc, uint32_t internalFormat,
                            uint32_t flags, int width, int height)
        : TextureSpecCase(context, name, desc, TEXTURETYPE_2D, mapGLUnsizedInternalFormat(internalFormat), flags, width,
                          height)
        , m_internalFormat(internalFormat)
    {
    }

protected:
    void createTexture(void)
    {
        const tcu::RenderTarget &renderTarget = TestCase::m_context.getRenderContext().getRenderTarget();
        bool targetHasRGB = renderTarget.getPixelFormat().redBits > 0 && renderTarget.getPixelFormat().greenBits > 0 &&
                            renderTarget.getPixelFormat().blueBits > 0;
        bool targetHasAlpha    = renderTarget.getPixelFormat().alphaBits > 0;
        tcu::TextureFormat fmt = m_texFormat;
        bool texHasRGB         = fmt.order != tcu::TextureFormat::A;
        bool texHasAlpha       = fmt.order == tcu::TextureFormat::RGBA || fmt.order == tcu::TextureFormat::LA ||
                           fmt.order == tcu::TextureFormat::A;
        int numLevels = (m_flags & MIPMAPS) ? de::max(deLog2Floor32(m_width), deLog2Floor32(m_height)) + 1 : 1;
        uint32_t tex  = 0;
        de::Random rnd(deStringHash(getName()));
        GradientShader shader;
        uint32_t shaderID = getCurrentContext()->createProgram(&shader);

        if ((texHasRGB && !targetHasRGB) || (texHasAlpha && !targetHasAlpha))
            throw tcu::NotSupportedError("Copying from current framebuffer is not supported", "", __FILE__, __LINE__);

        // Fill render target with gradient.
        sglr::drawQuad(*getCurrentContext(), shaderID, tcu::Vec3(-1.0f, -1.0f, 0.0f), tcu::Vec3(1.0f, 1.0f, 0.0f));

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        for (int ndx = 0; ndx < numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);
            int x      = rnd.getInt(0, getWidth() - levelW);
            int y      = rnd.getInt(0, getHeight() - levelH);

            glCopyTexImage2D(GL_TEXTURE_2D, ndx, m_internalFormat, x, y, levelW, levelH, 0);
        }
    }

    uint32_t m_internalFormat;
};

// Basic CopyTexImage2D() with cubemap usage
class BasicCopyTexImageCubeCase : public TextureSpecCase
{
public:
    BasicCopyTexImageCubeCase(Context &context, const char *name, const char *desc, uint32_t internalFormat,
                              uint32_t flags, int width, int height)
        : TextureSpecCase(context, name, desc, TEXTURETYPE_CUBE, mapGLUnsizedInternalFormat(internalFormat), flags,
                          width, height)
        , m_internalFormat(internalFormat)
    {
    }

protected:
    void createTexture(void)
    {
        const tcu::RenderTarget &renderTarget = TestCase::m_context.getRenderContext().getRenderTarget();
        bool targetHasRGB = renderTarget.getPixelFormat().redBits > 0 && renderTarget.getPixelFormat().greenBits > 0 &&
                            renderTarget.getPixelFormat().blueBits > 0;
        bool targetHasAlpha    = renderTarget.getPixelFormat().alphaBits > 0;
        tcu::TextureFormat fmt = m_texFormat;
        bool texHasRGB         = fmt.order != tcu::TextureFormat::A;
        bool texHasAlpha       = fmt.order == tcu::TextureFormat::RGBA || fmt.order == tcu::TextureFormat::LA ||
                           fmt.order == tcu::TextureFormat::A;
        int numLevels = (m_flags & MIPMAPS) ? deLog2Floor32(m_width) + 1 : 1;
        uint32_t tex  = 0;
        de::Random rnd(deStringHash(getName()));
        GradientShader shader;
        uint32_t shaderID = getCurrentContext()->createProgram(&shader);

        DE_ASSERT(m_width == m_height); // Non-square cubemaps are not supported by GLES2.

        if ((texHasRGB && !targetHasRGB) || (texHasAlpha && !targetHasAlpha))
            throw tcu::NotSupportedError("Copying from current framebuffer is not supported", "", __FILE__, __LINE__);

        // Fill render target with gradient.
        sglr::drawQuad(*getCurrentContext(), shaderID, tcu::Vec3(-1.0f, -1.0f, 0.0f), tcu::Vec3(1.0f, 1.0f, 0.0f));

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

        for (int ndx = 0; ndx < numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);

            for (int face = 0; face < DE_LENGTH_OF_ARRAY(s_cubeMapFaces); face++)
            {
                int x = rnd.getInt(0, getWidth() - levelW);
                int y = rnd.getInt(0, getHeight() - levelH);

                glCopyTexImage2D(s_cubeMapFaces[face], ndx, m_internalFormat, x, y, levelW, levelH, 0);
            }
        }
    }

    uint32_t m_internalFormat;
};

// Basic CopyTexSubImage2D() with 2D texture usage
class BasicCopyTexSubImage2DCase : public TextureSpecCase
{
public:
    BasicCopyTexSubImage2DCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t dataType,
                               uint32_t flags, int width, int height)
        : TextureSpecCase(context, name, desc, TEXTURETYPE_2D, glu::mapGLTransferFormat(format, dataType), flags, width,
                          height)
        , m_format(format)
        , m_dataType(dataType)
    {
    }

protected:
    void createTexture(void)
    {
        const tcu::RenderTarget &renderTarget = TestCase::m_context.getRenderContext().getRenderTarget();
        bool targetHasRGB = renderTarget.getPixelFormat().redBits > 0 && renderTarget.getPixelFormat().greenBits > 0 &&
                            renderTarget.getPixelFormat().blueBits > 0;
        bool targetHasAlpha    = renderTarget.getPixelFormat().alphaBits > 0;
        tcu::TextureFormat fmt = m_texFormat;
        bool texHasRGB         = fmt.order != tcu::TextureFormat::A;
        bool texHasAlpha       = fmt.order == tcu::TextureFormat::RGBA || fmt.order == tcu::TextureFormat::LA ||
                           fmt.order == tcu::TextureFormat::A;
        int numLevels = (m_flags & MIPMAPS) ? de::max(deLog2Floor32(m_width), deLog2Floor32(m_height)) + 1 : 1;
        uint32_t tex  = 0;
        tcu::TextureLevel data(fmt);
        de::Random rnd(deStringHash(getName()));
        GradientShader shader;
        uint32_t shaderID = getCurrentContext()->createProgram(&shader);

        if ((texHasRGB && !targetHasRGB) || (texHasAlpha && !targetHasAlpha))
            throw tcu::NotSupportedError("Copying from current framebuffer is not supported", "", __FILE__, __LINE__);

        if (m_dataType == GL_HALF_FLOAT_OES && !m_half_float_oes)
            throw tcu::NotSupportedError("GL_OES_texture_half_float is not supported", "", __FILE__, __LINE__);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // First specify full texture.
        for (int ndx = 0; ndx < numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);

            Vec4 colorA  = randomVector<4>(rnd);
            Vec4 colorB  = randomVector<4>(rnd);
            int cellSize = rnd.getInt(2, 16);

            data.setSize(levelW, levelH);
            tcu::fillWithGrid(data.getAccess(), cellSize, colorA, colorB);

            glTexImage2D(GL_TEXTURE_2D, ndx, m_format, levelW, levelH, 0, m_format, m_dataType,
                         data.getAccess().getDataPtr());
        }

        // Fill render target with gradient.
        sglr::drawQuad(*getCurrentContext(), shaderID, tcu::Vec3(-1.0f, -1.0f, 0.0f), tcu::Vec3(1.0f, 1.0f, 0.0f));

        // Re-specify parts of each level.
        for (int ndx = 0; ndx < numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);

            int w  = rnd.getInt(1, levelW);
            int h  = rnd.getInt(1, levelH);
            int xo = rnd.getInt(0, levelW - w);
            int yo = rnd.getInt(0, levelH - h);

            int x = rnd.getInt(0, getWidth() - w);
            int y = rnd.getInt(0, getHeight() - h);

            glCopyTexSubImage2D(GL_TEXTURE_2D, ndx, xo, yo, x, y, w, h);
        }
    }

    uint32_t m_format;
    uint32_t m_dataType;
};

// Basic CopyTexSubImage2D() with cubemap usage
class BasicCopyTexSubImageCubeCase : public TextureSpecCase
{
public:
    BasicCopyTexSubImageCubeCase(Context &context, const char *name, const char *desc, uint32_t format,
                                 uint32_t dataType, uint32_t flags, int width, int height)
        : TextureSpecCase(context, name, desc, TEXTURETYPE_CUBE, glu::mapGLTransferFormat(format, dataType), flags,
                          width, height)
        , m_format(format)
        , m_dataType(dataType)
    {
    }

protected:
    void createTexture(void)
    {
        const tcu::RenderTarget &renderTarget = TestCase::m_context.getRenderContext().getRenderTarget();
        bool targetHasRGB = renderTarget.getPixelFormat().redBits > 0 && renderTarget.getPixelFormat().greenBits > 0 &&
                            renderTarget.getPixelFormat().blueBits > 0;
        bool targetHasAlpha    = renderTarget.getPixelFormat().alphaBits > 0;
        tcu::TextureFormat fmt = m_texFormat;
        bool texHasRGB         = fmt.order != tcu::TextureFormat::A;
        bool texHasAlpha       = fmt.order == tcu::TextureFormat::RGBA || fmt.order == tcu::TextureFormat::LA ||
                           fmt.order == tcu::TextureFormat::A;
        int numLevels = (m_flags & MIPMAPS) ? de::max(deLog2Floor32(m_width), deLog2Floor32(m_height)) + 1 : 1;
        uint32_t tex  = 0;
        tcu::TextureLevel data(fmt);
        de::Random rnd(deStringHash(getName()));
        GradientShader shader;
        uint32_t shaderID = getCurrentContext()->createProgram(&shader);

        DE_ASSERT(m_width == m_height); // Non-square cubemaps are not supported by GLES2.

        if ((texHasRGB && !targetHasRGB) || (texHasAlpha && !targetHasAlpha))
            throw tcu::NotSupportedError("Copying from current framebuffer is not supported", "", __FILE__, __LINE__);

        if (m_dataType == GL_HALF_FLOAT_OES && !m_half_float_oes)
            throw tcu::NotSupportedError("GL_OES_texture_half_float is not supported", "", __FILE__, __LINE__);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (int ndx = 0; ndx < numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);

            data.setSize(levelW, levelH);

            for (int face = 0; face < DE_LENGTH_OF_ARRAY(s_cubeMapFaces); face++)
            {
                Vec4 colorA  = randomVector<4>(rnd);
                Vec4 colorB  = randomVector<4>(rnd);
                int cellSize = rnd.getInt(2, 16);

                tcu::fillWithGrid(data.getAccess(), cellSize, colorA, colorB);
                glTexImage2D(s_cubeMapFaces[face], ndx, m_format, levelW, levelH, 0, m_format, m_dataType,
                             data.getAccess().getDataPtr());
            }
        }

        // Fill render target with gradient.
        sglr::drawQuad(*getCurrentContext(), shaderID, tcu::Vec3(-1.0f, -1.0f, 0.0f), tcu::Vec3(1.0f, 1.0f, 0.0f));

        // Re-specify parts of each face and level.
        for (int ndx = 0; ndx < numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);

            for (int face = 0; face < DE_LENGTH_OF_ARRAY(s_cubeMapFaces); face++)
            {
                int w  = rnd.getInt(1, levelW);
                int h  = rnd.getInt(1, levelH);
                int xo = rnd.getInt(0, levelW - w);
                int yo = rnd.getInt(0, levelH - h);

                int x = rnd.getInt(0, getWidth() - w);
                int y = rnd.getInt(0, getHeight() - h);

                glCopyTexSubImage2D(s_cubeMapFaces[face], ndx, xo, yo, x, y, w, h);
            }
        }
    }

    uint32_t m_format;
    uint32_t m_dataType;
};

TextureSpecificationTests::TextureSpecificationTests(Context &context)
    : TestCaseGroup(context, "specification", "Texture Specification Tests")
{
}

TextureSpecificationTests::~TextureSpecificationTests(void)
{
}

void TextureSpecificationTests::init(void)
{
    struct
    {
        const char *name;
        uint32_t format;
        uint32_t dataType;
    } texFormats[] = {{"a8", GL_ALPHA, GL_UNSIGNED_BYTE},
                      {"l8", GL_LUMINANCE, GL_UNSIGNED_BYTE},
                      {"la88", GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE},
                      {"rgb565", GL_RGB, GL_UNSIGNED_SHORT_5_6_5},
                      {"rgb888", GL_RGB, GL_UNSIGNED_BYTE},
                      {"rgba4444", GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4},
                      {"rgba5551", GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1},
                      {"rgba8888", GL_RGBA, GL_UNSIGNED_BYTE},
                      {"rgba16f", GL_RGBA, GL_HALF_FLOAT_OES},
                      {"rgb16f", GL_RGB, GL_HALF_FLOAT_OES},
                      {"la16f", GL_LUMINANCE_ALPHA, GL_HALF_FLOAT_OES},
                      {"l16f", GL_LUMINANCE, GL_HALF_FLOAT_OES},
                      {"a16f", GL_ALPHA, GL_HALF_FLOAT_OES}};

    // Basic TexImage2D usage.
    {
        tcu::TestCaseGroup *basicTexImageGroup =
            new tcu::TestCaseGroup(m_testCtx, "basic_teximage2d", "Basic glTexImage2D() usage");
        addChild(basicTexImageGroup);
        for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(texFormats); formatNdx++)
        {
            const char *fmtName   = texFormats[formatNdx].name;
            uint32_t format       = texFormats[formatNdx].format;
            uint32_t dataType     = texFormats[formatNdx].dataType;
            const int tex2DWidth  = 64;
            const int tex2DHeight = 128;
            const int texCubeSize = 64;

            basicTexImageGroup->addChild(new BasicTexImage2DCase(m_context, (string(fmtName) + "_2d").c_str(), "",
                                                                 format, dataType, MIPMAPS, tex2DWidth, tex2DHeight));
            basicTexImageGroup->addChild(new BasicTexImageCubeCase(m_context, (string(fmtName) + "_cube").c_str(), "",
                                                                   format, dataType, MIPMAPS, texCubeSize,
                                                                   texCubeSize));
        }
    }

    // Randomized TexImage2D order.
    {
        tcu::TestCaseGroup *randomTexImageGroup =
            new tcu::TestCaseGroup(m_testCtx, "random_teximage2d", "Randomized glTexImage2D() usage");
        addChild(randomTexImageGroup);

        de::Random rnd(9);

        // 2D cases.
        for (int ndx = 0; ndx < 10; ndx++)
        {
            int formatNdx = rnd.getInt(0, DE_LENGTH_OF_ARRAY(texFormats) - 1);
            int width     = 1 << rnd.getInt(2, 8);
            int height    = 1 << rnd.getInt(2, 8);

            randomTexImageGroup->addChild(new RandomOrderTexImage2DCase(
                m_context, (string("2d_") + de::toString(ndx)).c_str(), "", texFormats[formatNdx].format,
                texFormats[formatNdx].dataType, MIPMAPS, width, height));
        }

        // Cubemap cases.
        for (int ndx = 0; ndx < 10; ndx++)
        {
            int formatNdx = rnd.getInt(0, DE_LENGTH_OF_ARRAY(texFormats) - 1);
            int size      = 1 << rnd.getInt(2, 8);

            randomTexImageGroup->addChild(new RandomOrderTexImageCubeCase(
                m_context, (string("cube_") + de::toString(ndx)).c_str(), "", texFormats[formatNdx].format,
                texFormats[formatNdx].dataType, MIPMAPS, size, size));
        }
    }

    // TexImage2D unpack alignment.
    {
        tcu::TestCaseGroup *alignGroup =
            new tcu::TestCaseGroup(m_testCtx, "teximage2d_align", "glTexImage2D() unpack alignment tests");
        addChild(alignGroup);

        alignGroup->addChild(
            new TexImage2DAlignCase(m_context, "2d_l8_4_8", "", GL_LUMINANCE, GL_UNSIGNED_BYTE, MIPMAPS, 4, 8, 8));
        alignGroup->addChild(
            new TexImage2DAlignCase(m_context, "2d_l8_63_1", "", GL_LUMINANCE, GL_UNSIGNED_BYTE, 0, 63, 30, 1));
        alignGroup->addChild(
            new TexImage2DAlignCase(m_context, "2d_l8_63_2", "", GL_LUMINANCE, GL_UNSIGNED_BYTE, 0, 63, 30, 2));
        alignGroup->addChild(
            new TexImage2DAlignCase(m_context, "2d_l8_63_4", "", GL_LUMINANCE, GL_UNSIGNED_BYTE, 0, 63, 30, 4));
        alignGroup->addChild(
            new TexImage2DAlignCase(m_context, "2d_l8_63_8", "", GL_LUMINANCE, GL_UNSIGNED_BYTE, 0, 63, 30, 8));
        alignGroup->addChild(new TexImage2DAlignCase(m_context, "2d_rgba4444_51_1", "", GL_RGBA,
                                                     GL_UNSIGNED_SHORT_4_4_4_4, 0, 51, 30, 1));
        alignGroup->addChild(new TexImage2DAlignCase(m_context, "2d_rgba4444_51_2", "", GL_RGBA,
                                                     GL_UNSIGNED_SHORT_4_4_4_4, 0, 51, 30, 2));
        alignGroup->addChild(new TexImage2DAlignCase(m_context, "2d_rgba4444_51_4", "", GL_RGBA,
                                                     GL_UNSIGNED_SHORT_4_4_4_4, 0, 51, 30, 4));
        alignGroup->addChild(new TexImage2DAlignCase(m_context, "2d_rgba4444_51_8", "", GL_RGBA,
                                                     GL_UNSIGNED_SHORT_4_4_4_4, 0, 51, 30, 8));
        alignGroup->addChild(
            new TexImage2DAlignCase(m_context, "2d_rgb888_39_1", "", GL_RGB, GL_UNSIGNED_BYTE, 0, 39, 43, 1));
        alignGroup->addChild(
            new TexImage2DAlignCase(m_context, "2d_rgb888_39_2", "", GL_RGB, GL_UNSIGNED_BYTE, 0, 39, 43, 2));
        alignGroup->addChild(
            new TexImage2DAlignCase(m_context, "2d_rgb888_39_4", "", GL_RGB, GL_UNSIGNED_BYTE, 0, 39, 43, 4));
        alignGroup->addChild(
            new TexImage2DAlignCase(m_context, "2d_rgb888_39_8", "", GL_RGB, GL_UNSIGNED_BYTE, 0, 39, 43, 8));
        alignGroup->addChild(
            new TexImage2DAlignCase(m_context, "2d_rgba8888_47_1", "", GL_RGBA, GL_UNSIGNED_BYTE, 0, 47, 27, 1));
        alignGroup->addChild(
            new TexImage2DAlignCase(m_context, "2d_rgba8888_47_2", "", GL_RGBA, GL_UNSIGNED_BYTE, 0, 47, 27, 2));
        alignGroup->addChild(
            new TexImage2DAlignCase(m_context, "2d_rgba8888_47_4", "", GL_RGBA, GL_UNSIGNED_BYTE, 0, 47, 27, 4));
        alignGroup->addChild(
            new TexImage2DAlignCase(m_context, "2d_rgba8888_47_8", "", GL_RGBA, GL_UNSIGNED_BYTE, 0, 47, 27, 8));

        alignGroup->addChild(
            new TexImageCubeAlignCase(m_context, "cube_l8_4_8", "", GL_LUMINANCE, GL_UNSIGNED_BYTE, MIPMAPS, 4, 4, 8));
        alignGroup->addChild(
            new TexImageCubeAlignCase(m_context, "cube_l8_63_1", "", GL_LUMINANCE, GL_UNSIGNED_BYTE, 0, 63, 63, 1));
        alignGroup->addChild(
            new TexImageCubeAlignCase(m_context, "cube_l8_63_2", "", GL_LUMINANCE, GL_UNSIGNED_BYTE, 0, 63, 63, 2));
        alignGroup->addChild(
            new TexImageCubeAlignCase(m_context, "cube_l8_63_4", "", GL_LUMINANCE, GL_UNSIGNED_BYTE, 0, 63, 63, 4));
        alignGroup->addChild(
            new TexImageCubeAlignCase(m_context, "cube_l8_63_8", "", GL_LUMINANCE, GL_UNSIGNED_BYTE, 0, 63, 63, 8));
        alignGroup->addChild(new TexImageCubeAlignCase(m_context, "cube_rgba4444_51_1", "", GL_RGBA,
                                                       GL_UNSIGNED_SHORT_4_4_4_4, 0, 51, 51, 1));
        alignGroup->addChild(new TexImageCubeAlignCase(m_context, "cube_rgba4444_51_2", "", GL_RGBA,
                                                       GL_UNSIGNED_SHORT_4_4_4_4, 0, 51, 51, 2));
        alignGroup->addChild(new TexImageCubeAlignCase(m_context, "cube_rgba4444_51_4", "", GL_RGBA,
                                                       GL_UNSIGNED_SHORT_4_4_4_4, 0, 51, 51, 4));
        alignGroup->addChild(new TexImageCubeAlignCase(m_context, "cube_rgba4444_51_8", "", GL_RGBA,
                                                       GL_UNSIGNED_SHORT_4_4_4_4, 0, 51, 51, 8));
        alignGroup->addChild(
            new TexImageCubeAlignCase(m_context, "cube_rgb888_39_1", "", GL_RGB, GL_UNSIGNED_BYTE, 0, 39, 39, 1));
        alignGroup->addChild(
            new TexImageCubeAlignCase(m_context, "cube_rgb888_39_2", "", GL_RGB, GL_UNSIGNED_BYTE, 0, 39, 39, 2));
        alignGroup->addChild(
            new TexImageCubeAlignCase(m_context, "cube_rgb888_39_4", "", GL_RGB, GL_UNSIGNED_BYTE, 0, 39, 39, 4));
        alignGroup->addChild(
            new TexImageCubeAlignCase(m_context, "cube_rgb888_39_8", "", GL_RGB, GL_UNSIGNED_BYTE, 0, 39, 39, 8));
        alignGroup->addChild(
            new TexImageCubeAlignCase(m_context, "cube_rgba8888_47_1", "", GL_RGBA, GL_UNSIGNED_BYTE, 0, 47, 47, 1));
        alignGroup->addChild(
            new TexImageCubeAlignCase(m_context, "cube_rgba8888_47_2", "", GL_RGBA, GL_UNSIGNED_BYTE, 0, 47, 47, 2));
        alignGroup->addChild(
            new TexImageCubeAlignCase(m_context, "cube_rgba8888_47_4", "", GL_RGBA, GL_UNSIGNED_BYTE, 0, 47, 47, 4));
        alignGroup->addChild(
            new TexImageCubeAlignCase(m_context, "cube_rgba8888_47_8", "", GL_RGBA, GL_UNSIGNED_BYTE, 0, 47, 47, 8));
    }

    // Basic TexSubImage2D usage.
    {
        tcu::TestCaseGroup *basicTexSubImageGroup =
            new tcu::TestCaseGroup(m_testCtx, "basic_texsubimage2d", "Basic glTexSubImage2D() usage");
        addChild(basicTexSubImageGroup);
        for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(texFormats); formatNdx++)
        {
            const char *fmtName   = texFormats[formatNdx].name;
            uint32_t format       = texFormats[formatNdx].format;
            uint32_t dataType     = texFormats[formatNdx].dataType;
            const int tex2DWidth  = 64;
            const int tex2DHeight = 128;
            const int texCubeSize = 64;

            basicTexSubImageGroup->addChild(new BasicTexSubImage2DCase(
                m_context, (string(fmtName) + "_2d").c_str(), "", format, dataType, MIPMAPS, tex2DWidth, tex2DHeight));
            basicTexSubImageGroup->addChild(new BasicTexSubImageCubeCase(m_context, (string(fmtName) + "_cube").c_str(),
                                                                         "", format, dataType, MIPMAPS, texCubeSize,
                                                                         texCubeSize));
        }
    }

    // TexSubImage2D to empty texture.
    {
        tcu::TestCaseGroup *texSubImageEmptyTexGroup = new tcu::TestCaseGroup(
            m_testCtx, "texsubimage2d_empty_tex", "glTexSubImage2D() to texture that has storage but no data");
        addChild(texSubImageEmptyTexGroup);
        for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(texFormats); formatNdx++)
        {
            const char *fmtName   = texFormats[formatNdx].name;
            uint32_t format       = texFormats[formatNdx].format;
            uint32_t dataType     = texFormats[formatNdx].dataType;
            const int tex2DWidth  = 64;
            const int tex2DHeight = 32;
            const int texCubeSize = 32;

            texSubImageEmptyTexGroup->addChild(new TexSubImage2DEmptyTexCase(
                m_context, (string(fmtName) + "_2d").c_str(), "", format, dataType, MIPMAPS, tex2DWidth, tex2DHeight));
            texSubImageEmptyTexGroup->addChild(
                new TexSubImageCubeEmptyTexCase(m_context, (string(fmtName) + "_cube").c_str(), "", format, dataType,
                                                MIPMAPS, texCubeSize, texCubeSize));
        }
    }

    // TexSubImage2D alignment cases.
    {
        tcu::TestCaseGroup *alignGroup =
            new tcu::TestCaseGroup(m_testCtx, "texsubimage2d_align", "glTexSubImage2D() unpack alignment tests");
        addChild(alignGroup);

        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_l8_1_1", "", GL_LUMINANCE, GL_UNSIGNED_BYTE, 64,
                                                        64, 13, 17, 1, 6, 1));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_l8_1_2", "", GL_LUMINANCE, GL_UNSIGNED_BYTE, 64,
                                                        64, 13, 17, 1, 6, 2));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_l8_1_4", "", GL_LUMINANCE, GL_UNSIGNED_BYTE, 64,
                                                        64, 13, 17, 1, 6, 4));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_l8_1_8", "", GL_LUMINANCE, GL_UNSIGNED_BYTE, 64,
                                                        64, 13, 17, 1, 6, 8));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_l8_63_1", "", GL_LUMINANCE, GL_UNSIGNED_BYTE, 64,
                                                        64, 1, 9, 63, 30, 1));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_l8_63_2", "", GL_LUMINANCE, GL_UNSIGNED_BYTE, 64,
                                                        64, 1, 9, 63, 30, 2));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_l8_63_4", "", GL_LUMINANCE, GL_UNSIGNED_BYTE, 64,
                                                        64, 1, 9, 63, 30, 4));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_l8_63_8", "", GL_LUMINANCE, GL_UNSIGNED_BYTE, 64,
                                                        64, 1, 9, 63, 30, 8));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_rgba4444_51_1", "", GL_RGBA,
                                                        GL_UNSIGNED_SHORT_4_4_4_4, 64, 64, 7, 29, 51, 30, 1));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_rgba4444_51_2", "", GL_RGBA,
                                                        GL_UNSIGNED_SHORT_4_4_4_4, 64, 64, 7, 29, 51, 30, 2));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_rgba4444_51_4", "", GL_RGBA,
                                                        GL_UNSIGNED_SHORT_4_4_4_4, 64, 64, 7, 29, 51, 30, 4));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_rgba4444_51_8", "", GL_RGBA,
                                                        GL_UNSIGNED_SHORT_4_4_4_4, 64, 64, 7, 29, 51, 30, 8));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_rgb888_39_1", "", GL_RGB, GL_UNSIGNED_BYTE, 64,
                                                        64, 11, 8, 39, 43, 1));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_rgb888_39_2", "", GL_RGB, GL_UNSIGNED_BYTE, 64,
                                                        64, 11, 8, 39, 43, 2));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_rgb888_39_4", "", GL_RGB, GL_UNSIGNED_BYTE, 64,
                                                        64, 11, 8, 39, 43, 4));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_rgb888_39_8", "", GL_RGB, GL_UNSIGNED_BYTE, 64,
                                                        64, 11, 8, 39, 43, 8));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_rgba8888_47_1", "", GL_RGBA, GL_UNSIGNED_BYTE,
                                                        64, 64, 10, 1, 47, 27, 1));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_rgba8888_47_2", "", GL_RGBA, GL_UNSIGNED_BYTE,
                                                        64, 64, 10, 1, 47, 27, 2));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_rgba8888_47_4", "", GL_RGBA, GL_UNSIGNED_BYTE,
                                                        64, 64, 10, 1, 47, 27, 4));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_rgba8888_47_8", "", GL_RGBA, GL_UNSIGNED_BYTE,
                                                        64, 64, 10, 1, 47, 27, 8));

        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_l8_1_1", "", GL_LUMINANCE, GL_UNSIGNED_BYTE,
                                                          64, 64, 13, 17, 1, 6, 1));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_l8_1_2", "", GL_LUMINANCE, GL_UNSIGNED_BYTE,
                                                          64, 64, 13, 17, 1, 6, 2));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_l8_1_4", "", GL_LUMINANCE, GL_UNSIGNED_BYTE,
                                                          64, 64, 13, 17, 1, 6, 4));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_l8_1_8", "", GL_LUMINANCE, GL_UNSIGNED_BYTE,
                                                          64, 64, 13, 17, 1, 6, 8));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_l8_63_1", "", GL_LUMINANCE, GL_UNSIGNED_BYTE,
                                                          64, 64, 1, 9, 63, 30, 1));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_l8_63_2", "", GL_LUMINANCE, GL_UNSIGNED_BYTE,
                                                          64, 64, 1, 9, 63, 30, 2));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_l8_63_4", "", GL_LUMINANCE, GL_UNSIGNED_BYTE,
                                                          64, 64, 1, 9, 63, 30, 4));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_l8_63_8", "", GL_LUMINANCE, GL_UNSIGNED_BYTE,
                                                          64, 64, 1, 9, 63, 30, 8));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_rgba4444_51_1", "", GL_RGBA,
                                                          GL_UNSIGNED_SHORT_4_4_4_4, 64, 64, 7, 29, 51, 30, 1));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_rgba4444_51_2", "", GL_RGBA,
                                                          GL_UNSIGNED_SHORT_4_4_4_4, 64, 64, 7, 29, 51, 30, 2));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_rgba4444_51_4", "", GL_RGBA,
                                                          GL_UNSIGNED_SHORT_4_4_4_4, 64, 64, 7, 29, 51, 30, 4));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_rgba4444_51_8", "", GL_RGBA,
                                                          GL_UNSIGNED_SHORT_4_4_4_4, 64, 64, 7, 29, 51, 30, 8));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_rgb888_39_1", "", GL_RGB, GL_UNSIGNED_BYTE,
                                                          64, 64, 11, 8, 39, 43, 1));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_rgb888_39_2", "", GL_RGB, GL_UNSIGNED_BYTE,
                                                          64, 64, 11, 8, 39, 43, 2));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_rgb888_39_4", "", GL_RGB, GL_UNSIGNED_BYTE,
                                                          64, 64, 11, 8, 39, 43, 4));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_rgb888_39_8", "", GL_RGB, GL_UNSIGNED_BYTE,
                                                          64, 64, 11, 8, 39, 43, 8));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_rgba8888_47_1", "", GL_RGBA,
                                                          GL_UNSIGNED_BYTE, 64, 64, 10, 1, 47, 27, 1));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_rgba8888_47_2", "", GL_RGBA,
                                                          GL_UNSIGNED_BYTE, 64, 64, 10, 1, 47, 27, 2));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_rgba8888_47_4", "", GL_RGBA,
                                                          GL_UNSIGNED_BYTE, 64, 64, 10, 1, 47, 27, 4));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_rgba8888_47_8", "", GL_RGBA,
                                                          GL_UNSIGNED_BYTE, 64, 64, 10, 1, 47, 27, 8));
    }

    // Basic glCopyTexImage2D() cases
    {
        tcu::TestCaseGroup *copyTexImageGroup =
            new tcu::TestCaseGroup(m_testCtx, "basic_copyteximage2d", "Basic glCopyTexImage2D() usage");
        addChild(copyTexImageGroup);

        copyTexImageGroup->addChild(new BasicCopyTexImage2DCase(m_context, "2d_alpha", "", GL_ALPHA, MIPMAPS, 128, 64));
        copyTexImageGroup->addChild(
            new BasicCopyTexImage2DCase(m_context, "2d_luminance", "", GL_LUMINANCE, MIPMAPS, 128, 64));
        copyTexImageGroup->addChild(
            new BasicCopyTexImage2DCase(m_context, "2d_luminance_alpha", "", GL_LUMINANCE_ALPHA, MIPMAPS, 128, 64));
        copyTexImageGroup->addChild(new BasicCopyTexImage2DCase(m_context, "2d_rgb", "", GL_RGB, MIPMAPS, 128, 64));
        copyTexImageGroup->addChild(new BasicCopyTexImage2DCase(m_context, "2d_rgba", "", GL_RGBA, MIPMAPS, 128, 64));

        copyTexImageGroup->addChild(
            new BasicCopyTexImageCubeCase(m_context, "cube_alpha", "", GL_ALPHA, MIPMAPS, 64, 64));
        copyTexImageGroup->addChild(
            new BasicCopyTexImageCubeCase(m_context, "cube_luminance", "", GL_LUMINANCE, MIPMAPS, 64, 64));
        copyTexImageGroup->addChild(
            new BasicCopyTexImageCubeCase(m_context, "cube_luminance_alpha", "", GL_LUMINANCE_ALPHA, MIPMAPS, 64, 64));
        copyTexImageGroup->addChild(new BasicCopyTexImageCubeCase(m_context, "cube_rgb", "", GL_RGB, MIPMAPS, 64, 64));
        copyTexImageGroup->addChild(
            new BasicCopyTexImageCubeCase(m_context, "cube_rgba", "", GL_RGBA, MIPMAPS, 64, 64));
    }

    // Basic glCopyTexSubImage2D() cases
    {
        tcu::TestCaseGroup *copyTexSubImageGroup =
            new tcu::TestCaseGroup(m_testCtx, "basic_copytexsubimage2d", "Basic glCopyTexSubImage2D() usage");
        addChild(copyTexSubImageGroup);

        copyTexSubImageGroup->addChild(
            new BasicCopyTexSubImage2DCase(m_context, "2d_alpha", "", GL_ALPHA, GL_UNSIGNED_BYTE, MIPMAPS, 128, 64));
        copyTexSubImageGroup->addChild(new BasicCopyTexSubImage2DCase(m_context, "2d_luminance", "", GL_LUMINANCE,
                                                                      GL_UNSIGNED_BYTE, MIPMAPS, 128, 64));
        copyTexSubImageGroup->addChild(new BasicCopyTexSubImage2DCase(
            m_context, "2d_luminance_alpha", "", GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, MIPMAPS, 128, 64));
        copyTexSubImageGroup->addChild(
            new BasicCopyTexSubImage2DCase(m_context, "2d_rgb", "", GL_RGB, GL_UNSIGNED_BYTE, MIPMAPS, 128, 64));
        copyTexSubImageGroup->addChild(
            new BasicCopyTexSubImage2DCase(m_context, "2d_rgba", "", GL_RGBA, GL_UNSIGNED_BYTE, MIPMAPS, 128, 64));

        copyTexSubImageGroup->addChild(
            new BasicCopyTexSubImageCubeCase(m_context, "cube_alpha", "", GL_ALPHA, GL_UNSIGNED_BYTE, MIPMAPS, 64, 64));
        copyTexSubImageGroup->addChild(new BasicCopyTexSubImageCubeCase(m_context, "cube_luminance", "", GL_LUMINANCE,
                                                                        GL_UNSIGNED_BYTE, MIPMAPS, 64, 64));
        copyTexSubImageGroup->addChild(new BasicCopyTexSubImageCubeCase(
            m_context, "cube_luminance_alpha", "", GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, MIPMAPS, 64, 64));
        copyTexSubImageGroup->addChild(
            new BasicCopyTexSubImageCubeCase(m_context, "cube_rgb", "", GL_RGB, GL_UNSIGNED_BYTE, MIPMAPS, 64, 64));
        copyTexSubImageGroup->addChild(
            new BasicCopyTexSubImageCubeCase(m_context, "cube_rgba", "", GL_RGBA, GL_UNSIGNED_BYTE, MIPMAPS, 64, 64));
    }
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
