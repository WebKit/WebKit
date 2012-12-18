/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef ASTBuilder_h
#define ASTBuilder_h

#include "NodeConstructors.h"
#include "SyntaxChecker.h"
#include <utility>

namespace JSC {

class ASTBuilder {
    struct BinaryOpInfo {
        BinaryOpInfo() {}
        BinaryOpInfo(int s, int d, int e, bool r)
            : start(s)
            , divot(d)
            , end(e)
            , hasAssignment(r)
        {
        }
        BinaryOpInfo(const BinaryOpInfo& lhs, const BinaryOpInfo& rhs)
            : start(lhs.start)
            , divot(rhs.start)
            , end(rhs.end)
            , hasAssignment(lhs.hasAssignment || rhs.hasAssignment)
        {
        }
        int start;
        int divot;
        int end;
        bool hasAssignment;
    };
    
    
    struct AssignmentInfo {
        AssignmentInfo() {}
        AssignmentInfo(ExpressionNode* node, int start, int divot, int initAssignments, Operator op)
            : m_node(node)
            , m_start(start)
            , m_divot(divot)
            , m_initAssignments(initAssignments)
            , m_op(op)
        {
        }
        ExpressionNode* m_node;
        int m_start;
        int m_divot;
        int m_initAssignments;
        Operator m_op;
    };
public:
    ASTBuilder(JSGlobalData* globalData, SourceCode* sourceCode)
        : m_globalData(globalData)
        , m_sourceCode(sourceCode)
        , m_scope(globalData)
        , m_evalCount(0)
    {
    }
    
    struct BinaryExprContext {
        BinaryExprContext(ASTBuilder&) {}
    };
    struct UnaryExprContext {
        UnaryExprContext(ASTBuilder&) {}
    };

    typedef SyntaxChecker FunctionBodyBuilder;

    typedef ExpressionNode* Expression;
    typedef JSC::SourceElements* SourceElements;
    typedef ArgumentsNode* Arguments;
    typedef CommaNode* Comma;
    typedef PropertyNode* Property;
    typedef PropertyListNode* PropertyList;
    typedef ElementNode* ElementList;
    typedef ArgumentListNode* ArgumentsList;
    typedef ParameterNode* FormalParameterList;
    typedef FunctionBodyNode* FunctionBody;
    typedef StatementNode* Statement;
    typedef ClauseListNode* ClauseList;
    typedef CaseClauseNode* Clause;
    typedef ConstDeclNode* ConstDeclList;
    typedef std::pair<ExpressionNode*, BinaryOpInfo> BinaryOperand;
    
    static const bool CreatesAST = true;
    static const bool NeedsFreeVariableInfo = true;
    static const bool CanUseFunctionCache = true;
    static const int  DontBuildKeywords = 0;
    static const int  DontBuildStrings = 0;

    ExpressionNode* makeBinaryNode(const JSTokenLocation&, int token, std::pair<ExpressionNode*, BinaryOpInfo>, std::pair<ExpressionNode*, BinaryOpInfo>);
    ExpressionNode* makeFunctionCallNode(const JSTokenLocation&, ExpressionNode* func, ArgumentsNode* args, int start, int divot, int end);

    JSC::SourceElements* createSourceElements() { return new (m_globalData) JSC::SourceElements(); }

    ParserArenaData<DeclarationStacks::VarStack>* varDeclarations() { return m_scope.m_varDeclarations; }
    ParserArenaData<DeclarationStacks::FunctionStack>* funcDeclarations() { return m_scope.m_funcDeclarations; }
    int features() const { return m_scope.m_features; }
    int numConstants() const { return m_scope.m_numConstants; }

    void appendToComma(CommaNode* commaNode, ExpressionNode* expr) { commaNode->append(expr); }

    CommaNode* createCommaExpr(const JSTokenLocation& location, ExpressionNode* lhs, ExpressionNode* rhs) { return new (m_globalData) CommaNode(location, lhs, rhs); }

    ExpressionNode* makeAssignNode(const JSTokenLocation&, ExpressionNode* left, Operator, ExpressionNode* right, bool leftHasAssignments, bool rightHasAssignments, int start, int divot, int end);
    ExpressionNode* makePrefixNode(const JSTokenLocation&, ExpressionNode*, Operator, int start, int divot, int end);
    ExpressionNode* makePostfixNode(const JSTokenLocation&, ExpressionNode*, Operator, int start, int divot, int end);
    ExpressionNode* makeTypeOfNode(const JSTokenLocation&, ExpressionNode*);
    ExpressionNode* makeDeleteNode(const JSTokenLocation&, ExpressionNode*, int start, int divot, int end);
    ExpressionNode* makeNegateNode(const JSTokenLocation&, ExpressionNode*);
    ExpressionNode* makeBitwiseNotNode(const JSTokenLocation&, ExpressionNode*);
    ExpressionNode* makeMultNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeDivNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeModNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeAddNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeSubNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeBitXOrNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeBitAndNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeBitOrNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeLeftShiftNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeRightShiftNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeURightShiftNode(const JSTokenLocation&, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);

