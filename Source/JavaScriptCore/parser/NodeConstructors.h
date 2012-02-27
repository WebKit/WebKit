/*
 *  Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef NodeConstructors_h
#define NodeConstructors_h

#include "Nodes.h"
#include "Lexer.h"
#include "Parser.h"

namespace JSC {

    inline void* ParserArenaFreeable::operator new(size_t size, JSGlobalData* globalData)
    {
        return globalData->parserArena->allocateFreeable(size);
    }

    inline void* ParserArenaDeletable::operator new(size_t size, JSGlobalData* globalData)
    {
        return globalData->parserArena->allocateDeletable(size);
    }

    inline ParserArenaRefCounted::ParserArenaRefCounted(JSGlobalData* globalData)
    {
        globalData->parserArena->derefWithArena(adoptRef(this));
    }

    inline Node::Node(int lineNumber)
        : m_lineNumber(lineNumber)
    {
    }

    inline ExpressionNode::ExpressionNode(int lineNumber, ResultType resultType)
        : Node(lineNumber)
        , m_resultType(resultType)
    {
    }

    inline StatementNode::StatementNode(int lineNumber)
        : Node(lineNumber)
        , m_lastLine(-1)
    {
    }

    inline NullNode::NullNode(int lineNumber)
        : ExpressionNode(lineNumber, ResultType::nullType())
    {
    }

    inline BooleanNode::BooleanNode(int lineNumber, bool value)
        : ExpressionNode(lineNumber, ResultType::booleanType())
        , m_value(value)
    {
    }

    inline NumberNode::NumberNode(int lineNumber, double value)
        : ExpressionNode(lineNumber, ResultType::numberType())
        , m_value(value)
    {
    }

    inline StringNode::StringNode(int lineNumber, const Identifier& value)
        : ExpressionNode(lineNumber, ResultType::stringType())
        , m_value(value)
    {
    }

    inline RegExpNode::RegExpNode(int lineNumber, const Identifier& pattern, const Identifier& flags)
        : ExpressionNode(lineNumber)
        , m_pattern(pattern)
        , m_flags(flags)
    {
    }

    inline ThisNode::ThisNode(int lineNumber)
        : ExpressionNode(lineNumber)
    {
    }

    inline ResolveNode::ResolveNode(int lineNumber, const Identifier& ident, int startOffset)
        : ExpressionNode(lineNumber)
        , m_ident(ident)
        , m_startOffset(startOffset)
    {
    }

    inline ElementNode::ElementNode(int elision, ExpressionNode* node)
        : m_next(0)
        , m_elision(elision)
        , m_node(node)
    {
    }

    inline ElementNode::ElementNode(ElementNode* l, int elision, ExpressionNode* node)
        : m_next(0)
        , m_elision(elision)
        , m_node(node)
    {
        l->m_next = this;
    }

    inline ArrayNode::ArrayNode(int lineNumber, int elision)
        : ExpressionNode(lineNumber)
        , m_element(0)
        , m_elision(elision)
        , m_optional(true)
    {
    }

    inline ArrayNode::ArrayNode(int lineNumber, ElementNode* element)
        : ExpressionNode(lineNumber)
        , m_element(element)
        , m_elision(0)
        , m_optional(false)
    {
    }

    inline ArrayNode::ArrayNode(int lineNumber, int elision, ElementNode* element)
        : ExpressionNode(lineNumber)
        , m_element(element)
        , m_elision(elision)
        , m_optional(true)
    {
    }

    inline PropertyNode::PropertyNode(JSGlobalData*, const Identifier& name, ExpressionNode* assign, Type type)
        : m_name(name)
        , m_assign(assign)
        , m_type(type)
    {
    }

    inline PropertyNode::PropertyNode(JSGlobalData* globalData, double name, ExpressionNode* assign, Type type)
        : m_name(globalData->parserArena->identifierArena().makeNumericIdentifier(globalData, name))
        , m_assign(assign)
        , m_type(type)
    {
    }

    inline PropertyListNode::PropertyListNode(int lineNumber, PropertyNode* node)
        : Node(lineNumber)
        , m_node(node)
        , m_next(0)
    {
    }

    inline PropertyListNode::PropertyListNode(int lineNumber, PropertyNode* node, PropertyListNode* list)
        : Node(lineNumber)
        , m_node(node)
        , m_next(0)
    {
        list->m_next = this;
    }

    inline ObjectLiteralNode::ObjectLiteralNode(int lineNumber)
        : ExpressionNode(lineNumber)
        , m_list(0)
    {
    }

    inline ObjectLiteralNode::ObjectLiteralNode(int lineNumber, PropertyListNode* list)
        : ExpressionNode(lineNumber)
        , m_list(list)
    {
    }

    inline BracketAccessorNode::BracketAccessorNode(int lineNumber, ExpressionNode* base, ExpressionNode* subscript, bool subscriptHasAssignments)
        : ExpressionNode(lineNumber)
        , m_base(base)
        , m_subscript(subscript)
        , m_subscriptHasAssignments(subscriptHasAssignments)
    {
    }

    inline DotAccessorNode::DotAccessorNode(int lineNumber, ExpressionNode* base, const Identifier& ident)
        : ExpressionNode(lineNumber)
        , m_base(base)
        , m_ident(ident)
    {
    }

    inline ArgumentListNode::ArgumentListNode(int lineNumber, ExpressionNode* expr)
        : Node(lineNumber)
        , m_next(0)
        , m_expr(expr)
    {
    }

    inline ArgumentListNode::ArgumentListNode(int lineNumber, ArgumentListNode* listNode, ExpressionNode* expr)
        : Node(lineNumber)
        , m_next(0)
        , m_expr(expr)
    {
        listNode->m_next = this;
    }

    inline ArgumentsNode::ArgumentsNode()
        : m_listNode(0)
    {
    }

    inline ArgumentsNode::ArgumentsNode(ArgumentListNode* listNode)
        : m_listNode(listNode)
    {
    }

    inline NewExprNode::NewExprNode(int lineNumber, ExpressionNode* expr)
        : ExpressionNode(lineNumber)
        , m_expr(expr)
        , m_args(0)
    {
    }

    inline NewExprNode::NewExprNode(int lineNumber, ExpressionNode* expr, ArgumentsNode* args)
        : ExpressionNode(lineNumber)
        , m_expr(expr)
        , m_args(args)
    {
    }

    inline EvalFunctionCallNode::EvalFunctionCallNode(int lineNumber, ArgumentsNode* args, unsigned divot, unsigned startOffset, unsigned endOffset)
        : ExpressionNode(lineNumber)
        , ThrowableExpressionData(divot, startOffset, endOffset)
        , m_args(args)
    {
    }

    inline FunctionCallValueNode::FunctionCallValueNode(int lineNumber, ExpressionNode* expr, ArgumentsNode* args, unsigned divot, unsigned startOffset, unsigned endOffset)
        : ExpressionNode(lineNumber)
        , ThrowableExpressionData(divot, startOffset, endOffset)
        , m_expr(expr)
        , m_args(args)
    {
    }

    inline FunctionCallResolveNode::FunctionCallResolveNode(int lineNumber, const Identifier& ident, ArgumentsNode* args, unsigned divot, unsigned startOffset, unsigned endOffset)
        : ExpressionNode(lineNumber)
        , ThrowableExpressionData(divot, startOffset, endOffset)
        , m_ident(ident)
        , m_args(args)
    {
    }

    inline FunctionCallBracketNode::FunctionCallBracketNode(int lineNumber, ExpressionNode* base, ExpressionNode* subscript, ArgumentsNode* args, unsigned divot, unsigned startOffset, unsigned endOffset)
        : ExpressionNode(lineNumber)
        , ThrowableSubExpressionData(divot, startOffset, endOffset)
        , m_base(base)
        , m_subscript(subscript)
        , m_args(args)
    {
    }

    inline FunctionCallDotNode::FunctionCallDotNode(int lineNumber, ExpressionNode* base, const Identifier& ident, ArgumentsNode* args, unsigned divot, unsigned startOffset, unsigned endOffset)
        : ExpressionNode(lineNumber)
        , ThrowableSubExpressionData(divot, startOffset, endOffset)
        , m_base(base)
        , m_ident(ident)
        , m_args(args)
    {
    }

    inline CallFunctionCallDotNode::CallFunctionCallDotNode(int lineNumber, ExpressionNode* base, const Identifier& ident, ArgumentsNode* args, unsigned divot, unsigned startOffset, unsigned endOffset)
        : FunctionCallDotNode(lineNumber, base, ident, args, divot, startOffset, endOffset)
    {
    }

    inline ApplyFunctionCallDotNode::ApplyFunctionCallDotNode(int lineNumber, ExpressionNode* base, const Identifier& ident, ArgumentsNode* args, unsigned divot, unsigned startOffset, unsigned endOffset)
        : FunctionCallDotNode(lineNumber, base, ident, args, divot, startOffset, endOffset)
    {
    }

    inline PrePostResolveNode::PrePostResolveNode(int lineNumber, const Identifier& ident, unsigned divot, unsigned startOffset, unsigned endOffset)
        : ExpressionNode(lineNumber, ResultType::numberType()) // could be reusable for pre?
        , ThrowableExpressionData(divot, startOffset, endOffset)
        , m_ident(ident)
    {
    }

    inline PostfixResolveNode::PostfixResolveNode(int lineNumber, const Identifier& ident, Operator oper, unsigned divot, unsigned startOffset, unsigned endOffset)
        : PrePostResolveNode(lineNumber, ident, divot, startOffset, endOffset)
        , m_operator(oper)
    {
    }

    inline PostfixBracketNode::PostfixBracketNode(int lineNumber, ExpressionNode* base, ExpressionNode* subscript, Operator oper, unsigned divot, unsigned startOffset, unsigned endOffset)
        : ExpressionNode(lineNumber)
        , ThrowableSubExpressionData(divot, startOffset, endOffset)
        , m_base(base)
        , m_subscript(subscript)
        , m_operator(oper)
    {
    }

    inline PostfixDotNode::PostfixDotNode(int lineNumber, ExpressionNode* base, const Identifier& ident, Operator oper, unsigned divot, unsigned startOffset, unsigned endOffset)
        : ExpressionNode(lineNumber)
        , ThrowableSubExpressionData(divot, startOffset, endOffset)
        , m_base(base)
        , m_ident(ident)
        , m_operator(oper)
    {
    }

    inline PostfixErrorNode::PostfixErrorNode(int lineNumber, ExpressionNode* expr, Operator oper, unsigned divot, unsigned startOffset, unsigned endOffset)
        : ExpressionNode(lineNumber)
        , ThrowableSubExpressionData(divot, startOffset, endOffset)
        , m_expr(expr)
        , m_operator(oper)
    {
    }

    inline DeleteResolveNode::DeleteResolveNode(int lineNumber, const Identifier& ident, unsigned divot, unsigned startOffset, unsigned endOffset)
        : ExpressionNode(lineNumber)
        , ThrowableExpressionData(divot, startOffset, endOffset)
        , m_ident(ident)
    {
    }

    inline DeleteBracketNode::DeleteBracketNode(int lineNumber, ExpressionNode* base, ExpressionNode* subscript, unsigned divot, unsigned startOffset, unsigned endOffset)
        : ExpressionNode(lineNumber)
        , ThrowableExpressionData(divot, startOffset, endOffset)
        , m_base(base)
        , m_subscript(subscript)
    {
    }

    inline DeleteDotNode::DeleteDotNode(int lineNumber, ExpressionNode* base, const Identifier& ident, unsigned divot, unsigned startOffset, unsigned endOffset)
        : ExpressionNode(lineNumber)
        , ThrowableExpressionData(divot, startOffset, endOffset)
        , m_base(base)
        , m_ident(ident)
    {
    }

    inline DeleteValueNode::DeleteValueNode(int lineNumber, ExpressionNode* expr)
        : ExpressionNode(lineNumber)
        , m_expr(expr)
    {
    }

    inline VoidNode::VoidNode(int lineNumber, ExpressionNode* expr)
        : ExpressionNode(lineNumber)
        , m_expr(expr)
    {
    }

    inline TypeOfResolveNode::TypeOfResolveNode(int lineNumber, const Identifier& ident)
        : ExpressionNode(lineNumber, ResultType::stringType())
        , m_ident(ident)
    {
    }

    inline TypeOfValueNode::TypeOfValueNode(int lineNumber, ExpressionNode* expr)
        : ExpressionNode(lineNumber, ResultType::stringType())
        , m_expr(expr)
    {
    }

    inline PrefixResolveNode::PrefixResolveNode(int lineNumber, const Identifier& ident, Operator oper, unsigned divot, unsigned startOffset, unsigned endOffset)
        : PrePostResolveNode(lineNumber, ident, divot, startOffset, endOffset)
        , m_operator(oper)
    {
    }

    inline PrefixBracketNode::PrefixBracketNode(int lineNumber, ExpressionNode* base, ExpressionNode* subscript, Operator oper, unsigned divot, unsigned startOffset, unsigned endOffset)
        : ExpressionNode(lineNumber)
        , ThrowablePrefixedSubExpressionData(divot, startOffset, endOffset)
        , m_base(base)
        , m_subscript(subscript)
        , m_operator(oper)
    {
    }

    inline PrefixDotNode::PrefixDotNode(int lineNumber, ExpressionNode* base, const Identifier& ident, Operator oper, unsigned divot, unsigned startOffset, unsigned endOffset)
        : ExpressionNode(lineNumber)
        , ThrowablePrefixedSubExpressionData(divot, startOffset, endOffset)
        , m_base(base)
        , m_ident(ident)
        , m_operator(oper)
    {
    }

    inline PrefixErrorNode::PrefixErrorNode(int lineNumber, ExpressionNode* expr, Operator oper, unsigned divot, unsigned startOffset, unsigned endOffset)
        : ExpressionNode(lineNumber)
        , ThrowableExpressionData(divot, startOffset, endOffset)
        , m_expr(expr)
        , m_operator(oper)
    {
    }

    inline UnaryOpNode::UnaryOpNode(int lineNumber, ResultType type, ExpressionNode* expr, OpcodeID opcodeID)
        : ExpressionNode(lineNumber, type)
        , m_expr(expr)
        , m_opcodeID(opcodeID)
    {
    }

    inline UnaryPlusNode::UnaryPlusNode(int lineNumber, ExpressionNode* expr)
        : UnaryOpNode(lineNumber, ResultType::numberType(), expr, op_to_jsnumber)
    {
    }

    inline NegateNode::NegateNode(int lineNumber, ExpressionNode* expr)
        : UnaryOpNode(lineNumber, ResultType::numberType(), expr, op_negate)
    {
    }

    inline BitwiseNotNode::BitwiseNotNode(int lineNumber, ExpressionNode* expr)
        : ExpressionNode(lineNumber, ResultType::forBitOp())
        , m_expr(expr)
    {
    }

    inline LogicalNotNode::LogicalNotNode(int lineNumber, ExpressionNode* expr)
        : UnaryOpNode(lineNumber, ResultType::booleanType(), expr, op_not)
    {
    }

    inline BinaryOpNode::BinaryOpNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, OpcodeID opcodeID, bool rightHasAssignments)
        : ExpressionNode(lineNumber)
        , m_expr1(expr1)
        , m_expr2(expr2)
        , m_opcodeID(opcodeID)
        , m_rightHasAssignments(rightHasAssignments)
    {
    }

    inline BinaryOpNode::BinaryOpNode(int lineNumber, ResultType type, ExpressionNode* expr1, ExpressionNode* expr2, OpcodeID opcodeID, bool rightHasAssignments)
        : ExpressionNode(lineNumber, type)
        , m_expr1(expr1)
        , m_expr2(expr2)
        , m_opcodeID(opcodeID)
        , m_rightHasAssignments(rightHasAssignments)
    {
    }

    inline MultNode::MultNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
        : BinaryOpNode(lineNumber, ResultType::numberType(), expr1, expr2, op_mul, rightHasAssignments)
    {
    }

    inline DivNode::DivNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
        : BinaryOpNode(lineNumber, ResultType::numberType(), expr1, expr2, op_div, rightHasAssignments)
    {
    }


    inline ModNode::ModNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
        : BinaryOpNode(lineNumber, ResultType::numberType(), expr1, expr2, op_mod, rightHasAssignments)
    {
    }

    inline AddNode::AddNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
        : BinaryOpNode(lineNumber, ResultType::forAdd(expr1->resultDescriptor(), expr2->resultDescriptor()), expr1, expr2, op_add, rightHasAssignments)
    {
    }

    inline SubNode::SubNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
        : BinaryOpNode(lineNumber, ResultType::numberType(), expr1, expr2, op_sub, rightHasAssignments)
    {
    }

    inline LeftShiftNode::LeftShiftNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
        : BinaryOpNode(lineNumber, ResultType::forBitOp(), expr1, expr2, op_lshift, rightHasAssignments)
    {
    }

    inline RightShiftNode::RightShiftNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
        : BinaryOpNode(lineNumber, ResultType::forBitOp(), expr1, expr2, op_rshift, rightHasAssignments)
    {
    }

    inline UnsignedRightShiftNode::UnsignedRightShiftNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
        : BinaryOpNode(lineNumber, ResultType::numberType(), expr1, expr2, op_urshift, rightHasAssignments)
    {
    }

    inline LessNode::LessNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
        : BinaryOpNode(lineNumber, ResultType::booleanType(), expr1, expr2, op_less, rightHasAssignments)
    {
    }

    inline GreaterNode::GreaterNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
        : BinaryOpNode(lineNumber, ResultType::booleanType(), expr1, expr2, op_greater, rightHasAssignments)
    {
    }

    inline LessEqNode::LessEqNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
        : BinaryOpNode(lineNumber, ResultType::booleanType(), expr1, expr2, op_lesseq, rightHasAssignments)
    {
    }

    inline GreaterEqNode::GreaterEqNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
        : BinaryOpNode(lineNumber, ResultType::booleanType(), expr1, expr2, op_greatereq, rightHasAssignments)
    {
    }

    inline ThrowableBinaryOpNode::ThrowableBinaryOpNode(int lineNumber, ResultType type, ExpressionNode* expr1, ExpressionNode* expr2, OpcodeID opcodeID, bool rightHasAssignments)
        : BinaryOpNode(lineNumber, type, expr1, expr2, opcodeID, rightHasAssignments)
    {
    }

    inline ThrowableBinaryOpNode::ThrowableBinaryOpNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, OpcodeID opcodeID, bool rightHasAssignments)
        : BinaryOpNode(lineNumber, expr1, expr2, opcodeID, rightHasAssignments)
    {
    }

    inline InstanceOfNode::InstanceOfNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
        : ThrowableBinaryOpNode(lineNumber, ResultType::booleanType(), expr1, expr2, op_instanceof, rightHasAssignments)
    {
    }

    inline InNode::InNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
        : ThrowableBinaryOpNode(lineNumber, expr1, expr2, op_in, rightHasAssignments)
    {
    }

    inline EqualNode::EqualNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
        : BinaryOpNode(lineNumber, ResultType::booleanType(), expr1, expr2, op_eq, rightHasAssignments)
    {
    }

    inline NotEqualNode::NotEqualNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
        : BinaryOpNode(lineNumber, ResultType::booleanType(), expr1, expr2, op_neq, rightHasAssignments)
    {
    }

    inline StrictEqualNode::StrictEqualNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
        : BinaryOpNode(lineNumber, ResultType::booleanType(), expr1, expr2, op_stricteq, rightHasAssignments)
    {
    }

    inline NotStrictEqualNode::NotStrictEqualNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
        : BinaryOpNode(lineNumber, ResultType::booleanType(), expr1, expr2, op_nstricteq, rightHasAssignments)
    {
    }

    inline BitAndNode::BitAndNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
        : BinaryOpNode(lineNumber, ResultType::forBitOp(), expr1, expr2, op_bitand, rightHasAssignments)
    {
    }

    inline BitOrNode::BitOrNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
        : BinaryOpNode(lineNumber, ResultType::forBitOp(), expr1, expr2, op_bitor, rightHasAssignments)
    {
    }

    inline BitXOrNode::BitXOrNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
        : BinaryOpNode(lineNumber, ResultType::forBitOp(), expr1, expr2, op_bitxor, rightHasAssignments)
    {
    }

    inline LogicalOpNode::LogicalOpNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, LogicalOperator oper)
        : ExpressionNode(lineNumber, ResultType::booleanType())
        , m_expr1(expr1)
        , m_expr2(expr2)
        , m_operator(oper)
    {
    }

    inline ConditionalNode::ConditionalNode(int lineNumber, ExpressionNode* logical, ExpressionNode* expr1, ExpressionNode* expr2)
        : ExpressionNode(lineNumber)
        , m_logical(logical)
        , m_expr1(expr1)
        , m_expr2(expr2)
    {
    }

    inline ReadModifyResolveNode::ReadModifyResolveNode(int lineNumber, const Identifier& ident, Operator oper, ExpressionNode*  right, bool rightHasAssignments, unsigned divot, unsigned startOffset, unsigned endOffset)
        : ExpressionNode(lineNumber)
        , ThrowableExpressionData(divot, startOffset, endOffset)
        , m_ident(ident)
        , m_right(right)
        , m_operator(oper)
        , m_rightHasAssignments(rightHasAssignments)
    {
    }

    inline AssignResolveNode::AssignResolveNode(int lineNumber, const Identifier& ident, ExpressionNode* right, bool rightHasAssignments)
        : ExpressionNode(lineNumber)
        , m_ident(ident)
        , m_right(right)
        , m_rightHasAssignments(rightHasAssignments)
    {
    }

    inline ReadModifyBracketNode::ReadModifyBracketNode(int lineNumber, ExpressionNode* base, ExpressionNode* subscript, Operator oper, ExpressionNode* right, bool subscriptHasAssignments, bool rightHasAssignments, unsigned divot, unsigned startOffset, unsigned endOffset)
        : ExpressionNode(lineNumber)
        , ThrowableSubExpressionData(divot, startOffset, endOffset)
        , m_base(base)
        , m_subscript(subscript)
        , m_right(right)
        , m_operator(oper)
        , m_subscriptHasAssignments(subscriptHasAssignments)
        , m_rightHasAssignments(rightHasAssignments)
    {
    }

    inline AssignBracketNode::AssignBracketNode(int lineNumber, ExpressionNode* base, ExpressionNode* subscript, ExpressionNode* right, bool subscriptHasAssignments, bool rightHasAssignments, unsigned divot, unsigned startOffset, unsigned endOffset)
        : ExpressionNode(lineNumber)
        , ThrowableExpressionData(divot, startOffset, endOffset)
        , m_base(base)
        , m_subscript(subscript)
        , m_right(right)
        , m_subscriptHasAssignments(subscriptHasAssignments)
        , m_rightHasAssignments(rightHasAssignments)
    {
    }

    inline AssignDotNode::AssignDotNode(int lineNumber, ExpressionNode* base, const Identifier& ident, ExpressionNode* right, bool rightHasAssignments, unsigned divot, unsigned startOffset, unsigned endOffset)
        : ExpressionNode(lineNumber)
        , ThrowableExpressionData(divot, startOffset, endOffset)
        , m_base(base)
        , m_ident(ident)
        , m_right(right)
        , m_rightHasAssignments(rightHasAssignments)
    {
    }

    inline ReadModifyDotNode::ReadModifyDotNode(int lineNumber, ExpressionNode* base, const Identifier& ident, Operator oper, ExpressionNode* right, bool rightHasAssignments, unsigned divot, unsigned startOffset, unsigned endOffset)
        : ExpressionNode(lineNumber)
        , ThrowableSubExpressionData(divot, startOffset, endOffset)
        , m_base(base)
        , m_ident(ident)
        , m_right(right)
        , m_operator(oper)
        , m_rightHasAssignments(rightHasAssignments)
    {
    }

    inline AssignErrorNode::AssignErrorNode(int lineNumber, ExpressionNode* left, Operator oper, ExpressionNode* right, unsigned divot, unsigned startOffset, unsigned endOffset)
        : ExpressionNode(lineNumber)
        , ThrowableExpressionData(divot, startOffset, endOffset)
        , m_left(left)
        , m_operator(oper)
        , m_right(right)
    {
    }

    inline CommaNode::CommaNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2)
        : ExpressionNode(lineNumber)
    {
        m_expressions.append(expr1);
        m_expressions.append(expr2);
    }

    inline ConstStatementNode::ConstStatementNode(int lineNumber, ConstDeclNode* next)
        : StatementNode(lineNumber)
        , m_next(next)
    {
    }

    inline SourceElements::SourceElements()
    {
    }

    inline EmptyStatementNode::EmptyStatementNode(int lineNumber)
        : StatementNode(lineNumber)
    {
    }

    inline DebuggerStatementNode::DebuggerStatementNode(int lineNumber)
        : StatementNode(lineNumber)
    {
    }
    
    inline ExprStatementNode::ExprStatementNode(int lineNumber, ExpressionNode* expr)
        : StatementNode(lineNumber)
        , m_expr(expr)
    {
    }

    inline VarStatementNode::VarStatementNode(int lineNumber, ExpressionNode* expr)
        : StatementNode(lineNumber)
        , m_expr(expr)
    {
    }
    
    inline IfNode::IfNode(int lineNumber, ExpressionNode* condition, StatementNode* ifBlock)
        : StatementNode(lineNumber)
        , m_condition(condition)
        , m_ifBlock(ifBlock)
    {
    }

    inline IfElseNode::IfElseNode(int lineNumber, ExpressionNode* condition, StatementNode* ifBlock, StatementNode* elseBlock)
        : IfNode(lineNumber, condition, ifBlock)
        , m_elseBlock(elseBlock)
    {
    }

    inline DoWhileNode::DoWhileNode(int lineNumber, StatementNode* statement, ExpressionNode* expr)
        : StatementNode(lineNumber)
        , m_statement(statement)
        , m_expr(expr)
    {
    }

    inline WhileNode::WhileNode(int lineNumber, ExpressionNode* expr, StatementNode* statement)
        : StatementNode(lineNumber)
        , m_expr(expr)
        , m_statement(statement)
    {
    }

    inline ForNode::ForNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, ExpressionNode* expr3, StatementNode* statement, bool expr1WasVarDecl)
        : StatementNode(lineNumber)
        , m_expr1(expr1)
        , m_expr2(expr2)
        , m_expr3(expr3)
        , m_statement(statement)
        , m_expr1WasVarDecl(expr1 && expr1WasVarDecl)
    {
        ASSERT(statement);
    }

    inline ContinueNode::ContinueNode(JSGlobalData* globalData, int lineNumber)
        : StatementNode(lineNumber)
        , m_ident(globalData->propertyNames->nullIdentifier)
    {
    }

    inline ContinueNode::ContinueNode(int lineNumber, const Identifier& ident)
        : StatementNode(lineNumber)
        , m_ident(ident)
    {
    }
    
    inline BreakNode::BreakNode(JSGlobalData* globalData, int lineNumber)
        : StatementNode(lineNumber)
        , m_ident(globalData->propertyNames->nullIdentifier)
    {
    }

    inline BreakNode::BreakNode(int lineNumber, const Identifier& ident)
        : StatementNode(lineNumber)
        , m_ident(ident)
    {
    }
    
    inline ReturnNode::ReturnNode(int lineNumber, ExpressionNode* value)
        : StatementNode(lineNumber)
        , m_value(value)
    {
    }

    inline WithNode::WithNode(int lineNumber, ExpressionNode* expr, StatementNode* statement, uint32_t divot, uint32_t expressionLength)
        : StatementNode(lineNumber)
        , m_expr(expr)
        , m_statement(statement)
        , m_divot(divot)
        , m_expressionLength(expressionLength)
    {
    }

    inline LabelNode::LabelNode(int lineNumber, const Identifier& name, StatementNode* statement)
        : StatementNode(lineNumber)
        , m_name(name)
        , m_statement(statement)
    {
    }

    inline ThrowNode::ThrowNode(int lineNumber, ExpressionNode* expr)
        : StatementNode(lineNumber)
        , m_expr(expr)
    {
    }

    inline TryNode::TryNode(int lineNumber, StatementNode* tryBlock, const Identifier& exceptionIdent, StatementNode* catchBlock, StatementNode* finallyBlock)
        : StatementNode(lineNumber)
        , m_tryBlock(tryBlock)
        , m_exceptionIdent(exceptionIdent)
        , m_catchBlock(catchBlock)
        , m_finallyBlock(finallyBlock)
    {
    }

    inline ParameterNode::ParameterNode(const Identifier& ident)
        : m_ident(ident)
        , m_next(0)
    {
    }

    inline ParameterNode::ParameterNode(ParameterNode* l, const Identifier& ident)
        : m_ident(ident)
        , m_next(0)
    {
        l->m_next = this;
    }

    inline FuncExprNode::FuncExprNode(int lineNumber, const Identifier& ident, FunctionBodyNode* body, const SourceCode& source, ParameterNode* parameter)
        : ExpressionNode(lineNumber)
        , m_body(body)
    {
        m_body->finishParsing(source, parameter, ident);
    }

    inline FuncDeclNode::FuncDeclNode(int lineNumber, const Identifier& ident, FunctionBodyNode* body, const SourceCode& source, ParameterNode* parameter)
        : StatementNode(lineNumber)
        , m_body(body)
    {
        m_body->finishParsing(source, parameter, ident);
    }

    inline CaseClauseNode::CaseClauseNode(ExpressionNode* expr, SourceElements* statements)
        : m_expr(expr)
        , m_statements(statements)
    {
    }

    inline ClauseListNode::ClauseListNode(CaseClauseNode* clause)
        : m_clause(clause)
        , m_next(0)
    {
    }

    inline ClauseListNode::ClauseListNode(ClauseListNode* clauseList, CaseClauseNode* clause)
        : m_clause(clause)
        , m_next(0)
    {
        clauseList->m_next = this;
    }

    inline CaseBlockNode::CaseBlockNode(ClauseListNode* list1, CaseClauseNode* defaultClause, ClauseListNode* list2)
        : m_list1(list1)
        , m_defaultClause(defaultClause)
        , m_list2(list2)
    {
    }

    inline SwitchNode::SwitchNode(int lineNumber, ExpressionNode* expr, CaseBlockNode* block)
        : StatementNode(lineNumber)
        , m_expr(expr)
        , m_block(block)
    {
    }

    inline ConstDeclNode::ConstDeclNode(int lineNumber, const Identifier& ident, ExpressionNode* init)
        : ExpressionNode(lineNumber)
        , m_ident(ident)
        , m_next(0)
        , m_init(init)
    {
    }

    inline BlockNode::BlockNode(int lineNumber, SourceElements* statements)
        : StatementNode(lineNumber)
        , m_statements(statements)
    {
    }

    inline ForInNode::ForInNode(JSGlobalData* globalData, int lineNumber, ExpressionNode* l, ExpressionNode* expr, StatementNode* statement)
        : StatementNode(lineNumber)
        , m_ident(globalData->propertyNames->nullIdentifier)
        , m_init(0)
        , m_lexpr(l)
        , m_expr(expr)
        , m_statement(statement)
        , m_identIsVarDecl(false)
    {
    }

    inline ForInNode::ForInNode(JSGlobalData* globalData, int lineNumber, const Identifier& ident, ExpressionNode* in, ExpressionNode* expr, StatementNode* statement, int divot, int startOffset, int endOffset)
        : StatementNode(lineNumber)
        , m_ident(ident)
        , m_init(0)
        , m_lexpr(new (globalData) ResolveNode(lineNumber, ident, divot - startOffset))
        , m_expr(expr)
        , m_statement(statement)
        , m_identIsVarDecl(true)
    {
        if (in) {
            AssignResolveNode* node = new (globalData) AssignResolveNode(lineNumber, ident, in, true);
            node->setExceptionSourceCode(divot, divot - startOffset, endOffset - divot);
            m_init = node;
        }
        // for( var foo = bar in baz )
    }

} // namespace JSC

#endif // NodeConstructors_h
