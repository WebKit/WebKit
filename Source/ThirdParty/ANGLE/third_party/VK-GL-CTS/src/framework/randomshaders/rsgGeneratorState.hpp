#ifndef _RSGGENERATORSTATE_HPP
#define _RSGGENERATORSTATE_HPP
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

#include "rsgDefs.hpp"
#include "rsgParameters.hpp"
#include "deRandom.hpp"
#include "rsgNameAllocator.hpp"

#include <vector>

namespace rsg
{

class Shader;
class VariableManager;
class VariableType;
class Statement;

enum ExpressionFlags
{
    CONST_EXPR        = (1 << 0),
    NO_VAR_ALLOCATION = (1 << 1)
};

enum
{
    PRECEDENCE_MAX = 100
};

class GeneratorState
{
public:
    GeneratorState(const ProgramParameters &programParams, de::Random &random);
    ~GeneratorState(void);

    const ProgramParameters &getProgramParameters(void) const
    {
        return m_programParams;
    }
    de::Random &getRandom(void)
    {
        return m_random;
    }

    const ShaderParameters &getShaderParameters(void) const
    {
        return *m_shaderParams;
    }
    Shader &getShader(void)
    {
        return *m_shader;
    }

    void setShader(const ShaderParameters &params, Shader &shader);

    NameAllocator &getNameAllocator(void)
    {
        return m_nameAllocator;
    }
    VariableManager &getVariableManager(void)
    {
        return *m_varManager;
    }
    const VariableManager &getVariableManager(void) const
    {
        return *m_varManager;
    }
    void setVariableManager(VariableManager &varManager)
    {
        m_varManager = &varManager;
    }

    // \todo [2011-06-10 pyry] Could we not expose whole statement stack to everyone?
    int getStatementDepth(void) const
    {
        return (int)m_statementStack->size();
    }
    void setStatementStack(std::vector<Statement *> &stack)
    {
        m_statementStack = &stack;
    }
    const Statement *getStatementStackEntry(int ndx) const
    {
        return m_statementStack->at(ndx);
    }

    int getExpressionDepth(void) const
    {
        return m_expressionDepth;
    }
    void setExpressionDepth(int depth)
    {
        m_expressionDepth = depth;
    }

    // \todo [2011-03-21 pyry] A bit of a hack... Move to ValueRange?
    uint32_t getExpressionFlags(void) const
    {
        return m_exprFlagStack.back();
    }
    void pushExpressionFlags(uint32_t flags)
    {
        m_exprFlagStack.push_back(flags);
    }
    void popExpressionFlags(void)
    {
        m_exprFlagStack.pop_back();
    }

    int getPrecedence(void) const
    {
        return m_precedenceStack.back();
    }
    void pushPrecedence(int precedence)
    {
        m_precedenceStack.push_back(precedence);
    }
    void popPrecedence(void)
    {
        m_precedenceStack.pop_back();
    }

private:
    const ProgramParameters &m_programParams;
    de::Random &m_random;

    const ShaderParameters *m_shaderParams;
    Shader *m_shader;

    NameAllocator m_nameAllocator;
    VariableManager *m_varManager;

    std::vector<Statement *> *m_statementStack;
    int m_expressionDepth;
    std::vector<uint32_t> m_exprFlagStack;
    std::vector<int> m_precedenceStack;
};

} // namespace rsg

#endif // _RSGGENERATORSTATE_HPP
