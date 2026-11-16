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
 * \brief Generator state.
 *//*--------------------------------------------------------------------*/

#include "rsgGeneratorState.hpp"
#include "rsgVariableManager.hpp"
#include "rsgVariableType.hpp"

namespace rsg
{

GeneratorState::GeneratorState(const ProgramParameters &programParams, de::Random &random)
    : m_programParams(programParams)
    , m_random(random)
    , m_shaderParams(nullptr)
    , m_shader(nullptr)
    , m_varManager(nullptr)
    , m_statementStack(nullptr)
    , m_expressionDepth(0)
{
    m_exprFlagStack.push_back(0);
    m_precedenceStack.push_back(PRECEDENCE_MAX);
}

GeneratorState::~GeneratorState(void)
{
}

void GeneratorState::setShader(const ShaderParameters &shaderParams, Shader &shader)
{
    m_shaderParams = &shaderParams;
    m_shader       = &shader;
}

} // namespace rsg
