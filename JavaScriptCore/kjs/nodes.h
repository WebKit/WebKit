// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
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
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef _NODES_H_
#define _NODES_H_

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
    virtual Value evaluate(ExecState *exec) = 0;
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
    Value throwError(ExecState *exec, ErrorType e, const char *msg);
    int line;
    unsigned int refcount;
    virtual int sourceId() const { return -1; }
  private:
#ifdef KJS_DEBUG_MEM
    // List of all nodes, for debugging purposes. Don't remove!
    static std::list<Node *> s_nodes;
#endif
    // disallow assignment
    Node& operator=(const Node&);
    Node(const Node &other);
  };

  class StatementNode : public Node {
  public:
    StatementNode();
    ~StatementNode();
    void setLoc(int line0, int line1, int sourceId);
    int firstLine() const { return l0; }
    int lastLine() const { return l1; }
    int sourceId() const { return sid; }
    bool hitStatement(ExecState *exec);
    bool abortStatement(ExecState *exec);
    virtual Completion execute(ExecState *exec) = 0;
    void pushLabel(const UString *id) {
      if (id) ls.push(*id);
    }
  protected:
    LabelStack ls;
  private:
    Value evaluate(ExecState */*exec*/) { return Undefined(); }
    int l0, l1;
    int sid;
    bool breakPoint;
  };

  class NullNode : public Node {
  public:
    NullNode() {}
    Value evaluate(ExecState *exec);
  };

  class BooleanNode : public Node {
  public:
    BooleanNode(bool v) : value(v) {}
    Value evaluate(ExecState *exec);
  private:
    bool value;
  };

  class NumberNode : public Node {
  public:
    NumberNode(double v) : value(v) { }
    Value evaluate(ExecState *exec);
  private:
    double value;
  };

  class StringNode : public Node {
  public:
    StringNode(const UString *v) { value = *v; }
    Value evaluate(ExecState *exec);
  private:
    UString value;
  };

  class RegExpNode : public Node {
  public:
    RegExpNode(const UString &p, const UString &f)
      : pattern(p), flags(f) { }
    Value evaluate(ExecState *exec);
  private:
    UString pattern, flags;
  };

  class ThisNode : public Node {
  public:
    ThisNode() {}
    Value evaluate(ExecState *exec);
  };

  class ResolveNode : public Node {
  public:
    ResolveNode(const UString *s) : ident(*s) { }
    Value evaluate(ExecState *exec);
  private:
    UString ident;
  };

  class GroupNode : public Node {
  public:
    GroupNode(Node *g) : group(g) { }
    virtual void ref();
    virtual bool deref();
    virtual ~GroupNode();
    Value evaluate(ExecState *exec);
  private:
    Node *group;
  };

  class ElisionNode : public Node {
  public:
    ElisionNode(ElisionNode *e) : elision(e) { }
    virtual void ref();
    virtual bool deref();
    virtual ~ElisionNode();
    Value evaluate(ExecState *exec);
  private:
    ElisionNode *elision;
  };

  class ElementNode : public Node {
  public:
    ElementNode(ElisionNode *e, Node *n) : list(0l), elision(e), node(n) { }
    ElementNode(ElementNode *l, ElisionNode *e, Node *n)
      : list(l), elision(e), node(n) { }
    virtual void ref();
    virtual bool deref();
    virtual ~ElementNode();
    Value evaluate(ExecState *exec);
  private:
    ElementNode *list;
    ElisionNode *elision;
    Node *node;
  };

  class ArrayNode : public Node {
  public:
    ArrayNode(ElisionNode *e) : element(0L), elision(e), opt(true) { }
    ArrayNode(ElementNode *ele)
      : element(ele), elision(0), opt(false) { }
    ArrayNode(ElisionNode *eli, ElementNode *ele)
      : element(ele), elision(eli), opt(true) { }
    virtual void ref();
    virtual bool deref();
    virtual ~ArrayNode();
    Value evaluate(ExecState *exec);
  private:
    ElementNode *element;
    ElisionNode *elision;
    bool opt;
  };

  class ObjectLiteralNode : public Node {
  public:
    ObjectLiteralNode(Node *l) : list(l) { }
    virtual void ref();
    virtual bool deref();
    virtual ~ObjectLiteralNode();
    Value evaluate(ExecState *exec);
  private:
    Node *list;
  };

  class PropertyValueNode : public Node {
  public:
    PropertyValueNode(Node *n, Node *a, Node *l = 0L)
      : name(n), assign(a), list(l) { }
    virtual void ref();
    virtual bool deref();
    virtual ~PropertyValueNode();
    Value evaluate(ExecState *exec);
  private:
    Node *name, *assign, *list;
  };

  class PropertyNode : public Node {
  public:
    PropertyNode(double d) : numeric(d) { }
    PropertyNode(const UString *s) : str(*s) { }
    Value evaluate(ExecState *exec);
  private:
    double numeric;
    UString str;
  };

  class AccessorNode1 : public Node {
  public:
    AccessorNode1(Node *e1, Node *e2) : expr1(e1), expr2(e2) {}
    virtual void ref();
    virtual bool deref();
    virtual ~AccessorNode1();
    Value evaluate(ExecState *exec);
  private:
    Node *expr1;
    Node *expr2;
  };

  class AccessorNode2 : public Node {
  public:
    AccessorNode2(Node *e, const UString *s) : expr(e), ident(*s) { }
    virtual void ref();
    virtual bool deref();
    virtual ~AccessorNode2();
    Value evaluate(ExecState *exec);
  private:
    Node *expr;
    UString ident;
  };

  class ArgumentListNode : public Node {
  public:
    ArgumentListNode(Node *e);
    ArgumentListNode(ArgumentListNode *l, Node *e);
    virtual void ref();
    virtual bool deref();
    virtual ~ArgumentListNode();
    Value evaluate(ExecState *exec);
    List evaluateList(ExecState *exec);
  private:
    ArgumentListNode *list;
    Node *expr;
  };

  class ArgumentsNode : public Node {
  public:
    ArgumentsNode(ArgumentListNode *l);
    virtual void ref();
    virtual bool deref();
    virtual ~ArgumentsNode();
    Value evaluate(ExecState *exec);
    List evaluateList(ExecState *exec);
  private:
    ArgumentListNode *list;
  };

  class NewExprNode : public Node {
  public:
    NewExprNode(Node *e) : expr(e), args(0L) {}
    NewExprNode(Node *e, ArgumentsNode *a) : expr(e), args(a) {}
    virtual void ref();
    virtual bool deref();
    virtual ~NewExprNode();
    Value evaluate(ExecState *exec);
  private:
    Node *expr;
    ArgumentsNode *args;
  };

  class FunctionCallNode : public Node {
  public:
    FunctionCallNode(Node *e, ArgumentsNode *a) : expr(e), args(a) {}
    virtual void ref();
    virtual bool deref();
    virtual ~FunctionCallNode();
    Value evaluate(ExecState *exec);
  private:
    Node *expr;
    ArgumentsNode *args;
  };

  class PostfixNode : public Node {
  public:
    PostfixNode(Node *e, Operator o) : expr(e), oper(o) {}
    virtual void ref();
    virtual bool deref();
    virtual ~PostfixNode();
    Value evaluate(ExecState *exec);
  private:
    Node *expr;
    Operator oper;
  };

  class DeleteNode : public Node {
  public:
    DeleteNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    virtual ~DeleteNode();
    Value evaluate(ExecState *exec);
  private:
    Node *expr;
  };

  class VoidNode : public Node {
  public:
    VoidNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    virtual ~VoidNode();
    Value evaluate(ExecState *exec);
  private:
    Node *expr;
  };

  class TypeOfNode : public Node {
  public:
    TypeOfNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    virtual ~TypeOfNode();
    Value evaluate(ExecState *exec);
  private:
    Node *expr;
  };

  class PrefixNode : public Node {
  public:
    PrefixNode(Operator o, Node *e) : oper(o), expr(e) {}
    virtual void ref();
    virtual bool deref();
    virtual ~PrefixNode();
    Value evaluate(ExecState *exec);
  private:
    Operator oper;
    Node *expr;
  };

  class UnaryPlusNode : public Node {
  public:
    UnaryPlusNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    virtual ~UnaryPlusNode();
    Value evaluate(ExecState *exec);
  private:
    Node *expr;
  };

  class NegateNode : public Node {
  public:
    NegateNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    virtual ~NegateNode();
    Value evaluate(ExecState *exec);
  private:
    Node *expr;
  };

  class BitwiseNotNode : public Node {
  public:
    BitwiseNotNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    virtual ~BitwiseNotNode();
    Value evaluate(ExecState *exec);
  private:
    Node *expr;
  };

  class LogicalNotNode : public Node {
  public:
    LogicalNotNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    virtual ~LogicalNotNode();
    Value evaluate(ExecState *exec);
  private:
    Node *expr;
  };

  class MultNode : public Node {
  public:
    MultNode(Node *t1, Node *t2, char op) : term1(t1), term2(t2), oper(op) {}
    virtual void ref();
    virtual bool deref();
    virtual ~MultNode();
    Value evaluate(ExecState *exec);
  private:
    Node *term1, *term2;
    char oper;
  };

  class AddNode : public Node {
  public:
    AddNode(Node *t1, Node *t2, char op) : term1(t1), term2(t2), oper(op) {}
    virtual void ref();
    virtual bool deref();
    virtual ~AddNode();
    Value evaluate(ExecState *exec);
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
    virtual ~ShiftNode();
    Value evaluate(ExecState *exec);
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
    virtual ~RelationalNode();
    Value evaluate(ExecState *exec);
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
    virtual ~EqualNode();
    Value evaluate(ExecState *exec);
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
    virtual ~BitOperNode();
    Value evaluate(ExecState *exec);
  private:
    Node *expr1, *expr2;
    Operator oper;
  };

  /** expr1 && expr2, expr1 || expr2 */
  class BinaryLogicalNode : public Node {
  public:
    BinaryLogicalNode(Node *e1, Operator o, Node *e2) :
      expr1(e1), expr2(e2), oper(o) {}
    virtual void ref();
    virtual bool deref();
    virtual ~BinaryLogicalNode();
    Value evaluate(ExecState *exec);
  private:
    Node *expr1, *expr2;
    Operator oper;
  };

  /** The ternary operator, "logical ? expr1 : expr2" */
  class ConditionalNode : public Node {
  public:
    ConditionalNode(Node *l, Node *e1, Node *e2) :
      logical(l), expr1(e1), expr2(e2) {}
    virtual void ref();
    virtual bool deref();
    virtual ~ConditionalNode();
    Value evaluate(ExecState *exec);
  private:
    Node *logical, *expr1, *expr2;
  };

  class AssignNode : public Node {
  public:
    AssignNode(Node *l, Operator o, Node *e) : left(l), oper(o), expr(e) {}
    virtual void ref();
    virtual bool deref();
    virtual ~AssignNode();
    Value evaluate(ExecState *exec);
  private:
    Node *left;
    Operator oper;
    Node *expr;
  };

  class CommaNode : public Node {
  public:
    CommaNode(Node *e1, Node *e2) : expr1(e1), expr2(e2) {}
    virtual void ref();
    virtual bool deref();
    virtual ~CommaNode();
    Value evaluate(ExecState *exec);
  private:
    Node *expr1, *expr2;
  };

  class StatListNode : public StatementNode {
  public:
    StatListNode(StatementNode *s) : statement(s), list(0L) { }
    StatListNode(StatListNode *l, StatementNode *s) : statement(s), list(l) { }
    virtual void ref();
    virtual bool deref();
    virtual ~StatListNode();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
  private:
    StatementNode *statement;
    StatListNode *list;
  };

  class AssignExprNode : public Node {
  public:
    AssignExprNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    virtual ~AssignExprNode();
    Value evaluate(ExecState *exec);
  private:
    Node *expr;
  };

  class VarDeclNode : public Node {
  public:
    VarDeclNode(const UString *id, AssignExprNode *in);
    virtual void ref();
    virtual bool deref();
    virtual ~VarDeclNode();
    Value evaluate(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
  private:
    UString ident;
    AssignExprNode *init;
  };

  class VarDeclListNode : public Node {
  public:
    VarDeclListNode(VarDeclNode *v) : list(0L), var(v) {}
    VarDeclListNode(Node *l, VarDeclNode *v) : list(l), var(v) {}
    virtual void ref();
    virtual bool deref();
    virtual ~VarDeclListNode();
    Value evaluate(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
  private:
    Node *list;
    VarDeclNode *var;
  };

  class VarStatementNode : public StatementNode {
  public:
    VarStatementNode(VarDeclListNode *l) : list(l) {}
    virtual void ref();
    virtual bool deref();
    virtual ~VarStatementNode();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
  private:
    VarDeclListNode *list;
  };

  class BlockNode : public StatementNode {
  public:
    BlockNode(SourceElementsNode *s) : source(s) {}
    virtual void ref();
    virtual bool deref();
    virtual ~BlockNode();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
  private:
    SourceElementsNode *source;
  };

  class EmptyStatementNode : public StatementNode {
  public:
    EmptyStatementNode() { } // debug
    virtual Completion execute(ExecState *exec);
  };

  class ExprStatementNode : public StatementNode {
  public:
    ExprStatementNode(Node *e) : expr(e) { }
    virtual void ref();
    virtual bool deref();
    virtual ~ExprStatementNode();
    virtual Completion execute(ExecState *exec);
  private:
    Node *expr;
  };

  class IfNode : public StatementNode {
  public:
    IfNode(Node *e, StatementNode *s1, StatementNode *s2)
      : expr(e), statement1(s1), statement2(s2) {}
    virtual void ref();
    virtual bool deref();
    virtual ~IfNode();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
  private:
    Node *expr;
    StatementNode *statement1, *statement2;
  };

  class DoWhileNode : public StatementNode {
  public:
    DoWhileNode(StatementNode *s, Node *e) : statement(s), expr(e) {}
    virtual void ref();
    virtual bool deref();
    virtual ~DoWhileNode();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
  private:
    StatementNode *statement;
    Node *expr;
  };

  class WhileNode : public StatementNode {
  public:
    WhileNode(Node *e, StatementNode *s) : expr(e), statement(s) {}
    virtual void ref();
    virtual bool deref();
    virtual ~WhileNode();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
  private:
    Node *expr;
    StatementNode *statement;
  };

  class ForNode : public StatementNode {
  public:
    ForNode(Node *e1, Node *e2, Node *e3, StatementNode *s) :
      expr1(e1), expr2(e2), expr3(e3), statement(s) {}
    virtual void ref();
    virtual bool deref();
    virtual ~ForNode();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
  private:
    Node *expr1, *expr2, *expr3;
    StatementNode *statement;
  };

  class ForInNode : public StatementNode {
  public:
    ForInNode(Node *l, Node *e, StatementNode *s);
    ForInNode(const UString *i, AssignExprNode *in, Node *e, StatementNode *s);
    virtual void ref();
    virtual bool deref();
    virtual ~ForInNode();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
  private:
    UString ident;
    AssignExprNode *init;
    Node *lexpr, *expr;
    VarDeclNode *varDecl;
    StatementNode *statement;
  };

  class ContinueNode : public StatementNode {
  public:
    ContinueNode() { }
    ContinueNode(const UString *i) : ident(*i) { }
    virtual Completion execute(ExecState *exec);
  private:
    UString ident;
  };

  class BreakNode : public StatementNode {
  public:
    BreakNode() { }
    BreakNode(const UString *i) : ident(*i) { }
    virtual Completion execute(ExecState *exec);
  private:
    UString ident;
  };

  class ReturnNode : public StatementNode {
  public:
    ReturnNode(Node *v) : value(v) {}
    virtual void ref();
    virtual bool deref();
    virtual ~ReturnNode();
    virtual Completion execute(ExecState *exec);
  private:
    Node *value;
  };

  class WithNode : public StatementNode {
  public:
    WithNode(Node *e, StatementNode *s) : expr(e), statement(s) {}
    virtual void ref();
    virtual bool deref();
    virtual ~WithNode();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
  private:
    Node *expr;
    StatementNode *statement;
  };

  class CaseClauseNode: public Node {
  public:
    CaseClauseNode(Node *e, StatListNode *l) : expr(e), list(l) { }
    virtual void ref();
    virtual bool deref();
    virtual ~CaseClauseNode();
    Value evaluate(ExecState *exec);
    Completion evalStatements(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
  private:
    Node *expr;
    StatListNode *list;
  };

  class ClauseListNode : public Node {
  public:
    ClauseListNode(CaseClauseNode *c) : cl(c), nx(0L) { }
    virtual void ref();
    virtual bool deref();
    virtual ~ClauseListNode();
    ClauseListNode* append(CaseClauseNode *c);
    Value evaluate(ExecState *exec);
    CaseClauseNode *clause() const { return cl; }
    ClauseListNode *next() const { return nx; }
    virtual void processVarDecls(ExecState *exec);
  private:
    CaseClauseNode *cl;
    ClauseListNode *nx;
  };

  class CaseBlockNode: public Node {
  public:
    CaseBlockNode(ClauseListNode *l1, CaseClauseNode *d, ClauseListNode *l2)
      : list1(l1), def(d), list2(l2) { }
    virtual void ref();
    virtual bool deref();
    virtual ~CaseBlockNode();
    Value evaluate(ExecState *exec);
    Completion evalBlock(ExecState *exec, const Value& input);
    virtual void processVarDecls(ExecState *exec);
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
    virtual ~SwitchNode();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
  private:
    Node *expr;
    CaseBlockNode *block;
  };

  class LabelNode : public StatementNode {
  public:
    LabelNode(const UString *l, StatementNode *s) : label(*l), statement(s) { }
    virtual void ref();
    virtual bool deref();
    virtual ~LabelNode();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
  private:
    UString label;
    StatementNode *statement;
  };

  class ThrowNode : public StatementNode {
  public:
    ThrowNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    virtual ~ThrowNode();
    virtual Completion execute(ExecState *exec);
  private:
    Node *expr;
  };

  class CatchNode : public StatementNode {
  public:
    CatchNode(const UString *i, StatementNode *b) : ident(*i), block(b) {}
    virtual void ref();
    virtual bool deref();
    virtual ~CatchNode();
    virtual Completion execute(ExecState *exec);
    Completion execute(ExecState *exec, const Value &arg);
    virtual void processVarDecls(ExecState *exec);
  private:
    UString ident;
    StatementNode *block;
  };

  class FinallyNode : public StatementNode {
  public:
    FinallyNode(StatementNode *b) : block(b) {}
    virtual void ref();
    virtual bool deref();
    virtual ~FinallyNode();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
  private:
    StatementNode *block;
  };

  class TryNode : public StatementNode {
  public:
    TryNode(StatementNode *b, Node *c = 0L, Node *f = 0L)
      : block(b), _catch((CatchNode*)c), _final((FinallyNode*)f) {}
    virtual void ref();
    virtual bool deref();
    virtual ~TryNode();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
  private:
    StatementNode *block;
    CatchNode *_catch;
    FinallyNode *_final;
  };

  class ParameterNode : public Node {
  public:
    ParameterNode(const UString *i) : id(*i), next(0L) { }
    ParameterNode *append(const UString *i);
    virtual void ref();
    virtual bool deref();
    virtual ~ParameterNode();
    Value evaluate(ExecState *exec);
    UString ident() { return id; }
    ParameterNode *nextParam() { return next; }
  private:
    UString id;
    ParameterNode *next;
  };

  // inherited by ProgramNode
  class FunctionBodyNode : public StatementNode {
  public:
    FunctionBodyNode(SourceElementsNode *s);
    virtual void ref();
    virtual bool deref();
    virtual ~FunctionBodyNode();
    Completion execute(ExecState *exec);
    virtual void processFuncDecl(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
  protected:
    SourceElementsNode *source;
  };

  class FuncDeclNode : public StatementNode {
  public:
    FuncDeclNode(const UString *i, ParameterNode *p, FunctionBodyNode *b)
      : ident(*i), param(p), body(b) { }
    virtual void ref();
    virtual bool deref();
    virtual ~FuncDeclNode();
    Completion execute(ExecState */*exec*/)
      { /* empty */ return Completion(); }
    void processFuncDecl(ExecState *exec);
  private:
    UString ident;
    ParameterNode *param;
    FunctionBodyNode *body;
  };

  class FuncExprNode : public Node {
  public:
    FuncExprNode(ParameterNode *p, FunctionBodyNode *b)
	: param(p), body(b) { }
    virtual void ref();
    virtual bool deref();
    virtual ~FuncExprNode();
    Value evaluate(ExecState *exec);
  private:
    ParameterNode *param;
    FunctionBodyNode *body;
  };

  class SourceElementNode : public StatementNode {
  public:
    SourceElementNode(StatementNode *s) { statement = s; function = 0L; }
    SourceElementNode(FuncDeclNode *f) { function = f; statement = 0L;}
    virtual void ref();
    virtual bool deref();
    virtual ~SourceElementNode();
    Completion execute(ExecState *exec);
    virtual void processFuncDecl(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
  private:
    StatementNode *statement;
    FuncDeclNode *function;
  };

  // A linked list of source element nodes
  class SourceElementsNode : public StatementNode {
  public:
    SourceElementsNode(SourceElementNode *s1) { element = s1; elements = 0L; }
    SourceElementsNode(SourceElementsNode *s1, SourceElementNode *s2)
      { elements = s1; element = s2; }
    virtual void ref();
    virtual bool deref();
    virtual ~SourceElementsNode();
    Completion execute(ExecState *exec);
    virtual void processFuncDecl(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
  private:
    SourceElementNode *element; // 'this' element
    SourceElementsNode *elements; // pointer to next
  };

  class ProgramNode : public FunctionBodyNode {
  public:
    ProgramNode(SourceElementsNode *s);
    ~ProgramNode();
  private:
    // Disallow copy
    ProgramNode(const ProgramNode &other);
  };

}; // namespace

#endif
