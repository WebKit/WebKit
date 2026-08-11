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
 * \brief Program generator.
 *//*--------------------------------------------------------------------*/

#include "rsgProgramGenerator.hpp"
#include "rsgShaderGenerator.hpp"
#include "rsgGeneratorState.hpp"

using std::vector;

namespace rsg
{

ProgramGenerator::ProgramGenerator(void)
{
}

ProgramGenerator::~ProgramGenerator(void)
{
}

void ProgramGenerator::generate(const ProgramParameters &programParams, Shader &vertexShader, Shader &fragmentShader)
{
    // Random number generator
    de::Random rnd(programParams.seed);

    GeneratorState state(programParams, rnd);

    // Fragment shader
    {
        ShaderGenerator shaderGen(state);
        vector<ShaderInput *> emptyOutputs; // \note [pyry] gl_FragColor is added in ShaderGenerator
        shaderGen.generate(programParams.fragmentParameters, fragmentShader, emptyOutputs);
    }

    // Vertex shader
    {
        ShaderGenerator shaderGen(state);

        // Initialize outputs from fragment shader inputs
        const vector<ShaderInput *> &fragmentInputs =
            fragmentShader.getInputs(); // \note gl_Position and dEQP_Position are handled in ShaderGenerator

        shaderGen.generate(programParams.vertexParameters, vertexShader, fragmentInputs);
    }

    // Allocate samplers \todo [pyry] Randomize allocation.
    {
        const vector<ShaderInput *> &vertexUniforms   = vertexShader.getUniforms();
        const vector<ShaderInput *> &fragmentUniforms = fragmentShader.getUniforms();
        vector<ShaderInput *> unifiedSamplers;
        int curSamplerNdx = 0;

        // Build unified sampler list.
        for (vector<ShaderInput *>::const_iterator i = vertexUniforms.begin(); i != vertexUniforms.end(); i++)
        {
            if ((*i)->getVariable()->getType().isSampler())
                unifiedSamplers.push_back(*i);
        }

        for (vector<ShaderInput *>::const_iterator i = fragmentUniforms.begin(); i != fragmentUniforms.end(); i++)
        {
            if ((*i)->getVariable()->getType().isSampler())
                unifiedSamplers.push_back(*i);
        }

        // Assign sampler indices.
        for (vector<ShaderInput *>::const_iterator i = unifiedSamplers.begin(); i != unifiedSamplers.end(); i++)
        {
            ShaderInput *input = *i;
            if (input->getVariable()->getType().isSampler())
            {
                input->getValueRange().getMin() = curSamplerNdx;
                input->getValueRange().getMax() = curSamplerNdx;
                curSamplerNdx += 1;
            }
        }
    }
}

} // namespace rsg
