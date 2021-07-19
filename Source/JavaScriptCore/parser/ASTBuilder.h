/*
 * Copyright (C) 2010-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "BuiltinNames.h"
#include "BytecodeIntrinsicRegistry.h"
#include "MathCommon.h"
#include "NodeConstructors.h"
#include "SyntaxChecker.h"
#include "VariableEnvironment.h"
#include <utility>

namespace JSC {

class ASTBuilder {
    struct BinaryOpInfo {
        BinaryOpInfo() {}
        BinaryOpInfo(const JSTextPosition& otherStart, const JSTextPosition& otherDivot, const JSTextPosition& otherEnd, bool rhsHasAssignment)
            : start(otherStart)
            , divot(otherDivot)
            , end(otherEnd)
            , hasAssignment(rhsHasAssignment)
        {
        }
        BinaryOpInfo(const BinaryOpInfo& lhs, const BinaryOpInfo& rhs)
            : start(lhs.start)
            , divot(rhs.start)
            , end(rhs.end)
            , hasAssignment(lhs.hasAssignment || rhs.hasAssignment)
        {
        }
        JSTextPosition start;
        JSTextPosition divot;
        JSTextPosition end;
        bool hasAssignment;
    };
    
    
    struct AssignmentInfo {
        AssignmentInfo() {}
        AssignmentInfo(ExpressionNode* node, const JSTextPosition& start, const JSTextPosition& divot, int initAssignments, Operator op)
            : m_node(node)
            , m_start(start)
            , m_divot(divot)
            , m_initAssignments(initAssignments)
            , m_op(op)
        {
            ASSERT(m_divot.offset >= m_divot.lineStartOffset);
            ASSERT(m_start.offset >= m_start.lineStartOffset);
        }
        ExpressionNode* m_node;
        JSTextPosition m_start;
        JSTextPosition m_divot;
        int m_initAssignments;
        Operator m_op;
    };
public:
    ASTBuilder(VM& vm, ParserArena& parserArena, SourceCode* sourceCode)
        : m_vm(vm)
        , m_parserArena(parserArena)
        , m_sourceCode(sourceCode)
        , m_evalCount(0)
    {
    }
    
    struct BinaryExprContext {
        BinaryExprContext(ASTBuilder&) {}
    };
    struct UnaryExprContext {
        UnaryExprContext(ASTBuilder&) {}
    };

    typedef ExpressionNode* Expression;
    typedef JSC::SourceElements* SourceElements;
    typedef ArgumentsNode* Arguments;
    typedef CommaNode* Comma;
    typedef PropertyNode* Property;
    typedef PropertyListNode* PropertyList;
    typedef ElementNode* ElementList;
    typedef ArgumentListNode* ArgumentsList;
    typedef TemplateExpressionListNode* TemplateExpressionList;
    typedef TemplateStringNode* TemplateString;
    typedef TemplateStringListNode* TemplateStringList;
    typedef TemplateLiteralNode* TemplateLiteral;
    typedef FunctionParameters* FormalParameterList;
    typedef FunctionMetadataNode* FunctionBody;
    typedef ClassExprNode* ClassExpression;
    typedef ModuleNameNode* ModuleName;
    typedef ImportSpecifierNode* ImportSpecifier;
    typedef ImportSpecifierListNode* ImportSpecifierList;
    typedef ExportSpecifierNode* ExportSpecifier;
    typedef ExportSpecifierListNode* ExportSpecifierList;
    typedef StatementNode* Statement;
    typedef ClauseListNode* ClauseList;
    typedef CaseClauseNode* Clause;
    typedef std::pair<ExpressionNode*, BinaryOpInfo> BinaryOperand;
    typedef DestructuringPatternNode* DestructuringPattern;
    typedef ArrayPatternNode* ArrayPattern;
    typedef ObjectPatternNode* ObjectPattern;
    typedef BindingNode* BindingPattern;
    typedef AssignmentElementNode* AssignmentElement;
    static constexpr bool CreatesAST = true;
    static constexpr bool NeedsFreeVariableInfo = true;
    static constexpr bool CanUseFunctionCache = true;
    static constexpr OptionSet<LexerFlags> DontBuildKeywords = { };
    static constexpr OptionSet<LexerFlags> DontBuildStrings = { };

    ExpressionNode* makeBinaryNode(const JSTokenLocation&, int token, std::pair<ExpressionNode*, BinaryOpInfo>, std::pair<ExpressionNode*, BinaryOpInfo>);
    ExpressionNode* makeFunctionCallNode(const JSTokenLocation&, ExpressionNode* func, bool previousBaseWasSuper, ArgumentsNode* args, const JSTextPosition& divotStart, const JSTextPosition& divot, const JSTextPosition& divotEnd, size_t callOrApplyChildDepth, bool isOptionalCall);

    JSC::SourceElements* createSourceElements() { return new (m_parserArena) JSC::SourceElements(); }

    int features() const { return m_scope.m_features; }
    int numConstants() const { return m_scope.m_numConstants; }

    ExpressionNode* makeAssignNode(const JSTokenLocation&, ExpressionNode* left, Operator, ExpressionNode* right, bool leftHasAssignments, bool rightHasAssignments, const JSTextPosition& start, const JSTextPosition& divot, const JSTextPosition& end);
    ExpressionNode* makePrefixNode(const JSTokenLocation&, ExpressionNode*, Operator, const JSTextPosition& start, const JSTextPosition& divot, const JSTextPosition& end);
    ExpressionNode* makePostfixNode(const JSTokenLocation&, ExpressionNode*, Operator, const JSTextPosition& start, const JSTextPosition& divot, const JSTextPosition& end);
    ExpressionNode* makeTypeOfNode(const JSTokenLocation&, ExpressionNode*);
    ExpressionNode* makeDeleteNode(const JSTokenLocation&, ExpressionNode*, const JSTextPosition& start, const JSTextPosition& divot, const JSTextPosition& end);
    ExpressionNode* makeNegateNode(const JSTokenLocation&, ExpressionNode*);
    ExpressionNode* makeBitwiseNotNode(const JSTokenLocation&, ExpressionNode*);
    ExpressionNode* makePowNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeMultNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeDivNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeModNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeAddNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeSubNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeBitXOrNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeBitAndNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeBitOrNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeCoalesceNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right);
    ExpressionNode* makeLeftShiftNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeRightShiftNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeURightShiftNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);

    ExpressionNode* createLogicalNot(const JSTokenLocation& location, ExpressionNode* expr)
    {
        if (expr->isNumber())
            return createBoolean(location, isZeroOrUnordered(static_cast<NumberNode*>(expr)->value()));

        return new (m_parserArena) LogicalNotNode(location, expr);
    }
    ExpressionNode* createUnaryPlus(const JSTokenLocation& location, ExpressionNode* expr) { return new (m_parserArena) UnaryPlusNode(location, expr); }
    ExpressionNode* createVoid(const JSTokenLocation& location, ExpressionNode* expr)
    {
        incConstants();
        return new (m_parserArena) VoidNode(location, expr);
    }
    ExpressionNode* createThisExpr(const JSTokenLocation& location)
    {
        usesThis();
        return new (m_parserArena) ThisNode(location);
    }
    ExpressionNode* createSuperExpr(const JSTokenLocation& location)
    {
        return new (m_parserArena) SuperNode(location);
    }
    ExpressionNode* createImportExpr(const JSTokenLocation& location, ExpressionNode* expr, const JSTextPosition& start, const JSTextPosition& divot, const JSTextPosition& end)
    {
        auto* node = new (m_parserArena) ImportNode(location, expr);
        setExceptionLocation(node, start, divot, end);
        return node;
    }
    ExpressionNode* createNewTargetExpr(const JSTokenLocation location)
    {
        usesNewTarget();
        return new (m_parserArena) NewTargetNode(location);
    }
    ExpressionNode* createImportMetaExpr(const JSTokenLocation& location, ExpressionNode* expr) { return new (m_parserArena) ImportMetaNode(location, expr); }
    bool isMetaProperty(ExpressionNode* node) { return node->isMetaProperty(); }
    bool isNewTarget(ExpressionNode* node) { return node->isNewTarget(); }
    bool isImportMeta(ExpressionNode* node) { return node->isImportMeta(); }
    ExpressionNode* createResolve(const JSTokenLocation& location, const Identifier& ident, const JSTextPosition& start, const JSTextPosition& end)
    {
        if (m_vm.propertyNames->arguments == ident)
            usesArguments();

        if (ident.isSymbol()) {
            auto entry = m_vm.bytecodeIntrinsicRegistry().lookup(ident);
            if (entry)
                return new (m_parserArena) BytecodeIntrinsicNode(BytecodeIntrinsicNode::Type::Constant, location, entry.value(), ident, nullptr, start, start, end);
        }

        return new (m_parserArena) ResolveNode(location, ident, start);
    }
    ExpressionNode* createPrivateIdentifierNode(const JSTokenLocation& location, const Identifier& ident)
    {
        return new (m_parserArena) PrivateIdentifierNode(location, ident);
    }
    ExpressionNode* createObjectLiteral(const JSTokenLocation& location) { return new (m_parserArena) ObjectLiteralNode(location); }
    ExpressionNode* createObjectLiteral(const JSTokenLocation& location, PropertyListNode* properties) { return new (m_parserArena) ObjectLiteralNode(location, properties); }

    ExpressionNode* createArray(const JSTokenLocation& location, int elisions)
    {
        if (elisions)
            incConstants();
        return new (m_parserArena) ArrayNode(location, elisions);
    }

    ExpressionNode* createArray(const JSTokenLocation& location, ElementNode* elems) { return new (m_parserArena) ArrayNode(location, elems); }
    ExpressionNode* createArray(const JSTokenLocation& location, int elisions, ElementNode* elems)
    {
        if (elisions)
            incConstants();
        return new (m_parserArena) ArrayNode(location, elisions, elems);
    }
    ExpressionNode* createDoubleExpr(const JSTokenLocation& location, double d)
    {
        incConstants();
        return new (m_parserArena) DoubleNode(location, d);
    }
    ExpressionNode* createIntegerExpr(const JSTokenLocation& location, double d)
    {
        incConstants();
        return new (m_parserArena) IntegerNode(location, d);
    }
    
    ExpressionNode* createBigInt(const JSTokenLocation& location, const Identifier* bigInt, uint8_t radix)
    {
        incConstants();
        return new (m_parserArena) BigIntNode(location, *bigInt, radix);
    }

    ExpressionNode* createString(const JSTokenLocation& location, const Identifier* string)
    {
        ASSERT(string);
        incConstants();
        return new (m_parserArena) StringNode(location, *string);
    }

    ExpressionNode* createBoolean(const JSTokenLocation& location, bool b)
    {
        incConstants();
        return new (m_parserArena) BooleanNode(location, b);
    }

    ExpressionNode* createNull(const JSTokenLocation& location)
    {
        incConstants();
        return new (m_parserArena) NullNode(location);
    }

    ExpressionNode* createBracketAccess(const JSTokenLocation& location, ExpressionNode* base, ExpressionNode* property, bool propertyHasAssignments, const JSTextPosition& start, const JSTextPosition& divot, const JSTextPosition& end)
    {
        if (base->isSuperNode())
            usesSuperProperty();

        BracketAccessorNode* node = new (m_parserArena) BracketAccessorNode(location, base, property, propertyHasAssignments);
        setExceptionLocation(node, start, divot, end);
        return node;
    }

    ExpressionNode* createDotAccess(const JSTokenLocation& location, ExpressionNode* base, const Identifier* property, DotType type, const JSTextPosition& start, const JSTextPosition& divot, const JSTextPosition& end)
    {
        if (base->isSuperNode())
            usesSuperProperty();
        
        DotAccessorNode* node = new (m_parserArena) DotAccessorNode(location, base, *property, type);
        setExceptionLocation(node, start, divot, end);
        return node;
    }

    ExpressionNode* createSpreadExpression(const JSTokenLocation& location, ExpressionNode* expression, const JSTextPosition& start, const JSTextPosition& divot, const JSTextPosition& end)
    {
        auto node = new (m_parserArena) SpreadExpressionNode(location, expression);
        setExceptionLocation(node, start, divot, end);
        return node;
    }

    ExpressionNode* createObjectSpreadExpression(const JSTokenLocation& location, ExpressionNode* expression, const JSTextPosition& start, const JSTextPosition& divot, const JSTextPosition& end)
    {
        auto node = new (m_parserArena) ObjectSpreadExpressionNode(location, expression);
        setExceptionLocation(node, start, divot, end);
        return node;
    }

    TemplateStringNode* createTemplateString(const JSTokenLocation& location, const Identifier* cooked, const Identifier* raw)
    {
        return new (m_parserArena) TemplateStringNode(location, cooked, raw);
    }

    TemplateStringListNode* createTemplateStringList(TemplateStringNode* templateString)
    {
        return new (m_parserArena) TemplateStringListNode(templateString);
    }

    TemplateStringListNode* createTemplateStringList(TemplateStringListNode* templateStringList, TemplateStringNode* templateString)
    {
        return new (m_parserArena) TemplateStringListNode(templateStringList, templateString);
    }

    TemplateExpressionListNode* createTemplateExpressionList(ExpressionNode* expression)
    {
        return new (m_parserArena) TemplateExpressionListNode(expression);
    }

    TemplateExpressionListNode* createTemplateExpressionList(TemplateExpressionListNode* templateExpressionListNode, ExpressionNode* expression)
    {
        return new (m_parserArena) TemplateExpressionListNode(templateExpressionListNode, expression);
    }

    TemplateLiteralNode* createTemplateLiteral(const JSTokenLocation& location, TemplateStringListNode* templateStringList)
    {
        return new (m_parserArena) TemplateLiteralNode(location, templateStringList);
    }

    TemplateLiteralNode* createTemplateLiteral(const JSTokenLocation& location, TemplateStringListNode* templateStringList, TemplateExpressionListNode* templateExpressionList)
    {
        return new (m_parserArena) TemplateLiteralNode(location, templateStringList, templateExpressionList);
    }

    ExpressionNode* createTaggedTemplate(const JSTokenLocation& location, ExpressionNode* base, TemplateLiteralNode* templateLiteral, const JSTextPosition& start, const JSTextPosition& divot, const JSTextPosition& end)
    {
        auto node = new (m_parserArena) TaggedTemplateNode(location, base, templateLiteral);
        setExceptionLocation(node, start, divot, end);
        setEndOffset(node, end.offset);
        return node;
    }

    ExpressionNode* createRegExp(const JSTokenLocation& location, const Identifier& pattern, const Identifier& flags, const JSTextPosition& start)
    {
        if (Yarr::hasError(Yarr::checkSyntax(pattern.string(), flags.string())))
            return nullptr;
        RegExpNode* node = new (m_parserArena) RegExpNode(location, pattern, flags);
        int size = pattern.length() + 2; // + 2 for the two /'s
        JSTextPosition end = start + size;
        setExceptionLocation(node, start, end, end);
        return node;
    }

    ExpressionNode* createNewExpr(const JSTokenLocation& location, ExpressionNode* expr, ArgumentsNode* arguments, const JSTextPosition& start, const JSTextPosition& divot, const JSTextPosition& end)
    {
        NewExprNode* node = new (m_parserArena) NewExprNode(location, expr, arguments);
        setExceptionLocation(node, start, divot, end);
        return node;
    }

    ExpressionNode* createNewExpr(const JSTokenLocation& location, ExpressionNode* expr, const JSTextPosition& start, const JSTextPosition& end)
    {
        NewExprNode* node = new (m_parserArena) NewExprNode(location, expr);
        setExceptionLocation(node, start, end, end);
        return node;
    }

    ExpressionNode* createOptionalChain(const JSTokenLocation& location, ExpressionNode* base, ExpressionNode* expr, bool isOutermost)
    {
        base->setIsOptionalChainBase();
        return new (m_parserArena) OptionalChainNode(location, expr, isOutermost);
    }

    ExpressionNode* createConditionalExpr(const JSTokenLocation& location, ExpressionNode* condition, ExpressionNode* lhs, ExpressionNode* rhs)
    {
        return new (m_parserArena) ConditionalNode(location, condition, lhs, rhs);
    }

    ExpressionNode* createAssignResolve(const JSTokenLocation& location, const Identifier& ident, ExpressionNode* rhs, const JSTextPosition& start, const JSTextPosition& divot, const JSTextPosition& end, AssignmentContext assignmentContext)
    {
        if (rhs->isBaseFuncExprNode()) {
            auto metadata = static_cast<BaseFuncExprNode*>(rhs)->metadata();
            metadata->setEcmaName(ident);
        } else if (rhs->isClassExprNode())
            static_cast<ClassExprNode*>(rhs)->setEcmaName(ident);
        AssignResolveNode* node = new (m_parserArena) AssignResolveNode(location, ident, rhs, assignmentContext);
        setExceptionLocation(node, start, divot, end);
        return node;
    }

    YieldExprNode* createYield(const JSTokenLocation& location)
    {
        return new (m_parserArena) YieldExprNode(location, nullptr, /* delegate */ false);
    }

    YieldExprNode* createYield(const JSTokenLocation& location, ExpressionNode* argument, bool delegate, const JSTextPosition& start, const JSTextPosition& divot, const JSTextPosition& end)
    {
        YieldExprNode* node = new (m_parserArena) YieldExprNode(location, argument, delegate);
        setExceptionLocation(node, start, divot, end);
        return node;
    }

    AwaitExprNode* createAwait(const JSTokenLocation& location, ExpressionNode* argument, const JSTextPosition& start, const JSTextPosition& divot, const JSTextPosition& end)
    {
        ASSERT(argument);
        usesAwait();
        AwaitExprNode* node = new (m_parserArena) AwaitExprNode(location, argument);
        setExceptionLocation(node, start, divot, end);
        return node;
    }

    DefineFieldNode* createDefineField(const JSTokenLocation& location, const Identifier* ident, ExpressionNode* initializer, DefineFieldNode::Type type)
    {
        return new (m_parserArena) DefineFieldNode(location, ident, initializer, type);
    }

    ClassExprNode* createClassExpr(const JSTokenLocation& location, const ParserClassInfo<ASTBuilder>& classInfo, VariableEnvironment&& classHeadEnvironment, VariableEnvironment&& classEnvironment, ExpressionNode* constructor,
        ExpressionNode* parentClass, PropertyListNode* classElements, const JSTextPosition& start, const JSTextPosition& divot, const JSTextPosition& end)
    {
        SourceCode source = m_sourceCode->subExpression(classInfo.startOffset, classInfo.endOffset, classInfo.startLine, classInfo.startColumn);
        ClassExprNode* node = new (m_parserArena) ClassExprNode(location, *classInfo.className, source, WTFMove(classHeadEnvironment), WTFMove(classEnvironment), constructor, parentClass, classElements);
        setExceptionLocation(node, start, divot, end);
        return node;
    }

    ExpressionNode* createFunctionExpr(const JSTokenLocation& location, const ParserFunctionInfo<ASTBuilder>& functionInfo)
    {
        FuncExprNode* result = new (m_parserArena) FuncExprNode(location, *functionInfo.name, functionInfo.body,
            m_sourceCode->subExpression(functionInfo.startOffset, functionInfo.endOffset, functionInfo.startLine, functionInfo.parametersStartColumn));
        functionInfo.body->setLoc(functionInfo.startLine, functionInfo.endLine, location.startOffset, location.lineStartOffset);
        return result;
    }

    ExpressionNode* createGeneratorFunctionBody(const JSTokenLocation& location, const ParserFunctionInfo<ASTBuilder>& functionInfo, const Identifier& name)
    {
        FuncExprNode* result = static_cast<FuncExprNode*>(createFunctionExpr(location, functionInfo));
        if (!name.isNull())
            result->metadata()->setEcmaName(name);
        return result;
    }

    ExpressionNode* createAsyncFunctionBody(const JSTokenLocation& location, const ParserFunctionInfo<ASTBuilder>& functionInfo, SourceParseMode parseMode)
    {
        if (parseMode == SourceParseMode::AsyncArrowFunctionBodyMode) {
            SourceCode source = m_sourceCode->subExpression(functionInfo.startOffset, functionInfo.body->isArrowFunctionBodyExpression() ? functionInfo.endOffset - 1 : functionInfo.endOffset, functionInfo.startLine, functionInfo.parametersStartColumn);
            FuncExprNode* result = new (m_parserArena) FuncExprNode(location, *functionInfo.name, functionInfo.body, source);
            functionInfo.body->setLoc(functionInfo.startLine, functionInfo.endLine, location.startOffset, location.lineStartOffset);
            return result;
        }
        return createFunctionExpr(location, functionInfo);
    }

    ExpressionNode* createMethodDefinition(const JSTokenLocation& location, const ParserFunctionInfo<ASTBuilder>& functionInfo)
    {
        MethodDefinitionNode* result = new (m_parserArena) MethodDefinitionNode(location, *functionInfo.name, functionInfo.body,
            m_sourceCode->subExpression(functionInfo.startOffset, functionInfo.endOffset, functionInfo.startLine, functionInfo.parametersStartColumn));
        functionInfo.body->setLoc(functionInfo.startLine, functionInfo.endLine, location.startOffset, location.lineStartOffset);
        return result;
    }
    
    FunctionMetadataNode* createFunctionMetadata(
        const JSTokenLocation& startLocation, const JSTokenLocation& endLocation, 
        unsigned startColumn, unsigned endColumn, int functionKeywordStart, 
        int functionNameStart, int parametersStart, LexicalScopeFeatures lexicalScopeFeatures, 
        ConstructorKind constructorKind, SuperBinding superBinding,
        unsigned parameterCount,
        SourceParseMode mode, bool isArrowFunctionBodyExpression)
    {
        return new (m_parserArena) FunctionMetadataNode(
            m_parserArena, startLocation, endLocation, startColumn, endColumn, 
            functionKeywordStart, functionNameStart, parametersStart, 
            lexicalScopeFeatures, constructorKind, superBinding,
            parameterCount, mode, isArrowFunctionBodyExpression);
    }

    ExpressionNode* createArrowFunctionExpr(const JSTokenLocation& location, const ParserFunctionInfo<ASTBuilder>& functionInfo)
    {
        usesArrowFunction();
        SourceCode source = m_sourceCode->subExpression(functionInfo.startOffset, functionInfo.body->isArrowFunctionBodyExpression() ? functionInfo.endOffset - 1 : functionInfo.endOffset, functionInfo.startLine, functionInfo.parametersStartColumn);
        ArrowFuncExprNode* result = new (m_parserArena) ArrowFuncExprNode(location, *functionInfo.name, functionInfo.body, source);
        functionInfo.body->setLoc(functionInfo.startLine, functionInfo.endLine, location.startOffset, location.lineStartOffset);
        return result;
    }

    ArgumentsNode* createArguments() { return new (m_parserArena) ArgumentsNode(); }
    ArgumentsNode* createArguments(ArgumentListNode* args, bool hasAssignments) { return new (m_parserArena) ArgumentsNode(args, hasAssignments); }
    ArgumentListNode* createArgumentsList(const JSTokenLocation& location, ExpressionNode* arg) { return new (m_parserArena) ArgumentListNode(location, arg); }
    ArgumentListNode* createArgumentsList(const JSTokenLocation& location, ArgumentListNode* args, ExpressionNode* arg) { return new (m_parserArena) ArgumentListNode(location, args, arg); }

    NEVER_INLINE PropertyNode* createGetterOrSetterProperty(const JSTokenLocation& location, PropertyNode::Type type,
        const Identifier* name, const ParserFunctionInfo<ASTBuilder>& functionInfo, ClassElementTag tag)
    {
        ASSERT(name);
        functionInfo.body->setLoc(functionInfo.startLine, functionInfo.endLine, location.startOffset, location.lineStartOffset);
        functionInfo.body->setEcmaName(*name);
        SourceCode source = m_sourceCode->subExpression(functionInfo.startOffset, functionInfo.endOffset, functionInfo.startLine, functionInfo.parametersStartColumn);
        MethodDefinitionNode* methodDef = new (m_parserArena) MethodDefinitionNode(location, m_vm.propertyNames->nullIdentifier, functionInfo.body, source);
        return new (m_parserArena) PropertyNode(*name, methodDef, type, SuperBinding::Needed, tag);
    }

    NEVER_INLINE PropertyNode* createGetterOrSetterProperty(const JSTokenLocation& location, PropertyNode::Type type,
        ExpressionNode* name, const ParserFunctionInfo<ASTBuilder>& functionInfo, ClassElementTag tag)
    {
        ASSERT(name);
        functionInfo.body->setLoc(functionInfo.startLine, functionInfo.endLine, location.startOffset, location.lineStartOffset);
        SourceCode source = m_sourceCode->subExpression(functionInfo.startOffset, functionInfo.endOffset, functionInfo.startLine, functionInfo.parametersStartColumn);
        MethodDefinitionNode* methodDef = new (m_parserArena) MethodDefinitionNode(location, m_vm.propertyNames->nullIdentifier, functionInfo.body, source);
        return new (m_parserArena) PropertyNode(name, methodDef, type, SuperBinding::Needed, tag);
    }

    NEVER_INLINE PropertyNode* createGetterOrSetterProperty(VM& vm, ParserArena& parserArena, const JSTokenLocation& location, PropertyNode::Type type,
        double name, const ParserFunctionInfo<ASTBuilder>& functionInfo, ClassElementTag tag)
    {
        functionInfo.body->setLoc(functionInfo.startLine, functionInfo.endLine, location.startOffset, location.lineStartOffset);
        const Identifier& ident = parserArena.identifierArena().makeNumericIdentifier(vm, name);
        SourceCode source = m_sourceCode->subExpression(functionInfo.startOffset, functionInfo.endOffset, functionInfo.startLine, functionInfo.parametersStartColumn);
        MethodDefinitionNode* methodDef = new (m_parserArena) MethodDefinitionNode(location, vm.propertyNames->nullIdentifier, functionInfo.body, source);
        return new (m_parserArena) PropertyNode(ident, methodDef, type, SuperBinding::Needed, tag);
    }

    PropertyNode* createProperty(const Identifier* propertyName, ExpressionNode* node, PropertyNode::Type type, SuperBinding superBinding, InferName inferName, ClassElementTag tag)
    {
        if (inferName == InferName::Allowed) {
            if (node->isBaseFuncExprNode()) {
                auto metadata = static_cast<BaseFuncExprNode*>(node)->metadata();
                metadata->setEcmaName(*propertyName);
            } else if (node->isClassExprNode())
                static_cast<ClassExprNode*>(node)->setEcmaName(*propertyName);
        }
        return new (m_parserArena) PropertyNode(*propertyName, node, type, superBinding, tag);
    }
    PropertyNode* createProperty(ExpressionNode* node, PropertyNode::Type type, SuperBinding superBinding, ClassElementTag tag)
    {
        return new (m_parserArena) PropertyNode(node, type, superBinding, tag);
    }
    PropertyNode* createProperty(VM& vm, ParserArena& parserArena, double propertyName, ExpressionNode* node, PropertyNode::Type type, SuperBinding superBinding, ClassElementTag tag)
    {
        return new (m_parserArena) PropertyNode(parserArena.identifierArena().makeNumericIdentifier(vm, propertyName), node, type, superBinding, tag);
    }
    PropertyNode* createProperty(ExpressionNode* propertyName, ExpressionNode* node, PropertyNode::Type type, SuperBinding superBinding, ClassElementTag tag) { return new (m_parserArena) PropertyNode(propertyName, node, type, superBinding, tag); }
    PropertyNode* createProperty(const Identifier* identifier, ExpressionNode* propertyName, ExpressionNode* node, PropertyNode::Type type, SuperBinding superBinding, ClassElementTag tag) { return new (m_parserArena) PropertyNode(*identifier, propertyName, node, type, superBinding, tag); }
    PropertyListNode* createPropertyList(const JSTokenLocation& location, PropertyNode* property) { return new (m_parserArena) PropertyListNode(location, property); }
    PropertyListNode* createPropertyList(const JSTokenLocation& location, PropertyNode* property, PropertyListNode* tail) { return new (m_parserArena) PropertyListNode(location, property, tail); }

    ElementNode* createElementList(int elisions, ExpressionNode* expr) { return new (m_parserArena) ElementNode(elisions, expr); }
    ElementNode* createElementList(ElementNode* elems, int elisions, ExpressionNode* expr) { return new (m_parserArena) ElementNode(elems, elisions, expr); }
    ElementNode* createElementList(ArgumentListNode* elems)
    {
        ElementNode* head = new (m_parserArena) ElementNode(0, elems->m_expr);
        ElementNode* tail = head;
        elems = elems->m_next;
        while (elems) {
            tail = new (m_parserArena) ElementNode(tail, 0, elems->m_expr);
            elems = elems->m_next;
        }
        return head;
    }

    FormalParameterList createFormalParameterList() { return new (m_parserArena) FunctionParameters(); }
    void appendParameter(FormalParameterList list, DestructuringPattern pattern, ExpressionNode* defaultValue) 
    { 
        list->append(pattern, defaultValue); 
        tryInferNameInPattern(pattern, defaultValue);
    }

    CaseClauseNode* createClause(ExpressionNode* expr, JSC::SourceElements* statements) { return new (m_parserArena) CaseClauseNode(expr, statements); }
    ClauseListNode* createClauseList(CaseClauseNode* clause) { return new (m_parserArena) ClauseListNode(clause); }
    ClauseListNode* createClauseList(ClauseListNode* tail, CaseClauseNode* clause) { return new (m_parserArena) ClauseListNode(tail, clause); }

    StatementNode* createFuncDeclStatement(const JSTokenLocation& location, const ParserFunctionInfo<ASTBuilder>& functionInfo)
    {
        FuncDeclNode* decl = new (m_parserArena) FuncDeclNode(location, *functionInfo.name, functionInfo.body,
            m_sourceCode->subExpression(functionInfo.startOffset, functionInfo.endOffset, functionInfo.startLine, functionInfo.parametersStartColumn));
        if (*functionInfo.name == m_vm.propertyNames->arguments)
            usesArguments();
        functionInfo.body->setLoc(functionInfo.startLine, functionInfo.endLine, location.startOffset, location.lineStartOffset);
        return decl;
    }

    StatementNode* createClassDeclStatement(const JSTokenLocation& location, ClassExprNode* classExpression,
        const JSTextPosition& classStart, const JSTextPosition& classEnd, unsigned startLine, unsigned endLine)
    {
        ExpressionNode* assign = createAssignResolve(location, classExpression->name(), classExpression, classStart, classStart + 1, classEnd, AssignmentContext::DeclarationStatement);
        ClassDeclNode* decl = new (m_parserArena) ClassDeclNode(location, assign);
        decl->setLoc(startLine, endLine, location.startOffset, location.lineStartOffset);
        return decl;
    }

    StatementNode* createBlockStatement(const JSTokenLocation& location, JSC::SourceElements* elements, int startLine, int endLine, VariableEnvironment&& lexicalVariables, DeclarationStacks::FunctionStack&& functionStack)
    {
        BlockNode* block = new (m_parserArena) BlockNode(location, elements, WTFMove(lexicalVariables), WTFMove(functionStack));
        block->setLoc(startLine, endLine, location.startOffset, location.lineStartOffset);
        return block;
    }

    StatementNode* createExprStatement(const JSTokenLocation& location, ExpressionNode* expr, const JSTextPosition& start, int end)
    {
        ExprStatementNode* result = new (m_parserArena) ExprStatementNode(location, expr);
        result->setLoc(start.line, end, start.offset, start.lineStartOffset);
        return result;
    }

    StatementNode* createIfStatement(const JSTokenLocation& location, ExpressionNode* condition, StatementNode* trueBlock, StatementNode* falseBlock, int start, int end)
    {
        IfElseNode* result = new (m_parserArena) IfElseNode(location, condition, trueBlock, falseBlock);
        result->setLoc(start, end, location.startOffset, location.lineStartOffset);
        return result;
    }

    StatementNode* createForLoop(const JSTokenLocation& location, ExpressionNode* initializer, ExpressionNode* condition, ExpressionNode* iter, StatementNode* statements, int start, int end, VariableEnvironment&& lexicalVariables)
    {
        ForNode* result = new (m_parserArena) ForNode(location, initializer, condition, iter, statements, WTFMove(lexicalVariables));
        result->setLoc(start, end, location.startOffset, location.lineStartOffset);
        return result;
    }

    StatementNode* createForInLoop(const JSTokenLocation& location, ExpressionNode* lhs, ExpressionNode* iter, StatementNode* statements, const JSTokenLocation&, const JSTextPosition& eStart, const JSTextPosition& eDivot, const JSTextPosition& eEnd, int start, int end, VariableEnvironment&& lexicalVariables)
    {
        ForInNode* result = new (m_parserArena) ForInNode(location, lhs, iter, statements, WTFMove(lexicalVariables));
        result->setLoc(start, end, location.startOffset, location.lineStartOffset);
        setExceptionLocation(result, eStart, eDivot, eEnd);
        return result;
    }
    
    StatementNode* createForInLoop(const JSTokenLocation& location, DestructuringPatternNode* pattern, ExpressionNode* iter, StatementNode* statements, const JSTokenLocation& declLocation, const JSTextPosition& eStart, const JSTextPosition& eDivot, const JSTextPosition& eEnd, int start, int end, VariableEnvironment&& lexicalVariables)
    {
        auto lexpr = new (m_parserArena) DestructuringAssignmentNode(declLocation, pattern, nullptr);
        return createForInLoop(location, lexpr, iter, statements, declLocation, eStart, eDivot, eEnd, start, end, WTFMove(lexicalVariables));
    }
    
    StatementNode* createForOfLoop(bool isForAwait, const JSTokenLocation& location, ExpressionNode* lhs, ExpressionNode* iter, StatementNode* statements, const JSTokenLocation&, const JSTextPosition& eStart, const JSTextPosition& eDivot, const JSTextPosition& eEnd, int start, int end, VariableEnvironment&& lexicalVariables)
    {
        ForOfNode* result = new (m_parserArena) ForOfNode(isForAwait, location, lhs, iter, statements, WTFMove(lexicalVariables));
        result->setLoc(start, end, location.startOffset, location.lineStartOffset);
        setExceptionLocation(result, eStart, eDivot, eEnd);
        return result;
    }
    
    StatementNode* createForOfLoop(bool isForAwait, const JSTokenLocation& location, DestructuringPatternNode* pattern, ExpressionNode* iter, StatementNode* statements, const JSTokenLocation& declLocation, const JSTextPosition& eStart, const JSTextPosition& eDivot, const JSTextPosition& eEnd, int start, int end, VariableEnvironment&& lexicalVariables)
    {
        auto lexpr = new (m_parserArena) DestructuringAssignmentNode(declLocation, pattern, nullptr);
        return createForOfLoop(isForAwait, location, lexpr, iter, statements, declLocation, eStart, eDivot, eEnd, start, end, WTFMove(lexicalVariables));
    }

    bool isBindingNode(const DestructuringPattern& pattern)
    {
        return pattern->isBindingNode();
    }

    bool isLocation(const Expression& node)
    {
        return node->isLocation();
    }

    bool isAssignmentLocation(const Expression& node)
    {
        return node->isAssignmentLocation();
    }

    bool isPrivateLocation(const Expression& node)
    {
        return node->isPrivateLocation();
    }

    bool isObjectLiteral(const Expression& node)
    {
        return node->isObjectLiteral();
    }

    bool isArrayLiteral(const Expression& node)
    {
        return node->isArrayLiteral();
    }

    bool isObjectOrArrayLiteral(const Expression& node)
    {
        return isObjectLiteral(node) || isArrayLiteral(node);
    }

    bool isFunctionCall(const Expression& node)
    {
        return node->isFunctionCall();
    }

    bool shouldSkipPauseLocation(StatementNode* statement) const
    {
        return !statement || statement->isLabel();
    }

    StatementNode* createEmptyStatement(const JSTokenLocation& location) { return new (m_parserArena) EmptyStatementNode(location); }

    StatementNode* createDeclarationStatement(const JSTokenLocation& location, ExpressionNode* expr, int start, int end)
    {
        StatementNode* result;
        result = new (m_parserArena) DeclarationStatement(location, expr);
        result->setLoc(start, end, location.startOffset, location.lineStartOffset);
        return result;
    }

    ExpressionNode* createEmptyVarExpression(const JSTokenLocation& location, const Identifier& identifier)
    {
        return new (m_parserArena) EmptyVarExpression(location, identifier);
    }

    ExpressionNode* createEmptyLetExpression(const JSTokenLocation& location, const Identifier& identifier)
    {
        return new (m_parserArena) EmptyLetExpression(location, identifier);
    }

    StatementNode* createReturnStatement(const JSTokenLocation& location, ExpressionNode* expression, const JSTextPosition& start, const JSTextPosition& end)
    {
        ReturnNode* result = new (m_parserArena) ReturnNode(location, expression);
        setExceptionLocation(result, start, end, end);
        result->setLoc(start.line, end.line, start.offset, start.lineStartOffset);
        return result;
    }

    StatementNode* createBreakStatement(const JSTokenLocation& location, const Identifier* ident, const JSTextPosition& start, const JSTextPosition& end)
    {
        BreakNode* result = new (m_parserArena) BreakNode(location, *ident);
        setExceptionLocation(result, start, end, end);
        result->setLoc(start.line, end.line, start.offset, start.lineStartOffset);
        return result;
    }

    StatementNode* createContinueStatement(const JSTokenLocation& location, const Identifier* ident, const JSTextPosition& start, const JSTextPosition& end)
    {
        ContinueNode* result = new (m_parserArena) ContinueNode(location, *ident);
        setExceptionLocation(result, start, end, end);
        result->setLoc(start.line, end.line, start.offset, start.lineStartOffset);
        return result;
    }

    StatementNode* createTryStatement(const JSTokenLocation& location, StatementNode* tryBlock, DestructuringPatternNode* catchPattern, StatementNode* catchBlock, StatementNode* finallyBlock, int startLine, int endLine, VariableEnvironment&& catchEnvironment)
    {
        TryNode* result = new (m_parserArena) TryNode(location, tryBlock, catchPattern, catchBlock, WTFMove(catchEnvironment), finallyBlock);
        result->setLoc(startLine, endLine, location.startOffset, location.lineStartOffset);
        return result;
    }

    StatementNode* createSwitchStatement(const JSTokenLocation& location, ExpressionNode* expr, ClauseListNode* firstClauses, CaseClauseNode* defaultClause, ClauseListNode* secondClauses, int startLine, int endLine, VariableEnvironment&& lexicalVariables, DeclarationStacks::FunctionStack&& functionStack)
    {
        CaseBlockNode* cases = new (m_parserArena) CaseBlockNode(firstClauses, defaultClause, secondClauses);
        SwitchNode* result = new (m_parserArena) SwitchNode(location, expr, cases, WTFMove(lexicalVariables), WTFMove(functionStack));
        result->setLoc(startLine, endLine, location.startOffset, location.lineStartOffset);
        return result;
    }

    StatementNode* createWhileStatement(const JSTokenLocation& location, ExpressionNode* expr, StatementNode* statement, int startLine, int endLine)
    {
        WhileNode* result = new (m_parserArena) WhileNode(location, expr, statement);
        result->setLoc(startLine, endLine, location.startOffset, location.lineStartOffset);
        return result;
    }

    StatementNode* createDoWhileStatement(const JSTokenLocation& location, StatementNode* statement, ExpressionNode* expr, int startLine, int endLine)
    {
        DoWhileNode* result = new (m_parserArena) DoWhileNode(location, statement, expr);
        result->setLoc(startLine, endLine, location.startOffset, location.lineStartOffset);
        return result;
    }

    StatementNode* createLabelStatement(const JSTokenLocation& location, const Identifier* ident, StatementNode* statement, const JSTextPosition& start, const JSTextPosition& end)
    {
        LabelNode* result = new (m_parserArena) LabelNode(location, *ident, statement);
        setExceptionLocation(result, start, end, end);
        return result;
    }

    StatementNode* createWithStatement(const JSTokenLocation& location, ExpressionNode* expr, StatementNode* statement, unsigned start, const JSTextPosition& end, unsigned startLine, unsigned endLine)
    {
        usesWith();
        WithNode* result = new (m_parserArena) WithNode(location, expr, statement, end, end - start);
        result->setLoc(startLine, endLine, location.startOffset, location.lineStartOffset);
        return result;
    }    
    
    StatementNode* createThrowStatement(const JSTokenLocation& location, ExpressionNode* expr, const JSTextPosition& start, const JSTextPosition& end)
    {
        ThrowNode* result = new (m_parserArena) ThrowNode(location, expr);
        result->setLoc(start.line, end.line, start.offset, start.lineStartOffset);
        setExceptionLocation(result, start, end, end);
        return result;
    }
    
    StatementNode* createDebugger(const JSTokenLocation& location, int startLine, int endLine)
    {
        DebuggerStatementNode* result = new (m_parserArena) DebuggerStatementNode(location);
        result->setLoc(startLine, endLine, location.startOffset, location.lineStartOffset);
        return result;
    }

    ModuleNameNode* createModuleName(const JSTokenLocation& location, const Identifier& moduleName)
    {
        return new (m_parserArena) ModuleNameNode(location, moduleName);
    }

    ImportSpecifierNode* createImportSpecifier(const JSTokenLocation& location, const Identifier& importedName, const Identifier& localName)
    {
        return new (m_parserArena) ImportSpecifierNode(location, importedName, localName);
    }

    ImportSpecifierListNode* createImportSpecifierList()
    {
        return new (m_parserArena) ImportSpecifierListNode();
    }

    void appendImportSpecifier(ImportSpecifierListNode* specifierList, ImportSpecifierNode* specifier)
    {
        specifierList->append(specifier);
    }

    StatementNode* createImportDeclaration(const JSTokenLocation& location, ImportSpecifierListNode* importSpecifierList, ModuleNameNode* moduleName)
    {
        return new (m_parserArena) ImportDeclarationNode(location, importSpecifierList, moduleName);
    }

    StatementNode* createExportAllDeclaration(const JSTokenLocation& location, ModuleNameNode* moduleName)
    {
        return new (m_parserArena) ExportAllDeclarationNode(location, moduleName);
    }

    StatementNode* createExportDefaultDeclaration(const JSTokenLocation& location, StatementNode* declaration, const Identifier& localName)
    {
        return new (m_parserArena) ExportDefaultDeclarationNode(location, declaration, localName);
    }

    StatementNode* createExportLocalDeclaration(const JSTokenLocation& location, StatementNode* declaration)
    {
        return new (m_parserArena) ExportLocalDeclarationNode(location, declaration);
    }

    StatementNode* createExportNamedDeclaration(const JSTokenLocation& location, ExportSpecifierListNode* exportSpecifierList, ModuleNameNode* moduleName)
    {
        return new (m_parserArena) ExportNamedDeclarationNode(location, exportSpecifierList, moduleName);
    }

    ExportSpecifierNode* createExportSpecifier(const JSTokenLocation& location, const Identifier& localName, const Identifier& exportedName)
    {
        return new (m_parserArena) ExportSpecifierNode(location, localName, exportedName);
    }

    ExportSpecifierListNode* createExportSpecifierList()
    {
        return new (m_parserArena) ExportSpecifierListNode();
    }

    void appendExportSpecifier(ExportSpecifierListNode* specifierList, ExportSpecifierNode* specifier)
    {
        specifierList->append(specifier);
    }

    void appendStatement(JSC::SourceElements* elements, JSC::StatementNode* statement)
    {
        elements->append(statement);
    }

    CommaNode* createCommaExpr(const JSTokenLocation& location, ExpressionNode* node)
    {
        return new (m_parserArena) CommaNode(location, node);
    }

    CommaNode* appendToCommaExpr(const JSTokenLocation& location, ExpressionNode*, ExpressionNode* tail, ExpressionNode* next)
    {
        ASSERT(tail->isCommaNode());
        ASSERT(next);
        CommaNode* newTail = new (m_parserArena) CommaNode(location, next);
        static_cast<CommaNode*>(tail)->setNext(newTail);
        return newTail;
    }

    int evalCount() const { return m_evalCount; }

    void appendBinaryExpressionInfo(int& operandStackDepth, ExpressionNode* current, const JSTextPosition& exprStart, const JSTextPosition& lhs, const JSTextPosition& rhs, bool hasAssignments)
    {
        operandStackDepth++;
        m_binaryOperandStack.append(std::make_pair(current, BinaryOpInfo(exprStart, lhs, rhs, hasAssignments)));
    }

    // Logic to handle datastructures used during parsing of binary expressions
    void operatorStackPop(int& operatorStackDepth)
    {
        operatorStackDepth--;
        m_binaryOperatorStack.removeLast();
    }
    bool operatorStackShouldReduce(int precedence)
    {
        // If the current precedence of the operator stack is the same to the one of the given operator,
        // it depends on the associative whether we reduce the stack.
        // If the operator is right associative, we should not reduce the stack right now.
        if (precedence == m_binaryOperatorStack.last().second)
            return !(m_binaryOperatorStack.last().first & RightAssociativeBinaryOpTokenFlag);
        return precedence < m_binaryOperatorStack.last().second;
    }
    const BinaryOperand& getFromOperandStack(int i) { return m_binaryOperandStack[m_binaryOperandStack.size() + i]; }
    void shrinkOperandStackBy(int& operandStackDepth, int amount)
    {
        operandStackDepth -= amount;
        ASSERT(operandStackDepth >= 0);
        m_binaryOperandStack.shrink(m_binaryOperandStack.size() - amount);
    }
    void appendBinaryOperation(const JSTokenLocation& location, int& operandStackDepth, int&, const BinaryOperand& lhs, const BinaryOperand& rhs)
    {
        operandStackDepth++;
        m_binaryOperandStack.append(std::make_pair(makeBinaryNode(location, m_binaryOperatorStack.last().first, lhs, rhs), BinaryOpInfo(lhs.second, rhs.second)));
    }
    void operatorStackAppend(int& operatorStackDepth, int op, int precedence)
    {
        operatorStackDepth++;
        m_binaryOperatorStack.append(std::make_pair(op, precedence));
    }
    ExpressionNode* popOperandStack(int&)
    {
        ExpressionNode* result = m_binaryOperandStack.last().first;
        m_binaryOperandStack.removeLast();
        return result;
    }
    
    void appendUnaryToken(int& tokenStackDepth, int type, const JSTextPosition& start)
    {
        tokenStackDepth++;
        m_unaryTokenStack.append(std::make_pair(type, start));
    }

    int unaryTokenStackLastType(int&)
    {
        return m_unaryTokenStack.last().first;
    }
    
    const JSTextPosition& unaryTokenStackLastStart(int&)
    {
        return m_unaryTokenStack.last().second;
    }
    
    void unaryTokenStackRemoveLast(int& tokenStackDepth)
    {
        tokenStackDepth--;
        m_unaryTokenStack.removeLast();
    }

    int unaryTokenStackDepth() const
    {
        return m_unaryTokenStack.size();
    }

    void setUnaryTokenStackDepth(int oldDepth)
    {
        m_unaryTokenStack.shrink(oldDepth);
    }
    
    void assignmentStackAppend(int& assignmentStackDepth, ExpressionNode* node, const JSTextPosition& start, const JSTextPosition& divot, int assignmentCount, Operator op)
    {
        assignmentStackDepth++;
        ASSERT(start.offset >= start.lineStartOffset);
        ASSERT(divot.offset >= divot.lineStartOffset);
        m_assignmentInfoStack.append(AssignmentInfo(node, start, divot, assignmentCount, op));
    }

    ExpressionNode* createAssignment(const JSTokenLocation& location, int& assignmentStackDepth, ExpressionNode* rhs, int initialAssignmentCount, int currentAssignmentCount, const JSTextPosition& lastTokenEnd)
    {
        AssignmentInfo& info = m_assignmentInfoStack.last();
        ExpressionNode* result = makeAssignNode(location, info.m_node, info.m_op, rhs, info.m_initAssignments != initialAssignmentCount, info.m_initAssignments != currentAssignmentCount, info.m_start, info.m_divot + 1, lastTokenEnd);
        m_assignmentInfoStack.removeLast();
        assignmentStackDepth--;
        return result;
    }

    PropertyNode::Type getType(const Property& property) const { return property->type(); }
    bool isUnderscoreProtoSetter(const Property& property) const { return PropertyNode::isUnderscoreProtoSetter(m_vm, *property); }
    bool isResolve(ExpressionNode* expr) const { return expr->isResolveNode(); }

    ExpressionNode* createDestructuringAssignment(const JSTokenLocation& location, DestructuringPattern pattern, ExpressionNode* initializer)
    {
        return new (m_parserArena) DestructuringAssignmentNode(location, pattern, initializer);
    }
    
    ArrayPattern createArrayPattern(const JSTokenLocation&)
    {
        return new (m_parserArena) ArrayPatternNode();
    }
    
    void appendArrayPatternSkipEntry(ArrayPattern node, const JSTokenLocation& location)
    {
        node->appendIndex(ArrayPatternNode::BindingType::Elision, location, nullptr, nullptr);
    }

    void appendArrayPatternEntry(ArrayPattern node, const JSTokenLocation& location, DestructuringPattern pattern, ExpressionNode* defaultValue)
    {
        node->appendIndex(ArrayPatternNode::BindingType::Element, location, pattern, defaultValue);
        tryInferNameInPattern(pattern, defaultValue);
    }

    void appendArrayPatternRestEntry(ArrayPattern node, const JSTokenLocation& location, DestructuringPattern pattern)
    {
        node->appendIndex(ArrayPatternNode::BindingType::RestElement, location, pattern, nullptr);
    }

    void finishArrayPattern(ArrayPattern node, const JSTextPosition& divotStart, const JSTextPosition& divot, const JSTextPosition& divotEnd)
    {
        setExceptionLocation(node, divotStart, divot, divotEnd);
    }
    
    ObjectPattern createObjectPattern(const JSTokenLocation&)
    {
        return new (m_parserArena) ObjectPatternNode();
    }
    
    void appendObjectPatternEntry(ObjectPattern node, const JSTokenLocation& location, bool wasString, const Identifier& identifier, DestructuringPattern pattern, ExpressionNode* defaultValue)
    {
        node->appendEntry(location, identifier, wasString, pattern, defaultValue, ObjectPatternNode::BindingType::Element);
        tryInferNameInPattern(pattern, defaultValue);
    }

    void appendObjectPatternEntry(VM& vm, ObjectPattern node, const JSTokenLocation& location, ExpressionNode* propertyExpression, DestructuringPattern pattern, ExpressionNode* defaultValue)
    {
        node->appendEntry(vm, location, propertyExpression, pattern, defaultValue, ObjectPatternNode::BindingType::Element);
        tryInferNameInPattern(pattern, defaultValue);
    }
    
    void appendObjectPatternRestEntry(VM& vm, ObjectPattern node, const JSTokenLocation& location, DestructuringPattern pattern)
    {
        node->appendEntry(vm, location, nullptr, pattern, nullptr, ObjectPatternNode::BindingType::RestElement);
    }

    void setContainsObjectRestElement(ObjectPattern node, bool containsRestElement)
    {
        node->setContainsRestElement(containsRestElement);
    }
    
    void setContainsComputedProperty(ObjectPattern node, bool containsComputedProperty)
    {
        node->setContainsComputedProperty(containsComputedProperty);
    }

    BindingPattern createBindingLocation(const JSTokenLocation&, const Identifier& boundProperty, const JSTextPosition& start, const JSTextPosition& end, AssignmentContext context)
    {
        return new (m_parserArena) BindingNode(boundProperty, start, end, context);
    }

    RestParameterNode* createRestParameter(DestructuringPatternNode* pattern, size_t numParametersToSkip)
    {
        return new (m_parserArena) RestParameterNode(pattern, numParametersToSkip);
    }

    AssignmentElement createAssignmentElement(const Expression& assignmentTarget, const JSTextPosition& start, const JSTextPosition& end)
    {
        return new (m_parserArena) AssignmentElementNode(assignmentTarget, start, end);
    }

    void setEndOffset(Node* node, int offset)
    {
        node->setEndOffset(offset);
    }

    int endOffset(Node* node)
    {
        return node->endOffset();
    }

    void setStartOffset(CaseClauseNode* node, int offset)
    {
        node->setStartOffset(offset);
    }

    void setStartOffset(Node* node, int offset)
    {
        node->setStartOffset(offset);
    }

    JSTextPosition breakpointLocation(Node* node)
    {
        node->setNeedsDebugHook();
        return node->position();
    }

    void propagateArgumentsUse() { usesArguments(); }
    
