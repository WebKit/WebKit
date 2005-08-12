// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
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

#ifndef _NODES_H_
#define _NODES_H_

#include "fast_malloc.h"

#include "internal.h"
//#include "debugger.h"
#ifndef NDEBUG
#ifndef __osf__
#include <list>
#endif
#endif

namespace KJS {

  class RegExp;
  class SourceElementsNode;
  class ProgramNode;
  class SourceStream;
  class PropertyValueNode;
  class PropertyNode;

  enum Operator { OpEqual,
		  OpEqEq,
		  OpNotEq,
		  OpStrEq,
		  OpStrNEq,
		  OpPlusEq,
		  OpMinusEq,
		  OpMultEq,
		  OpDivEq,
                  OpPlusPlus,
		  OpMinusMinus,
		  OpLess,
		  OpLessEq,
		  OpGreater,
		  OpGreaterEq,
		  OpAndEq,
		  OpXOrEq,
		  OpOrEq,
		  OpModEq,
                  OpAnd,
                  OpOr,
		  OpBitAnd,
		  OpBitXOr,
		  OpBitOr,
		  OpLShift,
		  OpRShift,
		  OpURShift,
		  OpIn,
		  OpInstanceOf
  };

  class Node {
  public:
    Node();
    virtual ~Node();

    KJS_FAST_ALLOCATED;

    virtual ValueImp *evaluate(ExecState *exec) = 0;
    virtual Reference evaluateReference(ExecState *exec);
    UString toString() const;
    virtual void streamTo(SourceStream &s) const = 0;
    virtual void processVarDecls(ExecState */*exec*/) {}
    int lineNo() const { return line; }

  public:
    // reference counting mechanism
    virtual void ref() { refcount++; }
#ifdef KJS_DEBUG_MEM
    virtual bool deref() { assert( refcount > 0 ); return (!--refcount); }
#else
    virtual bool deref() { return (!--refcount); }
#endif


#ifdef KJS_DEBUG_MEM
    static void finalCheck();
#endif
  protected:
    ValueImp *throwError(ExecState *exec, ErrorType e, const char *msg);
    ValueImp *throwError(ExecState *exec, ErrorType e, const char *msg, ValueImp *v, Node *expr);
    ValueImp *throwError(ExecState *exec, ErrorType e, const char *msg, Identifier label);
    ValueImp *throwError(ExecState *exec, ErrorType e, const char *msg, ValueImp *v, Identifier ident);
    ValueImp *throwError(ExecState *exec, ErrorType e, const char *msg, ValueImp *v, Node *e1, Node *e2);
    ValueImp *throwError(ExecState *exec, ErrorType e, const char *msg, ValueImp *v, Node *expr, Identifier ident);

    void setExceptionDetailsIfNeeded(ExecState *exec);
    int line;
    UString sourceURL;
    unsigned int refcount;
    virtual int sourceId() const { return -1; }
  private:
#ifdef KJS_DEBUG_MEM
    // List of all nodes, for debugging purposes. Don't remove!
    static std::list<Node *> *s_nodes;
#endif
    // disallow assignment
    Node& operator=(const Node&);
    Node(const Node &other);
  };

  class StatementNode : public Node {
  public:
    StatementNode();
    void setLoc(int line0, int line1, int sourceId);
    int firstLine() const { return l0; }
    int lastLine() const { return l1; }
    int sourceId() const { return sid; }
    bool hitStatement(ExecState *exec);
    bool abortStatement(ExecState *exec);
    virtual Completion execute(ExecState *exec) = 0;
    void pushLabel(const Identifier &id) { ls.push(id); }
    virtual void processFuncDecl(ExecState *exec);
  protected:
    LabelStack ls;
  private:
    ValueImp *evaluate(ExecState */*exec*/) { return Undefined(); }
    int l0, l1;
    int sid;
    bool breakPoint;
  };

  class NullNode : public Node {
  public:
    NullNode() {}
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  };