    ExpressionNode* createLogicalNot(const JSTokenLocation& location, ExpressionNode* expr)
    {
        if (expr->isNumber())
            return createBoolean(location, !static_cast<NumberNode*>(expr)->value());

        return new (m_globalData) LogicalNotNode(location, expr);
    }
    ExpressionNode* createUnaryPlus(const JSTokenLocation& location, ExpressionNode* expr) { return new (m_globalData) UnaryPlusNode(location, expr); }
    ExpressionNode* createVoid(const JSTokenLocation& location, ExpressionNode* expr)
    {
        incConstants();
        return new (m_globalData) VoidNode(location, expr);
    }
    ExpressionNode* thisExpr(const JSTokenLocation& location)
    {
        usesThis();
        return new (m_globalData) ThisNode(location);
    }
    ExpressionNode* createResolve(const JSTokenLocation& location, const Identifier* ident, int start)
    {
        if (m_globalData->propertyNames->arguments == *ident)
            usesArguments();
        return new (m_globalData) ResolveNode(location, *ident, start);
    }
    ExpressionNode* createObjectLiteral(const JSTokenLocation& location) { return new (m_globalData) ObjectLiteralNode(location); }
    ExpressionNode* createObjectLiteral(const JSTokenLocation& location, PropertyListNode* properties) { return new (m_globalData) ObjectLiteralNode(location, properties); }

    ExpressionNode* createArray(const JSTokenLocation& location, int elisions)
    {
        if (elisions)
            incConstants();
        return new (m_globalData) ArrayNode(location, elisions);
    }

    ExpressionNode* createArray(const JSTokenLocation& location, ElementNode* elems) { return new (m_globalData) ArrayNode(location, elems); }
    ExpressionNode* createArray(const JSTokenLocation& location, int elisions, ElementNode* elems)
    {
        if (elisions)
            incConstants();
        return new (m_globalData) ArrayNode(location, elisions, elems);
    }
    ExpressionNode* createNumberExpr(const JSTokenLocation& location, double d)
    {
        incConstants();
        return new (m_globalData) NumberNode(location, d);
    }

    ExpressionNode* createString(const JSTokenLocation& location, const Identifier* string)
    {
        incConstants();
        return new (m_globalData) StringNode(location, *string);
    }

    ExpressionNode* createBoolean(const JSTokenLocation& location, bool b)
    {
        incConstants();
        return new (m_globalData) BooleanNode(location, b);
    }

    ExpressionNode* createNull(const JSTokenLocation& location)
    {
        incConstants();
        return new (m_globalData) NullNode(location);
    }

    ExpressionNode* createBracketAccess(const JSTokenLocation& location, ExpressionNode* base, ExpressionNode* property, bool propertyHasAssignments, int start, int divot, int end)
    {
        BracketAccessorNode* node = new (m_globalData) BracketAccessorNode(location, base, property, propertyHasAssignments);
        setExceptionLocation(node, start, divot, end);
        return node;
    }

    ExpressionNode* createDotAccess(const JSTokenLocation& location, ExpressionNode* base, const Identifier* property, int start, int divot, int end)
    {
        DotAccessorNode* node = new (m_globalData) DotAccessorNode(location, base, *property);
        setExceptionLocation(node, start, divot, end);
        return node;
    }

    ExpressionNode* createRegExp(const JSTokenLocation& location, const Identifier& pattern, const Identifier& flags, int start)
    {
        if (Yarr::checkSyntax(pattern.string()))
            return 0;
        RegExpNode* node = new (m_globalData) RegExpNode(location, pattern, flags);
        int size = pattern.length() + 2; // + 2 for the two /'s
        setExceptionLocation(node, start, start + size, start + size);
        return node;
    }

    ExpressionNode* createNewExpr(const JSTokenLocation& location, ExpressionNode* expr, ArgumentsNode* arguments, int start, int divot, int end)
    {
        NewExprNode* node = new (m_globalData) NewExprNode(location, expr, arguments);
        setExceptionLocation(node, start, divot, end);
        return node;
    }

    ExpressionNode* createNewExpr(const JSTokenLocation& location, ExpressionNode* expr, int start, int end)
    {
        NewExprNode* node = new (m_globalData) NewExprNode(location, expr);
        setExceptionLocation(node, start, end, end);
        return node;
    }

    ExpressionNode* createConditionalExpr(const JSTokenLocation& location, ExpressionNode* condition, ExpressionNode* lhs, ExpressionNode* rhs)
    {
        return new (m_globalData) ConditionalNode(location, condition, lhs, rhs);
    }

    ExpressionNode* createAssignResolve(const JSTokenLocation& location, const Identifier& ident, ExpressionNode* rhs, int start, int divot, int end)
    {
        if (rhs->isFuncExprNode())
            static_cast<FuncExprNode*>(rhs)->body()->setInferredName(ident);
        AssignResolveNode* node = new (m_globalData) AssignResolveNode(location, ident, rhs);
        setExceptionLocation(node, start, divot, end);
        return node;
    }

