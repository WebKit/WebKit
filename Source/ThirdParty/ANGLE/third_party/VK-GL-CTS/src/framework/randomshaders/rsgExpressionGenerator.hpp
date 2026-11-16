#ifndef _RSGEXPRESSIONGENERATOR_HPP
#define _RSGEXPRESSIONGENERATOR_HPP
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

#include "rsgDefs.hpp"
#include "rsgVariableValue.hpp"
#include "rsgGeneratorState.hpp"
#include "rsgExpression.hpp"

#include <vector>

namespace rsg
{

class ExpressionGenerator
{
public:
    ExpressionGenerator(GeneratorState &state);
    ~ExpressionGenerator(void);

    Expression *generate(const ValueRange &valueRange, int initialDepth = 0);

private:
    void generate(Expression *root);

    GeneratorState &m_state;
    std::vector<Expression *> m_expressionStack;
};

} // namespace rsg

#endif // _RSGEXPRESSIONGENERATOR_HPP
