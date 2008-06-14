/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "RegisterID.h"
#include "SourceRange.h"
#include "SymbolTable.h"
#include <wtf/UnusedParam.h>
#include <wtf/ListRefPtr.h>
#include <wtf/MathExtras.h>
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>

#if PLATFORM(X86) && COMPILER(GCC)
#define KJS_FAST_CALL __attribute__((regparm(3)))
#else
#define KJS_FAST_CALL
#endif

namespace KJS {

    class ArgumentsNode;
    class CodeBlock;
    class CodeGenerator;
    class ConstDeclNode;
    class FuncDeclNode;
    class Node;
    class EvalCodeBlock;
    class ProgramCodeBlock;
    class PropertyListNode;
    class SourceStream;

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
        enum { IsConstant = 1, HasInitializer = 2 } VarAttrs;
        typedef Vector<std::pair<Identifier, unsigned>, 16> VarStack;
        typedef Vector<RefPtr<FuncDeclNode>, 16> FunctionStack;

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
        ParserRefCounted(PlacementNewAdoptType) KJS_FAST_CALL
        {
        }

    public:
        void ref() KJS_FAST_CALL;
        void deref() KJS_FAST_CALL;
        unsigned refcount() KJS_FAST_CALL;

        static void deleteNewObjects() KJS_FAST_CALL;