private:
    struct Scope {
        Scope()
            : m_features(0)
            , m_numConstants(0)
        {
        }
        int m_features;
        int m_numConstants;
    };

    static void setExceptionLocation(ThrowableExpressionData* node, const JSTextPosition& divotStart, const JSTextPosition& divot, const JSTextPosition& divotEnd)
    {
        ASSERT(divot.offset >= divot.lineStartOffset);
        node->setExceptionSourceCode(divot, divotStart, divotEnd);
    }

    void incConstants() { m_scope.m_numConstants++; }
    void usesThis() { m_scope.m_features |= ThisFeature; }
    void usesArrowFunction() { m_scope.m_features |= ArrowFunctionFeature; }
    void usesArguments() { m_scope.m_features |= ArgumentsFeature; }
    void usesWith() { m_scope.m_features |= WithFeature; }
    void usesSuperCall() { m_scope.m_features |= SuperCallFeature; }
    void usesSuperProperty() { m_scope.m_features |= SuperPropertyFeature; }
    void usesEval() 
    {
        m_evalCount++;
        m_scope.m_features |= EvalFeature;
    }
    void usesNewTarget() { m_scope.m_features |= NewTargetFeature; }
    void usesAwait() { m_scope.m_features |= AwaitFeature; }
    ExpressionNode* createIntegerLikeNumber(const JSTokenLocation& location, double d)
    {
        return new (m_parserArena) IntegerNode(location, d);
    }
    ExpressionNode* createDoubleLikeNumber(const JSTokenLocation& location, double d)
    {
        return new (m_parserArena) DoubleNode(location, d);
    }
    ExpressionNode* createBigIntWithSign(const JSTokenLocation& location, const Identifier& bigInt, uint8_t radix, bool sign)
    {
        return new (m_parserArena) BigIntNode(location, bigInt, radix, sign);
    }
    ExpressionNode* createNumberFromBinaryOperation(const JSTokenLocation& location, double value, const NumberNode& originalNodeA, const NumberNode& originalNodeB)
    {
        if (originalNodeA.isIntegerNode() && originalNodeB.isIntegerNode())
            return createIntegerLikeNumber(location, value);
        return createDoubleLikeNumber(location, value);
    }
    ExpressionNode* createNumberFromUnaryOperation(const JSTokenLocation& location, double value, const NumberNode& originalNode)
    {
        if (originalNode.isIntegerNode())
            return createIntegerLikeNumber(location, value);
        return createDoubleLikeNumber(location, value);
    }
    ExpressionNode* createBigIntFromUnaryOperation(const JSTokenLocation& location, bool sign, const BigIntNode& originalNode)
    {
        return createBigIntWithSign(location, originalNode.identifier(), originalNode.radix(), sign);
    }

    void tryInferNameInPattern(DestructuringPattern pattern, ExpressionNode* defaultValue)
    {
        if (!defaultValue)
            return;

        if (pattern->isBindingNode()) {
            const Identifier& ident = static_cast<BindingNode*>(pattern)->boundProperty();
            tryInferNameInPatternWithIdentifier(ident, defaultValue);
        } else if (pattern->isAssignmentElementNode()) {
            const ExpressionNode* assignmentTarget = static_cast<AssignmentElementNode*>(pattern)->assignmentTarget();
            if (assignmentTarget->isResolveNode()) {
                const Identifier& ident = static_cast<const ResolveNode*>(assignmentTarget)->identifier();
                tryInferNameInPatternWithIdentifier(ident, defaultValue);
            }
        }
    }

    void tryInferNameInPatternWithIdentifier(const Identifier& ident, ExpressionNode* defaultValue)
    {
        if (defaultValue->isBaseFuncExprNode()) {
            auto metadata = static_cast<BaseFuncExprNode*>(defaultValue)->metadata();
            metadata->setEcmaName(ident);
        } else if (defaultValue->isClassExprNode())
            static_cast<ClassExprNode*>(defaultValue)->setEcmaName(ident);
    }

    VM& m_vm;
    ParserArena& m_parserArena;
    SourceCode* m_sourceCode;
    Scope m_scope;
    Vector<BinaryOperand, 10, UnsafeVectorOverflow> m_binaryOperandStack;
    Vector<AssignmentInfo, 10, UnsafeVectorOverflow> m_assignmentInfoStack;
    Vector<std::pair<int, int>, 10, UnsafeVectorOverflow> m_binaryOperatorStack;
    Vector<std::pair<int, JSTextPosition>, 10, UnsafeVectorOverflow> m_unaryTokenStack;
    int m_evalCount;
};

