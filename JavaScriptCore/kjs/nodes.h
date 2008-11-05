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

#include "Error.h"
#include "JSString.h"
#include "JSType.h"
#include "Opcode.h"
#include "RegisterID.h"
#include "ResultType.h"
#include "SourceCode.h"
#include "SymbolTable.h"
#include "RegExp.h"
#include <wtf/ListRefPtr.h>
#include <wtf/MathExtras.h>
#include <wtf/OwnPtr.h>
#include <wtf/UnusedParam.h>
#include <wtf/Vector.h>

#if PLATFORM(X86) && COMPILER(GCC)
#define JSC_FAST_CALL __attribute__((regparm(3)))
#else
#define JSC_FAST_CALL
#endif

namespace JSC {

    class CodeBlock;
    class CodeGenerator;
    class FuncDeclNode;
    class Node;
    class EvalCodeBlock;
    class JSFunction;
    class ProgramCodeBlock;
    class PropertyListNode;
    class SourceStream;

    typedef unsigned int CodeFeatures;

    const CodeFeatures NoFeatures = 0;
    const CodeFeatures EvalFeature = 1 << 0;
    const CodeFeatures ClosureFeature = 1 << 1;
    const CodeFeatures AssignFeature = 1 << 2;
    const CodeFeatures ArgumentsFeature = 1 << 3;
    const CodeFeatures WithFeature = 1 << 4;
    const CodeFeatures CatchFeature = 1 << 5;
    const CodeFeatures ThisFeature = 1 << 6;
    const CodeFeatures AllFeatures = EvalFeature | ClosureFeature | AssignFeature | ArgumentsFeature | WithFeature | CatchFeature | ThisFeature;

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
        OpURShift
    };
    
    enum LogicalOperator {
        OpLogicalAnd,
        OpLogicalOr
    };

    enum Precedence {
        PrecPrimary,
        PrecMember,
        PrecCall,
        PrecLeftHandSide,
        PrecPostfix,
        PrecUnary,
        PrecMultiplicative,
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

    namespace DeclarationStacks {
        typedef Vector<Node*, 16> NodeStack;
        enum VarAttrs { IsConstant = 1, HasInitializer = 2 };
        typedef Vector<std::pair<Identifier, unsigned>, 16> VarStack;
        typedef Vector<RefPtr<FuncDeclNode>, 16> FunctionStack;
    }

    struct SwitchInfo {
        enum SwitchType { SwitchNone, SwitchImmediate, SwitchCharacter, SwitchString };
        uint32_t opcodeOffset;
        SwitchType switchType;
    };

    class ParserRefCounted : Noncopyable {
    protected:
        ParserRefCounted(JSGlobalData*) JSC_FAST_CALL;

        JSGlobalData* m_globalData;

    public:
        void ref() JSC_FAST_CALL;
        void deref() JSC_FAST_CALL;
        bool hasOneRef() JSC_FAST_CALL;

        static void deleteNewObjects(JSGlobalData*) JSC_FAST_CALL;

        virtual ~ParserRefCounted();
    };

    class Node : public ParserRefCounted {
    public:
        typedef DeclarationStacks::NodeStack NodeStack;
        typedef DeclarationStacks::VarStack VarStack;
        typedef DeclarationStacks::FunctionStack FunctionStack;

        Node(JSGlobalData*) JSC_FAST_CALL;

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
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* dst = 0) JSC_FAST_CALL 
        {
            ASSERT_WITH_MESSAGE(0, "Don't know how to generate code for:\n%s\n", toString().ascii());
            UNUSED_PARAM(dst); 
            return 0; 
        } // FIXME: Make this pure virtual.

        UString toString() const JSC_FAST_CALL;
        int lineNo() const { return m_line; }

        virtual bool isReturnNode() const JSC_FAST_CALL { return false; }

        // Serialization.
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL = 0;
        virtual Precedence precedence() const = 0;
        virtual bool needsParensIfLeftmost() const { return false; }
        
    protected:
        int m_line;
    };

    class ExpressionNode : public Node {
    public:
        ExpressionNode(JSGlobalData* globalData, ResultType resultDesc = ResultType::unknown()) JSC_FAST_CALL
            : Node(globalData)
            , m_resultDesc(resultDesc)
        {
        }

        virtual bool isNumber() const JSC_FAST_CALL { return false; }
        virtual bool isString() const JSC_FAST_CALL { return false; }
        virtual bool isNull() const JSC_FAST_CALL { return false; }
        virtual bool isPure(CodeGenerator&) const JSC_FAST_CALL { return false; }        
        virtual bool isLocation() const JSC_FAST_CALL { return false; }
        virtual bool isResolveNode() const JSC_FAST_CALL { return false; }
        virtual bool isBracketAccessorNode() const JSC_FAST_CALL { return false; }
        virtual bool isDotAccessorNode() const JSC_FAST_CALL { return false; }

        virtual ExpressionNode* stripUnaryPlus() { return this; }

        ResultType resultDescriptor() const JSC_FAST_CALL { return m_resultDesc; }

        // This needs to be in public in order to compile using GCC 3.x 
        typedef enum { EvalOperator, FunctionCall } CallerType;

    private:
        ResultType m_resultDesc;
    };

    class StatementNode : public Node {
    public:
        StatementNode(JSGlobalData*) JSC_FAST_CALL;
        void setLoc(int line0, int line1) JSC_FAST_CALL;
        int firstLine() const JSC_FAST_CALL { return lineNo(); }
        int lastLine() const JSC_FAST_CALL { return m_lastLine; }

        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
        virtual bool isEmptyStatement() const JSC_FAST_CALL { return false; }

        virtual bool isBlock() const JSC_FAST_CALL { return false; }
        virtual bool isLoop() const JSC_FAST_CALL { return false; }

    private:
        int m_lastLine;
    };

    class NullNode : public ExpressionNode {
    public:
        NullNode(JSGlobalData* globalData) JSC_FAST_CALL
            : ExpressionNode(globalData, ResultType::nullType())
        {
        }

        virtual bool isNull() const JSC_FAST_CALL { return true; }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }
    };

    class BooleanNode : public ExpressionNode {
    public:
        BooleanNode(JSGlobalData* globalData, bool value) JSC_FAST_CALL
            : ExpressionNode(globalData, ResultType::boolean())
            , m_value(value)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual bool isPure(CodeGenerator&) const JSC_FAST_CALL { return true; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }

    protected:
        bool m_value;
    };

    class NumberNode : public ExpressionNode {
    public:
        NumberNode(JSGlobalData* globalData, double v) JSC_FAST_CALL
            : ExpressionNode(globalData, ResultType::constNumber())
            , m_double(v)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return signbit(m_double) ? PrecUnary : PrecPrimary; }

        virtual bool isNumber() const JSC_FAST_CALL { return true; }
        virtual bool isPure(CodeGenerator&) const JSC_FAST_CALL { return true; }
        double value() const JSC_FAST_CALL { return m_double; }
        virtual void setValue(double d) JSC_FAST_CALL { m_double = d; }

    protected:
        double m_double;
    };

    class ImmediateNumberNode : public NumberNode {
    public:
        ImmediateNumberNode(JSGlobalData* globalData, JSValue* v, double d) JSC_FAST_CALL
            : NumberNode(globalData, d)
            , m_value(v)
        {
            ASSERT(v == JSImmediate::from(d));
        }

        virtual void setValue(double d) JSC_FAST_CALL { m_double = d; m_value = JSImmediate::from(d); ASSERT(m_value); }

    private:
        JSValue* m_value; // This is never a JSCell, only JSImmediate, thus no ProtectedPtr
    };

    class StringNode : public ExpressionNode {
    public:
        StringNode(JSGlobalData* globalData, const Identifier& v) JSC_FAST_CALL
            : ExpressionNode(globalData, ResultType::string())
            , m_value(v)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        
        virtual bool isString() const JSC_FAST_CALL { return true; }
        const Identifier& value() { return m_value; }
        virtual bool isPure(CodeGenerator&) const JSC_FAST_CALL { return true; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }

    private:
        Identifier m_value;
    };
    
    class ThrowableExpressionData {
    public:
        ThrowableExpressionData()
            : m_divot(static_cast<uint32_t>(-1))
            , m_startOffset(static_cast<uint16_t>(-1))
            , m_endOffset(static_cast<uint16_t>(-1))
        {
        }
        
        ThrowableExpressionData(unsigned divot, unsigned startOffset, unsigned endOffset)
            : m_divot(divot)
            , m_startOffset(startOffset)
            , m_endOffset(endOffset)
        {
        }
        
        void setExceptionSourceCode(unsigned divot, unsigned startOffset, unsigned endOffset)
        {
            m_divot = divot;
            m_startOffset = startOffset;
            m_endOffset = endOffset;
        }

        uint32_t divot() const { return m_divot; }
        uint16_t startOffset() const { return m_startOffset; }
        uint16_t endOffset() const { return m_endOffset; }

    protected:
        RegisterID* emitThrowError(CodeGenerator&, ErrorType, const char* msg);
        RegisterID* emitThrowError(CodeGenerator&, ErrorType, const char* msg, const Identifier&);
        uint32_t m_divot;
        uint16_t m_startOffset;
        uint16_t m_endOffset;
    };

    class ThrowableSubExpressionData : public ThrowableExpressionData {
    public:
        ThrowableSubExpressionData()
            : ThrowableExpressionData()
            , m_subexpressionDivotOffset(0)
            , m_subexpressionEndOffset(0)
        {
        }

        ThrowableSubExpressionData(unsigned divot, unsigned startOffset, unsigned endOffset)
            : ThrowableExpressionData(divot, startOffset, endOffset)
            , m_subexpressionDivotOffset(0)
            , m_subexpressionEndOffset(0)
        {
        }

        void setSubexpressionInfo(uint32_t subexpressionDivot, uint16_t subexpressionOffset) {
            ASSERT(subexpressionDivot <= m_divot);
            if ((m_divot - subexpressionDivot) & ~0xFFFF) // Overflow means we can't do this safely, so just point at the primary divot
                return;
            m_subexpressionDivotOffset = m_divot - subexpressionDivot;
            m_subexpressionEndOffset = subexpressionOffset;
        }

    protected:
        uint16_t m_subexpressionDivotOffset;
        uint16_t m_subexpressionEndOffset;
    };
    
    class ThrowablePrefixedSubExpressionData : public ThrowableExpressionData {
    public:
        ThrowablePrefixedSubExpressionData()
            : ThrowableExpressionData()
            , m_subexpressionDivotOffset(0)
            , m_subexpressionStartOffset(0)
        {
        }

        ThrowablePrefixedSubExpressionData(unsigned divot, unsigned startOffset, unsigned endOffset)
            : ThrowableExpressionData(divot, startOffset, endOffset)
            , m_subexpressionDivotOffset(0)
            , m_subexpressionStartOffset(0)
        {
        }

        void setSubexpressionInfo(uint32_t subexpressionDivot, uint16_t subexpressionOffset) {
            ASSERT(subexpressionDivot >= m_divot);
            if ((subexpressionDivot - m_divot) & ~0xFFFF) // Overflow means we can't do this safely, so just point at the primary divot
                return;
            m_subexpressionDivotOffset = subexpressionDivot - m_divot;
            m_subexpressionStartOffset = subexpressionOffset;
        }

    protected:
        uint16_t m_subexpressionDivotOffset;
        uint16_t m_subexpressionStartOffset;
    };

    class RegExpNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        RegExpNode(JSGlobalData* globalData, const UString& pattern, const UString& flags) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , m_pattern(pattern)
            , m_flags(flags)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }

    private:
        UString m_pattern;
        UString m_flags;
    };

    class ThisNode : public ExpressionNode {
    public:
        ThisNode(JSGlobalData* globalData) JSC_FAST_CALL
            : ExpressionNode(globalData)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }
    };

    class ResolveNode : public ExpressionNode {
    public:
        ResolveNode(JSGlobalData* globalData, const Identifier& ident, int startOffset) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , m_ident(ident)
            , m_startOffset(startOffset)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }

        virtual bool isPure(CodeGenerator&) const JSC_FAST_CALL;
        virtual bool isLocation() const JSC_FAST_CALL { return true; }
        virtual bool isResolveNode() const JSC_FAST_CALL { return true; }
        const Identifier& identifier() const JSC_FAST_CALL { return m_ident; }

    protected:
        Identifier m_ident;
        int32_t m_startOffset;
        
    };

    class ElementNode : public Node {
    public:
        ElementNode(JSGlobalData* globalData, int elision, ExpressionNode* node) JSC_FAST_CALL
            : Node(globalData)
            , m_elision(elision)
            , m_node(node)
        {
        }

        ElementNode(JSGlobalData* globalData, ElementNode* l, int elision, ExpressionNode* node) JSC_FAST_CALL
            : Node(globalData)
            , m_elision(elision)
            , m_node(node)
        {
            l->m_next = this;
        }

        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;

        int elision() const { return m_elision; }
        ExpressionNode* value() { return m_node.get(); }

        ElementNode* next() { return m_next.get(); }
        PassRefPtr<ElementNode> releaseNext() JSC_FAST_CALL { return m_next.release(); }

    private:
        ListRefPtr<ElementNode> m_next;
        int m_elision;
        RefPtr<ExpressionNode> m_node;
    };

    class ArrayNode : public ExpressionNode {
    public:
        ArrayNode(JSGlobalData* globalData, int elision) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , m_elision(elision)
            , m_optional(true)
        {
        }

        ArrayNode(JSGlobalData* globalData, ElementNode* element) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , m_element(element)
            , m_elision(0)
            , m_optional(false)
        {
        }

        ArrayNode(JSGlobalData* globalData, int elision, ElementNode* element) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , m_element(element)
            , m_elision(elision)
            , m_optional(true)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }

    private:
        RefPtr<ElementNode> m_element;
        int m_elision;
        bool m_optional;
    };

    class PropertyNode : public Node {
    public:
        enum Type { Constant, Getter, Setter };

        PropertyNode(JSGlobalData* globalData, const Identifier& name, ExpressionNode* assign, Type type) JSC_FAST_CALL
            : Node(globalData)
            , m_name(name)
            , m_assign(assign)
            , m_type(type)
        {
        }

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
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
        PropertyListNode(JSGlobalData* globalData, PropertyNode* node) JSC_FAST_CALL
            : Node(globalData)
            , m_node(node)
        {
        }

        PropertyListNode(JSGlobalData* globalData, PropertyNode* node, PropertyListNode* list) JSC_FAST_CALL
            : Node(globalData)
            , m_node(node)
        {
            list->m_next = this;
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

        PassRefPtr<PropertyListNode> releaseNext() JSC_FAST_CALL { return m_next.release(); }

    private:
        friend class ObjectLiteralNode;
        RefPtr<PropertyNode> m_node;
        ListRefPtr<PropertyListNode> m_next;
    };

    class ObjectLiteralNode : public ExpressionNode {
    public:
        ObjectLiteralNode(JSGlobalData* globalData) JSC_FAST_CALL
            : ExpressionNode(globalData)
        {
        }

        ObjectLiteralNode(JSGlobalData* globalData, PropertyListNode* list) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , m_list(list)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }
        virtual bool needsParensIfLeftmost() const { return true; }

    private:
        RefPtr<PropertyListNode> m_list;
    };
    
    class BracketAccessorNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        BracketAccessorNode(JSGlobalData* globalData, ExpressionNode* base, ExpressionNode* subscript, bool subscriptHasAssignments) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , m_base(base)
            , m_subscript(subscript)
            , m_subscriptHasAssignments(subscriptHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecMember; }

        virtual bool isLocation() const JSC_FAST_CALL { return true; }
        virtual bool isBracketAccessorNode() const JSC_FAST_CALL { return true; }
        ExpressionNode* base() JSC_FAST_CALL { return m_base.get(); }
        ExpressionNode* subscript() JSC_FAST_CALL { return m_subscript.get(); }

    private:
        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
        bool m_subscriptHasAssignments;
    };

    class DotAccessorNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        DotAccessorNode(JSGlobalData* globalData, ExpressionNode* base, const Identifier& ident) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , m_base(base)
            , m_ident(ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecMember; }

        virtual bool isLocation() const JSC_FAST_CALL { return true; }
        virtual bool isDotAccessorNode() const JSC_FAST_CALL { return true; }
        ExpressionNode* base() const JSC_FAST_CALL { return m_base.get(); }
        const Identifier& identifier() const JSC_FAST_CALL { return m_ident; }

    private:
        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
    };

    class ArgumentListNode : public Node {
    public:
        ArgumentListNode(JSGlobalData* globalData, ExpressionNode* expr) JSC_FAST_CALL
            : Node(globalData)
            , m_expr(expr)
        {
        }

        ArgumentListNode(JSGlobalData* globalData, ArgumentListNode* listNode, ExpressionNode* expr) JSC_FAST_CALL
            : Node(globalData)
            , m_expr(expr)
        {
            listNode->m_next = this;
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

        PassRefPtr<ArgumentListNode> releaseNext() JSC_FAST_CALL { return m_next.release(); }

        ListRefPtr<ArgumentListNode> m_next;
        RefPtr<ExpressionNode> m_expr;
    };

    class ArgumentsNode : public Node {
    public:
        ArgumentsNode(JSGlobalData* globalData) JSC_FAST_CALL
            : Node(globalData)
        {
        }

        ArgumentsNode(JSGlobalData* globalData, ArgumentListNode* listNode) JSC_FAST_CALL
            : Node(globalData)
            , m_listNode(listNode)
        {
        }

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

        RefPtr<ArgumentListNode> m_listNode;
    };

    class NewExprNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        NewExprNode(JSGlobalData* globalData, ExpressionNode* expr) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , m_expr(expr)
        {
        }

        NewExprNode(JSGlobalData* globalData, ExpressionNode* expr, ArgumentsNode* args) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , m_expr(expr)
            , m_args(args)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecLeftHandSide; }

    private:
        RefPtr<ExpressionNode> m_expr;
        RefPtr<ArgumentsNode> m_args;
    };

    class EvalFunctionCallNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        EvalFunctionCallNode(JSGlobalData* globalData, ArgumentsNode* args, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , ThrowableExpressionData(divot, startOffset, endOffset)
            , m_args(args)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecCall; }

    private:
        RefPtr<ArgumentsNode> m_args;
    };

    class FunctionCallValueNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        FunctionCallValueNode(JSGlobalData* globalData, ExpressionNode* expr, ArgumentsNode* args, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , ThrowableExpressionData(divot, startOffset, endOffset)
            , m_expr(expr)
            , m_args(args)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecCall; }

    private:
        RefPtr<ExpressionNode> m_expr;
        RefPtr<ArgumentsNode> m_args;
    };

    class FunctionCallResolveNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        FunctionCallResolveNode(JSGlobalData* globalData, const Identifier& ident, ArgumentsNode* args, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , ThrowableExpressionData(divot, startOffset, endOffset)
            , m_ident(ident)
            , m_args(args)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecCall; }

    protected:
        Identifier m_ident;
        RefPtr<ArgumentsNode> m_args;
        size_t m_index; // Used by LocalVarFunctionCallNode.
        size_t m_scopeDepth; // Used by ScopedVarFunctionCallNode and NonLocalVarFunctionCallNode
    };
    
    class FunctionCallBracketNode : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        FunctionCallBracketNode(JSGlobalData* globalData, ExpressionNode* base, ExpressionNode* subscript, ArgumentsNode* args, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , ThrowableSubExpressionData(divot, startOffset, endOffset)
            , m_base(base)
            , m_subscript(subscript)
            , m_args(args)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecCall; }

    protected:
        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
        RefPtr<ArgumentsNode> m_args;
    };

    class FunctionCallDotNode : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        FunctionCallDotNode(JSGlobalData* globalData, ExpressionNode* base, const Identifier& ident, ArgumentsNode* args, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , ThrowableSubExpressionData(divot, startOffset, endOffset)
            , m_base(base)
            , m_ident(ident)
            , m_args(args)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecCall; }

    private:
        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
        RefPtr<ArgumentsNode> m_args;
    };

    class PrePostResolveNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        PrePostResolveNode(JSGlobalData* globalData, const Identifier& ident, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : ExpressionNode(globalData, ResultType::constNumber()) // could be reusable for pre?
            , ThrowableExpressionData(divot, startOffset, endOffset)
            , m_ident(ident)
        {
        }

    protected:
        Identifier m_ident;
    };

    class PostfixResolveNode : public PrePostResolveNode {
    public:
        PostfixResolveNode(JSGlobalData* globalData, const Identifier& ident, Operator oper, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : PrePostResolveNode(globalData, ident, divot, startOffset, endOffset)
            , m_operator(oper)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecPostfix; }

    protected:
        Operator m_operator;
    };

    class PostfixBracketNode : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        PostfixBracketNode(JSGlobalData* globalData, ExpressionNode* base, ExpressionNode* subscript, Operator oper, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , ThrowableSubExpressionData(divot, startOffset, endOffset)
            , m_base(base)
            , m_subscript(subscript)
            , m_operator(oper)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecPostfix; }

    protected:
        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
        Operator m_operator;
    };

    class PostfixDotNode : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        PostfixDotNode(JSGlobalData* globalData, ExpressionNode* base, const Identifier& ident, Operator oper, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , ThrowableSubExpressionData(divot, startOffset, endOffset)
            , m_base(base)
            , m_ident(ident)
            , m_operator(oper)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecPostfix; }

    protected:
        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
        Operator m_operator;
    };

    class PostfixErrorNode : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        PostfixErrorNode(JSGlobalData* globalData, ExpressionNode* expr, Operator oper, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , ThrowableSubExpressionData(divot, startOffset, endOffset)
            , m_expr(expr)
            , m_operator(oper)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecPostfix; }

    private:
        RefPtr<ExpressionNode> m_expr;
        Operator m_operator;
    };

    class DeleteResolveNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        DeleteResolveNode(JSGlobalData* globalData, const Identifier& ident, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , ThrowableExpressionData(divot, startOffset, endOffset)
            , m_ident(ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        Identifier m_ident;
    };

    class DeleteBracketNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        DeleteBracketNode(JSGlobalData* globalData, ExpressionNode* base, ExpressionNode* subscript, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , ThrowableExpressionData(divot, startOffset, endOffset)
            , m_base(base)
            , m_subscript(subscript)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
    };

    class DeleteDotNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        DeleteDotNode(JSGlobalData* globalData, ExpressionNode* base, const Identifier& ident, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , ThrowableExpressionData(divot, startOffset, endOffset)
            , m_base(base)
            , m_ident(ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
    };

    class DeleteValueNode : public ExpressionNode {
    public:
        DeleteValueNode(JSGlobalData* globalData, ExpressionNode* expr) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        RefPtr<ExpressionNode> m_expr;
    };

    class VoidNode : public ExpressionNode {
    public:
        VoidNode(JSGlobalData* globalData, ExpressionNode* expr) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        RefPtr<ExpressionNode> m_expr;
    };

    class TypeOfResolveNode : public ExpressionNode {
    public:
        TypeOfResolveNode(JSGlobalData* globalData, const Identifier& ident) JSC_FAST_CALL
            : ExpressionNode(globalData, ResultType::string())
            , m_ident(ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

        const Identifier& identifier() const JSC_FAST_CALL { return m_ident; }

    protected:
        Identifier m_ident;
        size_t m_index; // Used by LocalTypeOfNode.
    };

    class TypeOfValueNode : public ExpressionNode {
    public:
        TypeOfValueNode(JSGlobalData* globalData, ExpressionNode* expr) JSC_FAST_CALL
            : ExpressionNode(globalData, ResultType::string())
            , m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        RefPtr<ExpressionNode> m_expr;
    };

    class PrefixResolveNode : public PrePostResolveNode {
    public:
        PrefixResolveNode(JSGlobalData* globalData, const Identifier& ident, Operator oper, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : PrePostResolveNode(globalData, ident, divot, startOffset, endOffset)
            , m_operator(oper)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    protected:
        Operator m_operator;
    };

    class PrefixBracketNode : public ExpressionNode, public ThrowablePrefixedSubExpressionData {
    public:
        PrefixBracketNode(JSGlobalData* globalData, ExpressionNode* base, ExpressionNode* subscript, Operator oper, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , ThrowablePrefixedSubExpressionData(divot, startOffset, endOffset)
            , m_base(base)
            , m_subscript(subscript)
            , m_operator(oper)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    protected:
        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
        Operator m_operator;
    };

    class PrefixDotNode : public ExpressionNode, public ThrowablePrefixedSubExpressionData {
    public:
        PrefixDotNode(JSGlobalData* globalData, ExpressionNode* base, const Identifier& ident, Operator oper, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , ThrowablePrefixedSubExpressionData(divot, startOffset, endOffset)
            , m_base(base)
            , m_ident(ident)
            , m_operator(oper)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecPostfix; }

    protected:
        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
        Operator m_operator;
    };

    class PrefixErrorNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        PrefixErrorNode(JSGlobalData* globalData, ExpressionNode* expr, Operator oper, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , ThrowableExpressionData(divot, startOffset, endOffset)
            , m_expr(expr)
            , m_operator(oper)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        RefPtr<ExpressionNode> m_expr;
        Operator m_operator;
    };

    class UnaryOpNode : public ExpressionNode {
    public:
        UnaryOpNode(JSGlobalData* globalData, ExpressionNode* expr)
            : ExpressionNode(globalData)
            , m_expr(expr)
        {
        }

        UnaryOpNode(JSGlobalData* globalData, ResultType type, ExpressionNode* expr)
            : ExpressionNode(globalData, type)
            , m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual OpcodeID opcode() const JSC_FAST_CALL = 0;

    protected:
        RefPtr<ExpressionNode> m_expr;
    };

    class UnaryPlusNode : public UnaryOpNode {
    public:
        UnaryPlusNode(JSGlobalData* globalData, ExpressionNode* expr) JSC_FAST_CALL
            : UnaryOpNode(globalData, ResultType::constNumber(), expr)
        {
        }

        virtual ExpressionNode* stripUnaryPlus() { return m_expr.get(); }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_to_jsnumber; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }
    };

    class NegateNode : public UnaryOpNode {
    public:
        NegateNode(JSGlobalData* globalData, ExpressionNode* expr) JSC_FAST_CALL
            : UnaryOpNode(globalData, ResultType::reusableNumber(), expr)
        {
        }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_negate; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }
    };

    class BitwiseNotNode : public UnaryOpNode {
    public:
        BitwiseNotNode(JSGlobalData* globalData, ExpressionNode* expr) JSC_FAST_CALL
            : UnaryOpNode(globalData, ResultType::reusableNumber(), expr)
        {
        }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_bitnot; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }
    };

    class LogicalNotNode : public UnaryOpNode {
    public:
        LogicalNotNode(JSGlobalData* globalData, ExpressionNode* expr) JSC_FAST_CALL
            : UnaryOpNode(globalData, ResultType::boolean(), expr)
        {
        }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_not; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }
    };

    class BinaryOpNode : public ExpressionNode {
    public:
        BinaryOpNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
            : ExpressionNode(globalData)
            , m_expr1(expr1)
            , m_expr2(expr2)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        BinaryOpNode(JSGlobalData* globalData, ResultType type, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
            : ExpressionNode(globalData, type)
            , m_expr1(expr1)
            , m_expr2(expr2)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual OpcodeID opcode() const JSC_FAST_CALL = 0;

    protected:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
        bool m_rightHasAssignments;
    };

    class ReverseBinaryOpNode : public ExpressionNode {
    public:
        ReverseBinaryOpNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
            : ExpressionNode(globalData)
            , m_expr1(expr1)
            , m_expr2(expr2)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        ReverseBinaryOpNode(JSGlobalData* globalData, ResultType type, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
            : ExpressionNode(globalData, type)
            , m_expr1(expr1)
            , m_expr2(expr2)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual OpcodeID opcode() const JSC_FAST_CALL = 0;

    protected:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
        bool m_rightHasAssignments;
    };

    class MultNode : public BinaryOpNode {
    public:
        MultNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : BinaryOpNode(globalData, ResultType::reusableNumber(), expr1, expr2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_mul; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecMultiplicative; }
    };

    class DivNode : public BinaryOpNode {
    public:
        DivNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : BinaryOpNode(globalData, ResultType::reusableNumber(), expr1, expr2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_div; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecMultiplicative; }
    };

    class ModNode : public BinaryOpNode {
    public:
        ModNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : BinaryOpNode(globalData, ResultType::reusableNumber(), expr1, expr2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_mod; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecMultiplicative; }
    };

    class AddNode : public BinaryOpNode {
    public:
        AddNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : BinaryOpNode(globalData, ResultType::forAdd(expr1->resultDescriptor(), expr2->resultDescriptor()), expr1, expr2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_add; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecAdditive; }
    };

    class SubNode : public BinaryOpNode {
    public:
        SubNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : BinaryOpNode(globalData, ResultType::reusableNumber(), expr1, expr2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_sub; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecAdditive; }
    };

    class LeftShiftNode : public BinaryOpNode {
    public:
        LeftShiftNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : BinaryOpNode(globalData, ResultType::reusableNumber(), expr1, expr2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_lshift; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecShift; }
    };

    class RightShiftNode : public BinaryOpNode {
    public:
        RightShiftNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : BinaryOpNode(globalData, ResultType::reusableNumber(), expr1, expr2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_rshift; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecShift; }
    };

    class UnsignedRightShiftNode : public BinaryOpNode {
    public:
        UnsignedRightShiftNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : BinaryOpNode(globalData, ResultType::reusableNumber(), expr1, expr2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_urshift; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecShift; }
    };

    class LessNode : public BinaryOpNode {
    public:
        LessNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : BinaryOpNode(globalData, ResultType::boolean(), expr1, expr2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_less; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecRelational; }
    };

    class GreaterNode : public ReverseBinaryOpNode {
    public:
        GreaterNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : ReverseBinaryOpNode(globalData, ResultType::boolean(), expr1, expr2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_less; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecRelational; }
    };

    class LessEqNode : public BinaryOpNode {
    public:
        LessEqNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : BinaryOpNode(globalData, ResultType::boolean(), expr1, expr2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_lesseq; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecRelational; }
    };

    class GreaterEqNode : public ReverseBinaryOpNode {
    public:
        GreaterEqNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : ReverseBinaryOpNode(globalData, ResultType::boolean(), expr1, expr2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_lesseq; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecRelational; }
    };

    class ThrowableBinaryOpNode : public BinaryOpNode, public ThrowableExpressionData {
    public:
        ThrowableBinaryOpNode(JSGlobalData* globalData, ResultType type, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : BinaryOpNode(globalData, type, expr1, expr2, rightHasAssignments)
        {
        }
        ThrowableBinaryOpNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : BinaryOpNode(globalData, expr1, expr2, rightHasAssignments)
        {
        }
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
    };
    
    class InstanceOfNode : public ThrowableBinaryOpNode {
    public:
        InstanceOfNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : ThrowableBinaryOpNode(globalData, ResultType::boolean(), expr1, expr2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_instanceof; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecRelational; }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
    };

    class InNode : public ThrowableBinaryOpNode {
    public:
        InNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : ThrowableBinaryOpNode(globalData, expr1, expr2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_in; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecRelational; }
    };

    class EqualNode : public BinaryOpNode {
    public:
        EqualNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : BinaryOpNode(globalData, ResultType::boolean(), expr1, expr2, rightHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_eq; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecEquality; }
    };

    class NotEqualNode : public BinaryOpNode {
    public:
        NotEqualNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : BinaryOpNode(globalData, ResultType::boolean(), expr1, expr2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_neq; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecEquality; }
    };

    class StrictEqualNode : public BinaryOpNode {
    public:
        StrictEqualNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : BinaryOpNode(globalData, ResultType::boolean(), expr1, expr2, rightHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_stricteq; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecEquality; }
    };

    class NotStrictEqualNode : public BinaryOpNode {
    public:
        NotStrictEqualNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : BinaryOpNode(globalData, ResultType::boolean(), expr1, expr2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_nstricteq; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecEquality; }
    };

    class BitAndNode : public BinaryOpNode {
    public:
        BitAndNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : BinaryOpNode(globalData, ResultType::reusableNumber(), expr1, expr2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_bitand; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecBitwiseAnd; }
    };

    class BitOrNode : public BinaryOpNode {
    public:
        BitOrNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : BinaryOpNode(globalData, ResultType::reusableNumber(), expr1, expr2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_bitor; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecBitwiseOr; }
    };

    class BitXOrNode : public BinaryOpNode {
    public:
        BitXOrNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments) JSC_FAST_CALL
            : BinaryOpNode(globalData, ResultType::reusableNumber(), expr1, expr2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const JSC_FAST_CALL { return op_bitxor; }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecBitwiseXor; }
    };

    /**
     * m_expr1 && m_expr2, m_expr1 || m_expr2
     */
    class LogicalOpNode : public ExpressionNode {
    public:
        LogicalOpNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, LogicalOperator oper) JSC_FAST_CALL
            : ExpressionNode(globalData, ResultType::boolean())
            , m_expr1(expr1)
            , m_expr2(expr2)
            , m_operator(oper)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return (m_operator == OpLogicalAnd) ? PrecLogicalAnd : PrecLogicalOr; }

    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
        LogicalOperator m_operator;
    };

    /**
     * The ternary operator, "m_logical ? m_expr1 : m_expr2"
     */
    class ConditionalNode : public ExpressionNode {
    public:
        ConditionalNode(JSGlobalData* globalData, ExpressionNode* logical, ExpressionNode* expr1, ExpressionNode* expr2) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , m_logical(logical)
            , m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecConditional; }

    private:
        RefPtr<ExpressionNode> m_logical;
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };

    class ReadModifyResolveNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        ReadModifyResolveNode(JSGlobalData* globalData, const Identifier& ident, Operator oper, ExpressionNode*  right, bool rightHasAssignments, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , ThrowableExpressionData(divot, startOffset, endOffset)
            , m_ident(ident)
            , m_right(right)
            , m_operator(oper)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecAssignment; }

    protected:
        Identifier m_ident;
        RefPtr<ExpressionNode> m_right;
        size_t m_index; // Used by ReadModifyLocalVarNode.
        Operator m_operator : 31;
        bool m_rightHasAssignments : 1;
    };

    class AssignResolveNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        AssignResolveNode(JSGlobalData* globalData, const Identifier& ident, ExpressionNode* right, bool rightHasAssignments) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , m_ident(ident)
            , m_right(right)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecAssignment; }

    protected:
        Identifier m_ident;
        RefPtr<ExpressionNode> m_right;
        size_t m_index; // Used by ReadModifyLocalVarNode.
        bool m_rightHasAssignments;
    };

    class ReadModifyBracketNode : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        ReadModifyBracketNode(JSGlobalData* globalData, ExpressionNode* base, ExpressionNode* subscript, Operator oper, ExpressionNode* right, bool subscriptHasAssignments, bool rightHasAssignments, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , ThrowableSubExpressionData(divot, startOffset, endOffset)
            , m_base(base)
            , m_subscript(subscript)
            , m_right(right)
            , m_operator(oper)
            , m_subscriptHasAssignments(subscriptHasAssignments)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecAssignment; }

    protected:
        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
        RefPtr<ExpressionNode> m_right;
        Operator m_operator : 30;
        bool m_subscriptHasAssignments : 1;
        bool m_rightHasAssignments : 1;
    };

    class AssignBracketNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        AssignBracketNode(JSGlobalData* globalData, ExpressionNode* base, ExpressionNode* subscript, ExpressionNode* right, bool subscriptHasAssignments, bool rightHasAssignments, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , ThrowableExpressionData(divot, startOffset, endOffset)
            , m_base(base)
            , m_subscript(subscript)
            , m_right(right)
            , m_subscriptHasAssignments(subscriptHasAssignments)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecAssignment; }

    protected:
        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
        RefPtr<ExpressionNode> m_right;
        bool m_subscriptHasAssignments : 1;
        bool m_rightHasAssignments : 1;
    };

    class AssignDotNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        AssignDotNode(JSGlobalData* globalData, ExpressionNode* base, const Identifier& ident, ExpressionNode* right, bool rightHasAssignments, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , ThrowableExpressionData(divot, startOffset, endOffset)
            , m_base(base)
            , m_ident(ident)
            , m_right(right)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecAssignment; }

    protected:
        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
        RefPtr<ExpressionNode> m_right;
        bool m_rightHasAssignments;
    };

    class ReadModifyDotNode : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        ReadModifyDotNode(JSGlobalData* globalData, ExpressionNode* base, const Identifier& ident, Operator oper, ExpressionNode* right, bool rightHasAssignments, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , ThrowableSubExpressionData(divot, startOffset, endOffset)
            , m_base(base)
            , m_ident(ident)
            , m_right(right)
            , m_operator(oper)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecAssignment; }

    protected:
        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
        RefPtr<ExpressionNode> m_right;
        Operator m_operator : 31;
        bool m_rightHasAssignments : 1;
    };

    class AssignErrorNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        AssignErrorNode(JSGlobalData* globalData, ExpressionNode* left, Operator oper, ExpressionNode* right, unsigned divot, unsigned startOffset, unsigned endOffset) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , ThrowableExpressionData(divot, startOffset, endOffset)
            , m_left(left)
            , m_operator(oper)
            , m_right(right)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecAssignment; }

    protected:
        RefPtr<ExpressionNode> m_left;
        Operator m_operator;
        RefPtr<ExpressionNode> m_right;
    };

    class CommaNode : public ExpressionNode {
    public:
        CommaNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecExpression; }

    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };
    
    class VarDeclCommaNode : public CommaNode {
    public:
        VarDeclCommaNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2) JSC_FAST_CALL
            : CommaNode(globalData, expr1, expr2)
        {
        }
        virtual Precedence precedence() const { return PrecAssignment; }
    };

    class ConstDeclNode : public ExpressionNode {
    public:
        ConstDeclNode(JSGlobalData* globalData, const Identifier& ident, ExpressionNode* in) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
        PassRefPtr<ConstDeclNode> releaseNext() JSC_FAST_CALL { return m_next.release(); }

        Identifier m_ident;
        ListRefPtr<ConstDeclNode> m_next;
        RefPtr<ExpressionNode> m_init;
        
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual RegisterID* emitCodeSingle(CodeGenerator&) JSC_FAST_CALL;
    };

    class ConstStatementNode : public StatementNode {
    public:
        ConstStatementNode(JSGlobalData* globalData, ConstDeclNode* next) JSC_FAST_CALL
            : StatementNode(globalData)
            , m_next(next)
        {
        }

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

    private:
        RefPtr<ConstDeclNode> m_next;
    };

    typedef Vector<RefPtr<StatementNode> > StatementVector;

    class SourceElements : public ParserRefCounted {
    public:
        SourceElements(JSGlobalData* globalData) : ParserRefCounted(globalData) {}

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
        BlockNode(JSGlobalData*, SourceElements* children) JSC_FAST_CALL;

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;

        StatementVector& children() { return m_children; }

        virtual bool isBlock() const JSC_FAST_CALL { return true; }
    protected:
        StatementVector m_children;
    };

    class EmptyStatementNode : public StatementNode {
    public:
        EmptyStatementNode(JSGlobalData* globalData) JSC_FAST_CALL // debug
            : StatementNode(globalData)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual bool isEmptyStatement() const JSC_FAST_CALL { return true; }
    };
    
    class DebuggerStatementNode : public StatementNode {
    public:
        DebuggerStatementNode(JSGlobalData* globalData) JSC_FAST_CALL
            : StatementNode(globalData)
        {
        }
        
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
    };

    class ExprStatementNode : public StatementNode {
    public:
        ExprStatementNode(JSGlobalData* globalData, ExpressionNode* expr) JSC_FAST_CALL
            : StatementNode(globalData)
            , m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;

    private:
        RefPtr<ExpressionNode> m_expr;
    };

    class VarStatementNode : public StatementNode {
    public:
        VarStatementNode(JSGlobalData* globalData, ExpressionNode* expr) JSC_FAST_CALL
            : StatementNode(globalData)
            , m_expr(expr)
        {
        }
        
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;

    private:
        RefPtr<ExpressionNode> m_expr;
    };

    class IfNode : public StatementNode {
    public:
        IfNode(JSGlobalData* globalData, ExpressionNode* condition, StatementNode* ifBlock) JSC_FAST_CALL
            : StatementNode(globalData)
            , m_condition(condition)
            , m_ifBlock(ifBlock)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;

    protected:
        RefPtr<ExpressionNode> m_condition;
        RefPtr<StatementNode> m_ifBlock;
    };

    class IfElseNode : public IfNode {
    public:
        IfElseNode(JSGlobalData* globalData, ExpressionNode* condition, StatementNode* ifBlock, StatementNode* elseBlock) JSC_FAST_CALL
            : IfNode(globalData, condition, ifBlock)
            , m_elseBlock(elseBlock)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;

    private:
        RefPtr<StatementNode> m_elseBlock;
    };

    class DoWhileNode : public StatementNode {
    public:
        DoWhileNode(JSGlobalData* globalData, StatementNode* statement, ExpressionNode* expr) JSC_FAST_CALL
            : StatementNode(globalData)
            , m_statement(statement)
            , m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;

        virtual bool isLoop() const JSC_FAST_CALL { return true; }
    private:
        RefPtr<StatementNode> m_statement;
        RefPtr<ExpressionNode> m_expr;
    };

    class WhileNode : public StatementNode {
    public:
        WhileNode(JSGlobalData* globalData, ExpressionNode* expr, StatementNode* statement) JSC_FAST_CALL
            : StatementNode(globalData)
            , m_expr(expr)
            , m_statement(statement)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;

        virtual bool isLoop() const JSC_FAST_CALL { return true; }
    private:
        RefPtr<ExpressionNode> m_expr;
        RefPtr<StatementNode> m_statement;
    };

    class ForNode : public StatementNode {
    public:
        ForNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, ExpressionNode* expr3, StatementNode* statement, bool expr1WasVarDecl) JSC_FAST_CALL
            : StatementNode(globalData)
            , m_expr1(expr1)
            , m_expr2(expr2)
            , m_expr3(expr3)
            , m_statement(statement)
            , m_expr1WasVarDecl(expr1 && expr1WasVarDecl)
        {
            ASSERT(statement);
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;

        virtual bool isLoop() const JSC_FAST_CALL { return true; }
    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
        RefPtr<ExpressionNode> m_expr3;
        RefPtr<StatementNode> m_statement;
        bool m_expr1WasVarDecl;
    };

    class ForInNode : public StatementNode, public ThrowableExpressionData {
    public:
        ForInNode(JSGlobalData*, ExpressionNode*, ExpressionNode*, StatementNode*) JSC_FAST_CALL;
        ForInNode(JSGlobalData*, const Identifier&, ExpressionNode*, ExpressionNode*, StatementNode*, int divot, int startOffset, int endOffset) JSC_FAST_CALL;
        
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;

        virtual bool isLoop() const JSC_FAST_CALL { return true; }
    private:
        Identifier m_ident;
        RefPtr<ExpressionNode> m_init;
        RefPtr<ExpressionNode> m_lexpr;
        RefPtr<ExpressionNode> m_expr;
        RefPtr<StatementNode> m_statement;
        bool m_identIsVarDecl;
    };

    class ContinueNode : public StatementNode, public ThrowableExpressionData {
    public:
        ContinueNode(JSGlobalData* globalData) JSC_FAST_CALL
            : StatementNode(globalData)
        {
        }

        ContinueNode(JSGlobalData* globalData, const Identifier& ident) JSC_FAST_CALL
            : StatementNode(globalData)
            , m_ident(ident)
        {
        }
        
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;

    private:
        Identifier m_ident;
    };

    class BreakNode : public StatementNode, public ThrowableExpressionData {
    public:
        BreakNode(JSGlobalData* globalData) JSC_FAST_CALL
            : StatementNode(globalData)
        {
        }

        BreakNode(JSGlobalData* globalData, const Identifier& ident) JSC_FAST_CALL
            : StatementNode(globalData)
            , m_ident(ident)
        {
        }
        
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;

    private:
        Identifier m_ident;
    };

    class ReturnNode : public StatementNode, public ThrowableExpressionData {
    public:
        ReturnNode(JSGlobalData* globalData, ExpressionNode* value) JSC_FAST_CALL
            : StatementNode(globalData)
            , m_value(value)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual bool isReturnNode() const JSC_FAST_CALL { return true; }

    private:
        RefPtr<ExpressionNode> m_value;
    };

    class WithNode : public StatementNode {
    public:
        WithNode(JSGlobalData* globalData, ExpressionNode* expr, StatementNode* statement, uint32_t divot, uint32_t expressionLength) JSC_FAST_CALL
            : StatementNode(globalData)
            , m_expr(expr)
            , m_statement(statement)
            , m_divot(divot)
            , m_expressionLength(expressionLength)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;

    private:
        RefPtr<ExpressionNode> m_expr;
        RefPtr<StatementNode> m_statement;
        uint32_t m_divot;
        uint32_t m_expressionLength;
    };

    class LabelNode : public StatementNode, public ThrowableExpressionData {
    public:
        LabelNode(JSGlobalData* globalData, const Identifier& name, StatementNode* statement) JSC_FAST_CALL
            : StatementNode(globalData)
            , m_name(name)
            , m_statement(statement)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;

    private:
        Identifier m_name;
        RefPtr<StatementNode> m_statement;
    };

    class ThrowNode : public StatementNode, public ThrowableExpressionData {
    public:
        ThrowNode(JSGlobalData* globalData, ExpressionNode* expr) JSC_FAST_CALL
            : StatementNode(globalData)
            , m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;

    private:
        RefPtr<ExpressionNode> m_expr;
    };

    class TryNode : public StatementNode {
    public:
        TryNode(JSGlobalData* globalData, StatementNode* tryBlock, const Identifier& exceptionIdent, StatementNode* catchBlock, StatementNode* finallyBlock) JSC_FAST_CALL
            : StatementNode(globalData)
            , m_tryBlock(tryBlock)
            , m_exceptionIdent(exceptionIdent)
            , m_catchBlock(catchBlock)
            , m_finallyBlock(finallyBlock)
        {
        }

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* dst = 0) JSC_FAST_CALL;

    private:
        RefPtr<StatementNode> m_tryBlock;
        Identifier m_exceptionIdent;
        RefPtr<StatementNode> m_catchBlock;
        RefPtr<StatementNode> m_finallyBlock;
    };

    class ParameterNode : public Node {
    public:
        ParameterNode(JSGlobalData* globalData, const Identifier& ident) JSC_FAST_CALL
            : Node(globalData)
            , m_ident(ident)
        {
        }

        ParameterNode(JSGlobalData* globalData, ParameterNode* l, const Identifier& ident) JSC_FAST_CALL
            : Node(globalData)
            , m_ident(ident)
        {
            l->m_next = this;
        }

        Identifier ident() JSC_FAST_CALL { return m_ident; }
        ParameterNode *nextParam() JSC_FAST_CALL { return m_next.get(); }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        PassRefPtr<ParameterNode> releaseNext() JSC_FAST_CALL { return m_next.release(); }
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

    private:
        friend class FuncDeclNode;
        friend class FuncExprNode;
        Identifier m_ident;
        ListRefPtr<ParameterNode> m_next;
    };

    class ScopeNode : public BlockNode {
    public:
        ScopeNode(JSGlobalData*, const SourceCode&, SourceElements*, VarStack*, FunctionStack*, CodeFeatures, int numConstants) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        
        const SourceCode& source() const { return m_source; }
        const UString& sourceURL() const JSC_FAST_CALL { return m_source.provider()->url(); }
        intptr_t sourceID() const { return m_source.provider()->asID(); }

        bool usesEval() const { return m_features & EvalFeature; }
        bool usesArguments() const { return m_features & ArgumentsFeature; }
        void setUsesArguments() { m_features |= ArgumentsFeature; }
        bool usesThis() const { return m_features & ThisFeature; }
        bool needsActivation() const { return m_features & (EvalFeature | ClosureFeature | WithFeature | CatchFeature); }

        VarStack& varStack() { return m_varStack; }
        FunctionStack& functionStack() { return m_functionStack; }

        int neededConstants()
        {
            // We may need 1 more constant than the count given by the parser,
            // because of the various uses of jsUndefined().
            return m_numConstants + 1;
        }

    protected:
        void setSource(const SourceCode& source) { m_source = source; }

        VarStack m_varStack;
        FunctionStack m_functionStack;

    private:
        SourceCode m_source;
        CodeFeatures m_features;
        int m_numConstants;
    };

    class ProgramNode : public ScopeNode {
    public:
        static ProgramNode* create(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, const SourceCode&, CodeFeatures, int numConstants) JSC_FAST_CALL;

        ProgramCodeBlock& byteCode(ScopeChainNode* scopeChain) JSC_FAST_CALL
        {
            if (!m_code)
                generateCode(scopeChain);
            return *m_code;
        }

    private:
        ProgramNode(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, const SourceCode&, CodeFeatures, int numConstants) JSC_FAST_CALL;

        void generateCode(ScopeChainNode*) JSC_FAST_CALL;
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        Vector<size_t> m_varIndexes; // Storage indexes belonging to the nodes in m_varStack. (Recorded to avoid double lookup.)
        Vector<size_t> m_functionIndexes; // Storage indexes belonging to the nodes in m_functionStack. (Recorded to avoid double lookup.)

        OwnPtr<ProgramCodeBlock> m_code;
    };

    class EvalNode : public ScopeNode {
    public:
        static EvalNode* create(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, const SourceCode&, CodeFeatures, int numConstants) JSC_FAST_CALL;

        EvalCodeBlock& byteCode(ScopeChainNode* scopeChain) JSC_FAST_CALL
        {
            if (!m_code)
                generateCode(scopeChain);
            return *m_code;
        }

    private:
        EvalNode(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, const SourceCode&, CodeFeatures, int numConstants) JSC_FAST_CALL;

        void generateCode(ScopeChainNode*) JSC_FAST_CALL;
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        
        OwnPtr<EvalCodeBlock> m_code;
    };

    class FunctionBodyNode : public ScopeNode {
    public:
        static FunctionBodyNode* create(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, const SourceCode&, CodeFeatures, int numConstants) JSC_FAST_CALL;
        static FunctionBodyNode* create(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, CodeFeatures, int numConstants) JSC_FAST_CALL;
        ~FunctionBodyNode();

        const Identifier* parameters() const JSC_FAST_CALL { return m_parameters; }
        size_t parameterCount() const { return m_parameterCount; }
        UString paramString() const JSC_FAST_CALL;
        Identifier* copyParameters();

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        
        SymbolTable& symbolTable() { return m_symbolTable; } // FIXME: Remove this
        
        CodeBlock& byteCode(ScopeChainNode* scopeChain) JSC_FAST_CALL
        {
            ASSERT(scopeChain);
            if (!m_code)
                generateCode(scopeChain);
            return *m_code;
        }

        CodeBlock& generatedByteCode() JSC_FAST_CALL
        {
            ASSERT(m_code);
            return *m_code;
        }

        bool isGenerated() JSC_FAST_CALL
        {
            return m_code;
        }

        void mark();

        void finishParsing(const SourceCode&, ParameterNode*);
        void finishParsing(Identifier* parameters, size_t parameterCount);
        
        UString toSourceString() const JSC_FAST_CALL { return UString("{") + source().toString() + UString("}"); }

        // These objects are ref/deref'd a lot in the scope chain, so this is a faster ref/deref.
        // If the virtual machine changes so this doesn't happen as much we can change back.
        void ref()
        {
            if (++m_refCount == 1)
                ScopeNode::ref();
        }
        void deref()
        {
            ASSERT(m_refCount);
            if (!--m_refCount)
                ScopeNode::deref();
        }

    protected:
        FunctionBodyNode(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, const SourceCode&, CodeFeatures, int numConstants) JSC_FAST_CALL;

    private:
        void generateCode(ScopeChainNode*) JSC_FAST_CALL;

        Identifier* m_parameters;
        size_t m_parameterCount;
        SymbolTable m_symbolTable;
        OwnPtr<CodeBlock> m_code;
        unsigned m_refCount;
    };

    class FuncExprNode : public ExpressionNode {
    public:
        FuncExprNode(JSGlobalData* globalData, const Identifier& ident, FunctionBodyNode* body, const SourceCode& source, ParameterNode* parameter = 0) JSC_FAST_CALL
            : ExpressionNode(globalData)
            , m_ident(ident)
            , m_parameter(parameter)
            , m_body(body)
        {
            m_body->finishParsing(source, m_parameter.get());
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        JSFunction* makeFunction(ExecState*, ScopeChainNode*) JSC_FAST_CALL;
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { return PrecMember; }
        virtual bool needsParensIfLeftmost() const { return true; }

        FunctionBodyNode* body() { return m_body.get(); }

    private:
        // Used for streamTo
        friend class PropertyNode;
        Identifier m_ident;
        RefPtr<ParameterNode> m_parameter;
        RefPtr<FunctionBodyNode> m_body;
    };

    class FuncDeclNode : public StatementNode {
    public:
        FuncDeclNode(JSGlobalData* globalData, const Identifier& ident, FunctionBodyNode* body, const SourceCode& source, ParameterNode* parameter = 0) JSC_FAST_CALL
            : StatementNode(globalData)
            , m_ident(ident)
            , m_parameter(parameter)
            , m_body(body)
        {
            m_body->finishParsing(source, m_parameter.get());
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        JSFunction* makeFunction(ExecState*, ScopeChainNode*) JSC_FAST_CALL;

        Identifier m_ident;

        FunctionBodyNode* body() { return m_body.get(); }

    private:
        RefPtr<ParameterNode> m_parameter;
        RefPtr<FunctionBodyNode> m_body;
    };

    class CaseClauseNode : public Node {
    public:
        CaseClauseNode(JSGlobalData* globalData, ExpressionNode* expr) JSC_FAST_CALL
            : Node(globalData)
            , m_expr(expr)
        {
        }

        CaseClauseNode(JSGlobalData* globalData, ExpressionNode* expr, SourceElements* children) JSC_FAST_CALL
            : Node(globalData)
            , m_expr(expr)
        {
            if (children)
                children->releaseContentsIntoVector(m_children);
        }

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

        ExpressionNode* expr() const { return m_expr.get(); }
        StatementVector& children() { return m_children; }

    private:
        RefPtr<ExpressionNode> m_expr;
        StatementVector m_children;
    };

    class ClauseListNode : public Node {
    public:
        ClauseListNode(JSGlobalData* globalData, CaseClauseNode* clause) JSC_FAST_CALL
            : Node(globalData)
            , m_clause(clause)
        {
        }

        ClauseListNode(JSGlobalData* globalData, ClauseListNode* clauseList, CaseClauseNode* clause) JSC_FAST_CALL
            : Node(globalData)
            , m_clause(clause)
        {
            clauseList->m_next = this;
        }

        CaseClauseNode* getClause() const JSC_FAST_CALL { return m_clause.get(); }
        ClauseListNode* getNext() const JSC_FAST_CALL { return m_next.get(); }
        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        PassRefPtr<ClauseListNode> releaseNext() JSC_FAST_CALL { return m_next.release(); }
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

    private:
        friend class CaseBlockNode;
        RefPtr<CaseClauseNode> m_clause;
        ListRefPtr<ClauseListNode> m_next;
    };

    class CaseBlockNode : public Node {
    public:
        CaseBlockNode(JSGlobalData* globalData, ClauseListNode* list1, CaseClauseNode* defaultClause, ClauseListNode* list2) JSC_FAST_CALL
            : Node(globalData)
            , m_list1(list1)
            , m_defaultClause(defaultClause)
            , m_list2(list2)
        {
        }

        RegisterID* emitCodeForBlock(CodeGenerator&, RegisterID* input, RegisterID* dst = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

    private:
        SwitchInfo::SwitchType tryOptimizedSwitch(Vector<ExpressionNode*, 8>& literalVector, int32_t& min_num, int32_t& max_num);
        RefPtr<ClauseListNode> m_list1;
        RefPtr<CaseClauseNode> m_defaultClause;
        RefPtr<ClauseListNode> m_list2;
    };

    class SwitchNode : public StatementNode {
    public:
        SwitchNode(JSGlobalData* globalData, ExpressionNode* expr, CaseBlockNode* block) JSC_FAST_CALL
            : StatementNode(globalData)
            , m_expr(expr)
            , m_block(block)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual void streamTo(SourceStream&) const JSC_FAST_CALL;

    private:
        RefPtr<ExpressionNode> m_expr;
        RefPtr<CaseBlockNode> m_block;
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

} // namespace JSC

#endif // NODES_H_
