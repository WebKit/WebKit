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
 * \brief Sampler State Query tests.
 *//*--------------------------------------------------------------------*/

#include "es3fSamplerStateQueryTests.hpp"
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
    case QUERY_SAMPLER_PARAM_INTEGER:
        return "_getsamplerparameteri";
    case QUERY_SAMPLER_PARAM_FLOAT:
        return "_getsamplerparameterf";
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

SamplerStateQueryTests::SamplerStateQueryTests(Context &context)
    : TestCaseGroup(context, "sampler", "Sampler State Query tests")
{
}
void SamplerStateQueryTests::init(void)
{
    using namespace gls::TextureStateQueryTests;

    static const QueryType verifiers[] = {QUERY_SAMPLER_PARAM_INTEGER, QUERY_SAMPLER_PARAM_FLOAT};
    static const struct
    {
        const char *name;
        const char *desc;
        TesterType tester;
    } states[] = {
        {"sampler_texture_wrap_s", "TEXTURE_WRAP_S", TESTER_TEXTURE_WRAP_S},
        {"sampler_texture_wrap_t", "TEXTURE_WRAP_T", TESTER_TEXTURE_WRAP_T},
        {"sampler_texture_wrap_r", "TEXTURE_WRAP_R", TESTER_TEXTURE_WRAP_R},
        {"sampler_texture_mag_filter", "TEXTURE_MAG_FILTER", TESTER_TEXTURE_MAG_FILTER},
        {"sampler_texture_min_filter", "TEXTURE_MIN_FILTER", TESTER_TEXTURE_MIN_FILTER},
        {"sampler_texture_min_lod", "TEXTURE_MIN_LOD", TESTER_TEXTURE_MIN_LOD},
        {"sampler_texture_max_lod", "TEXTURE_MAX_LOD", TESTER_TEXTURE_MAX_LOD},
        {"sampler_texture_compare_mode", "TEXTURE_COMPARE_MODE", TESTER_TEXTURE_COMPARE_MODE},
        {"sampler_texture_compare_func", "TEXTURE_COMPARE_FUNC", TESTER_TEXTURE_COMPARE_FUNC},
    };

    for (int stateNdx = 0; stateNdx < DE_LENGTH_OF_ARRAY(states); ++stateNdx)
    {
        FOR_EACH_VERIFIER(verifiers, addChild(createSamplerParamTest(
                                         m_testCtx, m_context.getRenderContext(),
                                         std::string() + states[stateNdx].name + getVerifierSuffix(verifier),
                                         states[stateNdx].desc, verifier, states[stateNdx].tester)))
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
