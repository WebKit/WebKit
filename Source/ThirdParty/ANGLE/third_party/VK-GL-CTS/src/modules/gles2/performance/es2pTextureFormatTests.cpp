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
 * \brief Texture format performance tests.
 *//*--------------------------------------------------------------------*/

#include "es2pTextureFormatTests.hpp"
#include "es2pTextureCases.hpp"
#include "gluStrUtil.hpp"

#include "glwEnums.hpp"

using std::string;

namespace deqp
{
namespace gles2
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
    static const struct
    {
        const char *name;
        uint32_t format;
        uint32_t dataType;
    } texFormats[] = {{"a8", GL_ALPHA, GL_UNSIGNED_BYTE},
                      {"l8", GL_LUMINANCE, GL_UNSIGNED_BYTE},
                      {"la88", GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE},
                      {"rgb565", GL_RGB, GL_UNSIGNED_SHORT_5_6_5},
                      {"rgba4444", GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4},
                      {"rgba5551", GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1},
                      {"rgb888", GL_RGB, GL_UNSIGNED_BYTE},
                      {"rgba8888", GL_RGBA, GL_UNSIGNED_BYTE}};

    for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(texFormats); formatNdx++)
    {
        uint32_t format        = texFormats[formatNdx].format;
        uint32_t dataType      = texFormats[formatNdx].dataType;
        string nameBase        = texFormats[formatNdx].name;
        uint32_t wrapS         = GL_CLAMP_TO_EDGE;
        uint32_t wrapT         = GL_CLAMP_TO_EDGE;
        uint32_t minFilter     = GL_NEAREST;
        uint32_t magFilter     = GL_NEAREST;
        int numTextures        = 1;
        string descriptionBase = string(glu::getTextureFormatName(format)) + ", " + glu::getTypeName(dataType);

        addChild(new Texture2DRenderCase(m_context, nameBase.c_str(), descriptionBase.c_str(), format, dataType, wrapS,
                                         wrapT, minFilter, magFilter, tcu::Mat3(), numTextures, false /* npot */));
    }
}

} // namespace Performance
} // namespace gles2
} // namespace deqp
