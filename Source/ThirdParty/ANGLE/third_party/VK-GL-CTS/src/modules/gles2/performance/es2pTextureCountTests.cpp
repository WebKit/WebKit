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
 * \brief Texture count performance tests.
 *//*--------------------------------------------------------------------*/

#include "es2pTextureCountTests.hpp"
#include "es2pTextureCases.hpp"
#include "gluStrUtil.hpp"

#include "deStringUtil.hpp"

#include "glwEnums.hpp"

using std::string;

namespace deqp
{
namespace gles2
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
        uint32_t format;
        uint32_t dataType;
    } texFormats[]               = {{"a8", GL_ALPHA, GL_UNSIGNED_BYTE},
                                    {"rgb565", GL_RGB, GL_UNSIGNED_SHORT_5_6_5},
                                    {"rgb888", GL_RGB, GL_UNSIGNED_BYTE},
                                    {"rgba8888", GL_RGBA, GL_UNSIGNED_BYTE}};
    static const int texCounts[] = {1, 2, 4, 8};

    for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(texFormats); formatNdx++)
    {
        for (int cntNdx = 0; cntNdx < DE_LENGTH_OF_ARRAY(texCounts); cntNdx++)
        {
            uint32_t format    = texFormats[formatNdx].format;
            uint32_t dataType  = texFormats[formatNdx].dataType;
            uint32_t wrapS     = GL_CLAMP_TO_EDGE;
            uint32_t wrapT     = GL_CLAMP_TO_EDGE;
            uint32_t minFilter = GL_NEAREST;
            uint32_t magFilter = GL_NEAREST;
            int numTextures    = texCounts[cntNdx];
            string name        = string(texFormats[formatNdx].name) + "_" + de::toString(numTextures);
            string description = string(glu::getTextureFormatName(format)) + ", " + glu::getTypeName(dataType);

            addChild(new Texture2DRenderCase(m_context, name.c_str(), description.c_str(), format, dataType, wrapS,
                                             wrapT, minFilter, magFilter, tcu::Mat3(), numTextures, false /* npot */));
        }
    }
}

} // namespace Performance
} // namespace gles2
} // namespace deqp