ExpressionNode* ASTBuilder::makeTypeOfNode(const JSTokenLocation& location, ExpressionNode* expr)
{
    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        return new (m_parserArena) TypeOfResolveNode(location, resolve->identifier());
    }
    return new (m_parserArena) TypeOfValueNode(location, expr);
}

ExpressionNode* ASTBuilder::makeDeleteNode(const JSTokenLocation& location, ExpressionNode* expr, const JSTextPosition& start, const JSTextPosition& divot, const JSTextPosition& end)
{
    if (expr->isOptionalChain()) {
        OptionalChainNode* optionalChain = static_cast<OptionalChainNode*>(expr);
        if (optionalChain->expr()->isLocation()) {
            ASSERT(!optionalChain->expr()->isResolveNode());
            optionalChain->setExpr(makeDeleteNode(location, optionalChain->expr(), start, divot, end));
            return optionalChain;
        }
    }

    if (!expr->isLocation())
        return new (m_parserArena) DeleteValueNode(location, expr);
    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        return new (m_parserArena) DeleteResolveNode(location, resolve->identifier(), divot, start, end);
    }
    if (expr->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(expr);
        return new (m_parserArena) DeleteBracketNode(location, bracket->base(), bracket->subscript(), divot, start, end);
    }
    ASSERT(expr->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(expr);
    return new (m_parserArena) DeleteDotNode(location, dot->base(), dot->identifier(), divot, start, end);
}