  class BooleanNode : public Node {
  public:
    BooleanNode(bool v) : value(v) {}
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    bool value;
  };

  class NumberNode : public Node {
  public:
    NumberNode(double v) : value(v) { }
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    double value;
  };

  class StringNode : public Node {
  public:
    StringNode(const UString *v) { value = *v; }
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    UString value;
  };

  class RegExpNode : public Node {
  public:
    RegExpNode(const UString &p, const UString &f)
      : pattern(p), flags(f) { }
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    UString pattern, flags;
  };

  class ThisNode : public Node {
  public:
    ThisNode() {}
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  };

  class ResolveNode : public Node {
  public:
    ResolveNode(const Identifier &s) : ident(s) { }
    ValueImp *evaluate(ExecState *exec);
    virtual Reference evaluateReference(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Identifier ident;
  };

  class GroupNode : public Node {
  public:
    GroupNode(Node *g) : group(g) { }
    virtual void ref();
    virtual bool deref();
    virtual ValueImp *evaluate(ExecState *exec);
    virtual Reference evaluateReference(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *group;
  };

  class ElementNode : public Node {
  public:
    // list pointer is tail of a circular list, cracked in the ArrayNode ctor
    ElementNode(int e, Node *n) : list(this), elision(e), node(n) { }
    ElementNode(ElementNode *l, int e, Node *n)
      : list(l->list), elision(e), node(n) { l->list = this; }
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    friend class ArrayNode;
    ElementNode *list;
    int elision;
    Node *node;
  };

  class ArrayNode : public Node {
  public:
    ArrayNode(int e) : element(0), elision(e), opt(true) { }
    ArrayNode(ElementNode *ele)
      : element(ele->list), elision(0), opt(false) { ele->list = 0; }
    ArrayNode(int eli, ElementNode *ele)
      : element(ele->list), elision(eli), opt(true) { ele->list = 0; }
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    ElementNode *element;
    int elision;
    bool opt;
  };

  class PropertyValueNode : public Node {
  public:
    // list pointer is tail of a circular list, cracked in the ObjectLiteralNode ctor
    PropertyValueNode(PropertyNode *n, Node *a)
      : name(n), assign(a), list(this) { }
    PropertyValueNode(PropertyNode *n, Node *a, PropertyValueNode *l)
      : name(n), assign(a), list(l->list) { l->list = this; }
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    friend class ObjectLiteralNode;
    PropertyNode *name;
    Node *assign;
    PropertyValueNode *list;
  };

  class ObjectLiteralNode : public Node {
  public:
    ObjectLiteralNode() : list(0) { }
    ObjectLiteralNode(PropertyValueNode *l) : list(l->list) { l->list = 0; }
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    PropertyValueNode *list;
  };

  class PropertyNode : public Node {
  public:
    PropertyNode(double d) : numeric(d) { }
    PropertyNode(const Identifier &s) : str(s) { }
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    double numeric;
    Identifier str;
  };

  class BracketAccessorNode : public Node {
  public:
    BracketAccessorNode(Node *e1, Node *e2) : expr1(e1), expr2(e2) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual Reference evaluateReference(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr1;
    Node *expr2;
  };

  class DotAccessorNode : public Node {
  public:
    DotAccessorNode(Node *e, const Identifier &s) : expr(e), ident(s) { }
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual Reference evaluateReference(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
    Identifier ident;
  };

  class ArgumentListNode : public Node {
  public:
    // list pointer is tail of a circular list, cracked in the ArgumentsNode ctor
    ArgumentListNode(Node *e) : list(this), expr(e) { }
    ArgumentListNode(ArgumentListNode *l, Node *e)
      : list(l->list), expr(e) { l->list = this; }
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    List evaluateList(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    friend class ArgumentsNode;
    ArgumentListNode *list;
    Node *expr;
  };

