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

#include "es3pTextureFormatTests.hpp"
#include "es3pTextureCases.hpp"
#include "gluStrUtil.hpp"

#include "glwEnums.hpp"

using std::string;

namespace deqp
{
namespace gles3
{
namespace Performance
{

TextureFormatTests::TextureFormatTests(Context &context)
    : TestCaseGroup(context, "format", "Texture Format Performance Tests")
{
}

TextureFormatTests::~TextureFormatTests(void)
{
}

void TextureFormatTests::init(void)
{
    struct
    {
        const char *name;
        uint32_t internalFormat;
    } texFormats[] = {{
                          "rgba32f",
                          GL_RGBA32F,
                      },
                      {
                          "rgba32i",
                          GL_RGBA32I,
                      },
                      {
                          "rgba32ui",
                          GL_RGBA32UI,
                      },
                      {
                          "rgba16f",
                          GL_RGBA16F,
                      },
                      {
                          "rgba16i",
                          GL_RGBA16I,
                      },
                      {
                          "rgba16ui",
                          GL_RGBA16UI,
                      },
                      {
                          "rgba8",
                          GL_RGBA8,
                      },
                      {
                          "rgba8i",
                          GL_RGBA8I,
                      },
                      {
                          "rgba8ui",
                          GL_RGBA8UI,
                      },
                      {
                          "srgb8_alpha8",
                          GL_SRGB8_ALPHA8,
                      },
                      {
                          "rgb10_a2",
                          GL_RGB10_A2,
                      },
                      {
                          "rgb10_a2ui",
                          GL_RGB10_A2UI,
                      },
                      {
                          "rgba4",
                          GL_RGBA4,
                      },
                      {
                          "rgb5_a1",
                          GL_RGB5_A1,
                      },
                      {
                          "rgba8_snorm",
                          GL_RGBA8_SNORM,
                      },
                      {
                          "rgb8",
                          GL_RGB8,
                      },
                      {
                          "rgb565",
                          GL_RGB565,
                      },
                      {
                          "r11f_g11f_b10f",
                          GL_R11F_G11F_B10F,
                      },
                      {
                          "rgb32f",
                          GL_RGB32F,
                      },
                      {
                          "rgb32i",
                          GL_RGB32I,
                      },
                      {
                          "rgb32ui",
                          GL_RGB32UI,
                      },
                      {
                          "rgb16f",
                          GL_RGB16F,
                      },
                      {
                          "rgb16i",
                          GL_RGB16I,
                      },
                      {
                          "rgb16ui",
                          GL_RGB16UI,
                      },
                      {
                          "rgb8_snorm",
                          GL_RGB8_SNORM,
                      },
                      {
                          "rgb8i",
                          GL_RGB8I,
                      },
                      {
                          "rgb8ui",
                          GL_RGB8UI,
                      },
                      {
                          "srgb8",
                          GL_SRGB8,
                      },
                      {
                          "rgb9_e5",
                          GL_RGB9_E5,
                      },
                      {
                          "rg32f",
                          GL_RG32F,
                      },
                      {
                          "rg32i",
                          GL_RG32I,
                      },
                      {
                          "rg32ui",
                          GL_RG32UI,
                      },
                      {
                          "rg16f",
                          GL_RG16F,
                      },
                      {
                          "rg16i",
                          GL_RG16I,
                      },
                      {
                          "rg16ui",
                          GL_RG16UI,
                      },
                      {
                          "rg8",
                          GL_RG8,
                      },
                      {
                          "rg8i",
                          GL_RG8I,
                      },
                      {
                          "rg8ui",
                          GL_RG8UI,
                      },
                      {
                          "rg8_snorm",
                          GL_RG8_SNORM,
                      },
                      {
                          "r32f",
                          GL_R32F,
                      },
                      {
                          "r32i",
                          GL_R32I,
                      },
                      {
                          "r32ui",
                          GL_R32UI,
                      },
                      {
                          "r16f",
                          GL_R16F,
                      },
                      {
                          "r16i",
                          GL_R16I,
                      },
                      {
                          "r16ui",
                          GL_R16UI,
                      },
                      {
                          "r8",
                          GL_R8,
                      },
                      {
                          "r8i",
                          GL_R8I,
                      },
                      {
                          "r8ui",
                          GL_R8UI,
                      },
                      {
                          "r8_snorm",
                          GL_R8_SNORM,
                      }};

    for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(texFormats); formatNdx++)
    {
        uint32_t format        = texFormats[formatNdx].internalFormat;
        string nameBase        = texFormats[formatNdx].name;
        uint32_t wrapS         = GL_CLAMP_TO_EDGE;
        uint32_t wrapT         = GL_CLAMP_TO_EDGE;
        uint32_t minFilter     = GL_NEAREST;
        uint32_t magFilter     = GL_NEAREST;
        int numTextures        = 1;
        string descriptionBase = glu::getTextureFormatName(format);

        addChild(new Texture2DRenderCase(m_context, nameBase.c_str(), descriptionBase.c_str(), format, wrapS, wrapT,
                                         minFilter, magFilter, tcu::Mat3(), numTextures, false /* npot */));
    }
}

} // namespace Performance
} // namespace gles3
} // namespace deqp
