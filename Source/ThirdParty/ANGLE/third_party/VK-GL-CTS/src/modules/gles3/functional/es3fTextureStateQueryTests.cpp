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
 * \brief Texture State Query tests.
 *//*--------------------------------------------------------------------*/

#include "es3fTextureStateQueryTests.hpp"
#include "glsStateQueryUtil.hpp"
#include "glsTextureStateQueryTests.hpp"

#include "gluRenderContext.hpp"
#include "glwEnums.hpp"

using namespace deqp::gls::StateQueryUtil;

namespace deqp
{
namespace gles3
{
namespace Functional
{

static const char *getVerifierSuffix(QueryType type)
{
    switch (type)
    {
    case QUERY_TEXTURE_PARAM_INTEGER:
        return "_gettexparameteri";
    case QUERY_TEXTURE_PARAM_FLOAT:
        return "_gettexparameterf";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

#define FOR_EACH_VERIFIER(VERIFIERS, CODE_BLOCK)                                             \
    for (int _verifierNdx = 0; _verifierNdx < DE_LENGTH_OF_ARRAY(VERIFIERS); _verifierNdx++) \
    {                                                                                        \
        QueryType verifier = (VERIFIERS)[_verifierNdx];                                      \
        CODE_BLOCK;                                                                          \
    }

TextureStateQueryTests::TextureStateQueryTests(Context &context)
    : TestCaseGroup(context, "texture", "Texture State Query tests")
{
}

void TextureStateQueryTests::init(void)
{
    using namespace gls::TextureStateQueryTests;

    static const QueryType verifiers[] = {QUERY_TEXTURE_PARAM_INTEGER, QUERY_TEXTURE_PARAM_FLOAT};

    static const struct
    {
        const char *name;
        glw::GLenum target;
    } textureTargets[] = {{"texture_2d", GL_TEXTURE_2D},
                          {"texture_3d", GL_TEXTURE_3D},
                          {"texture_2d_array", GL_TEXTURE_2D_ARRAY},
                          {"texture_cube_map", GL_TEXTURE_CUBE_MAP}};
    static const struct
    {
        const char *name;
        const char *desc;
        TesterType tester;
    } states[] = {
        {"texture_swizzle_r", "TEXTURE_SWIZZLE_R", TESTER_TEXTURE_SWIZZLE_R},
        {"texture_swizzle_g", "TEXTURE_SWIZZLE_G", TESTER_TEXTURE_SWIZZLE_G},
        {"texture_swizzle_b", "TEXTURE_SWIZZLE_B", TESTER_TEXTURE_SWIZZLE_B},
        {"texture_swizzle_a", "TEXTURE_SWIZZLE_A", TESTER_TEXTURE_SWIZZLE_A},
        {"texture_wrap_s", "TEXTURE_WRAP_S", TESTER_TEXTURE_WRAP_S},
        {"texture_wrap_t", "TEXTURE_WRAP_T", TESTER_TEXTURE_WRAP_T},
        {"texture_wrap_r", "TEXTURE_WRAP_R", TESTER_TEXTURE_WRAP_R},
        {"texture_mag_filter", "TEXTURE_MAG_FILTER", TESTER_TEXTURE_MAG_FILTER},
        {"texture_min_filter", "TEXTURE_MIN_FILTER", TESTER_TEXTURE_MIN_FILTER},
        {"texture_min_lod", "TEXTURE_MIN_LOD", TESTER_TEXTURE_MIN_LOD},
        {"texture_max_lod", "TEXTURE_MAX_LOD", TESTER_TEXTURE_MAX_LOD},
        {"texture_base_level", "TEXTURE_BASE_LEVEL", TESTER_TEXTURE_BASE_LEVEL},
        {"texture_max_level", "TEXTURE_MAX_LEVEL", TESTER_TEXTURE_MAX_LEVEL},
        {"texture_compare_mode", "TEXTURE_COMPARE_MODE", TESTER_TEXTURE_COMPARE_MODE},
        {"texture_compare_func", "TEXTURE_COMPARE_FUNC", TESTER_TEXTURE_COMPARE_FUNC},
        {"texture_immutable_levels", "TEXTURE_IMMUTABLE_LEVELS", TESTER_TEXTURE_IMMUTABLE_LEVELS},
        {"texture_immutable_format", "TEXTURE_IMMUTABLE_FORMAT", TESTER_TEXTURE_IMMUTABLE_FORMAT},
    };

    for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(textureTargets); ++targetNdx)
    {
        addChild(createIsTextureTest(m_testCtx, m_context.getRenderContext(),
                                     std::string() + textureTargets[targetNdx].name + "_is_texture", "IsTexture",
                                     textureTargets[targetNdx].target));

        for (int stateNdx = 0; stateNdx < DE_LENGTH_OF_ARRAY(states); ++stateNdx)
        {
            if (!isLegalTesterForTarget(textureTargets[targetNdx].target, states[stateNdx].tester))
                continue;

            FOR_EACH_VERIFIER(verifiers,
                              addChild(createTexParamTest(m_testCtx, m_context.getRenderContext(),
                                                          std::string() + textureTargets[targetNdx].name + "_" +
                                                              states[stateNdx].name + getVerifierSuffix(verifier),
                                                          states[stateNdx].desc, verifier,
                                                          textureTargets[targetNdx].target, states[stateNdx].tester)))
        }
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
