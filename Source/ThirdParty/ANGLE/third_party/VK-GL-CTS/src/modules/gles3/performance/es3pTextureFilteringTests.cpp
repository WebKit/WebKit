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
 * \brief Texture filtering performance tests.
 *//*--------------------------------------------------------------------*/

#include "es3pTextureFilteringTests.hpp"
#include "es3pTextureCases.hpp"
#include "tcuMatrixUtil.hpp"

#include "glwEnums.hpp"

using std::string;

namespace deqp
{
namespace gles3
{
namespace Performance
{

TextureFilteringTests::TextureFilteringTests(Context &context)
    : TestCaseGroup(context, "filter", "Texture Filtering Performance Tests")
{
}

TextureFilteringTests::~TextureFilteringTests(void)
{
}

void TextureFilteringTests::init(void)
{
    static const struct
    {
        const char *name;
        uint32_t internalFormat;
    } texFormats[] = {{"rgb565", GL_RGB565}, {"rgba8", GL_RGBA8}, {"rg16f", GL_RG16F}, {"rgba16f", GL_RGBA16F}};
    static const struct
    {
        const char *name;
        uint32_t filter;
        bool minify;
    } cases[] = {{"nearest", GL_NEAREST, true},
                 {"nearest", GL_NEAREST, false},
                 {"linear", GL_LINEAR, true},
                 {"linear", GL_LINEAR, false},
                 {"nearest_mipmap_nearest", GL_NEAREST_MIPMAP_NEAREST, true},
                 {"nearest_mipmap_linear", GL_NEAREST_MIPMAP_LINEAR, true},
                 {"linear_mipmap_nearest", GL_LINEAR_MIPMAP_NEAREST, true},
                 {"linear_mipmap_linear", GL_LINEAR_MIPMAP_LINEAR, true}};

    tcu::Mat3 minTransform = tcu::translationMatrix(tcu::Vec2(-0.3f, -0.6f)) * tcu::Mat3(tcu::Vec3(1.7f, 2.3f, 1.0f));
    tcu::Mat3 magTransform = tcu::translationMatrix(tcu::Vec2(0.3f, 0.4f)) * tcu::Mat3(tcu::Vec3(0.3f, 0.2f, 1.0f));

    for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); caseNdx++)
    {
        for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(texFormats); formatNdx++)
        {
            uint32_t format    = texFormats[formatNdx].internalFormat;
            uint32_t minFilter = cases[caseNdx].filter;
            uint32_t magFilter = (minFilter == GL_NEAREST || minFilter == GL_LINEAR) ? minFilter : GL_LINEAR;
            uint32_t wrapS     = GL_REPEAT;
            uint32_t wrapT     = GL_REPEAT;
            int numTextures    = 1;
            bool minify        = cases[caseNdx].minify;
            string name =
                string(cases[caseNdx].name) + (minify ? "_minify_" : "_magnify_") + texFormats[formatNdx].name;

            addChild(new Texture2DRenderCase(m_context, name.c_str(), "", format, wrapS, wrapT, minFilter, magFilter,
                                             minify ? minTransform : magTransform, numTextures, true /* pot */));
        }
    }
}

} // namespace Performance
} // namespace gles3
} // namespace deqp
