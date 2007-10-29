// -*- c-basic-offset: 2 -*-
/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef NODES_H_
#define NODES_H_

#include "Parser.h"
#include "internal.h"
#include "SymbolTable.h"
#include <wtf/ListRefPtr.h>
#include <wtf/Vector.h>

#if PLATFORM(X86) && COMPILER(GCC)
#define KJS_FAST_CALL __attribute__((regparm(3)))
#else
#define KJS_FAST_CALL
#endif

#if COMPILER(GCC)
#define KJS_NO_INLINE __attribute__((noinline))
#else
#define KJS_NO_INLINE
#endif

namespace KJS {

    class FuncDeclNode;
    class PropertyListNode;
    class SourceElementsNode;
    class SourceStream;
    class VarDeclNode;

    enum Operator {
        OpEqual,
        OpPlusEq,
        OpMinusEq,
        OpMultEq,
        OpDivEq,
        OpPlusPlus,
        OpMinusMinus,
        OpAndEq,
        OpXOrEq,
        OpOrEq,
        OpModEq,
        OpLShift,
        OpRShift,
        OpURShift,
    };

    enum Precedence {
        PrecPrimary,
        PrecMember,
        PrecCall,
        PrecLeftHandSide,
        PrecPostfix,
        PrecUnary,
        PrecMultiplicitave,
        PrecAdditive,
        PrecShift,
        PrecRelational,
        PrecEquality,
        PrecBitwiseAnd,
        PrecBitwiseXor,
        PrecBitwiseOr,
        PrecLogicalAnd,
        PrecLogicalOr,
        PrecConditional,
        PrecAssignment,
        PrecExpression
    };
  
  struct DeclarationStacks {
      typedef Vector<Node*, 16> NodeStack;
      typedef Vector<VarDeclNode*, 16> VarStack;
      typedef Vector<FuncDeclNode*, 16> FunctionStack;
      
      DeclarationStacks(ExecState* e, NodeStack& n, VarStack& v, FunctionStack& f)
        : exec(e)
        , nodeStack(n)
        , varStack(v)
        , functionStack(f)
      {
      }

      ExecState* exec;
      NodeStack& nodeStack;
      VarStack& varStack; 
      FunctionStack& functionStack;
  };

  class Node {
  public:
    Node() KJS_FAST_CALL;
    Node(PlacementNewAdoptType) KJS_FAST_CALL { }
    virtual ~Node();

    virtual JSValue *evaluate(ExecState *exec) KJS_FAST_CALL = 0;
    UString toString() const KJS_FAST_CALL;
    int lineNo() const KJS_FAST_CALL { return m_line; }
    void ref() KJS_FAST_CALL;
    void deref() KJS_FAST_CALL;
    unsigned refcount() KJS_FAST_CALL;
    static void clearNewNodes() KJS_FAST_CALL;

    virtual bool isNumber() const KJS_FAST_CALL { return false; }
    virtual bool isImmediateValue() const KJS_FAST_CALL { return false; }
    virtual bool isLocation() const KJS_FAST_CALL { return false; }
    virtual bool isResolveNode() const KJS_FAST_CALL { return false; }
    virtual bool isBracketAccessorNode() const KJS_FAST_CALL { return false; }
    virtual bool isDotAccessorNode() const KJS_FAST_CALL { return false; }

    // Serialization.
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL = 0;
    virtual Precedence precedence() const = 0;

    // Used for iterative, depth-first traversal of the node tree. Does not cross function call boundaries.
    bool mayHaveDeclarations() const { return m_mayHaveDeclarations; }
    virtual void getDeclarations(DeclarationStacks&) KJS_FAST_CALL { ASSERT_NOT_REACHED(); }
    
    // Used for iterative, depth-first traversal of the node tree. Does not cross function call boundaries.
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL { }

    virtual void breakCycle() KJS_FAST_CALL { }

  protected:
    Completion createErrorCompletion(ExecState *, ErrorType, const char *msg) KJS_FAST_CALL;
    Completion createErrorCompletion(ExecState *, ErrorType, const char *msg, const Identifier &) KJS_FAST_CALL;

    JSValue *throwError(ExecState *, ErrorType, const char *msg) KJS_FAST_CALL;
    JSValue* throwError(ExecState *, ErrorType, const char* msg, const char*) KJS_FAST_CALL;
    JSValue *throwError(ExecState *, ErrorType, const char *msg, JSValue *, Node *) KJS_FAST_CALL;
    JSValue *throwError(ExecState *, ErrorType, const char *msg, const Identifier &) KJS_FAST_CALL;
    JSValue *throwError(ExecState *, ErrorType, const char *msg, JSValue *, const Identifier &) KJS_FAST_CALL;
    JSValue *throwError(ExecState *, ErrorType, const char *msg, JSValue *, Node *, Node *) KJS_FAST_CALL;
    JSValue *throwError(ExecState *, ErrorType, const char *msg, JSValue *, Node *, const Identifier &) KJS_FAST_CALL;

    JSValue *throwUndefinedVariableError(ExecState *, const Identifier &) KJS_FAST_CALL;

    void handleException(ExecState*) KJS_FAST_CALL;
    void handleException(ExecState*, JSValue*) KJS_FAST_CALL;

    Completion rethrowException(ExecState*) KJS_FAST_CALL;

    int m_line : 31;
    bool m_mayHaveDeclarations : 1;
  private:
    // disallow assignment
    Node& operator=(const Node&) KJS_FAST_CALL;
    Node(const Node &other) KJS_FAST_CALL;
  };

