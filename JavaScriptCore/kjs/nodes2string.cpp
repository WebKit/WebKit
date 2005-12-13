/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003 Apple Computer, Inc.
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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "nodes.h"

namespace KJS {
  /**
   * A simple text streaming class that helps with code indentation.
   */
  class SourceStream {
  public:
    enum Format {
      Endl, Indent, Unindent
    };

    UString toString() const { return str; }
    SourceStream& operator<<(const Identifier &);
    SourceStream& operator<<(const UString &);
    SourceStream& operator<<(const char *);
    SourceStream& operator<<(char);
    SourceStream& operator<<(Format f);
    SourceStream& operator<<(const Node *);
    template <typename T> SourceStream& operator<<(RefPtr<T> n) { return this->operator<<(n.get()); }
  private:
    UString str; /* TODO: buffer */
    UString ind;
  };
};

using namespace KJS;

SourceStream& SourceStream::operator<<(char c)
{
  str += UString(c);
  return *this;
}

SourceStream& SourceStream::operator<<(const char *s)
{
  str += UString(s);
  return *this;
}

SourceStream& SourceStream::operator<<(const UString &s)
{
  str += s;
  return *this;
}

SourceStream& SourceStream::operator<<(const Identifier &s)
{
  str += s.ustring();
  return *this;
}

SourceStream& SourceStream::operator<<(const Node *n)
{
  if (n)
    n->streamTo(*this);
  return *this;
}

SourceStream& SourceStream::operator<<(Format f)
{
  switch (f) {
    case Endl:
      str += "\n" + ind;
      break;
    case Indent:
      ind += "  ";
      break;
    case Unindent:
      ind = ind.substr(0, ind.size() - 2);
      break;
  }

  return *this;
}

UString Node::toString() const
{
  SourceStream str;
  streamTo(str);

  return str.toString();
}

void NullNode::streamTo(SourceStream &s) const { s << "null"; }

void BooleanNode::streamTo(SourceStream &s) const
{
  s << (value ? "true" : "false");
}

void NumberNode::streamTo(SourceStream &s) const { s << UString::from(value); }

void StringNode::streamTo(SourceStream &s) const
{
  s << '"' << value << '"';
}

void RegExpNode::streamTo(SourceStream &s) const { s <<  pattern; }

void ThisNode::streamTo(SourceStream &s) const { s << "this"; }

void ResolveNode::streamTo(SourceStream &s) const { s << ident; }

void GroupNode::streamTo(SourceStream &s) const
{
  s << "(" << group << ")"; 
}

void ElementNode::streamTo(SourceStream &s) const
{
  for (const ElementNode *n = this; n; n = n->list.get()) {
    for (int i = 0; i < n->elision; i++)
      s << ",";
    s << n->node;
  }
}

void ArrayNode::streamTo(SourceStream &s) const
{
  s << "[" << element;
  for (int i = 0; i < elision; i++)
    s << ",";
  s << "]";
}

void ObjectLiteralNode::streamTo(SourceStream &s) const
{
  if (list)
    s << "{ " << list << " }";
  else
    s << "{ }";
}

void PropertyListNode::streamTo(SourceStream &s) const
{
  s << node;
  
  for (const PropertyListNode *n = list.get(); n; n = n->list.get())
    s << ", " << n->node;
}

void PropertyNode::streamTo(SourceStream &s) const
{
  switch (type) {
    case Constant:
      s << name << ": " << assign;
      break;
    case Getter:
    case Setter: {
      const FuncExprNode *func = static_cast<const FuncExprNode *>(assign.get());
      if (type == Getter)
        s << "get "; 
      else
        s << "set ";
      
      s << name << "(" << func->param << ")" << func->body;
      break;
    }
  }
}

void PropertyNameNode::streamTo(SourceStream &s) const
{
  if (str.isNull())
    s << UString::from(numeric);
  else
    s << str;
}

void BracketAccessorNode::streamTo(SourceStream &s) const
{
  s << expr1 << "[" << expr2 << "]";
}

void DotAccessorNode::streamTo(SourceStream &s) const
{
  s << expr << "." << ident;
}

