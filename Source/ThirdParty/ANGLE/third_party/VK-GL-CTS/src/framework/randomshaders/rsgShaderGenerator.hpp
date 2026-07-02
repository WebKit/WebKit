#ifndef _RSGSHADERGENERATOR_HPP
#define _RSGSHADERGENERATOR_HPP
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
 * \brief Shader generator.
 *//*--------------------------------------------------------------------*/

#include "rsgDefs.hpp"
#include "rsgShader.hpp"
#include "rsgGeneratorState.hpp"
#include "rsgVariableManager.hpp"

#include <vector>

namespace rsg
{

class ShaderGenerator
{
public:
    ShaderGenerator(GeneratorState &state);
    ~ShaderGenerator(void);

    void generate(const ShaderParameters &shaderParams, Shader &shader, const std::vector<ShaderInput *> &outputs);

private:
    ShaderGenerator(const ShaderGenerator &other);
    ShaderGenerator &operator=(const ShaderGenerator &other);

    GeneratorState &m_state;
    VariableManager m_varManager;
};

} // namespace rsg

#endif // _RSGSHADERGENERATOR_HPP
