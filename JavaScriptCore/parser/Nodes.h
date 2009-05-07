/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef Nodes_h
#define Nodes_h

#include "Error.h"
#include "JITCode.h"
#include "Opcode.h"
#include "ResultType.h"
#include "SourceCode.h"
#include "SymbolTable.h"
#include <wtf/MathExtras.h>
#include <wtf/OwnPtr.h>

#if PLATFORM(X86) && COMPILER(GCC)
#define JSC_FAST_CALL __attribute__((regparm(3)))
#else
#define JSC_FAST_CALL
#endif

namespace JSC {

    class ArgumentListNode;
    class CodeBlock;
    class BytecodeGenerator;
    class FuncDeclNode;
    class EvalCodeBlock;
    class JSFunction;
    class NodeReleaser;
    class ProgramCodeBlock;
    class PropertyListNode;
    class RegisterID;
    class ScopeChainNode;

    typedef unsigned CodeFeatures;

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

    namespace DeclarationStacks {
        enum VarAttrs { IsConstant = 1, HasInitializer = 2 };
        typedef Vector<std::pair<Identifier, unsigned> > VarStack;
        typedef Vector<RefPtr<FuncDeclNode> > FunctionStack;
    }

    struct SwitchInfo {
        enum SwitchType { SwitchNone, SwitchImmediate, SwitchCharacter, SwitchString };
        uint32_t bytecodeOffset;
        SwitchType switchType;
    };

    class ParserRefCounted : public RefCounted<ParserRefCounted> {
    protected:
        ParserRefCounted(JSGlobalData*);

    public:
        virtual ~ParserRefCounted();

        // Nonrecursive destruction.
        virtual void releaseNodes(NodeReleaser&);
    };

#ifdef NDEBUG
    inline ParserRefCounted::~ParserRefCounted()
    {
    }
#endif

    class Node : public ParserRefCounted {
    public:
        Node(JSGlobalData*);

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
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* dst = 0) JSC_FAST_CALL = 0;

        int lineNo() const { return m_line; }

