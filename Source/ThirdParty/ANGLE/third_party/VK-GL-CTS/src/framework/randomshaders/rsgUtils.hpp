#ifndef _RSGUTILS_HPP
#define _RSGUTILS_HPP
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
 * \brief Utilities.
 *//*--------------------------------------------------------------------*/

#include "rsgDefs.hpp"
#include "rsgShader.hpp"
#include "rsgVariableValue.hpp"
#include "deRandom.hpp"
#include "rsgGeneratorState.hpp"
#include "deMath.h"

#include <vector>

namespace rsg
{

void computeUnifiedUniforms(const Shader &vertexShader, const Shader &fragmentShader,
                            std::vector<const ShaderInput *> &uniforms);
void computeUniformValues(de::Random &rnd, std::vector<VariableValue> &values,
                          const std::vector<const ShaderInput *> &uniforms);

// \todo [2011-03-26 pyry] Move to ExpressionUtils!

bool isUndefinedValueRange(ConstValueRangeAccess valueRange);

int getConservativeValueExprDepth(const GeneratorState &state, ConstValueRangeAccess valueRange);
int getTypeConstructorDepth(const VariableType &type);

VariableType computeRandomType(GeneratorState &state, int maxScalars);
void computeRandomValueRange(GeneratorState &state, ValueRangeAccess valueRange);

float computeDynamicRangeWeight(ConstValueRangeAccess valueRange);

inline float getQuantizedFloat(de::Random &rnd, float min, float max, float step)
{
    int numSteps = (int)((max - min) / step);
    return min + step * (float)rnd.getInt(0, numSteps);
}

inline bool quantizeFloatRange(float &min, float &max)
{
    const float subdiv = 8;

    float newMin = deFloatCeil(min * subdiv) / subdiv;
    if (newMin <= max)
        min = newMin; // Minimum value quantized
    else
        return false;

    float newMax = deFloatFloor(max * subdiv) / subdiv;
    if (min <= newMax)
        max = newMax;
    else
        return false;

    return true;
}

} // namespace rsg

#endif // _RSGUTILS_HPP
