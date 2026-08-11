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
 * \brief Texture format performance tests.
 *//*--------------------------------------------------------------------*/

#include "es3pTextureCases.hpp"
#include "glsShaderPerformanceCase.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRenderTarget.hpp"
#include "gluTexture.hpp"
#include "gluTextureUtil.hpp"
#include "gluStrUtil.hpp"

#include "deStringUtil.hpp"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

namespace deqp
{
namespace gles3
{
namespace Performance
{

using namespace gls;
using namespace glw; // GL types
using std::string;
using std::vector;
using tcu::IVec4;
using tcu::TestLog;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;

Texture2DRenderCase::Texture2DRenderCase(Context &context, const char *name, const char *description,
                                         uint32_t internalFormat, uint32_t wrapS, uint32_t wrapT, uint32_t minFilter,
                                         uint32_t magFilter, const tcu::Mat3 &coordTransform, int numTextures,
                                         bool powerOfTwo)
    : ShaderPerformanceCase(context.getTestContext(), context.getRenderContext(), name, description, CASETYPE_FRAGMENT)
    , m_internalFormat(internalFormat)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_minFilter(minFilter)
    , m_magFilter(magFilter)
    , m_coordTransform(coordTransform)
    , m_numTextures(numTextures)
    , m_powerOfTwo(powerOfTwo)
{
}

Texture2DRenderCase::~Texture2DRenderCase(void)
{
    for (vector<glu::Texture2D *>::iterator i = m_textures.begin(); i != m_textures.end(); i++)
        delete *i;
    m_textures.clear();
}

static inline int roundDownToPowerOfTwo(int val)
{
    DE_ASSERT(val >= 0);
    int l0 = deClz32(val);
    return val & ~((1 << (31 - l0)) - 1);
}

void Texture2DRenderCase::init(void)
{
    TestLog &log = m_testCtx.getLog();

    const tcu::TextureFormat texFormat   = glu::mapGLInternalFormat(m_internalFormat);
    const tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(texFormat);
    const glu::Precision samplerPrec =
        (texFormat.type == tcu::TextureFormat::FLOAT || texFormat.type == tcu::TextureFormat::UNSIGNED_INT32 ||
         texFormat.type == tcu::TextureFormat::SIGNED_INT32) ?
            glu::PRECISION_HIGHP :
            glu::PRECISION_MEDIUMP;
    const glu::DataType samplerType = glu::getSampler2DType(texFormat);
    const bool isIntUint = samplerType == glu::TYPE_INT_SAMPLER_2D || samplerType == glu::TYPE_UINT_SAMPLER_2D;

    int width  = m_renderCtx.getRenderTarget().getWidth();
    int height = m_renderCtx.getRenderTarget().getHeight();

    if (m_powerOfTwo)
    {
        width  = roundDownToPowerOfTwo(width);
        height = roundDownToPowerOfTwo(height);
    }

    bool mipmaps = m_minFilter == GL_NEAREST_MIPMAP_NEAREST || m_minFilter == GL_NEAREST_MIPMAP_LINEAR ||
                   m_minFilter == GL_LINEAR_MIPMAP_NEAREST || m_minFilter == GL_LINEAR_MIPMAP_LINEAR;

    DE_ASSERT(m_powerOfTwo || (!mipmaps && m_wrapS == GL_CLAMP_TO_EDGE && m_wrapT == GL_CLAMP_TO_EDGE));

    Vec2 p00 = (m_coordTransform * Vec3(0.0f, 0.0f, 1.0f)).swizzle(0, 1);
    Vec2 p10 = (m_coordTransform * Vec3(1.0f, 0.0f, 1.0f)).swizzle(0, 1);
    Vec2 p01 = (m_coordTransform * Vec3(0.0f, 1.0f, 1.0f)).swizzle(0, 1);
    Vec2 p11 = (m_coordTransform * Vec3(1.0f, 1.0f, 1.0f)).swizzle(0, 1);

    m_attributes.push_back(AttribSpec("a_coords", Vec4(p00.x(), p00.y(), 0.0f, 0.0f),
                                      Vec4(p10.x(), p10.y(), 0.0f, 0.0f), Vec4(p01.x(), p01.y(), 0.0f, 0.0f),
                                      Vec4(p11.x(), p11.y(), 0.0f, 0.0f)));

    log << TestLog::Message << "Size: " << width << "x" << height << TestLog::EndMessage;
    log << TestLog::Message << "Format: " << glu::getTextureFormatName(m_internalFormat) << TestLog::EndMessage;
    log << TestLog::Message << "Coords: " << p00 << ", " << p10 << ", " << p01 << ", " << p11 << TestLog::EndMessage;
    log << TestLog::Message << "Wrap: " << glu::getTextureWrapModeStr(m_wrapS) << " / "
        << glu::getTextureWrapModeStr(m_wrapT) << TestLog::EndMessage;
    log << TestLog::Message << "Filter: " << glu::getTextureFilterStr(m_minFilter) << " / "
        << glu::getTextureFilterStr(m_magFilter) << TestLog::EndMessage;
    log << TestLog::Message << "Mipmaps: " << (mipmaps ? "true" : "false") << TestLog::EndMessage;
    log << TestLog::Message << "Using additive blending." << TestLog::EndMessage;

    // Use same viewport size as texture size.
    setViewportSize(width, height);

    m_vertShaderSource = "#version 300 es\n"
                         "in highp vec4 a_position;\n"
                         "in mediump vec2 a_coords;\n"
                         "out mediump vec2 v_coords;\n"
                         "void main (void)\n"
                         "{\n"
                         "    gl_Position = a_position;\n"
                         "    v_coords = a_coords;\n"
                         "}\n";

    std::ostringstream fragSrc;
    fragSrc << "#version 300 es\n";
    fragSrc << "layout(location = 0) out mediump vec4 o_color;\n";
    fragSrc << "in mediump vec2 v_coords;\n";

    for (int texNdx = 0; texNdx < m_numTextures; texNdx++)
        fragSrc << "uniform " << glu::getPrecisionName(samplerPrec) << " " << glu::getDataTypeName(samplerType)
                << " u_sampler" << texNdx << ";\n";

    fragSrc << "void main (void)\n"
            << "{\n";

    for (int texNdx = 0; texNdx < m_numTextures; texNdx++)
    {
        if (texNdx == 0)
            fragSrc << "\t" << glu::getPrecisionName(samplerPrec) << " vec4 r = ";
        else
            fragSrc << "\tr += ";

        if (isIntUint)
            fragSrc << "vec4(";

        fragSrc << "texture(u_sampler" << texNdx << ", v_coords)";

        if (isIntUint)
            fragSrc << ")";

        fragSrc << ";\n";
    }

    fragSrc << "    o_color = r;\n"
            << "}\n";

    m_fragShaderSource = fragSrc.str();

    m_textures.reserve(m_numTextures);
    for (int texNdx = 0; texNdx < m_numTextures; texNdx++)
    {
        static const IVec4 swizzles[] = {IVec4(0, 1, 2, 3), IVec4(1, 2, 3, 0), IVec4(2, 3, 0, 1), IVec4(3, 0, 1, 2),
                                         IVec4(3, 2, 1, 0), IVec4(2, 1, 0, 3), IVec4(1, 0, 3, 2), IVec4(0, 3, 2, 1)};
        const IVec4 &sw               = swizzles[texNdx % DE_LENGTH_OF_ARRAY(swizzles)];

        glu::Texture2D *texture = new glu::Texture2D(m_renderCtx, m_internalFormat, width, height);
        m_textures.push_back(texture);

        // Fill levels.
        int numLevels = mipmaps ? texture->getRefTexture().getNumLevels() : 1;
        for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
        {
            // \todo [2013-06-02 pyry] Values are not scaled back to 0..1 range in shaders.
            texture->getRefTexture().allocLevel(levelNdx);
            tcu::fillWithComponentGradients(texture->getRefTexture().getLevel(levelNdx),
                                            fmtInfo.valueMin.swizzle(sw[0], sw[1], sw[2], sw[3]),
                                            fmtInfo.valueMax.swizzle(sw[0], sw[1], sw[2], sw[3]));
        }

        texture->upload();
    }

    ShaderPerformanceCase::init();
}

void Texture2DRenderCase::deinit(void)
{
    for (vector<glu::Texture2D *>::iterator i = m_textures.begin(); i != m_textures.end(); i++)
        delete *i;
    m_textures.clear();

    ShaderPerformanceCase::deinit();
}

void Texture2DRenderCase::setupProgram(uint32_t program)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    for (int texNdx = 0; texNdx < m_numTextures; texNdx++)
    {
        int samplerLoc = gl.getUniformLocation(program, (string("u_sampler") + de::toString(texNdx)).c_str());
        gl.uniform1i(samplerLoc, texNdx);
    }
}

void Texture2DRenderCase::setupRenderState(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    // Setup additive blending.
    gl.enable(GL_BLEND);
    gl.blendFunc(GL_ONE, GL_ONE);
    gl.blendEquation(GL_FUNC_ADD);

    // Setup textures.
    for (int texNdx = 0; texNdx < m_numTextures; texNdx++)
    {
        gl.activeTexture(GL_TEXTURE0 + texNdx);
        gl.bindTexture(GL_TEXTURE_2D, m_textures[texNdx]->getGLTexture());
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_minFilter);
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_magFilter);
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_wrapS);
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_wrapT);
    }
}

} // namespace Performance
} // namespace gles3
} // namespace deqp
