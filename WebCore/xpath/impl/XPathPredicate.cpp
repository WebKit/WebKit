/*
 * Copyright 2005 Frerich Raabe <raabe@kde.org>
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

#include "config.h"

#if XPATH_SUPPORT

#include "XPathPredicate.h"

#include "Logging.h"
#include "Node.h"
#include "XPathFunctions.h"
#include "XPathValue.h"
#include <math.h>

#ifdef _MSC_VER // math functions missing from Microsoft Visual Studio standard C library
#define remainder(x, y) fmod((x), (y))
#endif

namespace WebCore {
namespace XPath {
        
Number::Number(double value)
    : m_value(value)
{
}

bool Number::isConstant() const
{
    return true;
}

Value Number::doEvaluate() const
{
    return m_value;
}

StringExpression::StringExpression(const String& value)
    : m_value(value)
{
}

bool StringExpression::isConstant() const
{
    return true;
}

Value StringExpression::doEvaluate() const
{
    return m_value;
}

Value Negative::doEvaluate() const
{
    Value p(subExpr(0)->evaluate());
    if (!p.isNumber()) {
        LOG(XPath, "Unary minus is undefined for non-numeric types.");
        return Value();
    }
    return -p.toNumber();
}

NumericOp::NumericOp(int opCode, Expression* lhs, Expression* rhs) :
    m_opCode(opCode)
{
    addSubExpression(lhs);
    addSubExpression(rhs);
}

Value NumericOp::doEvaluate() const
{
    Value lhs(subExpr(0)->evaluate());
    Value rhs(subExpr(1)->evaluate());
    
    if (!lhs.isNumber() || !rhs.isNumber()) {
        LOG(XPath, "Cannot perform operation on non-numeric types.");
        return Value();
    }

    double leftVal = lhs.toNumber(), rightVal = rhs.toNumber();

    switch (m_opCode) {
        case OP_Add:
            return leftVal + rightVal;
        case OP_Sub:
            return leftVal - rightVal;
        case OP_Mul:
            return leftVal * rightVal;
        case OP_Div:
            return leftVal / rightVal;
        case OP_Mod:
            return remainder(leftVal, rightVal);
        case OP_GT:
            return leftVal > rightVal;
        case OP_GE:
            return leftVal >= rightVal;
        case OP_LT:
            return leftVal < rightVal;
        case OP_LE:
            return leftVal <= rightVal;
    }
    
    return Value();
}

EqTestOp::EqTestOp(int opCode, Expression* lhs, Expression* rhs) :
    m_opCode(opCode)
{
    addSubExpression(lhs);
    addSubExpression(rhs);
}

Value EqTestOp::doEvaluate() const
{
    Value lhs(subExpr(0)->evaluate());
    Value rhs(subExpr(1)->evaluate());

    bool equal;
    if (lhs.isBoolean() || rhs.isBoolean()) {
        equal = (lhs.toBoolean() == rhs.toBoolean());
    } else if (lhs.isNumber() || rhs.isNumber()) {
        equal = (lhs.toNumber() == rhs.toNumber());
    } else {
        equal = (lhs.toString() == rhs.toString());
    }

    if (m_opCode == OP_EQ)
        return equal;

    return !equal;
}

LogicalOp::LogicalOp(int opCode, Expression* lhs, Expression* rhs) :
    m_opCode(opCode)
{
    addSubExpression(lhs);
    addSubExpression(rhs);
}

bool LogicalOp::shortCircuitOn() const
{
    if (m_opCode == OP_And)
        return false; //false and foo

    return true;  //true or bar
}

bool LogicalOp::isConstant() const
{
    return subExpr(0)->isConstant() &&
           subExpr(0)->evaluate().toBoolean() == shortCircuitOn();
}

Value LogicalOp::doEvaluate() const
{
    Value lhs(subExpr(0)->evaluate());

    // This is not only an optimization, http://www.w3.org/TR/xpath
    // dictates that we must do short-circuit evaluation
    bool lhsBool = lhs.toBoolean();
    if (lhsBool == shortCircuitOn())
        return lhsBool;

    return subExpr(1)->evaluate().toBoolean();
}

Value Union::doEvaluate() const
{
    Value lhs = subExpr(0)->evaluate();
    Value rhs = subExpr(1)->evaluate();
    if (!lhs.isNodeVector() || !rhs.isNodeVector()) {
        LOG(XPath, "Union operator '|' works only with nodevectors.");
        return NodeVector();
    }
    
    NodeVector lhsNodes = lhs.toNodeVector();
    NodeVector rhsNodes = rhs.toNodeVector();
    NodeVector result = lhsNodes;
    
    HashSet<Node*> nodes;
    
    for (unsigned i = 0; i < rhsNodes.size(); i++) {
        Node* node = rhsNodes[i].get();
        
        if (!nodes.contains(node)) {
            result.append(node);
            nodes.add(node);
        }
    }
    
    return result;
}

Predicate::Predicate(Expression* expr)
    : m_expr(expr)
{
}

Predicate::~Predicate()
{
    delete m_expr;
}

bool Predicate::evaluate() const
{
    ASSERT(m_expr != 0);

    Value result(m_expr->evaluate());

    // foo[3] really means foo[position()=3]
    if (result.isNumber()) {
        Expression* realExpr = new EqTestOp(EqTestOp::OP_EQ,
                        FunctionLibrary::self().createFunction("position"),
                        new Number(result.toNumber()));
        result = realExpr->evaluate();
        delete realExpr;
    }

    return result.toBoolean();
}

void Predicate::optimize()
{
    m_expr->optimize();
}

}
}

#endif // XPATH_SUPPORT
