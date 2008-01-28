/*
 *  Copyright (C) 2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#include "config.h"
#include "nodes.h"

#include <wtf/MathExtras.h>
#include <wtf/StringExtras.h>
#include <wtf/unicode/Unicode.h>

using namespace WTF;
using namespace Unicode;

namespace KJS {

// A simple text streaming class that helps with code indentation.

enum EndlType { Endl };
enum IndentType { Indent };
enum UnindentType { Unindent };
enum DotExprType { DotExpr };

class SourceStream {
public:
    SourceStream()
        : m_numberNeedsParens(false)
        , m_atStartOfStatement(true)
        , m_precedence(PrecExpression)
    {
    }

    UString toString() const { return m_string; }

    SourceStream& operator<<(const Identifier&);
    SourceStream& operator<<(const UString&);
    SourceStream& operator<<(const char*);
    SourceStream& operator<<(double);
    SourceStream& operator<<(char);
    SourceStream& operator<<(EndlType);
    SourceStream& operator<<(IndentType);
    SourceStream& operator<<(UnindentType);
    SourceStream& operator<<(DotExprType);
    SourceStream& operator<<(Precedence);
    SourceStream& operator<<(const Node*);
    template <typename T> SourceStream& operator<<(const RefPtr<T>& n) { return *this << n.get(); }

private:
    UString m_string;
    UString m_spacesForIndentation;
    bool m_numberNeedsParens;
    bool m_atStartOfStatement;
    Precedence m_precedence;
};

// --------

static UString escapeStringForPrettyPrinting(const UString& s)
{
    UString escapedString;

    for (int i = 0; i < s.size(); i++) {
        unsigned short c = s.data()[i].unicode();
        switch (c) {
            case '\"':
                escapedString += "\\\"";
                break;
            case '\n':
                escapedString += "\\n";
                break;
            case '\r':
                escapedString += "\\r";
                break;
            case '\t':
                escapedString += "\\t";
                break;
            case '\\':
                escapedString += "\\\\";
                break;
            default:
                if (c < 128 && isPrintableChar(c))
                    escapedString.append(c);
                else {
                    char hexValue[7];
                    snprintf(hexValue, 7, "\\u%04x", c);
                    escapedString += hexValue;
                }
        }
    }

    return escapedString;
}

static const char* operatorString(Operator oper)
{
    switch (oper) {
        case OpEqual:
            return "=";
        case OpMultEq:
            return "*=";
        case OpDivEq:
            return "/=";
        case OpPlusEq:
            return "+=";
        case OpMinusEq:
            return "-=";
        case OpLShift:
            return "<<=";
        case OpRShift:
            return ">>=";
        case OpURShift:
            return ">>>=";
        case OpAndEq:
            return "&=";
        case OpXOrEq:
            return "^=";
        case OpOrEq:
            return "|=";
        case OpModEq:
            return "%=";
        case OpPlusPlus:
            return "++";
        case OpMinusMinus:
            return "--";
    }
    ASSERT_NOT_REACHED();
    return "???";
}

static bool isParserRoundTripNumber(const UString& string)
{
    double number = string.toDouble(false, false);
    if (isnan(number) || isinf(number))
        return false;
    return string == UString::from(number);
}

// --------

SourceStream& SourceStream::operator<<(char c)
{
    m_numberNeedsParens = false;
    m_atStartOfStatement = false;
    UChar ch(c);
    m_string.append(ch);
    return *this;
}

SourceStream& SourceStream::operator<<(const char* s)
{
    m_numberNeedsParens = false;
    m_atStartOfStatement = false;
    m_string += s;
    return *this;
}

SourceStream& SourceStream::operator<<(double value)
{
    bool needParens = m_numberNeedsParens;
    m_numberNeedsParens = false;
    m_atStartOfStatement = false;

    if (needParens)
        m_string.append('(');
    m_string += UString::from(value);
    if (needParens)
        m_string.append(')');

    return *this;
}

SourceStream& SourceStream::operator<<(const UString& s)
{
    m_numberNeedsParens = false;
    m_atStartOfStatement = false;
    m_string += s;
    return *this;
}

SourceStream& SourceStream::operator<<(const Identifier& s)
{
    m_numberNeedsParens = false;
    m_atStartOfStatement = false;
    m_string += s.ustring();
    return *this;
}

SourceStream& SourceStream::operator<<(const Node* n)
{
    bool needParens = (m_precedence != PrecExpression && n->precedence() > m_precedence) || (m_atStartOfStatement && n->needsParensIfLeftmost());
    m_precedence = PrecExpression;
    if (!n)
        return *this;
    if (needParens) {
        m_numberNeedsParens = false;
        m_string.append('(');
    }
    n->streamTo(*this);
    if (needParens)
        m_string.append(')');
    return *this;
}

SourceStream& SourceStream::operator<<(EndlType)
{
    m_numberNeedsParens = false;
    m_atStartOfStatement = true;
    m_string.append('\n');
    m_string.append(m_spacesForIndentation);
    return *this;
}

SourceStream& SourceStream::operator<<(IndentType)
{
    m_numberNeedsParens = false;
    m_atStartOfStatement = false;
    m_spacesForIndentation += "  ";
    return *this;
}

SourceStream& SourceStream::operator<<(UnindentType)
{
    m_numberNeedsParens = false;
    m_atStartOfStatement = false;
    m_spacesForIndentation = m_spacesForIndentation.substr(0, m_spacesForIndentation.size() - 2);
    return *this;
}

inline SourceStream& SourceStream::operator<<(DotExprType)
{
    m_numberNeedsParens = true;
    return *this;
}

inline SourceStream& SourceStream::operator<<(Precedence precedence)
{
    m_precedence = precedence;
    return *this;
}

static void streamLeftAssociativeBinaryOperator(SourceStream& s, Precedence precedence,
    const char* operatorString, const Node* left, const Node* right)
{
    s << precedence << left
        << ' ' << operatorString << ' '
        << static_cast<Precedence>(precedence - 1) << right;
}

template <typename T> static inline void streamLeftAssociativeBinaryOperator(SourceStream& s,
    Precedence p, const char* o, const RefPtr<T>& l, const RefPtr<T>& r)
{
    streamLeftAssociativeBinaryOperator(s, p, o, l.get(), r.get());
}

static inline void bracketNodeStreamTo(SourceStream& s, const RefPtr<ExpressionNode>& base, const RefPtr<ExpressionNode>& subscript)
{
    s << PrecCall << base.get() << "[" << subscript.get() << "]";
}

static inline void dotNodeStreamTo(SourceStream& s, const RefPtr<ExpressionNode>& base, const Identifier& ident)
{
    s << DotExpr << PrecCall << base.get() << "." << ident;
}

// --------

UString Node::toString() const
{
    SourceStream stream;
    streamTo(stream);
    return stream.toString();
}

// --------

void NullNode::streamTo(SourceStream& s) const
{
    s << "null";
}

void FalseNode::streamTo(SourceStream& s) const
{
    s << "false";
}

void TrueNode::streamTo(SourceStream& s) const
{
    s << "true";
}

void PlaceholderTrueNode::streamTo(SourceStream&) const
{
}

void NumberNode::streamTo(SourceStream& s) const
{
    s << value();
}

void StringNode::streamTo(SourceStream& s) const
{
    s << '"' << escapeStringForPrettyPrinting(m_value) << '"';
}

void RegExpNode::streamTo(SourceStream& s) const
{
    s << '/' <<  m_regExp->pattern() << '/' << m_regExp->flags();
}

void ThisNode::streamTo(SourceStream& s) const
{
    s << "this";
}

void ResolveNode::streamTo(SourceStream& s) const
{
    s << m_ident;
}

void ElementNode::streamTo(SourceStream& s) const
{
    for (const ElementNode* n = this; n; n = n->m_next.get()) {
        for (int i = 0; i < n->m_elision; i++)
            s << ',';
        s << PrecAssignment << n->m_node;
        if (n->m_next)
            s << ',';
    }
}

void ArrayNode::streamTo(SourceStream& s) const
{
    s << '[' << m_element;
    for (int i = 0; i < m_elision; i++)
        s << ',';
    // Parser consumes one elision comma if there's array elements
    // present in the expression.
    if (m_optional && m_element)
        s << ',';
    s << ']';
}

void ObjectLiteralNode::streamTo(SourceStream& s) const
{
    if (m_list)
        s << "{ " << m_list << " }";
    else
        s << "{ }";
}

void PropertyListNode::streamTo(SourceStream& s) const
{
    s << m_node;
    for (const PropertyListNode* n = m_next.get(); n; n = n->m_next.get())
        s << ", " << n->m_node;
}

void PropertyNode::streamTo(SourceStream& s) const
{
    switch (m_type) {
        case Constant: {
            UString propertyName = name().ustring();
            if (isParserRoundTripNumber(propertyName))
                s << propertyName;
            else
                s << '"' << escapeStringForPrettyPrinting(propertyName) << '"';
            s << ": " << PrecAssignment << m_assign;
            break;
        }
        case Getter:
        case Setter: {
            const FuncExprNode* func = static_cast<const FuncExprNode*>(m_assign.get());
            if (m_type == Getter)
                s << "get ";
            else
                s << "set ";
            s << escapeStringForPrettyPrinting(name().ustring())
                << "(" << func->m_parameter << ')' << func->m_body;
            break;
        }
    }
}

void BracketAccessorNode::streamTo(SourceStream& s) const
{
    bracketNodeStreamTo(s, m_base, m_subscript);
}

void DotAccessorNode::streamTo(SourceStream& s) const
{
    dotNodeStreamTo(s, m_base, m_ident);
}

void ArgumentListNode::streamTo(SourceStream& s) const
{
    s << PrecAssignment << m_expr;
    for (ArgumentListNode* n = m_next.get(); n; n = n->m_next.get())
        s << ", " << PrecAssignment << n->m_expr;
}

void ArgumentsNode::streamTo(SourceStream& s) const
{
    s << '(' << m_listNode << ')';
}

void NewExprNode::streamTo(SourceStream& s) const
{
    s << "new " << PrecMember << m_expr << m_args;
}

void FunctionCallValueNode::streamTo(SourceStream& s) const
{
    s << PrecCall << m_expr << m_args;
}

void FunctionCallResolveNode::streamTo(SourceStream& s) const
{
    s << m_ident << m_args;
}

void FunctionCallBracketNode::streamTo(SourceStream& s) const
{
    bracketNodeStreamTo(s, m_base, m_subscript);
    s << m_args;
}

void FunctionCallDotNode::streamTo(SourceStream& s) const
{
    dotNodeStreamTo(s, m_base, m_ident);
    s << m_args;
}

void PostIncResolveNode::streamTo(SourceStream& s) const
{
    s << m_ident << "++";
}

void PostDecResolveNode::streamTo(SourceStream& s) const
{
    s << m_ident << "--";
}

void PostIncBracketNode::streamTo(SourceStream& s) const
{
    bracketNodeStreamTo(s, m_base, m_subscript);
    s << "++";
}

void PostDecBracketNode::streamTo(SourceStream& s) const
{
    bracketNodeStreamTo(s, m_base, m_subscript);
    s << "--";
}

void PostIncDotNode::streamTo(SourceStream& s) const
{
    dotNodeStreamTo(s, m_base, m_ident);
    s << "++";
}

void PostDecDotNode::streamTo(SourceStream& s) const
{
    dotNodeStreamTo(s, m_base, m_ident);
    s << "--";
}

void PostfixErrorNode::streamTo(SourceStream& s) const
{
    s << PrecLeftHandSide << m_expr;
    if (m_operator == OpPlusPlus)
        s << "++";
    else
        s << "--";
}

void DeleteResolveNode::streamTo(SourceStream& s) const
{
    s << "delete " << m_ident;
}

void DeleteBracketNode::streamTo(SourceStream& s) const
{
    s << "delete ";
    bracketNodeStreamTo(s, m_base, m_subscript);
}

void DeleteDotNode::streamTo(SourceStream& s) const
{
    s << "delete ";
    dotNodeStreamTo(s, m_base, m_ident);
}

void DeleteValueNode::streamTo(SourceStream& s) const
{
    s << "delete " << PrecUnary << m_expr;
}

void VoidNode::streamTo(SourceStream& s) const
{
    s << "void " << PrecUnary << m_expr;
}

void TypeOfValueNode::streamTo(SourceStream& s) const
{
    s << "typeof " << PrecUnary << m_expr;
}

void TypeOfResolveNode::streamTo(SourceStream& s) const
{
    s << "typeof " << m_ident;
}

void PreIncResolveNode::streamTo(SourceStream& s) const
{
    s << "++" << m_ident;
}

void PreDecResolveNode::streamTo(SourceStream& s) const
{
    s << "--" << m_ident;
}

void PreIncBracketNode::streamTo(SourceStream& s) const
{
    s << "++";
    bracketNodeStreamTo(s, m_base, m_subscript);
}

void PreDecBracketNode::streamTo(SourceStream& s) const
{
    s << "--";
    bracketNodeStreamTo(s, m_base, m_subscript);
}

void PreIncDotNode::streamTo(SourceStream& s) const
{
    s << "++";
    dotNodeStreamTo(s, m_base, m_ident);
}

void PreDecDotNode::streamTo(SourceStream& s) const
{
    s << "--";
    dotNodeStreamTo(s, m_base, m_ident);
}

void PrefixErrorNode::streamTo(SourceStream& s) const
{
    if (m_operator == OpPlusPlus)
        s << "++" << PrecUnary << m_expr;
    else
        s << "--" << PrecUnary << m_expr;
}

void UnaryPlusNode::streamTo(SourceStream& s) const
{
    s << "+ " << PrecUnary << m_expr;
}

void NegateNode::streamTo(SourceStream& s) const
{
    s << "- " << PrecUnary << m_expr;
}

void BitwiseNotNode::streamTo(SourceStream& s) const
{
    s << "~" << PrecUnary << m_expr;
}

void LogicalNotNode::streamTo(SourceStream& s) const
{
    s << "!" << PrecUnary << m_expr;
}

void MultNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), "*", m_term1, m_term2);
}

void DivNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), "/", m_term1, m_term2);
}

void ModNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), "%", m_term1, m_term2);
}

void AddNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), "+", m_term1, m_term2);
}

void SubNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), "-", m_term1, m_term2);
}

void LeftShiftNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), "<<", m_term1, m_term2);
}

void RightShiftNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), ">>", m_term1, m_term2);
}

void UnsignedRightShiftNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), ">>>", m_term1, m_term2);
}

void LessNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), "<", m_expr1, m_expr2);
}

void GreaterNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), ">", m_expr1, m_expr2);
}

void LessEqNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), "<=", m_expr1, m_expr2);
}

void GreaterEqNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), ">=", m_expr1, m_expr2);
}

void InstanceOfNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), "instanceof", m_expr1, m_expr2);
}

void InNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), "in", m_expr1, m_expr2);
}

void EqualNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), "==", m_expr1, m_expr2);
}

void NotEqualNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), "!=", m_expr1, m_expr2);
}

void StrictEqualNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), "===", m_expr1, m_expr2);
}

void NotStrictEqualNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), "!==", m_expr1, m_expr2);
}

void BitAndNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), "&", m_expr1, m_expr2);
}

void BitXOrNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), "^", m_expr1, m_expr2);
}

void BitOrNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), "|", m_expr1, m_expr2);
}

void LogicalAndNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), "&&", m_expr1, m_expr2);
}

void LogicalOrNode::streamTo(SourceStream& s) const
{
    streamLeftAssociativeBinaryOperator(s, precedence(), "||", m_expr1, m_expr2);
}

void ConditionalNode::streamTo(SourceStream& s) const
{
    s << PrecLogicalOr << m_logical
        << " ? " << PrecAssignment << m_expr1
        << " : " << PrecAssignment << m_expr2;
}

void ReadModifyResolveNode::streamTo(SourceStream& s) const
{
    s << m_ident << ' ' << operatorString(m_operator) << ' ' << PrecAssignment << m_right;
}

void AssignResolveNode::streamTo(SourceStream& s) const
{
    s << m_ident << " = " << PrecAssignment << m_right;
}

void ReadModifyBracketNode::streamTo(SourceStream& s) const
{
    bracketNodeStreamTo(s, m_base, m_subscript);
    s << ' ' << operatorString(m_operator) << ' ' << PrecAssignment << m_right;
}

void AssignBracketNode::streamTo(SourceStream& s) const
{
    bracketNodeStreamTo(s, m_base, m_subscript);
    s << " = " << PrecAssignment << m_right;
}

void ReadModifyDotNode::streamTo(SourceStream& s) const
{
    dotNodeStreamTo(s, m_base, m_ident);
    s << ' ' << operatorString(m_operator) << ' ' << PrecAssignment << m_right;
}

void AssignDotNode::streamTo(SourceStream& s) const
{
    dotNodeStreamTo(s, m_base, m_ident);
    s << " = " << PrecAssignment << m_right;
}

void AssignErrorNode::streamTo(SourceStream& s) const
{
    s << PrecLeftHandSide << m_left << ' '
        << operatorString(m_operator) << ' ' << PrecAssignment << m_right;
}

void CommaNode::streamTo(SourceStream& s) const
{
    s << PrecAssignment << m_expr1 << ", " << PrecAssignment << m_expr2;
}

void ConstDeclNode::streamTo(SourceStream& s) const
{
    s << m_ident;
    if (m_init)
        s << " = " << m_init;
    for (ConstDeclNode* n = m_next.get(); n; n = n->m_next.get()) {
        s << ", " << m_ident;
        if (m_init)
            s << " = " << m_init;
    }
}

void ConstStatementNode::streamTo(SourceStream& s) const
{
    s << Endl << "const " << m_next << ';';
}

static inline void statementListStreamTo(const Vector<RefPtr<StatementNode> >& nodes, SourceStream& s)
{
    for (Vector<RefPtr<StatementNode> >::const_iterator ptr = nodes.begin(); ptr != nodes.end(); ptr++)
        s << *ptr;
}

void BlockNode::streamTo(SourceStream& s) const
{
    s << Endl << "{" << Indent;
    statementListStreamTo(m_children, s);
    s << Unindent << Endl << "}";
}

void ScopeNode::streamTo(SourceStream& s) const
{
    s << Endl << "{" << Indent;

    bool printedVar = false;
    for (size_t i = 0; i < m_varStack.size(); ++i) {
        if (m_varStack[i].second == 0) {
            if (!printedVar) {
                s << Endl << "var ";
                printedVar = true;
            } else
                s << ", ";
            s << m_varStack[i].first;
        }
    }
    if (printedVar)
        s << ';';

    statementListStreamTo(m_children, s);
    s << Unindent << Endl << "}";
}

void EmptyStatementNode::streamTo(SourceStream& s) const
{
    s << Endl << ';';
}

void ExprStatementNode::streamTo(SourceStream& s) const
{
    s << Endl << m_expr << ';';
}

void VarStatementNode::streamTo(SourceStream& s) const
{
    s << Endl << "var " << m_expr << ';';
}

void IfNode::streamTo(SourceStream& s) const
{
    s << Endl << "if (" << m_condition << ')' << Indent << m_ifBlock << Unindent;
}

void IfElseNode::streamTo(SourceStream& s) const
{
    IfNode::streamTo(s);
    s << Endl << "else" << Indent << m_elseBlock << Unindent;
}

void DoWhileNode::streamTo(SourceStream& s) const
{
    s << Endl << "do " << Indent << m_statement << Unindent << Endl
        << "while (" << m_expr << ");";
}

void WhileNode::streamTo(SourceStream& s) const
{
    s << Endl << "while (" << m_expr << ')' << Indent << m_statement << Unindent;
}

void ForNode::streamTo(SourceStream& s) const
{
    s << Endl << "for ("
        << (m_expr1WasVarDecl ? "var " : "")
        << m_expr1
        << "; " << m_expr2
        << "; " << m_expr3
        << ')' << Indent << m_statement << Unindent;
}

void ForInNode::streamTo(SourceStream& s) const
{
    s << Endl << "for (";
    if (m_identIsVarDecl) {
        s << "var ";
        if (m_init)
            s << m_init;
        else
            s << PrecLeftHandSide << m_lexpr;
    } else
        s << PrecLeftHandSide << m_lexpr;

    s << " in " << m_expr << ')' << Indent << m_statement << Unindent;
}

void ContinueNode::streamTo(SourceStream& s) const
{
    s << Endl << "continue";
    if (!m_ident.isNull())
        s << ' ' << m_ident;
    s << ';';
}

void BreakNode::streamTo(SourceStream& s) const
{
    s << Endl << "break";
    if (!m_ident.isNull())
        s << ' ' << m_ident;
    s << ';';
}

void ReturnNode::streamTo(SourceStream& s) const
{
    s << Endl << "return";
    if (m_value)
        s << ' ' << m_value;
    s << ';';
}

void WithNode::streamTo(SourceStream& s) const
{
    s << Endl << "with (" << m_expr << ") " << m_statement;
}

void CaseClauseNode::streamTo(SourceStream& s) const
{
    s << Endl;
    if (m_expr)
        s << "case " << m_expr;
    else
        s << "default";
    s << ":" << Indent;
    statementListStreamTo(m_children, s);
    s << Unindent;
}

void ClauseListNode::streamTo(SourceStream& s) const
{
    for (const ClauseListNode* n = this; n; n = n->getNext())
        s << n->getClause();
}

void CaseBlockNode::streamTo(SourceStream& s) const
{
    for (const ClauseListNode* n = m_list1.get(); n; n = n->getNext())
        s << n->getClause();
    s << m_defaultClause;
    for (const ClauseListNode* n = m_list2.get(); n; n = n->getNext())
        s << n->getClause();
}

void SwitchNode::streamTo(SourceStream& s) const
{
    s << Endl << "switch (" << m_expr << ") {"
        << Indent << m_block << Unindent
        << Endl << "}";
}

void LabelNode::streamTo(SourceStream& s) const
{
    s << Endl << m_label << ":" << Indent << m_statement << Unindent;
}

void ThrowNode::streamTo(SourceStream& s) const
{
    s << Endl << "throw " << m_expr << ';';
}

void TryNode::streamTo(SourceStream& s) const
{
    s << Endl << "try " << m_tryBlock;
    if (m_catchBlock)
        s << Endl << "catch (" << m_exceptionIdent << ')' << m_catchBlock;
    if (m_finallyBlock)
        s << Endl << "finally " << m_finallyBlock;
}

void ParameterNode::streamTo(SourceStream& s) const
{
    s << m_ident;
    for (ParameterNode* n = m_next.get(); n; n = n->m_next.get())
        s << ", " << n->m_ident;
}

void FuncDeclNode::streamTo(SourceStream& s) const
{
    s << Endl << "function " << m_ident << '(' << m_parameter << ')' << m_body;
}

void FuncExprNode::streamTo(SourceStream& s) const
{
    s << "function " << m_ident << '(' << m_parameter << ')' << m_body;
}

} // namespace KJS