  class ArgumentsNode : public Node {
  public:
    ArgumentsNode() : list(0) { }
    ArgumentsNode(ArgumentListNode *l)
      : list(l->list) { l->list = 0; }
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    List evaluateList(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    ArgumentListNode *list;
  };

  class NewExprNode : public Node {
  public:
    NewExprNode(Node *e) : expr(e), args(0) {}
    NewExprNode(Node *e, ArgumentsNode *a) : expr(e), args(a) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
    ArgumentsNode *args;
  };

  class FunctionCallValueNode : public Node {
  public:
    FunctionCallValueNode(Node *e, ArgumentsNode *a) : expr(e), args(a) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
    ArgumentsNode *args;
  };

  class FunctionCallResolveNode : public Node {
  public:
    FunctionCallResolveNode(const Identifier& i, ArgumentsNode *a) : ident(i), args(a) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Identifier ident;
    ArgumentsNode *args;
  };

  class FunctionCallBracketNode : public Node {
  public:
    FunctionCallBracketNode(Node *b, Node *s, ArgumentsNode *a) : base(b), subscript(s), args(a) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  protected:
    Node *base;
    Node *subscript;
    ArgumentsNode *args;
  };

  class FunctionCallParenBracketNode : public FunctionCallBracketNode {
  public:
    FunctionCallParenBracketNode(Node *b, Node *s, ArgumentsNode *a) : FunctionCallBracketNode(b, s, a) {}
    virtual void streamTo(SourceStream &s) const;
  };

  class FunctionCallDotNode : public Node {
  public:
    FunctionCallDotNode(Node *b, const Identifier &i, ArgumentsNode *a) : base(b), ident(i), args(a) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  protected:
    Node *base;
    Identifier ident;
    ArgumentsNode *args;
  };

  class FunctionCallParenDotNode : public FunctionCallDotNode {
  public:
    FunctionCallParenDotNode(Node *b, const Identifier &i, ArgumentsNode *a) : FunctionCallDotNode(b, i, a) {}
    virtual void streamTo(SourceStream &s) const;
  };

  class PostfixNode : public Node {
  public:
    PostfixNode(Node *e, Operator o) : expr(e), oper(o) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
    Operator oper;
  };

  class DeleteNode : public Node {
  public:
    DeleteNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
  };

  class VoidNode : public Node {
  public:
    VoidNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
  };

  class TypeOfNode : public Node {
  public:
    TypeOfNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
  };

  class PrefixNode : public Node {
  public:
    PrefixNode(Operator o, Node *e) : oper(o), expr(e) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Operator oper;
    Node *expr;
  };

  class UnaryPlusNode : public Node {
  public:
    UnaryPlusNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
  };

  class NegateNode : public Node {
  public:
    NegateNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
  };

  class BitwiseNotNode : public Node {
  public:
    BitwiseNotNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
  };

  class LogicalNotNode : public Node {
  public:
    LogicalNotNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
  };

  class MultNode : public Node {
  public:
    MultNode(Node *t1, Node *t2, char op) : term1(t1), term2(t2), oper(op) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *term1, *term2;
    char oper;
  };

  class AddNode : public Node {
  public:
    AddNode(Node *t1, Node *t2, char op) : term1(t1), term2(t2), oper(op) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *term1, *term2;
    char oper;
  };

  class ShiftNode : public Node {
  public:
    ShiftNode(Node *t1, Operator o, Node *t2)
      : term1(t1), term2(t2), oper(o) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *term1, *term2;
    Operator oper;
  };

  class RelationalNode : public Node {
  public:
    RelationalNode(Node *e1, Operator o, Node *e2) :
      expr1(e1), expr2(e2), oper(o) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr1, *expr2;
    Operator oper;
  };

  class EqualNode : public Node {
  public:
    EqualNode(Node *e1, Operator o, Node *e2)
      : expr1(e1), expr2(e2), oper(o) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr1, *expr2;
    Operator oper;
  };