  class StatementNode : public Node {
  public:
    StatementNode() KJS_FAST_CALL;
    void setLoc(int line0, int line1) KJS_FAST_CALL;
    int firstLine() const KJS_FAST_CALL { return lineNo(); }
    int lastLine() const KJS_FAST_CALL { return m_lastLine; }
    bool hitStatement(ExecState*) KJS_FAST_CALL;
    virtual Completion execute(ExecState *exec) KJS_FAST_CALL = 0;
    void pushLabel(const Identifier &id) KJS_FAST_CALL { ls.push(id); }
    virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
  protected:
    LabelStack ls;
  private:
    JSValue *evaluate(ExecState*) KJS_FAST_CALL { return jsUndefined(); }
    int m_lastLine;
  };

  class NullNode : public Node {
  public:
    NullNode() KJS_FAST_CALL {}
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPrimary; }
  };

  class BooleanNode : public Node {
  public:
    BooleanNode(bool v) KJS_FAST_CALL : value(v) {}
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPrimary; }
  private:
    bool value;
  };

  class NumberNode : public Node {
  public:
    NumberNode(double v) KJS_FAST_CALL : val(v) {}
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPrimary; }

    virtual bool isNumber() const KJS_FAST_CALL { return true; }
    double value() const KJS_FAST_CALL { return val; }
    void setValue(double v) KJS_FAST_CALL { val = v; }
  private:
    double val;
  };
  
  class ImmediateNumberNode : public Node {
  public:
      ImmediateNumberNode(JSValue* v) KJS_FAST_CALL : m_value(v) {}
      JSValue* evaluate(ExecState*) KJS_FAST_CALL;
      virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
      virtual Precedence precedence() const { return PrecPrimary; }
      
      virtual bool isImmediateValue() const KJS_FAST_CALL { return true; }
      double value() const KJS_FAST_CALL { return JSImmediate::toDouble(m_value); }
      void setValue(double v) KJS_FAST_CALL { m_value = JSImmediate::fromDouble(v); ASSERT(m_value); }
  private:
      JSValue* m_value;
  };

  class StringNode : public Node {
  public:
    StringNode(const UString *v) KJS_FAST_CALL { value = *v; }
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPrimary; }
  private:
    UString value;
  };

  class RegExpNode : public Node {
  public:
    RegExpNode(const UString &p, const UString &f) KJS_FAST_CALL 
      : pattern(p), flags(f) { }
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPrimary; }
  private:
    UString pattern, flags;
  };

  class ThisNode : public Node {
  public:
    ThisNode() KJS_FAST_CALL {}
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPrimary; }
 };

  class ResolveNode : public Node {
  public:
    ResolveNode(const Identifier &s) KJS_FAST_CALL 
        : ident(s) 
    { 
    }

    // Special constructor for cases where we overwrite an object in place.
    ResolveNode(PlacementNewAdoptType) KJS_FAST_CALL 
        : Node(PlacementNewAdopt)
        , ident(PlacementNewAdopt) 
    {
    }

    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;

    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPrimary; }

    virtual bool isLocation() const KJS_FAST_CALL { return true; }
    virtual bool isResolveNode() const KJS_FAST_CALL { return true; }
    const Identifier& identifier() const KJS_FAST_CALL { return ident; }

  protected:
    Identifier ident;
    size_t index; // Used by LocalVarAccessNode.
  };

  class LocalVarAccessNode : public ResolveNode {
  public:
    // Overwrites a ResolveNode in place.
    LocalVarAccessNode(size_t i)
        : ResolveNode(PlacementNewAdopt)
    {
        ASSERT(i != missingSymbolMarker());
        index = i;
    }
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
  };

  class ElementNode : public Node {
  public:
    // list pointer is tail of a circular list, cracked in the ArrayNode ctor
    ElementNode(int e, Node *n) KJS_FAST_CALL : next(this), elision(e), node(n) { Parser::noteNodeCycle(this); }
    ElementNode(ElementNode *l, int e, Node *n) KJS_FAST_CALL
      : next(l->next), elision(e), node(n) { l->next = this; }
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    PassRefPtr<ElementNode> releaseNext() KJS_FAST_CALL { return next.release(); }
    virtual void breakCycle() KJS_FAST_CALL;
    virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
  private:
    friend class ArrayNode;
    ListRefPtr<ElementNode> next;
    int elision;
    RefPtr<Node> node;
  };

  class ArrayNode : public Node {
  public:
    ArrayNode(int e) KJS_FAST_CALL : elision(e), opt(true) { }
    ArrayNode(ElementNode *ele) KJS_FAST_CALL
      : element(ele->next.release()), elision(0), opt(false) { Parser::removeNodeCycle(element.get()); }
    ArrayNode(int eli, ElementNode *ele) KJS_FAST_CALL
      : element(ele->next.release()), elision(eli), opt(true) { Parser::removeNodeCycle(element.get()); }
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPrimary; }
  private:
    RefPtr<ElementNode> element;
    int elision;
    bool opt;
  };

  class PropertyNode : public Node {
  public:
    enum Type { Constant, Getter, Setter };
    PropertyNode(const Identifier& n, Node *a, Type t) KJS_FAST_CALL
      : m_name(n), assign(a), type(t) { }
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    friend class PropertyListNode;
    virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
    const Identifier& name() const { return m_name; }
  private:
    Identifier m_name;
    RefPtr<Node> assign;
    Type type;
  };
  
  class PropertyListNode : public Node {
  public:
    // list pointer is tail of a circular list, cracked in the ObjectLiteralNode ctor
    PropertyListNode(PropertyNode *n) KJS_FAST_CALL
      : node(n), next(this) { Parser::noteNodeCycle(this); }
    PropertyListNode(PropertyNode *n, PropertyListNode *l) KJS_FAST_CALL
      : node(n), next(l->next) { l->next = this; }
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    PassRefPtr<PropertyListNode> releaseNext() KJS_FAST_CALL { return next.release(); }
    virtual void breakCycle() KJS_FAST_CALL;
    virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
  private:
    friend class ObjectLiteralNode;
    RefPtr<PropertyNode> node;
    ListRefPtr<PropertyListNode> next;
  };

  class ObjectLiteralNode : public Node {
  public:
    ObjectLiteralNode() KJS_FAST_CALL { }
    ObjectLiteralNode(PropertyListNode *l) KJS_FAST_CALL : list(l->next.release()) { Parser::removeNodeCycle(list.get()); }
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPrimary; }
  private:
    RefPtr<PropertyListNode> list;
  };

  class BracketAccessorNode : public Node {
  public:
    BracketAccessorNode(Node *e1, Node *e2) KJS_FAST_CALL : expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecMember; }

    virtual bool isLocation() const KJS_FAST_CALL { return true; }
    virtual bool isBracketAccessorNode() const KJS_FAST_CALL { return true; }
    Node *base() KJS_FAST_CALL { return expr1.get(); }
    Node *subscript() KJS_FAST_CALL { return expr2.get(); }

  private:
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
  };

  class DotAccessorNode : public Node {
  public:
    DotAccessorNode(Node *e, const Identifier &s) KJS_FAST_CALL : expr(e), ident(s) { }
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecMember; }

    virtual bool isLocation() const KJS_FAST_CALL { return true; }
    virtual bool isDotAccessorNode() const KJS_FAST_CALL { return true; }
    Node *base() const KJS_FAST_CALL { return expr.get(); }
    const Identifier& identifier() const KJS_FAST_CALL { return ident; }

  private:
    RefPtr<Node> expr;
    Identifier ident;
  };

  class ArgumentListNode : public Node {
  public:
    // list pointer is tail of a circular list, cracked in the ArgumentsNode ctor
    ArgumentListNode(Node *e) KJS_FAST_CALL : next(this), expr(e) { Parser::noteNodeCycle(this); }
    ArgumentListNode(ArgumentListNode *l, Node *e) KJS_FAST_CALL 
      : next(l->next), expr(e) { l->next = this; }
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    List evaluateList(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    PassRefPtr<ArgumentListNode> releaseNext() KJS_FAST_CALL { return next.release(); }
    virtual void breakCycle() KJS_FAST_CALL;
    virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
  private:
    friend class ArgumentsNode;
    ListRefPtr<ArgumentListNode> next;
    RefPtr<Node> expr;
  };

  class ArgumentsNode : public Node {
  public:
    ArgumentsNode() KJS_FAST_CALL { }
    ArgumentsNode(ArgumentListNode *l) KJS_FAST_CALL
      : list(l->next.release()) { Parser::removeNodeCycle(list.get()); }
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    List evaluateList(ExecState *exec) KJS_FAST_CALL { return list ? list->evaluateList(exec) : List(); }
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
  private:
    RefPtr<ArgumentListNode> list;
  };

  class NewExprNode : public Node {
  public:
    NewExprNode(Node *e) KJS_FAST_CALL : expr(e) {}
    NewExprNode(Node *e, ArgumentsNode *a) KJS_FAST_CALL : expr(e), args(a) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecLeftHandSide; }
  private:
    RefPtr<Node> expr;
    RefPtr<ArgumentsNode> args;
  };

  class FunctionCallValueNode : public Node {
  public:
    FunctionCallValueNode(Node *e, ArgumentsNode *a) KJS_FAST_CALL : expr(e), args(a) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecCall; }
  private:
    RefPtr<Node> expr;
    RefPtr<ArgumentsNode> args;
  };

  class FunctionCallResolveNode : public Node {
  public:
    FunctionCallResolveNode(const Identifier& i, ArgumentsNode *a) KJS_FAST_CALL : ident(i), args(a) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecCall; }
  private:
    Identifier ident;
    RefPtr<ArgumentsNode> args;
  };

  class FunctionCallBracketNode : public Node {
  public:
    FunctionCallBracketNode(Node *b, Node *s, ArgumentsNode *a) KJS_FAST_CALL : base(b), subscript(s), args(a) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecCall; }
  protected:
    RefPtr<Node> base;
    RefPtr<Node> subscript;
    RefPtr<ArgumentsNode> args;
  };

  class FunctionCallDotNode : public Node {
  public:
    FunctionCallDotNode(Node *b, const Identifier &i, ArgumentsNode *a) KJS_FAST_CALL : base(b), ident(i), args(a) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecCall; }
  protected:
    RefPtr<Node> base;
    Identifier ident;
    RefPtr<ArgumentsNode> args;
  };

  class PostfixResolveNode : public Node {
  public:
    PostfixResolveNode(const Identifier& i, Operator o) KJS_FAST_CALL : m_ident(i), m_oper(o) {}
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPostfix; }
  private:
    Identifier m_ident;
    Operator m_oper;
  };

  class PostfixBracketNode : public Node {
  public:
    PostfixBracketNode(Node *b, Node *s, Operator o) KJS_FAST_CALL : m_base(b), m_subscript(s), m_oper(o) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPostfix; }
  private:
    RefPtr<Node> m_base;
    RefPtr<Node> m_subscript;
    Operator m_oper;
  };

  class PostfixDotNode : public Node {
  public:
    PostfixDotNode(Node *b, const Identifier& i, Operator o) KJS_FAST_CALL : m_base(b), m_ident(i), m_oper(o) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPostfix; }
  private:
    RefPtr<Node> m_base;
    Identifier m_ident;
    Operator m_oper;
  };

  class PostfixErrorNode : public Node {
  public:
    PostfixErrorNode(Node* e, Operator o) KJS_FAST_CALL : m_expr(e), m_oper(o) {}
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPostfix; }
  private:
    RefPtr<Node> m_expr;
    Operator m_oper;
  };

  class DeleteResolveNode : public Node {
  public:
    DeleteResolveNode(const Identifier& i) KJS_FAST_CALL : m_ident(i) {}
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    Identifier m_ident;
  };

  class DeleteBracketNode : public Node {
  public:
    DeleteBracketNode(Node *base, Node *subscript) KJS_FAST_CALL : m_base(base), m_subscript(subscript) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    RefPtr<Node> m_base;
    RefPtr<Node> m_subscript;
  };

  class DeleteDotNode : public Node {
  public:
    DeleteDotNode(Node *base, const Identifier& i) KJS_FAST_CALL : m_base(base), m_ident(i) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    RefPtr<Node> m_base;
    Identifier m_ident;
  };

  class DeleteValueNode : public Node {
  public:
    DeleteValueNode(Node *e) KJS_FAST_CALL : m_expr(e) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    RefPtr<Node> m_expr;
  };

  class VoidNode : public Node {
  public:
    VoidNode(Node *e) KJS_FAST_CALL : expr(e) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    RefPtr<Node> expr;
  };

  class TypeOfResolveNode : public Node {
  public:
    TypeOfResolveNode(const Identifier& i) KJS_FAST_CALL : m_ident(i) {}
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    Identifier m_ident;
  };

  class TypeOfValueNode : public Node {
  public:
    TypeOfValueNode(Node *e) KJS_FAST_CALL : m_expr(e) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    RefPtr<Node> m_expr;
  };

  class PrefixResolveNode : public Node {
  public:
    PrefixResolveNode(const Identifier& i, Operator o) KJS_FAST_CALL : m_ident(i), m_oper(o) {}
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    Identifier m_ident;
    Operator m_oper;
  };

  class PrefixBracketNode : public Node {
  public:
    PrefixBracketNode(Node *b, Node *s, Operator o) KJS_FAST_CALL : m_base(b), m_subscript(s), m_oper(o) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    RefPtr<Node> m_base;
    RefPtr<Node> m_subscript;
    Operator m_oper;
  };

  class PrefixDotNode : public Node {
  public:
    PrefixDotNode(Node *b, const Identifier& i, Operator o) KJS_FAST_CALL : m_base(b), m_ident(i), m_oper(o) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    RefPtr<Node> m_base;
    Identifier m_ident;
    Operator m_oper;
  };

  class PrefixErrorNode : public Node {
  public:
    PrefixErrorNode(Node* e, Operator o) KJS_FAST_CALL : m_expr(e), m_oper(o) {}
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    RefPtr<Node> m_expr;
    Operator m_oper;
  };

  class UnaryPlusNode : public Node {
  public:
    UnaryPlusNode(Node *e) KJS_FAST_CALL : expr(e) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    RefPtr<Node> expr;
  };

  class NegateNode : public Node {
  public:
    NegateNode(Node *e) KJS_FAST_CALL : expr(e) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    RefPtr<Node> expr;
  };

  class BitwiseNotNode : public Node {
  public:
    BitwiseNotNode(Node *e) KJS_FAST_CALL : expr(e) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    RefPtr<Node> expr;
  };

  class LogicalNotNode : public Node {
  public:
    LogicalNotNode(Node *e) KJS_FAST_CALL : expr(e) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    RefPtr<Node> expr;
  };
  
  class MultNode : public Node {
  public:
      MultNode(Node *t1, Node *t2) KJS_FAST_CALL : term1(t1), term2(t2) {}
      virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
      JSValue* evaluate(ExecState*) KJS_FAST_CALL;
      virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
      virtual Precedence precedence() const { return PrecMultiplicitave; }
  private:
      RefPtr<Node> term1;
      RefPtr<Node> term2;
  };
  
  class DivNode : public Node {
  public:
      DivNode(Node *t1, Node *t2) KJS_FAST_CALL : term1(t1), term2(t2) {}
      virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
      JSValue* evaluate(ExecState*) KJS_FAST_CALL;
      virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
      virtual Precedence precedence() const { return PrecMultiplicitave; }
  private:
      RefPtr<Node> term1;
      RefPtr<Node> term2;
  };
  
  class ModNode : public Node {
  public:
      ModNode(Node *t1, Node *t2) KJS_FAST_CALL : term1(t1), term2(t2) {}
      virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
      JSValue* evaluate(ExecState*) KJS_FAST_CALL;
      virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
      virtual Precedence precedence() const { return PrecMultiplicitave; }
  private:
      RefPtr<Node> term1;
      RefPtr<Node> term2;
  };

  class AddNode : public Node {
  public:
    AddNode(Node *t1, Node *t2) KJS_FAST_CALL : term1(t1), term2(t2) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
      virtual Precedence precedence() const { return PrecAdditive; }
  private:
    RefPtr<Node> term1;
    RefPtr<Node> term2;
  };
  
  class SubNode : public Node {
  public:
      SubNode(Node *t1, Node *t2) KJS_FAST_CALL : term1(t1), term2(t2) {}
      virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
      JSValue* evaluate(ExecState*) KJS_FAST_CALL;
      virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
      virtual Precedence precedence() const { return PrecAdditive; }
  private:
      RefPtr<Node> term1;
      RefPtr<Node> term2;
  };

  class LeftShiftNode : public Node {
  public:
    LeftShiftNode(Node *t1, Node *t2) KJS_FAST_CALL
      : term1(t1), term2(t2) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecShift; }
  private:
    RefPtr<Node> term1;
    RefPtr<Node> term2;
  };

  class RightShiftNode : public Node {
  public:
    RightShiftNode(Node *t1, Node *t2) KJS_FAST_CALL
      : term1(t1), term2(t2) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecShift; }
  private:
    RefPtr<Node> term1;
    RefPtr<Node> term2;
  };

  class UnsignedRightShiftNode : public Node {
  public:
    UnsignedRightShiftNode(Node *t1, Node *t2) KJS_FAST_CALL
      : term1(t1), term2(t2) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecShift; }
  private:
    RefPtr<Node> term1;
    RefPtr<Node> term2;
  };

  class LessNode : public Node {
  public:
    LessNode(Node *e1, Node *e2) KJS_FAST_CALL :
      expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecRelational; }
  private:
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
  };

  class GreaterNode : public Node {
  public:
    GreaterNode(Node *e1, Node *e2) KJS_FAST_CALL :
      expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecRelational; }
  private:
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
  };

  class LessEqNode : public Node {
  public:
    LessEqNode(Node *e1, Node *e2) KJS_FAST_CALL :
      expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecRelational; }
  private:
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
  };

  class GreaterEqNode : public Node {
  public:
    GreaterEqNode(Node *e1, Node *e2) KJS_FAST_CALL :
      expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecRelational; }
  private:
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
  };

  class InstanceOfNode : public Node {
  public:
    InstanceOfNode(Node *e1, Node *e2) KJS_FAST_CALL :
      expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecRelational; }
  private:
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
  };

  class InNode : public Node {
  public:
    InNode(Node *e1, Node *e2) KJS_FAST_CALL :
      expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecRelational; }
  private:
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
  };

  class EqualNode : public Node {
  public:
    EqualNode(Node *e1, Node *e2) KJS_FAST_CALL
      : expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecEquality; }
  private:
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
  };

  class NotEqualNode : public Node {
  public:
    NotEqualNode(Node *e1, Node *e2) KJS_FAST_CALL
      : expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecEquality; }
  private:
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
  };

  class StrictEqualNode : public Node {
  public:
    StrictEqualNode(Node *e1, Node *e2) KJS_FAST_CALL
      : expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecEquality; }
  private:
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
  };

  class NotStrictEqualNode : public Node {
  public:
    NotStrictEqualNode(Node *e1, Node *e2) KJS_FAST_CALL
      : expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecEquality; }
  private:
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
  };

  class BitAndNode : public Node {
  public:
    BitAndNode(Node *e1, Node *e2) KJS_FAST_CALL :
      expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecBitwiseAnd; }
  private:
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
  };

  class BitOrNode : public Node {
  public:
    BitOrNode(Node *e1, Node *e2) KJS_FAST_CALL :
      expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecBitwiseOr; }
  private:
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
  };

  class BitXOrNode : public Node {
  public:
    BitXOrNode(Node *e1, Node *e2) KJS_FAST_CALL :
      expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecBitwiseXor; }
  private:
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
  };

  /**
   * expr1 && expr2, expr1 || expr2
   */
  class LogicalAndNode : public Node {
  public:
    LogicalAndNode(Node *e1, Node *e2) KJS_FAST_CALL :
      expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecLogicalAnd; }
  private:
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
  };
  
  class LogicalOrNode : public Node {
  public:
    LogicalOrNode(Node *e1, Node *e2) KJS_FAST_CALL :
      expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecLogicalOr; }
  private:
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
  };

  /**
   * The ternary operator, "logical ? expr1 : expr2"
   */
  class ConditionalNode : public Node {
  public:
    ConditionalNode(Node *l, Node *e1, Node *e2) KJS_FAST_CALL :
      logical(l), expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecConditional; }
  private:
    RefPtr<Node> logical;
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
  };

  class AssignResolveNode : public Node {
  public:
    AssignResolveNode(const Identifier &ident, Operator oper, Node *right) KJS_FAST_CALL
      : m_ident(ident), m_oper(oper), m_right(right) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecAssignment; }
  protected:
    Identifier m_ident;
    Operator m_oper;
    RefPtr<Node> m_right;
  };

  class AssignBracketNode : public Node {
  public:
    AssignBracketNode(Node *base, Node *subscript, Operator oper, Node *right) KJS_FAST_CALL
      : m_base(base), m_subscript(subscript), m_oper(oper), m_right(right) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecAssignment; }
  protected:
    RefPtr<Node> m_base;
    RefPtr<Node> m_subscript;
    Operator m_oper;
    RefPtr<Node> m_right;
  };

  class AssignDotNode : public Node {
  public:
    AssignDotNode(Node *base, const Identifier& ident, Operator oper, Node *right) KJS_FAST_CALL
      : m_base(base), m_ident(ident), m_oper(oper), m_right(right) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecAssignment; }
  protected:
    RefPtr<Node> m_base;
    Identifier m_ident;
    Operator m_oper;
    RefPtr<Node> m_right;
  };

  class AssignErrorNode : public Node {
  public:
    AssignErrorNode(Node* left, Operator oper, Node* right) KJS_FAST_CALL
      : m_left(left), m_oper(oper), m_right(right) {}
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecAssignment; }
  protected:
    RefPtr<Node> m_left;
    Operator m_oper;
    RefPtr<Node> m_right;
  };

  class CommaNode : public Node {
  public:
    CommaNode(Node *e1, Node *e2) KJS_FAST_CALL : expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecExpression; }
  private:
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
  };

  class AssignExprNode : public Node {
  public:
    AssignExprNode(Node *e) KJS_FAST_CALL : expr(e) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
  private:
    RefPtr<Node> expr;
  };

  class VarDeclNode : public Node {
  public:
    enum Type { Variable, Constant };
    VarDeclNode(const Identifier &id, AssignExprNode *in, Type t) KJS_FAST_CALL;
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual void getDeclarations(DeclarationStacks&) KJS_FAST_CALL;
    virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
    Type varType;
    Identifier ident;
  private:
    JSValue* handleSlowCase(ExecState*, const ScopeChain&, JSValue*) KJS_FAST_CALL KJS_NO_INLINE;
    RefPtr<AssignExprNode> init;
  };

  class VarDeclListNode : public Node {
  public:
    // list pointer is tail of a circular list, cracked in the ForNode/VarStatementNode ctor
    VarDeclListNode(VarDeclNode *v) KJS_FAST_CALL : next(this), var(v) { Parser::noteNodeCycle(this); m_mayHaveDeclarations = true; }
    VarDeclListNode(VarDeclListNode *l, VarDeclNode *v) KJS_FAST_CALL
      : next(l->next), var(v) { l->next = this; m_mayHaveDeclarations = true; }
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    PassRefPtr<VarDeclListNode> releaseNext() KJS_FAST_CALL { return next.release(); }
    virtual void breakCycle() KJS_FAST_CALL;
    virtual void getDeclarations(DeclarationStacks&) KJS_FAST_CALL;
    virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
  private:
    friend class ForNode;
    friend class VarStatementNode;
    ListRefPtr<VarDeclListNode> next;
    RefPtr<VarDeclNode> var;
  };

  class VarStatementNode : public StatementNode {
  public:
    VarStatementNode(VarDeclListNode *l) KJS_FAST_CALL : next(l->next.release()) { Parser::removeNodeCycle(next.get()); m_mayHaveDeclarations = true; }
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual void getDeclarations(DeclarationStacks&) KJS_FAST_CALL;
  private:
    RefPtr<VarDeclListNode> next;
  };

  class BlockNode : public StatementNode {
  public:
    BlockNode(SourceElementsNode *s) KJS_FAST_CALL;
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual void getDeclarations(DeclarationStacks&) KJS_FAST_CALL;
  protected:
    RefPtr<SourceElementsNode> source;
  };

  class EmptyStatementNode : public StatementNode {
  public:
    EmptyStatementNode() KJS_FAST_CALL { } // debug
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
  };

  class ExprStatementNode : public StatementNode {
  public:
    ExprStatementNode(Node *e) KJS_FAST_CALL : expr(e) { }
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
  private:
    RefPtr<Node> expr;
  };

  class IfNode : public StatementNode {
  public:
    IfNode(Node *e, StatementNode *s1, StatementNode *s2) KJS_FAST_CALL
      : expr(e), statement1(s1), statement2(s2) { m_mayHaveDeclarations = statement1->mayHaveDeclarations() || (statement2 && statement2->mayHaveDeclarations()); }
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual void getDeclarations(DeclarationStacks&) KJS_FAST_CALL;
  private:
    RefPtr<Node> expr;
    RefPtr<StatementNode> statement1;
    RefPtr<StatementNode> statement2;
  };

  class DoWhileNode : public StatementNode {
  public:
    DoWhileNode(StatementNode *s, Node *e) KJS_FAST_CALL : statement(s), expr(e) { m_mayHaveDeclarations = true; }
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual void getDeclarations(DeclarationStacks&) KJS_FAST_CALL;
  private:
    RefPtr<StatementNode> statement;
    RefPtr<Node> expr;
  };

  class WhileNode : public StatementNode {
  public:
    WhileNode(Node *e, StatementNode *s) KJS_FAST_CALL : expr(e), statement(s) { m_mayHaveDeclarations = true; }
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual void getDeclarations(DeclarationStacks&) KJS_FAST_CALL;
  private:
    RefPtr<Node> expr;
    RefPtr<StatementNode> statement;
  };

  class ForNode : public StatementNode {
  public:
    ForNode(Node *e1, Node *e2, Node *e3, StatementNode *s) KJS_FAST_CALL :
      expr1(e1), expr2(e2), expr3(e3), statement(s) { m_mayHaveDeclarations = true; }
    ForNode(VarDeclListNode *e1, Node *e2, Node *e3, StatementNode *s) KJS_FAST_CALL :
      expr1(e1->next.release()), expr2(e2), expr3(e3), statement(s) { Parser::removeNodeCycle(expr1.get()); m_mayHaveDeclarations = true; }
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual void getDeclarations(DeclarationStacks&) KJS_FAST_CALL;
  private:
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
    RefPtr<Node> expr3;
    RefPtr<StatementNode> statement;
  };

  class ForInNode : public StatementNode {
  public:
    ForInNode(Node *l, Node *e, StatementNode *s) KJS_FAST_CALL;
    ForInNode(const Identifier &i, AssignExprNode *in, Node *e, StatementNode *s) KJS_FAST_CALL;
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual void getDeclarations(DeclarationStacks&) KJS_FAST_CALL;
  private:
    Identifier ident;
    RefPtr<AssignExprNode> init;
    RefPtr<Node> lexpr;
    RefPtr<Node> expr;
    RefPtr<VarDeclNode> varDecl;
    RefPtr<StatementNode> statement;
  };

  class ContinueNode : public StatementNode {
  public:
    ContinueNode() KJS_FAST_CALL { }
    ContinueNode(const Identifier &i) KJS_FAST_CALL : ident(i) { }
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
  private:
    Identifier ident;
  };

  class BreakNode : public StatementNode {
  public:
    BreakNode() KJS_FAST_CALL { }
    BreakNode(const Identifier &i) KJS_FAST_CALL : ident(i) { }
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
  private:
    Identifier ident;
  };

  class ReturnNode : public StatementNode {
  public:
    ReturnNode(Node *v) KJS_FAST_CALL : value(v) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
  private:
    RefPtr<Node> value;
  };

  class WithNode : public StatementNode {
  public:
    WithNode(Node *e, StatementNode *s) KJS_FAST_CALL : expr(e), statement(s) { m_mayHaveDeclarations = true; }
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual void getDeclarations(DeclarationStacks&) KJS_FAST_CALL;
  private:
    RefPtr<Node> expr;
    RefPtr<StatementNode> statement;
  };

  class LabelNode : public StatementNode {
  public:
    LabelNode(const Identifier &l, StatementNode *s) KJS_FAST_CALL : label(l), statement(s) { m_mayHaveDeclarations = true; }
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual void getDeclarations(DeclarationStacks&) KJS_FAST_CALL;
  private:
    Identifier label;
    RefPtr<StatementNode> statement;
  };

  class ThrowNode : public StatementNode {
  public:
    ThrowNode(Node *e) KJS_FAST_CALL : expr(e) {}
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
  private:
    RefPtr<Node> expr;
  };

  class TryNode : public StatementNode {
  public:
    TryNode(StatementNode *b, const Identifier &e, StatementNode *c, StatementNode *f) KJS_FAST_CALL
      : tryBlock(b), exceptionIdent(e), catchBlock(c), finallyBlock(f) { m_mayHaveDeclarations = true; }
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual void getDeclarations(DeclarationStacks&) KJS_FAST_CALL;
  private:
    RefPtr<StatementNode> tryBlock;
    Identifier exceptionIdent;
    RefPtr<StatementNode> catchBlock;
    RefPtr<StatementNode> finallyBlock;
  };

  class ParameterNode : public Node {
  public:
    // list pointer is tail of a circular list, cracked in the FuncDeclNode/FuncExprNode ctor
    ParameterNode(const Identifier &i) KJS_FAST_CALL : id(i), next(this) { Parser::noteNodeCycle(this); }
    ParameterNode(ParameterNode *next, const Identifier &i) KJS_FAST_CALL
      : id(i), next(next->next) { next->next = this; }
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    Identifier ident() KJS_FAST_CALL { return id; }
    ParameterNode *nextParam() KJS_FAST_CALL { return next.get(); }
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    PassRefPtr<ParameterNode> releaseNext() KJS_FAST_CALL { return next.release(); }
    virtual void breakCycle() KJS_FAST_CALL;
    virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
  private:
    friend class FuncDeclNode;
    friend class FuncExprNode;
    Identifier id;
    ListRefPtr<ParameterNode> next;
  };

  // inherited by ProgramNode
  class FunctionBodyNode : public BlockNode {
  public:
    FunctionBodyNode(SourceElementsNode *) KJS_FAST_CALL;
    int sourceId() KJS_FAST_CALL { return m_sourceId; }
    const UString& sourceURL() KJS_FAST_CALL { return m_sourceURL; }

    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    
    SymbolTable& symbolTable() { return m_symbolTable; }

    void addParam(const Identifier& ident) KJS_FAST_CALL;
    size_t numParams() const KJS_FAST_CALL { return m_parameters.size(); }
    Identifier paramName(size_t pos) const KJS_FAST_CALL { return m_parameters[pos]; }
    UString paramString() const KJS_FAST_CALL;
    Vector<Identifier>& parameters() KJS_FAST_CALL { return m_parameters; }
    ALWAYS_INLINE void processDeclarations(ExecState*);
    ALWAYS_INLINE void processDeclarationsForFunctionCode(ExecState*);
    ALWAYS_INLINE void processDeclarationsForProgramCode(ExecState*);
  private:
    UString m_sourceURL;
    int m_sourceId;

    void initializeDeclarationStacks(ExecState*);
    bool m_initializedDeclarationStacks;

    void initializeSymbolTable();
    bool m_initializedSymbolTable;
    
    void optimizeVariableAccess();
    bool m_optimizedResolveNodes;
    
    // Properties that will go into the ActivationImp's local storage. (Used for initializing the ActivationImp.)
    DeclarationStacks::VarStack m_varStack;
    DeclarationStacks::FunctionStack m_functionStack;
    Vector<Identifier> m_parameters;

    // Mapping from property name -> local storage index. (Used once to transform the AST, and subsequently for residual slow case lookups.)
    SymbolTable m_symbolTable;
  };

  class FuncExprNode : public Node {
  public:
    FuncExprNode(const Identifier &i, FunctionBodyNode *b, ParameterNode *p = 0) KJS_FAST_CALL 
      : ident(i), param(p ? p->next.release() : 0), body(b) { if (p) { Parser::removeNodeCycle(param.get()); } addParams(); }
    virtual JSValue *evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecMember; }
  private:
    void addParams() KJS_FAST_CALL;
    // Used for streamTo
    friend class PropertyNode;
    Identifier ident;
    RefPtr<ParameterNode> param;
    RefPtr<FunctionBodyNode> body;
  };

  class FuncDeclNode : public StatementNode {
  public:
    FuncDeclNode(const Identifier &i, FunctionBodyNode *b) KJS_FAST_CALL
      : ident(i), body(b) { addParams(); m_mayHaveDeclarations = true; }
    FuncDeclNode(const Identifier &i, ParameterNode *p, FunctionBodyNode *b) KJS_FAST_CALL
      : ident(i), param(p->next.release()), body(b) { Parser::removeNodeCycle(param.get()); addParams(); m_mayHaveDeclarations = true; }
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual void getDeclarations(DeclarationStacks&) KJS_FAST_CALL;
    ALWAYS_INLINE FunctionImp* makeFunction(ExecState*) KJS_FAST_CALL;
    Identifier ident;
  private:
    void addParams() KJS_FAST_CALL;
    RefPtr<ParameterNode> param;
    RefPtr<FunctionBodyNode> body;
  };

  // A linked list of source element nodes
  class SourceElementsNode : public StatementNode {
  public:
    static int count;
    // list pointer is tail of a circular list, cracked in the BlockNode (or subclass) ctor
    SourceElementsNode(StatementNode*) KJS_FAST_CALL;
    SourceElementsNode(SourceElementsNode *s1, StatementNode *s2) KJS_FAST_CALL;
    
    virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    PassRefPtr<SourceElementsNode> releaseNext() KJS_FAST_CALL { return next.release(); }
    virtual void breakCycle() KJS_FAST_CALL;
    virtual void getDeclarations(DeclarationStacks&) KJS_FAST_CALL;
  private:
    friend class BlockNode;
    friend class CaseClauseNode;
    RefPtr<StatementNode> node;
    ListRefPtr<SourceElementsNode> next;
  };

  class CaseClauseNode : public Node {
  public:
      CaseClauseNode(Node *e) KJS_FAST_CALL : expr(e) { m_mayHaveDeclarations = true; }
      CaseClauseNode(Node *e, SourceElementsNode *s) KJS_FAST_CALL
      : expr(e), source(s->next.release()) { Parser::removeNodeCycle(source.get()); m_mayHaveDeclarations = true; }
      virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
      JSValue* evaluate(ExecState*) KJS_FAST_CALL;
      Completion evalStatements(ExecState*) KJS_FAST_CALL;
      virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
      virtual void getDeclarations(DeclarationStacks&) KJS_FAST_CALL;
      virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
  private:
      RefPtr<Node> expr;
      RefPtr<SourceElementsNode> source;
  };
  
  class ClauseListNode : public Node {
  public:
      // list pointer is tail of a circular list, cracked in the CaseBlockNode ctor
      ClauseListNode(CaseClauseNode *c) KJS_FAST_CALL : clause(c), next(this) { Parser::noteNodeCycle(this); m_mayHaveDeclarations = true; }
      ClauseListNode(ClauseListNode *n, CaseClauseNode *c) KJS_FAST_CALL
      : clause(c), next(n->next) { n->next = this; m_mayHaveDeclarations = true; }
      virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
      JSValue* evaluate(ExecState*) KJS_FAST_CALL;
      CaseClauseNode *getClause() const KJS_FAST_CALL { return clause.get(); }
      ClauseListNode *getNext() const KJS_FAST_CALL { return next.get(); }
      virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
      PassRefPtr<ClauseListNode> releaseNext() KJS_FAST_CALL { return next.release(); }
      virtual void breakCycle() KJS_FAST_CALL;
      virtual void getDeclarations(DeclarationStacks&) KJS_FAST_CALL;
      virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
  private:
      friend class CaseBlockNode;
      RefPtr<CaseClauseNode> clause;
      ListRefPtr<ClauseListNode> next;
  };
  
  class CaseBlockNode : public Node {
  public:
      CaseBlockNode(ClauseListNode *l1, CaseClauseNode *d, ClauseListNode *l2) KJS_FAST_CALL;
      virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
      JSValue* evaluate(ExecState*) KJS_FAST_CALL;
      Completion evalBlock(ExecState *exec, JSValue *input) KJS_FAST_CALL;
      virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
      virtual void getDeclarations(DeclarationStacks&) KJS_FAST_CALL;
      virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
  private:
      RefPtr<ClauseListNode> list1;
      RefPtr<CaseClauseNode> def;
      RefPtr<ClauseListNode> list2;
  };
  
  class SwitchNode : public StatementNode {
  public:
      SwitchNode(Node *e, CaseBlockNode *b) KJS_FAST_CALL : expr(e), block(b) { m_mayHaveDeclarations = true; }
      virtual void optimizeVariableAccess(FunctionBodyNode*, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
      virtual Completion execute(ExecState*) KJS_FAST_CALL;
      virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
      virtual void getDeclarations(DeclarationStacks&) KJS_FAST_CALL;
  private:
      RefPtr<Node> expr;
      RefPtr<CaseBlockNode> block;
  };
  
  class ProgramNode : public FunctionBodyNode {
  public:
    ProgramNode(SourceElementsNode *s) KJS_FAST_CALL;
  };

} // namespace

#endif