    ExpressionNode* createFunctionExpr(const JSTokenLocation& location, const Identifier* name, FunctionBodyNode* body, ParameterNode* parameters, int openBracePos, int closeBracePos, int bodyStartLine, int bodyEndLine)
    {
        FuncExprNode* result = new (m_globalData) FuncExprNode(location, *name, body, m_sourceCode->subExpression(openBracePos, closeBracePos, bodyStartLine), parameters);
        body->setLoc(bodyStartLine, bodyEndLine, location.column);
        return result;
    }

    FunctionBodyNode* createFunctionBody(const JSTokenLocation& location, bool inStrictContext)
    {
        return FunctionBodyNode::create(m_globalData, location, inStrictContext);
    }

    void setFunctionStart(FunctionBodyNode* body, int functionStart)
    {
        body->setFunctionStart(functionStart);
    }
    
    template <bool> PropertyNode* createGetterOrSetterProperty(const JSTokenLocation& location, PropertyNode::Type type, const Identifier* name, ParameterNode* params, FunctionBodyNode* body, int openBracePos, int closeBracePos, int bodyStartLine, int bodyEndLine)
    {
        ASSERT(name);
        body->setLoc(bodyStartLine, bodyEndLine, location.column);
        body->setInferredName(*name);
        return new (m_globalData) PropertyNode(m_globalData, *name, new (m_globalData) FuncExprNode(location, m_globalData->propertyNames->nullIdentifier, body, m_sourceCode->subExpression(openBracePos, closeBracePos, bodyStartLine), params), type);
    }
    
    template <bool> PropertyNode* createGetterOrSetterProperty(JSGlobalData*, const JSTokenLocation& location, PropertyNode::Type type, double name, ParameterNode* params, FunctionBodyNode* body, int openBracePos, int closeBracePos, int bodyStartLine, int bodyEndLine)
    {
        body->setLoc(bodyStartLine, bodyEndLine, location.column);
        return new (m_globalData) PropertyNode(m_globalData, name, new (m_globalData) FuncExprNode(location, m_globalData->propertyNames->nullIdentifier, body, m_sourceCode->subExpression(openBracePos, closeBracePos, bodyStartLine), params), type);
    }

    ArgumentsNode* createArguments() { return new (m_globalData) ArgumentsNode(); }
    ArgumentsNode* createArguments(ArgumentListNode* args) { return new (m_globalData) ArgumentsNode(args); }
    ArgumentListNode* createArgumentsList(const JSTokenLocation& location, ExpressionNode* arg) { return new (m_globalData) ArgumentListNode(location, arg); }
    ArgumentListNode* createArgumentsList(const JSTokenLocation& location, ArgumentListNode* args, ExpressionNode* arg) { return new (m_globalData) ArgumentListNode(location, args, arg); }

    template <bool> PropertyNode* createProperty(const Identifier* propertyName, ExpressionNode* node, PropertyNode::Type type)
    {
        if (node->isFuncExprNode())
            static_cast<FuncExprNode*>(node)->body()->setInferredName(*propertyName);
        return new (m_globalData) PropertyNode(m_globalData, *propertyName, node, type);
    }
    template <bool> PropertyNode* createProperty(JSGlobalData*, double propertyName, ExpressionNode* node, PropertyNode::Type type) { return new (m_globalData) PropertyNode(m_globalData, propertyName, node, type); }
    PropertyListNode* createPropertyList(const JSTokenLocation& location, PropertyNode* property) { return new (m_globalData) PropertyListNode(location, property); }
    PropertyListNode* createPropertyList(const JSTokenLocation& location, PropertyNode* property, PropertyListNode* tail) { return new (m_globalData) PropertyListNode(location, property, tail); }

    ElementNode* createElementList(int elisions, ExpressionNode* expr) { return new (m_globalData) ElementNode(elisions, expr); }
    ElementNode* createElementList(ElementNode* elems, int elisions, ExpressionNode* expr) { return new (m_globalData) ElementNode(elems, elisions, expr); }

    ParameterNode* createFormalParameterList(const Identifier& ident) { return new (m_globalData) ParameterNode(ident); }
    ParameterNode* createFormalParameterList(ParameterNode* list, const Identifier& ident) { return new (m_globalData) ParameterNode(list, ident); }

    CaseClauseNode* createClause(ExpressionNode* expr, JSC::SourceElements* statements) { return new (m_globalData) CaseClauseNode(expr, statements); }
    ClauseListNode* createClauseList(CaseClauseNode* clause) { return new (m_globalData) ClauseListNode(clause); }
    ClauseListNode* createClauseList(ClauseListNode* tail, CaseClauseNode* clause) { return new (m_globalData) ClauseListNode(tail, clause); }

    void setUsesArguments(FunctionBodyNode* node) { node->setUsesArguments(); }

