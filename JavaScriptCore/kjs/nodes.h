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
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
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
    virtual Value evaluate(ExecState *exec) = 0;
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
    Value throwError(ExecState *exec, ErrorType e, const char *msg);
    Value throwError(ExecState *exec, ErrorType e, const char *msg, Value v, Node *expr);
    Value throwError(ExecState *exec, ErrorType e, const char *msg, Identifier label);
    int line;
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
    Value evaluate(ExecState */*exec*/) { return Undefined(); }
    int l0, l1;
    int sid;
    bool breakPoint;
  };

  class NullNode : public Node {
  public:
    NullNode() {}
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  };

  class BooleanNode : public Node {
  public:
    BooleanNode(bool v) : value(v) {}
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    bool value;
  };

  class NumberNode : public Node {
  public:
    NumberNode(double v) : value(v) { }
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    double value;
  };

  class StringNode : public Node {
  public:
    StringNode(const UString *v) { value = *v; }
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    UString value;
  };

  class RegExpNode : public Node {
  public:
    RegExpNode(const UString &p, const UString &f)
      : pattern(p), flags(f) { }
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    UString pattern, flags;
  };

  class ThisNode : public Node {
  public:
    ThisNode() {}
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  };

  class ResolveNode : public Node {
  public:
    ResolveNode(const Identifier &s) : ident(s) { }
    Value evaluate(ExecState *exec);
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
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const { group->streamTo(s); }
  private:
    Node *group;
  };

  class ElementNode : public Node {
  public:
    ElementNode(int e, Node *n) : list(0L), elision(e), node(n) { }
    ElementNode(ElementNode *l, int e, Node *n)
      : list(l), elision(e), node(n) { }
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    friend class ArrayNode;
    ElementNode *list;
    int elision;
    Node *node;
  };

  class ArrayNode : public Node {
  public:
    ArrayNode(int e) : element(0L), elision(e), opt(true) { }
    ArrayNode(ElementNode *ele)
      : element(ele), elision(0), opt(false) { reverseElementList(); }
    ArrayNode(int eli, ElementNode *ele)
      : element(ele), elision(eli), opt(true) { reverseElementList(); }
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    void reverseElementList();
    ElementNode *element;
    int elision;
    bool opt;
  };

  class ObjectLiteralNode : public Node {
  public:
    ObjectLiteralNode(PropertyValueNode *l) : list(l) { reverseList(); }
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    void reverseList();
    PropertyValueNode *list;
  };

  class PropertyValueNode : public Node {
  public:
    PropertyValueNode(PropertyNode *n, Node *a, PropertyValueNode *l = 0L)
      : name(n), assign(a), list(l) { }
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    friend class ObjectLiteralNode;
    PropertyNode *name;
    Node *assign;
    PropertyValueNode *list;
  };

  class PropertyNode : public Node {
  public:
    PropertyNode(double d) : numeric(d) { }
    PropertyNode(const Identifier &s) : str(s) { }
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    double numeric;
    Identifier str;
  };

  class AccessorNode1 : public Node {
  public:
    AccessorNode1(Node *e1, Node *e2) : expr1(e1), expr2(e2) {}
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
    virtual Reference evaluateReference(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr1;
    Node *expr2;
  };

  class AccessorNode2 : public Node {
  public:
    AccessorNode2(Node *e, const Identifier &s) : expr(e), ident(s) { }
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
    virtual Reference evaluateReference(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
    Identifier ident;
  };

  class ArgumentListNode : public Node {
  public:
    ArgumentListNode(Node *e);
    ArgumentListNode(ArgumentListNode *l, Node *e);
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
    List evaluateList(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    friend class ArgumentsNode;
    ArgumentListNode *list;
    Node *expr;
  };

  class ArgumentsNode : public Node {
  public:
    ArgumentsNode(ArgumentListNode *l) : list(l) { reverseList(); }
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
    List evaluateList(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    void reverseList();
    ArgumentListNode *list;
  };

  class NewExprNode : public Node {
  public:
    NewExprNode(Node *e) : expr(e), args(0L) {}
    NewExprNode(Node *e, ArgumentsNode *a) : expr(e), args(a) {}
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
    ArgumentsNode *args;
  };

  class FunctionCallNode : public Node {
  public:
    FunctionCallNode(Node *e, ArgumentsNode *a) : expr(e), args(a) {}
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
    ArgumentsNode *args;
  };

  class PostfixNode : public Node {
  public:
    PostfixNode(Node *e, Operator o) : expr(e), oper(o) {}
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
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
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
  };

  class VoidNode : public Node {
  public:
    VoidNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
  };

  class TypeOfNode : public Node {
  public:
    TypeOfNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
  };

  class PrefixNode : public Node {
  public:
    PrefixNode(Operator o, Node *e) : oper(o), expr(e) {}
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
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
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
  };

  class NegateNode : public Node {
  public:
    NegateNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
  };

  class BitwiseNotNode : public Node {
  public:
    BitwiseNotNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
  };

  class LogicalNotNode : public Node {
  public:
    LogicalNotNode(Node *e) : expr(e) {}
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
  };

  class MultNode : public Node {
  public:
    MultNode(Node *t1, Node *t2, char op) : term1(t1), term2(t2), oper(op) {}
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
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
    Value evaluate(ExecState *exec);
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
    Value evaluate(ExecState *exec);
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
    Value evaluate(ExecState *exec);
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
    Value evaluate(ExecState *exec);
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
    Value evaluate(ExecState *exec);
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
    Value evaluate(ExecState *exec);
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
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *logical, *expr1, *expr2;
  };

  class AssignNode : public Node {
  public:
    AssignNode(Node *l, Operator o, Node *e) : left(l), oper(o), expr(e) {}
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
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
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr1, *expr2;
  };

  class StatListNode : public StatementNode {
  public:
    StatListNode(StatementNode *s) : statement(s), list(0L) { }
    StatListNode(StatListNode *l, StatementNode *s) : statement(s), list(l) { }
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
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Node *expr;
  };

  class VarDeclNode : public Node {
  public:
    VarDeclNode(const Identifier &id, AssignExprNode *in);
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    Identifier ident;
    AssignExprNode *init;
  };

  class VarDeclListNode : public Node {
  public:
    VarDeclListNode(VarDeclNode *v) : list(0L), var(v) {}
    VarDeclListNode(VarDeclListNode *l, VarDeclNode *v) : list(l), var(v) {}
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    friend class VarStatementNode;
    VarDeclListNode *list;
    VarDeclNode *var;
  };

  class VarStatementNode : public StatementNode {
  public:
    VarStatementNode(VarDeclListNode *l) : list(l) { reverseList(); }
    virtual void ref();
    virtual bool deref();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    void reverseList();
    VarDeclListNode *list;
  };

  class BlockNode : public StatementNode {
  public:
    BlockNode(SourceElementsNode *s) : source(s) { reverseList(); }
    virtual void ref();
    virtual bool deref();
    virtual Completion execute(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  protected:
    void reverseList();
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

  class CaseClauseNode: public Node {
  public:
    CaseClauseNode(Node *e, StatListNode *l) : expr(e), list(l) { reverseList(); }
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
    Completion evalStatements(ExecState *exec);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    void reverseList();
    Node *expr;
    StatListNode *list;
  };

  class ClauseListNode : public Node {
  public:
    ClauseListNode(CaseClauseNode *c) : cl(c), nx(0L) { }
    ClauseListNode(ClauseListNode *n, CaseClauseNode *c) : cl(c), nx(n) { }
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
    CaseClauseNode *clause() const { return cl; }
    ClauseListNode *next() const { return nx; }
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    friend class CaseBlockNode;
    CaseClauseNode *cl;
    ClauseListNode *nx;
  };

  class CaseBlockNode: public Node {
  public:
    CaseBlockNode(ClauseListNode *l1, CaseClauseNode *d, ClauseListNode *l2)
      : list1(l1), def(d), list2(l2) { reverseLists(); }
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
    Completion evalBlock(ExecState *exec, const Value& input);
    virtual void processVarDecls(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    void reverseLists();
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
    Completion execute(ExecState *exec, const Value &arg);
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
    TryNode(StatementNode *b, Node *c = 0L, Node *f = 0L)
      : block(b), _catch((CatchNode*)c), _final((FinallyNode*)f) {}
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
    ParameterNode(const Identifier &i) : id(i), next(0L) { }
    ParameterNode(ParameterNode *list, const Identifier &i) : id(i), next(list) { }
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
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
    FuncDeclNode(const Identifier &i, ParameterNode *p, FunctionBodyNode *b)
      : ident(i), param(p), body(b) { reverseParameterList(); }
    virtual void ref();
    virtual bool deref();
    Completion execute(ExecState */*exec*/)
      { /* empty */ return Completion(); }
    void processFuncDecl(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    void reverseParameterList();
    Identifier ident;
    ParameterNode *param;
    FunctionBodyNode *body;
  };

  class FuncExprNode : public Node {
  public:
    FuncExprNode(ParameterNode *p, FunctionBodyNode *b)
	: param(p), body(b) { reverseParameterList(); }
    virtual void ref();
    virtual bool deref();
    Value evaluate(ExecState *exec);
    virtual void streamTo(SourceStream &s) const;
  private:
    void reverseParameterList();
    ParameterNode *param;
    FunctionBodyNode *body;
  };

  // A linked list of source element nodes
  class SourceElementsNode : public StatementNode {
  public:
    SourceElementsNode(StatementNode *s1) { element = s1; elements = 0L; }
    SourceElementsNode(SourceElementsNode *s1, StatementNode *s2)
      { elements = s1; element = s2; }
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

}; // namespace

#endif
