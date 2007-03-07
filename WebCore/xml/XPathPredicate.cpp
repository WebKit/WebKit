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

#if ENABLE(XPATH)

#include "XPathPredicate.h"

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
    return -p.toNumber();
}

NumericOp::NumericOp(Opcode opcode, Expression* lhs, Expression* rhs)
    : m_opcode(opcode)
{
    addSubExpression(lhs);
    addSubExpression(rhs);
}

Value NumericOp::doEvaluate() const
{
    Value lhs(subExpr(0)->evaluate());
    Value rhs(subExpr(1)->evaluate());
    
    double leftVal = lhs.toNumber();
    double rightVal = rhs.toNumber();

    switch (m_opcode) {
        case OP_Add:
            return leftVal + rightVal;
        case OP_Sub:
            return leftVal - rightVal;
        case OP_Mul:
            return leftVal * rightVal;
        case OP_Div:
            return leftVal / rightVal;
        case OP_Mod:
            return fmod(leftVal, rightVal);
    }
    
    return Value();
}

EqTestOp::EqTestOp(Opcode opcode, Expression* lhs, Expression* rhs)
    : m_opcode(opcode)
{
    addSubExpression(lhs);
    addSubExpression(rhs);
}

bool EqTestOp::compare(const Value& lhs, const Value& rhs) const
{
    if (lhs.isNodeVector()) {
        const NodeVector& lhsVector = lhs.toNodeVector();
        if (rhs.isNodeVector()) {
            // If both objects to be compared are node-sets, then the comparison will be true if and only if
            // there is a node in the first node-set and a node in the second node-set such that the result of
            // performing the comparison on the string-values of the two nodes is true.
            const NodeVector& rhsVector = rhs.toNodeVector();
            for (unsigned lindex = 0; lindex < lhsVector.size(); ++lindex)
                for (unsigned rindex = 0; rindex < rhsVector.size(); ++rindex)
                    if (compare(stringValue(lhsVector[lindex].get()), stringValue(rhsVector[rindex].get())))
                        return true;
            return false;
        }
        if (rhs.isNumber()) {
            // If one object to be compared is a node-set and the other is a number, then the comparison will be true
            // if and only if there is a node in the node-set such that the result of performing the comparison on the number
            // to be compared and on the result of converting the string-value of that node to a number using the number function is true.
            for (unsigned lindex = 0; lindex < lhsVector.size(); ++lindex)
                if (compare(Value(stringValue(lhsVector[lindex].get())).toNumber(), rhs))
                    return true;
            return false;
        }
        if (rhs.isString()) {
            // If one object to be compared is a node-set and the other is a string, then the comparison will be true
            // if and only if there is a node in the node-set such that the result of performing the comparison on
            // the string-value of the node and the other string is true.
            for (unsigned lindex = 0; lindex < lhsVector.size(); ++lindex)
                if (compare(stringValue(lhsVector[lindex].get()), rhs))
                    return true;
            return false;
        }
        if (rhs.isBoolean()) {
            // If one object to be compared is a node-set and the other is a boolean, then the comparison will be true
            // if and only if the result of performing the comparison on the boolean and on the result of converting
            // the node-set to a boolean using the boolean function is true.
            return compare(lhs.toBoolean(), rhs);
        }
        ASSERT(0);
    }
    if (rhs.isNodeVector()) {
        const NodeVector& rhsVector = rhs.toNodeVector();
        if (lhs.isNumber()) {
            for (unsigned rindex = 0; rindex < rhsVector.size(); ++rindex)
                if (compare(lhs, Value(stringValue(rhsVector[rindex].get())).toNumber()))
                    return true;
            return false;
        }
        if (lhs.isString()) {
            for (unsigned rindex = 0; rindex < rhsVector.size(); ++rindex)
                if (compare(lhs, stringValue(rhsVector[rindex].get())))
                    return true;
            return false;
        }
        if (lhs.isBoolean())
            return compare(lhs, rhs.toBoolean());
        ASSERT(0);
    }
    
    // Neither side is a NodeVector.
    switch (m_opcode) {
        case OP_EQ:
        case OP_NE:
            bool equal;
            if (lhs.isBoolean() || rhs.isBoolean())
                equal = lhs.toBoolean() == rhs.toBoolean();
            else if (lhs.isNumber() || rhs.isNumber())
                equal = lhs.toNumber() == rhs.toNumber();
            else
                equal = lhs.toString() == rhs.toString();

            if (m_opcode == OP_EQ)
                return equal;
            return !equal;
        case OP_GT:
            return lhs.toNumber() > rhs.toNumber();
        case OP_GE:
            return lhs.toNumber() >= rhs.toNumber();
        case OP_LT:
            return lhs.toNumber() < rhs.toNumber();
        case OP_LE:
            return lhs.toNumber() <= rhs.toNumber();
    }
    ASSERT(0);
    return false;
}

Value EqTestOp::doEvaluate() const
{
    Value lhs(subExpr(0)->evaluate());
    Value rhs(subExpr(1)->evaluate());

    return compare(lhs, rhs);
}

LogicalOp::LogicalOp(Opcode opcode, Expression* lhs, Expression* rhs)
    : m_opcode(opcode)
{
    addSubExpression(lhs);
    addSubExpression(rhs);
}

bool LogicalOp::shortCircuitOn() const
{
    if (m_opcode == OP_And)
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
    // FIXME: This algorithm doesn't return nodes in document order, as it should.
    Value lhs = subExpr(0)->evaluate();
    Value rhs = subExpr(1)->evaluate();
    if (!lhs.isNodeVector() || !rhs.isNodeVector())
        return NodeVector();
    
    NodeVector lhsNodes = lhs.toNodeVector();
    NodeVector rhsNodes = rhs.toNodeVector();
    NodeVector result = lhsNodes;
    
    HashSet<Node*> nodes;
    for (size_t i = 0; i < result.size(); ++i)
        nodes.add(result[i].get());
    
    for (size_t i = 0; i < rhsNodes.size(); ++i) {
        Node* node = rhsNodes[i].get();
        if (nodes.add(node).second)
            result.append(node);
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

    // foo[3] means foo[position()=3]
    if (result.isNumber())
        return EqTestOp(EqTestOp::OP_EQ, createFunction("position"), new Number(result.toNumber())).evaluate().toBoolean();

    return result.toBoolean();
}

void Predicate::optimize()
{
    m_expr->optimize();
}

}
}

#endif // ENABLE(XPATH)