ExpressionNode* ASTBuilder::makeNegateNode(const JSTokenLocation& location, ExpressionNode* n)
{
    if (n->isNumber()) {
        const NumberNode& numberNode = static_cast<const NumberNode&>(*n);
        return createNumberFromUnaryOperation(location, -numberNode.value(), numberNode);
    }

    if (n->isBigInt()) {
        const BigIntNode& bigIntNode = static_cast<const BigIntNode&>(*n);
        return createBigIntFromUnaryOperation(location, !bigIntNode.sign(), bigIntNode);
    }

    return new (m_parserArena) NegateNode(location, n);
}

ExpressionNode* ASTBuilder::makeBitwiseNotNode(const JSTokenLocation& location, ExpressionNode* expr)
{
    if (expr->isNumber())
        return createIntegerLikeNumber(location, ~toInt32(static_cast<NumberNode*>(expr)->value()));
    return new (m_parserArena) BitwiseNotNode(location, expr);
}

ExpressionNode* ASTBuilder::makePowNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    auto* strippedExpr1 = expr1->stripUnaryPlus();
    auto* strippedExpr2 = expr2->stripUnaryPlus();

    if (strippedExpr1->isNumber() && strippedExpr2->isNumber()) {
        const NumberNode& numberExpr1 = static_cast<NumberNode&>(*strippedExpr1);
        const NumberNode& numberExpr2 = static_cast<NumberNode&>(*strippedExpr2);
        return createNumberFromBinaryOperation(location, operationMathPow(numberExpr1.value(), numberExpr2.value()), numberExpr1, numberExpr2);
    }

    if (strippedExpr1->isNumber())
        expr1 = strippedExpr1;
    if (strippedExpr2->isNumber())
        expr2 = strippedExpr2;

    return new (m_parserArena) PowNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeMultNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    // FIXME: Unary + change the evaluation order.
    // https://bugs.webkit.org/show_bug.cgi?id=159968
    expr1 = expr1->stripUnaryPlus();
    expr2 = expr2->stripUnaryPlus();

    if (expr1->isNumber() && expr2->isNumber()) {
        const NumberNode& numberExpr1 = static_cast<NumberNode&>(*expr1);
        const NumberNode& numberExpr2 = static_cast<NumberNode&>(*expr2);
        return createNumberFromBinaryOperation(location, numberExpr1.value() * numberExpr2.value(), numberExpr1, numberExpr2);
    }

    if (expr1->isNumber() && static_cast<NumberNode*>(expr1)->value() == 1)
        return new (m_parserArena) UnaryPlusNode(location, expr2);

    if (expr2->isNumber() && static_cast<NumberNode*>(expr2)->value() == 1)
        return new (m_parserArena) UnaryPlusNode(location, expr1);

    return new (m_parserArena) MultNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeDivNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    // FIXME: Unary + change the evaluation order.
    // https://bugs.webkit.org/show_bug.cgi?id=159968
    expr1 = expr1->stripUnaryPlus();
    expr2 = expr2->stripUnaryPlus();

    if (expr1->isNumber() && expr2->isNumber()) {
        const NumberNode& numberExpr1 = static_cast<NumberNode&>(*expr1);
        const NumberNode& numberExpr2 = static_cast<NumberNode&>(*expr2);
        double result = numberExpr1.value() / numberExpr2.value();
        if (static_cast<int64_t>(result) == result)
            return createNumberFromBinaryOperation(location, result, numberExpr1, numberExpr2);
        return createDoubleLikeNumber(location, result);
    }
    return new (m_parserArena) DivNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeModNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    // FIXME: Unary + change the evaluation order.
    // https://bugs.webkit.org/show_bug.cgi?id=159968
    expr1 = expr1->stripUnaryPlus();
    expr2 = expr2->stripUnaryPlus();

    if (expr1->isNumber() && expr2->isNumber()) {
        const NumberNode& numberExpr1 = static_cast<NumberNode&>(*expr1);
        const NumberNode& numberExpr2 = static_cast<NumberNode&>(*expr2);
        return createIntegerLikeNumber(location, fmod(numberExpr1.value(), numberExpr2.value()));
    }
    return new (m_parserArena) ModNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeAddNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{

    if (expr1->isNumber() && expr2->isNumber()) {
        const NumberNode& numberExpr1 = static_cast<NumberNode&>(*expr1);
        const NumberNode& numberExpr2 = static_cast<NumberNode&>(*expr2);
        return createNumberFromBinaryOperation(location, numberExpr1.value() + numberExpr2.value(), numberExpr1, numberExpr2);
    }
    return new (m_parserArena) AddNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeSubNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    // FIXME: Unary + change the evaluation order.
    // https://bugs.webkit.org/show_bug.cgi?id=159968
    expr1 = expr1->stripUnaryPlus();
    expr2 = expr2->stripUnaryPlus();

    if (expr1->isNumber() && expr2->isNumber()) {
        const NumberNode& numberExpr1 = static_cast<NumberNode&>(*expr1);
        const NumberNode& numberExpr2 = static_cast<NumberNode&>(*expr2);
        return createNumberFromBinaryOperation(location, numberExpr1.value() - numberExpr2.value(), numberExpr1, numberExpr2);
    }
    return new (m_parserArena) SubNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeLeftShiftNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber()) {
        const NumberNode& numberExpr1 = static_cast<NumberNode&>(*expr1);
        const NumberNode& numberExpr2 = static_cast<NumberNode&>(*expr2);
        return createIntegerLikeNumber(location, toInt32(numberExpr1.value()) << (toUInt32(numberExpr2.value()) & 0x1f));
    }
    return new (m_parserArena) LeftShiftNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeRightShiftNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber()) {
        const NumberNode& numberExpr1 = static_cast<NumberNode&>(*expr1);
        const NumberNode& numberExpr2 = static_cast<NumberNode&>(*expr2);
        return createIntegerLikeNumber(location, toInt32(numberExpr1.value()) >> (toUInt32(numberExpr2.value()) & 0x1f));
    }
    return new (m_parserArena) RightShiftNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeURightShiftNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber()) {
        const NumberNode& numberExpr1 = static_cast<NumberNode&>(*expr1);
        const NumberNode& numberExpr2 = static_cast<NumberNode&>(*expr2);
        return createIntegerLikeNumber(location, toUInt32(numberExpr1.value()) >> (toUInt32(numberExpr2.value()) & 0x1f));
    }
    return new (m_parserArena) UnsignedRightShiftNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeBitOrNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber()) {
        const NumberNode& numberExpr1 = static_cast<NumberNode&>(*expr1);
        const NumberNode& numberExpr2 = static_cast<NumberNode&>(*expr2);
        return createIntegerLikeNumber(location, toInt32(numberExpr1.value()) | toInt32(numberExpr2.value()));
    }
    return new (m_parserArena) BitOrNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeBitAndNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber()) {
        const NumberNode& numberExpr1 = static_cast<NumberNode&>(*expr1);
        const NumberNode& numberExpr2 = static_cast<NumberNode&>(*expr2);
        return createIntegerLikeNumber(location, toInt32(numberExpr1.value()) & toInt32(numberExpr2.value()));
    }
    return new (m_parserArena) BitAndNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeBitXOrNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber()) {
        const NumberNode& numberExpr1 = static_cast<NumberNode&>(*expr1);
        const NumberNode& numberExpr2 = static_cast<NumberNode&>(*expr2);
        return createIntegerLikeNumber(location, toInt32(numberExpr1.value()) ^ toInt32(numberExpr2.value()));
    }
    return new (m_parserArena) BitXOrNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeCoalesceNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2)
{
    // Optimization for `x?.y ?? z`.
    if (expr1->isOptionalChain()) {
        OptionalChainNode* optionalChain = static_cast<OptionalChainNode*>(expr1);
        if (!optionalChain->expr()->isDeleteNode()) {
            constexpr bool hasAbsorbedOptionalChain = true;
            return new (m_parserArena) CoalesceNode(location, optionalChain->expr(), expr2, hasAbsorbedOptionalChain);
        }
    }
    constexpr bool hasAbsorbedOptionalChain = false;
    return new (m_parserArena) CoalesceNode(location, expr1, expr2, hasAbsorbedOptionalChain);
}