void ArgumentListNode::streamTo(SourceStream &s) const
{
  s << expr;
  for (ArgumentListNode *n = list.get(); n; n = n->list.get())
    s << ", " << n->expr;
}

void ArgumentsNode::streamTo(SourceStream &s) const
{
  s << "(" << list << ")";
}

void NewExprNode::streamTo(SourceStream &s) const
{
  s << "new " << expr << args;
}

void FunctionCallValueNode::streamTo(SourceStream &s) const
{
  s << expr << args;
}

void FunctionCallResolveNode::streamTo(SourceStream &s) const
{
  s << ident << args;
}

void FunctionCallBracketNode::streamTo(SourceStream &s) const
{
  s << base << "[" << subscript << "]" << args;
}

void FunctionCallParenBracketNode::streamTo(SourceStream &s) const
{
  s << "(" << base << "[" << subscript << "])" << args;
}

void FunctionCallDotNode::streamTo(SourceStream &s) const
{
  s << base << "." << ident << args;
}

void FunctionCallParenDotNode::streamTo(SourceStream &s) const
{
  s << "(" << base << "." << ident << ")" << args;
}

void PostfixResolveNode::streamTo(SourceStream &s) const
{
  s << m_ident;
  if (m_oper == OpPlusPlus)
    s << "++";
  else
    s << "--";
}

void PostfixBracketNode::streamTo(SourceStream &s) const
{
  s << m_base << "[" << m_subscript << "]";
  if (m_oper == OpPlusPlus)
    s << "++";
  else
    s << "--";
}

void PostfixDotNode::streamTo(SourceStream &s) const
{
  s << m_base << "." << m_ident;
  if (m_oper == OpPlusPlus)
    s << "++";
  else
    s << "--";
}

void DeleteResolveNode::streamTo(SourceStream &s) const
{
  s << "delete " << m_ident;
}

void DeleteBracketNode::streamTo(SourceStream &s) const
{
  s << "delete " << m_base << "[" << m_subscript << "]";
}

void DeleteDotNode::streamTo(SourceStream &s) const
{
  s << "delete " << m_base << "." << m_ident;
}

void DeleteValueNode::streamTo(SourceStream &s) const
{
  s << "delete " << m_expr;
}

void VoidNode::streamTo(SourceStream &s) const
{
  s << "void " << expr;
}

void TypeOfValueNode::streamTo(SourceStream &s) const
{
  s << "typeof " << m_expr;
}

void TypeOfResolveNode::streamTo(SourceStream &s) const
{
  s << "typeof " << m_ident;
}

void PrefixResolveNode::streamTo(SourceStream &s) const
{
  if (m_oper == OpPlusPlus)
    s << "++";
  else
    s << "--";
  s << m_ident;
}

void PrefixBracketNode::streamTo(SourceStream &s) const
{
  if (m_oper == OpPlusPlus)
    s << "++";
  else
    s << "--";
  s << m_base << "[" << m_subscript << "]";
}

void PrefixDotNode::streamTo(SourceStream &s) const
{
  if (m_oper == OpPlusPlus)
    s << "++";
  else
    s << "--";
  s << m_base << "." << m_ident;
}

void UnaryPlusNode::streamTo(SourceStream &s) const
{
  s << "+" << expr;
}

void NegateNode::streamTo(SourceStream &s) const
{
  s << "-" << expr;
}

void BitwiseNotNode::streamTo(SourceStream &s) const
{
  s << "~" << expr;
}

void LogicalNotNode::streamTo(SourceStream &s) const
{
  s << "!" << expr;
}

void MultNode::streamTo(SourceStream &s) const
{
  s << term1 << oper << term2;
}

void AddNode::streamTo(SourceStream &s) const
{
  s << term1 << oper << term2;
}

void ShiftNode::streamTo(SourceStream &s) const
{
  s << term1;
  if (oper == OpLShift)
    s << "<<";
  else if (oper == OpRShift)
    s << ">>";
  else
    s << ">>>";
  s << term2;
}