        virtual ~ParserRefCounted();
    };

    class Node : public ParserRefCounted {
    public:
        typedef DeclarationStacks::NodeStack NodeStack;
        typedef DeclarationStacks::VarStack VarStack;
        typedef DeclarationStacks::FunctionStack FunctionStack;

        Node() KJS_FAST_CALL;
        Node(PlacementNewAdoptType placementAdopt) KJS_FAST_CALL
            : ParserRefCounted(placementAdopt)
        {
        }

        /*
            Return value: The register holding the production's value.
                     dst: An optional parameter specifying the most efficient
                          destination at which to store the production's value.
                          The callee must honor dst.

            dst provides for a crude form of copy propagation. For example,

            x = 1

            becomes
            
            load r[x], 1
            
            instead of 

            load r0, 1
            mov r[x], r0
            
            because the assignment node, "x =", passes r[x] as dst to the number
            node, "1".
        */
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* dst = 0) KJS_FAST_CALL 
        {
            ASSERT_WITH_MESSAGE(0, "Don't know how to generate code for:\n%s\n", toString().ascii());
            UNUSED_PARAM(dst); 
            return 0; 
        } // FIXME: Make this pure virtual.

        UString toString() const KJS_FAST_CALL;
        int lineNo() const KJS_FAST_CALL { return m_line; }

        virtual bool isReturnNode() const KJS_FAST_CALL { return false; }

        // Serialization.
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL = 0;
        virtual Precedence precedence() const = 0;
        virtual bool needsParensIfLeftmost() const { return false; }
        
    protected:
        Node(JSType) KJS_FAST_CALL; // used by ExpressionNode

        RegisterID* emitThrowError(CodeGenerator&, ErrorType, const char* msg);
        RegisterID* emitThrowError(CodeGenerator&, ErrorType, const char* msg, const Identifier&);

        int m_line : 28;
        unsigned m_expectedReturnType : 3; // JSType
    };

    class ExpressionNode : public Node {
    public:
        ExpressionNode() KJS_FAST_CALL : Node() {}
        ExpressionNode(JSType expectedReturn) KJS_FAST_CALL
            : Node(expectedReturn)
        {
        }

        // Special constructor for cases where we overwrite an object in place.
        ExpressionNode(PlacementNewAdoptType) KJS_FAST_CALL
            : Node(PlacementNewAdopt)
        {
        }

        virtual bool isNumber() const KJS_FAST_CALL { return false; }
        virtual bool isConstant() const KJS_FAST_CALL { return false; }        
        virtual bool isLocation() const KJS_FAST_CALL { return false; }
        virtual bool isResolveNode() const KJS_FAST_CALL { return false; }
        virtual bool isBracketAccessorNode() const KJS_FAST_CALL { return false; }
        virtual bool isDotAccessorNode() const KJS_FAST_CALL { return false; }

        JSType expectedReturnType() const KJS_FAST_CALL { return static_cast<JSType>(m_expectedReturnType); }

        // This needs to be in public in order to compile using GCC 3.x 
        typedef enum { EvalOperator, FunctionCall } CallerType;
    };

    class StatementNode : public Node {
    public:
        StatementNode() KJS_FAST_CALL;
        void setLoc(int line0, int line1) KJS_FAST_CALL;
        int firstLine() const KJS_FAST_CALL { return lineNo(); }
        int lastLine() const KJS_FAST_CALL { return m_lastLine; }

        virtual void pushLabel(const Identifier& ident) KJS_FAST_CALL { m_labelStack.push(ident); }
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
        virtual bool isEmptyStatement() const KJS_FAST_CALL { return false; }

    protected:
        LabelStack m_labelStack;

    private:
        int m_lastLine;
    };

    class NullNode : public ExpressionNode {
    public:
        NullNode() KJS_FAST_CALL
            : ExpressionNode(NullType)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }
    };

    class BooleanNode : public ExpressionNode {
    public:
        BooleanNode(bool value) KJS_FAST_CALL
            : ExpressionNode(BooleanType)
            , m_value(value)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual bool isConstant() const KJS_FAST_CALL { return true; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }

    protected:
        bool m_value;
    };

    class NumberNode : public ExpressionNode {
    public:
        NumberNode(double v) KJS_FAST_CALL
            : ExpressionNode(NumberType)
            , m_double(v)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return signbit(m_double) ? PrecUnary : PrecPrimary; }

        virtual bool isNumber() const KJS_FAST_CALL { return true; }
        virtual bool isConstant() const KJS_FAST_CALL { return true; }
        double value() const KJS_FAST_CALL { return m_double; }
        virtual void setValue(double d) KJS_FAST_CALL { m_double = d; }

    protected:
        double m_double;
    };

    class ImmediateNumberNode : public NumberNode {
    public:
        ImmediateNumberNode(JSValue* v, double d) KJS_FAST_CALL
            : NumberNode(d)
            , m_value(v)
        {
            ASSERT(v == JSImmediate::from(d));
        }

        virtual void setValue(double d) KJS_FAST_CALL { m_double = d; m_value = JSImmediate::from(d); ASSERT(m_value); }

    private:
        JSValue* m_value; // This is never a JSCell, only JSImmediate, thus no ProtectedPtr
    };

    class StringNode : public ExpressionNode {
    public:
        StringNode(const UString* v) KJS_FAST_CALL
            : ExpressionNode(StringType)
            , m_value(*v)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual bool isConstant() const KJS_FAST_CALL { return true; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }

    private:
        UString m_value;
    };

    class RegExpNode : public ExpressionNode {
    public:
        RegExpNode(const UString& pattern, const UString& flags) KJS_FAST_CALL
            : m_regExp(RegExp::create(pattern, flags))
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }

    private:
        RefPtr<RegExp> m_regExp;
    };

    class ThisNode : public ExpressionNode {
    public:
        ThisNode() KJS_FAST_CALL
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }
    };

    class ResolveNode : public ExpressionNode {
    public:
        ResolveNode(const Identifier& ident) KJS_FAST_CALL
            : m_ident(ident)
        {
        }

        // Special constructor for cases where we overwrite an object in place.
        ResolveNode(PlacementNewAdoptType) KJS_FAST_CALL
            : ExpressionNode(PlacementNewAdopt)
            , m_ident(PlacementNewAdopt)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }

        virtual bool isLocation() const KJS_FAST_CALL { return true; }
        virtual bool isResolveNode() const KJS_FAST_CALL { return true; }
        const Identifier& identifier() const KJS_FAST_CALL { return m_ident; }

    protected:
        Identifier m_ident;
        int m_index; // Used by LocalVarAccessNode and ScopedVarAccessNode.
        size_t m_scopeDepth; // Used by ScopedVarAccessNode
    };

    class ElementNode : public Node {
    public:
        ElementNode(int elision, ExpressionNode* node) KJS_FAST_CALL
            : m_elision(elision)
            , m_node(node)
        {
        }

        ElementNode(ElementNode* l, int elision, ExpressionNode* node) KJS_FAST_CALL
            : m_elision(elision)
            , m_node(node)
        {
            l->m_next = this;
        }

        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

        PassRefPtr<ElementNode> releaseNext() KJS_FAST_CALL { return m_next.release(); }

    private:
        friend class ArrayNode;
        ListRefPtr<ElementNode> m_next;
        int m_elision;
        RefPtr<ExpressionNode> m_node;
    };

    class ArrayNode : public ExpressionNode {
    public:
        ArrayNode(int elision) KJS_FAST_CALL
            : m_elision(elision)
            , m_optional(true)
        {
        }

        ArrayNode(ElementNode* element) KJS_FAST_CALL
            : m_element(element)
            , m_elision(0)
            , m_optional(false)
        {
        }

        ArrayNode(int elision, ElementNode* element) KJS_FAST_CALL
            : m_element(element)
            , m_elision(elision)
            , m_optional(true)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }

    private:
        RefPtr<ElementNode> m_element;
        int m_elision;
        bool m_optional;
    };

    class PropertyNode : public Node {
    public:
        enum Type { Constant, Getter, Setter };

        PropertyNode(const Identifier& name, ExpressionNode* assign, Type type) KJS_FAST_CALL
            : m_name(name)
            , m_assign(assign)
            , m_type(type)
        {
        }

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

        const Identifier& name() const { return m_name; }

    private:
        friend class PropertyListNode;
        Identifier m_name;
        RefPtr<ExpressionNode> m_assign;
        Type m_type;
    };

    class PropertyListNode : public Node {
    public:
        PropertyListNode(PropertyNode* node) KJS_FAST_CALL
            : m_node(node)
        {
        }

        PropertyListNode(PropertyNode* node, PropertyListNode* list) KJS_FAST_CALL
            : m_node(node)
        {
            list->m_next = this;
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

        PassRefPtr<PropertyListNode> releaseNext() KJS_FAST_CALL { return m_next.release(); }

    private:
        friend class ObjectLiteralNode;
        RefPtr<PropertyNode> m_node;
        ListRefPtr<PropertyListNode> m_next;
    };

    class ObjectLiteralNode : public ExpressionNode {
    public:
        ObjectLiteralNode() KJS_FAST_CALL
        {
        }

        ObjectLiteralNode(PropertyListNode* list) KJS_FAST_CALL
            : m_list(list)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }
        virtual bool needsParensIfLeftmost() const { return true; }

    private:
        RefPtr<PropertyListNode> m_list;
    };

    class BracketAccessorNode : public ExpressionNode {
    public:
        BracketAccessorNode(ExpressionNode* base, ExpressionNode* subscript, bool subscriptHasAssignments) KJS_FAST_CALL
            : m_base(base)
            , m_subscript(subscript)
            , m_subscriptHasAssignments(subscriptHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecMember; }

        virtual bool isLocation() const KJS_FAST_CALL { return true; }
        virtual bool isBracketAccessorNode() const KJS_FAST_CALL { return true; }
        ExpressionNode* base() KJS_FAST_CALL { return m_base.get(); }
        ExpressionNode* subscript() KJS_FAST_CALL { return m_subscript.get(); }

    private:
        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
        bool m_subscriptHasAssignments;
    };

    class DotAccessorNode : public ExpressionNode {
    public:
        DotAccessorNode(ExpressionNode* base, const Identifier& ident) KJS_FAST_CALL
            : m_base(base)
            , m_ident(ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecMember; }

        virtual bool isLocation() const KJS_FAST_CALL { return true; }
        virtual bool isDotAccessorNode() const KJS_FAST_CALL { return true; }
        ExpressionNode* base() const KJS_FAST_CALL { return m_base.get(); }
        const Identifier& identifier() const KJS_FAST_CALL { return m_ident; }

    private:
        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
    };

    class ArgumentListNode : public Node {
    public:
        ArgumentListNode(ExpressionNode* expr) KJS_FAST_CALL
            : m_expr(expr)
        {
        }

        ArgumentListNode(ArgumentListNode* listNode, ExpressionNode* expr) KJS_FAST_CALL
            : m_expr(expr)
        {
            listNode->m_next = this;
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

        PassRefPtr<ArgumentListNode> releaseNext() KJS_FAST_CALL { return m_next.release(); }

        ListRefPtr<ArgumentListNode> m_next;
        RefPtr<ExpressionNode> m_expr;
    };

    class ArgumentsNode : public Node {
    public:
        ArgumentsNode() KJS_FAST_CALL
        {
        }

        ArgumentsNode(ArgumentListNode* listNode) KJS_FAST_CALL
            : m_listNode(listNode)
        {
        }

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

        RefPtr<ArgumentListNode> m_listNode;
    };

    class NewExprNode : public ExpressionNode {
    public:
        NewExprNode(ExpressionNode* expr) KJS_FAST_CALL
            : m_expr(expr)
        {
        }

        NewExprNode(ExpressionNode* expr, ArgumentsNode* args) KJS_FAST_CALL
            : m_expr(expr)
            , m_args(args)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecLeftHandSide; }

    private:
        RefPtr<ExpressionNode> m_expr;
        RefPtr<ArgumentsNode> m_args;
    };

    class EvalFunctionCallNode : public ExpressionNode {
    public:
        EvalFunctionCallNode(ArgumentsNode* args) KJS_FAST_CALL
            : m_args(args)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecCall; }

    private:
        RefPtr<ArgumentsNode> m_args;
    };

    class FunctionCallValueNode : public ExpressionNode {
    public:
        FunctionCallValueNode(ExpressionNode* expr, ArgumentsNode* args) KJS_FAST_CALL
            : m_expr(expr)
            , m_args(args)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecCall; }

    private:
        RefPtr<ExpressionNode> m_expr;
        RefPtr<ArgumentsNode> m_args;
    };

    class FunctionCallResolveNode : public ExpressionNode {
    public:
        FunctionCallResolveNode(const Identifier& ident, ArgumentsNode* args) KJS_FAST_CALL
            : m_ident(ident)
            , m_args(args)
        {
        }

        FunctionCallResolveNode(PlacementNewAdoptType) KJS_FAST_CALL
            : ExpressionNode(PlacementNewAdopt)
            , m_ident(PlacementNewAdopt)
            , m_args(PlacementNewAdopt)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecCall; }

    protected:
        Identifier m_ident;
        RefPtr<ArgumentsNode> m_args;
        size_t m_index; // Used by LocalVarFunctionCallNode.
        size_t m_scopeDepth; // Used by ScopedVarFunctionCallNode and NonLocalVarFunctionCallNode
    };
    
    class FunctionCallBracketNode : public ExpressionNode {
    public:
        FunctionCallBracketNode(ExpressionNode* base, ExpressionNode* subscript, ArgumentsNode* args) KJS_FAST_CALL
            : m_base(base)
            , m_subscript(subscript)
            , m_args(args)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecCall; }

    protected:
        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
        RefPtr<ArgumentsNode> m_args;
    };

    class FunctionCallDotNode : public ExpressionNode {
    public:
        FunctionCallDotNode(ExpressionNode* base, const Identifier& ident, ArgumentsNode* args) KJS_FAST_CALL
            : m_base(base)
            , m_ident(ident)
            , m_args(args)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecCall; }

    private:
        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
        RefPtr<ArgumentsNode> m_args;
    };

    class PrePostResolveNode : public ExpressionNode {
    public:
        PrePostResolveNode(const Identifier& ident) KJS_FAST_CALL
            : ExpressionNode(NumberType)
            , m_ident(ident)
        {
        }

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
        PostIncResolveNode(const Identifier& ident) KJS_FAST_CALL
            : PrePostResolveNode(ident)
        {
        }

        PostIncResolveNode(PlacementNewAdoptType) KJS_FAST_CALL
            : PrePostResolveNode(PlacementNewAdopt)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecPostfix; }
    };

    class PostDecResolveNode : public PrePostResolveNode {
    public:
        PostDecResolveNode(const Identifier& ident) KJS_FAST_CALL
            : PrePostResolveNode(ident)
        {
        }

        PostDecResolveNode(PlacementNewAdoptType) KJS_FAST_CALL
            : PrePostResolveNode(PlacementNewAdopt)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecPostfix; }
    };

    class PostfixBracketNode : public ExpressionNode {
    public:
        PostfixBracketNode(ExpressionNode* base, ExpressionNode* subscript) KJS_FAST_CALL
            : m_base(base)
            , m_subscript(subscript)
        {
        }

        virtual Precedence precedence() const { return PrecPostfix; }

    protected:
        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
    };

    class PostIncBracketNode : public PostfixBracketNode {
    public:
        PostIncBracketNode(ExpressionNode* base, ExpressionNode* subscript) KJS_FAST_CALL
            : PostfixBracketNode(base, subscript)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    };

    class PostDecBracketNode : public PostfixBracketNode {
    public:
        PostDecBracketNode(ExpressionNode* base, ExpressionNode* subscript) KJS_FAST_CALL
            : PostfixBracketNode(base, subscript)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    };

    class PostfixDotNode : public ExpressionNode {
    public:
        PostfixDotNode(ExpressionNode* base, const Identifier& ident) KJS_FAST_CALL
            : m_base(base)
            , m_ident(ident)
        {
        }

        virtual Precedence precedence() const { return PrecPostfix; }

    protected:
        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
    };

    class PostIncDotNode : public PostfixDotNode {
    public:
        PostIncDotNode(ExpressionNode* base, const Identifier& ident) KJS_FAST_CALL
            : PostfixDotNode(base, ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    };

    class PostDecDotNode : public PostfixDotNode {
    public:
        PostDecDotNode(ExpressionNode*  base, const Identifier& ident) KJS_FAST_CALL
            : PostfixDotNode(base, ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    };

    class PostfixErrorNode : public ExpressionNode {
    public:
        PostfixErrorNode(ExpressionNode* expr, Operator oper) KJS_FAST_CALL
            : m_expr(expr)
            , m_operator(oper)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecPostfix; }

    private:
        RefPtr<ExpressionNode> m_expr;
        Operator m_operator;
    };

    class DeleteResolveNode : public ExpressionNode {
    public:
        DeleteResolveNode(const Identifier& ident) KJS_FAST_CALL
            : m_ident(ident)
        {
        }

        DeleteResolveNode(PlacementNewAdoptType) KJS_FAST_CALL
            : ExpressionNode(PlacementNewAdopt)
            , m_ident(PlacementNewAdopt)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        Identifier m_ident;
    };

    class DeleteBracketNode : public ExpressionNode {
    public:
        DeleteBracketNode(ExpressionNode* base, ExpressionNode* subscript) KJS_FAST_CALL
            : m_base(base)
            , m_subscript(subscript)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
    };

    class DeleteDotNode : public ExpressionNode {
    public:
        DeleteDotNode(ExpressionNode* base, const Identifier& ident) KJS_FAST_CALL
            : m_base(base)
            , m_ident(ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
    };

    class DeleteValueNode : public ExpressionNode {
    public:
        DeleteValueNode(ExpressionNode* expr) KJS_FAST_CALL
            : m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        RefPtr<ExpressionNode> m_expr;
    };

    class VoidNode : public ExpressionNode {
    public:
        VoidNode(ExpressionNode* expr) KJS_FAST_CALL
            : m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        RefPtr<ExpressionNode> m_expr;
    };

    class TypeOfResolveNode : public ExpressionNode {
    public:
        TypeOfResolveNode(const Identifier& ident) KJS_FAST_CALL
            : ExpressionNode(StringType)
            , m_ident(ident)
        {
        }

        TypeOfResolveNode(PlacementNewAdoptType) KJS_FAST_CALL
            : ExpressionNode(PlacementNewAdopt)
            , m_ident(PlacementNewAdopt)
        {
            m_expectedReturnType = StringType;
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

        const Identifier& identifier() const KJS_FAST_CALL { return m_ident; }

    protected:
        Identifier m_ident;
        size_t m_index; // Used by LocalTypeOfNode.
    };

    class TypeOfValueNode : public ExpressionNode {
    public:
        TypeOfValueNode(ExpressionNode* expr) KJS_FAST_CALL
            : ExpressionNode(StringType)
            , m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        RefPtr<ExpressionNode> m_expr;
    };

    class PreIncResolveNode : public PrePostResolveNode {
    public:
        PreIncResolveNode(const Identifier& ident) KJS_FAST_CALL
            : PrePostResolveNode(ident)
        {
        }

        PreIncResolveNode(PlacementNewAdoptType) KJS_FAST_CALL
            : PrePostResolveNode(PlacementNewAdopt)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }
    };

    class PreDecResolveNode : public PrePostResolveNode {
    public:
        PreDecResolveNode(const Identifier& ident) KJS_FAST_CALL
            : PrePostResolveNode(ident)
        {
        }

        PreDecResolveNode(PlacementNewAdoptType) KJS_FAST_CALL
            : PrePostResolveNode(PlacementNewAdopt)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }
    };

    class PrefixBracketNode : public ExpressionNode {
    public:
        PrefixBracketNode(ExpressionNode* base, ExpressionNode* subscript) KJS_FAST_CALL
            : m_base(base)
            , m_subscript(subscript)
        {
        }

        virtual Precedence precedence() const { return PrecUnary; }

    protected:
        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
    };

    class PreIncBracketNode : public PrefixBracketNode {
    public:
        PreIncBracketNode(ExpressionNode* base, ExpressionNode* subscript) KJS_FAST_CALL
            : PrefixBracketNode(base, subscript)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    };

    class PreDecBracketNode : public PrefixBracketNode {
    public:
        PreDecBracketNode(ExpressionNode* base, ExpressionNode* subscript) KJS_FAST_CALL
            : PrefixBracketNode(base, subscript)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    };

    class PrefixDotNode : public ExpressionNode {
    public:
        PrefixDotNode(ExpressionNode* base, const Identifier& ident) KJS_FAST_CALL
            : m_base(base)
            , m_ident(ident)
        {
        }

        virtual Precedence precedence() const { return PrecPostfix; }

    protected:
        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
    };

    class PreIncDotNode : public PrefixDotNode {
    public:
        PreIncDotNode(ExpressionNode* base, const Identifier& ident) KJS_FAST_CALL
            : PrefixDotNode(base, ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    };

    class PreDecDotNode : public PrefixDotNode {
    public:
        PreDecDotNode(ExpressionNode* base, const Identifier& ident) KJS_FAST_CALL
            : PrefixDotNode(base, ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    };

    class PrefixErrorNode : public ExpressionNode {
    public:
        PrefixErrorNode(ExpressionNode* expr, Operator oper) KJS_FAST_CALL
            : m_expr(expr)
            , m_operator(oper)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        RefPtr<ExpressionNode> m_expr;
        Operator m_operator;
    };

    class UnaryPlusNode : public ExpressionNode {
    public:
        UnaryPlusNode(ExpressionNode* expr) KJS_FAST_CALL
            : ExpressionNode(NumberType)
            , m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        RefPtr<ExpressionNode> m_expr;
    };

    class NegateNode : public ExpressionNode {
    public:
        NegateNode(ExpressionNode* expr) KJS_FAST_CALL
            : ExpressionNode(NumberType)
            , m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        RefPtr<ExpressionNode> m_expr;
    };

    class BitwiseNotNode : public ExpressionNode {
    public:
        BitwiseNotNode(ExpressionNode* expr) KJS_FAST_CALL
            : ExpressionNode(NumberType)
            , m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        RefPtr<ExpressionNode> m_expr;
    };

    class LogicalNotNode : public ExpressionNode {
    public:
        LogicalNotNode(ExpressionNode* expr) KJS_FAST_CALL
            : ExpressionNode(BooleanType)
            , m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        RefPtr<ExpressionNode> m_expr;
    };

    class MultNode : public ExpressionNode {
    public:
        MultNode(ExpressionNode* term1, ExpressionNode* term2) KJS_FAST_CALL
            : ExpressionNode(NumberType)
            , m_term1(term1)
            , m_term2(term2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecMultiplicitave; }

    private:
        RefPtr<ExpressionNode> m_term1;
        RefPtr<ExpressionNode> m_term2;
    };

    class DivNode : public ExpressionNode {
    public:
        DivNode(ExpressionNode* term1, ExpressionNode* term2) KJS_FAST_CALL
            : ExpressionNode(NumberType)
            , m_term1(term1)
            , m_term2(term2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecMultiplicitave; }

    private:
        RefPtr<ExpressionNode> m_term1;
        RefPtr<ExpressionNode> m_term2;
    };

    class ModNode : public ExpressionNode {
    public:
        ModNode(ExpressionNode* term1, ExpressionNode* term2) KJS_FAST_CALL
            : ExpressionNode(NumberType)
            , m_term1(term1)
            , m_term2(term2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecMultiplicitave; }

    private:
        RefPtr<ExpressionNode> m_term1;
        RefPtr<ExpressionNode> m_term2;
    };

    class AddNode : public ExpressionNode {
    public:
        AddNode(ExpressionNode* term1, ExpressionNode* term2) KJS_FAST_CALL
            : m_term1(term1)
            , m_term2(term2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecAdditive; }

    protected:
        AddNode(ExpressionNode* term1, ExpressionNode* term2, JSType expectedReturn) KJS_FAST_CALL
            : ExpressionNode(expectedReturn)
            , m_term1(term1)
            , m_term2(term2)
        {
        }

        RefPtr<ExpressionNode> m_term1;
        RefPtr<ExpressionNode> m_term2;
    };

    class SubNode : public ExpressionNode {
    public:
        SubNode(ExpressionNode* term1, ExpressionNode* term2) KJS_FAST_CALL
            : ExpressionNode(NumberType)
            , m_term1(term1)
            , m_term2(term2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecAdditive; }

    private:
        RefPtr<ExpressionNode> m_term1;
        RefPtr<ExpressionNode> m_term2;
    };

    class LeftShiftNode : public ExpressionNode {
    public:
        LeftShiftNode(ExpressionNode* term1, ExpressionNode* term2) KJS_FAST_CALL
            : ExpressionNode(NumberType)
            , m_term1(term1)
            , m_term2(term2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecShift; }

    private:
        RefPtr<ExpressionNode> m_term1;
        RefPtr<ExpressionNode> m_term2;
    };

    class RightShiftNode : public ExpressionNode {
    public:
        RightShiftNode(ExpressionNode* term1, ExpressionNode* term2) KJS_FAST_CALL
            : ExpressionNode(NumberType)
            , m_term1(term1)
            , m_term2(term2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecShift; }

    private:
        RefPtr<ExpressionNode> m_term1;
        RefPtr<ExpressionNode> m_term2;
    };

    class UnsignedRightShiftNode : public ExpressionNode {
    public:
        UnsignedRightShiftNode(ExpressionNode* term1, ExpressionNode* term2) KJS_FAST_CALL
            : ExpressionNode(NumberType)
            , m_term1(term1)
            , m_term2(term2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecShift; }

    private:
        RefPtr<ExpressionNode> m_term1;
        RefPtr<ExpressionNode> m_term2;
    };

    class LessNode : public ExpressionNode {
    public:
        LessNode(ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : ExpressionNode(BooleanType)
            , m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecRelational; }

    protected:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };

    class GreaterNode : public ExpressionNode {
    public:
        GreaterNode(ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecRelational; }

    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };

    class LessEqNode : public ExpressionNode {
    public:
        LessEqNode(ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecRelational; }

    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };

    class GreaterEqNode : public ExpressionNode {
    public:
        GreaterEqNode(ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecRelational; }

    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };

    class InstanceOfNode : public ExpressionNode {
    public:
        InstanceOfNode(ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : ExpressionNode(BooleanType)
            , m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecRelational; }

    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };

    class InNode : public ExpressionNode {
    public:
        InNode(ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecRelational; }

    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };

    class EqualNode : public ExpressionNode {
    public:
        EqualNode(ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : ExpressionNode(BooleanType)
            , m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecEquality; }

    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };

    class NotEqualNode : public ExpressionNode {
    public:
        NotEqualNode(ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : ExpressionNode(BooleanType)
            , m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecEquality; }

    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };

    class StrictEqualNode : public ExpressionNode {
    public:
        StrictEqualNode(ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : ExpressionNode(BooleanType)
            , m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecEquality; }

    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };

    class NotStrictEqualNode : public ExpressionNode {
    public:
        NotStrictEqualNode(ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : ExpressionNode(BooleanType)
            , m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecEquality; }

    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };

    class BitAndNode : public ExpressionNode {
    public:
        BitAndNode(ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : ExpressionNode(NumberType)
            , m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecBitwiseAnd; }

    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };

    class BitOrNode : public ExpressionNode {
    public:
        BitOrNode(ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : ExpressionNode(NumberType)
            , m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecBitwiseOr; }

    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };

    class BitXOrNode : public ExpressionNode {
    public:
        BitXOrNode(ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : ExpressionNode(NumberType)
            , m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecBitwiseXor; }

    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };

    /**
     * m_expr1 && m_expr2, m_expr1 || m_expr2
     */
    class LogicalAndNode : public ExpressionNode {
    public:
        LogicalAndNode(ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : ExpressionNode(BooleanType)
            , m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecLogicalAnd; }

    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };

    class LogicalOrNode : public ExpressionNode {
    public:
        LogicalOrNode(ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : ExpressionNode(BooleanType)
            , m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecLogicalOr; }

    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };

    /**
     * The ternary operator, "m_logical ? m_expr1 : m_expr2"
     */
    class ConditionalNode : public ExpressionNode {
    public:
        ConditionalNode(ExpressionNode* logical, ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : m_logical(logical)
            , m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecConditional; }

    private:
        RefPtr<ExpressionNode> m_logical;
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };

    class ReadModifyResolveNode : public ExpressionNode {
    public:
        ReadModifyResolveNode(const Identifier& ident, Operator oper, ExpressionNode*  right, bool rightHasAssignments) KJS_FAST_CALL
            : m_ident(ident)
            , m_right(right)
            , m_operator(oper)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        ReadModifyResolveNode(PlacementNewAdoptType) KJS_FAST_CALL
            : ExpressionNode(PlacementNewAdopt)
            , m_ident(PlacementNewAdopt)
            , m_right(PlacementNewAdopt)
            , m_rightHasAssignments(true)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecAssignment; }

    protected:
        Identifier m_ident;
        RefPtr<ExpressionNode> m_right;
        size_t m_index; // Used by ReadModifyLocalVarNode.
        Operator m_operator : 31;
        bool m_rightHasAssignments : 1;
    };

    class AssignResolveNode : public ExpressionNode {
    public:
        AssignResolveNode(const Identifier& ident, ExpressionNode* right, bool rightHasAssignments) KJS_FAST_CALL
            : m_ident(ident)
            , m_right(right)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        AssignResolveNode(PlacementNewAdoptType) KJS_FAST_CALL
            : ExpressionNode(PlacementNewAdopt)
            , m_ident(PlacementNewAdopt)
            , m_right(PlacementNewAdopt)
        {
        }
        
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecAssignment; }

    protected:
        Identifier m_ident;
        RefPtr<ExpressionNode> m_right;
        size_t m_index; // Used by ReadModifyLocalVarNode.
        bool m_rightHasAssignments;
    };

    class ReadModifyBracketNode : public ExpressionNode {
    public:
        ReadModifyBracketNode(ExpressionNode* base, ExpressionNode* subscript, Operator oper, ExpressionNode* right, bool subscriptHasAssignments, bool rightHasAssignments) KJS_FAST_CALL
            : m_base(base)
            , m_subscript(subscript)
            , m_right(right)
            , m_operator(oper)
            , m_subscriptHasAssignments(subscriptHasAssignments)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecAssignment; }

    protected:
        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
        RefPtr<ExpressionNode> m_right;
        Operator m_operator : 30;
        bool m_subscriptHasAssignments : 1;
        bool m_rightHasAssignments : 1;
    };

    class AssignBracketNode : public ExpressionNode {
    public:
        AssignBracketNode(ExpressionNode* base, ExpressionNode* subscript, ExpressionNode* right, bool subscriptHasAssignments, bool rightHasAssignments) KJS_FAST_CALL
            : m_base(base)
            , m_subscript(subscript)
            , m_right(right)
            , m_subscriptHasAssignments(subscriptHasAssignments)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecAssignment; }

    protected:
        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
        RefPtr<ExpressionNode> m_right;
        bool m_subscriptHasAssignments : 1;
        bool m_rightHasAssignments : 1;
    };

    class AssignDotNode : public ExpressionNode {
    public:
        AssignDotNode(ExpressionNode* base, const Identifier& ident, ExpressionNode* right, bool rightHasAssignments) KJS_FAST_CALL
            : m_base(base)
            , m_ident(ident)
            , m_right(right)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecAssignment; }

    protected:
        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
        RefPtr<ExpressionNode> m_right;
        bool m_rightHasAssignments;
    };

    class ReadModifyDotNode : public ExpressionNode {
    public:
        ReadModifyDotNode(ExpressionNode* base, const Identifier& ident, Operator oper, ExpressionNode* right, bool rightHasAssignments) KJS_FAST_CALL
            : m_base(base)
            , m_ident(ident)
            , m_right(right)
            , m_operator(oper)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecAssignment; }

    protected:
        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
        RefPtr<ExpressionNode> m_right;
        Operator m_operator : 31;
        bool m_rightHasAssignments : 1;
    };

    class AssignErrorNode : public ExpressionNode {
    public:
        AssignErrorNode(ExpressionNode* left, Operator oper, ExpressionNode* right) KJS_FAST_CALL
            : m_left(left)
            , m_operator(oper)
            , m_right(right)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecAssignment; }

    protected:
        RefPtr<ExpressionNode> m_left;
        Operator m_operator;
        RefPtr<ExpressionNode> m_right;
    };

    class CommaNode : public ExpressionNode {
    public:
        CommaNode(ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecExpression; }

    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };
    
    class VarDeclCommaNode : public CommaNode {
    public:
        VarDeclCommaNode(ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : CommaNode(expr1, expr2)
        {
        }
        virtual Precedence precedence() const { return PrecAssignment; }
    };

    class ConstDeclNode : public ExpressionNode {
    public:
        ConstDeclNode(const Identifier& ident, ExpressionNode* in) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
        PassRefPtr<ConstDeclNode> releaseNext() KJS_FAST_CALL { return m_next.release(); }

        Identifier m_ident;
        ListRefPtr<ConstDeclNode> m_next;
        RefPtr<ExpressionNode> m_init;
        
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual RegisterID* emitCodeSingle(CodeGenerator&) KJS_FAST_CALL;
    };

    class ConstStatementNode : public StatementNode {
    public:
        ConstStatementNode(ConstDeclNode* next) KJS_FAST_CALL
            : m_next(next)
        {
        }

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

    private:
        RefPtr<ConstDeclNode> m_next;
    };

    typedef Vector<RefPtr<StatementNode> > StatementVector;

    class SourceElements : public ParserRefCounted {
    public:
        void append(PassRefPtr<StatementNode>);
        void releaseContentsIntoVector(StatementVector& destination)
        {
            ASSERT(destination.isEmpty());
            m_statements.swap(destination);
        }

    private:
        StatementVector m_statements;
    };

    class BlockNode : public StatementNode {
    public:
        BlockNode(SourceElements* children) KJS_FAST_CALL;

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

        StatementVector& children() { return m_children; }

    protected:
        StatementVector m_children;
    };

    class EmptyStatementNode : public StatementNode {
    public:
        EmptyStatementNode() KJS_FAST_CALL // debug
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual bool isEmptyStatement() const KJS_FAST_CALL { return true; }
    };
    
    class DebuggerStatementNode : public StatementNode {
    public:
        DebuggerStatementNode() KJS_FAST_CALL
        {
        }
        
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    };

    class ExprStatementNode : public StatementNode {
    public:
        ExprStatementNode(ExpressionNode* expr) KJS_FAST_CALL
            : m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        RefPtr<ExpressionNode> m_expr;
    };

    class VarStatementNode : public StatementNode {
    public:
        VarStatementNode(ExpressionNode* expr) KJS_FAST_CALL
            : m_expr(expr)
        {
        }
        
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        RefPtr<ExpressionNode> m_expr;
    };

    class IfNode : public StatementNode {
    public:
        IfNode(ExpressionNode* condition, StatementNode* ifBlock) KJS_FAST_CALL
            : m_condition(condition)
            , m_ifBlock(ifBlock)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    protected:
        RefPtr<ExpressionNode> m_condition;
        RefPtr<StatementNode> m_ifBlock;
    };

    class IfElseNode : public IfNode {
    public:
        IfElseNode(ExpressionNode* condtion, StatementNode* ifBlock, StatementNode* elseBlock) KJS_FAST_CALL
            : IfNode(condtion, ifBlock)
            , m_elseBlock(elseBlock)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        RefPtr<StatementNode> m_elseBlock;
    };

    class DoWhileNode : public StatementNode {
    public:
        DoWhileNode(StatementNode* statement, ExpressionNode* expr) KJS_FAST_CALL
            : m_statement(statement)
            , m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        RefPtr<StatementNode> m_statement;
        RefPtr<ExpressionNode> m_expr;
    };

    class WhileNode : public StatementNode {
    public:
        WhileNode(ExpressionNode* expr, StatementNode* statement) KJS_FAST_CALL
            : m_expr(expr)
            , m_statement(statement)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        RefPtr<ExpressionNode> m_expr;
        RefPtr<StatementNode> m_statement;
    };

    class ForNode : public StatementNode {
    public:
        ForNode(ExpressionNode* expr1, ExpressionNode* expr2, ExpressionNode* expr3, StatementNode* statement, bool expr1WasVarDecl) KJS_FAST_CALL
            : m_expr1(expr1)
            , m_expr2(expr2)
            , m_expr3(expr3)
            , m_statement(statement)
            , m_expr1WasVarDecl(expr1 && expr1WasVarDecl)
        {
            ASSERT(statement);
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
        RefPtr<ExpressionNode> m_expr3;
        RefPtr<StatementNode> m_statement;
        bool m_expr1WasVarDecl;
    };

    class ForInNode : public StatementNode {
    public:
        ForInNode(ExpressionNode*, ExpressionNode*, StatementNode*) KJS_FAST_CALL;
        ForInNode(const Identifier&, ExpressionNode*, ExpressionNode*, StatementNode*) KJS_FAST_CALL;
        
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        Identifier m_ident;
        RefPtr<ExpressionNode> m_init;
        RefPtr<ExpressionNode> m_lexpr;
        RefPtr<ExpressionNode> m_expr;
        RefPtr<StatementNode> m_statement;
        bool m_identIsVarDecl;
    };

    class ContinueNode : public StatementNode {
    public:
        ContinueNode() KJS_FAST_CALL
        {
        }

        ContinueNode(const Identifier& ident) KJS_FAST_CALL
            : m_ident(ident)
        {
        }
        
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        Identifier m_ident;
    };

    class BreakNode : public StatementNode {
    public:
        BreakNode() KJS_FAST_CALL
        {
        }

        BreakNode(const Identifier& ident) KJS_FAST_CALL
            : m_ident(ident)
        {
        }
        
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        Identifier m_ident;
    };

    class ReturnNode : public StatementNode {
    public:
        ReturnNode(ExpressionNode* value) KJS_FAST_CALL
            : m_value(value)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual bool isReturnNode() const KJS_FAST_CALL { return true; }

    private:
        RefPtr<ExpressionNode> m_value;
    };

    class WithNode : public StatementNode {
    public:
        WithNode(ExpressionNode* expr, StatementNode* statement) KJS_FAST_CALL
            : m_expr(expr)
            , m_statement(statement)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        RefPtr<ExpressionNode> m_expr;
        RefPtr<StatementNode> m_statement;
    };

    class LabelNode : public StatementNode {
    public:
        LabelNode(const Identifier& label, StatementNode* statement) KJS_FAST_CALL
            : m_label(label)
            , m_statement(statement)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual void pushLabel(const Identifier& ident) KJS_FAST_CALL { m_statement->pushLabel(ident); }

    private:
        Identifier m_label;
        RefPtr<StatementNode> m_statement;
    };

    class ThrowNode : public StatementNode {
    public:
        ThrowNode(ExpressionNode* expr) KJS_FAST_CALL
            : m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        RefPtr<ExpressionNode> m_expr;
    };

    class TryNode : public StatementNode {
    public:
        TryNode(StatementNode* tryBlock, const Identifier& exceptionIdent, StatementNode* catchBlock, StatementNode* finallyBlock) KJS_FAST_CALL
            : m_tryBlock(tryBlock)
            , m_exceptionIdent(exceptionIdent)
            , m_catchBlock(catchBlock)
            , m_finallyBlock(finallyBlock)
        {
        }

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* dst = 0) KJS_FAST_CALL;

    private:
        RefPtr<StatementNode> m_tryBlock;
        Identifier m_exceptionIdent;
        RefPtr<StatementNode> m_catchBlock;
        RefPtr<StatementNode> m_finallyBlock;
    };

    class ParameterNode : public Node {
    public:
        ParameterNode(const Identifier& ident) KJS_FAST_CALL
            : m_ident(ident)
        {
        }

        ParameterNode(ParameterNode* l, const Identifier& ident) KJS_FAST_CALL
            : m_ident(ident)
        {
            l->m_next = this;
        }

        Identifier ident() KJS_FAST_CALL { return m_ident; }
        ParameterNode *nextParam() KJS_FAST_CALL { return m_next.get(); }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        PassRefPtr<ParameterNode> releaseNext() KJS_FAST_CALL { return m_next.release(); }
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

    private:
        friend class FuncDeclNode;
        friend class FuncExprNode;
        Identifier m_ident;
        ListRefPtr<ParameterNode> m_next;
    };

    class ScopeNode : public BlockNode {
    public:
        ScopeNode(SourceElements*, VarStack*, FunctionStack*, bool usesEval, bool needsClosure) KJS_FAST_CALL;

        int sourceId() const KJS_FAST_CALL { return m_sourceId; }
        const UString& sourceURL() const KJS_FAST_CALL { return m_sourceURL; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        
        bool usesEval() const { return m_usesEval; }
        bool needsClosure() const { return m_needsClosure; }
        
        VarStack& varStack() { return m_varStack; }
        FunctionStack& functionStack() { return m_functionStack; }
        
    protected:
        VarStack m_varStack;
        FunctionStack m_functionStack;

    private:
        UString m_sourceURL;
        int m_sourceId;
        bool m_usesEval;
        bool m_needsClosure;
    };

    class ProgramNode : public ScopeNode {
    public:
        static ProgramNode* create(SourceElements*, VarStack*, FunctionStack*, bool usesEval, bool needsClosure) KJS_FAST_CALL;
        virtual ~ProgramNode();
        
        ProgramCodeBlock& code(ScopeChainNode* scopeChain, bool canCreateGlobals) KJS_FAST_CALL
        {
            if (!m_code)
                generateCode(scopeChain, canCreateGlobals);
            return *m_code;
        }

    private:
        ProgramNode(SourceElements*, VarStack*, FunctionStack*, bool usesEval, bool needsClosure) KJS_FAST_CALL;

        void generateCode(ScopeChainNode*, bool) KJS_FAST_CALL;
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        Vector<size_t> m_varIndexes; // Storage indexes belonging to the nodes in m_varStack. (Recorded to avoid double lookup.)
        Vector<size_t> m_functionIndexes; // Storage indexes belonging to the nodes in m_functionStack. (Recorded to avoid double lookup.)

        OwnPtr<ProgramCodeBlock> m_code;
    };

    class EvalNode : public ScopeNode {
    public:
        static EvalNode* create(SourceElements*, VarStack*, FunctionStack*, bool usesEval, bool needsClosure) KJS_FAST_CALL;
        virtual ~EvalNode();
        
        EvalCodeBlock& code(ScopeChainNode* scopeChain) KJS_FAST_CALL
        {
            if (!m_code)
                generateCode(scopeChain);
            return *m_code;
        }

    private:
        EvalNode(SourceElements*, VarStack*, FunctionStack*, bool usesEval, bool needsClosure) KJS_FAST_CALL;

        void generateCode(ScopeChainNode*) KJS_FAST_CALL;
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        OwnPtr<EvalCodeBlock> m_code;
    };

    class FunctionBodyNode : public ScopeNode {
    public:
        static FunctionBodyNode* create(SourceElements*, VarStack*, FunctionStack*, bool usesEval, bool needsClosure) KJS_FAST_CALL;
        virtual ~FunctionBodyNode();
        
        Vector<Identifier>& parameters() KJS_FAST_CALL { return m_parameters; }
        UString paramString() const KJS_FAST_CALL;

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        
        SymbolTable& symbolTable() { return m_symbolTable; } // FIXME: Remove this
        
        CodeBlock& code(ScopeChainNode* scopeChain) KJS_FAST_CALL
        {
            ASSERT(scopeChain);
            if (!m_code)
                generateCode(scopeChain);
            return *m_code;
        }

        CodeBlock& generatedCode() KJS_FAST_CALL
        {
            ASSERT(m_code);
            return *m_code;
        }

        void mark();

        void setSource(const SourceRange& source) { m_source = source; } 
        UString toSourceString() const KJS_FAST_CALL { return UString("{") + m_source.toString() + UString("}"); }

    protected:
        FunctionBodyNode(SourceElements*, VarStack*, FunctionStack*, bool usesEval, bool needsClosure) KJS_FAST_CALL;

    private:
        void generateCode(ScopeChainNode*) KJS_FAST_CALL;
        
        Vector<Identifier> m_parameters;
        SymbolTable m_symbolTable;
        OwnPtr<CodeBlock> m_code;
        SourceRange m_source;
    };

    class FuncExprNode : public ExpressionNode {
    public:
        FuncExprNode(const Identifier& ident, FunctionBodyNode* body, const SourceRange& source, ParameterNode* parameter = 0) KJS_FAST_CALL
            : m_ident(ident)
            , m_parameter(parameter)
            , m_body(body)
        {
            addParams();
            m_body->setSource(source);
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        FunctionImp* makeFunction(ExecState*, ScopeChainNode*) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecMember; }
        virtual bool needsParensIfLeftmost() const { return true; }

        FunctionBodyNode* body() { return m_body.get(); }

    private:
        void addParams() KJS_FAST_CALL;

        // Used for streamTo
        friend class PropertyNode;
        Identifier m_ident;
        RefPtr<ParameterNode> m_parameter;
        RefPtr<FunctionBodyNode> m_body;
    };

    class FuncDeclNode : public StatementNode {
    public:
        FuncDeclNode(const Identifier& ident, FunctionBodyNode* body, const SourceRange& source, ParameterNode* parameter = 0) KJS_FAST_CALL
            : m_ident(ident)
            , m_parameter(parameter)
            , m_body(body)
        {
            addParams();
            m_body->setSource(source);
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        FunctionImp* makeFunction(ExecState*, ScopeChainNode*) KJS_FAST_CALL;

        Identifier m_ident;

        FunctionBodyNode* body() { return m_body.get(); }

    private:
        void addParams() KJS_FAST_CALL;

        RefPtr<ParameterNode> m_parameter;
        RefPtr<FunctionBodyNode> m_body;
    };

    class CaseClauseNode : public Node {
    public:
        CaseClauseNode(ExpressionNode* expr) KJS_FAST_CALL
            : m_expr(expr)
        {
        }

        CaseClauseNode(ExpressionNode* expr, SourceElements* children) KJS_FAST_CALL
            : m_expr(expr)
        {
            if (children)
                children->releaseContentsIntoVector(m_children);
        }

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

        ExpressionNode* expr() const { return m_expr.get(); }
        StatementVector& children() { return m_children; }

    private:
        RefPtr<ExpressionNode> m_expr;
        StatementVector m_children;
    };

    class ClauseListNode : public Node {
    public:
        ClauseListNode(CaseClauseNode* clause) KJS_FAST_CALL
            : m_clause(clause)
        {
        }

        ClauseListNode(ClauseListNode* clauseList, CaseClauseNode* clause) KJS_FAST_CALL
            : m_clause(clause)
        {
            clauseList->m_next = this;
        }

        CaseClauseNode* getClause() const KJS_FAST_CALL { return m_clause.get(); }
        ClauseListNode* getNext() const KJS_FAST_CALL { return m_next.get(); }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        PassRefPtr<ClauseListNode> releaseNext() KJS_FAST_CALL { return m_next.release(); }
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

    private:
        friend class CaseBlockNode;
        RefPtr<CaseClauseNode> m_clause;
        ListRefPtr<ClauseListNode> m_next;
    };

    class CaseBlockNode : public Node {
    public:
        CaseBlockNode(ClauseListNode* list1, CaseClauseNode* defaultClause, ClauseListNode* list2) KJS_FAST_CALL
            : m_list1(list1)
            , m_defaultClause(defaultClause)
            , m_list2(list2)
        {
        }

        RegisterID* emitCodeForBlock(CodeGenerator&, RegisterID* input, RegisterID* dst = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

    private:
        RefPtr<ClauseListNode> m_list1;
        RefPtr<CaseClauseNode> m_defaultClause;
        RefPtr<ClauseListNode> m_list2;
    };

    class SwitchNode : public StatementNode {
    public:
        SwitchNode(ExpressionNode* expr, CaseBlockNode* block) KJS_FAST_CALL
            : m_expr(expr)
            , m_block(block)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        RefPtr<ExpressionNode> m_expr;
        RefPtr<CaseBlockNode> m_block;
    };

    class BreakpointCheckStatement : public StatementNode {
    public:
        BreakpointCheckStatement(PassRefPtr<StatementNode>) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        RefPtr<StatementNode> m_statement;
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

    struct ConstDeclList {
        ConstDeclNode* head;
        ConstDeclNode* tail;
    };

    struct ParameterList {
        ParameterNode* head;
        ParameterNode* tail;
    };

    struct ClauseList {
        ClauseListNode* head;
        ClauseListNode* tail;
    };

} // namespace KJS

#endif // NODES_H_
