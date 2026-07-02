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
 * \brief Shader Class.
 *//*--------------------------------------------------------------------*/

#include "rsgShader.hpp"

using std::vector;

namespace rsg
{

namespace
{

template <typename T>
void deleteVectorElements(std::vector<T *> &vec)
{
    for (typename std::vector<T *>::iterator i = vec.begin(); i != vec.end(); i++)
        delete *i;
    vec.clear();
}

} // namespace

Function::Function(void)
{
}

Function::Function(const char *name) : m_name(name)
{
}

Function::~Function(void)
{
    deleteVectorElements(m_parameters);
}

ShaderInput::ShaderInput(const Variable *variable, ConstValueRangeAccess valueRange)
    : m_variable(variable)
    , m_min(variable->getType().getScalarSize())
    , m_max(variable->getType().getScalarSize())
{
    ValueAccess(variable->getType(), &m_min[0]) = valueRange.getMin().value();
    ValueAccess(variable->getType(), &m_max[0]) = valueRange.getMax().value();
}

Shader::Shader(Type type) : m_type(type), m_mainFunction("main")
{
}

Shader::~Shader(void)
{
    deleteVectorElements(m_functions);
    deleteVectorElements(m_globalStatements);
    deleteVectorElements(m_inputs);
    deleteVectorElements(m_uniforms);
}

void Shader::getOutputs(vector<const Variable *> &outputs) const
{
    outputs.clear();
    const vector<Variable *> &globalVars = m_globalScope.getDeclaredVariables();
    for (vector<Variable *>::const_iterator i = globalVars.begin(); i != globalVars.end(); i++)
    {
        const Variable *var = *i;
        if (var->getStorage() == Variable::STORAGE_SHADER_OUT)
            outputs.push_back(var);
    }
}

void Shader::tokenize(GeneratorState &state, TokenStream &str) const
{
    // Add default precision for float in fragment shaders \todo [pyry] Proper precision handling
    if (state.getShader().getType() == Shader::TYPE_FRAGMENT)
        str << Token::PRECISION << Token::MEDIUM_PRECISION << Token::FLOAT << Token::SEMICOLON << Token::NEWLINE;

    // Tokenize global declaration statements
    for (int ndx = (int)m_globalStatements.size() - 1; ndx >= 0; ndx--)
        m_globalStatements[ndx]->tokenize(state, str);

    // Tokenize all functions
    for (int ndx = (int)m_functions.size() - 1; ndx >= 0; ndx--)
    {
        str << Token::NEWLINE;
        m_functions[ndx]->tokenize(state, str);
    }

    // Tokenize main
    str << Token::NEWLINE;
    m_mainFunction.tokenize(state, str);
}

void Shader::execute(ExecutionContext &execCtx) const
{
    // Execute global statements (declarations)
    for (vector<Statement *>::const_reverse_iterator i = m_globalStatements.rbegin(); i != m_globalStatements.rend();
         i++)
        (*i)->execute(execCtx);

    // \todo [2011-03-08 pyry] Proper function calls
    m_mainFunction.getBody().execute(execCtx);
}

void Function::tokenize(GeneratorState &state, TokenStream &str) const
{
    // Return type
    m_returnType.tokenizeShortType(str);

    // Function name
    DE_ASSERT(m_name != "");
    str << Token(m_name.c_str());

    // Parameters
    str << Token::LEFT_PAREN;

    for (vector<Variable *>::const_iterator i = m_parameters.begin(); i != m_parameters.end(); i++)
    {
        if (i != m_parameters.begin())
            str << Token::COMMA;
        (*i)->tokenizeDeclaration(state, str);
    }

    str << Token::RIGHT_PAREN << Token::NEWLINE;

    // Tokenize body
    m_functionBlock.tokenize(state, str);
}

} // namespace rsg
