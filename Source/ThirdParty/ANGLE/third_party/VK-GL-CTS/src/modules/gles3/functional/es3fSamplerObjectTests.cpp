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
 * \brief Sampler object tests.
 *//*--------------------------------------------------------------------*/
#include "es3fSamplerObjectTests.hpp"

#include "glsSamplerObjectTest.hpp"

#include "tcuTexture.hpp"
#include "tcuSurface.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "tcuRGBA.hpp"
#include "tcuRenderTarget.hpp"

#include "gluShaderProgram.hpp"
#include "gluPixelTransfer.hpp"
#include "gluRenderContext.hpp"

#include "deRandom.hpp"
#include "deString.h"

#include "glwEnums.hpp"

namespace deqp
{
namespace gles3
{
namespace Functional
{

SamplerObjectTests::SamplerObjectTests(Context &context) : TestCaseGroup(context, "samplers", "Texture sampler tests")
{
}

SamplerObjectTests::~SamplerObjectTests(void)
{
}

void SamplerObjectTests::init(void)
{
    gls::TextureSamplerTest::TestSpec simpleTestCases[] = {
        {"diff_wrap_t",
         "Different GL_TEXTURE_WRAP_T",
         GL_TEXTURE_2D,
         {GL_NEAREST, GL_NEAREST, GL_MIRRORED_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_wrap_s",
         "Different GL_TEXTURE_WRAP_S",
         GL_TEXTURE_2D,
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_MIRRORED_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_wrap_r",
         "Different GL_TEXTURE_WRAP_R",
         GL_TEXTURE_2D,
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_MIRRORED_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_min_filter",
         "Different GL_TEXTURE_MIN_FILTER",
         GL_TEXTURE_2D,
         {GL_LINEAR, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_mag_filter",
         "Different GL_TEXTURE_MAG_FILTER",
         GL_TEXTURE_2D,
         {GL_NEAREST, GL_LINEAR, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_max_lod",
         "Different GL_TEXTURE_MAX_LOD",
         GL_TEXTURE_2D,
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, -999.0f}},
        {"diff_min_lod",
         "Different GL_TEXTURE_MIN_LOD",
         GL_TEXTURE_2D,
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, 0.0f, 1000.0f},
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, 100.0f, 1000.0f}}};

    TestCaseGroup *simpleTexture2D = new TestCaseGroup(m_context, "single_tex_2d", "Simple 2D texture with sampler");

    for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(simpleTestCases); testNdx++)
        simpleTexture2D->addChild(
            new gls::TextureSamplerTest(m_testCtx, m_context.getRenderContext(), simpleTestCases[testNdx]));

    addChild(simpleTexture2D);

    gls::MultiTextureSamplerTest::TestSpec multiTestCases[] = {
        {"diff_wrap_t",
         "Different GL_TEXTURE_WRAP_T",
         GL_TEXTURE_2D,
         {GL_NEAREST, GL_NEAREST, GL_MIRRORED_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_MIRRORED_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_wrap_s",
         "Different GL_TEXTURE_WRAP_S",
         GL_TEXTURE_2D,
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_MIRRORED_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_MIRRORED_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_wrap_r",
         "Different GL_TEXTURE_WRAP_R",
         GL_TEXTURE_2D,
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_MIRRORED_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_MIRRORED_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_min_filter",
         "Different GL_TEXTURE_MIN_FILTER",
         GL_TEXTURE_2D,
         {GL_LINEAR, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_LINEAR, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_mag_filter",
         "Different GL_TEXTURE_MAG_FILTER",
         GL_TEXTURE_2D,
         {GL_NEAREST, GL_LINEAR, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_LINEAR, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_max_lod",
         "Different GL_TEXTURE_MAX_LOD",
         GL_TEXTURE_2D,
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, -999.0f}},
        {"diff_min_lod",
         "Different GL_TEXTURE_MIN_LOD",
         GL_TEXTURE_2D,
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, 0.0f, 1000.0f},
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, 0.0f, 1000.0f},
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, 100.0f, 1000.0f}}};

