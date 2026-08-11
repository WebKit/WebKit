#ifndef _RSGSTATEMENT_HPP
#define _RSGSTATEMENT_HPP
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
 * \brief Statements.
 *//*--------------------------------------------------------------------*/

#include "rsgDefs.hpp"
#include "rsgGeneratorState.hpp"
#include "rsgVariableManager.hpp"
#include "rsgExpression.hpp"
#include "rsgToken.hpp"

#include <vector>

namespace rsg
{

class Statement
{
public:
    Statement(void);
    virtual ~Statement(void);

    virtual Statement *createNextChild(GeneratorState &state)            = 0;
    virtual void tokenize(GeneratorState &state, TokenStream &str) const = 0;
    virtual void execute(ExecutionContext &execCtx) const                = 0;

protected:
};

// IfStatement
// ForLoopStatement
// WhileStatement
// DoWhileStatement

class ExpressionStatement : public Statement
{
public:
    ExpressionStatement(GeneratorState &state);
    virtual ~ExpressionStatement(void);

    Statement *createNextChild(GeneratorState &state)
    {
        DE_UNREF(state);
        return nullptr;
    }
    void tokenize(GeneratorState &state, TokenStream &str) const;
    void execute(ExecutionContext &execCtx) const;

    static float getWeight(const GeneratorState &state);

protected:
    Expression *m_expression;
};

class DeclarationStatement : public Statement
{
public:
    DeclarationStatement(GeneratorState &state, Variable *variable = nullptr);
    virtual ~DeclarationStatement(void);

    Statement *createNextChild(GeneratorState &state)
    {
        DE_UNREF(state);
        return nullptr;
    }
    void tokenize(GeneratorState &state, TokenStream &str) const;
    void execute(ExecutionContext &execCtx) const;

    static float getWeight(const GeneratorState &state);

protected:
    const Variable *m_variable;
    Expression *m_expression;
};

class BlockStatement : public Statement
{
public:
    BlockStatement(GeneratorState &state);
    virtual ~BlockStatement(void);

    BlockStatement(void) : m_numChildrenToCreate(0)
    {
    }
    void init(GeneratorState &state);

    Statement *createNextChild(GeneratorState &state);
    void tokenize(GeneratorState &state, TokenStream &str) const;
    void execute(ExecutionContext &execCtx) const;

    static float getWeight(const GeneratorState &state);

    void addChild(Statement *statement);

private:
    VariableScope m_scope;

    int m_numChildrenToCreate;
    std::vector<Statement *> m_children;
};

class ConditionalStatement : public Statement
{
public:
    ConditionalStatement(GeneratorState &state);
    virtual ~ConditionalStatement(void);

    Statement *createNextChild(GeneratorState &state);
    void tokenize(GeneratorState &state, TokenStream &str) const;
    void execute(ExecutionContext &execCtx) const;

    static float getWeight(const GeneratorState &state);

private:
    bool isElseBlockRequired(const GeneratorState &state) const;

    Expression *m_condition;
    Statement *m_trueStatement;
    Statement *m_falseStatement;

    ValueScope m_conditionalScope;
};

// \note Used for generating mandatory assignments (shader outputs, function outputs).
//       Normally assignment is handled inside ExpressionStatement where generator may
//       choose to generate AssignOp expression.
class AssignStatement : public Statement
{
public:
    AssignStatement(const Variable *variable, Expression *value);
    AssignStatement(GeneratorState &state, const Variable *variable, ConstValueRangeAccess valueRange);
    virtual ~AssignStatement(void);

    Statement *createNextChild(GeneratorState &state)
    {
        DE_UNREF(state);
        return nullptr;
    }
    void tokenize(GeneratorState &state, TokenStream &str) const;
    void execute(ExecutionContext &execCtx) const;

private:
    const Variable *m_variable;
    Expression *m_valueExpr;
};

} // namespace rsg

#endif // _RSGSTATEMENT_HPP
