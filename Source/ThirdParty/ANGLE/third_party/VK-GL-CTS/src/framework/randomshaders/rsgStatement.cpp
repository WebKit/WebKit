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

#include "rsgStatement.hpp"
#include "rsgExpressionGenerator.hpp"
#include "rsgUtils.hpp"

#include <typeinfo>

using std::vector;

namespace rsg
{

namespace
{

inline bool isCurrentTopStatementBlock(const GeneratorState &state)
{
    int stackDepth = state.getStatementDepth();
    return dynamic_cast<const BlockStatement *>(state.getStatementStackEntry(stackDepth - 1)) != nullptr;
}

template <class T>
float getWeight(const GeneratorState &state)
{
    return T::getWeight(state);
}
template <class T>
Statement *create(GeneratorState &state)
{
    return new T(state);
}

struct StatementSpec
{
    float (*getWeight)(const GeneratorState &state);
    Statement *(*create)(GeneratorState &state);
};

const StatementSpec *chooseStatement(GeneratorState &state)
{
    static const StatementSpec statementSpecs[] = {{getWeight<BlockStatement>, create<BlockStatement>},
                                                   {getWeight<ExpressionStatement>, create<ExpressionStatement>},
                                                   {getWeight<DeclarationStatement>, create<DeclarationStatement>},
                                                   {getWeight<ConditionalStatement>, create<ConditionalStatement>}};

    float weights[DE_LENGTH_OF_ARRAY(statementSpecs)];

    // Compute weights
    float sum = 0.0f;
    for (int ndx = 0; ndx < (int)DE_LENGTH_OF_ARRAY(statementSpecs); ndx++)
    {
        weights[ndx] = statementSpecs[ndx].getWeight(state);
        sum += weights[ndx];
    }

    DE_ASSERT(sum > 0.0f);

    // Random number in range
    float p = state.getRandom().getFloat(0.0f, sum);

    const StatementSpec *spec        = nullptr;
    const StatementSpec *lastNonZero = nullptr;

    // Find element in that point
    sum = 0.0f;
    for (int ndx = 0; ndx < (int)DE_LENGTH_OF_ARRAY(statementSpecs); ndx++)
    {
        sum += weights[ndx];
        if (p < sum)
        {
            spec = &statementSpecs[ndx];
            break;
        }
        else if (weights[ndx] > 0.0f)
            lastNonZero = &statementSpecs[ndx];
    }

    if (!spec)
        spec = lastNonZero;

    return spec;
}

Statement *createStatement(GeneratorState &state)
{
    return chooseStatement(state)->create(state);
}

} // namespace

Statement::Statement(void)
{
}

Statement::~Statement(void)
{
}

ExpressionStatement::ExpressionStatement(GeneratorState &state) : m_expression(nullptr)
{
    ExpressionGenerator generator(state);
    m_expression = generator.generate(ValueRange(VariableType(VariableType::TYPE_VOID)));
}

ExpressionStatement::~ExpressionStatement(void)
{
    delete m_expression;
}

float ExpressionStatement::getWeight(const GeneratorState &state)
{
    DE_UNREF(state);
    return 1.0f;
}

void ExpressionStatement::execute(ExecutionContext &execCtx) const
{
    m_expression->evaluate(execCtx);
}

BlockStatement::BlockStatement(GeneratorState &state)
{
    init(state);
}

void BlockStatement::init(GeneratorState &state)
{
    // Select number of children statements to construct
    m_numChildrenToCreate = state.getRandom().getInt(0, state.getShaderParameters().maxStatementsPerBlock);

    // Push scope
    state.getVariableManager().pushVariableScope(m_scope);
}

BlockStatement::~BlockStatement(void)
{
    for (vector<Statement *>::iterator i = m_children.begin(); i != m_children.end(); i++)
        delete *i;
    m_children.clear();
}

void BlockStatement::addChild(Statement *statement)
{
    try
    {
        m_children.push_back(statement);
    }
    catch (const std::exception &)
    {
        delete statement;
        throw;
    }
}

Statement *BlockStatement::createNextChild(GeneratorState &state)
{
    if ((int)m_children.size() < m_numChildrenToCreate)
    {
        // Select and create a new child
        Statement *child = createStatement(state);
        addChild(child);
        return child;
    }
    else
    {
        // Done, pop scope
        state.getVariableManager().popVariableScope();
        return nullptr;
    }
}

float BlockStatement::getWeight(const GeneratorState &state)
{
    if (state.getStatementDepth() + 1 < state.getShaderParameters().maxStatementDepth)
    {
        if (isCurrentTopStatementBlock(state))
            return 0.2f; // Low probability for anonymous blocks.
        else
            return 1.0f;
    }
    else
        return 0.0f;
}

void BlockStatement::tokenize(GeneratorState &state, TokenStream &str) const
{
    str << Token::LEFT_BRACE << Token::NEWLINE << Token::INDENT_INC;

    for (vector<Statement *>::const_reverse_iterator i = m_children.rbegin(); i != m_children.rend(); i++)
        (*i)->tokenize(state, str);

    str << Token::INDENT_DEC << Token::RIGHT_BRACE << Token::NEWLINE;
}

void BlockStatement::execute(ExecutionContext &execCtx) const
{
    for (vector<Statement *>::const_reverse_iterator i = m_children.rbegin(); i != m_children.rend(); i++)
        (*i)->execute(execCtx);
}

void ExpressionStatement::tokenize(GeneratorState &state, TokenStream &str) const
{
    DE_ASSERT(m_expression);
    m_expression->tokenize(state, str);
    str << Token::SEMICOLON << Token::NEWLINE;
}

namespace
{

inline bool canDeclareVariable(const Variable *variable)
{
    return variable->getStorage() == Variable::STORAGE_LOCAL;
}

bool hasDeclarableVars(const VariableManager &varMgr)
{
    const vector<Variable *> &liveVars = varMgr.getLiveVariables();
    for (vector<Variable *>::const_iterator i = liveVars.begin(); i != liveVars.end(); i++)
    {
        if (canDeclareVariable(*i))
            return true;
    }
    return false;
}

} // namespace

DeclarationStatement::DeclarationStatement(GeneratorState &state, Variable *variable)
    : m_variable(nullptr)
    , m_expression(nullptr)
{
    if (variable == nullptr)
    {
        // Choose random
        // \todo [2011-02-03 pyry] Allocate a new here?
        // \todo [2011-05-26 pyry] Weights?
        const vector<Variable *> &liveVars = state.getVariableManager().getLiveVariables();
        vector<Variable *> candidates;

        for (vector<Variable *>::const_iterator i = liveVars.begin(); i != liveVars.end(); i++)
        {
            if (canDeclareVariable(*i))
                candidates.push_back(*i);
        }

        variable = state.getRandom().choose<Variable *>(candidates.begin(), candidates.end());
    }

    DE_ASSERT(variable);
    m_variable = variable;

    const ValueEntry *value = state.getVariableManager().getValue(variable);

    bool createInitializer = false;

    switch (m_variable->getStorage())
    {
    case Variable::STORAGE_CONST:
        DE_ASSERT(value);
        createInitializer = true;
        break;

    case Variable::STORAGE_LOCAL:
        // initializer is always created if value isn't null.
        createInitializer = value;
        break;

    default:
        createInitializer = false;
        break;
    }

    if (createInitializer)
    {
        ExpressionGenerator generator(state);

        // Take copy of value range for generating initializer expression
        ValueRange valueRange = value->getValueRange();

        // Declare (removes value entry)
        state.getVariableManager().declareVariable(variable);

        bool isConst = m_variable->getStorage() == Variable::STORAGE_CONST;

        if (isConst)
            state.pushExpressionFlags(state.getExpressionFlags() | CONST_EXPR);

        m_expression = generator.generate(valueRange, 1);

        if (isConst)
            state.popExpressionFlags();
    }
    else
        state.getVariableManager().declareVariable(variable);
}

DeclarationStatement::~DeclarationStatement(void)
{
    delete m_expression;
}

float DeclarationStatement::getWeight(const GeneratorState &state)
{
    if (!hasDeclarableVars(state.getVariableManager()))
        return 0.0f;

    if (!isCurrentTopStatementBlock(state))
        return 0.0f;

    return state.getProgramParameters().declarationStatementBaseWeight;
}

void DeclarationStatement::tokenize(GeneratorState &state, TokenStream &str) const
{
    m_variable->tokenizeDeclaration(state, str);

    if (m_expression)
    {
        str << Token::EQUAL;
        m_expression->tokenize(state, str);
    }

    str << Token::SEMICOLON << Token::NEWLINE;
}

void DeclarationStatement::execute(ExecutionContext &execCtx) const
{
    if (m_expression)
    {
        m_expression->evaluate(execCtx);
        execCtx.getValue(m_variable) = m_expression->getValue().value();
    }
}

ConditionalStatement::ConditionalStatement(GeneratorState &)
    : m_condition(nullptr)
    , m_trueStatement(nullptr)
    , m_falseStatement(nullptr)
{
}

ConditionalStatement::~ConditionalStatement(void)
{
    delete m_condition;
    delete m_trueStatement;
    delete m_falseStatement;
}

bool ConditionalStatement::isElseBlockRequired(const GeneratorState &state) const
{
    // If parent is conditional statement with else block and this is the true statement,
    // else block must be generated or otherwise parent "else" will end up parsed as else statement for this if.
    const ConditionalStatement *curChild = this;
    int curStackNdx                      = state.getStatementDepth() - 2;

    while (curStackNdx >= 0)
    {
        const ConditionalStatement *curParent =
            dynamic_cast<const ConditionalStatement *>(state.getStatementStackEntry(curStackNdx));

        if (!curParent)
            break; // Not a conditional statement - can end search here.

        if (curChild == curParent->m_trueStatement && curParent->m_falseStatement)
            return true; // Else block is mandatory.

        // Continue search.
        curChild = curParent;
        curStackNdx -= 1;
    }

    return false;
}

Statement *ConditionalStatement::createNextChild(GeneratorState &state)
{
    // If has neither true or false statements, choose randomly whether to create false block.
    if (!m_falseStatement && !m_trueStatement && (isElseBlockRequired(state) || state.getRandom().getBool()))
    {
        // Construct false statement
        state.getVariableManager().pushValueScope(m_conditionalScope);
        m_falseStatement = createStatement(state);

        return m_falseStatement;
    }
    else if (!m_trueStatement)
    {
        if (m_falseStatement)
        {
            // Pop previous value scope.
            state.getVariableManager().popValueScope();
            m_conditionalScope.clear();
        }

        // Construct true statement
        state.getVariableManager().pushValueScope(m_conditionalScope);
        m_trueStatement = createStatement(state);

        return m_trueStatement;
    }
    else
    {
        // Pop conditional scope.
        state.getVariableManager().popValueScope();
        m_conditionalScope.clear();

        // Create condition
        DE_ASSERT(!m_condition);

        ExpressionGenerator generator(state);

        ValueRange range        = ValueRange(VariableType::getScalarType(VariableType::TYPE_BOOL));
        range.getMin().asBool() = false;
        range.getMax().asBool() = true;

        m_condition = generator.generate(range, 1);

        return nullptr; // Done with this statement
    }
}

namespace
{

bool isBlockStatement(const Statement *statement)
{
    return dynamic_cast<const BlockStatement *>(statement) != nullptr;
}

bool isConditionalStatement(const Statement *statement)
{
    return dynamic_cast<const ConditionalStatement *>(statement) != nullptr;
}

} // namespace

void ConditionalStatement::tokenize(GeneratorState &state, TokenStream &str) const
{
    DE_ASSERT(m_condition && m_trueStatement);

    // if (condition)
    str << Token::IF << Token::LEFT_PAREN;
    m_condition->tokenize(state, str);
    str << Token::RIGHT_PAREN << Token::NEWLINE;

    // Statement executed if true
    if (!isBlockStatement(m_trueStatement))
    {
        str << Token::INDENT_INC;
        m_trueStatement->tokenize(state, str);
        str << Token::INDENT_DEC;
    }
    else
        m_trueStatement->tokenize(state, str);

    if (m_falseStatement)
    {
        str << Token::ELSE;

        if (isConditionalStatement(m_falseStatement))
        {
            m_falseStatement->tokenize(state, str);
        }
        else if (isBlockStatement(m_falseStatement))
        {
            str << Token::NEWLINE;
            m_falseStatement->tokenize(state, str);
        }
        else
        {
            str << Token::NEWLINE << Token::INDENT_INC;
            m_falseStatement->tokenize(state, str);
            str << Token::INDENT_DEC;
        }
    }
}

void ConditionalStatement::execute(ExecutionContext &execCtx) const
{
    // Evaluate condition
    m_condition->evaluate(execCtx);

    ExecMaskStorage maskStorage; // Value might change when we are evaluating true block so we have to take a copy.
    ExecValueAccess trueMask = maskStorage.getValue();

    trueMask = m_condition->getValue().value();

    // And mask, execute true statement and pop
    execCtx.andExecutionMask(trueMask);
    m_trueStatement->execute(execCtx);
    execCtx.popExecutionMask();

    if (m_falseStatement)
    {
        // Construct negated mask, execute false statement and pop
        ExecMaskStorage tmp;
        ExecValueAccess falseMask = tmp.getValue();

        for (int i = 0; i < EXEC_VEC_WIDTH; i++)
            falseMask.asBool(i) = !trueMask.asBool(i);

        execCtx.andExecutionMask(falseMask);
        m_falseStatement->execute(execCtx);
        execCtx.popExecutionMask();
    }
}

float ConditionalStatement::getWeight(const GeneratorState &state)
{
    if (!state.getProgramParameters().useConditionals)
        return 0.0f;

    int availableLevels = state.getShaderParameters().maxStatementDepth - state.getStatementDepth();
    return (availableLevels > 1) ? 1.0f : 0.0f;
}

AssignStatement::AssignStatement(const Variable *variable, Expression *value)
    : m_variable(variable)
    , m_valueExpr(value) // \note Takes ownership of value
{
}

AssignStatement::AssignStatement(GeneratorState &state, const Variable *variable, ConstValueRangeAccess valueRange)
    : m_variable(variable)
    , m_valueExpr(nullptr)
{
    // Generate random value
    ExpressionGenerator generator(state);
    m_valueExpr = generator.generate(valueRange, 1);
}

AssignStatement::~AssignStatement(void)
{
    delete m_valueExpr;
}

void AssignStatement::tokenize(GeneratorState &state, TokenStream &str) const
{
    str << Token(m_variable->getName()) << Token::EQUAL;
    m_valueExpr->tokenize(state, str);
    str << Token::SEMICOLON << Token::NEWLINE;
}

void AssignStatement::execute(ExecutionContext &execCtx) const
{
    m_valueExpr->evaluate(execCtx);
    assignMasked(execCtx.getValue(m_variable), m_valueExpr->getValue(), execCtx.getExecutionMask());
}

} // namespace rsg