    StatementNode* createFuncDeclStatement(const JSTokenLocation& location, const Identifier* name, FunctionBodyNode* body, ParameterNode* parameters, int openBracePos, int closeBracePos, int bodyStartLine, int bodyEndLine)
    {
        FuncDeclNode* decl = new (m_globalData) FuncDeclNode(location, *name, body, m_sourceCode->subExpression(openBracePos, closeBracePos, bodyStartLine), parameters);
        if (*name == m_globalData->propertyNames->arguments)
            usesArguments();
        m_scope.m_funcDeclarations->data.append(decl->body());
        body->setLoc(bodyStartLine, bodyEndLine, location.column);
        return decl;
    }

    StatementNode* createBlockStatement(const JSTokenLocation& location, JSC::SourceElements* elements, int startLine, int endLine)
    {
        BlockNode* block = new (m_globalData) BlockNode(location, elements);
        block->setLoc(startLine, endLine, location.column);
        return block;
    }

    StatementNode* createExprStatement(const JSTokenLocation& location, ExpressionNode* expr, int start, int end)
    {
        ExprStatementNode* result = new (m_globalData) ExprStatementNode(location, expr);
        result->setLoc(start, end, location.column);
        return result;
    }

    StatementNode* createIfStatement(const JSTokenLocation& location, ExpressionNode* condition, StatementNode* trueBlock, int start, int end)
    {
        IfNode* result = new (m_globalData) IfNode(location, condition, trueBlock);
        result->setLoc(start, end, location.column);
        return result;
    }

    StatementNode* createIfStatement(const JSTokenLocation& location, ExpressionNode* condition, StatementNode* trueBlock, StatementNode* falseBlock, int start, int end)
    {
        IfNode* result = new (m_globalData) IfElseNode(location, condition, trueBlock, falseBlock);
        result->setLoc(start, end, location.column);
        return result;
    }

    StatementNode* createForLoop(const JSTokenLocation& location, ExpressionNode* initializer, ExpressionNode* condition, ExpressionNode* iter, StatementNode* statements, int start, int end)
    {
        ForNode* result = new (m_globalData) ForNode(location, initializer, condition, iter, statements);
        result->setLoc(start, end);
        return result;
    }

    StatementNode* createForInLoop(const JSTokenLocation& location, const Identifier* ident, ExpressionNode* initializer, ExpressionNode* iter, StatementNode* statements, int start, int divot, int end, int initStart, int initEnd, int startLine, int endLine)
    {
        ForInNode* result = new (m_globalData) ForInNode(m_globalData, location, *ident, initializer, iter, statements, initStart, initStart - start, initEnd - initStart);
        result->setLoc(startLine, endLine, location.column);
        setExceptionLocation(result, start, divot + 1, end);
        return result;
    }

    StatementNode* createForInLoop(const JSTokenLocation& location, ExpressionNode* lhs, ExpressionNode* iter, StatementNode* statements, int eStart, int eDivot, int eEnd, int start, int end)
    {
        ForInNode* result = new (m_globalData) ForInNode(location, lhs, iter, statements);
        result->setLoc(start, end, location.column);
        setExceptionLocation(result, eStart, eDivot, eEnd);
        return result;
    }

    StatementNode* createEmptyStatement(const JSTokenLocation& location) { return new (m_globalData) EmptyStatementNode(location); }

    StatementNode* createVarStatement(const JSTokenLocation& location, ExpressionNode* expr, int start, int end)
    {
        StatementNode* result;
        if (!expr)
            result = new (m_globalData) EmptyStatementNode(location);
        else
            result = new (m_globalData) VarStatementNode(location, expr);
        result->setLoc(start, end, location.column);
        return result;
    }

    StatementNode* createReturnStatement(const JSTokenLocation& location, ExpressionNode* expression, int eStart, int eEnd, int startLine, int endLine)
    {
        ReturnNode* result = new (m_globalData) ReturnNode(location, expression);
        setExceptionLocation(result, eStart, eEnd, eEnd);
        result->setLoc(startLine, endLine, location.column);
        return result;
    }

    StatementNode* createBreakStatement(const JSTokenLocation& location, int eStart, int eEnd, int startLine, int endLine)
    {
        BreakNode* result = new (m_globalData) BreakNode(m_globalData, location);
        setExceptionLocation(result, eStart, eEnd, eEnd);
        result->setLoc(startLine, endLine, location.column);
        return result;
    }

    StatementNode* createBreakStatement(const JSTokenLocation& location, const Identifier* ident, int eStart, int eEnd, int startLine, int endLine)
    {
        BreakNode* result = new (m_globalData) BreakNode(location, *ident);
        setExceptionLocation(result, eStart, eEnd, eEnd);
        result->setLoc(startLine, endLine, location.column);
        return result;
    }

    StatementNode* createContinueStatement(const JSTokenLocation& location, int eStart, int eEnd, int startLine, int endLine)
    {
        ContinueNode* result = new (m_globalData) ContinueNode(m_globalData, location);
        setExceptionLocation(result, eStart, eEnd, eEnd);
        result->setLoc(startLine, endLine, location.column);
        return result;
    }

