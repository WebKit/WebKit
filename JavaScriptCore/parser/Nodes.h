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
#include "ParserArena.h"
#include "ResultType.h"
#include "SourceCode.h"
#include "SymbolTable.h"
#include <wtf/MathExtras.h>
#include <wtf/OwnPtr.h>

namespace JSC {

    class ArgumentListNode;
    class CodeBlock;
    class BytecodeGenerator;
    class FuncDeclNode;
    class EvalCodeBlock;
    class JSFunction;
    class ProgramCodeBlock;
    class PropertyListNode;
    class ReadModifyResolveNode;
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
        typedef Vector<FuncDeclNode*> FunctionStack;
    }

    struct SwitchInfo {
        enum SwitchType { SwitchNone, SwitchImmediate, SwitchCharacter, SwitchString };
        uint32_t bytecodeOffset;
        SwitchType switchType;
    };

    class ParserArenaDeletable {
    protected:
        ParserArenaDeletable() { }

    public:
        virtual ~ParserArenaDeletable() { }

        // Objects created with this version of new are deleted when the arena is deleted.
        void* operator new(size_t, JSGlobalData*);

        // Objects created with this version of new are not deleted when the arena is deleted.
        // Other arrangements must be made.
        void* operator new(size_t);
    };

    class ParserArenaRefCounted : public RefCounted<ParserArenaRefCounted> {
    protected:
        ParserArenaRefCounted(JSGlobalData*);

    public:
        virtual ~ParserArenaRefCounted()
        {
            ASSERT(deletionHasBegun());
        }
    };

    class Node : public ParserArenaDeletable {
    protected:
        Node(JSGlobalData*);

    public:
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
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* dst = 0) = 0;

        int lineNo() const { return m_line; }

    protected:
        int m_line;
    };

    class ExpressionNode : public Node {
    public:
        ExpressionNode(JSGlobalData*, ResultType = ResultType::unknownType());

        virtual bool isNumber() const { return false; }
        virtual bool isString() const { return false; }
        virtual bool isNull() const { return false; }
        virtual bool isPure(BytecodeGenerator&) const { return false; }        
        virtual bool isLocation() const { return false; }
        virtual bool isResolveNode() const { return false; }
        virtual bool isBracketAccessorNode() const { return false; }
        virtual bool isDotAccessorNode() const { return false; }
        virtual bool isFuncExprNode() const { return false; }
        virtual bool isCommaNode() const { return false; }
        virtual bool isSimpleArray() const { return false; }
        virtual bool isAdd() const { return false; }

        virtual ExpressionNode* stripUnaryPlus() { return this; }

        ResultType resultDescriptor() const { return m_resultType; }

        // This needs to be in public in order to compile using GCC 3.x 
        typedef enum { EvalOperator, FunctionCall } CallerType;

    private:
        ResultType m_resultType;
    };

    class StatementNode : public Node {
    public:
        StatementNode(JSGlobalData*);

        void setLoc(int line0, int line1);
        int firstLine() const { return lineNo(); }
        int lastLine() const { return m_lastLine; }

        virtual bool isEmptyStatement() const { return false; }
        virtual bool isReturnNode() const { return false; }
        virtual bool isExprStatement() const { return false; }

        virtual bool isBlock() const { return false; }

    private:
        int m_lastLine;
    };

    class NullNode : public ExpressionNode {
    public:
        NullNode(JSGlobalData*);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        virtual bool isNull() const { return true; }
    };

    class BooleanNode : public ExpressionNode {
    public:
        BooleanNode(JSGlobalData*, bool value);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        virtual bool isPure(BytecodeGenerator&) const { return true; }

        bool m_value;
    };

    class NumberNode : public ExpressionNode {
    public:
        NumberNode(JSGlobalData*, double v);

        double value() const { return m_double; }
        void setValue(double d) { m_double = d; }

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        virtual bool isNumber() const { return true; }
        virtual bool isPure(BytecodeGenerator&) const { return true; }

        double m_double;
    };

    class StringNode : public ExpressionNode {
    public:
        StringNode(JSGlobalData*, const Identifier& v);

        const Identifier& value() { return m_value; }
        virtual bool isPure(BytecodeGenerator&) const { return true; }

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);
        
        virtual bool isString() const { return true; }

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
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        UString m_pattern;
        UString m_flags;
    };

    class ThisNode : public ExpressionNode {
    public:
        ThisNode(JSGlobalData*);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);
    };

    class ResolveNode : public ExpressionNode {
    public:
        ResolveNode(JSGlobalData*, const Identifier&, int startOffset);

        const Identifier& identifier() const { return m_ident; }

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        virtual bool isPure(BytecodeGenerator&) const ;
        virtual bool isLocation() const { return true; }
        virtual bool isResolveNode() const { return true; }

        Identifier m_ident;
        int32_t m_startOffset;
    };

    class ElementNode : public ParserArenaDeletable {
    public:
        ElementNode(JSGlobalData*, int elision, ExpressionNode*);
        ElementNode(JSGlobalData*, ElementNode*, int elision, ExpressionNode*);

        int elision() const { return m_elision; }
        ExpressionNode* value() { return m_node; }
        ElementNode* next() { return m_next; }

    private:
        ElementNode* m_next;
        int m_elision;
        ExpressionNode* m_node;
    };

    class ArrayNode : public ExpressionNode {
    public:
        ArrayNode(JSGlobalData*, int elision);
        ArrayNode(JSGlobalData*, ElementNode*);
        ArrayNode(JSGlobalData*, int elision, ElementNode*);

        ArgumentListNode* toArgumentList(JSGlobalData*) const;

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        virtual bool isSimpleArray() const ;

        ElementNode* m_element;
        int m_elision;
        bool m_optional;
    };

    class PropertyNode : public ParserArenaDeletable {
    public:
        enum Type { Constant, Getter, Setter };

        PropertyNode(JSGlobalData*, const Identifier& name, ExpressionNode* value, Type);

        const Identifier& name() const { return m_name; }

    private:
        friend class PropertyListNode;
        Identifier m_name;
        ExpressionNode* m_assign;
        Type m_type;
    };

    class PropertyListNode : public Node {
    public:
        PropertyListNode(JSGlobalData*, PropertyNode*);
        PropertyListNode(JSGlobalData*, PropertyNode*, PropertyListNode*);

        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

    private:
        PropertyNode* m_node;
        PropertyListNode* m_next;
    };

    class ObjectLiteralNode : public ExpressionNode {
    public:
        ObjectLiteralNode(JSGlobalData*);
        ObjectLiteralNode(JSGlobalData*, PropertyListNode*);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        PropertyListNode* m_list;
    };
    
    class BracketAccessorNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        BracketAccessorNode(JSGlobalData*, ExpressionNode* base, ExpressionNode* subscript, bool subscriptHasAssignments);

        ExpressionNode* base() const { return m_base; }
        ExpressionNode* subscript() const { return m_subscript; }

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        virtual bool isLocation() const { return true; }
        virtual bool isBracketAccessorNode() const { return true; }

        ExpressionNode* m_base;
        ExpressionNode* m_subscript;
        bool m_subscriptHasAssignments;
    };

    class DotAccessorNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        DotAccessorNode(JSGlobalData*, ExpressionNode* base, const Identifier&);

        ExpressionNode* base() const { return m_base; }
        const Identifier& identifier() const { return m_ident; }

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        virtual bool isLocation() const { return true; }
        virtual bool isDotAccessorNode() const { return true; }

        ExpressionNode* m_base;
        Identifier m_ident;
    };

    class ArgumentListNode : public Node {
    public:
        ArgumentListNode(JSGlobalData*, ExpressionNode*);
        ArgumentListNode(JSGlobalData*, ArgumentListNode*, ExpressionNode*);

        ArgumentListNode* m_next;
        ExpressionNode* m_expr;

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);
    };

    class ArgumentsNode : public ParserArenaDeletable {
    public:
        ArgumentsNode(JSGlobalData*);
        ArgumentsNode(JSGlobalData*, ArgumentListNode*);

        ArgumentListNode* m_listNode;
    };

    class NewExprNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        NewExprNode(JSGlobalData*, ExpressionNode*);
        NewExprNode(JSGlobalData*, ExpressionNode*, ArgumentsNode*);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_expr;
        ArgumentsNode* m_args;
    };

    class EvalFunctionCallNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        EvalFunctionCallNode(JSGlobalData*, ArgumentsNode*, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ArgumentsNode* m_args;
    };

    class FunctionCallValueNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        FunctionCallValueNode(JSGlobalData*, ExpressionNode*, ArgumentsNode*, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_expr;
        ArgumentsNode* m_args;
    };

    class FunctionCallResolveNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        FunctionCallResolveNode(JSGlobalData*, const Identifier&, ArgumentsNode*, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        Identifier m_ident;
        ArgumentsNode* m_args;
        size_t m_index; // Used by LocalVarFunctionCallNode.
        size_t m_scopeDepth; // Used by ScopedVarFunctionCallNode and NonLocalVarFunctionCallNode
    };
    
    class FunctionCallBracketNode : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        FunctionCallBracketNode(JSGlobalData*, ExpressionNode* base, ExpressionNode* subscript, ArgumentsNode*, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_base;
        ExpressionNode* m_subscript;
        ArgumentsNode* m_args;
    };

    class FunctionCallDotNode : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        FunctionCallDotNode(JSGlobalData*, ExpressionNode* base, const Identifier&, ArgumentsNode*, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

    protected:
        ExpressionNode* m_base;
        const Identifier m_ident;
        ArgumentsNode* m_args;
    };

    class CallFunctionCallDotNode : public FunctionCallDotNode {
    public:
        CallFunctionCallDotNode(JSGlobalData*, ExpressionNode* base, const Identifier&, ArgumentsNode*, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);
    };
    
    class ApplyFunctionCallDotNode : public FunctionCallDotNode {
    public:
        ApplyFunctionCallDotNode(JSGlobalData*, ExpressionNode* base, const Identifier&, ArgumentsNode*, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);
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
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        Operator m_operator;
    };

    class PostfixBracketNode : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        PostfixBracketNode(JSGlobalData*, ExpressionNode* base, ExpressionNode* subscript, Operator, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_base;
        ExpressionNode* m_subscript;
        Operator m_operator;
    };

    class PostfixDotNode : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        PostfixDotNode(JSGlobalData*, ExpressionNode* base, const Identifier&, Operator, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_base;
        Identifier m_ident;
        Operator m_operator;
    };

    class PostfixErrorNode : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        PostfixErrorNode(JSGlobalData*, ExpressionNode*, Operator, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_expr;
        Operator m_operator;
    };

    class DeleteResolveNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        DeleteResolveNode(JSGlobalData*, const Identifier&, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        Identifier m_ident;
    };

    class DeleteBracketNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        DeleteBracketNode(JSGlobalData*, ExpressionNode* base, ExpressionNode* subscript, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_base;
        ExpressionNode* m_subscript;
    };

    class DeleteDotNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        DeleteDotNode(JSGlobalData*, ExpressionNode* base, const Identifier&, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_base;
        Identifier m_ident;
    };

    class DeleteValueNode : public ExpressionNode {
    public:
        DeleteValueNode(JSGlobalData*, ExpressionNode*);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_expr;
    };

    class VoidNode : public ExpressionNode {
    public:
        VoidNode(JSGlobalData*, ExpressionNode*);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_expr;
    };

    class TypeOfResolveNode : public ExpressionNode {
    public:
        TypeOfResolveNode(JSGlobalData*, const Identifier&);

        const Identifier& identifier() const { return m_ident; }

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        Identifier m_ident;
    };

    class TypeOfValueNode : public ExpressionNode {
    public:
        TypeOfValueNode(JSGlobalData*, ExpressionNode*);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_expr;
    };

    class PrefixResolveNode : public PrePostResolveNode {
    public:
        PrefixResolveNode(JSGlobalData*, const Identifier&, Operator, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        Operator m_operator;
    };

    class PrefixBracketNode : public ExpressionNode, public ThrowablePrefixedSubExpressionData {
    public:
        PrefixBracketNode(JSGlobalData*, ExpressionNode* base, ExpressionNode* subscript, Operator, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_base;
        ExpressionNode* m_subscript;
        Operator m_operator;
    };

    class PrefixDotNode : public ExpressionNode, public ThrowablePrefixedSubExpressionData {
    public:
        PrefixDotNode(JSGlobalData*, ExpressionNode* base, const Identifier&, Operator, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_base;
        Identifier m_ident;
        Operator m_operator;
    };

    class PrefixErrorNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        PrefixErrorNode(JSGlobalData*, ExpressionNode*, Operator, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_expr;
        Operator m_operator;
    };

    class UnaryOpNode : public ExpressionNode {
    public:
        UnaryOpNode(JSGlobalData*, ResultType, ExpressionNode*, OpcodeID);

    protected:
        ExpressionNode* expr() { return m_expr; }

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        OpcodeID opcodeID() const { return m_opcodeID; }

        ExpressionNode* m_expr;
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

        RegisterID* emitStrcat(BytecodeGenerator& generator, RegisterID* dst, RegisterID* lhs = 0, ReadModifyResolveNode* emitExpressionInfoForMe = 0);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

    protected:
        OpcodeID opcodeID() const { return m_opcodeID; }

    protected:
        ExpressionNode* m_expr1;
        ExpressionNode* m_expr2;
    private:
        OpcodeID m_opcodeID;
    protected:
        bool m_rightHasAssignments;
    };

    class ReverseBinaryOpNode : public BinaryOpNode {
    public:
        ReverseBinaryOpNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, OpcodeID, bool rightHasAssignments);
        ReverseBinaryOpNode(JSGlobalData*, ResultType, ExpressionNode* expr1, ExpressionNode* expr2, OpcodeID, bool rightHasAssignments);

        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);
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

        virtual bool isAdd() const { return true; }
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
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);
    };
    
    class InstanceOfNode : public ThrowableBinaryOpNode {
    public:
        InstanceOfNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);
    };

    class InNode : public ThrowableBinaryOpNode {
    public:
        InNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class EqualNode : public BinaryOpNode {
    public:
        EqualNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);
    };

    class NotEqualNode : public BinaryOpNode {
    public:
        NotEqualNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class StrictEqualNode : public BinaryOpNode {
    public:
        StrictEqualNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);
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

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_expr1;
        ExpressionNode* m_expr2;
        LogicalOperator m_operator;
    };

    // The ternary operator, "m_logical ? m_expr1 : m_expr2"
    class ConditionalNode : public ExpressionNode {
    public:
        ConditionalNode(JSGlobalData*, ExpressionNode* logical, ExpressionNode* expr1, ExpressionNode* expr2);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_logical;
        ExpressionNode* m_expr1;
        ExpressionNode* m_expr2;
    };

    class ReadModifyResolveNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        ReadModifyResolveNode(JSGlobalData*, const Identifier&, Operator, ExpressionNode*  right, bool rightHasAssignments, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        Identifier m_ident;
        ExpressionNode* m_right;
        size_t m_index; // Used by ReadModifyLocalVarNode.
        Operator m_operator;
        bool m_rightHasAssignments;
    };

    class AssignResolveNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        AssignResolveNode(JSGlobalData*, const Identifier&, ExpressionNode* right, bool rightHasAssignments);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        Identifier m_ident;
        ExpressionNode* m_right;
        size_t m_index; // Used by ReadModifyLocalVarNode.
        bool m_rightHasAssignments;
    };

    class ReadModifyBracketNode : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        ReadModifyBracketNode(JSGlobalData*, ExpressionNode* base, ExpressionNode* subscript, Operator, ExpressionNode* right, bool subscriptHasAssignments, bool rightHasAssignments, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_base;
        ExpressionNode* m_subscript;
        ExpressionNode* m_right;
        Operator m_operator : 30;
        bool m_subscriptHasAssignments : 1;
        bool m_rightHasAssignments : 1;
    };

    class AssignBracketNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        AssignBracketNode(JSGlobalData*, ExpressionNode* base, ExpressionNode* subscript, ExpressionNode* right, bool subscriptHasAssignments, bool rightHasAssignments, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_base;
        ExpressionNode* m_subscript;
        ExpressionNode* m_right;
        bool m_subscriptHasAssignments : 1;
        bool m_rightHasAssignments : 1;
    };

    class AssignDotNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        AssignDotNode(JSGlobalData*, ExpressionNode* base, const Identifier&, ExpressionNode* right, bool rightHasAssignments, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_base;
        Identifier m_ident;
        ExpressionNode* m_right;
        bool m_rightHasAssignments;
    };

    class ReadModifyDotNode : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        ReadModifyDotNode(JSGlobalData*, ExpressionNode* base, const Identifier&, Operator, ExpressionNode* right, bool rightHasAssignments, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_base;
        Identifier m_ident;
        ExpressionNode* m_right;
        Operator m_operator : 31;
        bool m_rightHasAssignments : 1;
    };

    class AssignErrorNode : public ExpressionNode, public ThrowableExpressionData {
    public:
        AssignErrorNode(JSGlobalData*, ExpressionNode* left, Operator, ExpressionNode* right, unsigned divot, unsigned startOffset, unsigned endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_left;
        Operator m_operator;
        ExpressionNode* m_right;
    };
    
    typedef Vector<ExpressionNode*, 8> ExpressionVector;

    class CommaNode : public ExpressionNode {
    public:
        CommaNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2);

        void append(ExpressionNode* expr) { m_expressions.append(expr); }

    private:
        virtual bool isCommaNode() const { return true; }
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionVector m_expressions;
    };
    
    class ConstDeclNode : public ExpressionNode {
    public:
        ConstDeclNode(JSGlobalData*, const Identifier&, ExpressionNode*);

        bool hasInitializer() const { return m_init; }
        const Identifier& ident() { return m_ident; }

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);
        virtual RegisterID* emitCodeSingle(BytecodeGenerator&);

        Identifier m_ident;

    public:
        ConstDeclNode* m_next;

    private:
        ExpressionNode* m_init;
    };

    class ConstStatementNode : public StatementNode {
    public:
        ConstStatementNode(JSGlobalData*, ConstDeclNode* next);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ConstDeclNode* m_next;
    };

    typedef Vector<StatementNode*> StatementVector;

    class SourceElements : public ParserArenaDeletable {
    public:
        SourceElements(JSGlobalData*);

        void append(StatementNode*);
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

        StatementVector& children() { return m_children; }

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        virtual bool isBlock() const { return true; }

        StatementVector m_children;
    };

    class EmptyStatementNode : public StatementNode {
    public:
        EmptyStatementNode(JSGlobalData*);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        virtual bool isEmptyStatement() const { return true; }
    };
    
    class DebuggerStatementNode : public StatementNode {
    public:
        DebuggerStatementNode(JSGlobalData*);
        
    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);
    };

    class ExprStatementNode : public StatementNode {
    public:
        ExprStatementNode(JSGlobalData*, ExpressionNode*);

        ExpressionNode* expr() const { return m_expr; }

    private:
        virtual bool isExprStatement() const { return true; }

        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_expr;
    };

    class VarStatementNode : public StatementNode {
    public:
        VarStatementNode(JSGlobalData*, ExpressionNode*);        

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_expr;
    };

    class IfNode : public StatementNode {
    public:
        IfNode(JSGlobalData*, ExpressionNode* condition, StatementNode* ifBlock);

    protected:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_condition;
        StatementNode* m_ifBlock;
    };

    class IfElseNode : public IfNode {
    public:
        IfElseNode(JSGlobalData*, ExpressionNode* condition, StatementNode* ifBlock, StatementNode* elseBlock);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        StatementNode* m_elseBlock;
    };

    class DoWhileNode : public StatementNode {
    public:
        DoWhileNode(JSGlobalData*, StatementNode* statement, ExpressionNode*);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        StatementNode* m_statement;
        ExpressionNode* m_expr;
    };

    class WhileNode : public StatementNode {
    public:
        WhileNode(JSGlobalData*, ExpressionNode*, StatementNode* statement);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_expr;
        StatementNode* m_statement;
    };

    class ForNode : public StatementNode {
    public:
        ForNode(JSGlobalData*, ExpressionNode* expr1, ExpressionNode* expr2, ExpressionNode* expr3, StatementNode* statement, bool expr1WasVarDecl);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_expr1;
        ExpressionNode* m_expr2;
        ExpressionNode* m_expr3;
        StatementNode* m_statement;
        bool m_expr1WasVarDecl;
    };

    class ForInNode : public StatementNode, public ThrowableExpressionData {
    public:
        ForInNode(JSGlobalData*, ExpressionNode*, ExpressionNode*, StatementNode*);
        ForInNode(JSGlobalData*, const Identifier&, ExpressionNode*, ExpressionNode*, StatementNode*, int divot, int startOffset, int endOffset);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        Identifier m_ident;
        ExpressionNode* m_init;
        ExpressionNode* m_lexpr;
        ExpressionNode* m_expr;
        StatementNode* m_statement;
        bool m_identIsVarDecl;
    };

    class ContinueNode : public StatementNode, public ThrowableExpressionData {
    public:
        ContinueNode(JSGlobalData*);
        ContinueNode(JSGlobalData*, const Identifier&);
        
    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        Identifier m_ident;
    };

    class BreakNode : public StatementNode, public ThrowableExpressionData {
    public:
        BreakNode(JSGlobalData*);
        BreakNode(JSGlobalData*, const Identifier&);
        
    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        Identifier m_ident;
    };

    class ReturnNode : public StatementNode, public ThrowableExpressionData {
    public:
        ReturnNode(JSGlobalData*, ExpressionNode* value);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        virtual bool isReturnNode() const { return true; }

        ExpressionNode* m_value;
    };

    class WithNode : public StatementNode {
    public:
        WithNode(JSGlobalData*, ExpressionNode*, StatementNode*, uint32_t divot, uint32_t expressionLength);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_expr;
        StatementNode* m_statement;
        uint32_t m_divot;
        uint32_t m_expressionLength;
    };

    class LabelNode : public StatementNode, public ThrowableExpressionData {
    public:
        LabelNode(JSGlobalData*, const Identifier& name, StatementNode*);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        Identifier m_name;
        StatementNode* m_statement;
    };

    class ThrowNode : public StatementNode, public ThrowableExpressionData {
    public:
        ThrowNode(JSGlobalData*, ExpressionNode*);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_expr;
    };

    class TryNode : public StatementNode {
    public:
        TryNode(JSGlobalData*, StatementNode* tryBlock, const Identifier& exceptionIdent, bool catchHasEval, StatementNode* catchBlock, StatementNode* finallyBlock);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* dst = 0);

        StatementNode* m_tryBlock;
        Identifier m_exceptionIdent;
        StatementNode* m_catchBlock;
        StatementNode* m_finallyBlock;
        bool m_catchHasEval;
    };

    class ParameterNode : public ParserArenaDeletable {
    public:
        ParameterNode(JSGlobalData*, const Identifier&);
        ParameterNode(JSGlobalData*, ParameterNode*, const Identifier&);

        const Identifier& ident() const { return m_ident; }
        ParameterNode* nextParam() const { return m_next; }

    private:
        Identifier m_ident;
        ParameterNode* m_next;
    };

    struct ScopeNodeData {
        typedef DeclarationStacks::VarStack VarStack;
        typedef DeclarationStacks::FunctionStack FunctionStack;

        ScopeNodeData(ParserArena&, SourceElements*, VarStack*, FunctionStack*, int numConstants);

        ParserArena m_arena;
        VarStack m_varStack;
        FunctionStack m_functionStack;
        int m_numConstants;
        StatementVector m_children;

        void mark();
    };

    class ScopeNode : public StatementNode, public ParserArenaRefCounted {
    public:
        typedef DeclarationStacks::VarStack VarStack;
        typedef DeclarationStacks::FunctionStack FunctionStack;

        ScopeNode(JSGlobalData*);
        ScopeNode(JSGlobalData*, const SourceCode&, SourceElements*, VarStack*, FunctionStack*, CodeFeatures, int numConstants);

        void adoptData(std::auto_ptr<ScopeNodeData> data)
        {
            ASSERT(!data->m_arena.contains(this));
            ASSERT(!m_data);
            m_data.adopt(data);
        }
        ScopeNodeData* data() const { return m_data.get(); }
        void destroyData() { m_data.clear(); }

        const SourceCode& source() const { return m_source; }
        const UString& sourceURL() const { return m_source.provider()->url(); }
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

#if ENABLE(JIT)
        JITCode& generatedJITCode()
        {
            ASSERT(m_jitCode);
            return m_jitCode;
        }

        ExecutablePool* getExecutablePool()
        {
            return m_jitCode.getExecutablePool();
        }

        void setJITCode(const JITCode jitCode)
        {
            m_jitCode = jitCode;
        }
#endif

    protected:
        void setSource(const SourceCode& source) { m_source = source; }

#if ENABLE(JIT)
        JITCode m_jitCode;
#endif

    private:
        OwnPtr<ScopeNodeData> m_data;
        CodeFeatures m_features;
        SourceCode m_source;
    };

    class ProgramNode : public ScopeNode {
    public:
        static PassRefPtr<ProgramNode> create(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, const SourceCode&, CodeFeatures, int numConstants);

        ProgramCodeBlock& bytecode(ScopeChainNode* scopeChain) 
        {
            if (!m_code)
                generateBytecode(scopeChain);
            return *m_code;
        }

#if ENABLE(JIT)
        JITCode& jitCode(ScopeChainNode* scopeChain)
        {
            if (!m_jitCode)
                generateJITCode(scopeChain);
            return m_jitCode;
        }
#endif

    private:
        ProgramNode(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, const SourceCode&, CodeFeatures, int numConstants);

        void generateBytecode(ScopeChainNode*);
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

#if ENABLE(JIT)
        void generateJITCode(ScopeChainNode*);
#endif

        OwnPtr<ProgramCodeBlock> m_code;
    };

    class EvalNode : public ScopeNode {
    public:
        static PassRefPtr<EvalNode> create(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, const SourceCode&, CodeFeatures, int numConstants);

        EvalCodeBlock& bytecode(ScopeChainNode* scopeChain) 
        {
            if (!m_code)
                generateBytecode(scopeChain);
            return *m_code;
        }

        EvalCodeBlock& bytecodeForExceptionInfoReparse(ScopeChainNode*, CodeBlock*);

        virtual void mark();

#if ENABLE(JIT)
        JITCode& jitCode(ScopeChainNode* scopeChain)
        {
            if (!m_jitCode)
                generateJITCode(scopeChain);
            return m_jitCode;
        }
#endif

    private:
        EvalNode(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, const SourceCode&, CodeFeatures, int numConstants);

        void generateBytecode(ScopeChainNode*);
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

#if ENABLE(JIT)
        void generateJITCode(ScopeChainNode*);
#endif
        
        OwnPtr<EvalCodeBlock> m_code;
    };

    class FunctionBodyNode : public ScopeNode {
        friend class JIT;
    public:
#if ENABLE(JIT)
        static PassRefPtr<FunctionBodyNode> createNativeThunk(JSGlobalData*);
#endif
        static FunctionBodyNode* create(JSGlobalData*);
        static PassRefPtr<FunctionBodyNode> create(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, const SourceCode&, CodeFeatures, int numConstants);
        virtual ~FunctionBodyNode();

        const Identifier* parameters() const { return m_parameters; }
        size_t parameterCount() const { return m_parameterCount; }
        UString paramString() const ;
        Identifier* copyParameters();

        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        bool isGenerated() const
        {
            return m_code;
        }

        bool isHostFunction() const;

        virtual void mark();

        void finishParsing(const SourceCode&, ParameterNode*);
        void finishParsing(Identifier* parameters, size_t parameterCount);
        
        UString toSourceString() const { return source().toString(); }

        CodeBlock& bytecodeForExceptionInfoReparse(ScopeChainNode*, CodeBlock*);
#if ENABLE(JIT)
        JITCode& jitCode(ScopeChainNode* scopeChain)
        {
            if (!m_jitCode)
                generateJITCode(scopeChain);
            return m_jitCode;
        }
#endif

        CodeBlock& bytecode(ScopeChainNode* scopeChain) 
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
        FunctionBodyNode(JSGlobalData*);
        FunctionBodyNode(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, const SourceCode&, CodeFeatures, int numConstants);

        void generateBytecode(ScopeChainNode*);
#if ENABLE(JIT)
        void generateJITCode(ScopeChainNode*);
#endif
        Identifier* m_parameters;
        size_t m_parameterCount;
        OwnPtr<CodeBlock> m_code;
    };

    class FuncExprNode : public ExpressionNode, public ParserArenaRefCounted {
    public:
        FuncExprNode(JSGlobalData*, const Identifier&, FunctionBodyNode* body, const SourceCode& source, ParameterNode* parameter = 0);

        JSFunction* makeFunction(ExecState*, ScopeChainNode*);

        FunctionBodyNode* body() { return m_body.get(); }

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        virtual bool isFuncExprNode() const { return true; } 

        Identifier m_ident;
        RefPtr<FunctionBodyNode> m_body;
    };

    class FuncDeclNode : public StatementNode, public ParserArenaRefCounted {
    public:
        FuncDeclNode(JSGlobalData*, const Identifier&, FunctionBodyNode*, const SourceCode&, ParameterNode* = 0);

        JSFunction* makeFunction(ExecState*, ScopeChainNode*);

        Identifier m_ident;

        FunctionBodyNode* body() { return m_body.get(); }

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        RefPtr<FunctionBodyNode> m_body;
    };

    class CaseClauseNode : public ParserArenaDeletable {
    public:
        CaseClauseNode(JSGlobalData*, ExpressionNode*);
        CaseClauseNode(JSGlobalData*, ExpressionNode*, SourceElements*);

        ExpressionNode* expr() const { return m_expr; }
        StatementVector& children() { return m_children; }

    private:
        ExpressionNode* m_expr;
        StatementVector m_children;
    };

    class ClauseListNode : public ParserArenaDeletable {
    public:
        ClauseListNode(JSGlobalData*, CaseClauseNode*);
        ClauseListNode(JSGlobalData*, ClauseListNode*, CaseClauseNode*);

        CaseClauseNode* getClause() const { return m_clause; }
        ClauseListNode* getNext() const { return m_next; }

    private:
        CaseClauseNode* m_clause;
        ClauseListNode* m_next;
    };

    class CaseBlockNode : public ParserArenaDeletable {
    public:
        CaseBlockNode(JSGlobalData*, ClauseListNode* list1, CaseClauseNode* defaultClause, ClauseListNode* list2);

        RegisterID* emitBytecodeForBlock(BytecodeGenerator&, RegisterID* input, RegisterID* dst = 0);

    private:
        SwitchInfo::SwitchType tryOptimizedSwitch(Vector<ExpressionNode*, 8>& literalVector, int32_t& min_num, int32_t& max_num);
        ClauseListNode* m_list1;
        CaseClauseNode* m_defaultClause;
        ClauseListNode* m_list2;
    };

    class SwitchNode : public StatementNode {
    public:
        SwitchNode(JSGlobalData*, ExpressionNode*, CaseBlockNode*);

    private:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = 0);

        ExpressionNode* m_expr;
        CaseBlockNode* m_block;
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
