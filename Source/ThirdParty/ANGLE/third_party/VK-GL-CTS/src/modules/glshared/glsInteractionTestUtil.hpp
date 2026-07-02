#ifndef _GLSINTERACTIONTESTUTIL_HPP
#define _GLSINTERACTIONTESTUTIL_HPP
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
 * \brief Interaction test utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "glsFragmentOpUtil.hpp"
#include "gluRenderContext.hpp"
#include "rrRenderState.hpp"

namespace de
{
class Random;
}

namespace deqp
{
namespace gls
{
namespace InteractionTestUtil
{

struct BlendState
{
    uint32_t equation;
    uint32_t srcFunc;
    uint32_t dstFunc;

    BlendState(void) : equation(0), srcFunc(0), dstFunc(0)
    {
    }
};

struct StencilState
{
    uint32_t function;
    int reference;
    uint32_t compareMask;

    uint32_t stencilFailOp;
    uint32_t depthFailOp;
    uint32_t depthPassOp;

    uint32_t writeMask;

    StencilState(void)
        : function(0)
        , reference(0)
        , compareMask(0)
        , stencilFailOp(0)
        , depthFailOp(0)
        , depthPassOp(0)
        , writeMask(0)
    {
    }
};

struct RenderState
{
    bool scissorTestEnabled;
    rr::WindowRectangle scissorRectangle;

    bool stencilTestEnabled;
    StencilState stencil[rr::FACETYPE_LAST];

    bool depthTestEnabled;
    uint32_t depthFunc;
    bool depthWriteMask;

    bool blendEnabled;
    BlendState blendRGBState;
    BlendState blendAState;
    tcu::Vec4 blendColor;

    bool ditherEnabled;

    tcu::BVec4 colorMask;

    RenderState(void)
        : scissorTestEnabled(false)
        , scissorRectangle(0, 0, 0, 0)
        , stencilTestEnabled(false)
        , depthTestEnabled(false)
        , depthFunc(0)
        , depthWriteMask(false)
        , blendEnabled(false)
        , ditherEnabled(false)
    {
    }
};

struct RenderCommand
{
    gls::FragmentOpUtil::IntegerQuad quad;
    RenderState state;
};

void computeRandomRenderState(de::Random &rnd, RenderState &state, glu::ApiType apiType, int targetWidth,
                              int targetHeight);
void computeRandomQuad(de::Random &rnd, gls::FragmentOpUtil::IntegerQuad &quad, int targetWidth, int targetHeight);
void computeRandomRenderCommands(de::Random &rnd, glu::ApiType apiType, int numCommands, int targetW, int targetH,
                                 std::vector<RenderCommand> &dst);

} // namespace InteractionTestUtil
} // namespace gls
} // namespace deqp

#endif // _GLSINTERACTIONTESTUTIL_HPP