    StatementNode* createContinueStatement(const JSTokenLocation& location, const Identifier* ident, int eStart, int eEnd, int startLine, int endLine)
    {
        ContinueNode* result = new (m_globalData) ContinueNode(location, *ident);
        setExceptionLocation(result, eStart, eEnd, eEnd);
        result->setLoc(startLine, endLine, location.column);
        return result;
    }

    StatementNode* createTryStatement(const JSTokenLocation& location, StatementNode* tryBlock, const Identifier* ident, StatementNode* catchBlock, StatementNode* finallyBlock, int startLine, int endLine)
    {
        TryNode* result = new (m_globalData) TryNode(location, tryBlock, *ident, catchBlock, finallyBlock);
        if (catchBlock)
            usesCatch();
        result->setLoc(startLine, endLine, location.column);
        return result;
    }

    StatementNode* createSwitchStatement(const JSTokenLocation& location, ExpressionNode* expr, ClauseListNode* firstClauses, CaseClauseNode* defaultClause, ClauseListNode* secondClauses, int startLine, int endLine)
    {
        CaseBlockNode* cases = new (m_globalData) CaseBlockNode(firstClauses, defaultClause, secondClauses);
        SwitchNode* result = new (m_globalData) SwitchNode(location, expr, cases);
        result->setLoc(startLine, endLine, location.column);
        return result;
    }

    StatementNode* createWhileStatement(const JSTokenLocation& location, ExpressionNode* expr, StatementNode* statement, int startLine, int endLine)
    {
        WhileNode* result = new (m_globalData) WhileNode(location, expr, statement);
        result->setLoc(startLine, endLine, location.column);
        return result;
    }

    StatementNode* createDoWhileStatement(const JSTokenLocation& location, StatementNode* statement, ExpressionNode* expr, int startLine, int endLine)
    {
        DoWhileNode* result = new (m_globalData) DoWhileNode(location, statement, expr);
        result->setLoc(startLine, endLine, location.column);
        return result;
    }

    StatementNode* createLabelStatement(const JSTokenLocation& location, const Identifier* ident, StatementNode* statement, int start, int end)
    {
        LabelNode* result = new (m_globalData) LabelNode(location, *ident, statement);
        setExceptionLocation(result, start, end, end);
        return result;
    }

    StatementNode* createWithStatement(const JSTokenLocation& location, ExpressionNode* expr, StatementNode* statement, int start, int end, int startLine, int endLine)
    {
        usesWith();
        WithNode* result = new (m_globalData) WithNode(location, expr, statement, end, end - start);
        result->setLoc(startLine, endLine, location.column);
        return result;
    }    
    
    StatementNode* createThrowStatement(const JSTokenLocation& location, ExpressionNode* expr, int start, int end, int startLine, int endLine)
    {
        ThrowNode* result = new (m_globalData) ThrowNode(location, expr);
        result->setLoc(startLine, endLine, location.column);
        setExceptionLocation(result, start, end, end);
        return result;
    }
    
    StatementNode* createDebugger(const JSTokenLocation& location, int startLine, int endLine)
    {
        DebuggerStatementNode* result = new (m_globalData) DebuggerStatementNode(location);
        result->setLoc(startLine, endLine, location.column);
        return result;
    }
    
    StatementNode* createConstStatement(const JSTokenLocation& location, ConstDeclNode* decls, int startLine, int endLine)
    {
        ConstStatementNode* result = new (m_globalData) ConstStatementNode(location, decls);
        result->setLoc(startLine, endLine, location.column);
        return result;
    }

    ConstDeclNode* appendConstDecl(const JSTokenLocation& location, ConstDeclNode* tail, const Identifier* name, ExpressionNode* initializer)
    {
        ConstDeclNode* result = new (m_globalData) ConstDeclNode(location, *name, initializer);
        if (tail)
            tail->m_next = result;
        return result;
    }

    void appendStatement(JSC::SourceElements* elements, JSC::StatementNode* statement)
    {
        elements->append(statement);
    }

    void addVar(const Identifier* ident, int attrs)
    {
        if (m_globalData->propertyNames->arguments == *ident)
            usesArguments();
        m_scope.m_varDeclarations->data.append(std::make_pair(ident, attrs));
    }

    ExpressionNode* combineCommaNodes(const JSTokenLocation& location, ExpressionNode* list, ExpressionNode* init)
    {
        if (!list)
            return init;
        if (list->isCommaNode()) {
            static_cast<CommaNode*>(list)->append(init);
            return list;
        }
        return new (m_globalData) CommaNode(location, list, init);
    }

    int evalCount() const { return m_evalCount; }