    TestCaseGroup *multiTexture2D =
        new TestCaseGroup(m_context, "multi_tex_2d", "Multiple texture units 2D texture with sampler");

    for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(multiTestCases); testNdx++)
        multiTexture2D->addChild(
            new gls::MultiTextureSamplerTest(m_testCtx, m_context.getRenderContext(), multiTestCases[testNdx]));

    addChild(multiTexture2D);

    gls::TextureSamplerTest::TestSpec simpleTestCases3D[] = {
        {"diff_wrap_t",
         "Different GL_TEXTURE_WRAP_T",
         GL_TEXTURE_3D,
         {GL_NEAREST, GL_NEAREST, GL_MIRRORED_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_wrap_s",
         "Different GL_TEXTURE_WRAP_S",
         GL_TEXTURE_3D,
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_MIRRORED_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_wrap_r",
         "Different GL_TEXTURE_WRAP_R",
         GL_TEXTURE_3D,
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_MIRRORED_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_min_filter",
         "Different GL_TEXTURE_MIN_FILTER",
         GL_TEXTURE_3D,
         {GL_LINEAR, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_mag_filter",
         "Different GL_TEXTURE_MAG_FILTER",
         GL_TEXTURE_3D,
         {GL_NEAREST, GL_LINEAR, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_max_lod",
         "Different GL_TEXTURE_MAX_LOD",
         GL_TEXTURE_3D,
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, -999.0f}},
        {"diff_min_lod",
         "Different GL_TEXTURE_MIN_LOD",
         GL_TEXTURE_3D,
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, 0.0f, 1000.0f},
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, 100.0f, 1000.0f}}};

    TestCaseGroup *simpleTexture3D = new TestCaseGroup(m_context, "single_tex_3d", "Simple 3D texture with sampler");

