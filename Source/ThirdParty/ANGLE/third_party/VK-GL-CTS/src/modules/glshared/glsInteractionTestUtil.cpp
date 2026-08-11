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

#include "glsInteractionTestUtil.hpp"

#include "tcuVector.hpp"

#include "deRandom.hpp"
#include "deMath.h"

#include "glwEnums.hpp"

namespace deqp
{
namespace gls
{
namespace InteractionTestUtil
{

using std::vector;
using tcu::IVec2;
using tcu::Vec4;

static Vec4 getRandomColor(de::Random &rnd)
{
    static const float components[] = {0.0f, 0.2f, 0.4f, 0.5f, 0.6f, 0.8f, 1.0f};
    float r                         = rnd.choose<float>(DE_ARRAY_BEGIN(components), DE_ARRAY_END(components));
    float g                         = rnd.choose<float>(DE_ARRAY_BEGIN(components), DE_ARRAY_END(components));
    float b                         = rnd.choose<float>(DE_ARRAY_BEGIN(components), DE_ARRAY_END(components));
    float a                         = rnd.choose<float>(DE_ARRAY_BEGIN(components), DE_ARRAY_END(components));
    return Vec4(r, g, b, a);
}

void computeRandomRenderState(de::Random &rnd, RenderState &state, glu::ApiType apiType, int targetWidth,
                              int targetHeight)
{
    // Constants governing randomization.
    const float scissorTestProbability = 0.2f;
    const float stencilTestProbability = 0.4f;
    const float depthTestProbability   = 0.6f;
    const float blendProbability       = 0.4f;
    const float ditherProbability      = 0.5f;

    const float depthWriteProbability = 0.7f;
    const float colorWriteProbability = 0.7f;

    const int minStencilVal = -3;
    const int maxStencilVal = 260;

    const int maxScissorOutOfBounds = 10;
    const float minScissorSize      = 0.7f;

    static const uint32_t compareFuncs[] = {GL_NEVER, GL_ALWAYS, GL_LESS,    GL_LEQUAL,
                                            GL_EQUAL, GL_GEQUAL, GL_GREATER, GL_NOTEQUAL};

    static const uint32_t stencilOps[] = {GL_KEEP, GL_ZERO,   GL_REPLACE,   GL_INCR,
                                          GL_DECR, GL_INVERT, GL_INCR_WRAP, GL_DECR_WRAP};

    static const uint32_t blendEquations[] = {GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, GL_MIN, GL_MAX};

    static const uint32_t blendFuncs[] = {GL_ZERO,
                                          GL_ONE,
                                          GL_SRC_COLOR,
                                          GL_ONE_MINUS_SRC_COLOR,
                                          GL_DST_COLOR,
                                          GL_ONE_MINUS_DST_COLOR,
                                          GL_SRC_ALPHA,
                                          GL_ONE_MINUS_SRC_ALPHA,
                                          GL_DST_ALPHA,
                                          GL_ONE_MINUS_DST_ALPHA,
                                          GL_CONSTANT_COLOR,
                                          GL_ONE_MINUS_CONSTANT_COLOR,
                                          GL_CONSTANT_ALPHA,
                                          GL_ONE_MINUS_CONSTANT_ALPHA,
                                          GL_SRC_ALPHA_SATURATE};

    static const uint32_t blendEquationsES2[] = {GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT};

    static const uint32_t blendFuncsDstES2[] = {GL_ZERO,           GL_ONE,
                                                GL_SRC_COLOR,      GL_ONE_MINUS_SRC_COLOR,
                                                GL_DST_COLOR,      GL_ONE_MINUS_DST_COLOR,
                                                GL_SRC_ALPHA,      GL_ONE_MINUS_SRC_ALPHA,
                                                GL_DST_ALPHA,      GL_ONE_MINUS_DST_ALPHA,
                                                GL_CONSTANT_COLOR, GL_ONE_MINUS_CONSTANT_COLOR,
                                                GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA};

    state.scissorTestEnabled = rnd.getFloat() < scissorTestProbability;
    state.stencilTestEnabled = rnd.getFloat() < stencilTestProbability;
    state.depthTestEnabled   = rnd.getFloat() < depthTestProbability;
    state.blendEnabled       = rnd.getFloat() < blendProbability;
    state.ditherEnabled      = rnd.getFloat() < ditherProbability;

    if (state.scissorTestEnabled)
    {
        int minScissorW = deCeilFloatToInt32(minScissorSize * (float)targetWidth);
        int minScissorH = deCeilFloatToInt32(minScissorSize * (float)targetHeight);
        int maxScissorW = targetWidth + 2 * maxScissorOutOfBounds;
        int maxScissorH = targetHeight + 2 * maxScissorOutOfBounds;

        int scissorW = rnd.getInt(minScissorW, maxScissorW);
        int scissorH = rnd.getInt(minScissorH, maxScissorH);
        int scissorX = rnd.getInt(-maxScissorOutOfBounds, targetWidth + maxScissorOutOfBounds - scissorW);
        int scissorY = rnd.getInt(-maxScissorOutOfBounds, targetHeight + maxScissorOutOfBounds - scissorH);

        state.scissorRectangle = rr::WindowRectangle(scissorX, scissorY, scissorW, scissorH);
    }

    if (state.stencilTestEnabled)
    {
        for (int ndx = 0; ndx < 2; ndx++)
        {
            state.stencil[ndx].function =
                rnd.choose<uint32_t>(DE_ARRAY_BEGIN(compareFuncs), DE_ARRAY_END(compareFuncs));
            state.stencil[ndx].reference   = rnd.getInt(minStencilVal, maxStencilVal);
            state.stencil[ndx].compareMask = rnd.getUint32();
            state.stencil[ndx].stencilFailOp =
                rnd.choose<uint32_t>(DE_ARRAY_BEGIN(stencilOps), DE_ARRAY_END(stencilOps));
            state.stencil[ndx].depthFailOp = rnd.choose<uint32_t>(DE_ARRAY_BEGIN(stencilOps), DE_ARRAY_END(stencilOps));
            state.stencil[ndx].depthPassOp = rnd.choose<uint32_t>(DE_ARRAY_BEGIN(stencilOps), DE_ARRAY_END(stencilOps));
            state.stencil[ndx].writeMask   = rnd.getUint32();
        }
    }

    if (state.depthTestEnabled)
    {
        state.depthFunc      = rnd.choose<uint32_t>(DE_ARRAY_BEGIN(compareFuncs), DE_ARRAY_END(compareFuncs));
        state.depthWriteMask = rnd.getFloat() < depthWriteProbability;
    }

    if (state.blendEnabled)
    {
        if (apiType == glu::ApiType::es(2, 0))
        {
            state.blendRGBState.equation =
                rnd.choose<uint32_t>(DE_ARRAY_BEGIN(blendEquationsES2), DE_ARRAY_END(blendEquationsES2));
            state.blendRGBState.srcFunc = rnd.choose<uint32_t>(DE_ARRAY_BEGIN(blendFuncs), DE_ARRAY_END(blendFuncs));
            state.blendRGBState.dstFunc =
                rnd.choose<uint32_t>(DE_ARRAY_BEGIN(blendFuncsDstES2), DE_ARRAY_END(blendFuncsDstES2));

            state.blendAState.equation =
                rnd.choose<uint32_t>(DE_ARRAY_BEGIN(blendEquationsES2), DE_ARRAY_END(blendEquationsES2));
            state.blendAState.srcFunc = rnd.choose<uint32_t>(DE_ARRAY_BEGIN(blendFuncs), DE_ARRAY_END(blendFuncs));
            state.blendAState.dstFunc =
                rnd.choose<uint32_t>(DE_ARRAY_BEGIN(blendFuncsDstES2), DE_ARRAY_END(blendFuncsDstES2));
        }
        else
        {
            state.blendRGBState.equation =
                rnd.choose<uint32_t>(DE_ARRAY_BEGIN(blendEquations), DE_ARRAY_END(blendEquations));
            state.blendRGBState.srcFunc = rnd.choose<uint32_t>(DE_ARRAY_BEGIN(blendFuncs), DE_ARRAY_END(blendFuncs));
            state.blendRGBState.dstFunc = rnd.choose<uint32_t>(DE_ARRAY_BEGIN(blendFuncs), DE_ARRAY_END(blendFuncs));

            state.blendAState.equation =
                rnd.choose<uint32_t>(DE_ARRAY_BEGIN(blendEquations), DE_ARRAY_END(blendEquations));
            state.blendAState.srcFunc = rnd.choose<uint32_t>(DE_ARRAY_BEGIN(blendFuncs), DE_ARRAY_END(blendFuncs));
            state.blendAState.dstFunc = rnd.choose<uint32_t>(DE_ARRAY_BEGIN(blendFuncs), DE_ARRAY_END(blendFuncs));
        }

        state.blendColor = getRandomColor(rnd);
    }

    for (int ndx = 0; ndx < 4; ndx++)
        state.colorMask[ndx] = rnd.getFloat() < colorWriteProbability;
}

void computeRandomQuad(de::Random &rnd, gls::FragmentOpUtil::IntegerQuad &quad, int targetWidth, int targetHeight)
{
    // \note In viewport coordinates.
    // \todo [2012-12-18 pyry] Out-of-bounds values.
    // \note Not using depth 1.0 since clearing with 1.0 and rendering with 1.0 may not be same value.
    static const float depthValues[] = {0.0f, 0.2f, 0.4f, 0.5f, 0.51f, 0.6f, 0.8f, 0.95f};

    const int maxOutOfBounds = 0;
    const float minSize      = 0.5f;

    int minW = deCeilFloatToInt32(minSize * (float)targetWidth);
    int minH = deCeilFloatToInt32(minSize * (float)targetHeight);
    int maxW = targetWidth + 2 * maxOutOfBounds;
    int maxH = targetHeight + 2 * maxOutOfBounds;

    int width  = rnd.getInt(minW, maxW);
    int height = rnd.getInt(minH, maxH);
    int x      = rnd.getInt(-maxOutOfBounds, targetWidth + maxOutOfBounds - width);
    int y      = rnd.getInt(-maxOutOfBounds, targetHeight + maxOutOfBounds - height);

    bool flipX = rnd.getBool();
    bool flipY = rnd.getBool();

    float depth = rnd.choose<float>(DE_ARRAY_BEGIN(depthValues), DE_ARRAY_END(depthValues));

    quad.posA = IVec2(flipX ? (x + width - 1) : x, flipY ? (y + height - 1) : y);
    quad.posB = IVec2(flipX ? x : (x + width - 1), flipY ? y : (y + height - 1));

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(quad.color); ndx++)
        quad.color[ndx] = getRandomColor(rnd);

    std::fill(DE_ARRAY_BEGIN(quad.depth), DE_ARRAY_END(quad.depth), depth);
}

void computeRandomRenderCommands(de::Random &rnd, glu::ApiType apiType, int numCommands, int targetW, int targetH,
                                 vector<RenderCommand> &dst)
{
    DE_ASSERT(dst.empty());

    dst.resize(numCommands);
    for (vector<RenderCommand>::iterator cmd = dst.begin(); cmd != dst.end(); cmd++)
    {
        computeRandomRenderState(rnd, cmd->state, apiType, targetW, targetH);
        computeRandomQuad(rnd, cmd->quad, targetW, targetH);
    }
}

} // namespace InteractionTestUtil
} // namespace gls
} // namespace deqp
