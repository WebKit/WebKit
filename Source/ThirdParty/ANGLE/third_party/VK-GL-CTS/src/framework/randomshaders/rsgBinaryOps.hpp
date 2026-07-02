#ifndef _RSGBINARYOPS_HPP
#define _RSGBINARYOPS_HPP
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
 * \brief Binary operators.
 *//*--------------------------------------------------------------------*/

#include "rsgDefs.hpp"
#include "rsgExpression.hpp"

namespace rsg
{

enum Associativity
{
    ASSOCIATIVITY_LEFT = 0,
    ASSOCIATIVITY_RIGHT,

    ASSOCIATIVITY_LAST
};

template <int Precedence, Associativity Assoc>
class BinaryOp : public Expression
{
public:
    BinaryOp(Token::Type operatorToken);
    virtual ~BinaryOp(void);

    Expression *createNextChild(GeneratorState &state);
    void tokenize(GeneratorState &state, TokenStream &str) const;
    void evaluate(ExecutionContext &execCtx);
    ExecConstValueAccess getValue(void) const
    {
        return m_value.getValue(m_type);
    }

    virtual void evaluate(ExecValueAccess dst, ExecConstValueAccess a, ExecConstValueAccess b) = 0;

protected:
    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);

    Token::Type m_operator;
    VariableType m_type;
    ExecValueStorage m_value;

    ValueRange m_leftValueRange;
    ValueRange m_rightValueRange;

    Expression *m_leftValueExpr;
    Expression *m_rightValueExpr;
};

template <int Precedence, bool Float, bool Int, bool Bool, class ComputeValueRange, class EvaluateComp>
class BinaryVecOp : public BinaryOp<Precedence, ASSOCIATIVITY_LEFT>
{
public:
    BinaryVecOp(GeneratorState &state, Token::Type operatorToken, ConstValueRangeAccess valueRange);
    virtual ~BinaryVecOp(void);

    void evaluate(ExecValueAccess dst, ExecConstValueAccess a, ExecConstValueAccess b);
};

struct ComputeMulRange
{
    void operator()(de::Random &rnd, float dstMin, float dstMax, float &aMin, float &aMax, float &bMin,
                    float &bMax) const;
    void operator()(de::Random &rnd, int dstMin, int dstMax, int &aMin, int &aMax, int &bMin, int &bMax) const;
    void operator()(de::Random &, bool, bool, bool &, bool &, bool &, bool &) const
    {
        DE_ASSERT(false);
    }
};

struct EvaluateMul
{
    template <typename T>
    inline T operator()(T a, T b) const
    {
        return a * b;
    }
};

typedef BinaryVecOp<4, true, true, false, ComputeMulRange, EvaluateMul> MulBase;

class MulOp : public MulBase
{
public:
    MulOp(GeneratorState &state, ConstValueRangeAccess valueRange);
    virtual ~MulOp(void)
    {
    }

    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);
};

struct ComputeAddRange
{
    template <typename T>
    void operator()(de::Random &rnd, T dstMin, T dstMax, T &aMin, T &aMax, T &bMin, T &bMax) const;
};

struct EvaluateAdd
{
    template <typename T>
    inline T operator()(T a, T b) const
    {
        return a + b;
    }
};

typedef BinaryVecOp<5, true, true, false, ComputeAddRange, EvaluateAdd> AddBase;

class AddOp : public AddBase
{
public:
    AddOp(GeneratorState &state, ConstValueRangeAccess valueRange);
    virtual ~AddOp(void)
    {
    }

    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);
};

struct ComputeSubRange
{
    template <typename T>
    void operator()(de::Random &rnd, T dstMin, T dstMax, T &aMin, T &aMax, T &bMin, T &bMax) const;
};

struct EvaluateSub
{
    template <typename T>
    inline T operator()(T a, T b) const
    {
        return a - b;
    }
};

typedef BinaryVecOp<5, true, true, false, ComputeSubRange, EvaluateSub> SubBase;

class SubOp : public SubBase
{
public:
    SubOp(GeneratorState &state, ConstValueRangeAccess valueRange);
    virtual ~SubOp(void)
    {
    }

    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);
};

/* Template for Relational Operators. */

template <class ComputeValueRange, class EvaluateComp>
class RelationalOp : public BinaryOp<7, ASSOCIATIVITY_LEFT>
{
public:
    RelationalOp(GeneratorState &state, Token::Type operatorToken, ConstValueRangeAccess valueRange);
    virtual ~RelationalOp(void);