    for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(simpleTestCases3D); testNdx++)
        simpleTexture3D->addChild(
            new gls::TextureSamplerTest(m_testCtx, m_context.getRenderContext(), simpleTestCases3D[testNdx]));

    addChild(simpleTexture3D);

    gls::MultiTextureSamplerTest::TestSpec multiTestCases3D[] = {
        {"diff_wrap_t",
         "Different GL_TEXTURE_WRAP_T",
         GL_TEXTURE_3D,
         {GL_NEAREST, GL_NEAREST, GL_MIRRORED_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_MIRRORED_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_wrap_s",
         "Different GL_TEXTURE_WRAP_S",
         GL_TEXTURE_3D,
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_MIRRORED_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_MIRRORED_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_wrap_r",
         "Different GL_TEXTURE_WRAP_R",
         GL_TEXTURE_3D,
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_MIRRORED_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_MIRRORED_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_min_filter",
         "Different GL_TEXTURE_MIN_FILTER",
         GL_TEXTURE_3D,
         {GL_LINEAR, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_LINEAR, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_mag_filter",
         "Different GL_TEXTURE_MAG_FILTER",
         GL_TEXTURE_3D,
         {GL_NEAREST, GL_LINEAR, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_LINEAR, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_max_lod",
         "Different GL_TEXTURE_MAX_LOD",
         GL_TEXTURE_3D,
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, -999.0f}},
        {"diff_min_lod",
         "Different GL_TEXTURE_MIN_LOD",
         GL_TEXTURE_3D,
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, 0.0f, 1000.0f},
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, 0.0f, 1000.0f},
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, 100.0f, 1000.0f}}};

    TestCaseGroup *multiTexture3D =
        new TestCaseGroup(m_context, "multi_tex_3d", "Multiple texture units 3D texture with sampler");

    for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(multiTestCases3D); testNdx++)
        multiTexture3D->addChild(
            new gls::MultiTextureSamplerTest(m_testCtx, m_context.getRenderContext(), multiTestCases3D[testNdx]));

    addChild(multiTexture3D);

    gls::TextureSamplerTest::TestSpec simpleTestCasesCube[] = {
        {"diff_wrap_t",
         "Different GL_TEXTURE_WRAP_T",
         GL_TEXTURE_CUBE_MAP,
         {GL_NEAREST, GL_NEAREST, GL_MIRRORED_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_wrap_s",
         "Different GL_TEXTURE_WRAP_S",
         GL_TEXTURE_CUBE_MAP,
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_MIRRORED_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_wrap_r",
         "Different GL_TEXTURE_WRAP_R",
         GL_TEXTURE_CUBE_MAP,
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_MIRRORED_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_min_filter",
         "Different GL_TEXTURE_MIN_FILTER",
         GL_TEXTURE_CUBE_MAP,
         {GL_LINEAR, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_mag_filter",
         "Different GL_TEXTURE_MAG_FILTER",
         GL_TEXTURE_CUBE_MAP,
         {GL_NEAREST, GL_LINEAR, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_max_lod",
         "Different GL_TEXTURE_MAX_LOD",
         GL_TEXTURE_CUBE_MAP,
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, -999.0f}},
        {"diff_min_lod",
         "Different GL_TEXTURE_MIN_LOD",
         GL_TEXTURE_CUBE_MAP,
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, 0.0f, 1000.0f},
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, 100.0f, 1000.0f}}};

    TestCaseGroup *simpleTextureCube =
        new TestCaseGroup(m_context, "single_cubemap", "Simple cubemap texture with sampler");

    for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(simpleTestCasesCube); testNdx++)
        simpleTextureCube->addChild(
            new gls::TextureSamplerTest(m_testCtx, m_context.getRenderContext(), simpleTestCasesCube[testNdx]));

    addChild(simpleTextureCube);

    gls::MultiTextureSamplerTest::TestSpec multiTestCasesCube[] = {
        {"diff_wrap_t",
         "Different GL_TEXTURE_WRAP_T",
         GL_TEXTURE_CUBE_MAP,
         {GL_NEAREST, GL_NEAREST, GL_MIRRORED_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_MIRRORED_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_wrap_s",
         "Different GL_TEXTURE_WRAP_S",
         GL_TEXTURE_CUBE_MAP,
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_MIRRORED_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_MIRRORED_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_wrap_r",
         "Different GL_TEXTURE_WRAP_R",
         GL_TEXTURE_CUBE_MAP,
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_MIRRORED_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_MIRRORED_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_min_filter",
         "Different GL_TEXTURE_MIN_FILTER",
         GL_TEXTURE_CUBE_MAP,
         {GL_LINEAR, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_LINEAR, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_mag_filter",
         "Different GL_TEXTURE_MAG_FILTER",
         GL_TEXTURE_CUBE_MAP,
         {GL_NEAREST, GL_LINEAR, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_LINEAR, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f}},
        {"diff_max_lod",
         "Different GL_TEXTURE_MAX_LOD",
         GL_TEXTURE_CUBE_MAP,
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, 1000.0f},
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, -1000.0f, -999.0f}},
        {"diff_min_lod",
         "Different GL_TEXTURE_MIN_LOD",
         GL_TEXTURE_CUBE_MAP,
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, 0.0f, 1000.0f},
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, 0.0f, 1000.0f},
         {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_REPEAT, GL_REPEAT, GL_REPEAT, 100.0f, 1000.0f}}};

    TestCaseGroup *multiTextureCube =
        new TestCaseGroup(m_context, "multi_cubemap", "Multiple texture units cubemap textures with sampler");

    for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(multiTestCasesCube); testNdx++)
        multiTextureCube->addChild(
            new gls::MultiTextureSamplerTest(m_testCtx, m_context.getRenderContext(), multiTestCasesCube[testNdx]));

    addChild(multiTextureCube);
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