void RelationalNode::streamTo(SourceStream &s) const
{
  s << expr1;
  switch (oper) {
  case OpLess:
    s << " < ";
    break;
  case OpGreater:
    s << " > ";
    break;
  case OpLessEq:
    s << " <= ";
    break;
  case OpGreaterEq:
    s << " >= ";
    break;
  case OpInstanceOf:
    s << " instanceof ";
    break;
  case OpIn:
    s << " in ";
    break;
  default:
    ;
  }
  s << expr2;
}

void EqualNode::streamTo(SourceStream &s) const
{
  s << expr1;
 switch (oper) {
 case OpEqEq:
   s << " == ";
   break;
 case OpNotEq:
   s << " != ";
   break;
 case OpStrEq:
   s << " === ";
   break;
 case OpStrNEq:
   s << " !== ";
   break;
 default:
   ;
 }
  s << expr2;
}

void BitOperNode::streamTo(SourceStream &s) const
{
  s << expr1;
  if (oper == OpBitAnd)
    s << " & ";
  else if (oper == OpBitXOr)
    s << " ^ ";
  else
    s << " | ";
  s << expr2;
}

void BinaryLogicalNode::streamTo(SourceStream &s) const
{
  s << expr1 << (oper == OpAnd ? " && " : " || ") << expr2;
}

void ConditionalNode::streamTo(SourceStream &s) const
{
  s << logical << " ? " << expr1 << " : " << expr2;
}

static void streamAssignmentOperatorTo(SourceStream &s, Operator oper)
{
  const char *opStr;
  switch (oper) {
  case OpEqual:
    opStr = " = ";
    break;
  case OpMultEq:
    opStr = " *= ";
    break;
  case OpDivEq:
    opStr = " /= ";
    break;
  case OpPlusEq:
    opStr = " += ";
    break;
  case OpMinusEq:
    opStr = " -= ";
    break;
  case OpLShift:
    opStr = " <<= ";
    break;
  case OpRShift:
    opStr = " >>= ";
    break;
  case OpURShift:
    opStr = " >>>= ";
    break;
  case OpAndEq:
    opStr = " &= ";
    break;
  case OpXOrEq:
    opStr = " ^= ";
    break;
  case OpOrEq:
    opStr = " |= ";
    break;
  case OpModEq:
    opStr = " %= ";
    break;
  default:
    opStr = " ?= ";
  }
  s << opStr;
}

void AssignResolveNode::streamTo(SourceStream &s) const
{
  s << m_ident;
  streamAssignmentOperatorTo(s, m_oper);
  s << m_right;
}

void AssignBracketNode::streamTo(SourceStream &s) const
{
  s << m_base << "[" << m_subscript << "]";
  streamAssignmentOperatorTo(s, m_oper);
  s << m_right;
}

void AssignDotNode::streamTo(SourceStream &s) const
{
  s << m_base << "." << m_ident;
  streamAssignmentOperatorTo(s, m_oper);
  s << m_right;
}

void CommaNode::streamTo(SourceStream &s) const
{
  s << expr1 << ", " << expr2;
}

void StatListNode::streamTo(SourceStream &s) const
{
  for (const StatListNode *n = this; n; n = n->list.get())
    s << n->statement;
}

void AssignExprNode::streamTo(SourceStream &s) const
{
  s << " = " << expr;
}

void VarDeclNode::streamTo(SourceStream &s) const
{
  s << ident << init;
}

void VarDeclListNode::streamTo(SourceStream &s) const
{
  s << var;
  for (VarDeclListNode *n = list.get(); n; n = n->list.get())
    s << ", " << n->var;
}

void VarStatementNode::streamTo(SourceStream &s) const
{
  s << SourceStream::Endl << "var " << list << ";";
}

void BlockNode::streamTo(SourceStream &s) const
{
  s << SourceStream::Endl << "{" << SourceStream::Indent
    << source << SourceStream::Unindent << SourceStream::Endl << "}";
}

void EmptyStatementNode::streamTo(SourceStream &s) const
{
  s << SourceStream::Endl << ";";
}

void ExprStatementNode::streamTo(SourceStream &s) const
{
  s << SourceStream::Endl << expr << ";";
}