    void evaluate(ExecValueAccess dst, ExecConstValueAccess a, ExecConstValueAccess b);

    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);
};

/* Less Than. */

struct ComputeLessThanRange
{
    template <typename T>
    void operator()(de::Random &rnd, bool dstMin, bool dstMax, T &aMin, T &aMax, T &bMin, T &bMax) const;
};

struct EvaluateLessThan
{
    template <typename T>
    inline bool operator()(T a, T b) const
    {
        return a < b;
    }
};

typedef RelationalOp<ComputeLessThanRange, EvaluateLessThan> LessThanBase;

class LessThanOp : public LessThanBase
{
public:
    LessThanOp(GeneratorState &state, ConstValueRangeAccess valueRange);
    virtual ~LessThanOp(void)
    {
    }

    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);
};

/* Less or Equal. */

struct ComputeLessOrEqualRange
{
    template <typename T>
    void operator()(de::Random &rnd, bool dstMin, bool dstMax, T &aMin, T &aMax, T &bMin, T &bMax) const;
};

struct EvaluateLessOrEqual
{
    template <typename T>
    inline bool operator()(T a, T b) const
    {
        return a <= b;
    }
};

typedef RelationalOp<ComputeLessOrEqualRange, EvaluateLessOrEqual> LessOrEqualBase;

class LessOrEqualOp : public LessOrEqualBase
{
public:
    LessOrEqualOp(GeneratorState &state, ConstValueRangeAccess valueRange);
    virtual ~LessOrEqualOp(void)
    {
    }

    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);
};

/* Greater Than. */

struct ComputeGreaterThanRange
{
    template <typename T>
    void operator()(de::Random &rnd, bool dstMin, bool dstMax, T &aMin, T &aMax, T &bMin, T &bMax) const
    {
        ComputeLessThanRange()(rnd, dstMin, dstMax, bMin, bMax, aMin, aMax);
    }
};

struct EvaluateGreaterThan
{
    template <typename T>
    inline bool operator()(T a, T b) const
    {
        return a > b;
    }
};

typedef RelationalOp<ComputeGreaterThanRange, EvaluateGreaterThan> GreaterThanBase;

class GreaterThanOp : public GreaterThanBase
{
public:
    GreaterThanOp(GeneratorState &state, ConstValueRangeAccess valueRange);
    virtual ~GreaterThanOp(void)
    {
    }

    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);
};

/* Greater or Equal. */

struct ComputeGreaterOrEqualRange
{
    template <typename T>
    void operator()(de::Random &rnd, bool dstMin, bool dstMax, T &aMin, T &aMax, T &bMin, T &bMax) const
    {
        ComputeLessOrEqualRange()(rnd, dstMin, dstMax, bMin, bMax, aMin, aMax);
    }
};

struct EvaluateGreaterOrEqual
{
    template <typename T>
    inline bool operator()(T a, T b) const
    {
        return a >= b;
    }
};

typedef RelationalOp<ComputeGreaterOrEqualRange, EvaluateGreaterOrEqual> GreaterOrEqualBase;

class GreaterOrEqualOp : public GreaterOrEqualBase
{
public:
    GreaterOrEqualOp(GeneratorState &state, ConstValueRangeAccess valueRange);
    virtual ~GreaterOrEqualOp(void)
    {
    }

    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);
};

/* Equality comparison. */

template <bool IsEqual>
class EqualityComparisonOp : public BinaryOp<8, ASSOCIATIVITY_LEFT>
{
public:
    EqualityComparisonOp(GeneratorState &state, ConstValueRangeAccess valueRange);
    virtual ~EqualityComparisonOp(void)
    {
    }

    void evaluate(ExecValueAccess dst, ExecConstValueAccess a, ExecConstValueAccess b);

    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);
};

// \note Since template implementation is in .cpp we have to reference specialized constructor and static functions from there.
class EqualOp : public EqualityComparisonOp<true>
{
public:
    EqualOp(GeneratorState &state, ConstValueRangeAccess valueRange);
    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);
};

class NotEqualOp : public EqualityComparisonOp<false>
{
public:
    NotEqualOp(GeneratorState &state, ConstValueRangeAccess valueRange);
    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);
};

} // namespace rsg

#endif // _RSGBINARYOPS_HPP
