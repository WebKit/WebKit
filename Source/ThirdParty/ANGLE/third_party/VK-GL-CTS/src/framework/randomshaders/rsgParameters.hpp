#ifndef _RSGPARAMETERS_HPP
#define _RSGPARAMETERS_HPP
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
 * \brief Shader generator parameters.
 *//*--------------------------------------------------------------------*/

#include "rsgDefs.hpp"

#include <set>
#include <string>
#include <vector>

namespace rsg
{

enum
{
    NUM_RESERVED_SHADER_INPUTS = 1 // Taken by dEQP_Position
};

enum Version
{
    VERSION_100, //!< GLSL ES 1.0
    VERSION_300, //!< GLSL ES 3.0

    VERSION_LAST
};

class ShaderParameters
{
public:
    ShaderParameters(void)
        : randomize(false)
        , maxStatementDepth(2)
        , maxStatementsPerBlock(10)
        , maxExpressionDepth(5)
        , maxCombinedVariableScalars(32)
        , maxUniformScalars(512)
        , maxInputVariables(8)
        , texLookupBaseWeight(0.0f)
        , maxSamplers(8)
        , useTexture2D(false)
        , useTextureCube(false)
    {
    }

    bool randomize; //!< If not enabled, only simple passthrough will be generated
    int maxStatementDepth;
    int maxStatementsPerBlock;
    int maxExpressionDepth;
    int maxCombinedVariableScalars;
    int maxUniformScalars;
    int maxInputVariables;

    float texLookupBaseWeight;
    int maxSamplers;
    bool useTexture2D;
    bool useTextureCube;
};

class ProgramParameters
{
public:
    ProgramParameters(void)
        : seed(0)
        , version(VERSION_100)
        , declarationStatementBaseWeight(1.0f)
        , useScalarConversions(false)
        , useSwizzle(false)
        , useComparisonOps(false)
        , useConditionals(false)
        , trigonometricBaseWeight(0.0f)
        , exponentialBaseWeight(0.0f)
    {
    }

    uint32_t seed;
    Version version;
    ShaderParameters vertexParameters;
    ShaderParameters fragmentParameters;

    bool declarationStatementBaseWeight;
    bool useScalarConversions;
    bool useSwizzle;
    bool useComparisonOps;
    bool useConditionals;

    float trigonometricBaseWeight;
    float exponentialBaseWeight;
};

} // namespace rsg

#endif // _RSGPARAMETERS_HPP
