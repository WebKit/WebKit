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
 * \brief Texture count performance tests.
 *//*--------------------------------------------------------------------*/

#include "es3pTextureCountTests.hpp"
#include "es3pTextureCases.hpp"
#include "gluStrUtil.hpp"

#include "deStringUtil.hpp"

#include "glwEnums.hpp"

using std::string;

namespace deqp
{
namespace gles3
{
namespace Performance
{

TextureCountTests::TextureCountTests(Context &context)
    : TestCaseGroup(context, "count", "Texture Count Performance Tests")
{
}

TextureCountTests::~TextureCountTests(void)
{
}

void TextureCountTests::init(void)
{
    static const struct
    {
        const char *name;
        uint32_t internalFormat;
    } texFormats[] = {{"rgb565", GL_RGB565}, {"rgb8", GL_RGB8},       {"rgba8", GL_RGBA8},    {"rgba8ui", GL_RGBA8UI},
                      {"rg16f", GL_RG16F},   {"rgba16f", GL_RGBA16F}, {"rgba32f", GL_RGBA32F}};
    static const int texCounts[] = {1, 2, 4, 8};

    for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(texFormats); formatNdx++)
    {
        for (int cntNdx = 0; cntNdx < DE_LENGTH_OF_ARRAY(texCounts); cntNdx++)
        {
            uint32_t format    = texFormats[formatNdx].internalFormat;
            uint32_t wrapS     = GL_CLAMP_TO_EDGE;
            uint32_t wrapT     = GL_CLAMP_TO_EDGE;
            uint32_t minFilter = GL_NEAREST;
            uint32_t magFilter = GL_NEAREST;
            int numTextures    = texCounts[cntNdx];
            string name        = string(texFormats[formatNdx].name) + "_" + de::toString(numTextures);
            string description = glu::getTextureFormatName(format);

            addChild(new Texture2DRenderCase(m_context, name.c_str(), description.c_str(), format, wrapS, wrapT,
                                             minFilter, magFilter, tcu::Mat3(), numTextures, false /* npot */));
        }
    }
}

} // namespace Performance
} // namespace gles3
} // namespace deqp