    void appendBinaryExpressionInfo(int& operandStackDepth, ExpressionNode* current, int exprStart, int lhs, int rhs, bool hasAssignments)
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
    bool operatorStackHasHigherPrecedence(int&, int precedence)
    {
        return precedence <= m_binaryOperatorStack.last().second;
    }
    const BinaryOperand& getFromOperandStack(int i) { return m_binaryOperandStack[m_binaryOperandStack.size() + i]; }
    void shrinkOperandStackBy(int& operandStackDepth, int amount)
    {
        operandStackDepth -= amount;
        ASSERT(operandStackDepth >= 0);
        m_binaryOperandStack.resize(m_binaryOperandStack.size() - amount);
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
    
    void appendUnaryToken(int& tokenStackDepth, int type, int start)
    {
        tokenStackDepth++;
        m_unaryTokenStack.append(std::make_pair(type, start));
    }

    int unaryTokenStackLastType(int&)
    {
        return m_unaryTokenStack.last().first;
    }
    
    int unaryTokenStackLastStart(int&)
    {
        return m_unaryTokenStack.last().second;
    }
    
    void unaryTokenStackRemoveLast(int& tokenStackDepth)
    {
        tokenStackDepth--;
        m_unaryTokenStack.removeLast();
    }
    
    void assignmentStackAppend(int& assignmentStackDepth, ExpressionNode* node, int start, int divot, int assignmentCount, Operator op)
    {
        assignmentStackDepth++;
        m_assignmentInfoStack.append(AssignmentInfo(node, start, divot, assignmentCount, op));
    }

    ExpressionNode* createAssignment(const JSTokenLocation& location, int& assignmentStackDepth, ExpressionNode* rhs, int initialAssignmentCount, int currentAssignmentCount, int lastTokenEnd)
    {
        ExpressionNode* result = makeAssignNode(location, m_assignmentInfoStack.last().m_node, m_assignmentInfoStack.last().m_op, rhs, m_assignmentInfoStack.last().m_initAssignments != initialAssignmentCount, m_assignmentInfoStack.last().m_initAssignments != currentAssignmentCount, m_assignmentInfoStack.last().m_start, m_assignmentInfoStack.last().m_divot + 1, lastTokenEnd);
        m_assignmentInfoStack.removeLast();
        assignmentStackDepth--;
        return result;
    }
    
    const Identifier& getName(Property property) const { return property->name(); }
    PropertyNode::Type getType(Property property) const { return property->type(); }

    bool isResolve(ExpressionNode* expr) const { return expr->isResolveNode(); }

private:
    struct Scope {
        Scope(JSGlobalData* globalData)
            : m_varDeclarations(new (globalData) ParserArenaData<DeclarationStacks::VarStack>)
            , m_funcDeclarations(new (globalData) ParserArenaData<DeclarationStacks::FunctionStack>)
            , m_features(0)
            , m_numConstants(0)
        {
        }
        ParserArenaData<DeclarationStacks::VarStack>* m_varDeclarations;
        ParserArenaData<DeclarationStacks::FunctionStack>* m_funcDeclarations;
        int m_features;
        int m_numConstants;
    };

    static void setExceptionLocation(ThrowableExpressionData* node, unsigned start, unsigned divot, unsigned end)
    {
        node->setExceptionSourceCode(divot, divot - start, end - divot);
    }

    void incConstants() { m_scope.m_numConstants++; }
    void usesThis() { m_scope.m_features |= ThisFeature; }
    void usesCatch() { m_scope.m_features |= CatchFeature; }
    void usesArguments() { m_scope.m_features |= ArgumentsFeature; }
    void usesWith() { m_scope.m_features |= WithFeature; }
    void usesEval() 
    {
        m_evalCount++;
        m_scope.m_features |= EvalFeature;
    }
    ExpressionNode* createNumber(const JSTokenLocation& location, double d)
    {
        return new (m_globalData) NumberNode(location, d);
    }
    
    JSGlobalData* m_globalData;
    SourceCode* m_sourceCode;
    Scope m_scope;
    Vector<BinaryOperand, 10> m_binaryOperandStack;
    Vector<AssignmentInfo, 10> m_assignmentInfoStack;
    Vector<pair<int, int>, 10> m_binaryOperatorStack;
    Vector<pair<int, int>, 10> m_unaryTokenStack;
    int m_evalCount;
};

ExpressionNode* ASTBuilder::makeTypeOfNode(const JSTokenLocation& location, ExpressionNode* expr)
{
    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        return new (m_globalData) TypeOfResolveNode(location, resolve->identifier());
    }
    return new (m_globalData) TypeOfValueNode(location, expr);
}

ExpressionNode* ASTBuilder::makeDeleteNode(const JSTokenLocation& location, ExpressionNode* expr, int start, int divot, int end)
{
    if (!expr->isLocation())
        return new (m_globalData) DeleteValueNode(location, expr);
    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        return new (m_globalData) DeleteResolveNode(location, resolve->identifier(), divot, divot - start, end - divot);
    }
    if (expr->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(expr);
        return new (m_globalData) DeleteBracketNode(location, bracket->base(), bracket->subscript(), divot, divot - start, end - divot);
    }
    ASSERT(expr->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(expr);
    return new (m_globalData) DeleteDotNode(location, dot->base(), dot->identifier(), divot, divot - start, end - divot);
}

ExpressionNode* ASTBuilder::makeNegateNode(const JSTokenLocation& location, ExpressionNode* n)
{
    if (n->isNumber()) {
        NumberNode* numberNode = static_cast<NumberNode*>(n);
        numberNode->setValue(-numberNode->value());
        return numberNode;
    }

    return new (m_globalData) NegateNode(location, n);
}

ExpressionNode* ASTBuilder::makeBitwiseNotNode(const JSTokenLocation& location, ExpressionNode* expr)
{
    if (expr->isNumber())
        return createNumber(location, ~toInt32(static_cast<NumberNode*>(expr)->value()));
    return new (m_globalData) BitwiseNotNode(location, expr);
}

ExpressionNode* ASTBuilder::makeMultNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    expr1 = expr1->stripUnaryPlus();
    expr2 = expr2->stripUnaryPlus();

