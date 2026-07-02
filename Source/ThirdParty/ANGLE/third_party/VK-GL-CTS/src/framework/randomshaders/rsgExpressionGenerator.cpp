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

#include "rsgExpressionGenerator.hpp"

namespace rsg
{

ExpressionGenerator::ExpressionGenerator(GeneratorState &state) : m_state(state)
{
}

ExpressionGenerator::~ExpressionGenerator(void)
{
}

Expression *ExpressionGenerator::generate(const ValueRange &valueRange, int initialDepth)
{
    // Create root
    m_state.setExpressionDepth(initialDepth);
    Expression *root = Expression::createRandom(m_state, valueRange);

    try
    {
        // Generate full expression
        generate(root);
    }
    catch (const std::exception &)
    {
        delete root;
        m_expressionStack.clear();
        throw;
    }

    return root;
}

void ExpressionGenerator::generate(Expression *root)
{
    DE_ASSERT(m_expressionStack.empty());

    // Initialize stack
    m_expressionStack.push_back(root);
    m_state.setExpressionDepth(m_state.getExpressionDepth() + 1);

    // Process until done
    while (!m_expressionStack.empty())
    {
        DE_ASSERT(m_state.getExpressionDepth() <= m_state.getShaderParameters().maxExpressionDepth);

        Expression *curExpr = m_expressionStack[m_expressionStack.size() - 1];
        Expression *child   = curExpr->createNextChild(m_state);

        if (child)
        {
            m_expressionStack.push_back(child);
            m_state.setExpressionDepth(m_state.getExpressionDepth() + 1);
        }
        else
        {
            m_expressionStack.pop_back();
            m_state.setExpressionDepth(m_state.getExpressionDepth() - 1);
        }
    }
}

} // namespace rsg
