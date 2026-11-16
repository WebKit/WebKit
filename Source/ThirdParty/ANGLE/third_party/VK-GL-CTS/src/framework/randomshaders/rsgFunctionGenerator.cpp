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
 * \brief Expression generator.
 *//*--------------------------------------------------------------------*/

#include "rsgFunctionGenerator.hpp"
#include "rsgUtils.hpp"

using std::vector;

namespace rsg
{

FunctionGenerator::FunctionGenerator(GeneratorState &state, Function &function) : m_state(state), m_function(function)
{
}

FunctionGenerator::~FunctionGenerator(void)
{
}

void FunctionGenerator::generate(void)
{
    std::vector<Statement *> statementStack;

    // Initialize
    m_state.setStatementStack(statementStack);
    statementStack.push_back(&m_function.getBody());
    m_function.getBody().init(m_state);

    // Process until statement stack is empty
    while (!statementStack.empty())
    {
        DE_ASSERT((int)statementStack.size() <= m_state.getShaderParameters().maxStatementDepth);

        Statement *curStatement   = statementStack.back();
        Statement *childStatement = curStatement->createNextChild(m_state);

        if (childStatement)
            statementStack.push_back(childStatement);
        else
            statementStack.pop_back();
    }

    // Create assignments if variables have bound value range
    for (vector<Variable *>::iterator i = m_requiredAssignments.begin(); i != m_requiredAssignments.end(); i++)
    {
        Variable *variable      = *i;
        const ValueEntry *entry = m_state.getVariableManager().getValue(variable);
        ValueRange valueRange(variable->getType());

        valueRange.getMin() = entry->getValueRange().getMin().value();
        valueRange.getMax() = entry->getValueRange().getMax().value();

        // Remove value entry from this scope. After this entry ptr is invalid.
        m_state.getVariableManager().removeValueFromCurrentScope(variable);

        if (!isUndefinedValueRange(valueRange.asAccess()))
            m_function.getBody().addChild(new AssignStatement(m_state, variable, valueRange.asAccess()));
    }
}

} // namespace rsg
