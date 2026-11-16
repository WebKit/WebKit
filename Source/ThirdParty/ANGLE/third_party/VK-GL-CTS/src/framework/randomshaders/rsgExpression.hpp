#ifndef _RSGEXPRESSION_HPP
#define _RSGEXPRESSION_HPP
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
 * \brief Expressions.
 *
 * Creating expressions:
 *  + Children must be created in in reverse evaluation order.
 *    - Must be tokenized / evaluated taking that order in account.
 *
 * Evaluation:
 *  + Done recursively. (Do we have enough stack?)
 *  + R-values: Nodes must implement getValue() in some way. Value
 *    must be valid after evaluate().
 *  + L-values: Valid writable value access proxy must be returned after
 *    evaluate().
 *//*--------------------------------------------------------------------*/

#include "rsgDefs.hpp"
#include "rsgGeneratorState.hpp"
#include "rsgVariableValue.hpp"
#include "rsgVariable.hpp"
#include "rsgVariableManager.hpp"
#include "rsgExecutionContext.hpp"

namespace rsg
{

// \todo [2011-06-10 pyry] Declare in ShaderParameters?
const float unusedValueWeight = 0.05f;

class Expression
{
public:
    virtual ~Expression(void);

    // Shader generation API
    virtual Expression *createNextChild(GeneratorState &state)           = 0;
    virtual void tokenize(GeneratorState &state, TokenStream &str) const = 0;

    // Execution API
    virtual void evaluate(ExecutionContext &ctx)      = 0;
    virtual ExecConstValueAccess getValue(void) const = 0;
    virtual ExecValueAccess getLValue(void) const
    {
        DE_ASSERT(false);
        throw Exception("Expression::getLValue(): not L-value node");
    }

    static Expression *createRandom(GeneratorState &state, ConstValueRangeAccess valueRange);
    static Expression *createRandomLValue(GeneratorState &state, ConstValueRangeAccess valueRange);
};

class VariableAccess : public Expression
{
public:
    virtual ~VariableAccess(void)
    {
    }

    Expression *createNextChild(GeneratorState &state)
    {
        DE_UNREF(state);
        return nullptr;
    }
    void tokenize(GeneratorState &state, TokenStream &str) const
    {
        DE_UNREF(state);
        str << Token(m_variable->getName());
    }

    void evaluate(ExecutionContext &ctx);
    ExecConstValueAccess getValue(void) const
    {
        return m_valueAccess;
    }
    ExecValueAccess getLValue(void) const
    {
        return m_valueAccess;
    }

protected:
    VariableAccess(void) : m_variable(nullptr)
    {
    }

    const Variable *m_variable;
    ExecValueAccess m_valueAccess;
};

class VariableRead : public VariableAccess
{
public:
    VariableRead(GeneratorState &state, ConstValueRangeAccess valueRange);
    VariableRead(const Variable *variable);
    virtual ~VariableRead(void)
    {
    }

    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);
};

class VariableWrite : public VariableAccess
{
public:
    VariableWrite(GeneratorState &state, ConstValueRangeAccess valueRange);
    virtual ~VariableWrite(void)
    {
    }

    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);
};

class FloatLiteral : public Expression
{
public:
    FloatLiteral(GeneratorState &state, ConstValueRangeAccess valueRange);
    FloatLiteral(float customValue);
    virtual ~FloatLiteral(void)
    {
    }

    Expression *createNextChild(GeneratorState &state)
    {
        DE_UNREF(state);
        return nullptr;
    }
    void tokenize(GeneratorState &state, TokenStream &str) const;

    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);

    void evaluate(ExecutionContext &ctx)
    {
        DE_UNREF(ctx);
    }
    ExecConstValueAccess getValue(void) const
    {
        return m_value.getValue(VariableType::getScalarType(VariableType::TYPE_FLOAT));
    }

private:
    ExecValueStorage m_value;
};

class IntLiteral : public Expression
{
public:
    IntLiteral(GeneratorState &state, ConstValueRangeAccess valueRange);
    virtual ~IntLiteral(void)
    {
    }

    Expression *createNextChild(GeneratorState &state)
    {
        DE_UNREF(state);
        return nullptr;
    }
    void tokenize(GeneratorState &state, TokenStream &str) const;

    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);

    void evaluate(ExecutionContext &ctx)
    {
        DE_UNREF(ctx);
    }
    ExecConstValueAccess getValue(void) const
    {
        return m_value.getValue(VariableType::getScalarType(VariableType::TYPE_INT));
    }

private:
    ExecValueStorage m_value;
};

