#ifndef _GLSTEXTURESTATEQUERYTESTS_HPP
#define _GLSTEXTURESTATEQUERYTESTS_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "gluRenderContext.hpp"
#include "glsStateQueryUtil.hpp"
#include "glwDefs.hpp"

namespace deqp
{
namespace gls
{
namespace TextureStateQueryTests
{

#define GEN_PURE_SETTERS(X) X, X##_SET_PURE_INT, X##_SET_PURE_UINT

enum TesterType
{
    GEN_PURE_SETTERS(TESTER_TEXTURE_SWIZZLE_R),
    GEN_PURE_SETTERS(TESTER_TEXTURE_SWIZZLE_G),
    GEN_PURE_SETTERS(TESTER_TEXTURE_SWIZZLE_B),
    GEN_PURE_SETTERS(TESTER_TEXTURE_SWIZZLE_A),
    GEN_PURE_SETTERS(TESTER_TEXTURE_WRAP_S),
    GEN_PURE_SETTERS(TESTER_TEXTURE_WRAP_T),
    GEN_PURE_SETTERS(TESTER_TEXTURE_WRAP_R),
    GEN_PURE_SETTERS(TESTER_TEXTURE_MAG_FILTER),
    GEN_PURE_SETTERS(TESTER_TEXTURE_MIN_FILTER),
    GEN_PURE_SETTERS(TESTER_TEXTURE_MIN_LOD),
    GEN_PURE_SETTERS(TESTER_TEXTURE_MAX_LOD),
    GEN_PURE_SETTERS(TESTER_TEXTURE_BASE_LEVEL),
    GEN_PURE_SETTERS(TESTER_TEXTURE_MAX_LEVEL),
    GEN_PURE_SETTERS(TESTER_TEXTURE_COMPARE_MODE),
    GEN_PURE_SETTERS(TESTER_TEXTURE_COMPARE_FUNC),
    TESTER_TEXTURE_IMMUTABLE_LEVELS,
    TESTER_TEXTURE_IMMUTABLE_FORMAT,

    GEN_PURE_SETTERS(TESTER_DEPTH_STENCIL_TEXTURE_MODE),
    GEN_PURE_SETTERS(TESTER_TEXTURE_SRGB_DECODE_EXT),
    TESTER_TEXTURE_BORDER_COLOR,
    TESTER_TEXTURE_WRAP_S_CLAMP_TO_BORDER,
    TESTER_TEXTURE_WRAP_T_CLAMP_TO_BORDER,
    TESTER_TEXTURE_WRAP_R_CLAMP_TO_BORDER,

    TESTER_LAST
};

#undef GEN_PURE_SETTERS

bool isLegalTesterForTarget(glw::GLenum target, TesterType tester);
bool isMultisampleTarget(glw::GLenum target);
bool isSamplerStateTester(TesterType tester);

tcu::TestCase *createIsTextureTest(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx,
                                   const std::string &name, const std::string &description, glw::GLenum target);
tcu::TestCase *createTexParamTest(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx,
                                  const std::string &name, const std::string &description,
                                  StateQueryUtil::QueryType queryType, glw::GLenum target, TesterType tester);
tcu::TestCase *createSamplerParamTest(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx,
                                      const std::string &name, const std::string &description,
                                      StateQueryUtil::QueryType queryType, TesterType tester);

} // namespace TextureStateQueryTests
} // namespace gls
} // namespace deqp

#endif // _GLSTEXTURESTATEQUERYTESTS_HPP
