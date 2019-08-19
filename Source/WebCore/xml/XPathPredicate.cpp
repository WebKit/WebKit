/*
 * Copyright 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
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
#include "XPathPredicate.h"

#include "XPathFunctions.h"
#include "XPathUtil.h"
#include <math.h>
#include <wtf/MathExtras.h>

namespace WebCore {
namespace XPath {
        
Number::Number(double value)
    : m_value(value)
{
}

Value Number::evaluate() const
{
    return m_value;
}

StringExpression::StringExpression(String&& value)
    : m_value(WTFMove(value))
{
}

Value StringExpression::evaluate() const
{
    return m_value;
}

Negative::Negative(std::unique_ptr<Expression> expression)
{
    addSubexpression(WTFMove(expression));
}

Value Negative::evaluate() const
{
    return -subexpression(0).evaluate().toNumber();
}

NumericOp::NumericOp(Opcode opcode, std::unique_ptr<Expression> lhs, std::unique_ptr<Expression> rhs)
    : m_opcode(opcode)
{
    addSubexpression(WTFMove(lhs));
    addSubexpression(WTFMove(rhs));
}

Value NumericOp::evaluate() const
{
    double leftVal = subexpression(0).evaluate().toNumber();
    double rightVal = subexpression(1).evaluate().toNumber();

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

    ASSERT_NOT_REACHED();
    return 0.0;
}

EqTestOp::EqTestOp(Opcode opcode, std::unique_ptr<Expression> lhs, std::unique_ptr<Expression> rhs)
    : m_opcode(opcode)
{
    addSubexpression(WTFMove(lhs));
    addSubexpression(WTFMove(rhs));
}

bool EqTestOp::compare(const Value& lhs, const Value& rhs) const
{
    if (lhs.isNodeSet()) {
        const NodeSet& lhsSet = lhs.toNodeSet();
        if (rhs.isNodeSet()) {
            // If both objects to be compared are node-sets, then the comparison will be true if and only if
            // there is a node in the first node-set and a node in the second node-set such that the result of
            // performing the comparison on the string-values of the two nodes is true.
            const NodeSet& rhsSet = rhs.toNodeSet();
            for (auto& lhs : lhsSet) {
                for (auto& rhs : rhsSet) {
                    if (compare(stringValue(lhs.get()), stringValue(rhs.get())))
                        return true;
                }
            }
            return false;
        }
        if (rhs.isNumber()) {
            // If one object to be compared is a node-set and the other is a number, then the comparison will be true
            // if and only if there is a node in the node-set such that the result of performing the comparison on the number
            // to be compared and on the result of converting the string-value of that node to a number using the number function is true.
            for (auto& lhs : lhsSet) {
                if (compare(Value(stringValue(lhs.get())).toNumber(), rhs))
                    return true;
            }
            return false;
        }
        if (rhs.isString()) {
            // If one object to be compared is a node-set and the other is a string, then the comparison will be true
            // if and only if there is a node in the node-set such that the result of performing the comparison on
            // the string-value of the node and the other string is true.
            for (auto& lhs : lhsSet) {
                if (compare(stringValue(lhs.get()), rhs))
                    return true;
            }
            return false;
        }
        if (rhs.isBoolean()) {
            // If one object to be compared is a node-set and the other is a boolean, then the comparison will be true
            // if and only if the result of performing the comparison on the boolean and on the result of converting
            // the node-set to a boolean using the boolean function is true.
            return compare(lhs.toBoolean(), rhs);
        }
        ASSERT_NOT_REACHED();
    }
    if (rhs.isNodeSet()) {
        const NodeSet& rhsSet = rhs.toNodeSet();
        if (lhs.isNumber()) {
            for (auto& rhs : rhsSet) {
                if (compare(lhs, Value(stringValue(rhs.get())).toNumber()))
                    return true;
            }
            return false;
        }
        if (lhs.isString()) {
            for (auto& rhs : rhsSet) {
                if (compare(lhs, stringValue(rhs.get())))
                    return true;
            }
            return false;
        }
        if (lhs.isBoolean())
            return compare(lhs, rhs.toBoolean());
        ASSERT_NOT_REACHED();
    }
    
    // Neither side is a NodeSet.
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

    ASSERT_NOT_REACHED();
    return false;
}

Value EqTestOp::evaluate() const
{
    Value lhs(subexpression(0).evaluate());
    Value rhs(subexpression(1).evaluate());
    return compare(lhs, rhs);
}

LogicalOp::LogicalOp(Opcode opcode, std::unique_ptr<Expression> lhs, std::unique_ptr<Expression> rhs)
    : m_opcode(opcode)
{
    addSubexpression(WTFMove(lhs));
    addSubexpression(WTFMove(rhs));
}

inline bool LogicalOp::shortCircuitOn() const
{
    return m_opcode != OP_And;
}

Value LogicalOp::evaluate() const
{
    // This is not only an optimization, http://www.w3.org/TR/xpath
    // dictates that we must do short-circuit evaluation
    bool lhsBool = subexpression(0).evaluate().toBoolean();
    if (lhsBool == shortCircuitOn())
        return lhsBool;

    return subexpression(1).evaluate().toBoolean();
}

Union::Union(std::unique_ptr<Expression> lhs, std::unique_ptr<Expression> rhs)
{
    addSubexpression(WTFMove(lhs));
    addSubexpression(WTFMove(rhs));
}

Value Union::evaluate() const
{
    Value lhsResult = subexpression(0).evaluate();
    Value rhs = subexpression(1).evaluate();

    NodeSet& resultSet = lhsResult.modifiableNodeSet();
    const NodeSet& rhsNodes = rhs.toNodeSet();

    HashSet<Node*> nodes;
    for (auto& result : resultSet)
        nodes.add(result.get());

    for (auto& node : rhsNodes) {
        if (nodes.add(node.get()).isNewEntry)
            resultSet.append(node.get());
    }

    // It would also be possible to perform a merge sort here to avoid making an unsorted result,
    // but that would waste the time in cases when order is not important.
    resultSet.markSorted(false);

    return lhsResult;
}

bool evaluatePredicate(const Expression& expression)
{
    Value result(expression.evaluate());

    // foo[3] means foo[position()=3]
    if (result.isNumber())
        return EqTestOp(EqTestOp::OP_EQ, Function::create("position"_s), makeUnique<Number>(result.toNumber())).evaluate().toBoolean();

    return result.toBoolean();
}

bool predicateIsContextPositionSensitive(const Expression& expression)
{
    return expression.isContextPositionSensitive() || expression.resultType() == Value::NumberValue;
}

}
}