    if (expr1->isNumber() && expr2->isNumber())
        return createNumber(location, static_cast<NumberNode*>(expr1)->value() * static_cast<NumberNode*>(expr2)->value());

    if (expr1->isNumber() && static_cast<NumberNode*>(expr1)->value() == 1)
        return new (m_globalData) UnaryPlusNode(location, expr2);

    if (expr2->isNumber() && static_cast<NumberNode*>(expr2)->value() == 1)
        return new (m_globalData) UnaryPlusNode(location, expr1);

    return new (m_globalData) MultNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeDivNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    expr1 = expr1->stripUnaryPlus();
    expr2 = expr2->stripUnaryPlus();

    if (expr1->isNumber() && expr2->isNumber())
        return createNumber(location, static_cast<NumberNode*>(expr1)->value() / static_cast<NumberNode*>(expr2)->value());
    return new (m_globalData) DivNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeModNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    expr1 = expr1->stripUnaryPlus();
    expr2 = expr2->stripUnaryPlus();
    
    if (expr1->isNumber() && expr2->isNumber())
        return createNumber(location, fmod(static_cast<NumberNode*>(expr1)->value(), static_cast<NumberNode*>(expr2)->value()));
    return new (m_globalData) ModNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeAddNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber())
        return createNumber(location, static_cast<NumberNode*>(expr1)->value() + static_cast<NumberNode*>(expr2)->value());
    return new (m_globalData) AddNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeSubNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    expr1 = expr1->stripUnaryPlus();
    expr2 = expr2->stripUnaryPlus();

    if (expr1->isNumber() && expr2->isNumber())
        return createNumber(location, static_cast<NumberNode*>(expr1)->value() - static_cast<NumberNode*>(expr2)->value());
    return new (m_globalData) SubNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeLeftShiftNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber())
        return createNumber(location, toInt32(static_cast<NumberNode*>(expr1)->value()) << (toUInt32(static_cast<NumberNode*>(expr2)->value()) & 0x1f));
    return new (m_globalData) LeftShiftNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeRightShiftNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber())
        return createNumber(location, toInt32(static_cast<NumberNode*>(expr1)->value()) >> (toUInt32(static_cast<NumberNode*>(expr2)->value()) & 0x1f));
    return new (m_globalData) RightShiftNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeURightShiftNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber())
        return createNumber(location, toUInt32(static_cast<NumberNode*>(expr1)->value()) >> (toUInt32(static_cast<NumberNode*>(expr2)->value()) & 0x1f));
    return new (m_globalData) UnsignedRightShiftNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeBitOrNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber())
        return createNumber(location, toInt32(static_cast<NumberNode*>(expr1)->value()) | toInt32(static_cast<NumberNode*>(expr2)->value()));
    return new (m_globalData) BitOrNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeBitAndNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber())
        return createNumber(location, toInt32(static_cast<NumberNode*>(expr1)->value()) & toInt32(static_cast<NumberNode*>(expr2)->value()));
    return new (m_globalData) BitAndNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeBitXOrNode(const JSTokenLocation& location, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber())
        return createNumber(location, toInt32(static_cast<NumberNode*>(expr1)->value()) ^ toInt32(static_cast<NumberNode*>(expr2)->value()));
    return new (m_globalData) BitXOrNode(location, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeFunctionCallNode(const JSTokenLocation& location, ExpressionNode* func, ArgumentsNode* args, int start, int divot, int end)
{
    if (!func->isLocation())
        return new (m_globalData) FunctionCallValueNode(location, func, args, divot, divot - start, end - divot);
    if (func->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(func);
        const Identifier& identifier = resolve->identifier();
        if (identifier == m_globalData->propertyNames->eval) {
            usesEval();
            return new (m_globalData) EvalFunctionCallNode(location, args, divot, divot - start, end - divot);
        }
        return new (m_globalData) FunctionCallResolveNode(location, identifier, args, divot, divot - start, end - divot);
    }
    if (func->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(func);
        FunctionCallBracketNode* node = new (m_globalData) FunctionCallBracketNode(location, bracket->base(), bracket->subscript(), args, divot, divot - start, end - divot);
        node->setSubexpressionInfo(bracket->divot(), bracket->endOffset());
        return node;
    }
    ASSERT(func->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(func);
    FunctionCallDotNode* node;
    if (dot->identifier() == m_globalData->propertyNames->call)
        node = new (m_globalData) CallFunctionCallDotNode(location, dot->base(), dot->identifier(), args, divot, divot - start, end - divot);
    else if (dot->identifier() == m_globalData->propertyNames->apply)
        node = new (m_globalData) ApplyFunctionCallDotNode(location, dot->base(), dot->identifier(), args, divot, divot - start, end - divot);
    else
        node = new (m_globalData) FunctionCallDotNode(location, dot->base(), dot->identifier(), args, divot, divot - start, end - divot);
    node->setSubexpressionInfo(dot->divot(), dot->endOffset());
    return node;
}

ExpressionNode* ASTBuilder::makeBinaryNode(const JSTokenLocation& location, int token, pair<ExpressionNode*, BinaryOpInfo> lhs, pair<ExpressionNode*, BinaryOpInfo> rhs)
{
    switch (token) {
    case OR:
        return new (m_globalData) LogicalOpNode(location, lhs.first, rhs.first, OpLogicalOr);

    case AND:
        return new (m_globalData) LogicalOpNode(location, lhs.first, rhs.first, OpLogicalAnd);

    case BITOR:
        return makeBitOrNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case BITXOR:
        return makeBitXOrNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case BITAND:
        return makeBitAndNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case EQEQ:
        return new (m_globalData) EqualNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case NE:
        return new (m_globalData) NotEqualNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case STREQ:
        return new (m_globalData) StrictEqualNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case STRNEQ:
        return new (m_globalData) NotStrictEqualNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case LT:
        return new (m_globalData) LessNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case GT:
        return new (m_globalData) GreaterNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case LE:
        return new (m_globalData) LessEqNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case GE:
        return new (m_globalData) GreaterEqNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);

    case INSTANCEOF: {
        InstanceOfNode* node = new (m_globalData) InstanceOfNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);
        setExceptionLocation(node, lhs.second.start, rhs.second.start, rhs.second.end);
        return node;
    }

    case INTOKEN: {
        InNode* node = new (m_globalData) InNode(location, lhs.first, rhs.first, rhs.second.hasAssignment);
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
    }
    CRASH();
    return 0;
}

ExpressionNode* ASTBuilder::makeAssignNode(const JSTokenLocation& location, ExpressionNode* loc, Operator op, ExpressionNode* expr, bool locHasAssignments, bool exprHasAssignments, int start, int divot, int end)
{
    if (!loc->isLocation())
        return new (m_globalData) AssignErrorNode(location, divot, divot - start, end - divot);

    if (loc->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(loc);
        if (op == OpEqual) {
            if (expr->isFuncExprNode())
                static_cast<FuncExprNode*>(expr)->body()->setInferredName(resolve->identifier());
            AssignResolveNode* node = new (m_globalData) AssignResolveNode(location, resolve->identifier(), expr);
            setExceptionLocation(node, start, divot, end);
            return node;
        }
        return new (m_globalData) ReadModifyResolveNode(location, resolve->identifier(), op, expr, exprHasAssignments, divot, divot - start, end - divot);
    }
    if (loc->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(loc);
        if (op == OpEqual)
            return new (m_globalData) AssignBracketNode(location, bracket->base(), bracket->subscript(), expr, locHasAssignments, exprHasAssignments, bracket->divot(), bracket->divot() - start, end - bracket->divot());
        ReadModifyBracketNode* node = new (m_globalData) ReadModifyBracketNode(location, bracket->base(), bracket->subscript(), op, expr, locHasAssignments, exprHasAssignments, divot, divot - start, end - divot);
        node->setSubexpressionInfo(bracket->divot(), bracket->endOffset());
        return node;
    }
    ASSERT(loc->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(loc);
    if (op == OpEqual) {
        if (expr->isFuncExprNode())
            static_cast<FuncExprNode*>(expr)->body()->setInferredName(dot->identifier());
        return new (m_globalData) AssignDotNode(location, dot->base(), dot->identifier(), expr, exprHasAssignments, dot->divot(), dot->divot() - start, end - dot->divot());
    }

    ReadModifyDotNode* node = new (m_globalData) ReadModifyDotNode(location, dot->base(), dot->identifier(), op, expr, exprHasAssignments, divot, divot - start, end - divot);
    node->setSubexpressionInfo(dot->divot(), dot->endOffset());
    return node;
}

ExpressionNode* ASTBuilder::makePrefixNode(const JSTokenLocation& location, ExpressionNode* expr, Operator op, int start, int divot, int end)
{
    return new (m_globalData) PrefixNode(location, expr, op, divot, divot - start, end - divot);
}

ExpressionNode* ASTBuilder::makePostfixNode(const JSTokenLocation& location, ExpressionNode* expr, Operator op, int start, int divot, int end)
{
    return new (m_globalData) PostfixNode(location, expr, op, divot, divot - start, end - divot);
}

}

#endif