  class BitOperNode : public Node {
  public:
    BitOperNode(Node *e1, Operator o, Node *e2) :
      expr1(e1), expr2(e2), oper(o) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr1, *expr2;
    Operator oper;
  };

  /**
   * expr1 && expr2, expr1 || expr2
   */
  class BinaryLogicalNode : public Node {
  public:
    BinaryLogicalNode(Node *e1, Operator o, Node *e2) :
      expr1(e1), expr2(e2), oper(o) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr1, *expr2;
    Operator oper;
  };

  /**
   * The ternary operator, "logical ? expr1 : expr2"
   */
  class ConditionalNode : public Node {
  public:
    ConditionalNode(Node *l, Node *e1, Node *e2) :
      logical(l), expr1(e1), expr2(e2) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *logical, *expr1, *expr2;
  };

  class AssignResolveNode : public Node {
  public:
    AssignResolveNode(const Identifier &ident, Operator oper, Node *right) 
      : m_ident(ident), m_oper(oper), m_right(right) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  protected:
    Identifier m_ident;
    Operator m_oper;
    Node *m_right;
  };

  class AssignBracketNode : public Node {
  public:
    AssignBracketNode(Node *base, Node *subscript, Operator oper, Node *right) 
      : m_base(base), m_subscript(subscript), m_oper(oper), m_right(right) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  protected:
    Node *m_base;
    Node *m_subscript;
    Operator m_oper;
    Node *m_right;
  };

  class AssignDotNode : public Node {
  public:
    AssignDotNode(Node *base, const Identifier& ident, Operator oper, Node *right)
      : m_base(base), m_ident(ident), m_oper(oper), m_right(right) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  protected:
    Node *m_base;
    Identifier m_ident;
    Operator m_oper;
    Node *m_right;
  };

  class CommaNode : public Node {
  public:
    CommaNode(Node *e1, Node *e2) : expr1(e1), expr2(e2) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr1, *expr2;
  };

  class StatListNode : public StatementNode {
  public:
    // list pointer is tail of a circular list, cracked in the CaseClauseNode ctor
    StatListNode(StatementNode *s);
    StatListNode(StatListNode *l, StatementNode *s);
    virtual void ref();
    virtual bool deref();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    friend class CaseClauseNode;
    StatementNode *statement;
    StatListNode *list;
  };

  class AssignExprNode : public Node {
  public:
    AssignExprNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
  };

  class VarDeclNode : public Node {
  public:
    enum Type { Variable, Constant };
    VarDeclNode(const Identifier &id, AssignExprNode *in, Type t);
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Type varType;
    Identifier ident;
    AssignExprNode *init;
  };

  class VarDeclListNode : public Node {
  public:
    // list pointer is tail of a circular list, cracked in the ForNode/VarStatementNode ctor
    VarDeclListNode(VarDeclNode *v) : list(this), var(v) {}
    VarDeclListNode(VarDeclListNode *l, VarDeclNode *v)
      : list(l->list), var(v) { l->list = this; }
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    friend class ForNode;
    friend class VarStatementNode;
    VarDeclListNode *list;
    VarDeclNode *var;
  };

  class VarStatementNode : public StatementNode {
  public:
    VarStatementNode(VarDeclListNode *l) : list(l->list) { l->list = 0; }
    virtual void ref();
    virtual bool deref();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    VarDeclListNode *list;
  };

  class BlockNode : public StatementNode {
  public:
    BlockNode(SourceElementsNode *s);
    virtual void ref();
    virtual bool deref();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  protected:
    SourceElementsNode *source;
  };

  class EmptyStatementNode : public StatementNode {
  public:
    EmptyStatementNode() { } // debug
    virtual Completion execute(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  };

  class ExprStatementNode : public StatementNode {
  public:
    ExprStatementNode(Node *e) : expr(e) { }
    virtual void ref();
    virtual bool deref();
    virtual Completion execute(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
  };

