/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
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

#ifndef NODES_H_
#define NODES_H_

#include "internal.h"
#include "regexp.h"
#include "SymbolTable.h"
#include <wtf/ListRefPtr.h>
#include <wtf/OwnPtr.h>
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
    class Node;
    class PropertyListNode;
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

  class ParserRefCounted : Noncopyable {
  protected:
    ParserRefCounted() KJS_FAST_CALL;
    ParserRefCounted(PlacementNewAdoptType) KJS_FAST_CALL { }
    
  public:
    void ref() KJS_FAST_CALL;
    void deref() KJS_FAST_CALL;
    unsigned refcount() KJS_FAST_CALL;
    
    static void deleteNewObjects() KJS_FAST_CALL;
  
    virtual ~ParserRefCounted();
  };

  class Node : public ParserRefCounted {
  public:
    Node() KJS_FAST_CALL;
    Node(PlacementNewAdoptType placementAdopt) KJS_FAST_CALL
    : ParserRefCounted(placementAdopt) { }

    UString toString() const KJS_FAST_CALL;
    int lineNo() const KJS_FAST_CALL { return m_line; }

    // Serialization.
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL = 0;
    virtual Precedence precedence() const = 0;
    
    // Used for iterative, depth-first traversal of the node tree. Does not cross function call boundaries.
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL { }

  protected:
    Node(JSType) KJS_FAST_CALL; // used by ExpressionNode
    Completion createErrorCompletion(ExecState *, ErrorType, const char *msg) KJS_FAST_CALL;
    Completion createErrorCompletion(ExecState *, ErrorType, const char *msg, const Identifier &) KJS_FAST_CALL;

    JSValue* throwError(ExecState*, ErrorType, const char* msg) KJS_FAST_CALL;
    JSValue* throwError(ExecState*, ErrorType, const char* msg, const char*) KJS_FAST_CALL;
    JSValue* throwError(ExecState*, ErrorType, const char* msg, JSValue*, Node*) KJS_FAST_CALL;
    JSValue* throwError(ExecState*, ErrorType, const char* msg, const Identifier&) KJS_FAST_CALL;
    JSValue* throwError(ExecState*, ErrorType, const char* msg, JSValue*, const Identifier&) KJS_FAST_CALL;
    JSValue* throwError(ExecState*, ErrorType, const char* msg, JSValue*, Node*, Node*) KJS_FAST_CALL;
    JSValue* throwError(ExecState*, ErrorType, const char* msg, JSValue*, Node*, const Identifier&) KJS_FAST_CALL;

    JSValue* throwUndefinedVariableError(ExecState*, const Identifier&) KJS_FAST_CALL;

    void handleException(ExecState*) KJS_FAST_CALL;
    void handleException(ExecState*, JSValue*) KJS_FAST_CALL;

    Completion rethrowException(ExecState*) KJS_FAST_CALL;

    int m_line : 28;
    unsigned m_expectedReturnType : 3; // JSType
  };
    
    class ExpressionNode : public Node {
    public:
        ExpressionNode() KJS_FAST_CALL : Node() {}
        ExpressionNode(JSType expectedReturn) KJS_FAST_CALL
            : Node(expectedReturn) {}
        
        // Special constructor for cases where we overwrite an object in place.
        ExpressionNode(PlacementNewAdoptType) KJS_FAST_CALL
            : Node(PlacementNewAdopt) {}
        
        virtual bool isNumber() const KJS_FAST_CALL { return false; }
        virtual bool isLocation() const KJS_FAST_CALL { return false; }
        virtual bool isResolveNode() const KJS_FAST_CALL { return false; }
        virtual bool isBracketAccessorNode() const KJS_FAST_CALL { return false; }
        virtual bool isDotAccessorNode() const KJS_FAST_CALL { return false; }
        
        JSType expectedReturnType() const KJS_FAST_CALL { return static_cast<JSType>(m_expectedReturnType); }
        
        virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL = 0;
        virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
        virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
        virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
        virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;

        // Used to optimize those nodes that do extra work when returning a result, even if the result has no semantic relevance
        virtual void optimizeForUnnecessaryResult() { }
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
    int m_lastLine;
  };

  class NullNode : public ExpressionNode {
  public:
    NullNode() KJS_FAST_CALL : ExpressionNode(NullType) {}
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPrimary; }
  };

  class FalseNode : public ExpressionNode {
  public:
    FalseNode() KJS_FAST_CALL : ExpressionNode(BooleanType) {}
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL { return false; }
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPrimary; }
  };

  class TrueNode : public ExpressionNode {
  public:
    TrueNode() KJS_FAST_CALL : ExpressionNode(BooleanType) {}
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL { return true; }
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPrimary; }
  };

  class NumberNode : public ExpressionNode {
  public:
    NumberNode(double v) KJS_FAST_CALL : ExpressionNode(NumberType), m_double(v) {}
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
    virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPrimary; }

    virtual bool isNumber() const KJS_FAST_CALL { return true; }
    double value() const KJS_FAST_CALL { return m_double; }
    virtual void setValue(double d) KJS_FAST_CALL { m_double = d; }

  protected:
    double m_double;
  };
  
  class ImmediateNumberNode : public NumberNode {
  public:
      ImmediateNumberNode(JSValue* v, double d) KJS_FAST_CALL : NumberNode(d), m_value(v) { ASSERT(v == JSImmediate::from(d)); }
      virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
      virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
      virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
      
      virtual void setValue(double d) KJS_FAST_CALL { m_double = d; m_value = JSImmediate::from(d); ASSERT(m_value); }
  private:
      JSValue* m_value; // This is never a JSCell, only JSImmediate, thus no ProtectedPtr
  };

  class StringNode : public ExpressionNode {
  public:
    StringNode(const UString* v) KJS_FAST_CALL : ExpressionNode(StringType), m_value(*v) {}
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPrimary; }

  private:
    UString m_value;
  };

  class RegExpNode : public ExpressionNode {
  public:
    RegExpNode(const UString& pattern, const UString& flags) KJS_FAST_CALL 
        : m_regExp(new RegExp(pattern, flags))
    {
    }
    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPrimary; }
  private:
    RefPtr<RegExp> m_regExp;
  };

  class ThisNode : public ExpressionNode {
  public:
    ThisNode() KJS_FAST_CALL {}
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPrimary; }
 };

  class ResolveNode : public ExpressionNode {
  public:
    ResolveNode(const Identifier &s) KJS_FAST_CALL 
        : ident(s) 
    { 
    }

    // Special constructor for cases where we overwrite an object in place.
    ResolveNode(PlacementNewAdoptType) KJS_FAST_CALL 
        : ExpressionNode(PlacementNewAdopt)
        , ident(PlacementNewAdopt)
    {
    }

    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;

    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
    virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPrimary; }

    virtual bool isLocation() const KJS_FAST_CALL { return true; }
    virtual bool isResolveNode() const KJS_FAST_CALL { return true; }
    const Identifier& identifier() const KJS_FAST_CALL { return ident; }

  protected:
    ALWAYS_INLINE JSValue* inlineEvaluate(ExecState*);
    Identifier ident;
    size_t index; // Used by LocalVarAccessNode.
  };

  class LocalVarAccessNode : public ResolveNode {
  public:
    // Overwrites a ResolveNode in place.
    LocalVarAccessNode(size_t i) KJS_FAST_CALL
        : ResolveNode(PlacementNewAdopt)
    {
        ASSERT(i != missingSymbolMarker());
        index = i;
    }
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
    virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
  private:
    ALWAYS_INLINE JSValue* inlineEvaluate(ExecState*);
  };

  class ElementNode : public Node {
  public:
    ElementNode(int e, ExpressionNode* n) KJS_FAST_CALL : elision(e), node(n) { }
    ElementNode(ElementNode* l, int e, ExpressionNode* n) KJS_FAST_CALL
      : elision(e), node(n) { l->next = this; }
    virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;

    PassRefPtr<ElementNode> releaseNext() KJS_FAST_CALL { return next.release(); }

    JSValue* evaluate(ExecState*) KJS_FAST_CALL;

  private:
    friend class ArrayNode;
    ListRefPtr<ElementNode> next;
    int elision;
    RefPtr<ExpressionNode> node;
  };

  class ArrayNode : public ExpressionNode {
  public:
    ArrayNode(int e) KJS_FAST_CALL : elision(e), opt(true) { }
    ArrayNode(ElementNode* ele) KJS_FAST_CALL
      : element(ele), elision(0), opt(false) { }
    ArrayNode(int eli, ElementNode* ele) KJS_FAST_CALL
      : element(ele), elision(eli), opt(true) { }
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
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
    PropertyNode(const Identifier& n, ExpressionNode* a, Type t) KJS_FAST_CALL
      : m_name(n), assign(a), type(t) { }
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    const Identifier& name() const { return m_name; }

  private:
    friend class PropertyListNode;
    Identifier m_name;
    RefPtr<ExpressionNode> assign;
    Type type;
  };
  
  class PropertyListNode : public Node {
  public:
    PropertyListNode(PropertyNode* n) KJS_FAST_CALL
      : node(n) { }
    PropertyListNode(PropertyNode* n, PropertyListNode* l) KJS_FAST_CALL
      : node(n) { l->next = this; }
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

    JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    PassRefPtr<PropertyListNode> releaseNext() KJS_FAST_CALL { return next.release(); }

  private:
    friend class ObjectLiteralNode;
    RefPtr<PropertyNode> node;
    ListRefPtr<PropertyListNode> next;
  };

  class ObjectLiteralNode : public ExpressionNode {
  public:
    ObjectLiteralNode() KJS_FAST_CALL { }
    ObjectLiteralNode(PropertyListNode* l) KJS_FAST_CALL : list(l) { }
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPrimary; }
  private:
    RefPtr<PropertyListNode> list;
  };

  class BracketAccessorNode : public ExpressionNode {
  public:
    BracketAccessorNode(ExpressionNode* e1, ExpressionNode* e2) KJS_FAST_CALL : expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
    virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecMember; }

    virtual bool isLocation() const KJS_FAST_CALL { return true; }
    virtual bool isBracketAccessorNode() const KJS_FAST_CALL { return true; }
    ExpressionNode* base() KJS_FAST_CALL { return expr1.get(); }
    ExpressionNode* subscript() KJS_FAST_CALL { return expr2.get(); }

  private:
    ALWAYS_INLINE JSValue* inlineEvaluate(ExecState*);
    RefPtr<ExpressionNode> expr1;
    RefPtr<ExpressionNode> expr2;
  };

  class DotAccessorNode : public ExpressionNode {
  public:
    DotAccessorNode(ExpressionNode* e, const Identifier& s) KJS_FAST_CALL : expr(e), ident(s) { }
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
    virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecMember; }

    virtual bool isLocation() const KJS_FAST_CALL { return true; }
    virtual bool isDotAccessorNode() const KJS_FAST_CALL { return true; }
    ExpressionNode* base() const KJS_FAST_CALL { return expr.get(); }
    const Identifier& identifier() const KJS_FAST_CALL { return ident; }

  private:
    ALWAYS_INLINE JSValue* inlineEvaluate(ExecState*);
    RefPtr<ExpressionNode> expr;
    Identifier ident;
  };

  class ArgumentListNode : public Node {
  public:
    ArgumentListNode(ExpressionNode* e) KJS_FAST_CALL : expr(e) { }
    ArgumentListNode(ArgumentListNode* l, ExpressionNode* e) KJS_FAST_CALL 
      : expr(e) { l->next = this; }
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

    void evaluateList(ExecState*, List&) KJS_FAST_CALL;
    PassRefPtr<ArgumentListNode> releaseNext() KJS_FAST_CALL { return next.release(); }

  private:
    friend class ArgumentsNode;
    ListRefPtr<ArgumentListNode> next;
    RefPtr<ExpressionNode> expr;
  };

  class ArgumentsNode : public Node {
  public:
    ArgumentsNode() KJS_FAST_CALL { }
    ArgumentsNode(ArgumentListNode* l) KJS_FAST_CALL
      : listNode(l) { }
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

    void evaluateList(ExecState* exec, List& list) KJS_FAST_CALL { if (listNode) listNode->evaluateList(exec, list); }

  private:
    RefPtr<ArgumentListNode> listNode;
  };

  class NewExprNode : public ExpressionNode {
  public:
    NewExprNode(ExpressionNode* e) KJS_FAST_CALL : expr(e) {}
    NewExprNode(ExpressionNode* e, ArgumentsNode* a) KJS_FAST_CALL : expr(e), args(a) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
    virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecLeftHandSide; }
  private:
    ALWAYS_INLINE JSValue* inlineEvaluate(ExecState*);
    RefPtr<ExpressionNode> expr;
    RefPtr<ArgumentsNode> args;
  };

  class FunctionCallValueNode : public ExpressionNode {
  public:
    FunctionCallValueNode(ExpressionNode* e, ArgumentsNode* a) KJS_FAST_CALL : expr(e), args(a) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecCall; }
  private:
    RefPtr<ExpressionNode> expr;
    RefPtr<ArgumentsNode> args;
  };

  class FunctionCallResolveNode : public ExpressionNode {
  public:
    FunctionCallResolveNode(const Identifier& i, ArgumentsNode* a) KJS_FAST_CALL
        : ident(i)
        , args(a)
    {
    }

    FunctionCallResolveNode(PlacementNewAdoptType) KJS_FAST_CALL 
        : ExpressionNode(PlacementNewAdopt)
        , ident(PlacementNewAdopt)
        , args(PlacementNewAdopt)
    {
    }

    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
    virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecCall; }

  protected:
    ALWAYS_INLINE JSValue* inlineEvaluate(ExecState*);
    Identifier ident;
    RefPtr<ArgumentsNode> args;
    size_t index; // Used by LocalVarFunctionCallNode.
  };

  class LocalVarFunctionCallNode : public FunctionCallResolveNode {
  public:
    LocalVarFunctionCallNode(size_t i) KJS_FAST_CALL
        : FunctionCallResolveNode(PlacementNewAdopt)
    {
        ASSERT(i != missingSymbolMarker());
        index = i;
    }

    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
    virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
  private:
    ALWAYS_INLINE JSValue* inlineEvaluate(ExecState*);
  };

  class FunctionCallBracketNode : public ExpressionNode {
  public:
    FunctionCallBracketNode(ExpressionNode* b, ExpressionNode* s, ArgumentsNode* a) KJS_FAST_CALL : base(b), subscript(s), args(a) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecCall; }
  protected:
    RefPtr<ExpressionNode> base;
    RefPtr<ExpressionNode> subscript;
    RefPtr<ArgumentsNode> args;
  };

  class FunctionCallDotNode : public ExpressionNode {
  public:
    FunctionCallDotNode(ExpressionNode* b, const Identifier& i, ArgumentsNode* a) KJS_FAST_CALL : base(b), ident(i), args(a) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
    virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecCall; }
  private:
    ALWAYS_INLINE JSValue* inlineEvaluate(ExecState*);
    RefPtr<ExpressionNode> base;
    Identifier ident;
    RefPtr<ArgumentsNode> args;
  };

  class PrePostResolveNode : public ExpressionNode {
  public:
    PrePostResolveNode(const Identifier& i) KJS_FAST_CALL : ExpressionNode(NumberType), m_ident(i) {}
      
    PrePostResolveNode(PlacementNewAdoptType) KJS_FAST_CALL 
        : ExpressionNode(PlacementNewAdopt)
        , m_ident(PlacementNewAdopt)
    {
    }
      
  protected:
      Identifier m_ident;
      size_t m_index; // Used by LocalVarPostfixNode.      
  };
    
  class PostIncResolveNode : public PrePostResolveNode {
  public:
    PostIncResolveNode(const Identifier& i) KJS_FAST_CALL : PrePostResolveNode(i) {}

    PostIncResolveNode(PlacementNewAdoptType) KJS_FAST_CALL 
        : PrePostResolveNode(PlacementNewAdopt)
    {
    }

    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPostfix; }
    virtual void optimizeForUnnecessaryResult();

  };

  class PostIncLocalVarNode : public PostIncResolveNode {
  public:
    PostIncLocalVarNode(size_t i) KJS_FAST_CALL
        : PostIncResolveNode(PlacementNewAdopt)
    {
        ASSERT(i != missingSymbolMarker());
        m_index = i;
    }

    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void optimizeForUnnecessaryResult();
  };

  class PostDecResolveNode : public PrePostResolveNode {
  public:
    PostDecResolveNode(const Identifier& i) KJS_FAST_CALL : PrePostResolveNode(i) {}

    PostDecResolveNode(PlacementNewAdoptType) KJS_FAST_CALL 
        : PrePostResolveNode(PlacementNewAdopt)
    {
    }

    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPostfix; }
    virtual void optimizeForUnnecessaryResult();
  };

  class PostDecLocalVarNode : public PostDecResolveNode {
  public:
    PostDecLocalVarNode(size_t i) KJS_FAST_CALL
        : PostDecResolveNode(PlacementNewAdopt)
    {
        ASSERT(i != missingSymbolMarker());
        m_index = i;
    }

    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
    virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
    virtual void optimizeForUnnecessaryResult();
  private:
    ALWAYS_INLINE double inlineEvaluateToNumber(ExecState*);
  };

  class PostfixBracketNode : public ExpressionNode {
  public:
    PostfixBracketNode(ExpressionNode* b, ExpressionNode* s) KJS_FAST_CALL : m_base(b), m_subscript(s) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPostfix; }
  protected:
    virtual bool isIncrement() const = 0;
    RefPtr<ExpressionNode> m_base;
    RefPtr<ExpressionNode> m_subscript;
  };

  class PostIncBracketNode : public PostfixBracketNode {
  public:
    PostIncBracketNode(ExpressionNode* b, ExpressionNode* s) KJS_FAST_CALL : PostfixBracketNode(b, s) {}
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
  protected:
    virtual bool isIncrement() const { return true; }
  };

  class PostDecBracketNode : public PostfixBracketNode {
  public:
    PostDecBracketNode(ExpressionNode* b, ExpressionNode* s) KJS_FAST_CALL : PostfixBracketNode(b, s) {}
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
  protected:
    virtual bool isIncrement() const { return false; }
  };

    class PostfixDotNode : public ExpressionNode {
  public:
    PostfixDotNode(ExpressionNode* b, const Identifier& i) KJS_FAST_CALL : m_base(b), m_ident(i) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPostfix; }
  protected:
    virtual bool isIncrement() const = 0;
    RefPtr<ExpressionNode> m_base;
    Identifier m_ident;
  };

  class PostIncDotNode : public PostfixDotNode {
  public:
    PostIncDotNode(ExpressionNode*  b, const Identifier& i) KJS_FAST_CALL : PostfixDotNode(b, i) {}
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
  protected:
    virtual bool isIncrement() const { return true; }
  };

  class PostDecDotNode : public PostfixDotNode {
  public:
    PostDecDotNode(ExpressionNode*  b, const Identifier& i) KJS_FAST_CALL : PostfixDotNode(b, i) {}
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
  protected:
    virtual bool isIncrement() const { return false; }
  };

  class PostfixErrorNode : public ExpressionNode {
  public:
    PostfixErrorNode(ExpressionNode* e, Operator o) KJS_FAST_CALL : m_expr(e), m_oper(o) {}
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPostfix; }
  private:
    RefPtr<ExpressionNode> m_expr;
    Operator m_oper;
  };

  class DeleteResolveNode : public ExpressionNode {
  public:
    DeleteResolveNode(const Identifier& i) KJS_FAST_CALL : m_ident(i) {}
    DeleteResolveNode(PlacementNewAdoptType) KJS_FAST_CALL 
        : ExpressionNode(PlacementNewAdopt)
        , m_ident(PlacementNewAdopt)
    {
    }

    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    Identifier m_ident;
  };

  class LocalVarDeleteNode : public DeleteResolveNode {
  public:
    LocalVarDeleteNode() KJS_FAST_CALL
        : DeleteResolveNode(PlacementNewAdopt)
    {
    }

    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
  };

  class DeleteBracketNode : public ExpressionNode {
  public:
    DeleteBracketNode(ExpressionNode* base, ExpressionNode* subscript) KJS_FAST_CALL : m_base(base), m_subscript(subscript) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    RefPtr<ExpressionNode> m_base;
    RefPtr<ExpressionNode> m_subscript;
  };

  class DeleteDotNode : public ExpressionNode {
  public:
    DeleteDotNode(ExpressionNode* base, const Identifier& i) KJS_FAST_CALL : m_base(base), m_ident(i) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    RefPtr<ExpressionNode> m_base;
    Identifier m_ident;
  };

  class DeleteValueNode : public ExpressionNode {
  public:
    DeleteValueNode(ExpressionNode* e) KJS_FAST_CALL : m_expr(e) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    RefPtr<ExpressionNode> m_expr;
  };

  class VoidNode : public ExpressionNode {
  public:
    VoidNode(ExpressionNode* e) KJS_FAST_CALL : expr(e) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    RefPtr<ExpressionNode> expr;
  };

  class TypeOfResolveNode : public ExpressionNode {
  public:
    TypeOfResolveNode(const Identifier &s) KJS_FAST_CALL 
        : ExpressionNode(StringType), m_ident(s) {}
    
    TypeOfResolveNode(PlacementNewAdoptType) KJS_FAST_CALL 
        : ExpressionNode(PlacementNewAdopt)
        , m_ident(PlacementNewAdopt) 
    {
        m_expectedReturnType = StringType;
    }

    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }

    const Identifier& identifier() const KJS_FAST_CALL { return m_ident; }

  protected:
    Identifier m_ident;
    size_t m_index; // Used by LocalTypeOfNode.
  };
    
  class LocalVarTypeOfNode : public TypeOfResolveNode {
  public:
    LocalVarTypeOfNode(size_t i) KJS_FAST_CALL 
        : TypeOfResolveNode(PlacementNewAdopt)
    {
        m_expectedReturnType = StringType;
        ASSERT(i != missingSymbolMarker());
        m_index = i;
    }

    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
  };

  class TypeOfValueNode : public ExpressionNode {
  public:
    TypeOfValueNode(ExpressionNode* e) KJS_FAST_CALL : ExpressionNode(StringType), m_expr(e) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    RefPtr<ExpressionNode> m_expr;
  };

  class PreIncResolveNode : public PrePostResolveNode {
  public:
    PreIncResolveNode(const Identifier &s) KJS_FAST_CALL 
        : PrePostResolveNode(s) 
    { 
    }
    
    PreIncResolveNode(PlacementNewAdoptType) KJS_FAST_CALL 
        : PrePostResolveNode(PlacementNewAdopt)
    {
    }
    
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;

    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  };

  class PreIncLocalVarNode : public PreIncResolveNode {
  public:
    PreIncLocalVarNode(size_t i) KJS_FAST_CALL 
        : PreIncResolveNode(PlacementNewAdopt)
    { 
        ASSERT(i != missingSymbolMarker());
        m_index = i;
    }
    
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
  };
  
  class PreDecResolveNode : public PrePostResolveNode {
  public:
    PreDecResolveNode(const Identifier &s) KJS_FAST_CALL 
        : PrePostResolveNode(s) 
    { 
    }
    
    PreDecResolveNode(PlacementNewAdoptType) KJS_FAST_CALL 
        : PrePostResolveNode(PlacementNewAdopt)
    {
    }
    
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;

    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  };

  class PreDecLocalVarNode : public PreDecResolveNode {
  public:
    PreDecLocalVarNode(size_t i) KJS_FAST_CALL 
        : PreDecResolveNode(PlacementNewAdopt)
    { 
        ASSERT(i != missingSymbolMarker());
        m_index = i;
    }
    
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
  };
  
  class PrefixBracketNode : public ExpressionNode {
  public:
    PrefixBracketNode(ExpressionNode* b, ExpressionNode* s) KJS_FAST_CALL : m_base(b), m_subscript(s) {}
    
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  protected:
    virtual bool isIncrement() const = 0;
    RefPtr<ExpressionNode> m_base;
    RefPtr<ExpressionNode> m_subscript;
  };
  
  class PreIncBracketNode : public PrefixBracketNode {
  public:
    PreIncBracketNode(ExpressionNode*  b, ExpressionNode*  s) KJS_FAST_CALL : PrefixBracketNode(b, s) {}
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
  protected:
    bool isIncrement() const { return true; }
  };
  
  class PreDecBracketNode : public PrefixBracketNode {
  public:
    PreDecBracketNode(ExpressionNode*  b, ExpressionNode*  s) KJS_FAST_CALL : PrefixBracketNode(b, s) {}
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
  protected:
    bool isIncrement() const { return false; }
  };

  class PrefixDotNode : public ExpressionNode {
  public:
    PrefixDotNode(ExpressionNode* b, const Identifier& i) KJS_FAST_CALL : m_base(b), m_ident(i) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecPostfix; }
  protected:
    virtual bool isIncrement() const = 0;
    RefPtr<ExpressionNode> m_base;
    Identifier m_ident;
  };

  class PreIncDotNode : public PrefixDotNode {
  public:
    PreIncDotNode(ExpressionNode* b, const Identifier& i) KJS_FAST_CALL : PrefixDotNode(b, i) {}
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
  protected:
    virtual bool isIncrement() const { return true; }
  };

  class PreDecDotNode : public PrefixDotNode {
  public:
    PreDecDotNode(ExpressionNode* b, const Identifier& i) KJS_FAST_CALL : PrefixDotNode(b, i) {}
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
  protected:
    virtual bool isIncrement() const { return false; }
  };

  class PrefixErrorNode : public ExpressionNode {
  public:
    PrefixErrorNode(ExpressionNode* e, Operator o) KJS_FAST_CALL : m_expr(e), m_oper(o) {}
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    RefPtr<ExpressionNode> m_expr;
    Operator m_oper;
  };

  class UnaryPlusNode : public ExpressionNode {
  public:
    UnaryPlusNode(ExpressionNode* e) KJS_FAST_CALL : ExpressionNode(NumberType), m_expr(e) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
    virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    RefPtr<ExpressionNode> m_expr;
  };

  class NegateNode : public ExpressionNode {
  public:
    NegateNode(ExpressionNode* e) KJS_FAST_CALL : ExpressionNode(NumberType), expr(e) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    RefPtr<ExpressionNode> expr;
  };

  class BitwiseNotNode : public ExpressionNode {
  public:
    BitwiseNotNode(ExpressionNode* e) KJS_FAST_CALL : ExpressionNode(NumberType), expr(e) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
    virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    ALWAYS_INLINE int32_t inlineEvaluateToInt32(ExecState*);
    RefPtr<ExpressionNode> expr;
  };

  class LogicalNotNode : public ExpressionNode {
  public:
    LogicalNotNode(ExpressionNode* e) KJS_FAST_CALL : ExpressionNode(BooleanType), expr(e) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecUnary; }
  private:
    RefPtr<ExpressionNode> expr;
  };
  
  class MultNode : public ExpressionNode {
  public:
      MultNode(ExpressionNode* t1, ExpressionNode* t2) KJS_FAST_CALL : ExpressionNode(NumberType), term1(t1), term2(t2) {}
      virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
      virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
      virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
      virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
      virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
      virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
      virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
      virtual Precedence precedence() const { return PrecMultiplicitave; }
  private:
      ALWAYS_INLINE double inlineEvaluateToNumber(ExecState*);
      RefPtr<ExpressionNode> term1;
      RefPtr<ExpressionNode> term2;
  };
  
  class DivNode : public ExpressionNode {
  public:
      DivNode(ExpressionNode* t1, ExpressionNode* t2) KJS_FAST_CALL : ExpressionNode(NumberType), term1(t1), term2(t2) {}
      virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
      virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
      virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
      virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
      virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
      virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
      virtual Precedence precedence() const { return PrecMultiplicitave; }
  private:
      ALWAYS_INLINE double inlineEvaluateToNumber(ExecState*);
      RefPtr<ExpressionNode> term1;
      RefPtr<ExpressionNode> term2;
  };
  
  class ModNode : public ExpressionNode {
  public:
      ModNode(ExpressionNode* t1, ExpressionNode* t2) KJS_FAST_CALL : ExpressionNode(NumberType), term1(t1), term2(t2) {}
      virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
      virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
      virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
      virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
      virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
      virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
      virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
      virtual Precedence precedence() const { return PrecMultiplicitave; }
  private:
      ALWAYS_INLINE double inlineEvaluateToNumber(ExecState*);
      RefPtr<ExpressionNode> term1;
      RefPtr<ExpressionNode> term2;
  };

  class AddNode : public ExpressionNode {
  public:
    AddNode(ExpressionNode* t1, ExpressionNode* t2) KJS_FAST_CALL : term1(t1), term2(t2) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
    virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecAdditive; }
  protected:
    AddNode(ExpressionNode* t1, ExpressionNode* t2, JSType expectedReturn) KJS_FAST_CALL : ExpressionNode(expectedReturn), term1(t1), term2(t2) {}
    RefPtr<ExpressionNode> term1;
    RefPtr<ExpressionNode> term2;
  private:
    ALWAYS_INLINE double inlineEvaluateToNumber(ExecState*);
  };

    class AddNumbersNode : public AddNode {
    public:
        AddNumbersNode(ExpressionNode* t1, ExpressionNode* t2) KJS_FAST_CALL : AddNode(t1, t2, NumberType) {}
        virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
        virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
        virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
        virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
    private:
        ALWAYS_INLINE double inlineEvaluateToNumber(ExecState*) KJS_FAST_CALL;
    };

    class AddStringLeftNode : public AddNode {
    public:
        AddStringLeftNode(ExpressionNode* t1, ExpressionNode* t2) KJS_FAST_CALL : AddNode(t1, t2, StringType) {}
        virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    };

    class AddStringRightNode : public AddNode {
    public:
        AddStringRightNode(ExpressionNode* t1, ExpressionNode* t2) KJS_FAST_CALL : AddNode(t1, t2, StringType) {}
        virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    };

    class AddStringsNode : public AddNode {
    public:
        AddStringsNode(ExpressionNode* t1, ExpressionNode* t2) KJS_FAST_CALL : AddNode(t1, t2, StringType) {}
        virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    };

  class SubNode : public ExpressionNode {
  public:
      SubNode(ExpressionNode* t1, ExpressionNode* t2) KJS_FAST_CALL : ExpressionNode(NumberType), term1(t1), term2(t2) {}
      virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
      virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
      virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
      virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
      virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
      virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
      virtual Precedence precedence() const { return PrecAdditive; }
  private:
      ALWAYS_INLINE double inlineEvaluateToNumber(ExecState*);
      RefPtr<ExpressionNode> term1;
      RefPtr<ExpressionNode> term2;
  };

  class LeftShiftNode : public ExpressionNode {
  public:
    LeftShiftNode(ExpressionNode* t1, ExpressionNode* t2) KJS_FAST_CALL
      : ExpressionNode(NumberType), term1(t1), term2(t2) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
    virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecShift; }
  private:
    ALWAYS_INLINE int32_t inlineEvaluateToInt32(ExecState*);
    RefPtr<ExpressionNode> term1;
    RefPtr<ExpressionNode> term2;
  };

  class RightShiftNode : public ExpressionNode {
  public:
    RightShiftNode(ExpressionNode* t1, ExpressionNode* t2) KJS_FAST_CALL
      : ExpressionNode(NumberType), term1(t1), term2(t2) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
    virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecShift; }
  private:
    ALWAYS_INLINE int32_t inlineEvaluateToInt32(ExecState*);
    RefPtr<ExpressionNode> term1;
    RefPtr<ExpressionNode> term2;
  };

  class UnsignedRightShiftNode : public ExpressionNode {
  public:
    UnsignedRightShiftNode(ExpressionNode* t1, ExpressionNode* t2) KJS_FAST_CALL
      : ExpressionNode(NumberType), term1(t1), term2(t2) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
    virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecShift; }
  private:
    ALWAYS_INLINE uint32_t inlineEvaluateToUInt32(ExecState*);
    RefPtr<ExpressionNode> term1;
    RefPtr<ExpressionNode> term2;
  };

  class LessNode : public ExpressionNode {
  public:
    LessNode(ExpressionNode* e1, ExpressionNode* e2) KJS_FAST_CALL
      : ExpressionNode(BooleanType), expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecRelational; }
  private:
    ALWAYS_INLINE bool inlineEvaluateToBoolean(ExecState*);
  protected:
    RefPtr<ExpressionNode> expr1;
    RefPtr<ExpressionNode> expr2;
  };

    class LessNumbersNode : public LessNode {
    public:
        LessNumbersNode(ExpressionNode* e1, ExpressionNode* e2) KJS_FAST_CALL
            : LessNode(e1, e2) {}
        virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
        virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    private:
        ALWAYS_INLINE bool inlineEvaluateToBoolean(ExecState*);
    };

    class LessStringsNode : public LessNode {
    public:
        LessStringsNode(ExpressionNode* e1, ExpressionNode* e2) KJS_FAST_CALL
            : LessNode(e1, e2) {}
        virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
        virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    private:
        ALWAYS_INLINE bool inlineEvaluateToBoolean(ExecState*);
    };

  class GreaterNode : public ExpressionNode {
  public:
    GreaterNode(ExpressionNode* e1, ExpressionNode* e2) KJS_FAST_CALL :
      expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecRelational; }
  private:
    ALWAYS_INLINE bool inlineEvaluateToBoolean(ExecState*);
    RefPtr<ExpressionNode> expr1;
    RefPtr<ExpressionNode> expr2;
  };

  class LessEqNode : public ExpressionNode {
  public:
    LessEqNode(ExpressionNode* e1, ExpressionNode* e2) KJS_FAST_CALL :
      expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecRelational; }
  private:
    ALWAYS_INLINE bool inlineEvaluateToBoolean(ExecState*);
    RefPtr<ExpressionNode> expr1;
    RefPtr<ExpressionNode> expr2;
  };

  class GreaterEqNode : public ExpressionNode {
  public:
    GreaterEqNode(ExpressionNode* e1, ExpressionNode* e2) KJS_FAST_CALL :
      expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecRelational; }
  private:
    ALWAYS_INLINE bool inlineEvaluateToBoolean(ExecState*);
    RefPtr<ExpressionNode> expr1;
    RefPtr<ExpressionNode> expr2;
  };

    class InstanceOfNode : public ExpressionNode {
  public:
    InstanceOfNode(ExpressionNode* e1, ExpressionNode* e2) KJS_FAST_CALL
        : ExpressionNode(BooleanType), expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecRelational; }
  private:
    RefPtr<ExpressionNode> expr1;
    RefPtr<ExpressionNode> expr2;
  };

    class InNode : public ExpressionNode {
  public:
    InNode(ExpressionNode* e1, ExpressionNode* e2) KJS_FAST_CALL :
      expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecRelational; }
  private:
    RefPtr<ExpressionNode> expr1;
    RefPtr<ExpressionNode> expr2;
  };

    class EqualNode : public ExpressionNode {
  public:
    EqualNode(ExpressionNode* e1, ExpressionNode* e2) KJS_FAST_CALL
        : ExpressionNode(BooleanType), expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecEquality; }
  private:
    ALWAYS_INLINE bool inlineEvaluateToBoolean(ExecState*);
    RefPtr<ExpressionNode> expr1;
    RefPtr<ExpressionNode> expr2;
  };

    class NotEqualNode : public ExpressionNode {
  public:
    NotEqualNode(ExpressionNode* e1, ExpressionNode* e2) KJS_FAST_CALL
        : ExpressionNode(BooleanType), expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecEquality; }
  private:
    ALWAYS_INLINE bool inlineEvaluateToBoolean(ExecState*);
    RefPtr<ExpressionNode> expr1;
    RefPtr<ExpressionNode> expr2;
  };

    class StrictEqualNode : public ExpressionNode {
  public:
    StrictEqualNode(ExpressionNode* e1, ExpressionNode* e2) KJS_FAST_CALL
        : ExpressionNode(BooleanType), expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecEquality; }
  private:
    ALWAYS_INLINE bool inlineEvaluateToBoolean(ExecState*);
    RefPtr<ExpressionNode> expr1;
    RefPtr<ExpressionNode> expr2;
  };

    class NotStrictEqualNode : public ExpressionNode {
  public:
    NotStrictEqualNode(ExpressionNode* e1, ExpressionNode* e2) KJS_FAST_CALL
        : ExpressionNode(BooleanType), expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecEquality; }
  private:
    ALWAYS_INLINE bool inlineEvaluateToBoolean(ExecState*);
    RefPtr<ExpressionNode> expr1;
    RefPtr<ExpressionNode> expr2;
  };

    class BitAndNode : public ExpressionNode {
  public:
    BitAndNode(ExpressionNode* e1, ExpressionNode* e2) KJS_FAST_CALL
        : ExpressionNode(NumberType), expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
    virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecBitwiseAnd; }
  private:
    ALWAYS_INLINE int32_t inlineEvaluateToInt32(ExecState*);
    RefPtr<ExpressionNode> expr1;
    RefPtr<ExpressionNode> expr2;
  };

    class BitOrNode : public ExpressionNode {
  public:
    BitOrNode(ExpressionNode* e1, ExpressionNode* e2) KJS_FAST_CALL
        : ExpressionNode(NumberType), expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
    virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecBitwiseOr; }
  private:
    ALWAYS_INLINE int32_t inlineEvaluateToInt32(ExecState*);
    RefPtr<ExpressionNode> expr1;
    RefPtr<ExpressionNode> expr2;
  };

    class BitXOrNode : public ExpressionNode {
  public:
    BitXOrNode(ExpressionNode* e1, ExpressionNode* e2) KJS_FAST_CALL
        : ExpressionNode(NumberType), expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
    virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecBitwiseXor; }
  private:
    ALWAYS_INLINE int32_t inlineEvaluateToInt32(ExecState*);
    RefPtr<ExpressionNode> expr1;
    RefPtr<ExpressionNode> expr2;
  };

  /**
   * expr1 && expr2, expr1 || expr2
   */
    class LogicalAndNode : public ExpressionNode {
  public:
    LogicalAndNode(ExpressionNode* e1, ExpressionNode* e2) KJS_FAST_CALL
        : ExpressionNode(BooleanType), expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecLogicalAnd; }
  private:
    ALWAYS_INLINE bool inlineEvaluateToBoolean(ExecState*);
    RefPtr<ExpressionNode> expr1;
    RefPtr<ExpressionNode> expr2;
  };
  
    class LogicalOrNode : public ExpressionNode {
  public:
    LogicalOrNode(ExpressionNode* e1, ExpressionNode* e2) KJS_FAST_CALL
        : ExpressionNode(BooleanType), expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecLogicalOr; }
  private:
    ALWAYS_INLINE bool inlineEvaluateToBoolean(ExecState*);
    RefPtr<ExpressionNode> expr1;
    RefPtr<ExpressionNode> expr2;
  };

  /**
   * The ternary operator, "logical ? expr1 : expr2"
   */
    class ConditionalNode : public ExpressionNode {
  public:
    ConditionalNode(ExpressionNode*  l, ExpressionNode* e1, ExpressionNode* e2) KJS_FAST_CALL :
      logical(l), expr1(e1), expr2(e2) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
    virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecConditional; }
  private:
    RefPtr<ExpressionNode> logical;
    RefPtr<ExpressionNode> expr1;
    RefPtr<ExpressionNode> expr2;
  };

    class ReadModifyResolveNode : public ExpressionNode {
  public:
    ReadModifyResolveNode(const Identifier &ident, Operator oper, ExpressionNode*  right) KJS_FAST_CALL
      : m_ident(ident)
      , m_oper(oper)
      , m_right(right) 
      {
      }

    ReadModifyResolveNode(PlacementNewAdoptType) KJS_FAST_CALL 
      : ExpressionNode(PlacementNewAdopt)
      , m_ident(PlacementNewAdopt) 
      , m_right(PlacementNewAdopt) 
    {
    }

    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecAssignment; }
  protected:
    Identifier m_ident;
    Operator m_oper;
    RefPtr<ExpressionNode> m_right;
    size_t m_index; // Used by ReadModifyLocalVarNode.
  };

  class ReadModifyLocalVarNode : public ReadModifyResolveNode {
  public:
    ReadModifyLocalVarNode(size_t i) KJS_FAST_CALL 
        : ReadModifyResolveNode(PlacementNewAdopt)
    { 
        ASSERT(i != missingSymbolMarker());
        m_index = i;
    }
    
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
  };

    class AssignResolveNode : public ExpressionNode {
  public:
     AssignResolveNode(const Identifier &ident, ExpressionNode*  right) KJS_FAST_CALL
      : m_ident(ident)
      , m_right(right) 
      {
      }

    AssignResolveNode(PlacementNewAdoptType) KJS_FAST_CALL 
      : ExpressionNode(PlacementNewAdopt)
      , m_ident(PlacementNewAdopt) 
      , m_right(PlacementNewAdopt) 
    {
    }

    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecAssignment; }
  protected:
    Identifier m_ident;
    RefPtr<ExpressionNode> m_right;
    size_t m_index; // Used by ReadModifyLocalVarNode.
  };

  class AssignLocalVarNode : public AssignResolveNode {
  public:
    AssignLocalVarNode(size_t i) KJS_FAST_CALL 
        : AssignResolveNode(PlacementNewAdopt)
    { 
        ASSERT(i != missingSymbolMarker());
        m_index = i;
    }
    
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
  };

    class ReadModifyBracketNode : public ExpressionNode {
  public:
    ReadModifyBracketNode(ExpressionNode*  base, ExpressionNode*  subscript, Operator oper, ExpressionNode*  right) KJS_FAST_CALL
      : m_base(base), m_subscript(subscript), m_oper(oper), m_right(right) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecAssignment; }
  protected:
    RefPtr<ExpressionNode> m_base;
    RefPtr<ExpressionNode> m_subscript;
    Operator m_oper;
    RefPtr<ExpressionNode> m_right;
  };

    class AssignBracketNode : public ExpressionNode {
  public:
    AssignBracketNode(ExpressionNode*  base, ExpressionNode*  subscript, ExpressionNode*  right) KJS_FAST_CALL
      : m_base(base), m_subscript(subscript), m_right(right) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecAssignment; }
  protected:
    RefPtr<ExpressionNode> m_base;
    RefPtr<ExpressionNode> m_subscript;
    RefPtr<ExpressionNode> m_right;
  };

    class AssignDotNode : public ExpressionNode {
  public:
    AssignDotNode(ExpressionNode*  base, const Identifier& ident, ExpressionNode*  right) KJS_FAST_CALL
      : m_base(base), m_ident(ident), m_right(right) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecAssignment; }
  protected:
    RefPtr<ExpressionNode> m_base;
    Identifier m_ident;
    RefPtr<ExpressionNode> m_right;
  };

    class ReadModifyDotNode : public ExpressionNode {
  public:
    ReadModifyDotNode(ExpressionNode*  base, const Identifier& ident, Operator oper, ExpressionNode*  right) KJS_FAST_CALL
      : m_base(base), m_ident(ident), m_oper(oper), m_right(right) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecAssignment; }
  protected:
    RefPtr<ExpressionNode> m_base;
    Identifier m_ident;
    Operator m_oper;
    RefPtr<ExpressionNode> m_right;
  };

    class AssignErrorNode : public ExpressionNode {
  public:
    AssignErrorNode(ExpressionNode* left, Operator oper, ExpressionNode* right) KJS_FAST_CALL
      : m_left(left), m_oper(oper), m_right(right) {}
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecAssignment; }
  protected:
    RefPtr<ExpressionNode> m_left;
    Operator m_oper;
    RefPtr<ExpressionNode> m_right;
  };

    class CommaNode : public ExpressionNode {
  public:
    CommaNode(ExpressionNode* e1, ExpressionNode* e2) KJS_FAST_CALL : expr1(e1), expr2(e2)
    {
        e1->optimizeForUnnecessaryResult();
    }
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { return PrecExpression; }
  private:
    RefPtr<ExpressionNode> expr1;
    RefPtr<ExpressionNode> expr2;
  };

    class AssignExprNode : public ExpressionNode {
  public:
    AssignExprNode(ExpressionNode* e) KJS_FAST_CALL : expr(e) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    virtual bool evaluateToBoolean(ExecState*) KJS_FAST_CALL;
    virtual double evaluateToNumber(ExecState*) KJS_FAST_CALL;
    virtual int32_t evaluateToInt32(ExecState*) KJS_FAST_CALL;
    virtual uint32_t evaluateToUInt32(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
  private:
    RefPtr<ExpressionNode> expr;
  };

    class VarDeclNode : public ExpressionNode {
  public:
    enum Type { Variable, Constant };
    VarDeclNode(const Identifier &id, AssignExprNode *in, Type t) KJS_FAST_CALL;
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual KJS::JSValue* evaluate(ExecState*) KJS_FAST_CALL;
    void evaluateSingle(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
    PassRefPtr<VarDeclNode> releaseNext() KJS_FAST_CALL { return next.release(); }

    Type varType;
    Identifier ident;
    ListRefPtr<VarDeclNode> next;
  private:
    void handleSlowCase(ExecState*, const ScopeChain&, JSValue*) KJS_FAST_CALL KJS_NO_INLINE;
    RefPtr<AssignExprNode> init;
  };

  class VarStatementNode : public StatementNode {
  public:
    VarStatementNode(VarDeclNode* l) KJS_FAST_CALL : next(l) { }
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
  private:
    RefPtr<VarDeclNode> next;
  };

  typedef Vector<RefPtr<StatementNode> > SourceElements;

  class SourceElementsStub : public Node {
  public:
    SourceElementsStub()
      : m_sourceElements(new SourceElements)
      {}
    void append(StatementNode* element) { m_sourceElements->append(element); }
    SourceElements* release() { 
        SourceElements* elems = m_sourceElements.release(); 
        return elems;
    }
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL { ASSERT_NOT_REACHED(); }
    virtual Completion execute(ExecState*) KJS_FAST_CALL  { ASSERT_NOT_REACHED(); return Completion(); }
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL  { ASSERT_NOT_REACHED(); }
    virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
  private:
    OwnPtr<SourceElements> m_sourceElements;
  };
    
  class BlockNode : public StatementNode {
  public:
    BlockNode(SourceElements* children) KJS_FAST_CALL;
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
  protected:
    OwnPtr<SourceElements> m_children;
  };

  class EmptyStatementNode : public StatementNode {
  public:
    EmptyStatementNode() KJS_FAST_CALL { } // debug
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
  };

  class ExprStatementNode : public StatementNode {
  public:
    ExprStatementNode(ExpressionNode* e) KJS_FAST_CALL : expr(e) { }
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
  private:
    RefPtr<ExpressionNode> expr;
  };

  class IfNode : public StatementNode {
  public:
    IfNode(ExpressionNode* e, StatementNode *s1, StatementNode *s2) KJS_FAST_CALL
      : expr(e), statement1(s1), statement2(s2) { }
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
  private:
    RefPtr<ExpressionNode> expr;
    RefPtr<StatementNode> statement1;
    RefPtr<StatementNode> statement2;
  };

  class DoWhileNode : public StatementNode {
  public:
    DoWhileNode(StatementNode *s, ExpressionNode* e) KJS_FAST_CALL : statement(s), expr(e) { }
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
  private:
    RefPtr<StatementNode> statement;
    RefPtr<ExpressionNode> expr;
  };

  class WhileNode : public StatementNode {
  public:
    WhileNode(ExpressionNode* e, StatementNode *s) KJS_FAST_CALL : expr(e), statement(s) { }
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
  private:
    RefPtr<ExpressionNode> expr;
    RefPtr<StatementNode> statement;
  };

  class ForNode : public StatementNode {
  public:
    ForNode(ExpressionNode* e1, ExpressionNode* e2, ExpressionNode* e3, StatementNode* s) KJS_FAST_CALL :
      expr1(e1), expr2(e2), expr3(e3), statement(s)
    {
        if (expr1)
            expr1->optimizeForUnnecessaryResult();
        if (expr3)
            expr3->optimizeForUnnecessaryResult();
    }

    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
  private:
    RefPtr<ExpressionNode> expr1;
    RefPtr<ExpressionNode> expr2;
    RefPtr<ExpressionNode> expr3;
    RefPtr<StatementNode> statement;
  };

  class ForInNode : public StatementNode {
  public:
    ForInNode(ExpressionNode*  l, ExpressionNode* e, StatementNode *s) KJS_FAST_CALL;
    ForInNode(const Identifier &i, AssignExprNode *in, ExpressionNode* e, StatementNode *s) KJS_FAST_CALL;
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    VarDeclNode* getVarDecl() { return varDecl.get(); }
  private:
    Identifier ident;
    RefPtr<AssignExprNode> init;
    RefPtr<ExpressionNode> lexpr;
    RefPtr<ExpressionNode> expr;
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
    ReturnNode(ExpressionNode* v) KJS_FAST_CALL : value(v) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
  private:
    RefPtr<ExpressionNode> value;
  };

  class WithNode : public StatementNode {
  public:
    WithNode(ExpressionNode* e, StatementNode* s) KJS_FAST_CALL : expr(e), statement(s) { }
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
  private:
    RefPtr<ExpressionNode> expr;
    RefPtr<StatementNode> statement;
  };

  class LabelNode : public StatementNode {
  public:
    LabelNode(const Identifier &l, StatementNode *s) KJS_FAST_CALL : label(l), statement(s) { }
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
  private:
    Identifier label;
    RefPtr<StatementNode> statement;
  };

  class ThrowNode : public StatementNode {
  public:
    ThrowNode(ExpressionNode* e) KJS_FAST_CALL : expr(e) {}
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
  private:
    RefPtr<ExpressionNode> expr;
  };

  class TryNode : public StatementNode {
  public:
    TryNode(StatementNode *b, const Identifier &e, StatementNode *c, StatementNode *f) KJS_FAST_CALL
      : tryBlock(b), exceptionIdent(e), catchBlock(c), finallyBlock(f) { }
    virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
  private:
    RefPtr<StatementNode> tryBlock;
    Identifier exceptionIdent;
    RefPtr<StatementNode> catchBlock;
    RefPtr<StatementNode> finallyBlock;
  };

    class ParameterNode : public Node {
  public:
    ParameterNode(const Identifier& i) KJS_FAST_CALL : id(i) { }
    ParameterNode(ParameterNode* l, const Identifier& i) KJS_FAST_CALL
      : id(i) { l->next = this; }
    Identifier ident() KJS_FAST_CALL { return id; }
    ParameterNode *nextParam() KJS_FAST_CALL { return next.get(); }
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    PassRefPtr<ParameterNode> releaseNext() KJS_FAST_CALL { return next.release(); }
    virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
  private:
    friend class FuncDeclNode;
    friend class FuncExprNode;
    Identifier id;
    ListRefPtr<ParameterNode> next;
  };

  class ScopeNode : public BlockNode {
  public:
    ScopeNode(SourceElements*, DeclarationStacks::VarStack*, DeclarationStacks::FunctionStack*) KJS_FAST_CALL;

    int sourceId() KJS_FAST_CALL { return m_sourceId; }
    const UString& sourceURL() KJS_FAST_CALL { return m_sourceURL; }

  protected:

    DeclarationStacks::VarStack m_varStack;
    DeclarationStacks::FunctionStack m_functionStack;

  private:
    UString m_sourceURL;
    int m_sourceId;
  };

  class ProgramNode : public ScopeNode {
  public:
    ProgramNode(SourceElements*, DeclarationStacks::VarStack*, DeclarationStacks::FunctionStack*) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    
  private:
    ALWAYS_INLINE void processDeclarations(ExecState*) KJS_FAST_CALL;
  };

  class EvalNode : public ScopeNode {
  public:
    EvalNode(SourceElements*, DeclarationStacks::VarStack*, DeclarationStacks::FunctionStack*) KJS_FAST_CALL;
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    
  private:
    ALWAYS_INLINE void processDeclarations(ExecState*) KJS_FAST_CALL;
  };

  class FunctionBodyNode : public ScopeNode {
  public:
    FunctionBodyNode(SourceElements*, DeclarationStacks::VarStack*, DeclarationStacks::FunctionStack*) KJS_FAST_CALL;

    virtual Completion execute(ExecState*) KJS_FAST_CALL;

    SymbolTable& symbolTable() KJS_FAST_CALL { return m_symbolTable; }

    Vector<Identifier>& parameters() KJS_FAST_CALL { return m_parameters; }
    UString paramString() const KJS_FAST_CALL;

  private:
    void initializeSymbolTable(ExecState*) KJS_FAST_CALL;
    void optimizeVariableAccess() KJS_FAST_CALL;
    ALWAYS_INLINE void processDeclarations(ExecState*) KJS_FAST_CALL;

    bool m_initialized;
    Vector<Identifier> m_parameters;
    SymbolTable m_symbolTable;
  };

  class FuncExprNode : public ExpressionNode {
  public:
    FuncExprNode(const Identifier& i, FunctionBodyNode* b, ParameterNode* p = 0) KJS_FAST_CALL 
      : ident(i), param(p), body(b) { addParams(); }
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
    FuncDeclNode(const Identifier& i, FunctionBodyNode* b) KJS_FAST_CALL
      : ident(i), body(b) { addParams(); }
    FuncDeclNode(const Identifier& i, ParameterNode* p, FunctionBodyNode* b) KJS_FAST_CALL
      : ident(i), param(p), body(b) { addParams(); }
    virtual Completion execute(ExecState*) KJS_FAST_CALL;
    virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    ALWAYS_INLINE FunctionImp* makeFunction(ExecState*) KJS_FAST_CALL;
    Identifier ident;
  private:
    void addParams() KJS_FAST_CALL;
    RefPtr<ParameterNode> param;
    RefPtr<FunctionBodyNode> body;
  };

    class CaseClauseNode : public Node {
  public:
      CaseClauseNode(ExpressionNode* e) KJS_FAST_CALL : expr(e) { }
      CaseClauseNode(ExpressionNode* e, SourceElements* children) KJS_FAST_CALL
      : expr(e), m_children(children) { }
      virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
      virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
      virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

      JSValue* evaluate(ExecState*) KJS_FAST_CALL;
      Completion evalStatements(ExecState*) KJS_FAST_CALL;

  private:
      RefPtr<ExpressionNode> expr;
      OwnPtr<SourceElements> m_children;
  };
  
    class ClauseListNode : public Node {
  public:
      ClauseListNode(CaseClauseNode* c) KJS_FAST_CALL : clause(c) { }
      ClauseListNode(ClauseListNode* n, CaseClauseNode* c) KJS_FAST_CALL
      : clause(c) { n->next = this; }
      virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
      CaseClauseNode* getClause() const KJS_FAST_CALL { return clause.get(); }
      ClauseListNode* getNext() const KJS_FAST_CALL { return next.get(); }
      virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
      PassRefPtr<ClauseListNode> releaseNext() KJS_FAST_CALL { return next.release(); }
      virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
  private:
      friend class CaseBlockNode;
      RefPtr<CaseClauseNode> clause;
      ListRefPtr<ClauseListNode> next;
  };
  
    class CaseBlockNode : public Node {
  public:
      CaseBlockNode(ClauseListNode* l1, CaseClauseNode* d, ClauseListNode* l2) KJS_FAST_CALL;
      virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
      Completion evalBlock(ExecState *exec, JSValue *input) KJS_FAST_CALL;
      virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
      virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
  private:
      RefPtr<ClauseListNode> list1;
      RefPtr<CaseClauseNode> def;
      RefPtr<ClauseListNode> list2;
  };
  
  class SwitchNode : public StatementNode {
  public:
      SwitchNode(ExpressionNode* e, CaseBlockNode *b) KJS_FAST_CALL : expr(e), block(b) { }
      virtual void optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack&) KJS_FAST_CALL;
      virtual Completion execute(ExecState*) KJS_FAST_CALL;
      virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
  private:
      RefPtr<ExpressionNode> expr;
      RefPtr<CaseBlockNode> block;
  };
  
  struct ElementList {
      ElementNode* head;
      ElementNode* tail;
  };

  struct PropertyList {
      PropertyListNode* head;
      PropertyListNode* tail;
  };

  struct ArgumentList {
      ArgumentListNode* head;
      ArgumentListNode* tail;
  };

  struct VarDeclList {
      VarDeclNode* head;
      VarDeclNode* tail;
  };

  struct ParameterList {
      ParameterNode* head;
      ParameterNode* tail;
  };

  struct ClauseList {
      ClauseListNode* head;
      ClauseListNode* tail;
  };

} // namespace

#endif