ExpressionNode* ASTBuilder::makeFunctionCallNode(const JSTokenLocation& location, ExpressionNode* func, bool previousBaseWasSuper, ArgumentsNode* args, const JSTextPosition& divotStart, const JSTextPosition& divot, const JSTextPosition& divotEnd, size_t callOrApplyChildDepth, bool isOptionalCall)
{
    ASSERT(divot.offset >= divot.lineStartOffset);
    if (func->isSuperNode())
        usesSuperCall();

    if (func->isBytecodeIntrinsicNode()) {
        ASSERT(!isOptionalCall);
        BytecodeIntrinsicNode* intrinsic = static_cast<BytecodeIntrinsicNode*>(func);
        if (intrinsic->type() == BytecodeIntrinsicNode::Type::Constant && intrinsic->entry().type() == BytecodeIntrinsicRegistry::Type::Emitter)
            return new (m_parserArena) BytecodeIntrinsicNode(BytecodeIntrinsicNode::Type::Function, location, intrinsic->entry(), intrinsic->identifier(), args, divot, divotStart, divotEnd);
    }

    if (func->isOptionalChain()) {
        OptionalChainNode* optionalChain = static_cast<OptionalChainNode*>(func);
        if (optionalChain->expr()->isLocation()) {
            ASSERT(!optionalChain->expr()->isResolveNode());
            // We must take care to preserve our `this` value in cases like `a?.b?.()` and `(a?.b)()`, respectively.
            if (isOptionalCall)
                return makeFunctionCallNode(location, optionalChain->expr(), previousBaseWasSuper, args, divotStart, divot, divotEnd, callOrApplyChildDepth, isOptionalCall);  
            optionalChain->setExpr(makeFunctionCallNode(location, optionalChain->expr(), previousBaseWasSuper, args, divotStart, divot, divotEnd, callOrApplyChildDepth, isOptionalCall));
            return optionalChain;
        }
    }

    if (!func->isLocation())
        return new (m_parserArena) FunctionCallValueNode(location, func, args, divot, divotStart, divotEnd);
    if (func->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(func);
        const Identifier& identifier = resolve->identifier();
        if (identifier == m_vm.propertyNames->eval && !isOptionalCall) {
            usesEval();
            return new (m_parserArena) EvalFunctionCallNode(location, args, divot, divotStart, divotEnd);
        }
        return new (m_parserArena) FunctionCallResolveNode(location, identifier, args, divot, divotStart, divotEnd);
    }
    if (func->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(func);
        FunctionCallBracketNode* node = new (m_parserArena) FunctionCallBracketNode(location, bracket->base(), bracket->subscript(), bracket->subscriptHasAssignments(), args, divot, divotStart, divotEnd);
        node->setSubexpressionInfo(bracket->divot(), bracket->divotEnd().offset);
        return node;
    }
    ASSERT(func->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(func);
    FunctionCallDotNode* node = nullptr;
    if (!previousBaseWasSuper && (dot->identifier() == m_vm.propertyNames->builtinNames().callPublicName() || dot->identifier() == m_vm.propertyNames->builtinNames().callPrivateName()))
        node = new (m_parserArena) CallFunctionCallDotNode(location, dot->base(), dot->identifier(), dot->type(), args, divot, divotStart, divotEnd, callOrApplyChildDepth);
    else if (!previousBaseWasSuper && (dot->identifier() == m_vm.propertyNames->builtinNames().applyPublicName() || dot->identifier() == m_vm.propertyNames->builtinNames().applyPrivateName())) {
        // FIXME: This check is only needed because we haven't taught the bytecode generator to inline
        // Reflect.apply yet. See https://bugs.webkit.org/show_bug.cgi?id=190668.
        if (!dot->base()->isResolveNode() || static_cast<ResolveNode*>(dot->base())->identifier() != m_vm.propertyNames->Reflect)
            node = new (m_parserArena) ApplyFunctionCallDotNode(location, dot->base(), dot->identifier(), dot->type(), args, divot, divotStart, divotEnd, callOrApplyChildDepth);
    } else if (!previousBaseWasSuper 
        && dot->identifier() == m_vm.propertyNames->hasOwnProperty
        && args->m_listNode
        && args->m_listNode->m_expr
        && args->m_listNode->m_expr->isResolveNode()
        && !args->m_listNode->m_next
        && (dot->base()->isResolveNode() || dot->base()->isThisNode())) {
        // We match the AST pattern:
        // <resolveNode|thisNode>.hasOwnProperty(<resolveNode>)
        // i.e:
        // o.hasOwnProperty(p)
        node = new (m_parserArena) HasOwnPropertyFunctionCallDotNode(location, dot->base(), dot->identifier(), dot->type(), args, divot, divotStart, divotEnd);
    }
    if (!node)
        node = new (m_parserArena) FunctionCallDotNode(location, dot->base(), dot->identifier(), dot->type(), args, divot, divotStart, divotEnd);
    node->setSubexpressionInfo(dot->divot(), dot->divotEnd().offset);
    return node;
}

ExpressionNode* ASTBuilder::makeBinaryNode(const JSTokenLocation& location, int token, std::pair<ExpressionNode*, BinaryOpInfo> lhs, std::pair<ExpressionNode*, BinaryOpInfo> rhs)
{
    switch (token) {
    case COALESCE:
        return makeCoalesceNode(location, lhs.first, rhs.first);

    case OR:
        return new (m_parserArena) LogicalOpNode(location, lhs.first, rhs.first, LogicalOperator::Or);

    case AND:
        return new (m_parserArena) LogicalOpNode(location, lhs.first, rhs.first, LogicalOperator::And);

    case BITOR:
        return makeBitOrNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case BITXOR:
        return makeBitXOrNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case BITAND:
        return makeBitAndNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case EQEQ:
        return new (m_parserArena) EqualNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case NE:
        return new (m_parserArena) NotEqualNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case STREQ:
        return new (m_parserArena) StrictEqualNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case STRNEQ:
        return new (m_parserArena) NotStrictEqualNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case LT:
        return new (m_parserArena) LessNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case GT:
        return new (m_parserArena) GreaterNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case LE:
        return new (m_parserArena) LessEqNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case GE:
        return new (m_parserArena) GreaterEqNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case INSTANCEOF: {
        InstanceOfNode* node = new (m_parserArena) InstanceOfNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);
        setExceptionLocation(node, lhs.second.start, rhs.second.start, rhs.second.end);
        return node;
    }

    case INTOKEN: {
        InNode* node = new (m_parserArena) InNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);
        setExceptionLocation(node, lhs.second.start, rhs.second.start, rhs.second.end);
        return node;
    }

    case LSHIFT:
        return makeLeftShiftNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case RSHIFT:
        return makeRightShiftNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case URSHIFT:
        return makeURightShiftNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case PLUS:
        return makeAddNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case MINUS:
        return makeSubNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case TIMES:
        return makeMultNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case DIVIDE:
        return makeDivNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case MOD:
        return makeModNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case POW:
        return makePowNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);
    }
    CRASH();
    return nullptr;
}