    protected:
        int m_line;
    };

    class ExpressionNode : public Node {
    public:
        ExpressionNode(JSGlobalData*, ResultType = ResultType::unknownType());

        virtual bool isNumber() const JSC_FAST_CALL { return false; }
        virtual bool isString() const JSC_FAST_CALL { return false; }
        virtual bool isNull() const JSC_FAST_CALL { return false; }
        virtual bool isPure(BytecodeGenerator&) const JSC_FAST_CALL { return false; }        
        virtual bool isLocation() const JSC_FAST_CALL { return false; }
        virtual bool isResolveNode() const JSC_FAST_CALL { return false; }
        virtual bool isBracketAccessorNode() const JSC_FAST_CALL { return false; }
        virtual bool isDotAccessorNode() const JSC_FAST_CALL { return false; }
        virtual bool isFuncExprNode() const JSC_FAST_CALL { return false; } 
        virtual bool isSimpleArray() const JSC_FAST_CALL { return false; }
        virtual bool isAdd() const JSC_FAST_CALL { return false; }

        virtual ExpressionNode* stripUnaryPlus() { return this; }

        ResultType resultDescriptor() const JSC_FAST_CALL { return m_resultType; }

        // This needs to be in public in order to compile using GCC 3.x 
        typedef enum { EvalOperator, FunctionCall } CallerType;

    private:
        ResultType m_resultType;
    };

    class StatementNode : public Node {
    public:
        StatementNode(JSGlobalData*);

        void setLoc(int line0, int line1) JSC_FAST_CALL;
        int firstLine() const JSC_FAST_CALL { return lineNo(); }
        int lastLine() const JSC_FAST_CALL { return m_lastLine; }

        virtual bool isEmptyStatement() const JSC_FAST_CALL { return false; }
        virtual bool isReturnNode() const JSC_FAST_CALL { return false; }
        virtual bool isExprStatement() const JSC_FAST_CALL { return false; }

        virtual bool isBlock() const JSC_FAST_CALL { return false; }

    private:
        int m_lastLine;
    };

    class NullNode : public ExpressionNode {
    public:
        NullNode(JSGlobalData*);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual bool isNull() const JSC_FAST_CALL { return true; }
    };

    class BooleanNode : public ExpressionNode {
    public:
        BooleanNode(JSGlobalData*, bool value);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual bool isPure(BytecodeGenerator&) const JSC_FAST_CALL { return true; }

        bool m_value;
    };

    class NumberNode : public ExpressionNode {
    public:
        NumberNode(JSGlobalData*, double v);

        double value() const JSC_FAST_CALL { return m_double; }
        void setValue(double d) JSC_FAST_CALL { m_double = d; }

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual bool isNumber() const JSC_FAST_CALL { return true; }
        virtual bool isPure(BytecodeGenerator&) const JSC_FAST_CALL { return true; }

        double m_double;
    };

    class StringNode : public ExpressionNode {
    public:
        StringNode(JSGlobalData*, const Identifier& v);

        const Identifier& value() { return m_value; }
        virtual bool isPure(BytecodeGenerator&) const JSC_FAST_CALL { return true; }

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        
        virtual bool isString() const JSC_FAST_CALL { return true; }

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
        RegisterID* emitThrowError(BytecodeGenerator&, ErrorType, const char* msg);
        RegisterID* emitThrowError(BytecodeGenerator&, ErrorType, const char* msg, const Identifier&);

    private:
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

        void setSubexpressionInfo(uint32_t subexpressionDivot, uint16_t subexpressionOffset)
        {
            ASSERT(subexpressionDivot <= divot());
            if ((divot() - subexpressionDivot) & ~0xFFFF) // Overflow means we can't do this safely, so just point at the primary divot
                return;
            m_subexpressionDivotOffset = divot() - subexpressionDivot;
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

        void setSubexpressionInfo(uint32_t subexpressionDivot, uint16_t subexpressionOffset)
        {
            ASSERT(subexpressionDivot >= divot());
            if ((subexpressionDivot - divot()) & ~0xFFFF) // Overflow means we can't do this safely, so just point at the primary divot
                return;
            m_subexpressionDivotOffset = subexpressionDivot - divot();
            m_subexpressionStartOffset = subexpressionOffset;
        }

    protected:
        uint16_t m_subexpressionDivotOffset;
        uint16_t m_subexpressionStartOffset;
    };

    class RegExpNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        RegExpNode(JSGlobalData*, const UString& pattern, const UString& flags);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        UString m_pattern;
        UString m_flags;
    };

    class ThisNode : public ExpressionNode {
    public:
        ThisNode(JSGlobalData*);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
    };

    class ResolveNode : public ExpressionNode {
    public:
        ResolveNode(JSGlobalData*, const Identifier&, int startOffset);

        const Identifier& identifier() const JSC_FAST_CALL { return m_ident; }

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual bool isPure(BytecodeGenerator&) const JSC_FAST_CALL;
        virtual bool isLocation() const JSC_FAST_CALL { return true; }
        virtual bool isResolveNode() const JSC_FAST_CALL { return true; }

        Identifier m_ident;
        int32_t m_startOffset;
    };

    class ElementNode : public ParserRefCounted {
    public:
        ElementNode(JSGlobalData*, int elision, ExpressionNode*);
        ElementNode(JSGlobalData*, ElementNode*, int elision, ExpressionNode*);
        virtual ~ElementNode();
        virtual void releaseNodes(NodeReleaser&);

        int elision() const { return m_elision; }
        ExpressionNode* value() { return m_node.get(); }
        ElementNode* next() { return m_next.get(); }

    private:
        RefPtr<ElementNode> m_next;
        int m_elision;
        RefPtr<ExpressionNode> m_node;
    };

    class ArrayNode : public ExpressionNode {
    public:
        ArrayNode(JSGlobalData*, int elision);
        ArrayNode(JSGlobalData*, ElementNode*);
        ArrayNode(JSGlobalData*, int elision, ElementNode*);
        virtual ~ArrayNode();
        virtual void releaseNodes(NodeReleaser&);

        PassRefPtr<ArgumentListNode> toArgumentList(JSGlobalData*) const;

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual bool isSimpleArray() const JSC_FAST_CALL;

        RefPtr<ElementNode> m_element;
        int m_elision;
        bool m_optional;
    };

    class PropertyNode : public ParserRefCounted {
    public:
        enum Type { Constant, Getter, Setter };

        PropertyNode(JSGlobalData*, const Identifier& name, ExpressionNode* value, Type);
        virtual ~PropertyNode();
        virtual void releaseNodes(NodeReleaser&);

        const Identifier& name() const { return m_name; }

    private:
        friend class PropertyListNode;
        Identifier m_name;
        RefPtr<ExpressionNode> m_assign;
        Type m_type;
    };

    class PropertyListNode : public Node {
    public:
        PropertyListNode(JSGlobalData*, PropertyNode*);
        PropertyListNode(JSGlobalData*, PropertyNode*, PropertyListNode*);
        virtual ~PropertyListNode();
        virtual void releaseNodes(NodeReleaser&);

        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

    private:
        RefPtr<PropertyNode> m_node;
        RefPtr<PropertyListNode> m_next;
    };

    class ObjectLiteralNode : public ExpressionNode {
    public:
        ObjectLiteralNode(JSGlobalData*);
        ObjectLiteralNode(JSGlobalData*, PropertyListNode*);
        virtual ~ObjectLiteralNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<PropertyListNode> m_list;
    };
    
    class BracketAccessorNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        BracketAccessorNode(JSGlobalData*, ExpressionNode* base, ExpressionNode* subscript, bool subscriptHasAssignments);
        virtual ~BracketAccessorNode();
        virtual void releaseNodes(NodeReleaser&);

        ExpressionNode* base() JSC_FAST_CALL { return m_base.get(); }
        ExpressionNode* subscript() JSC_FAST_CALL { return m_subscript.get(); }

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual bool isLocation() const JSC_FAST_CALL { return true; }
        virtual bool isBracketAccessorNode() const JSC_FAST_CALL { return true; }

        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
        bool m_subscriptHasAssignments;
    };

    class DotAccessorNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        DotAccessorNode(JSGlobalData*, ExpressionNode* base, const Identifier&);
        virtual ~DotAccessorNode();
        virtual void releaseNodes(NodeReleaser&);

        ExpressionNode* base() const JSC_FAST_CALL { return m_base.get(); }
        const Identifier& identifier() const JSC_FAST_CALL { return m_ident; }

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual bool isLocation() const JSC_FAST_CALL { return true; }
        virtual bool isDotAccessorNode() const JSC_FAST_CALL { return true; }

        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
    };

    class ArgumentListNode : public Node {
    public:
        ArgumentListNode(JSGlobalData*, ExpressionNode*);
        ArgumentListNode(JSGlobalData*, ArgumentListNode*, ExpressionNode*);
        virtual ~ArgumentListNode();
        virtual void releaseNodes(NodeReleaser&);

        RefPtr<ArgumentListNode> m_next;
        RefPtr<ExpressionNode> m_expr;

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
    };

    class ArgumentsNode : public ParserRefCounted {
    public:
        ArgumentsNode(JSGlobalData*);
        ArgumentsNode(JSGlobalData*, ArgumentListNode*);
        virtual ~ArgumentsNode();
        virtual void releaseNodes(NodeReleaser&);

        RefPtr<ArgumentListNode> m_listNode;
    };

    class NewExprNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        NewExprNode(JSGlobalData*, ExpressionNode*);
        NewExprNode(JSGlobalData*, ExpressionNode*, ArgumentsNode*);
        virtual ~NewExprNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_expr;
        RefPtr<ArgumentsNode> m_args;
    };

    class EvalFunctionCallNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        EvalFunctionCallNode(JSGlobalData*, ArgumentsNode*, unsigned divot, unsigned startOffset, unsigned endOffset);
        virtual ~EvalFunctionCallNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ArgumentsNode> m_args;
    };

    class FunctionCallValueNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        FunctionCallValueNode(JSGlobalData*, ExpressionNode*, ArgumentsNode*, unsigned divot, unsigned startOffset, unsigned endOffset);
        virtual ~FunctionCallValueNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_expr;
        RefPtr<ArgumentsNode> m_args;
    };

    class FunctionCallResolveNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        FunctionCallResolveNode(JSGlobalData*, const Identifier&, ArgumentsNode*, unsigned divot, unsigned startOffset, unsigned endOffset);

        virtual ~FunctionCallResolveNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        Identifier m_ident;
        RefPtr<ArgumentsNode> m_args;
        size_t m_index; // Used by LocalVarFunctionCallNode.
        size_t m_scopeDepth; // Used by ScopedVarFunctionCallNode and NonLocalVarFunctionCallNode
    };
    
    class FunctionCallBracketNode : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        FunctionCallBracketNode(JSGlobalData*, ExpressionNode* base, ExpressionNode* subscript, ArgumentsNode*, unsigned divot, unsigned startOffset, unsigned endOffset);
        virtual ~FunctionCallBracketNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
        RefPtr<ArgumentsNode> m_args;
    };

    class FunctionCallDotNode : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        FunctionCallDotNode(JSGlobalData*, ExpressionNode* base, const Identifier&, ArgumentsNode*, unsigned divot, unsigned startOffset, unsigned endOffset);
        virtual ~FunctionCallDotNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

    protected:
        RefPtr<ExpressionNode> m_base;
        const Identifier m_ident;
        RefPtr<ArgumentsNode> m_args;
    };

    class CallFunctionCallDotNode : public FunctionCallDotNode {
    public:
        CallFunctionCallDotNode(JSGlobalData*, ExpressionNode* base, const Identifier&, ArgumentsNode*, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
    };
    
    class ApplyFunctionCallDotNode : public FunctionCallDotNode {
    public:
        ApplyFunctionCallDotNode(JSGlobalData*, ExpressionNode* base, const Identifier&, ArgumentsNode*, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
    };

    class PrePostResolveNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        PrePostResolveNode(JSGlobalData*, const Identifier&, unsigned divot, unsigned startOffset, unsigned endOffset);

    protected:
        const Identifier m_ident;
    };

    class PostfixResolveNode : public PrePostResolveNode {
    public:
        PostfixResolveNode(JSGlobalData*, const Identifier&, Operator, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        Operator m_operator;
    };

    class PostfixBracketNode : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        PostfixBracketNode(JSGlobalData*, ExpressionNode* base, ExpressionNode* subscript, Operator, unsigned divot, unsigned startOffset, unsigned endOffset);
        virtual ~PostfixBracketNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
        Operator m_operator;
    };

    class PostfixDotNode : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        PostfixDotNode(JSGlobalData*, ExpressionNode* base, const Identifier&, Operator, unsigned divot, unsigned startOffset, unsigned endOffset);
        virtual ~PostfixDotNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
        Operator m_operator;
    };

    class PostfixErrorNode : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        PostfixErrorNode(JSGlobalData*, ExpressionNode*, Operator, unsigned divot, unsigned startOffset, unsigned endOffset);
        virtual ~PostfixErrorNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_expr;
        Operator m_operator;
    };

    class DeleteResolveNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        DeleteResolveNode(JSGlobalData*, const Identifier&, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        Identifier m_ident;
    };

    class DeleteBracketNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        DeleteBracketNode(JSGlobalData*, ExpressionNode* base, ExpressionNode* subscript, unsigned divot, unsigned startOffset, unsigned endOffset);
        virtual ~DeleteBracketNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
    };

    class DeleteDotNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        DeleteDotNode(JSGlobalData*, ExpressionNode* base, const Identifier&, unsigned divot, unsigned startOffset, unsigned endOffset);
        virtual ~DeleteDotNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
    };

    class DeleteValueNode : public ExpressionNode {
    public:
        DeleteValueNode(JSGlobalData*, ExpressionNode*);
        virtual ~DeleteValueNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_expr;
    };

    class VoidNode : public ExpressionNode {
    public:
        VoidNode(JSGlobalData*, ExpressionNode*);
        virtual ~VoidNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_expr;
    };

    class TypeOfResolveNode : public ExpressionNode {
    public:
        TypeOfResolveNode(JSGlobalData*, const Identifier&);

        const Identifier& identifier() const JSC_FAST_CALL { return m_ident; }

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        Identifier m_ident;
    };

    class TypeOfValueNode : public ExpressionNode {
    public:
        TypeOfValueNode(JSGlobalData*, ExpressionNode*);

        virtual ~TypeOfValueNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_expr;
    };

    class PrefixResolveNode : public PrePostResolveNode {
    public:
        PrefixResolveNode(JSGlobalData*, const Identifier&, Operator, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        Operator m_operator;
    };

    class PrefixBracketNode : public ExpressionNode, public ThrowablePrefixedSubExpressionData {
    public:
        PrefixBracketNode(JSGlobalData*, ExpressionNode* base, ExpressionNode* subscript, Operator, unsigned divot, unsigned startOffset, unsigned endOffset);
        virtual ~PrefixBracketNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
        Operator m_operator;
    };

    class PrefixDotNode : public ExpressionNode, public ThrowablePrefixedSubExpressionData {
    public:
        PrefixDotNode(JSGlobalData*, ExpressionNode* base, const Identifier&, Operator, unsigned divot, unsigned startOffset, unsigned endOffset);

        virtual ~PrefixDotNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
        Operator m_operator;
    };

    class PrefixErrorNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        PrefixErrorNode(JSGlobalData*, ExpressionNode*, Operator, unsigned divot, unsigned startOffset, unsigned endOffset);

        virtual ~PrefixErrorNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_expr;
        Operator m_operator;
    };

    class UnaryOpNode : public ExpressionNode {
    public:
        UnaryOpNode(JSGlobalData*, ResultType, ExpressionNode*, OpcodeID);
        virtual ~UnaryOpNode();
        virtual void releaseNodes(NodeReleaser&);

    protected:
        ExpressionNode* expr() { return m_expr.get(); }

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        OpcodeID opcodeID() const { return m_opcodeID; }

        RefPtr<ExpressionNode> m_expr;
        OpcodeID m_opcodeID;
    };

    class UnaryPlusNode : public UnaryOpNode {
    public:
        UnaryPlusNode(JSGlobalData*, ExpressionNode*);

    private:
        virtual ExpressionNode* stripUnaryPlus() { return expr(); }
    };

    class NegateNode : public UnaryOpNode {
    public:
        NegateNode(JSGlobalData*, ExpressionNode*);
    };

    class BitwiseNotNode : public UnaryOpNode {
    public:
        BitwiseNotNode(JSGlobalData*, ExpressionNode*);
    };

    class LogicalNotNode : public UnaryOpNode {
    public:
        LogicalNotNode(JSGlobalData*, ExpressionNode*);
    };

    class BinaryOpNode : public ExpressionNode {
    public:
        BinaryOpNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, OpcodeID, bool rightHasAssignments);
        BinaryOpNode(JSGlobalData*, ResultType, ExpressionNode* expr1, ExpressionNode* expr2, OpcodeID, bool rightHasAssignments);
        virtual ~BinaryOpNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        RegisterID* emitStrcat(BytecodeGenerator& generator, RegisterID* dst, RegisterID* lhs= 0);

    protected:
        OpcodeID opcodeID() const { return m_opcodeID; }

    protected:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    private:
        OpcodeID m_opcodeID;
    protected:
        bool m_rightHasAssignments;
    };

    class ReverseBinaryOpNode : public BinaryOpNode {
    public:
        ReverseBinaryOpNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, OpcodeID, bool rightHasAssignments);
        ReverseBinaryOpNode(JSGlobalData*, ResultType, ExpressionNode* expr1, ExpressionNode* expr2, OpcodeID, bool rightHasAssignments);

        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
    };

    class MultNode : public BinaryOpNode {
    public:
        MultNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class DivNode : public BinaryOpNode {
    public:
        DivNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class ModNode : public BinaryOpNode {
    public:
        ModNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class AddNode : public BinaryOpNode {
    public:
        AddNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);

        virtual bool isAdd() const JSC_FAST_CALL { return true; }
    };

    class SubNode : public BinaryOpNode {
    public:
        SubNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class LeftShiftNode : public BinaryOpNode {
    public:
        LeftShiftNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class RightShiftNode : public BinaryOpNode {
    public:
        RightShiftNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class UnsignedRightShiftNode : public BinaryOpNode {
    public:
        UnsignedRightShiftNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class LessNode : public BinaryOpNode {
    public:
        LessNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class GreaterNode : public ReverseBinaryOpNode {
    public:
        GreaterNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class LessEqNode : public BinaryOpNode {
    public:
        LessEqNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class GreaterEqNode : public ReverseBinaryOpNode {
    public:
        GreaterEqNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class ThrowableBinaryOpNode : public BinaryOpNode, public ThrowableExpressionData {
    public:
        ThrowableBinaryOpNode(JSGlobalData*, ResultType, ExpressionNode* expr1, ExpressionNode* expr2, OpcodeID, bool rightHasAssignments);
        ThrowableBinaryOpNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, OpcodeID, bool rightHasAssignments);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
    };
    
    class InstanceOfNode : public ThrowableBinaryOpNode {
    public:
        InstanceOfNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
    };

    class InNode : public ThrowableBinaryOpNode {
    public:
        InNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class EqualNode : public BinaryOpNode {
    public:
        EqualNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
    };

    class NotEqualNode : public BinaryOpNode {
    public:
        NotEqualNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class StrictEqualNode : public BinaryOpNode {
    public:
        StrictEqualNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
    };

    class NotStrictEqualNode : public BinaryOpNode {
    public:
        NotStrictEqualNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class BitAndNode : public BinaryOpNode {
    public:
        BitAndNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class BitOrNode : public BinaryOpNode {
    public:
        BitOrNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class BitXOrNode : public BinaryOpNode {
    public:
        BitXOrNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    // m_expr1 && m_expr2, m_expr1 || m_expr2
    class LogicalOpNode : public ExpressionNode {
    public:
        LogicalOpNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, LogicalOperator);
        virtual ~LogicalOpNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
        LogicalOperator m_operator;
    };

    // The ternary operator, "m_logical ? m_expr1 : m_expr2"
    class ConditionalNode : public ExpressionNode {
    public:
        ConditionalNode(JSGlobalData*, ExpressionNode* logical, ExpressionNode* expr1, ExpressionNode* expr2);
        virtual ~ConditionalNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_logical;
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };

    class ReadModifyResolveNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        ReadModifyResolveNode(JSGlobalData*, const Identifier&, Operator, ExpressionNode*  right, bool rightHasAssignments, unsigned divot, unsigned startOffset, unsigned endOffset);
        virtual ~ReadModifyResolveNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        Identifier m_ident;
        RefPtr<ExpressionNode> m_right;
        size_t m_index; // Used by ReadModifyLocalVarNode.
        Operator m_operator;
        bool m_rightHasAssignments;
    };

    class AssignResolveNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        AssignResolveNode(JSGlobalData*, const Identifier&, ExpressionNode* right, bool rightHasAssignments);
        virtual ~AssignResolveNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        Identifier m_ident;
        RefPtr<ExpressionNode> m_right;
        size_t m_index; // Used by ReadModifyLocalVarNode.
        bool m_rightHasAssignments;
    };

    class ReadModifyBracketNode : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        ReadModifyBracketNode(JSGlobalData*, ExpressionNode* base, ExpressionNode* subscript, Operator, ExpressionNode* right, bool subscriptHasAssignments, bool rightHasAssignments, unsigned divot, unsigned startOffset, unsigned endOffset);
        virtual ~ReadModifyBracketNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
        RefPtr<ExpressionNode> m_right;
        Operator m_operator : 30;
        bool m_subscriptHasAssignments : 1;
        bool m_rightHasAssignments : 1;
    };

    class AssignBracketNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        AssignBracketNode(JSGlobalData*, ExpressionNode* base, ExpressionNode* subscript, ExpressionNode* right, bool subscriptHasAssignments, bool rightHasAssignments, unsigned divot, unsigned startOffset, unsigned endOffset);
        virtual ~AssignBracketNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
        RefPtr<ExpressionNode> m_right;
        bool m_subscriptHasAssignments : 1;
        bool m_rightHasAssignments : 1;
    };

    class AssignDotNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        AssignDotNode(JSGlobalData*, ExpressionNode* base, const Identifier&, ExpressionNode* right, bool rightHasAssignments, unsigned divot, unsigned startOffset, unsigned endOffset);
        virtual ~AssignDotNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
        RefPtr<ExpressionNode> m_right;
        bool m_rightHasAssignments;
    };

    class ReadModifyDotNode : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        ReadModifyDotNode(JSGlobalData*, ExpressionNode* base, const Identifier&, Operator, ExpressionNode* right, bool rightHasAssignments, unsigned divot, unsigned startOffset, unsigned endOffset);
        virtual ~ReadModifyDotNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
        RefPtr<ExpressionNode> m_right;
        Operator m_operator : 31;
        bool m_rightHasAssignments : 1;
    };

    class AssignErrorNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        AssignErrorNode(JSGlobalData*, ExpressionNode* left, Operator, ExpressionNode* right, unsigned divot, unsigned startOffset, unsigned endOffset);
        virtual ~AssignErrorNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_left;
        Operator m_operator;
        RefPtr<ExpressionNode> m_right;
    };

    class CommaNode : public ExpressionNode {
    public:
        CommaNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2);
        virtual ~CommaNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };
    
    class VarDeclCommaNode : public CommaNode {
    public:
        VarDeclCommaNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2);
    };

    class ConstDeclNode : public ExpressionNode {
    public:
        ConstDeclNode(JSGlobalData*, const Identifier&, ExpressionNode*);
        virtual ~ConstDeclNode();
        virtual void releaseNodes(NodeReleaser&);

        bool hasInitializer() const { return m_init; }
        const Identifier& ident() { return m_ident; }

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        virtual RegisterID* emitCodeSingle(BytecodeGenerator&) JSC_FAST_CALL;

        Identifier m_ident;

    public:
        RefPtr<ConstDeclNode> m_next;

    private:
        RefPtr<ExpressionNode> m_init;
    };

    class ConstStatementNode : public StatementNode {
    public:
        ConstStatementNode(JSGlobalData*, ConstDeclNode* next);
        virtual ~ConstStatementNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ConstDeclNode> m_next;
    };

    typedef Vector<RefPtr<StatementNode> > StatementVector;

    class SourceElements : public ParserRefCounted {
    public:
        SourceElements(JSGlobalData*);

        void append(PassRefPtr<StatementNode>);
        void releaseContentsIntoVector(StatementVector& destination)
        {
            ASSERT(destination.isEmpty());
            m_statements.swap(destination);
            destination.shrinkToFit();
        }

    private:
        StatementVector m_statements;
    };

    class BlockNode : public StatementNode {
    public:
        BlockNode(JSGlobalData*, SourceElements* children);

        virtual ~BlockNode();
        virtual void releaseNodes(NodeReleaser&);

        StatementVector& children() { return m_children; }

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual bool isBlock() const JSC_FAST_CALL { return true; }

        StatementVector m_children;
    };

    class EmptyStatementNode : public StatementNode {
    public:
        EmptyStatementNode(JSGlobalData*);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual bool isEmptyStatement() const JSC_FAST_CALL { return true; }
    };
    
    class DebuggerStatementNode : public StatementNode {
    public:
        DebuggerStatementNode(JSGlobalData*);
        
    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
    };

    class ExprStatementNode : public StatementNode {
    public:
        ExprStatementNode(JSGlobalData*, ExpressionNode*);

        ExpressionNode* expr() const { return m_expr.get(); }

    private:
        virtual bool isExprStatement() const JSC_FAST_CALL { return true; }

        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_expr;
    };

    class VarStatementNode : public StatementNode {
    public:
        VarStatementNode(JSGlobalData*, ExpressionNode*);        
        virtual ~VarStatementNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_expr;
    };

    class IfNode : public StatementNode {
    public:
        IfNode(JSGlobalData*, ExpressionNode* condition, StatementNode* ifBlock);
        virtual ~IfNode();
        virtual void releaseNodes(NodeReleaser&);

    protected:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_condition;
        RefPtr<StatementNode> m_ifBlock;
    };

    class IfElseNode : public IfNode {
    public:
        IfElseNode(JSGlobalData*, ExpressionNode* condition, StatementNode* ifBlock, StatementNode* elseBlock);
        virtual ~IfElseNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<StatementNode> m_elseBlock;
    };

    class DoWhileNode : public StatementNode {
    public:
        DoWhileNode(JSGlobalData*, StatementNode* statement, ExpressionNode*);
        virtual ~DoWhileNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<StatementNode> m_statement;
        RefPtr<ExpressionNode> m_expr;
    };

    class WhileNode : public StatementNode {
    public:
        WhileNode(JSGlobalData*, ExpressionNode*, StatementNode* statement);
        virtual ~WhileNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_expr;
        RefPtr<StatementNode> m_statement;
    };

    class ForNode : public StatementNode {
    public:
        ForNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, ExpressionNode* expr3, StatementNode* statement, bool expr1WasVarDecl);
        virtual ~ForNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
        RefPtr<ExpressionNode> m_expr3;
        RefPtr<StatementNode> m_statement;
        bool m_expr1WasVarDecl;
    };

    class ForInNode : public StatementNode, public ThrowableExpressionData {
    public:
        ForInNode(JSGlobalData*, ExpressionNode*, ExpressionNode*, StatementNode*);
        ForInNode(JSGlobalData*, const Identifier&, ExpressionNode*, ExpressionNode*, StatementNode*, int divot, int startOffset, int endOffset);
        virtual ~ForInNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        Identifier m_ident;
        RefPtr<ExpressionNode> m_init;
        RefPtr<ExpressionNode> m_lexpr;
        RefPtr<ExpressionNode> m_expr;
        RefPtr<StatementNode> m_statement;
        bool m_identIsVarDecl;
    };

    class ContinueNode : public StatementNode, public ThrowableExpressionData {
    public:
        ContinueNode(JSGlobalData*);
        ContinueNode(JSGlobalData*, const Identifier&);
        
    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        Identifier m_ident;
    };

    class BreakNode : public StatementNode, public ThrowableExpressionData {
    public:
        BreakNode(JSGlobalData*);
        BreakNode(JSGlobalData*, const Identifier&);
        
    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        Identifier m_ident;
    };

    class ReturnNode : public StatementNode, public ThrowableExpressionData {
    public:
        ReturnNode(JSGlobalData*, ExpressionNode* value);
        virtual ~ReturnNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual bool isReturnNode() const JSC_FAST_CALL { return true; }

        RefPtr<ExpressionNode> m_value;
    };

    class WithNode : public StatementNode {
    public:
        WithNode(JSGlobalData*, ExpressionNode*, StatementNode*, uint32_t divot, uint32_t expressionLength);
        virtual ~WithNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_expr;
        RefPtr<StatementNode> m_statement;
        uint32_t m_divot;
        uint32_t m_expressionLength;
    };

    class LabelNode : public StatementNode, public ThrowableExpressionData {
    public:
        LabelNode(JSGlobalData*, const Identifier& name, StatementNode*);
        virtual ~LabelNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        Identifier m_name;
        RefPtr<StatementNode> m_statement;
    };

    class ThrowNode : public StatementNode, public ThrowableExpressionData {
    public:
        ThrowNode(JSGlobalData*, ExpressionNode*);
        virtual ~ThrowNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ExpressionNode> m_expr;
    };

    class TryNode : public StatementNode {
    public:
        TryNode(JSGlobalData*, StatementNode* tryBlock, const Identifier& exceptionIdent, bool catchHasEval, StatementNode* catchBlock, StatementNode* finallyBlock);
        virtual ~TryNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* dst = 0) JSC_FAST_CALL;

        RefPtr<StatementNode> m_tryBlock;
        Identifier m_exceptionIdent;
        RefPtr<StatementNode> m_catchBlock;
        RefPtr<StatementNode> m_finallyBlock;
        bool m_catchHasEval;
    };

    class ParameterNode : public ParserRefCounted {
    public:
        ParameterNode(JSGlobalData*, const Identifier&);
        ParameterNode(JSGlobalData*, ParameterNode*, const Identifier&);
        virtual ~ParameterNode();
        virtual void releaseNodes(NodeReleaser&);

        const Identifier& ident() const JSC_FAST_CALL { return m_ident; }
        ParameterNode* nextParam() const JSC_FAST_CALL { return m_next.get(); }

    private:
        Identifier m_ident;
        RefPtr<ParameterNode> m_next;
    };

    struct ScopeNodeData {
        typedef DeclarationStacks::VarStack VarStack;
        typedef DeclarationStacks::FunctionStack FunctionStack;

        ScopeNodeData(SourceElements*, VarStack*, FunctionStack*, int numConstants);

        VarStack m_varStack;
        FunctionStack m_functionStack;
        int m_numConstants;
        StatementVector m_children;

        void mark();
    };

    class ScopeNode : public StatementNode {
    public:
        typedef DeclarationStacks::VarStack VarStack;
        typedef DeclarationStacks::FunctionStack FunctionStack;

        ScopeNode(JSGlobalData*) JSC_FAST_CALL;
        ScopeNode(JSGlobalData*, const SourceCode&, SourceElements*, VarStack*, FunctionStack*, CodeFeatures, int numConstants) JSC_FAST_CALL;
        virtual ~ScopeNode();
        virtual void releaseNodes(NodeReleaser&);

        void adoptData(std::auto_ptr<ScopeNodeData> data) { m_data.adopt(data); }
        ScopeNodeData* data() const { return m_data.get(); }
        void destroyData() { m_data.clear(); }

        const SourceCode& source() const { return m_source; }
        const UString& sourceURL() const JSC_FAST_CALL { return m_source.provider()->url(); }
        intptr_t sourceID() const { return m_source.provider()->asID(); }

        void setFeatures(CodeFeatures features) { m_features = features; }
        CodeFeatures features() { return m_features; }

        bool usesEval() const { return m_features & EvalFeature; }
        bool usesArguments() const { return m_features & ArgumentsFeature; }
        void setUsesArguments() { m_features |= ArgumentsFeature; }
        bool usesThis() const { return m_features & ThisFeature; }
        bool needsActivation() const { return m_features & (EvalFeature | ClosureFeature | WithFeature | CatchFeature); }

        VarStack& varStack() { ASSERT(m_data); return m_data->m_varStack; }
        FunctionStack& functionStack() { ASSERT(m_data); return m_data->m_functionStack; }

        StatementVector& children() { ASSERT(m_data); return m_data->m_children; }

        int neededConstants()
        {
            ASSERT(m_data);
            // We may need 2 more constants than the count given by the parser,
            // because of the various uses of jsUndefined() and jsNull().
            return m_data->m_numConstants + 2;
        }

        virtual void mark() { }

    protected:
        void setSource(const SourceCode& source) { m_source = source; }

    private:
        OwnPtr<ScopeNodeData> m_data;
        CodeFeatures m_features;
        SourceCode m_source;
    };

    class ProgramNode : public ScopeNode {
    public:
        static ProgramNode* create(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, const SourceCode&, CodeFeatures, int numConstants) JSC_FAST_CALL;

        ProgramCodeBlock& bytecode(ScopeChainNode* scopeChain) JSC_FAST_CALL
        {
            if (!m_code)
                generateBytecode(scopeChain);
            return *m_code;
        }

    private:
        ProgramNode(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, const SourceCode&, CodeFeatures, int numConstants) JSC_FAST_CALL;

        void generateBytecode(ScopeChainNode*) JSC_FAST_CALL;
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        OwnPtr<ProgramCodeBlock> m_code;
    };

    class EvalNode : public ScopeNode {
    public:
        static EvalNode* create(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, const SourceCode&, CodeFeatures, int numConstants) JSC_FAST_CALL;

        EvalCodeBlock& bytecode(ScopeChainNode* scopeChain) JSC_FAST_CALL
        {
            if (!m_code)
                generateBytecode(scopeChain);
            return *m_code;
        }

        EvalCodeBlock& bytecodeForExceptionInfoReparse(ScopeChainNode*, CodeBlock*) JSC_FAST_CALL;

        virtual void mark();

    private:
        EvalNode(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, const SourceCode&, CodeFeatures, int numConstants) JSC_FAST_CALL;

        void generateBytecode(ScopeChainNode*) JSC_FAST_CALL;
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;
        
        OwnPtr<EvalCodeBlock> m_code;
    };

    class FunctionBodyNode : public ScopeNode {
        friend class JIT;
    public:
#if ENABLE(JIT)
        static PassRefPtr<FunctionBodyNode> createNativeThunk(JSGlobalData*) JSC_FAST_CALL;
#endif
        static FunctionBodyNode* create(JSGlobalData*) JSC_FAST_CALL;
        static FunctionBodyNode* create(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, const SourceCode&, CodeFeatures, int numConstants) JSC_FAST_CALL;
        virtual ~FunctionBodyNode();

        const Identifier* parameters() const JSC_FAST_CALL { return m_parameters; }
        size_t parameterCount() const { return m_parameterCount; }
        UString paramString() const JSC_FAST_CALL;
        Identifier* copyParameters();

        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        bool isGenerated() const
        {
            return m_code;
        }

        bool isHostFunction() const
        {
#if ENABLE(JIT)
            return m_jitCode && !m_code;
#else
            return true;
#endif
        }

        virtual void mark();

        void finishParsing(const SourceCode&, ParameterNode*);
        void finishParsing(Identifier* parameters, size_t parameterCount);
        
        UString toSourceString() const JSC_FAST_CALL { return source().toString(); }

        CodeBlock& bytecodeForExceptionInfoReparse(ScopeChainNode*, CodeBlock*) JSC_FAST_CALL;
#if ENABLE(JIT)
        JITCode generatedJITCode()
        {
            ASSERT(m_jitCode);
            return m_jitCode;
        }

        JITCode jitCode(ScopeChainNode* scopeChain)
        {
            if (!m_jitCode)
                generateJITCode(scopeChain);
            return m_jitCode;
        }
#endif
        CodeBlock& bytecode(ScopeChainNode* scopeChain) JSC_FAST_CALL
        {
            ASSERT(scopeChain);
            if (!m_code)
                generateBytecode(scopeChain);
            return *m_code;
        }
        
        CodeBlock& generatedBytecode()
        {
            ASSERT(m_code);
            return *m_code;
        }
        
    private:
        FunctionBodyNode(JSGlobalData*) JSC_FAST_CALL;
        FunctionBodyNode(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, const SourceCode&, CodeFeatures, int numConstants) JSC_FAST_CALL;

        void generateBytecode(ScopeChainNode*) JSC_FAST_CALL;
#if ENABLE(JIT)
        void generateJITCode(ScopeChainNode*) JSC_FAST_CALL;
        
        JITCode m_jitCode;
#endif
        Identifier* m_parameters;
        size_t m_parameterCount;
        OwnPtr<CodeBlock> m_code;
    };

    class FuncExprNode : public ExpressionNode {
    public:
        FuncExprNode(JSGlobalData*, const Identifier&, FunctionBodyNode* body, const SourceCode& source, ParameterNode* parameter = 0);
        virtual ~FuncExprNode();
        virtual void releaseNodes(NodeReleaser&);

        JSFunction* makeFunction(ExecState*, ScopeChainNode*) JSC_FAST_CALL;

        FunctionBodyNode* body() { return m_body.get(); }

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        virtual bool isFuncExprNode() const JSC_FAST_CALL { return true; } 

        Identifier m_ident;
        RefPtr<ParameterNode> m_parameter;
        RefPtr<FunctionBodyNode> m_body;
    };

    class FuncDeclNode : public StatementNode {
    public:
        FuncDeclNode(JSGlobalData*, const Identifier&, FunctionBodyNode*, const SourceCode&, ParameterNode* = 0);
        virtual ~FuncDeclNode();
        virtual void releaseNodes(NodeReleaser&);

        JSFunction* makeFunction(ExecState*, ScopeChainNode*) JSC_FAST_CALL;

        Identifier m_ident;

        FunctionBodyNode* body() { return m_body.get(); }

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

        RefPtr<ParameterNode> m_parameter;
        RefPtr<FunctionBodyNode> m_body;
    };

    class CaseClauseNode : public ParserRefCounted {
    public:
        CaseClauseNode(JSGlobalData*, ExpressionNode*);
        CaseClauseNode(JSGlobalData*, ExpressionNode*, SourceElements*);
        virtual ~CaseClauseNode();
        virtual void releaseNodes(NodeReleaser&);

        ExpressionNode* expr() const { return m_expr.get(); }
        StatementVector& children() { return m_children; }

    private:
        RefPtr<ExpressionNode> m_expr;
        StatementVector m_children;
    };

    class ClauseListNode : public ParserRefCounted {
    public:
        ClauseListNode(JSGlobalData*, CaseClauseNode*);
        ClauseListNode(JSGlobalData*, ClauseListNode*, CaseClauseNode*);
        virtual ~ClauseListNode();
        virtual void releaseNodes(NodeReleaser&);

        CaseClauseNode* getClause() const JSC_FAST_CALL { return m_clause.get(); }
        ClauseListNode* getNext() const JSC_FAST_CALL { return m_next.get(); }

    private:
        RefPtr<CaseClauseNode> m_clause;
        RefPtr<ClauseListNode> m_next;
    };

    class CaseBlockNode : public ParserRefCounted {
    public:
        CaseBlockNode(JSGlobalData*, ClauseListNode* list1, CaseClauseNode* defaultClause, ClauseListNode* list2);
        virtual ~CaseBlockNode();
        virtual void releaseNodes(NodeReleaser&);

        RegisterID* emitBytecodeForBlock(BytecodeGenerator&, RegisterID* input, RegisterID* dst = 0) JSC_FAST_CALL;

    private:
        SwitchInfo::SwitchType tryOptimizedSwitch(Vector<ExpressionNode*, 8>& literalVector, int32_t& min_num, int32_t& max_num);
        RefPtr<ClauseListNode> m_list1;
        RefPtr<CaseClauseNode> m_defaultClause;
        RefPtr<ClauseListNode> m_list2;
    };

    class SwitchNode : public StatementNode {
    public:
        SwitchNode(JSGlobalData*, ExpressionNode*, CaseBlockNode*);
        virtual ~SwitchNode();
        virtual void releaseNodes(NodeReleaser&);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0) JSC_FAST_CALL;

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

#endif // Nodes_h