  class IfNode : public StatementNode {
  public:
    IfNode(Node *e, StatementNode *s1, StatementNode *s2)
      : expr(e), statement1(s1), statement2(s2) {}
    virtual void ref();
    virtual bool deref();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
    StatementNode *statement1, *statement2;
  };

  class DoWhileNode : public StatementNode {
  public:
    DoWhileNode(StatementNode *s, Node *e) : statement(s), expr(e) {}
    virtual void ref();
    virtual bool deref();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    StatementNode *statement;
    Node *expr;
  };

  class WhileNode : public StatementNode {
  public:
    WhileNode(Node *e, StatementNode *s) : expr(e), statement(s) {}
    virtual void ref();
    virtual bool deref();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
    StatementNode *statement;
  };

  class ForNode : public StatementNode {
  public:
    ForNode(Node *e1, Node *e2, Node *e3, StatementNode *s) :
      expr1(e1), expr2(e2), expr3(e3), statement(s) {}
    ForNode(VarDeclListNode *e1, Node *e2, Node *e3, StatementNode *s) :
      expr1(e1->list), expr2(e2), expr3(e3), statement(s) { e1->list = 0; }
    virtual void ref();
    virtual bool deref();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr1, *expr2, *expr3;
    StatementNode *statement;
  };

  class ForInNode : public StatementNode {
  public:
    ForInNode(Node *l, Node *e, StatementNode *s);
    ForInNode(const Identifier &i, AssignExprNode *in, Node *e, StatementNode *s);
    virtual void ref();
    virtual bool deref();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Identifier ident;
    AssignExprNode *init;
    Node *lexpr, *expr;
    VarDeclNode *varDecl;
    StatementNode *statement;
  };

  class ContinueNode : public StatementNode {
  public:
    ContinueNode() { }
    ContinueNode(const Identifier &i) : ident(i) { }
    virtual Completion execute(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Identifier ident;
  };

  class BreakNode : public StatementNode {
  public:
    BreakNode() { }
    BreakNode(const Identifier &i) : ident(i) { }
    virtual Completion execute(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Identifier ident;
  };

  class ReturnNode : public StatementNode {
  public:
    ReturnNode(Node *v) : value(v) {}
    virtual void ref();
    virtual bool deref();
    virtual Completion execute(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *value;
  };

  class WithNode : public StatementNode {
  public:
    WithNode(Node *e, StatementNode *s) : expr(e), statement(s) {}
    virtual void ref();
    virtual bool deref();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
    StatementNode *statement;
  };

  class CaseClauseNode : public Node {
  public:
    CaseClauseNode(Node *e) : expr(e), list(0) { }
    CaseClauseNode(Node *e, StatListNode *l)
      : expr(e), list(l->list) { l->list = 0; }
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    Completion evalStatements(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
    StatListNode *list;
  };

  class ClauseListNode : public Node {
  public:
    // list pointer is tail of a circular list, cracked in the CaseBlockNode ctor
    ClauseListNode(CaseClauseNode *c) : cl(c), nx(this) { }
    ClauseListNode(ClauseListNode *n, CaseClauseNode *c)
      : cl(c), nx(n->nx) { n->nx = this; }
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    CaseClauseNode *clause() const { return cl; }
    ClauseListNode *next() const { return nx; }
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    friend class CaseBlockNode;
    CaseClauseNode *cl;
    ClauseListNode *nx;
  };

  class CaseBlockNode : public Node {
  public:
    CaseBlockNode(ClauseListNode *l1, CaseClauseNode *d, ClauseListNode *l2);
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    Completion evalBlock(ExecState *exec, ValueImp *input);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    ClauseListNode *list1;
    CaseClauseNode *def;
    ClauseListNode *list2;
  };

  class SwitchNode : public StatementNode {
  public:
    SwitchNode(Node *e, CaseBlockNode *b) : expr(e), block(b) { }
    virtual void ref();
    virtual bool deref();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
    CaseBlockNode *block;
  };

