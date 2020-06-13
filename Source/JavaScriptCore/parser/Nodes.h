/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2019 Apple Inc. All rights reserved.
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

#pragma once

#include "BytecodeIntrinsicRegistry.h"
#include "JITCode.h"
#include "Label.h"
#include "ParserArena.h"
#include "ParserModes.h"
#include "ParserTokens.h"
#include "ResultType.h"
#include "SourceCode.h"
#include "SymbolTable.h"
#include "VariableEnvironment.h"
#include <wtf/MathExtras.h>
#include <wtf/SmallPtrSet.h>

namespace JSC {

    enum OpcodeID : unsigned;

    class ArgumentListNode;
    class BytecodeGenerator;
    class FunctionMetadataNode;
    class FunctionParameters;
    class ModuleAnalyzer;
    class ModuleScopeData;
    class PropertyListNode;
    class ReadModifyResolveNode;
    class RegisterID;
    class ScopeNode;

    typedef SmallPtrSet<UniquedStringImpl*> UniquedStringImplPtrSet;

    enum class Operator : uint8_t {
        Equal,
        PlusEq,
        MinusEq,
        MultEq,
        DivEq,
        PlusPlus,
        MinusMinus,
        BitAndEq,
        BitXOrEq,
        BitOrEq,
        ModEq,
        PowEq,
        CoalesceEq,
        OrEq,
        AndEq,
        LShift,
        RShift,
        URShift
    };
    
    enum class LogicalOperator : uint8_t {
        And,
        Or
    };

    enum FallThroughMode : uint8_t {
        FallThroughMeansTrue = 0,
        FallThroughMeansFalse = 1
    };
    inline FallThroughMode invert(FallThroughMode fallThroughMode) { return static_cast<FallThroughMode>(!fallThroughMode); }

    namespace DeclarationStacks {
        typedef Vector<FunctionMetadataNode*> FunctionStack;
    }

    struct SwitchInfo {
        enum SwitchType : uint8_t { SwitchNone, SwitchImmediate, SwitchCharacter, SwitchString };
        uint32_t bytecodeOffset;
        SwitchType switchType;
    };

    enum class AssignmentContext : uint8_t { 
        DeclarationStatement, 
        ConstDeclarationStatement, 
        AssignmentExpression 
    };

    class ParserArenaFreeable {
    public:
        // ParserArenaFreeable objects are freed when the arena is deleted.
        // Destructors are not called. Clients must not call delete on such objects.
        void* operator new(size_t, ParserArena&);
    };

    class ParserArenaDeletable {
    public:
        virtual ~ParserArenaDeletable() { }

        // ParserArenaDeletable objects are deleted when the arena is deleted.
        // Clients must not call delete directly on such objects.
        template<typename T> void* operator new(size_t, ParserArena&);
    };

#define JSC_MAKE_PARSER_ARENA_DELETABLE_ALLOCATED_IMPL(__classToNew) \
        void* operator new(size_t size, ParserArena& parserArena) \
        { \
            return ParserArenaDeletable::operator new<__classToNew>(size, parserArena); \
        }

#define JSC_MAKE_PARSER_ARENA_DELETABLE_ALLOCATED(__classToNew) \
    public: \
        JSC_MAKE_PARSER_ARENA_DELETABLE_ALLOCATED_IMPL(__classToNew) \
    private: \
        typedef int __thisIsHereToForceASemicolonAfterThisMacro

    DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(ParserArenaRoot);
    class ParserArenaRoot {
        WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(ParserArenaRoot);
    protected:
        ParserArenaRoot(ParserArena&);

    public:
        ParserArena& parserArena() { return m_arena; }
        virtual ~ParserArenaRoot() { }

    protected:
        ParserArena m_arena;
    };

    class Node : public ParserArenaFreeable {
    protected:
        Node(const JSTokenLocation&);

    public:
        virtual ~Node() { }

        int firstLine() const { return m_position.line; }
        int startOffset() const { return m_position.offset; }
        int endOffset() const { return m_endOffset; }
        int lineStartOffset() const { return m_position.lineStartOffset; }
        const JSTextPosition& position() const { return m_position; }
        void setEndOffset(int offset) { m_endOffset = offset; }
        void setStartOffset(int offset) { m_position.offset = offset; }

        bool needsDebugHook() const { return m_needsDebugHook; }
        void setNeedsDebugHook() { m_needsDebugHook = true; }

    protected:
        JSTextPosition m_position;
        int m_endOffset { -1 };
        bool m_needsDebugHook { false };
    };

    class ExpressionNode : public Node {
    protected:
        ExpressionNode(const JSTokenLocation&, ResultType = ResultType::unknownType());

    public:
        virtual RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* destination = nullptr) = 0;

        virtual bool isNumber() const { return false; }
        virtual bool isString() const { return false; }
        virtual bool isBigInt() const { return false; }
        virtual bool isObjectLiteral() const { return false; }
        virtual bool isArrayLiteral() const { return false; }
        virtual bool isNull() const { return false; }
        virtual bool isPure(BytecodeGenerator&) const { return false; }        
        virtual bool isConstant() const { return false; }
        virtual bool isLocation() const { return false; }
        virtual bool isPrivateLocation() const { return false; }
        virtual bool isAssignmentLocation() const { return isLocation(); }
        virtual bool isResolveNode() const { return false; }
        virtual bool isAssignResolveNode() const { return false; }
        virtual bool isBracketAccessorNode() const { return false; }
        virtual bool isDotAccessorNode() const { return false; }
        virtual bool isDestructuringNode() const { return false; }
        virtual bool isBaseFuncExprNode() const { return false; }
        virtual bool isFuncExprNode() const { return false; }
        virtual bool isArrowFuncExprNode() const { return false; }
        virtual bool isClassExprNode() const { return false; }
        virtual bool isCommaNode() const { return false; }
        virtual bool isSimpleArray() const { return false; }
        virtual bool isAdd() const { return false; }
        virtual bool isSubtract() const { return false; }
        virtual bool isBoolean() const { return false; }
        virtual bool isThisNode() const { return false; }
        virtual bool isSpreadExpression() const { return false; }
        virtual bool isSuperNode() const { return false; }
        virtual bool isImportNode() const { return false; }
        virtual bool isMetaProperty() const { return false; }
        virtual bool isNewTarget() const { return false; }
        virtual bool isImportMeta() const { return false; }
        virtual bool isBytecodeIntrinsicNode() const { return false; }
        virtual bool isBinaryOpNode() const { return false; }
        virtual bool isFunctionCall() const { return false; }
        virtual bool isDeleteNode() const { return false; }
        virtual bool isOptionalChain() const { return false; }

        virtual void emitBytecodeInConditionContext(BytecodeGenerator&, Label&, Label&, FallThroughMode);

        virtual ExpressionNode* stripUnaryPlus() { return this; }

        ResultType resultDescriptor() const { return m_resultType; }

        bool isOnlyChildOfStatement() const { return m_isOnlyChildOfStatement; }
        void setIsOnlyChildOfStatement() { m_isOnlyChildOfStatement = true; }

        bool isOptionalChainBase() const { return m_isOptionalChainBase; }
        void setIsOptionalChainBase() { m_isOptionalChainBase = true; }

