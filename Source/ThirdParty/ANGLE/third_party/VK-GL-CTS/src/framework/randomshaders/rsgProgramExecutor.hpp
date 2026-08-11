#ifndef _RSGPROGRAMEXECUTOR_HPP
#define _RSGPROGRAMEXECUTOR_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Random Shader Generator
 * ----------------------------------------------------
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
 * \brief Program Executor.
 *//*--------------------------------------------------------------------*/

#include "rsgDefs.hpp"
#include "rsgShader.hpp"
#include "rsgVariableValue.hpp"
#include "tcuTexture.hpp"
#include "rsgSamplers.hpp"

#include <vector>

namespace tcu
{
class Surface;
}

namespace rsg
{

class ProgramExecutor
{
public:
    ProgramExecutor(const tcu::PixelBufferAccess &dst, int gridWidth, int gridHeight);
    ~ProgramExecutor(void);

    void setTexture(int samplerNdx, const tcu::Texture2D *texture, const tcu::Sampler &sampler);
    void setTexture(int samplerNdx, const tcu::TextureCube *texture, const tcu::Sampler &sampler);

    void execute(const Shader &vertexShader, const Shader &fragmentShader, const std::vector<VariableValue> &uniforms);

private:
    tcu::PixelBufferAccess m_dst;
    int m_gridWidth;
    int m_gridHeight;

    Sampler2DMap m_samplers2D;
    SamplerCubeMap m_samplersCube;
};

inline void getVertexInterpolationCoords(float &xd, float &yd, float x, float y, int inputElementNdx)
{
    if (inputElementNdx % 4 < 2)
        xd = x;
    else
        xd = 1.0f - x;

    if (inputElementNdx % 2 == 0)
        yd = y;
    else
        yd = 1.0f - y;
}

} // namespace rsg

#endif // _RSGPROGRAMEXECUTOR_HPP
