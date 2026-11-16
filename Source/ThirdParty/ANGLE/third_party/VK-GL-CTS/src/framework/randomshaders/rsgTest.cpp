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
 * \brief Random Shader Generator Tester.
 *//*--------------------------------------------------------------------*/

#include "rsgProgramGenerator.hpp"
#include "rsgProgramExecutor.hpp"
#include "tcuSurface.hpp"
#include "tcuImageIO.hpp"
#include "rsgUtils.hpp"
#include "deStringUtil.hpp"

#include <iostream>
#include <string>
#include <cstdio>

using std::string;

void runTest(uint32_t seed)
{
    printf("Seed: %d\n", seed);

    // Generate test program
    try
    {
        rsg::ProgramParameters programParams;

        programParams.seed                                 = seed;
        programParams.fragmentParameters.randomize         = true;
        programParams.fragmentParameters.maxStatementDepth = 3;

        rsg::Shader vertexShader(rsg::Shader::TYPE_VERTEX);
        rsg::Shader fragmentShader(rsg::Shader::TYPE_FRAGMENT);

        rsg::ProgramGenerator generator;
        generator.generate(programParams, vertexShader, fragmentShader);

        std::cout << "Vertex shader:\n--\n" << vertexShader.getSource() << "--\n";
        std::cout << "Fragment shader:\n--\n" << fragmentShader.getSource() << "--\n";

        // Uniforms
        std::vector<const rsg::ShaderInput *> uniforms;
        std::vector<rsg::VariableValue> uniformValues;
        de::Random rnd(seed);
        rsg::computeUnifiedUniforms(vertexShader, fragmentShader, uniforms);
        rsg::computeUniformValues(rnd, uniformValues, uniforms);

        // Render image
        tcu::Surface surface(64, 64);
        rsg::ProgramExecutor executor(surface.getAccess(), 3, 5);

        executor.execute(vertexShader, fragmentShader, uniformValues);

        string fileName = string("test-") + de::toString(seed) + ".png";
        tcu::ImageIO::savePNG(surface.getAccess(), fileName.c_str());
        std::cout << fileName << " written\n";
    }
    catch (const std::exception &e)
    {
        printf("Failed: %s\n", e.what());
    }
}

int main(int argc, const char *const *argv)
{
    DE_UNREF(argc && argv);

    for (int seed = 0; seed < 10; seed++)
        runTest(seed);

    return 0;
}