    private:
        ResultType m_resultType;
        bool m_isOnlyChildOfStatement { false };
        bool m_isOptionalChainBase { false };
    };

    class StatementNode : public Node {
    protected:
        StatementNode(const JSTokenLocation&);

    public:
        virtual void emitBytecode(BytecodeGenerator&, RegisterID* destination = nullptr) = 0;

        void setLoc(unsigned firstLine, unsigned lastLine, int startOffset, int lineStartOffset);
        unsigned lastLine() const { return m_lastLine; }

        StatementNode* next() const { return m_next; }
        void setNext(StatementNode* next) { m_next = next; }

        virtual bool hasCompletionValue() const { return true; }
        virtual bool hasEarlyBreakOrContinue() const { return false; }

        virtual bool isEmptyStatement() const { return false; }
        virtual bool isDebuggerStatement() const { return false; }
        virtual bool isFunctionNode() const { return false; }
        virtual bool isReturnNode() const { return false; }
        virtual bool isExprStatement() const { return false; }
        virtual bool isBreak() const { return false; }
        virtual bool isContinue() const { return false; }
        virtual bool isLabel() const { return false; }
        virtual bool isBlock() const { return false; }
        virtual bool isFuncDeclNode() const { return false; }
        virtual bool isModuleDeclarationNode() const { return false; }
        virtual bool isForOfNode() const { return false; }
        virtual bool isDefineFieldNode() const { return false; }

    protected:
        int m_lastLine { -1 };
        StatementNode* m_next { nullptr };
    };

    class VariableEnvironmentNode : public ParserArenaDeletable {
        JSC_MAKE_PARSER_ARENA_DELETABLE_ALLOCATED(VariableEnvironmentNode);
    public:
        typedef DeclarationStacks::FunctionStack FunctionStack;

        VariableEnvironmentNode()
        {
        }

        VariableEnvironmentNode(VariableEnvironment& lexicalDeclaredVariables);
        VariableEnvironmentNode(VariableEnvironment& lexicalDeclaredVariables, FunctionStack&&);

        VariableEnvironment& lexicalVariables() { return m_lexicalVariables; }
        FunctionStack& functionStack() { return m_functionStack; }

    protected:
        VariableEnvironment m_lexicalVariables;
        FunctionStack m_functionStack;
    };

    class ConstantNode : public ExpressionNode {
    public:
        ConstantNode(const JSTokenLocation&, ResultType);
        bool isPure(BytecodeGenerator&) const override { return true; }
        bool isConstant() const  override { return true; }
        virtual JSValue jsValue(BytecodeGenerator&) const = 0;
    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) override;
        void emitBytecodeInConditionContext(BytecodeGenerator&, Label& trueTarget, Label& falseTarget, FallThroughMode) override;
    };

    class NullNode final : public ConstantNode {
    public:
        NullNode(const JSTokenLocation&);

    private:
        bool isNull() const final { return true; }
        JSValue jsValue(BytecodeGenerator&) const final { return jsNull(); }
    };

    class BooleanNode final : public ConstantNode {
    public:
        BooleanNode(const JSTokenLocation&, bool value);
        bool value() { return m_value; }

    private:
        bool isBoolean() const final { return true; }
        JSValue jsValue(BytecodeGenerator&) const final { return jsBoolean(m_value); }

        bool m_value;
    };

    class NumberNode : public ConstantNode {
    public:
        NumberNode(const JSTokenLocation&, double value);
        double value() const { return m_value; }
        virtual bool isIntegerNode() const = 0;
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

    private:
        bool isNumber() const final { return true; }
        JSValue jsValue(BytecodeGenerator&) const override { return jsNumber(m_value); }

        double m_value;
    };

    class DoubleNode : public NumberNode {
    public:
        DoubleNode(const JSTokenLocation&, double value);

    private:
        bool isIntegerNode() const override { return false; }
    };

    // An integer node represent a number represented as an integer (e.g. 42 instead of 42., 42.0, 42e0)
    class IntegerNode final : public DoubleNode {
    public:
        IntegerNode(const JSTokenLocation&, double value);
        bool isIntegerNode() const final { return true; }
    };

    class StringNode final : public ConstantNode {
    public:
        StringNode(const JSTokenLocation&, const Identifier&);
        const Identifier& value() { return m_value; }

    private:
        bool isString() const final { return true; }
        JSValue jsValue(BytecodeGenerator&) const final;

        const Identifier& m_value;
    };

    class BigIntNode final : public ConstantNode {
    public:
        BigIntNode(const JSTokenLocation&, const Identifier&, uint8_t radix);
        BigIntNode(const JSTokenLocation&, const Identifier&, uint8_t radix, bool sign);
        const Identifier& value() { return m_value; }

        const Identifier& identifier() const { return m_value; }
        uint8_t radix() const { return m_radix; }
        bool sign() const { return m_sign; }

    private:
        bool isBigInt() const final { return true; }
        JSValue jsValue(BytecodeGenerator&) const final;

        const Identifier& m_value;
        const uint8_t m_radix;
        const bool m_sign;
    };

    class ThrowableExpressionData {
    public:
        ThrowableExpressionData() = default;
        
        ThrowableExpressionData(const JSTextPosition& divot, const JSTextPosition& start, const JSTextPosition& end)
            : m_divot(divot)
            , m_divotStart(start)
            , m_divotEnd(end)
        {
            checkConsistency();
        }

        void setExceptionSourceCode(const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd)
        {
            m_divot = divot;
            m_divotStart = divotStart;
            m_divotEnd = divotEnd;
            checkConsistency();
        }

        const JSTextPosition& divot() const { return m_divot; }
        const JSTextPosition& divotStart() const { return m_divotStart; }
        const JSTextPosition& divotEnd() const { return m_divotEnd; }

        void checkConsistency() const
        {
            ASSERT(m_divot.offset >= m_divot.lineStartOffset);
            ASSERT(m_divotStart.offset >= m_divotStart.lineStartOffset);
            ASSERT(m_divotEnd.offset >= m_divotEnd.lineStartOffset);
            ASSERT(m_divot.offset >= m_divotStart.offset);
            ASSERT(m_divotEnd.offset >= m_divot.offset);
        }
    protected:
        RegisterID* emitThrowReferenceError(BytecodeGenerator&, const String& message, RegisterID* dst = nullptr);

    private:
        JSTextPosition m_divot;
        JSTextPosition m_divotStart;
        JSTextPosition m_divotEnd;
    };

    class ThrowableSubExpressionData : public ThrowableExpressionData {
    public:
        ThrowableSubExpressionData()
            : m_subexpressionDivotOffset(0)
            , m_subexpressionEndOffset(0)
            , m_subexpressionLineOffset(0)
            , m_subexpressionLineStartOffset(0)
        {
        }

        ThrowableSubExpressionData(const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd)
            : ThrowableExpressionData(divot, divotStart, divotEnd)
            , m_subexpressionDivotOffset(0)
            , m_subexpressionEndOffset(0)
            , m_subexpressionLineOffset(0)
            , m_subexpressionLineStartOffset(0)
        {
        }

        void setSubexpressionInfo(const JSTextPosition& subexpressionDivot, int subexpressionOffset)
        {
            ASSERT(subexpressionDivot.offset <= divot().offset);
            // Overflow means we can't do this safely, so just point at the primary divot,
            // divotLine, or divotLineStart.
            if ((divot() - subexpressionDivot.offset) & ~0xFFFF)
                return;
            if ((divot().line - subexpressionDivot.line) & ~0xFFFF)
                return;
            if ((divot().lineStartOffset - subexpressionDivot.lineStartOffset) & ~0xFFFF)
                return;
            if ((divotEnd() - subexpressionOffset) & ~0xFFFF)
                return;
            m_subexpressionDivotOffset = divot() - subexpressionDivot.offset;
            m_subexpressionEndOffset = divotEnd() - subexpressionOffset;
            m_subexpressionLineOffset = divot().line - subexpressionDivot.line;
            m_subexpressionLineStartOffset = divot().lineStartOffset - subexpressionDivot.lineStartOffset;
        }

        JSTextPosition subexpressionDivot()
        {
            int newLine = divot().line - m_subexpressionLineOffset;
            int newOffset = divot().offset - m_subexpressionDivotOffset;
            int newLineStartOffset = divot().lineStartOffset - m_subexpressionLineStartOffset;
            return JSTextPosition(newLine, newOffset, newLineStartOffset);
        }
        JSTextPosition subexpressionStart() { return divotStart(); }
        JSTextPosition subexpressionEnd() { return divotEnd() - static_cast<int>(m_subexpressionEndOffset); }

    protected:
        uint16_t m_subexpressionDivotOffset;
        uint16_t m_subexpressionEndOffset;
        uint16_t m_subexpressionLineOffset;
        uint16_t m_subexpressionLineStartOffset;
    };
    
    class ThrowablePrefixedSubExpressionData : public ThrowableExpressionData {
    public:
        ThrowablePrefixedSubExpressionData()
            : m_subexpressionDivotOffset(0)
            , m_subexpressionStartOffset(0)
            , m_subexpressionLineOffset(0)
            , m_subexpressionLineStartOffset(0)
        {
        }

        ThrowablePrefixedSubExpressionData(const JSTextPosition& divot, const JSTextPosition& start, const JSTextPosition& end)
            : ThrowableExpressionData(divot, start, end)
            , m_subexpressionDivotOffset(0)
            , m_subexpressionStartOffset(0)
            , m_subexpressionLineOffset(0)
            , m_subexpressionLineStartOffset(0)
        {
        }

        void setSubexpressionInfo(const JSTextPosition& subexpressionDivot, int subexpressionOffset)
        {
            ASSERT(subexpressionDivot.offset >= divot().offset);
            // Overflow means we can't do this safely, so just point at the primary divot,
            // divotLine, or divotLineStart.
            if ((subexpressionDivot.offset - divot()) & ~0xFFFF) 
                return;
            if ((subexpressionDivot.line - divot().line) & ~0xFFFF)
                return;
            if ((subexpressionDivot.lineStartOffset - divot().lineStartOffset) & ~0xFFFF)
                return;
            if ((subexpressionOffset - divotStart()) & ~0xFFFF) 
                return;
            m_subexpressionDivotOffset = subexpressionDivot.offset - divot();
            m_subexpressionStartOffset = subexpressionOffset - divotStart();
            m_subexpressionLineOffset = subexpressionDivot.line - divot().line;
            m_subexpressionLineStartOffset = subexpressionDivot.lineStartOffset - divot().lineStartOffset;
        }

        JSTextPosition subexpressionDivot()
        {
            int newLine = divot().line + m_subexpressionLineOffset;
            int newOffset = divot().offset + m_subexpressionDivotOffset;
            int newLineStartOffset = divot().lineStartOffset + m_subexpressionLineStartOffset;
            return JSTextPosition(newLine, newOffset, newLineStartOffset);
        }
        JSTextPosition subexpressionStart() { return divotStart() + static_cast<int>(m_subexpressionStartOffset); }
        JSTextPosition subexpressionEnd() { return divotEnd(); }

    protected:
        uint16_t m_subexpressionDivotOffset;
        uint16_t m_subexpressionStartOffset;
        uint16_t m_subexpressionLineOffset;
        uint16_t m_subexpressionLineStartOffset;
    };

    class TemplateExpressionListNode final : public ParserArenaFreeable {
    public:
        TemplateExpressionListNode(ExpressionNode*);
        TemplateExpressionListNode(TemplateExpressionListNode*, ExpressionNode*);

        ExpressionNode* value() { return m_node; }
        TemplateExpressionListNode* next() { return m_next; }

    private:
        TemplateExpressionListNode* m_next { nullptr };
        ExpressionNode* m_node { nullptr };
    };

    class TemplateStringNode final : public ExpressionNode {
    public:
        TemplateStringNode(const JSTokenLocation&, const Identifier* cooked, const Identifier* raw);

        const Identifier* cooked() { return m_cooked; }
        const Identifier* raw() { return m_raw; }

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        const Identifier* m_cooked;
        const Identifier* m_raw;
    };

    class TemplateStringListNode final : public ParserArenaFreeable {
    public:
        TemplateStringListNode(TemplateStringNode*);
        TemplateStringListNode(TemplateStringListNode*, TemplateStringNode*);

        TemplateStringNode* value() { return m_node; }
        TemplateStringListNode* next() { return m_next; }

    private:
        TemplateStringListNode* m_next { nullptr };
        TemplateStringNode* m_node { nullptr };
    };

    class TemplateLiteralNode final : public ExpressionNode {
    public:
        TemplateLiteralNode(const JSTokenLocation&, TemplateStringListNode*);
        TemplateLiteralNode(const JSTokenLocation&, TemplateStringListNode*, TemplateExpressionListNode*);

        TemplateStringListNode* templateStrings() const { return m_templateStrings; }
        TemplateExpressionListNode* templateExpressions() const { return m_templateExpressions; }

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        TemplateStringListNode* m_templateStrings;
        TemplateExpressionListNode* m_templateExpressions;
    };

    class TaggedTemplateNode final : public ExpressionNode, public ThrowableExpressionData {
    public:
        TaggedTemplateNode(const JSTokenLocation&, ExpressionNode*, TemplateLiteralNode*);

        TemplateLiteralNode* templateLiteral() const { return m_templateLiteral; }

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* m_tag;
        TemplateLiteralNode* m_templateLiteral;
    };

    class RegExpNode final : public ExpressionNode, public ThrowableExpressionData {
    public:
        RegExpNode(const JSTokenLocation&, const Identifier& pattern, const Identifier& flags);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        const Identifier& m_pattern;
        const Identifier& m_flags;
    };

    class ThisNode final : public ExpressionNode {
    public:
        ThisNode(const JSTokenLocation&);

    private:
        bool isThisNode() const final { return true; }
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
    };

    class SuperNode final : public ExpressionNode {
    public:
        SuperNode(const JSTokenLocation&);

    private:
        bool isSuperNode() const final { return true; }
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
    };

    class ImportNode final : public ExpressionNode, public ThrowableExpressionData {
    public:
        ImportNode(const JSTokenLocation&, ExpressionNode*);

    private:
        bool isImportNode() const final { return true; }
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* m_expr;
    };

    class MetaPropertyNode : public ExpressionNode {
    public:
        MetaPropertyNode(const JSTokenLocation&);

    private:
        bool isMetaProperty() const final { return true; }
    };

    class NewTargetNode final : public MetaPropertyNode {
    public:
        NewTargetNode(const JSTokenLocation&);

    private:
        bool isNewTarget() const final { return true; }
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
    };

    class ImportMetaNode final : public MetaPropertyNode {
    public:
        ImportMetaNode(const JSTokenLocation&, ExpressionNode*);

    private:
        bool isImportMeta() const final { return true; }
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* m_expr;
    };

    class ResolveNode final : public ExpressionNode {
    public:
        ResolveNode(const JSTokenLocation&, const Identifier&, const JSTextPosition& start);

        const Identifier& identifier() const { return m_ident; }

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        bool isPure(BytecodeGenerator&) const final;
        bool isLocation() const final { return true; }
        bool isResolveNode() const final { return true; }

        const Identifier& m_ident;
        JSTextPosition m_start;
    };

    class ElementNode final : public ParserArenaFreeable {
    public:
        ElementNode(int elision, ExpressionNode*);
        ElementNode(ElementNode*, int elision, ExpressionNode*);

        int elision() const { return m_elision; }
        ExpressionNode* value() { return m_node; }
        ElementNode* next() { return m_next; }

    private:
        ElementNode* m_next { nullptr };
        ExpressionNode* m_node;
        int m_elision;
    };

    class ArrayNode final : public ExpressionNode {
    public:
        ArrayNode(const JSTokenLocation&, int elision);
        ArrayNode(const JSTokenLocation&, ElementNode*);
        ArrayNode(const JSTokenLocation&, int elision, ElementNode*);

        bool isArrayLiteral() const final { return true; }

        ArgumentListNode* toArgumentList(ParserArena&, int, int) const;

        ElementNode* elements() const { return m_element; }
    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        bool isSimpleArray() const final;

        ElementNode* m_element;
        int m_elision;
        bool m_optional;
    };

    enum class ClassElementTag : uint8_t { No, Instance, Static, LastTag };
    class PropertyNode final : public ParserArenaFreeable {
    public:
        enum Type : uint8_t { Constant = 1, Getter = 2, Setter = 4, Computed = 8, Shorthand = 16, Spread = 32, Private = 64 };
        enum PutType : uint8_t { Unknown, KnownDirect };

        PropertyNode(const Identifier&, ExpressionNode*, Type, PutType, SuperBinding, ClassElementTag);
        PropertyNode(ExpressionNode*, Type, PutType, SuperBinding, ClassElementTag);
        PropertyNode(ExpressionNode* propertyName, ExpressionNode*, Type, PutType, SuperBinding, ClassElementTag);
        PropertyNode(const Identifier&, ExpressionNode* propertyName, ExpressionNode*, Type, PutType, SuperBinding, ClassElementTag);

        ExpressionNode* expressionName() const { return m_expression; }
        const Identifier* name() const { return m_name; }

        Type type() const { return static_cast<Type>(m_type); }
        bool needsSuperBinding() const { return m_needsSuperBinding; }
        bool isClassProperty() const { return static_cast<ClassElementTag>(m_classElementTag) != ClassElementTag::No; }
        bool isStaticClassProperty() const { return static_cast<ClassElementTag>(m_classElementTag) == ClassElementTag::Static; }
        bool isInstanceClassProperty() const { return static_cast<ClassElementTag>(m_classElementTag) == ClassElementTag::Instance; }
        bool isClassField() const { return isClassProperty() && !needsSuperBinding(); }
        bool isInstanceClassField() const { return isInstanceClassProperty() && !needsSuperBinding(); }
        bool isOverriddenByDuplicate() const { return m_isOverriddenByDuplicate; }
        bool isPrivate() const { return m_type & Private; }
        bool hasComputedName() const { return m_expression; }
        bool isComputedClassField() const { return isClassField() && hasComputedName(); }
        void setIsOverriddenByDuplicate() { m_isOverriddenByDuplicate = true; }
        PutType putType() const { return static_cast<PutType>(m_putType); }

    private:
        friend class PropertyListNode;
        const Identifier* m_name;
        ExpressionNode* m_expression;
        ExpressionNode* m_assign;
        unsigned m_type : 7;
        unsigned m_needsSuperBinding : 1;
        unsigned m_putType : 1;
        static_assert(1 << 2 > static_cast<unsigned>(ClassElementTag::LastTag), "ClassElementTag shouldn't use more than two bits");
        unsigned m_classElementTag : 2;
        unsigned m_isOverriddenByDuplicate : 1;
    };

    class PropertyListNode final : public ExpressionNode {
    public:
        PropertyListNode(const JSTokenLocation&, PropertyNode*);
        PropertyListNode(const JSTokenLocation&, PropertyNode*, PropertyListNode*);

        bool hasStaticallyNamedProperty(const Identifier& propName);
        bool isComputedClassField() const
        {
            return m_node->isComputedClassField();
        }
        bool isInstanceClassField() const
        {
            return m_node->isInstanceClassField();
        }
        bool hasInstanceFields() const;

        static bool shouldCreateLexicalScopeForClass(PropertyListNode*);

        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID*, RegisterID*, Vector<JSTextPosition>*);

        void emitDeclarePrivateFieldNames(BytecodeGenerator&, RegisterID* scope);

    private:
        RegisterID* emitBytecode(BytecodeGenerator& generator, RegisterID* dst = nullptr) final
        {
            return emitBytecode(generator, dst, nullptr, nullptr);
        }
        void emitPutConstantProperty(BytecodeGenerator&, RegisterID*, PropertyNode&);
        void emitSaveComputedFieldName(BytecodeGenerator&, PropertyNode&);

        PropertyNode* m_node;
        PropertyListNode* m_next { nullptr };
    };

    class ObjectLiteralNode final : public ExpressionNode {
    public:
        ObjectLiteralNode(const JSTokenLocation&);
        ObjectLiteralNode(const JSTokenLocation&, PropertyListNode*);
        bool isObjectLiteral() const final { return true; }

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        PropertyListNode* m_list;
    };
    
    class BracketAccessorNode final : public ExpressionNode, public ThrowableExpressionData {
    public:
        BracketAccessorNode(const JSTokenLocation&, ExpressionNode* base, ExpressionNode* subscript, bool subscriptHasAssignments);

        ExpressionNode* base() const { return m_base; }
        ExpressionNode* subscript() const { return m_subscript; }

        bool subscriptHasAssignments() const { return m_subscriptHasAssignments; }

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        bool isLocation() const final { return true; }
        bool isBracketAccessorNode() const final { return true; }

        ExpressionNode* m_base;
        ExpressionNode* m_subscript;
        bool m_subscriptHasAssignments;
    };

    enum class DotType { Name, PrivateField };
    class BaseDotNode : public ExpressionNode {
    public:
        BaseDotNode(const JSTokenLocation&, ExpressionNode* base, const Identifier&, DotType);

        ExpressionNode* base() const { return m_base; }
        const Identifier& identifier() const { return m_ident; }
        DotType type() const { return m_type; }
        bool isPrivateField() const { return m_type == DotType::PrivateField; }

        RegisterID* emitGetPropertyValue(BytecodeGenerator&, RegisterID* dst, RegisterID* base, RefPtr<RegisterID>& thisValue);
        RegisterID* emitGetPropertyValue(BytecodeGenerator&, RegisterID* dst, RegisterID* base);
        RegisterID* emitPutProperty(BytecodeGenerator&, RegisterID* base, RegisterID* value, RefPtr<RegisterID>& thisValue);
        RegisterID* emitPutProperty(BytecodeGenerator&, RegisterID* base, RegisterID* value);

    protected:
        ExpressionNode* m_base;
        const Identifier& m_ident;
        DotType m_type;
    };

    class DotAccessorNode final : public BaseDotNode, public ThrowableExpressionData {
    public:
        DotAccessorNode(const JSTokenLocation&, ExpressionNode* base, const Identifier&, DotType);

        ExpressionNode* base() const { return m_base; }
        const Identifier& identifier() const { return m_ident; }

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        bool isLocation() const final { return true; }
        bool isPrivateLocation() const override { return m_type == DotType::PrivateField; }
        bool isDotAccessorNode() const final { return true; }
    };

    class SpreadExpressionNode final : public ExpressionNode, public ThrowableExpressionData {
    public:
        SpreadExpressionNode(const JSTokenLocation&, ExpressionNode*);
        
        ExpressionNode* expression() const { return m_expression; }
        
    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
        
        bool isSpreadExpression() const final { return true; }
        ExpressionNode* m_expression;
    };
    
    class ObjectSpreadExpressionNode final : public ExpressionNode, public ThrowableExpressionData {
    public:
        ObjectSpreadExpressionNode(const JSTokenLocation&, ExpressionNode*);
        
        ExpressionNode* expression() const { return m_expression; }
        
    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
        
        ExpressionNode* m_expression;
    };

    class ArgumentListNode final : public ExpressionNode {
    public:
        ArgumentListNode(const JSTokenLocation&, ExpressionNode*);
        ArgumentListNode(const JSTokenLocation&, ArgumentListNode*, ExpressionNode*);

        ArgumentListNode* m_next { nullptr };
        ExpressionNode* m_expr;

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
    };

    class ArgumentsNode final : public ParserArenaFreeable {
    public:
        ArgumentsNode();
        ArgumentsNode(ArgumentListNode*);

        ArgumentListNode* m_listNode;
    };

    class NewExprNode final : public ExpressionNode, public ThrowableExpressionData {
    public:
        NewExprNode(const JSTokenLocation&, ExpressionNode*);
        NewExprNode(const JSTokenLocation&, ExpressionNode*, ArgumentsNode*);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* m_expr;
        ArgumentsNode* m_args;
    };

    class EvalFunctionCallNode final : public ExpressionNode, public ThrowableExpressionData {
    public:
        EvalFunctionCallNode(const JSTokenLocation&, ArgumentsNode*, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        bool isFunctionCall() const final { return true; }

        ArgumentsNode* m_args;
    };

    class FunctionCallValueNode final : public ExpressionNode, public ThrowableExpressionData {
    public:
        FunctionCallValueNode(const JSTokenLocation&, ExpressionNode*, ArgumentsNode*, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        bool isFunctionCall() const final { return true; }

        ExpressionNode* m_expr;
        ArgumentsNode* m_args;
    };

    class FunctionCallResolveNode final : public ExpressionNode, public ThrowableExpressionData {
    public:
        FunctionCallResolveNode(const JSTokenLocation&, const Identifier&, ArgumentsNode*, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        bool isFunctionCall() const final { return true; }

        const Identifier& m_ident;
        ArgumentsNode* m_args;
    };
    
    class FunctionCallBracketNode final : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        FunctionCallBracketNode(const JSTokenLocation&, ExpressionNode* base, ExpressionNode* subscript, bool subscriptHasAssignments, ArgumentsNode*, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        bool isFunctionCall() const final { return true; }

        ExpressionNode* m_base;
        ExpressionNode* m_subscript;
        ArgumentsNode* m_args;
        bool m_subscriptHasAssignments;
    };

    class FunctionCallDotNode : public BaseDotNode, public ThrowableSubExpressionData {
    public:
        FunctionCallDotNode(const JSTokenLocation&, ExpressionNode* base, const Identifier&, DotType, ArgumentsNode*, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) override;

    protected:
        bool isFunctionCall() const override { return true; }

        ArgumentsNode* m_args;
    };

    class BytecodeIntrinsicNode final : public ExpressionNode, public ThrowableExpressionData {
    public:
        enum class Type : uint8_t {
            Constant,
            Function
        };

        BytecodeIntrinsicNode(Type, const JSTokenLocation&, BytecodeIntrinsicRegistry::Entry, const Identifier&, ArgumentsNode*, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

        bool isBytecodeIntrinsicNode() const final { return true; }

        Type type() const { return m_type; }
        BytecodeIntrinsicRegistry::Entry entry() const { return m_entry; }
        const Identifier& identifier() const { return m_ident; }

#define JSC_DECLARE_BYTECODE_INTRINSIC_FUNCTIONS(name) RegisterID* emit_intrinsic_##name(BytecodeGenerator&, RegisterID*);
        JSC_COMMON_BYTECODE_INTRINSIC_FUNCTIONS_EACH_NAME(JSC_DECLARE_BYTECODE_INTRINSIC_FUNCTIONS)
        JSC_COMMON_BYTECODE_INTRINSIC_CONSTANTS_EACH_NAME(JSC_DECLARE_BYTECODE_INTRINSIC_FUNCTIONS)
#undef JSC_DECLARE_BYTECODE_INTRINSIC_FUNCTIONS

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        bool isFunctionCall() const final { return m_type == Type::Function; }

        BytecodeIntrinsicRegistry::Entry m_entry;
        const Identifier& m_ident;
        ArgumentsNode* m_args;
        Type m_type;
    };

    class CallFunctionCallDotNode final : public FunctionCallDotNode {
    public:
        CallFunctionCallDotNode(const JSTokenLocation&, ExpressionNode* base, const Identifier&, DotType, ArgumentsNode*, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd, size_t distanceToInnermostCallOrApply);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
        size_t m_distanceToInnermostCallOrApply;
    };
    
    class ApplyFunctionCallDotNode final : public FunctionCallDotNode {
    public:
        ApplyFunctionCallDotNode(const JSTokenLocation&, ExpressionNode* base, const Identifier&, DotType, ArgumentsNode*, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd, size_t distanceToInnermostCallOrApply);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
        size_t m_distanceToInnermostCallOrApply;
    };

    class HasOwnPropertyFunctionCallDotNode final : public FunctionCallDotNode {
    public:
        HasOwnPropertyFunctionCallDotNode(const JSTokenLocation&, ExpressionNode* base, const Identifier&, DotType, ArgumentsNode*, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
    };

    class DeleteResolveNode final : public ExpressionNode, public ThrowableExpressionData {
    public:
        DeleteResolveNode(const JSTokenLocation&, const Identifier&, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        bool isDeleteNode() const final { return true; }

        const Identifier& m_ident;
    };

    class DeleteBracketNode final : public ExpressionNode, public ThrowableExpressionData {
    public:
        DeleteBracketNode(const JSTokenLocation&, ExpressionNode* base, ExpressionNode* subscript, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        bool isDeleteNode() const final { return true; }

        ExpressionNode* m_base;
        ExpressionNode* m_subscript;
    };

    class DeleteDotNode final : public ExpressionNode, public ThrowableExpressionData {
    public:
        DeleteDotNode(const JSTokenLocation&, ExpressionNode* base, const Identifier&, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        bool isDeleteNode() const final { return true; }

        ExpressionNode* m_base;
        const Identifier& m_ident;
    };

    class DeleteValueNode final : public ExpressionNode {
    public:
        DeleteValueNode(const JSTokenLocation&, ExpressionNode*);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        bool isDeleteNode() const final { return true; }

        ExpressionNode* m_expr;
    };

    class VoidNode final : public ExpressionNode {
    public:
        VoidNode(const JSTokenLocation&, ExpressionNode*);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* m_expr;
    };

    class TypeOfResolveNode final : public ExpressionNode {
    public:
        TypeOfResolveNode(const JSTokenLocation&, const Identifier&);

        const Identifier& identifier() const { return m_ident; }

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        const Identifier& m_ident;
    };

    class TypeOfValueNode final : public ExpressionNode {
    public:
        TypeOfValueNode(const JSTokenLocation&, ExpressionNode*);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* m_expr;
    };

    class PrefixNode : public ExpressionNode, public ThrowablePrefixedSubExpressionData {
    public:
        PrefixNode(const JSTokenLocation&, ExpressionNode*, Operator, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

    protected:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) override;
        virtual RegisterID* emitResolve(BytecodeGenerator&, RegisterID* = nullptr);
        virtual RegisterID* emitBracket(BytecodeGenerator&, RegisterID* = nullptr);
        virtual RegisterID* emitDot(BytecodeGenerator&, RegisterID* = nullptr);

        ExpressionNode* m_expr;
        Operator m_operator;
    };

    class PostfixNode final : public PrefixNode {
    public:
        PostfixNode(const JSTokenLocation&, ExpressionNode*, Operator, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
        RegisterID* emitResolve(BytecodeGenerator&, RegisterID* = nullptr) final;
        RegisterID* emitBracket(BytecodeGenerator&, RegisterID* = nullptr) final;
        RegisterID* emitDot(BytecodeGenerator&, RegisterID* = nullptr) final;
    };

    class UnaryOpNode : public ExpressionNode {
    public:
        UnaryOpNode(const JSTokenLocation&, ResultType, ExpressionNode*, OpcodeID);

    protected:
        ExpressionNode* expr() { return m_expr; }
        const ExpressionNode* expr() const { return m_expr; }
        OpcodeID opcodeID() const { return m_opcodeID; }

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) override;

        ExpressionNode* m_expr;
        OpcodeID m_opcodeID;
    };

    class UnaryPlusNode final : public UnaryOpNode {
    public:
        UnaryPlusNode(const JSTokenLocation&, ExpressionNode*);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* stripUnaryPlus() final { return expr(); }
    };

    class NegateNode final : public UnaryOpNode {
    public:
        NegateNode(const JSTokenLocation&, ExpressionNode*);
    };

    class BitwiseNotNode final : public UnaryOpNode {
    public:
        BitwiseNotNode(const JSTokenLocation&, ExpressionNode*);
    };
 
    class LogicalNotNode final : public UnaryOpNode {
    public:
        LogicalNotNode(const JSTokenLocation&, ExpressionNode*);
    private:
        void emitBytecodeInConditionContext(BytecodeGenerator&, Label& trueTarget, Label& falseTarget, FallThroughMode) final;
    };

    class BinaryOpNode : public ExpressionNode {
    public:
        BinaryOpNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, OpcodeID, bool rightHasAssignments);
        BinaryOpNode(const JSTokenLocation&, ResultType, ExpressionNode* expr1, ExpressionNode* expr2, OpcodeID, bool rightHasAssignments);

        RegisterID* emitStrcat(BytecodeGenerator&, RegisterID* destination, RegisterID* lhs = nullptr, ReadModifyResolveNode* emitExpressionInfoForMe = nullptr);
        void emitBytecodeInConditionContext(BytecodeGenerator&, Label& trueTarget, Label& falseTarget, FallThroughMode) override;

        ExpressionNode* lhs() { return m_expr1; };
        ExpressionNode* rhs() { return m_expr2; };

        bool isBinaryOpNode() const override { return true; }

    private:
        enum class UInt32Result : uint8_t { UInt32, Constant, };

        void tryFoldToBranch(BytecodeGenerator&, TriState& branchCondition, ExpressionNode*& branchExpression);
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) override;

    protected:
        OpcodeID opcodeID() const { return m_opcodeID; }

    protected:
        bool m_rightHasAssignments;
        bool m_shouldToUnsignedResult { true };
    private:
        OpcodeID m_opcodeID;
    protected:
        ExpressionNode* m_expr1;
        ExpressionNode* m_expr2;
    };

    class PowNode final : public BinaryOpNode {
    public:
        PowNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class MultNode final : public BinaryOpNode {
    public:
        MultNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class DivNode final : public BinaryOpNode {
    public:
        DivNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class ModNode final : public BinaryOpNode {
    public:
        ModNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class AddNode final : public BinaryOpNode {
    public:
        AddNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);

        bool isAdd() const final { return true; }
    };

    class SubNode final : public BinaryOpNode {
    public:
        SubNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);

        bool isSubtract() const final { return true; }
    };

    class LeftShiftNode final : public BinaryOpNode {
    public:
        LeftShiftNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class RightShiftNode final : public BinaryOpNode {
    public:
        RightShiftNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class UnsignedRightShiftNode final : public BinaryOpNode {
    public:
        UnsignedRightShiftNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class LessNode final : public BinaryOpNode {
    public:
        LessNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class GreaterNode final : public BinaryOpNode {
    public:
        GreaterNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class LessEqNode final : public BinaryOpNode {
    public:
        LessEqNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class GreaterEqNode final : public BinaryOpNode {
    public:
        GreaterEqNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class ThrowableBinaryOpNode : public BinaryOpNode, public ThrowableExpressionData {
    public:
        ThrowableBinaryOpNode(const JSTokenLocation&, ResultType, ExpressionNode* expr1, ExpressionNode* expr2, OpcodeID, bool rightHasAssignments);
        ThrowableBinaryOpNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, OpcodeID, bool rightHasAssignments);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) override;
    };
    
    class InstanceOfNode final : public ThrowableBinaryOpNode {
    public:
        InstanceOfNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
    };

    class InNode final : public ThrowableBinaryOpNode {
    public:
        InNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
    };

    class EqualNode final : public BinaryOpNode {
    public:
        EqualNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
    };

    class NotEqualNode final : public BinaryOpNode {
    public:
        NotEqualNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class StrictEqualNode final : public BinaryOpNode {
    public:
        StrictEqualNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
    };

    class NotStrictEqualNode final : public BinaryOpNode {
    public:
        NotStrictEqualNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class BitAndNode final : public BinaryOpNode {
    public:
        BitAndNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class BitOrNode final : public BinaryOpNode {
    public:
        BitOrNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    class BitXOrNode final : public BinaryOpNode {
    public:
        BitXOrNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments);
    };

    // m_expr1 && m_expr2, m_expr1 || m_expr2
    class LogicalOpNode final : public ExpressionNode {
    public:
        LogicalOpNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, LogicalOperator);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
        void emitBytecodeInConditionContext(BytecodeGenerator&, Label& trueTarget, Label& falseTarget, FallThroughMode) final;

        LogicalOperator m_operator;
        ExpressionNode* m_expr1;
        ExpressionNode* m_expr2;
    };

    class CoalesceNode final : public ExpressionNode {
    public:
        CoalesceNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, bool);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* m_expr1;
        ExpressionNode* m_expr2;
        bool m_hasAbsorbedOptionalChain;
    };

    class OptionalChainNode final : public ExpressionNode {
    public:
        OptionalChainNode(const JSTokenLocation&, ExpressionNode*, bool);

        void setExpr(ExpressionNode* expr) { m_expr = expr; }
        ExpressionNode* expr() const { return m_expr; }

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        bool isOptionalChain() const final { return true; }

        ExpressionNode* m_expr;
        bool m_isOutermost;
    };

    // The ternary operator, "m_logical ? m_expr1 : m_expr2"
    class ConditionalNode final : public ExpressionNode {
    public:
        ConditionalNode(const JSTokenLocation&, ExpressionNode* logical, ExpressionNode* expr1, ExpressionNode* expr2);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* m_logical;
        ExpressionNode* m_expr1;
        ExpressionNode* m_expr2;
    };

    class ReadModifyResolveNode final : public ExpressionNode, public ThrowableExpressionData {
    public:
        ReadModifyResolveNode(const JSTokenLocation&, const Identifier&, Operator, ExpressionNode*  right, bool rightHasAssignments, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        const Identifier& m_ident;
        ExpressionNode* m_right;
        Operator m_operator;
        bool m_rightHasAssignments;
    };

    class ShortCircuitReadModifyResolveNode final : public ExpressionNode, public ThrowableExpressionData {
    public:
        ShortCircuitReadModifyResolveNode(const JSTokenLocation&, const Identifier&, Operator, ExpressionNode*  right, bool rightHasAssignments, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        const Identifier& m_ident;
        ExpressionNode* m_right;
        Operator m_operator;
        bool m_rightHasAssignments;
    };

    class AssignResolveNode final : public ExpressionNode, public ThrowableExpressionData {
    public:
        AssignResolveNode(const JSTokenLocation&, const Identifier&, ExpressionNode* right, AssignmentContext);
        bool isAssignResolveNode() const final { return true; }
        const Identifier& identifier() const { return m_ident; }

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        const Identifier& m_ident;
        ExpressionNode* m_right;
        AssignmentContext m_assignmentContext;
    };

    class ReadModifyBracketNode final : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        ReadModifyBracketNode(const JSTokenLocation&, ExpressionNode* base, ExpressionNode* subscript, Operator, ExpressionNode* right, bool subscriptHasAssignments, bool rightHasAssignments, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* m_base;
        ExpressionNode* m_subscript;
        ExpressionNode* m_right;
        Operator m_operator;
        bool m_subscriptHasAssignments : 1;
        bool m_rightHasAssignments : 1;
    };

    class ShortCircuitReadModifyBracketNode final : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        ShortCircuitReadModifyBracketNode(const JSTokenLocation&, ExpressionNode* base, ExpressionNode* subscript, Operator, ExpressionNode* right, bool subscriptHasAssignments, bool rightHasAssignments, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* m_base;
        ExpressionNode* m_subscript;
        ExpressionNode* m_right;
        Operator m_operator;
        bool m_subscriptHasAssignments : 1;
        bool m_rightHasAssignments : 1;
    };

    class AssignBracketNode final : public ExpressionNode, public ThrowableExpressionData {
    public:
        AssignBracketNode(const JSTokenLocation&, ExpressionNode* base, ExpressionNode* subscript, ExpressionNode* right, bool subscriptHasAssignments, bool rightHasAssignments, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* m_base;
        ExpressionNode* m_subscript;
        ExpressionNode* m_right;
        bool m_subscriptHasAssignments : 1;
        bool m_rightHasAssignments : 1;
    };

    class AssignDotNode final : public BaseDotNode, public ThrowableExpressionData {
    public:
        AssignDotNode(const JSTokenLocation&, ExpressionNode* base, const Identifier&, DotType, ExpressionNode* right, bool rightHasAssignments, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* m_right;
        bool m_rightHasAssignments;
    };

    class ReadModifyDotNode final : public BaseDotNode, public ThrowableSubExpressionData {
    public:
        ReadModifyDotNode(const JSTokenLocation&, ExpressionNode* base, const Identifier&, DotType, Operator, ExpressionNode* right, bool rightHasAssignments, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* m_right;
        Operator m_operator;
        bool m_rightHasAssignments : 1;
    };

    class ShortCircuitReadModifyDotNode final : public ExpressionNode, public ThrowableSubExpressionData {
    public:
        ShortCircuitReadModifyDotNode(const JSTokenLocation&, ExpressionNode* base, const Identifier&, Operator, ExpressionNode* right, bool rightHasAssignments, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* m_base;
        const Identifier& m_ident;
        ExpressionNode* m_right;
        Operator m_operator;
        bool m_rightHasAssignments : 1;
    };

    class AssignErrorNode final : public ExpressionNode, public ThrowableExpressionData {
    public:
        AssignErrorNode(const JSTokenLocation&, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
    };
    
    class CommaNode final : public ExpressionNode {
    public:
        CommaNode(const JSTokenLocation&, ExpressionNode*);

        void setNext(CommaNode* next) { m_next = next; }
        CommaNode* next() { return m_next; }

    private:
        bool isCommaNode() const final { return true; }
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* m_expr;
        CommaNode* m_next { nullptr };
    };
    
    class SourceElements final : public ParserArenaFreeable {
    public:
        SourceElements();

        void append(StatementNode*);

        StatementNode* singleStatement() const;
        StatementNode* lastStatement() const;

        bool hasCompletionValue() const;
        bool hasEarlyBreakOrContinue() const;

        void emitBytecode(BytecodeGenerator&, RegisterID* destination);
        void analyzeModule(ModuleAnalyzer&);

    private:
        StatementNode* m_head { nullptr };
        StatementNode* m_tail { nullptr };
    };

    class BlockNode final : public StatementNode, public VariableEnvironmentNode {
        JSC_MAKE_PARSER_ARENA_DELETABLE_ALLOCATED(BlockNode);
    public:
        BlockNode(const JSTokenLocation&, SourceElements*, VariableEnvironment&, FunctionStack&&);

        StatementNode* singleStatement() const;
        StatementNode* lastStatement() const;

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        bool hasCompletionValue() const final;
        bool hasEarlyBreakOrContinue() const final;

        bool isBlock() const final { return true; }

        SourceElements* m_statements;
    };

    class EmptyStatementNode final : public StatementNode {
    public:
        EmptyStatementNode(const JSTokenLocation&);

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        bool hasCompletionValue() const final { return false; }
        bool isEmptyStatement() const final { return true; }
    };
    
    class DebuggerStatementNode final : public StatementNode {
    public:
        DebuggerStatementNode(const JSTokenLocation&);

        bool hasCompletionValue() const final { return false; }
        bool isDebuggerStatement() const final { return true; }
        
    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
    };

    class ExprStatementNode final : public StatementNode {
    public:
        ExprStatementNode(const JSTokenLocation&, ExpressionNode*);

        ExpressionNode* expr() const { return m_expr; }

    private:
        bool isExprStatement() const final { return true; }

        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* m_expr;
    };

    class DeclarationStatement final : public StatementNode {
    public:
        DeclarationStatement(const JSTokenLocation&, ExpressionNode*);
    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        bool hasCompletionValue() const final { return false; }

        ExpressionNode* m_expr;
    };

    class EmptyVarExpression final : public ExpressionNode {
    public:
        EmptyVarExpression(const JSTokenLocation&, const Identifier&);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        const Identifier& m_ident;
    };

    class EmptyLetExpression final : public ExpressionNode {
    public:
        EmptyLetExpression(const JSTokenLocation&, const Identifier&);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        const Identifier& m_ident;
    };

    class IfElseNode final : public StatementNode {
    public:
        IfElseNode(const JSTokenLocation&, ExpressionNode* condition, StatementNode* ifBlock, StatementNode* elseBlock);

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
        bool tryFoldBreakAndContinue(BytecodeGenerator&, StatementNode* ifBlock,
            Label*& trueTarget, FallThroughMode&);

        ExpressionNode* m_condition;
        StatementNode* m_ifBlock;
        StatementNode* m_elseBlock;
    };

    class DoWhileNode final : public StatementNode {
    public:
        DoWhileNode(const JSTokenLocation&, StatementNode*, ExpressionNode*);

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        StatementNode* m_statement;
        ExpressionNode* m_expr;
    };

    class WhileNode final : public StatementNode {
    public:
        WhileNode(const JSTokenLocation&, ExpressionNode*, StatementNode*);

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* m_expr;
        StatementNode* m_statement;
    };

    class ForNode final : public StatementNode, public VariableEnvironmentNode {
        JSC_MAKE_PARSER_ARENA_DELETABLE_ALLOCATED(ForNode);
    public:
        ForNode(const JSTokenLocation&, ExpressionNode* expr1, ExpressionNode* expr2, ExpressionNode* expr3, StatementNode*, VariableEnvironment&);

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* m_expr1;
        ExpressionNode* m_expr2;
        ExpressionNode* m_expr3;
        StatementNode* m_statement;
    };
    
    class DestructuringPatternNode;
    
    class EnumerationNode : public StatementNode, public ThrowableExpressionData, public VariableEnvironmentNode {
        JSC_MAKE_PARSER_ARENA_DELETABLE_ALLOCATED(EnumerationNode);
    public:
        EnumerationNode(const JSTokenLocation&, ExpressionNode*, ExpressionNode*, StatementNode*, VariableEnvironment&);

        ExpressionNode* lexpr() const { return m_lexpr; }
        ExpressionNode* expr() const { return m_expr; }

    protected:
        ExpressionNode* m_lexpr;
        ExpressionNode* m_expr;
        StatementNode* m_statement;
    };
    
    class ForInNode final : public EnumerationNode {
        JSC_MAKE_PARSER_ARENA_DELETABLE_ALLOCATED(ForInNode);
    public:
        ForInNode(const JSTokenLocation&, ExpressionNode*, ExpressionNode*, StatementNode*, VariableEnvironment&);

    private:
        RegisterID* tryGetBoundLocal(BytecodeGenerator&);
        void emitLoopHeader(BytecodeGenerator&, RegisterID* propertyName);

        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
    };
    
    class ForOfNode final : public EnumerationNode {
        JSC_MAKE_PARSER_ARENA_DELETABLE_ALLOCATED(ForOfNode);
    public:
        ForOfNode(bool, const JSTokenLocation&, ExpressionNode*, ExpressionNode*, StatementNode*, VariableEnvironment&);
        bool isForOfNode() const final { return true; }
        bool isForAwait() const { return m_isForAwait; }

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        const bool m_isForAwait;
    };

    class ContinueNode final : public StatementNode, public ThrowableExpressionData {
    public:
        ContinueNode(const JSTokenLocation&, const Identifier&);
        Label* trivialTarget(BytecodeGenerator&);
        
    private:
        bool hasCompletionValue() const final { return false; }
        bool isContinue() const final { return true; }
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        const Identifier& m_ident;
    };

    class BreakNode final : public StatementNode, public ThrowableExpressionData {
    public:
        BreakNode(const JSTokenLocation&, const Identifier&);
        Label* trivialTarget(BytecodeGenerator&);
        
    private:
        bool hasCompletionValue() const final { return false; }
        bool isBreak() const final { return true; }
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        const Identifier& m_ident;
    };

    class ReturnNode final : public StatementNode, public ThrowableExpressionData {
    public:
        ReturnNode(const JSTokenLocation&, ExpressionNode* value);

        ExpressionNode* value() { return m_value; }

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        bool isReturnNode() const final { return true; }

        ExpressionNode* m_value;
    };

    class WithNode final : public StatementNode {
    public:
        WithNode(const JSTokenLocation&, ExpressionNode*, StatementNode*, const JSTextPosition& divot, uint32_t expressionLength);

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* m_expr;
        StatementNode* m_statement;
        JSTextPosition m_divot;
        uint32_t m_expressionLength;
    };

    class LabelNode final : public StatementNode, public ThrowableExpressionData {
    public:
        LabelNode(const JSTokenLocation&, const Identifier& name, StatementNode*);

        bool isLabel() const final { return true; }

    private:
        bool hasCompletionValue() const final { return m_statement->hasCompletionValue(); }
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        const Identifier& m_name;
        StatementNode* m_statement;
    };

    class ThrowNode final : public StatementNode, public ThrowableExpressionData {
    public:
        ThrowNode(const JSTokenLocation&, ExpressionNode*);

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* m_expr;
    };

    class TryNode final : public StatementNode, public VariableEnvironmentNode {
        JSC_MAKE_PARSER_ARENA_DELETABLE_ALLOCATED(TryNode);
    public:
        TryNode(const JSTokenLocation&, StatementNode* tryBlock, DestructuringPatternNode* catchPattern, StatementNode* catchBlock, VariableEnvironment& catchEnvironment, StatementNode* finallyBlock);

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        StatementNode* m_tryBlock;
        DestructuringPatternNode* m_catchPattern;
        StatementNode* m_catchBlock;
        StatementNode* m_finallyBlock;
    };

    class ScopeNode : public StatementNode, public ParserArenaRoot, public VariableEnvironmentNode {
    public:
        // ScopeNode is never directly instantiate. The life-cycle of its derived classes are
        // managed using std::unique_ptr. Hence, though ScopeNode extends VariableEnvironmentNode,
        // which in turn extends ParserArenaDeletable, we don't want to use ParserArenaDeletable's
        // new for allocation.
        using ParserArenaRoot::operator new;

        ScopeNode(ParserArena&, const JSTokenLocation& start, const JSTokenLocation& end, bool inStrictContext);
        ScopeNode(ParserArena&, const JSTokenLocation& start, const JSTokenLocation& end, const SourceCode&, SourceElements*, VariableEnvironment&, FunctionStack&&, VariableEnvironment&, UniquedStringImplPtrSet&&, CodeFeatures, InnerArrowFunctionCodeFeatures, int numConstants);

        const SourceCode& source() const { return m_source; }
        const String& sourceURL() const { return m_source.provider()->url().string(); }
        intptr_t sourceID() const { return m_source.providerID(); }

        int startLine() const { return m_startLineNumber; }
        int startStartOffset() const { return m_startStartOffset; }
        int startLineStartOffset() const { return m_startLineStartOffset; }

        void setFeatures(CodeFeatures features) { m_features = features; }
        CodeFeatures features() { return m_features; }
        InnerArrowFunctionCodeFeatures innerArrowFunctionCodeFeatures() { return m_innerArrowFunctionCodeFeatures; }
        bool doAnyInnerArrowFunctionsUseAnyFeature() { return m_innerArrowFunctionCodeFeatures != NoInnerArrowFunctionFeatures; }
        bool doAnyInnerArrowFunctionsUseArguments() { return m_innerArrowFunctionCodeFeatures & ArgumentsInnerArrowFunctionFeature; }
        bool doAnyInnerArrowFunctionsUseSuperCall() { return m_innerArrowFunctionCodeFeatures & SuperCallInnerArrowFunctionFeature; }
        bool doAnyInnerArrowFunctionsUseSuperProperty() { return m_innerArrowFunctionCodeFeatures & SuperPropertyInnerArrowFunctionFeature; }
        bool doAnyInnerArrowFunctionsUseEval() { return m_innerArrowFunctionCodeFeatures & EvalInnerArrowFunctionFeature; }
        bool doAnyInnerArrowFunctionsUseThis() { return m_innerArrowFunctionCodeFeatures & ThisInnerArrowFunctionFeature; }
        bool doAnyInnerArrowFunctionsUseNewTarget() { return m_innerArrowFunctionCodeFeatures & NewTargetInnerArrowFunctionFeature; }

        bool usesEval() const { return m_features & EvalFeature; }
        bool usesArguments() const { return (m_features & ArgumentsFeature) && !(m_features & ShadowsArgumentsFeature); }
        bool usesArrowFunction() const { return m_features & ArrowFunctionFeature; }
        bool isStrictMode() const { return m_features & StrictModeFeature; }
        void setUsesArguments() { m_features |= ArgumentsFeature; }
        bool usesThis() const { return m_features & ThisFeature; }
        bool usesSuperCall() const { return m_features & SuperCallFeature; }
        bool usesSuperProperty() const { return m_features & SuperPropertyFeature; }
        bool usesNewTarget() const { return m_features & NewTargetFeature; }
        bool needsActivation() const { return (hasCapturedVariables()) || (m_features & (EvalFeature | WithFeature)); }
        bool hasCapturedVariables() const { return m_varDeclarations.hasCapturedVariables(); }
        bool captures(UniquedStringImpl* uid) { return m_varDeclarations.captures(uid); }
        bool captures(const Identifier& ident) { return captures(ident.impl()); }
        bool hasSloppyModeHoistedFunction(UniquedStringImpl* uid) const { return m_sloppyModeHoistedFunctions.contains(uid); }

        bool needsNewTargetRegisterForThisScope() const
        {
            return usesSuperCall() || usesNewTarget();
        }

        VariableEnvironment& varDeclarations() { return m_varDeclarations; }

        int neededConstants()
        {
            // We may need 2 more constants than the count given by the parser,
            // because of the various uses of jsUndefined() and jsNull().
            return m_numConstants + 2;
        }

        StatementNode* singleStatement() const;

        bool hasCompletionValue() const override;
        bool hasEarlyBreakOrContinue() const override;

        void emitStatementsBytecode(BytecodeGenerator&, RegisterID* destination);
        
        void analyzeModule(ModuleAnalyzer&);

    protected:
        int m_startLineNumber;
        unsigned m_startStartOffset;
        unsigned m_startLineStartOffset;

    private:
        CodeFeatures m_features;
        InnerArrowFunctionCodeFeatures m_innerArrowFunctionCodeFeatures;
        SourceCode m_source;
        VariableEnvironment m_varDeclarations;
        UniquedStringImplPtrSet m_sloppyModeHoistedFunctions;
        int m_numConstants;
        SourceElements* m_statements;
    };

    class ProgramNode final : public ScopeNode {
    public:
        ProgramNode(ParserArena&, const JSTokenLocation& start, const JSTokenLocation& end, unsigned startColumn, unsigned endColumn, SourceElements*, VariableEnvironment&, FunctionStack&&, VariableEnvironment&, UniquedStringImplPtrSet&&, FunctionParameters*, const SourceCode&, CodeFeatures, InnerArrowFunctionCodeFeatures, int numConstants, RefPtr<ModuleScopeData>&&);

        unsigned startColumn() const { return m_startColumn; }
        unsigned endColumn() const { return m_endColumn; }

        static constexpr bool scopeIsFunction = false;

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
        unsigned m_startColumn;
        unsigned m_endColumn;
    };

    class EvalNode final : public ScopeNode {
    public:
        EvalNode(ParserArena&, const JSTokenLocation& start, const JSTokenLocation& end, unsigned startColumn, unsigned endColumn, SourceElements*, VariableEnvironment&, FunctionStack&&, VariableEnvironment&, UniquedStringImplPtrSet&&, FunctionParameters*, const SourceCode&, CodeFeatures, InnerArrowFunctionCodeFeatures, int numConstants, RefPtr<ModuleScopeData>&&);

        ALWAYS_INLINE unsigned startColumn() const { return 0; }
        unsigned endColumn() const { return m_endColumn; }

        static constexpr bool scopeIsFunction = false;

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        unsigned m_endColumn;
    };

    class ModuleProgramNode final : public ScopeNode {
    public:
        ModuleProgramNode(ParserArena&, const JSTokenLocation& start, const JSTokenLocation& end, unsigned startColumn, unsigned endColumn, SourceElements*, VariableEnvironment&, FunctionStack&&, VariableEnvironment&, UniquedStringImplPtrSet&&, FunctionParameters*, const SourceCode&, CodeFeatures, InnerArrowFunctionCodeFeatures, int numConstants, RefPtr<ModuleScopeData>&&);

        unsigned startColumn() const { return m_startColumn; }
        unsigned endColumn() const { return m_endColumn; }

        static constexpr bool scopeIsFunction = false;

        ModuleScopeData& moduleScopeData()
        {
            return m_moduleScopeData;
        }

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
        unsigned m_startColumn;
        unsigned m_endColumn;
        Ref<ModuleScopeData> m_moduleScopeData;
    };

    class ModuleNameNode final : public Node {
    public:
        ModuleNameNode(const JSTokenLocation&, const Identifier& moduleName);

        const Identifier& moduleName() { return m_moduleName; }

    private:
        const Identifier& m_moduleName;
    };

    class ImportSpecifierNode final : public Node {
    public:
        ImportSpecifierNode(const JSTokenLocation&, const Identifier& importedName, const Identifier& localName);

        const Identifier& importedName() { return m_importedName; }
        const Identifier& localName() { return m_localName; }

    private:
        const Identifier& m_importedName;
        const Identifier& m_localName;
    };

    class ImportSpecifierListNode final : public ParserArenaDeletable {
        JSC_MAKE_PARSER_ARENA_DELETABLE_ALLOCATED(ImportSpecifierListNode);
    public:
        typedef Vector<ImportSpecifierNode*, 3> Specifiers;

        const Specifiers& specifiers() const { return m_specifiers; }
        void append(ImportSpecifierNode* specifier)
        {
            m_specifiers.append(specifier);
        }

    private:
        Specifiers m_specifiers;
    };

    class ModuleDeclarationNode : public StatementNode {
    public:
        virtual void analyzeModule(ModuleAnalyzer&) = 0;
        bool hasCompletionValue() const override { return false; }
        bool isModuleDeclarationNode() const override { return true; }

    protected:
        ModuleDeclarationNode(const JSTokenLocation&);
    };

    class ImportDeclarationNode final : public ModuleDeclarationNode {
    public:
        ImportDeclarationNode(const JSTokenLocation&, ImportSpecifierListNode*, ModuleNameNode*);

        ImportSpecifierListNode* specifierList() const { return m_specifierList; }
        ModuleNameNode* moduleName() const { return m_moduleName; }

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
        void analyzeModule(ModuleAnalyzer&) final;

        ImportSpecifierListNode* m_specifierList;
        ModuleNameNode* m_moduleName;
    };

    class ExportAllDeclarationNode final : public ModuleDeclarationNode {
    public:
        ExportAllDeclarationNode(const JSTokenLocation&, ModuleNameNode*);

        ModuleNameNode* moduleName() const { return m_moduleName; }

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
        void analyzeModule(ModuleAnalyzer&) final;

        ModuleNameNode* m_moduleName;
    };

    class ExportDefaultDeclarationNode final : public ModuleDeclarationNode {
    public:
        ExportDefaultDeclarationNode(const JSTokenLocation&, StatementNode*, const Identifier& localName);

        const StatementNode& declaration() const { return *m_declaration; }
        const Identifier& localName() const { return m_localName; }

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
        void analyzeModule(ModuleAnalyzer&) final;
        StatementNode* m_declaration;
        const Identifier& m_localName;
    };

    class ExportLocalDeclarationNode final : public ModuleDeclarationNode {
    public:
        ExportLocalDeclarationNode(const JSTokenLocation&, StatementNode*);

        const StatementNode& declaration() const { return *m_declaration; }

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
        void analyzeModule(ModuleAnalyzer&) final;
        StatementNode* m_declaration;
    };

    class ExportSpecifierNode final : public Node {
    public:
        ExportSpecifierNode(const JSTokenLocation&, const Identifier& localName, const Identifier& exportedName);

        const Identifier& exportedName() { return m_exportedName; }
        const Identifier& localName() { return m_localName; }

    private:
        const Identifier& m_localName;
        const Identifier& m_exportedName;
    };

    class ExportSpecifierListNode final : public ParserArenaDeletable {
        JSC_MAKE_PARSER_ARENA_DELETABLE_ALLOCATED(ExportSpecifierListNode);
    public:
        typedef Vector<ExportSpecifierNode*, 3> Specifiers;

        const Specifiers& specifiers() const { return m_specifiers; }
        void append(ExportSpecifierNode* specifier)
        {
            m_specifiers.append(specifier);
        }

    private:
        Specifiers m_specifiers;
    };

    class ExportNamedDeclarationNode final : public ModuleDeclarationNode {
    public:
        ExportNamedDeclarationNode(const JSTokenLocation&, ExportSpecifierListNode*, ModuleNameNode*);

        ExportSpecifierListNode* specifierList() const { return m_specifierList; }
        ModuleNameNode* moduleName() const { return m_moduleName; }

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
        void analyzeModule(ModuleAnalyzer&) final;
        ExportSpecifierListNode* m_specifierList;
        ModuleNameNode* m_moduleName { nullptr };
    };

    class FunctionMetadataNode final : public ParserArenaDeletable, public Node {
        JSC_MAKE_PARSER_ARENA_DELETABLE_ALLOCATED(FunctionMetadataNode);
    public:
        FunctionMetadataNode(
            ParserArena&, const JSTokenLocation& start, const JSTokenLocation& end, 
            unsigned startColumn, unsigned endColumn, int functionKeywordStart, 
            int functionNameStart, int parametersStart, bool isInStrictContext, 
            ConstructorKind, SuperBinding, unsigned parameterCount,
            SourceParseMode, bool isArrowFunctionBodyExpression);
        FunctionMetadataNode(
            const JSTokenLocation& start, const JSTokenLocation& end, 
            unsigned startColumn, unsigned endColumn, int functionKeywordStart, 
            int functionNameStart, int parametersStart, bool isInStrictContext, 
            ConstructorKind, SuperBinding, unsigned parameterCount,
            SourceParseMode, bool isArrowFunctionBodyExpression);

        void dump(PrintStream&) const;

        void finishParsing(const SourceCode&, const Identifier&, FunctionMode);
        
        void overrideName(const Identifier& ident) { m_ident = ident; }
        const Identifier& ident() { return m_ident; }
        void setEcmaName(const Identifier& ecmaName) { m_ecmaName = ecmaName; }
        const Identifier& ecmaName() { return m_ident.isEmpty() ? m_ecmaName : m_ident; }

        FunctionMode functionMode() { return m_functionMode; }

        int functionNameStart() const { return m_functionNameStart; }
        int functionKeywordStart() const { return m_functionKeywordStart; }
        int parametersStart() const { return m_parametersStart; }
        unsigned startColumn() const { return m_startColumn; }
        unsigned endColumn() const { return m_endColumn; }
        unsigned parameterCount() const { return m_parameterCount; }
        SourceParseMode parseMode() const { return m_parseMode; }

        void setEndPosition(JSTextPosition);

        const SourceCode& source() const { return m_source; }
        const SourceCode& classSource() const { return m_classSource; }
        void setClassSource(const SourceCode& source) { m_classSource = source; }

        int startStartOffset() const { return m_startStartOffset; }
        bool isInStrictContext() const { return m_isInStrictContext; }
        SuperBinding superBinding() { return static_cast<SuperBinding>(m_superBinding); }
        ConstructorKind constructorKind() { return static_cast<ConstructorKind>(m_constructorKind); }
        bool isConstructorAndNeedsClassFieldInitializer() const { return m_needsClassFieldInitializer; }
        void setNeedsClassFieldInitializer(bool value)
        {
            ASSERT(!value || constructorKind() != ConstructorKind::None);
            m_needsClassFieldInitializer = value;
        }
        bool isArrowFunctionBodyExpression() const { return m_isArrowFunctionBodyExpression; }

        void setLoc(unsigned firstLine, unsigned lastLine, int startOffset, int lineStartOffset)
        {
            m_lastLine = lastLine;
            m_position = JSTextPosition(firstLine, startOffset, lineStartOffset);
            ASSERT(m_position.offset >= m_position.lineStartOffset);
        }
        unsigned lastLine() const { return m_lastLine; }

        bool operator==(const FunctionMetadataNode&) const;
        bool operator!=(const FunctionMetadataNode& other) const
        {
            return !(*this == other);
        }

    public:
        unsigned m_isInStrictContext : 1;
        unsigned m_superBinding : 1;
        unsigned m_constructorKind : 2;
        unsigned m_needsClassFieldInitializer : 1;
        unsigned m_isArrowFunctionBodyExpression : 1;
        SourceParseMode m_parseMode;
        FunctionMode m_functionMode;
        Identifier m_ident;
        Identifier m_ecmaName;
        unsigned m_startColumn;
        unsigned m_endColumn;
        int m_functionKeywordStart;
        int m_functionNameStart;
        int m_parametersStart;
        SourceCode m_source;
        SourceCode m_classSource;
        int m_startStartOffset;
        unsigned m_parameterCount;
        int m_lastLine { 0 };
    };

    class FunctionNode final : public ScopeNode {
    public:
        FunctionNode(ParserArena&, const JSTokenLocation& start, const JSTokenLocation& end, unsigned startColumn, unsigned endColumn, SourceElements*, VariableEnvironment&, FunctionStack&&, VariableEnvironment&, UniquedStringImplPtrSet&&, FunctionParameters*, const SourceCode&, CodeFeatures, InnerArrowFunctionCodeFeatures, int numConstants, RefPtr<ModuleScopeData>&&);

        FunctionParameters* parameters() const { return m_parameters; }

        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        bool isFunctionNode() const final { return true; }

        void finishParsing(const Identifier&, FunctionMode);
        
        const Identifier& ident() { return m_ident; }

        FunctionMode functionMode() const { return m_functionMode; }

        unsigned startColumn() const { return m_startColumn; }
        unsigned endColumn() const { return m_endColumn; }

        static constexpr bool scopeIsFunction = true;

    private:
        Identifier m_ident;
        FunctionMode m_functionMode;
        FunctionParameters* m_parameters;
        unsigned m_startColumn;
        unsigned m_endColumn;
    };

    class BaseFuncExprNode : public ExpressionNode {
    public:
        FunctionMetadataNode* metadata() { return m_metadata; }

        bool isBaseFuncExprNode() const override { return true; }

    protected:
        BaseFuncExprNode(const JSTokenLocation&, const Identifier&, FunctionMetadataNode*, const SourceCode&, FunctionMode);

        FunctionMetadataNode* m_metadata;
    };


    class FuncExprNode : public BaseFuncExprNode {
    public:
        FuncExprNode(const JSTokenLocation&, const Identifier&, FunctionMetadataNode*, const SourceCode&);

    protected:
        FuncExprNode(const JSTokenLocation&, const Identifier&, FunctionMetadataNode*, const SourceCode&, FunctionMode);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) override;

        bool isFuncExprNode() const override { return true; }
    };

    class ArrowFuncExprNode final : public BaseFuncExprNode {
    public:
        ArrowFuncExprNode(const JSTokenLocation&, const Identifier&, FunctionMetadataNode*, const SourceCode&);

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        bool isArrowFuncExprNode() const final { return true; }
    };

    class MethodDefinitionNode final : public FuncExprNode {
    public:
        MethodDefinitionNode(const JSTokenLocation&, const Identifier&, FunctionMetadataNode*, const SourceCode&);
        
    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;
    };

    class YieldExprNode final : public ExpressionNode, public ThrowableExpressionData {
    public:
        YieldExprNode(const JSTokenLocation&, ExpressionNode* argument, bool delegate);

        ExpressionNode* argument() const { return m_argument; }
        bool delegate() const { return m_delegate; }

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* m_argument;
        bool m_delegate;
    };

    class AwaitExprNode final : public ExpressionNode, public ThrowableExpressionData {
    public:
        AwaitExprNode(const JSTokenLocation&, ExpressionNode* argument);

        ExpressionNode* argument() const { return m_argument; }

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        ExpressionNode* m_argument;
    };

    class DefineFieldNode final : public StatementNode {
    public:
        enum class Type { Name, PrivateName, ComputedName };
        DefineFieldNode(const JSTokenLocation&, const Identifier*, ExpressionNode*, Type);

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* destination = nullptr) final;

        bool isDefineFieldNode() const final { return true; }

        const Identifier* m_ident;
        ExpressionNode* m_assign;
        Type m_type;
    };

    class ClassExprNode final : public ExpressionNode, public VariableEnvironmentNode {
        JSC_MAKE_PARSER_ARENA_DELETABLE_ALLOCATED(ClassExprNode);
    public:
        ClassExprNode(const JSTokenLocation&, const Identifier&, const SourceCode& classSource,
            VariableEnvironment& classEnvironment, ExpressionNode* constructorExpresssion,
            ExpressionNode* parentClass, PropertyListNode* classElements);

        const Identifier& name() { return m_name; }
        const Identifier& ecmaName() { return m_ecmaName ? *m_ecmaName : m_name; }
        void setEcmaName(const Identifier& name) { m_ecmaName = m_name.isNull() ? &name : &m_name; }

        bool hasStaticProperty(const Identifier& propName) { return m_classElements ? m_classElements->hasStaticallyNamedProperty(propName) : false; }
        bool hasInstanceFields() const { return m_classElements ? m_classElements->hasInstanceFields() : false; }

    private:
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        bool isClassExprNode() const final { return true; }

        SourceCode m_classSource;
        const Identifier& m_name;
        const Identifier* m_ecmaName;
        ExpressionNode* m_constructorExpression;
        ExpressionNode* m_classHeritage;
        PropertyListNode* m_classElements;
        bool m_needsLexicalScope;
    };

    class DestructuringPatternNode : public ParserArenaFreeable {
    public:
        virtual ~DestructuringPatternNode() { }
        virtual void collectBoundIdentifiers(Vector<Identifier>&) const = 0;
        virtual void bindValue(BytecodeGenerator&, RegisterID* source) const = 0;
        virtual void toString(StringBuilder&) const = 0;

        virtual bool isBindingNode() const { return false; }
        virtual bool isAssignmentElementNode() const { return false; }
        virtual bool isRestParameter() const { return false; }
        virtual RegisterID* emitDirectBinding(BytecodeGenerator&, RegisterID*, ExpressionNode*) { return nullptr; }
        
    protected:
        DestructuringPatternNode();
    };

    class ArrayPatternNode final : public DestructuringPatternNode, public ParserArenaDeletable, public ThrowableExpressionData {
        JSC_MAKE_PARSER_ARENA_DELETABLE_ALLOCATED(ArrayPatternNode);
    public:
        ArrayPatternNode();
        enum class BindingType : uint8_t {
            Elision,
            Element,
            RestElement
        };

        void appendIndex(BindingType bindingType, const JSTokenLocation&, DestructuringPatternNode* node, ExpressionNode* defaultValue)
        {
            m_targetPatterns.append({ bindingType, node, defaultValue });
        }

    private:
        struct Entry {
            BindingType bindingType;
            DestructuringPatternNode* pattern;
            ExpressionNode* defaultValue;
        };
        void collectBoundIdentifiers(Vector<Identifier>&) const final;
        void bindValue(BytecodeGenerator&, RegisterID*) const final;
        RegisterID* emitDirectBinding(BytecodeGenerator&, RegisterID* dst, ExpressionNode*) final;
        void toString(StringBuilder&) const final;

        Vector<Entry> m_targetPatterns;
    };
    
    class ObjectPatternNode final : public DestructuringPatternNode, public ParserArenaDeletable, public ThrowableExpressionData {
        JSC_MAKE_PARSER_ARENA_DELETABLE_ALLOCATED(ObjectPatternNode);
    public:
        ObjectPatternNode();
        enum class BindingType : uint8_t {
            Element,
            RestElement
        };
        void appendEntry(const JSTokenLocation&, const Identifier& identifier, bool wasString, DestructuringPatternNode* pattern, ExpressionNode* defaultValue, BindingType bindingType)
        {
            m_targetPatterns.append(Entry{ identifier, nullptr, wasString, pattern, defaultValue, bindingType });  
        }

        void appendEntry(VM& vm, const JSTokenLocation&, ExpressionNode* propertyExpression, DestructuringPatternNode* pattern, ExpressionNode* defaultValue, BindingType bindingType)
        {
            m_targetPatterns.append(Entry{ vm.propertyNames->nullIdentifier, propertyExpression, false, pattern, defaultValue, bindingType });
        }
        
        void setContainsRestElement(bool containsRestElement)
        {
            m_containsRestElement = containsRestElement;
        }
        
        void setContainsComputedProperty(bool containsComputedProperty)
        {
            m_containsComputedProperty = containsComputedProperty;
        }

    private:
        void collectBoundIdentifiers(Vector<Identifier>&) const final;
        void bindValue(BytecodeGenerator&, RegisterID*) const final;
        void toString(StringBuilder&) const final;
        struct Entry {
            const Identifier& propertyName;
            ExpressionNode* propertyExpression;
            bool wasString;
            DestructuringPatternNode* pattern;
            ExpressionNode* defaultValue;
            BindingType bindingType;
        };
        bool m_containsRestElement { false };
        bool m_containsComputedProperty { false };
        Vector<Entry> m_targetPatterns;
    };

    class BindingNode final: public DestructuringPatternNode {
    public:
        BindingNode(const Identifier& boundProperty, const JSTextPosition& start, const JSTextPosition& end, AssignmentContext);
        const Identifier& boundProperty() const { return m_boundProperty; }

        const JSTextPosition& divotStart() const { return m_divotStart; }
        const JSTextPosition& divotEnd() const { return m_divotEnd; }
        
    private:
        void collectBoundIdentifiers(Vector<Identifier>&) const final;
        void bindValue(BytecodeGenerator&, RegisterID*) const final;
        void toString(StringBuilder&) const final;
        
        bool isBindingNode() const final { return true; }

        JSTextPosition m_divotStart;
        JSTextPosition m_divotEnd;
        const Identifier& m_boundProperty;
        AssignmentContext m_bindingContext;
    };

    class RestParameterNode final : public DestructuringPatternNode {
    public:
        RestParameterNode(DestructuringPatternNode*, unsigned numParametersToSkip);

        bool isRestParameter() const final { return true; }

        void emit(BytecodeGenerator&);

    private:
        void collectBoundIdentifiers(Vector<Identifier>&) const final;
        void bindValue(BytecodeGenerator&, RegisterID*) const final;
        void toString(StringBuilder&) const final;

        DestructuringPatternNode* m_pattern;
        unsigned m_numParametersToSkip;
    };

    class AssignmentElementNode final : public DestructuringPatternNode {
    public:
        AssignmentElementNode(ExpressionNode* assignmentTarget, const JSTextPosition& start, const JSTextPosition& end);
        const ExpressionNode* assignmentTarget() { return m_assignmentTarget; }

        const JSTextPosition& divotStart() const { return m_divotStart; }
        const JSTextPosition& divotEnd() const { return m_divotEnd; }

    private:
        void collectBoundIdentifiers(Vector<Identifier>&) const final;
        void bindValue(BytecodeGenerator&, RegisterID*) const final;
        void toString(StringBuilder&) const final;

        bool isAssignmentElementNode() const final { return true; }

        JSTextPosition m_divotStart;
        JSTextPosition m_divotEnd;
        ExpressionNode* m_assignmentTarget;
    };

    class DestructuringAssignmentNode final : public ExpressionNode {
    public:
        DestructuringAssignmentNode(const JSTokenLocation&, DestructuringPatternNode*, ExpressionNode*);
        DestructuringPatternNode* bindings() { return m_bindings; }
        
    private:
        bool isAssignmentLocation() const final { return true; }
        bool isDestructuringNode() const final { return true; }
        RegisterID* emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        DestructuringPatternNode* m_bindings;
        ExpressionNode* m_initializer;
    };

    class FunctionParameters final : public ParserArenaDeletable {
        JSC_MAKE_PARSER_ARENA_DELETABLE_ALLOCATED(FunctionParameters);
    public:
        FunctionParameters();
        ALWAYS_INLINE unsigned size() const { return m_patterns.size(); }
        ALWAYS_INLINE std::pair<DestructuringPatternNode*, ExpressionNode*> at(unsigned index) { return m_patterns[index]; }
        ALWAYS_INLINE void append(DestructuringPatternNode* pattern, ExpressionNode* defaultValue)
        {
            ASSERT(pattern);

            // http://www.ecma-international.org/ecma-262/6.0/index.html#sec-functiondeclarationinstantiation
            // This implements IsSimpleParameterList in the Ecma 2015 spec.
            // If IsSimpleParameterList is false, we will create a strict-mode like arguments object.
            // IsSimpleParameterList is false if the argument list contains any default parameter values,
            // a rest parameter, or any destructuring patterns.
            // If we do have default parameters, destructuring parameters, or a rest parameter, our parameters will be allocated in a different scope.

            bool hasDefaultParameterValue = defaultValue;
            bool isSimpleParameter = !hasDefaultParameterValue && pattern->isBindingNode();
            m_isSimpleParameterList &= isSimpleParameter;

            m_patterns.append(std::make_pair(pattern, defaultValue));
        }
        ALWAYS_INLINE bool isSimpleParameterList() const { return m_isSimpleParameterList; }

    private:

        Vector<std::pair<DestructuringPatternNode*, ExpressionNode*>, 3> m_patterns;
        bool m_isSimpleParameterList { true };
    };

    class FuncDeclNode final : public StatementNode {
    public:
        FuncDeclNode(const JSTokenLocation&, const Identifier&, FunctionMetadataNode*, const SourceCode&);

        bool hasCompletionValue() const final { return false; }
        bool isFuncDeclNode() const final { return true; }
        FunctionMetadataNode* metadata() { return m_metadata; }

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        FunctionMetadataNode* m_metadata;
    };

    class ClassDeclNode final : public StatementNode {
    public:
        ClassDeclNode(const JSTokenLocation&, ExpressionNode* classExpression);

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

        bool hasCompletionValue() const final { return false; }

        ExpressionNode* m_classDeclaration;
    };

    class CaseClauseNode final : public ParserArenaFreeable {
    public:
        CaseClauseNode(ExpressionNode*, SourceElements* = nullptr);

        ExpressionNode* expr() const { return m_expr; }

        void emitBytecode(BytecodeGenerator&, RegisterID* destination);
        void setStartOffset(int offset) { m_startOffset = offset; }

    private:
        ExpressionNode* m_expr;
        SourceElements* m_statements;
        int m_startOffset;
    };

    class ClauseListNode final : public ParserArenaFreeable {
    public:
        ClauseListNode(CaseClauseNode*);
        ClauseListNode(ClauseListNode*, CaseClauseNode*);

        CaseClauseNode* getClause() const { return m_clause; }
        ClauseListNode* getNext() const { return m_next; }

    private:
        CaseClauseNode* m_clause;
        ClauseListNode* m_next { nullptr };
    };

    class CaseBlockNode final : public ParserArenaFreeable {
    public:
        CaseBlockNode(ClauseListNode* list1, CaseClauseNode* defaultClause, ClauseListNode* list2);

        void emitBytecodeForBlock(BytecodeGenerator&, RegisterID* input, RegisterID* destination);

    private:
        SwitchInfo::SwitchType tryTableSwitch(Vector<ExpressionNode*, 8>& literalVector, int32_t& min_num, int32_t& max_num);
        static constexpr size_t s_tableSwitchMinimum = 3;
        ClauseListNode* m_list1;
        CaseClauseNode* m_defaultClause;
        ClauseListNode* m_list2;
    };

    class SwitchNode final : public StatementNode, public VariableEnvironmentNode {
        JSC_MAKE_PARSER_ARENA_DELETABLE_ALLOCATED(SwitchNode);
    public:
        SwitchNode(const JSTokenLocation&, ExpressionNode*, CaseBlockNode*, VariableEnvironment&, FunctionStack&&);

    private:
        void emitBytecode(BytecodeGenerator&, RegisterID* = nullptr) final;

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

    struct ClauseList {
        ClauseListNode* head;
        ClauseListNode* tail;
    };

} // namespace JSC