ExpressionNode* ASTBuilder::makeAssignNode(const JSTokenLocation& location, ExpressionNode* loc, Operator op, ExpressionNode* expr, bool locHasAssignments, bool exprHasAssignments, const JSTextPosition& start, const JSTextPosition& divot, const JSTextPosition& end)
{
    if (!loc->isLocation()) {
        ASSERT(loc->isFunctionCall());
        return new (m_parserArena) AssignErrorNode(location, divot, start, end);
    }

    if (loc->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(loc);

        if (op == Operator::Equal) {
            if (expr->isBaseFuncExprNode()) {
                auto metadata = static_cast<BaseFuncExprNode*>(expr)->metadata();
                metadata->setEcmaName(resolve->identifier());
            } else if (expr->isClassExprNode())
                static_cast<ClassExprNode*>(expr)->setEcmaName(resolve->identifier());
            AssignResolveNode* node = new (m_parserArena) AssignResolveNode(location, resolve->identifier(), expr, AssignmentContext::AssignmentExpression);
            setExceptionLocation(node, start, divot, end);
            return node;
        }

        if (op == Operator::CoalesceEq || op == Operator::OrEq || op == Operator::AndEq) {
            if (expr->isBaseFuncExprNode()) {
                auto metadata = static_cast<BaseFuncExprNode*>(expr)->metadata();
                metadata->setEcmaName(resolve->identifier());
            } else if (expr->isClassExprNode())
                static_cast<ClassExprNode*>(expr)->setEcmaName(resolve->identifier());
            return new (m_parserArena) ShortCircuitReadModifyResolveNode(location, resolve->identifier(), op, expr, exprHasAssignments, divot, start, end);
        }

        return new (m_parserArena) ReadModifyResolveNode(location, resolve->identifier(), op, expr, exprHasAssignments, divot, start, end);
    }

    if (loc->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(loc);

        if (op == Operator::Equal)
            return new (m_parserArena) AssignBracketNode(location, bracket->base(), bracket->subscript(), expr, locHasAssignments, exprHasAssignments, bracket->divot(), start, end);

        if (op == Operator::CoalesceEq || op == Operator::OrEq || op == Operator::AndEq) {
            auto* node = new (m_parserArena) ShortCircuitReadModifyBracketNode(location, bracket->base(), bracket->subscript(), op, expr, locHasAssignments, exprHasAssignments, divot, start, end);
            node->setSubexpressionInfo(bracket->divot(), bracket->divotEnd().offset);
            return node;
        }

        ReadModifyBracketNode* node = new (m_parserArena) ReadModifyBracketNode(location, bracket->base(), bracket->subscript(), op, expr, locHasAssignments, exprHasAssignments, divot, start, end);
        node->setSubexpressionInfo(bracket->divot(), bracket->divotEnd().offset);
        return node;
    }

    ASSERT(loc->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(loc);

    if (op == Operator::Equal)
        return new (m_parserArena) AssignDotNode(location, dot->base(), dot->identifier(), dot->type(), expr, exprHasAssignments, dot->divot(), start, end);

    if (op == Operator::CoalesceEq || op == Operator::OrEq || op == Operator::AndEq) {
        auto* node = new (m_parserArena) ShortCircuitReadModifyDotNode(location, dot->base(), dot->identifier(), op, expr, exprHasAssignments, divot, start, end);
        node->setSubexpressionInfo(dot->divot(), dot->divotEnd().offset);
        return node;
    }

    ReadModifyDotNode* node = new (m_parserArena) ReadModifyDotNode(location, dot->base(), dot->identifier(), dot->type(), op, expr, exprHasAssignments, divot, start, end);
    node->setSubexpressionInfo(dot->divot(), dot->divotEnd().offset);
    return node;
}

ExpressionNode* ASTBuilder::makePrefixNode(const JSTokenLocation& location, ExpressionNode* expr, Operator op, const JSTextPosition& start, const JSTextPosition& divot, const JSTextPosition& end)
{
    return new (m_parserArena) PrefixNode(location, expr, op, divot, start, end);
}

ExpressionNode* ASTBuilder::makePostfixNode(const JSTokenLocation& location, ExpressionNode* expr, Operator op, const JSTextPosition& start, const JSTextPosition& divot, const JSTextPosition& end)
{
    return new (m_parserArena) PostfixNode(location, expr, op, divot, start, end);
}

}
