#ifndef _RSGSHADER_HPP
#define _RSGSHADER_HPP
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

#include "rsgDefs.hpp"
#include "rsgVariable.hpp"
#include "rsgStatement.hpp"
#include "rsgVariableManager.hpp"
#include "rsgToken.hpp"
#include "rsgExecutionContext.hpp"

#include <vector>
#include <string>

namespace rsg
{

class Function
{
public:
    Function(void);
    Function(const char *name);
    ~Function(void);

    const VariableType &getReturnType(void) const
    {
        return m_returnType;
    }
    void setReturnType(const VariableType &type)
    {
        m_returnType = type;
    }

    void addParameter(Variable *variable);

    BlockStatement &getBody(void)
    {
        return m_functionBlock;
    }
    const BlockStatement &getBody(void) const
    {
        return m_functionBlock;
    }

    void tokenize(GeneratorState &state, TokenStream &stream) const;

private:
    std::string m_name;
    std::vector<Variable *> m_parameters;
    VariableType m_returnType;

    BlockStatement m_functionBlock;
};

class ShaderInput
{
public:
    ShaderInput(const Variable *variable, ConstValueRangeAccess valueRange);
    ~ShaderInput(void)
    {
    }

    const Variable *getVariable(void) const
    {
        return m_variable;
    }
    ConstValueRangeAccess getValueRange(void) const
    {
        return ConstValueRangeAccess(m_variable->getType(), &m_min[0], &m_max[0]);
    }
    ValueRangeAccess getValueRange(void)
    {
        return ValueRangeAccess(m_variable->getType(), &m_min[0], &m_max[0]);
    }

private:
    const Variable *m_variable;
    std::vector<Scalar> m_min;
    std::vector<Scalar> m_max;
};

class Shader
{
public:
    enum Type
    {
        TYPE_VERTEX = 0,
        TYPE_FRAGMENT,

        TYPE_LAST
    };

    Shader(Type type);
    ~Shader(void);

    Type getType(void) const
    {
        return m_type;
    }
    const char *getSource(void) const
    {
        return m_source.c_str();
    }

    void execute(ExecutionContext &execCtx) const;

    // For generator implementation only
    Function &getMain(void)
    {
        return m_mainFunction;
    }
    Function &allocateFunction(void);

    VariableScope &getGlobalScope(void)
    {
        return m_globalScope;
    }
    std::vector<Statement *> &getGlobalStatements(void)
    {
        return m_globalStatements;
    }

    void tokenize(GeneratorState &state, TokenStream &str) const;
    void setSource(const char *source)
    {
        m_source = source;
    }

    std::vector<ShaderInput *> &getInputs(void)
    {
        return m_inputs;
    }
    std::vector<ShaderInput *> &getUniforms(void)
    {
        return m_uniforms;
    }

    // For executor
    const std::vector<ShaderInput *> &getInputs(void) const
    {
        return m_inputs;
    }
    const std::vector<ShaderInput *> &getUniforms(void) const
    {
        return m_uniforms;
    }
    void getOutputs(std::vector<const Variable *> &outputs) const;

private:
    Type m_type;

    VariableScope m_globalScope;
    std::vector<Statement *> m_globalStatements;

    std::vector<ShaderInput *> m_inputs;
    std::vector<ShaderInput *> m_uniforms;

    std::vector<Function *> m_functions;
    Function m_mainFunction;

    std::string m_source;
};

} // namespace rsg

#endif // _RSGSHADER_HPP
