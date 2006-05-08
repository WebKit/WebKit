/*
 * predicate.h - Copyright 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef XPathPredicate_H
#define XPathPredicate_H

#if XPATH_SUPPORT

#include "XPathExpressionNode.h"

namespace WebCore {
namespace XPath {
        
class Number : public Expression
{
public:
    Number(double value);

    bool isConstant() const;

private:
    virtual Value doEvaluate() const;

    double m_value;
};

class StringExpression : public Expression
{
public:
    StringExpression(const String& value);

    bool isConstant() const;

private:
    virtual Value doEvaluate() const;

    String m_value;
};

class Negative : public Expression
{
private:
    virtual Value doEvaluate() const;
};

class BinaryExprBase : public Expression
{
};

class NumericOp : public BinaryExprBase
{
public:
    enum {
        OP_Add = 1,
        OP_Sub,
        OP_Mul,
        OP_Div,
        OP_Mod,
        OP_GT,
        OP_LT,
        OP_GE,
        OP_LE
    };

    NumericOp(int opCode, Expression* lhs, Expression* rhs);

private:
    virtual Value doEvaluate() const;
    int m_opCode;
};

class EqTestOp : public BinaryExprBase
{
public:
    enum {
        OP_EQ = 1,
        OP_NE
    };

    EqTestOp(int opCode, Expression* lhs, Expression* rhs);

private:
    virtual Value doEvaluate() const;
    int m_opCode;
};

class LogicalOp : public BinaryExprBase
{
public:
    enum {
        OP_And = 1,
        OP_Or
    };

    LogicalOp(int opCode, Expression* lhs, Expression* rhs);

    virtual bool isConstant() const;

private:
    bool shortCircuitOn() const;
    virtual Value doEvaluate() const;
    int m_opCode;
};

class Union : public BinaryExprBase
{
private:
    virtual Value doEvaluate() const;
};

class Predicate
{
public:
    Predicate(Expression*);
    ~Predicate();

    bool evaluate() const;

    void optimize();

private:
    Predicate(const Predicate &rhs);
    Predicate &operator=(const Predicate &rhs);

    Expression* m_expr;
};

}
}

#endif // XPATH_SUPPORT

#endif // PREDICATE_H