  class LabelNode : public StatementNode {
  public:
    LabelNode(const Identifier &l, StatementNode *s) : label(l), statement(s) { }
    virtual void ref();
    virtual bool deref();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Identifier label;
    StatementNode *statement;
  };

  class ThrowNode : public StatementNode {
  public:
    ThrowNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    virtual Completion execute(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
  };

  class CatchNode : public StatementNode {
  public:
    CatchNode(const Identifier &i, StatementNode *b) : ident(i), block(b) {}
    virtual void ref();
    virtual bool deref();
    virtual Completion execute(ExecState *exec);
    Completion execute(ExecState *exec, ValueImp *arg);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Identifier ident;
    StatementNode *block;
  };

  class FinallyNode : public StatementNode {
  public:
    FinallyNode(StatementNode *b) : block(b) {}
    virtual void ref();
    virtual bool deref();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    StatementNode *block;
  };

  class TryNode : public StatementNode {
  public:
    TryNode(StatementNode *b, CatchNode *c)
      : block(b), _catch(c), _final(0) {}
    TryNode(StatementNode *b, FinallyNode *f)
      : block(b), _catch(0), _final(f) {}
    TryNode(StatementNode *b, CatchNode *c, FinallyNode *f)
      : block(b), _catch(c), _final(f) {}
    virtual void ref();
    virtual bool deref();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    StatementNode *block;
    CatchNode *_catch;
    FinallyNode *_final;
  };

  class ParameterNode : public Node {
  public:
    // list pointer is tail of a circular list, cracked in the FuncDeclNode/FuncExprNode ctor
    ParameterNode(const Identifier &i) : id(i), next(this) { }
    ParameterNode(ParameterNode *list, const Identifier &i)
      : id(i), next(list->next) { list->next = this; }
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    Identifier ident() { return id; }
    ParameterNode *nextParam() { return next; }
    virtual void streamTo(SourceStream &s) const;
  private:
    friend class FuncDeclNode;
    friend class FuncExprNode;
    Identifier id;
    ParameterNode *next;
  };

  // inherited by ProgramNode
  class FunctionBodyNode : public BlockNode {
  public:
    FunctionBodyNode(SourceElementsNode *s);
    void processFuncDecl(ExecState *exec);
  };

  class FuncDeclNode : public StatementNode {
  public:
    FuncDeclNode(const Identifier &i, FunctionBodyNode *b)
      : ident(i), param(0), body(b) { }
    FuncDeclNode(const Identifier &i, ParameterNode *p, FunctionBodyNode *b)
      : ident(i), param(p->next), body(b) { p->next = 0; }
    virtual void ref();
    virtual bool deref();
    Completion execute(ExecState */*exec*/)
      { /* empty */ return Completion(); }
    void processFuncDecl(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Identifier ident;
    ParameterNode *param;
    FunctionBodyNode *body;
  };

  class FuncExprNode : public Node {
  public:
    FuncExprNode(FunctionBodyNode *b) : param(0), body(b) { }
    FuncExprNode(ParameterNode *p, FunctionBodyNode *b)
      : param(p->next), body(b) { p->next = 0; }
    virtual void ref();
    virtual bool deref();
    ValueImp *evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    ParameterNode *param;
    FunctionBodyNode *body;
  };

  // A linked list of source element nodes
  class SourceElementsNode : public StatementNode {
  public:
    // list pointer is tail of a circular list, cracked in the BlockNode (or subclass) ctor
    SourceElementsNode(StatementNode *s1);
    SourceElementsNode(SourceElementsNode *s1, StatementNode *s2);
    virtual void ref();
    virtual bool deref();
    Completion execute(ExecState *exec);
    void processFuncDecl(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    friend class BlockNode;
    StatementNode *element; // 'this' element
    SourceElementsNode *elements; // pointer to next
  };

  class ProgramNode : public FunctionBodyNode {
  public:
    ProgramNode(SourceElementsNode *s);
  };

} // namespace

#endif
