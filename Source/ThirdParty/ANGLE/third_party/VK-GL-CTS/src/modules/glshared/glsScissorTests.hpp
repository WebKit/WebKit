#ifndef _GLSSCISSORTESTS_HPP
#define _GLSSCISSORTESTS_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
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
 * \brief Common parts for ES2/3 scissor tests
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "tcuVectorType.hpp"
#include "tcuVector.hpp"
#include "sglrGLContext.hpp"

namespace glu
{
class RenderContext;
} // namespace glu

namespace sglr
{
class Context;
} // namespace sglr

namespace deqp
{
namespace gls
{
namespace Functional
{
namespace ScissorTestInternal
{

using tcu::Vec4;

enum PrimitiveType
{
    POINT = 0,
    LINE,
    TRIANGLE,

    PRIMITIVETYPE_LAST
};

enum ClearType
{
    CLEAR_COLOR_FIXED = 0,
    CLEAR_COLOR_FLOAT,
    CLEAR_COLOR_INT,
    CLEAR_COLOR_UINT,
    CLEAR_DEPTH,
    CLEAR_STENCIL,
    CLEAR_DEPTH_STENCIL,

    CLEAR_LAST
};

// Areas are of the form (x,y,widht,height) in the range [0,1]
tcu::TestNode *createPrimitiveTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                   const char *desc, const Vec4 &scissorArea, const Vec4 &renderArea,
                                   PrimitiveType type, int primitiveCount);
tcu::TestNode *createClearTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                               const char *desc, const Vec4 &scissorArea, uint32_t clearMode);

tcu::TestNode *createFramebufferClearTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                          const char *desc, ClearType clearType);

tcu::TestNode *createFramebufferBlitTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                         const char *desc, const Vec4 &scissorArea);

} // namespace ScissorTestInternal
} // namespace Functional
} // namespace gls
} // namespace deqp

#endif // _GLSSCISSORTESTS_HPP