void IfNode::streamTo(SourceStream &s) const
{
  s << SourceStream::Endl << "if (" << expr << ")" << SourceStream::Indent
    << statement1 << SourceStream::Unindent;
  if (statement2)
    s << SourceStream::Endl << "else" << SourceStream::Indent
      << statement2 << SourceStream::Unindent;
}

void DoWhileNode::streamTo(SourceStream &s) const
{
  s << SourceStream::Endl << "do " << SourceStream::Indent
    << statement << SourceStream::Unindent << SourceStream::Endl
    << "while (" << expr << ");";
}

void WhileNode::streamTo(SourceStream &s) const
{
  s << SourceStream::Endl << "while (" << expr << ")" << SourceStream::Indent
    << statement << SourceStream::Unindent;
}

void ForNode::streamTo(SourceStream &s) const
{
  s << SourceStream::Endl << "for ("
    << expr1  // TODO: doesn't properly do "var i = 0"
    << "; " << expr2
    << "; " << expr3
    << ")" << SourceStream::Indent << statement << SourceStream::Unindent;
}

void ForInNode::streamTo(SourceStream &s) const
{
  s << SourceStream::Endl << "for (";
  if (varDecl)
    s << "var " << varDecl;
  else
    s << lexpr;
  
  if (init)
    s << " = " << init;
  s << " in " << expr << ")" << SourceStream::Indent
    << statement << SourceStream::Unindent;
}

void ContinueNode::streamTo(SourceStream &s) const
{
  s << SourceStream::Endl << "continue";
  if (!ident.isNull())
    s << " " << ident;
  s << ";";
}

void BreakNode::streamTo(SourceStream &s) const
{
  s << SourceStream::Endl << "break";
  if (!ident.isNull())
    s << " " << ident;
  s << ";";
}

void ReturnNode::streamTo(SourceStream &s) const
{
  s << SourceStream::Endl << "return";
  if (value)
    s << " " << value;
  s << ";";
}

void WithNode::streamTo(SourceStream &s) const
{
  s << SourceStream::Endl << "with (" << expr << ") "
    << statement;
}

void CaseClauseNode::streamTo(SourceStream &s) const
{
  s << SourceStream::Endl;
  if (expr)
    s << "case " << expr;
  else
    s << "default";
  s << ":" << SourceStream::Indent;
  if (list)
    s << list;
  s << SourceStream::Unindent;
}

void ClauseListNode::streamTo(SourceStream &s) const
{
  for (const ClauseListNode *n = this; n; n = n->next())
    s << n->clause();
}

void CaseBlockNode::streamTo(SourceStream &s) const
{
  for (const ClauseListNode *n = list1.get(); n; n = n->next())
    s << n->clause();
  if (def)
    s << def;
  for (const ClauseListNode *n = list2.get(); n; n = n->next())
    s << n->clause();
}

void SwitchNode::streamTo(SourceStream &s) const
{
  s << SourceStream::Endl << "switch (" << expr << ") {"
    << SourceStream::Indent << block << SourceStream::Unindent
    << SourceStream::Endl << "}";
}

void LabelNode::streamTo(SourceStream &s) const
{
  s << SourceStream::Endl << label << ":" << SourceStream::Indent
    << statement << SourceStream::Unindent;
}

void ThrowNode::streamTo(SourceStream &s) const
{
  s << SourceStream::Endl << "throw " << expr << ";";
}

void TryNode::streamTo(SourceStream &s) const
{
  s << "try " << tryBlock;
  if (catchBlock)
    s << SourceStream::Endl << "catch (" << exceptionIdent << ")" << catchBlock;
  if (finallyBlock)
    s << SourceStream::Endl << "finally " << finallyBlock;
}

void ParameterNode::streamTo(SourceStream &s) const
{
  s << id;
  for (ParameterNode *n = next.get(); n; n = n->next.get())
    s << ", " << n->id;
}

void FuncDeclNode::streamTo(SourceStream &s) const
{
  s << "function " << ident << "(" << param << ")" << body;
}

void FuncExprNode::streamTo(SourceStream &s) const
{
  s << "function " << ident << "(" << param << ")" << body;
}

void SourceElementsNode::streamTo(SourceStream &s) const
{
  for (const SourceElementsNode *n = this; n; n = n->elements.get())
    s << n->element;
}