class BoolLiteral : public Expression
{
public:
    BoolLiteral(GeneratorState &state, ConstValueRangeAccess valueRange);
    BoolLiteral(bool customValue);
    virtual ~BoolLiteral(void)
    {
    }

    Expression *createNextChild(GeneratorState &state)
    {
        DE_UNREF(state);
        return nullptr;
    }
    void tokenize(GeneratorState &state, TokenStream &str) const;

    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);

    void evaluate(ExecutionContext &ctx)
    {
        DE_UNREF(ctx);
    }
    ExecConstValueAccess getValue(void) const
    {
        return m_value.getValue(VariableType::getScalarType(VariableType::TYPE_BOOL));
    }

private:
    ExecValueStorage m_value;
};

class ConstructorOp : public Expression
{
public:
    ConstructorOp(GeneratorState &state, ConstValueRangeAccess valueRange);
    virtual ~ConstructorOp(void);

    Expression *createNextChild(GeneratorState &state);
    void tokenize(GeneratorState &state, TokenStream &str) const;

    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);

    void evaluate(ExecutionContext &ctx);
    ExecConstValueAccess getValue(void) const
    {
        return m_value.getValue(m_valueRange.getType());
    }

private:
    ValueRange m_valueRange;
    ExecValueStorage m_value;

    std::vector<ValueRange> m_inputValueRanges;
    std::vector<Expression *> m_inputExpressions;
};

class AssignOp : public Expression
{
public:
    AssignOp(GeneratorState &state, ConstValueRangeAccess valueRange);
    virtual ~AssignOp(void);

    Expression *createNextChild(GeneratorState &state);
    void tokenize(GeneratorState &state, TokenStream &str) const;

    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);

    // \todo [2011-02-28 pyry] LValue variant of AssignOp
    // static float getLValueWeight (const GeneratorState& state, ConstValueRangeAccess valueRange);

    void evaluate(ExecutionContext &ctx);
    ExecConstValueAccess getValue(void) const
    {
        return m_value.getValue(m_valueRange.getType());
    }

private:
    ValueRange m_valueRange;
    ExecValueStorage m_value;

    Expression *m_lvalueExpr;
    Expression *m_rvalueExpr;
};

class ParenOp : public Expression
{
public:
    ParenOp(GeneratorState &state, ConstValueRangeAccess valueRange);
    virtual ~ParenOp(void);

    Expression *createNextChild(GeneratorState &state);
    void tokenize(GeneratorState &state, TokenStream &str) const;

    void setChild(Expression *expression);
    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);

    void evaluate(ExecutionContext &execCtx)
    {
        m_child->evaluate(execCtx);
    }
    ExecConstValueAccess getValue(void) const
    {
        return m_child->getValue();
    }

private:
    ValueRange m_valueRange;
    Expression *m_child;
};

class SwizzleOp : public Expression
{
public:
    SwizzleOp(GeneratorState &state, ConstValueRangeAccess valueRange);
    virtual ~SwizzleOp(void);

    Expression *createNextChild(GeneratorState &state);
    void tokenize(GeneratorState &state, TokenStream &str) const;

    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);

    void evaluate(ExecutionContext &execCtx);
    ExecConstValueAccess getValue(void) const
    {
        return m_value.getValue(m_outValueRange.getType());
    }

private:
    ValueRange m_outValueRange;
    int m_numInputElements;
    uint8_t m_swizzle[4];
    Expression *m_child;
    ExecValueStorage m_value;
};

class TexLookup : public Expression
{
public:
    TexLookup(GeneratorState &state, ConstValueRangeAccess valueRange);
    virtual ~TexLookup(void);

    Expression *createNextChild(GeneratorState &state);
    void tokenize(GeneratorState &state, TokenStream &str) const;

    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);

    void evaluate(ExecutionContext &execCtx);
    ExecConstValueAccess getValue(void) const
    {
        return m_value.getValue(m_valueType);
    }

private:
    enum Type
    {
        TYPE_TEXTURE2D,
        TYPE_TEXTURE2D_LOD,
        TYPE_TEXTURE2D_PROJ,
        TYPE_TEXTURE2D_PROJ_LOD,

        TYPE_TEXTURECUBE,
        TYPE_TEXTURECUBE_LOD,

        TYPE_LAST
    };

    Type m_type;
    const Variable *m_sampler;
    Expression *m_coordExpr;
    Expression *m_lodBiasExpr;
    VariableType m_valueType;
    ExecValueStorage m_value;
};

} // namespace rsg

#endif // _RSGEXPRESSION_HPP
