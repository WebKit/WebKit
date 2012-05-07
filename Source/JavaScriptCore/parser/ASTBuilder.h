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

    ExpressionNode* makeBinaryNode(int lineNumber, int token, std::pair<ExpressionNode*, BinaryOpInfo>, std::pair<ExpressionNode*, BinaryOpInfo>);
    ExpressionNode* makeFunctionCallNode(int lineNumber, ExpressionNode* func, ArgumentsNode* args, int start, int divot, int end);

    JSC::SourceElements* createSourceElements() { return new (m_globalData) JSC::SourceElements(); }

    ParserArenaData<DeclarationStacks::VarStack>* varDeclarations() { return m_scope.m_varDeclarations; }
    ParserArenaData<DeclarationStacks::FunctionStack>* funcDeclarations() { return m_scope.m_funcDeclarations; }
    int features() const { return m_scope.m_features; }
    int numConstants() const { return m_scope.m_numConstants; }

    void appendToComma(CommaNode* commaNode, ExpressionNode* expr) { commaNode->append(expr); }

    CommaNode* createCommaExpr(int lineNumber, ExpressionNode* lhs, ExpressionNode* rhs) { return new (m_globalData) CommaNode(lineNumber, lhs, rhs); }

    ExpressionNode* makeAssignNode(int lineNumber, ExpressionNode* left, Operator, ExpressionNode* right, bool leftHasAssignments, bool rightHasAssignments, int start, int divot, int end);
    ExpressionNode* makePrefixNode(int lineNumber, ExpressionNode*, Operator, int start, int divot, int end);
    ExpressionNode* makePostfixNode(int lineNumber, ExpressionNode*, Operator, int start, int divot, int end);
    ExpressionNode* makeTypeOfNode(int lineNumber, ExpressionNode*);
    ExpressionNode* makeDeleteNode(int lineNumber, ExpressionNode*, int start, int divot, int end);
    ExpressionNode* makeNegateNode(int lineNumber, ExpressionNode*);
    ExpressionNode* makeBitwiseNotNode(int lineNumber, ExpressionNode*);
    ExpressionNode* makeMultNode(int lineNumber, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeDivNode(int lineNumber, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeModNode(int lineNumber, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeAddNode(int lineNumber, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeSubNode(int lineNumber, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeBitXOrNode(int lineNumber, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeBitAndNode(int lineNumber, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeBitOrNode(int lineNumber, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeLeftShiftNode(int lineNumber, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeRightShiftNode(int lineNumber, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
    ExpressionNode* makeURightShiftNode(int lineNumber, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);

    ExpressionNode* createLogicalNot(int lineNumber, ExpressionNode* expr) { return new (m_globalData) LogicalNotNode(lineNumber, expr); }
    ExpressionNode* createUnaryPlus(int lineNumber, ExpressionNode* expr) { return new (m_globalData) UnaryPlusNode(lineNumber, expr); }
    ExpressionNode* createVoid(int lineNumber, ExpressionNode* expr)
    {
        incConstants();
        return new (m_globalData) VoidNode(lineNumber, expr);
    }
    ExpressionNode* thisExpr(int lineNumber)
    {
        usesThis();
        return new (m_globalData) ThisNode(lineNumber);
    }
    ExpressionNode* createResolve(int lineNumber, const Identifier* ident, int start)
    {
        if (m_globalData->propertyNames->arguments == *ident)
            usesArguments();
        return new (m_globalData) ResolveNode(lineNumber, *ident, start);
    }
    ExpressionNode* createObjectLiteral(int lineNumber) { return new (m_globalData) ObjectLiteralNode(lineNumber); }
    ExpressionNode* createObjectLiteral(int lineNumber, PropertyListNode* properties) { return new (m_globalData) ObjectLiteralNode(lineNumber, properties); }

    ExpressionNode* createArray(int lineNumber, int elisions)
    {
        if (elisions)
            incConstants();
        return new (m_globalData) ArrayNode(lineNumber, elisions);
    }

    ExpressionNode* createArray(int lineNumber, ElementNode* elems) { return new (m_globalData) ArrayNode(lineNumber, elems); }
    ExpressionNode* createArray(int lineNumber, int elisions, ElementNode* elems)
    {
        if (elisions)
            incConstants();
        return new (m_globalData) ArrayNode(lineNumber, elisions, elems);
    }
    ExpressionNode* createNumberExpr(int lineNumber, double d)
    {
        incConstants();
        return new (m_globalData) NumberNode(lineNumber, d);
    }

    ExpressionNode* createString(int lineNumber, const Identifier* string)
    {
        incConstants();
        return new (m_globalData) StringNode(lineNumber, *string);
    }

    ExpressionNode* createBoolean(int lineNumber, bool b)
    {
        incConstants();
        return new (m_globalData) BooleanNode(lineNumber, b);
    }

    ExpressionNode* createNull(int lineNumber)
    {
        incConstants();
        return new (m_globalData) NullNode(lineNumber);
    }

    ExpressionNode* createBracketAccess(int lineNumber, ExpressionNode* base, ExpressionNode* property, bool propertyHasAssignments, int start, int divot, int end)
    {
        BracketAccessorNode* node = new (m_globalData) BracketAccessorNode(lineNumber, base, property, propertyHasAssignments);
        setExceptionLocation(node, start, divot, end);
        return node;
    }

    ExpressionNode* createDotAccess(int lineNumber, ExpressionNode* base, const Identifier* property, int start, int divot, int end)
    {
        DotAccessorNode* node = new (m_globalData) DotAccessorNode(lineNumber, base, *property);
        setExceptionLocation(node, start, divot, end);
        return node;
    }

    ExpressionNode* createRegExp(int lineNumber, const Identifier& pattern, const Identifier& flags, int start)
    {
        if (Yarr::checkSyntax(pattern.ustring()))
            return 0;
        RegExpNode* node = new (m_globalData) RegExpNode(lineNumber, pattern, flags);
        int size = pattern.length() + 2; // + 2 for the two /'s
        setExceptionLocation(node, start, start + size, start + size);
        return node;
    }

    ExpressionNode* createNewExpr(int lineNumber, ExpressionNode* expr, ArgumentsNode* arguments, int start, int divot, int end)
    {
        NewExprNode* node = new (m_globalData) NewExprNode(lineNumber, expr, arguments);
        setExceptionLocation(node, start, divot, end);
        return node;
    }

    ExpressionNode* createNewExpr(int lineNumber, ExpressionNode* expr, int start, int end)
    {
        NewExprNode* node = new (m_globalData) NewExprNode(lineNumber, expr);
        setExceptionLocation(node, start, end, end);
        return node;
    }

    ExpressionNode* createConditionalExpr(int lineNumber, ExpressionNode* condition, ExpressionNode* lhs, ExpressionNode* rhs)
    {
        return new (m_globalData) ConditionalNode(lineNumber, condition, lhs, rhs);
    }

    ExpressionNode* createAssignResolve(int lineNumber, const Identifier& ident, ExpressionNode* rhs, bool rhsHasAssignment, int start, int divot, int end)
    {
        if (rhs->isFuncExprNode())
            static_cast<FuncExprNode*>(rhs)->body()->setInferredName(ident);
        AssignResolveNode* node = new (m_globalData) AssignResolveNode(lineNumber, ident, rhs, rhsHasAssignment);
        setExceptionLocation(node, start, divot, end);
        return node;
    }

    ExpressionNode* createFunctionExpr(int lineNumber, const Identifier* name, FunctionBodyNode* body, ParameterNode* parameters, int openBracePos, int closeBracePos, int bodyStartLine, int bodyEndLine)
    {
        FuncExprNode* result = new (m_globalData) FuncExprNode(lineNumber, *name, body, m_sourceCode->subExpression(openBracePos, closeBracePos, bodyStartLine), parameters);
        body->setLoc(bodyStartLine, bodyEndLine);
        return result;
    }

    FunctionBodyNode* createFunctionBody(int lineNumber, bool inStrictContext)
    {
        return FunctionBodyNode::create(m_globalData, lineNumber, inStrictContext);
    }
    
    template <bool> PropertyNode* createGetterOrSetterProperty(int lineNumber, PropertyNode::Type type, const Identifier* name, ParameterNode* params, FunctionBodyNode* body, int openBracePos, int closeBracePos, int bodyStartLine, int bodyEndLine)
    {
        ASSERT(name);
        body->setLoc(bodyStartLine, bodyEndLine);
        body->setInferredName(*name);
        return new (m_globalData) PropertyNode(m_globalData, *name, new (m_globalData) FuncExprNode(lineNumber, m_globalData->propertyNames->nullIdentifier, body, m_sourceCode->subExpression(openBracePos, closeBracePos, bodyStartLine), params), type);
    }
    
    template <bool> PropertyNode* createGetterOrSetterProperty(JSGlobalData*, int lineNumber, PropertyNode::Type type, double name, ParameterNode* params, FunctionBodyNode* body, int openBracePos, int closeBracePos, int bodyStartLine, int bodyEndLine)
    {
        body->setLoc(bodyStartLine, bodyEndLine);
        return new (m_globalData) PropertyNode(m_globalData, name, new (m_globalData) FuncExprNode(lineNumber, m_globalData->propertyNames->nullIdentifier, body, m_sourceCode->subExpression(openBracePos, closeBracePos, bodyStartLine), params), type);
    }

    ArgumentsNode* createArguments() { return new (m_globalData) ArgumentsNode(); }
    ArgumentsNode* createArguments(ArgumentListNode* args) { return new (m_globalData) ArgumentsNode(args); }
    ArgumentListNode* createArgumentsList(int lineNumber, ExpressionNode* arg) { return new (m_globalData) ArgumentListNode(lineNumber, arg); }
    ArgumentListNode* createArgumentsList(int lineNumber, ArgumentListNode* args, ExpressionNode* arg) { return new (m_globalData) ArgumentListNode(lineNumber, args, arg); }

    template <bool> PropertyNode* createProperty(const Identifier* propertyName, ExpressionNode* node, PropertyNode::Type type)
    {
        if (node->isFuncExprNode())
            static_cast<FuncExprNode*>(node)->body()->setInferredName(*propertyName);
        return new (m_globalData) PropertyNode(m_globalData, *propertyName, node, type);
    }
    template <bool> PropertyNode* createProperty(JSGlobalData*, double propertyName, ExpressionNode* node, PropertyNode::Type type) { return new (m_globalData) PropertyNode(m_globalData, propertyName, node, type); }
    PropertyListNode* createPropertyList(int lineNumber, PropertyNode* property) { return new (m_globalData) PropertyListNode(lineNumber, property); }
    PropertyListNode* createPropertyList(int lineNumber, PropertyNode* property, PropertyListNode* tail) { return new (m_globalData) PropertyListNode(lineNumber, property, tail); }

    ElementNode* createElementList(int elisions, ExpressionNode* expr) { return new (m_globalData) ElementNode(elisions, expr); }
    ElementNode* createElementList(ElementNode* elems, int elisions, ExpressionNode* expr) { return new (m_globalData) ElementNode(elems, elisions, expr); }

    ParameterNode* createFormalParameterList(const Identifier& ident) { return new (m_globalData) ParameterNode(ident); }
    ParameterNode* createFormalParameterList(ParameterNode* list, const Identifier& ident) { return new (m_globalData) ParameterNode(list, ident); }

    CaseClauseNode* createClause(ExpressionNode* expr, JSC::SourceElements* statements) { return new (m_globalData) CaseClauseNode(expr, statements); }
    ClauseListNode* createClauseList(CaseClauseNode* clause) { return new (m_globalData) ClauseListNode(clause); }
    ClauseListNode* createClauseList(ClauseListNode* tail, CaseClauseNode* clause) { return new (m_globalData) ClauseListNode(tail, clause); }

    void setUsesArguments(FunctionBodyNode* node) { node->setUsesArguments(); }

    StatementNode* createFuncDeclStatement(int lineNumber, const Identifier* name, FunctionBodyNode* body, ParameterNode* parameters, int openBracePos, int closeBracePos, int bodyStartLine, int bodyEndLine)
    {
        FuncDeclNode* decl = new (m_globalData) FuncDeclNode(lineNumber, *name, body, m_sourceCode->subExpression(openBracePos, closeBracePos, bodyStartLine), parameters);
        if (*name == m_globalData->propertyNames->arguments)
            usesArguments();
        m_scope.m_funcDeclarations->data.append(decl->body());
        body->setLoc(bodyStartLine, bodyEndLine);
        return decl;
    }

    StatementNode* createBlockStatement(int lineNumber, JSC::SourceElements* elements, int startLine, int endLine)
    {
        BlockNode* block = new (m_globalData) BlockNode(lineNumber, elements);
        block->setLoc(startLine, endLine);
        return block;
    }

    StatementNode* createExprStatement(int lineNumber, ExpressionNode* expr, int start, int end)
    {
        ExprStatementNode* result = new (m_globalData) ExprStatementNode(lineNumber, expr);
        result->setLoc(start, end);
        return result;
    }

    StatementNode* createIfStatement(int lineNumber, ExpressionNode* condition, StatementNode* trueBlock, int start, int end)
    {
        IfNode* result = new (m_globalData) IfNode(lineNumber, condition, trueBlock);
        result->setLoc(start, end);
        return result;
    }

    StatementNode* createIfStatement(int lineNumber, ExpressionNode* condition, StatementNode* trueBlock, StatementNode* falseBlock, int start, int end)
    {
        IfNode* result = new (m_globalData) IfElseNode(lineNumber, condition, trueBlock, falseBlock);
        result->setLoc(start, end);
        return result;
    }

    StatementNode* createForLoop(int lineNumber, ExpressionNode* initializer, ExpressionNode* condition, ExpressionNode* iter, StatementNode* statements, bool b, int start, int end)
    {
        ForNode* result = new (m_globalData) ForNode(lineNumber, initializer, condition, iter, statements, b);
        result->setLoc(start, end);
        return result;
    }

    StatementNode* createForInLoop(int lineNumber, const Identifier* ident, ExpressionNode* initializer, ExpressionNode* iter, StatementNode* statements, int start, int divot, int end, int initStart, int initEnd, int startLine, int endLine)
    {
        ForInNode* result = new (m_globalData) ForInNode(m_globalData, lineNumber, *ident, initializer, iter, statements, initStart, initStart - start, initEnd - initStart);
        result->setLoc(startLine, endLine);
        setExceptionLocation(result, start, divot + 1, end);
        return result;
    }

    StatementNode* createForInLoop(int lineNumber, ExpressionNode* lhs, ExpressionNode* iter, StatementNode* statements, int eStart, int eDivot, int eEnd, int start, int end)
    {
        ForInNode* result = new (m_globalData) ForInNode(m_globalData, lineNumber, lhs, iter, statements);
        result->setLoc(start, end);
        setExceptionLocation(result, eStart, eDivot, eEnd);
        return result;
    }

    StatementNode* createEmptyStatement(int lineNumber) { return new (m_globalData) EmptyStatementNode(lineNumber); }

    StatementNode* createVarStatement(int lineNumber, ExpressionNode* expr, int start, int end)
    {
        StatementNode* result;
        if (!expr)
            result = new (m_globalData) EmptyStatementNode(lineNumber);
        else
            result = new (m_globalData) VarStatementNode(lineNumber, expr);
        result->setLoc(start, end);
        return result;
    }

    StatementNode* createReturnStatement(int lineNumber, ExpressionNode* expression, int eStart, int eEnd, int startLine, int endLine)
    {
        ReturnNode* result = new (m_globalData) ReturnNode(lineNumber, expression);
        setExceptionLocation(result, eStart, eEnd, eEnd);
        result->setLoc(startLine, endLine);
        return result;
    }

    StatementNode* createBreakStatement(int lineNumber, int eStart, int eEnd, int startLine, int endLine)
    {
        BreakNode* result = new (m_globalData) BreakNode(m_globalData, lineNumber);
        setExceptionLocation(result, eStart, eEnd, eEnd);
        result->setLoc(startLine, endLine);
        return result;
    }

    StatementNode* createBreakStatement(int lineNumber, const Identifier* ident, int eStart, int eEnd, int startLine, int endLine)
    {
        BreakNode* result = new (m_globalData) BreakNode(lineNumber, *ident);
        setExceptionLocation(result, eStart, eEnd, eEnd);
        result->setLoc(startLine, endLine);
        return result;
    }

    StatementNode* createContinueStatement(int lineNumber, int eStart, int eEnd, int startLine, int endLine)
    {
        ContinueNode* result = new (m_globalData) ContinueNode(m_globalData, lineNumber);
        setExceptionLocation(result, eStart, eEnd, eEnd);
        result->setLoc(startLine, endLine);
        return result;
    }

    StatementNode* createContinueStatement(int lineNumber, const Identifier* ident, int eStart, int eEnd, int startLine, int endLine)
    {
        ContinueNode* result = new (m_globalData) ContinueNode(lineNumber, *ident);
        setExceptionLocation(result, eStart, eEnd, eEnd);
        result->setLoc(startLine, endLine);
        return result;
    }

    StatementNode* createTryStatement(int lineNumber, StatementNode* tryBlock, const Identifier* ident, StatementNode* catchBlock, StatementNode* finallyBlock, int startLine, int endLine)
    {
        TryNode* result = new (m_globalData) TryNode(lineNumber, tryBlock, *ident, catchBlock, finallyBlock);
        if (catchBlock)
            usesCatch();
        result->setLoc(startLine, endLine);
        return result;
    }

    StatementNode* createSwitchStatement(int lineNumber, ExpressionNode* expr, ClauseListNode* firstClauses, CaseClauseNode* defaultClause, ClauseListNode* secondClauses, int startLine, int endLine)
    {
        CaseBlockNode* cases = new (m_globalData) CaseBlockNode(firstClauses, defaultClause, secondClauses);
        SwitchNode* result = new (m_globalData) SwitchNode(lineNumber, expr, cases);
        result->setLoc(startLine, endLine);
        return result;
    }

    StatementNode* createWhileStatement(int lineNumber, ExpressionNode* expr, StatementNode* statement, int startLine, int endLine)
    {
        WhileNode* result = new (m_globalData) WhileNode(lineNumber, expr, statement);
        result->setLoc(startLine, endLine);
        return result;
    }

    StatementNode* createDoWhileStatement(int lineNumber, StatementNode* statement, ExpressionNode* expr, int startLine, int endLine)
    {
        DoWhileNode* result = new (m_globalData) DoWhileNode(lineNumber, statement, expr);
        result->setLoc(startLine, endLine);
        return result;
    }

    StatementNode* createLabelStatement(int lineNumber, const Identifier* ident, StatementNode* statement, int start, int end)
    {
        LabelNode* result = new (m_globalData) LabelNode(lineNumber, *ident, statement);
        setExceptionLocation(result, start, end, end);
        return result;
    }

    StatementNode* createWithStatement(int lineNumber, ExpressionNode* expr, StatementNode* statement, int start, int end, int startLine, int endLine)
    {
        usesWith();
        WithNode* result = new (m_globalData) WithNode(lineNumber, expr, statement, end, end - start);
        result->setLoc(startLine, endLine);
        return result;
    }    
    
    StatementNode* createThrowStatement(int lineNumber, ExpressionNode* expr, int start, int end, int startLine, int endLine)
    {
        ThrowNode* result = new (m_globalData) ThrowNode(lineNumber, expr);
        result->setLoc(startLine, endLine);
        setExceptionLocation(result, start, end, end);
        return result;
    }
    
    StatementNode* createDebugger(int lineNumber, int startLine, int endLine)
    {
        DebuggerStatementNode* result = new (m_globalData) DebuggerStatementNode(lineNumber);
        result->setLoc(startLine, endLine);
        return result;
    }
    
    StatementNode* createConstStatement(int lineNumber, ConstDeclNode* decls, int startLine, int endLine)
    {
        ConstStatementNode* result = new (m_globalData) ConstStatementNode(lineNumber, decls);
        result->setLoc(startLine, endLine);
        return result;
    }

    ConstDeclNode* appendConstDecl(int lineNumber, ConstDeclNode* tail, const Identifier* name, ExpressionNode* initializer)
    {
        ConstDeclNode* result = new (m_globalData) ConstDeclNode(lineNumber, *name, initializer);
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

    ExpressionNode* combineCommaNodes(int lineNumber, ExpressionNode* list, ExpressionNode* init)
    {
        if (!list)
            return init;
        if (list->isCommaNode()) {
            static_cast<CommaNode*>(list)->append(init);
            return list;
        }
        return new (m_globalData) CommaNode(lineNumber, list, init);
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
    void appendBinaryOperation(int lineNumber, int& operandStackDepth, int&, const BinaryOperand& lhs, const BinaryOperand& rhs)
    {
        operandStackDepth++;
        m_binaryOperandStack.append(std::make_pair(makeBinaryNode(lineNumber, m_binaryOperatorStack.last().first, lhs, rhs), BinaryOpInfo(lhs.second, rhs.second)));
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

    ExpressionNode* createAssignment(int lineNumber, int& assignmentStackDepth, ExpressionNode* rhs, int initialAssignmentCount, int currentAssignmentCount, int lastTokenEnd)
    {
        ExpressionNode* result = makeAssignNode(lineNumber, m_assignmentInfoStack.last().m_node, m_assignmentInfoStack.last().m_op, rhs, m_assignmentInfoStack.last().m_initAssignments != initialAssignmentCount, m_assignmentInfoStack.last().m_initAssignments != currentAssignmentCount, m_assignmentInfoStack.last().m_start, m_assignmentInfoStack.last().m_divot + 1, lastTokenEnd);
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
    ExpressionNode* createNumber(int lineNumber, double d)
    {
        return new (m_globalData) NumberNode(lineNumber, d);
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

ExpressionNode* ASTBuilder::makeTypeOfNode(int lineNumber, ExpressionNode* expr)
{
    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        return new (m_globalData) TypeOfResolveNode(lineNumber, resolve->identifier());
    }
    return new (m_globalData) TypeOfValueNode(lineNumber, expr);
}

ExpressionNode* ASTBuilder::makeDeleteNode(int lineNumber, ExpressionNode* expr, int start, int divot, int end)
{
    if (!expr->isLocation())
        return new (m_globalData) DeleteValueNode(lineNumber, expr);
    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        return new (m_globalData) DeleteResolveNode(lineNumber, resolve->identifier(), divot, divot - start, end - divot);
    }
    if (expr->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(expr);
        return new (m_globalData) DeleteBracketNode(lineNumber, bracket->base(), bracket->subscript(), divot, divot - start, end - divot);
    }
    ASSERT(expr->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(expr);
    return new (m_globalData) DeleteDotNode(lineNumber, dot->base(), dot->identifier(), divot, divot - start, end - divot);
}

ExpressionNode* ASTBuilder::makeNegateNode(int lineNumber, ExpressionNode* n)
{
    if (n->isNumber()) {
        NumberNode* numberNode = static_cast<NumberNode*>(n);
        numberNode->setValue(-numberNode->value());
        return numberNode;
    }

    return new (m_globalData) NegateNode(lineNumber, n);
}

ExpressionNode* ASTBuilder::makeBitwiseNotNode(int lineNumber, ExpressionNode* expr)
{
    if (expr->isNumber())
        return createNumber(lineNumber, ~toInt32(static_cast<NumberNode*>(expr)->value()));
    return new (m_globalData) BitwiseNotNode(lineNumber, expr);
}

ExpressionNode* ASTBuilder::makeMultNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    expr1 = expr1->stripUnaryPlus();
    expr2 = expr2->stripUnaryPlus();

    if (expr1->isNumber() && expr2->isNumber())
        return createNumber(lineNumber, static_cast<NumberNode*>(expr1)->value() * static_cast<NumberNode*>(expr2)->value());

    if (expr1->isNumber() && static_cast<NumberNode*>(expr1)->value() == 1)
        return new (m_globalData) UnaryPlusNode(lineNumber, expr2);

    if (expr2->isNumber() && static_cast<NumberNode*>(expr2)->value() == 1)
        return new (m_globalData) UnaryPlusNode(lineNumber, expr1);

    return new (m_globalData) MultNode(lineNumber, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeDivNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    expr1 = expr1->stripUnaryPlus();
    expr2 = expr2->stripUnaryPlus();

    if (expr1->isNumber() && expr2->isNumber())
        return createNumber(lineNumber, static_cast<NumberNode*>(expr1)->value() / static_cast<NumberNode*>(expr2)->value());
    return new (m_globalData) DivNode(lineNumber, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeModNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    expr1 = expr1->stripUnaryPlus();
    expr2 = expr2->stripUnaryPlus();
    
    if (expr1->isNumber() && expr2->isNumber())
        return createNumber(lineNumber, fmod(static_cast<NumberNode*>(expr1)->value(), static_cast<NumberNode*>(expr2)->value()));
    return new (m_globalData) ModNode(lineNumber, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeAddNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber())
        return createNumber(lineNumber, static_cast<NumberNode*>(expr1)->value() + static_cast<NumberNode*>(expr2)->value());
    return new (m_globalData) AddNode(lineNumber, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeSubNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    expr1 = expr1->stripUnaryPlus();
    expr2 = expr2->stripUnaryPlus();

    if (expr1->isNumber() && expr2->isNumber())
        return createNumber(lineNumber, static_cast<NumberNode*>(expr1)->value() - static_cast<NumberNode*>(expr2)->value());
    return new (m_globalData) SubNode(lineNumber, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeLeftShiftNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber())
        return createNumber(lineNumber, toInt32(static_cast<NumberNode*>(expr1)->value()) << (toUInt32(static_cast<NumberNode*>(expr2)->value()) & 0x1f));
    return new (m_globalData) LeftShiftNode(lineNumber, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeRightShiftNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber())
        return createNumber(lineNumber, toInt32(static_cast<NumberNode*>(expr1)->value()) >> (toUInt32(static_cast<NumberNode*>(expr2)->value()) & 0x1f));
    return new (m_globalData) RightShiftNode(lineNumber, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeURightShiftNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber())
        return createNumber(lineNumber, toUInt32(static_cast<NumberNode*>(expr1)->value()) >> (toUInt32(static_cast<NumberNode*>(expr2)->value()) & 0x1f));
    return new (m_globalData) UnsignedRightShiftNode(lineNumber, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeBitOrNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber())
        return createNumber(lineNumber, toInt32(static_cast<NumberNode*>(expr1)->value()) | toInt32(static_cast<NumberNode*>(expr2)->value()));
    return new (m_globalData) BitOrNode(lineNumber, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeBitAndNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber())
        return createNumber(lineNumber, toInt32(static_cast<NumberNode*>(expr1)->value()) & toInt32(static_cast<NumberNode*>(expr2)->value()));
    return new (m_globalData) BitAndNode(lineNumber, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeBitXOrNode(int lineNumber, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber())
        return createNumber(lineNumber, toInt32(static_cast<NumberNode*>(expr1)->value()) ^ toInt32(static_cast<NumberNode*>(expr2)->value()));
    return new (m_globalData) BitXOrNode(lineNumber, expr1, expr2, rightHasAssignments);
}

ExpressionNode* ASTBuilder::makeFunctionCallNode(int lineNumber, ExpressionNode* func, ArgumentsNode* args, int start, int divot, int end)
{
    if (!func->isLocation())
        return new (m_globalData) FunctionCallValueNode(lineNumber, func, args, divot, divot - start, end - divot);
    if (func->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(func);
        const Identifier& identifier = resolve->identifier();
        if (identifier == m_globalData->propertyNames->eval) {
            usesEval();
            return new (m_globalData) EvalFunctionCallNode(lineNumber, args, divot, divot - start, end - divot);
        }
        return new (m_globalData) FunctionCallResolveNode(lineNumber, identifier, args, divot, divot - start, end - divot);
    }
    if (func->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(func);
        FunctionCallBracketNode* node = new (m_globalData) FunctionCallBracketNode(lineNumber, bracket->base(), bracket->subscript(), args, divot, divot - start, end - divot);
        node->setSubexpressionInfo(bracket->divot(), bracket->endOffset());
        return node;
    }
    ASSERT(func->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(func);
    FunctionCallDotNode* node;
    if (dot->identifier() == m_globalData->propertyNames->call)
        node = new (m_globalData) CallFunctionCallDotNode(lineNumber, dot->base(), dot->identifier(), args, divot, divot - start, end - divot);
    else if (dot->identifier() == m_globalData->propertyNames->apply)
        node = new (m_globalData) ApplyFunctionCallDotNode(lineNumber, dot->base(), dot->identifier(), args, divot, divot - start, end - divot);
    else
        node = new (m_globalData) FunctionCallDotNode(lineNumber, dot->base(), dot->identifier(), args, divot, divot - start, end - divot);
    node->setSubexpressionInfo(dot->divot(), dot->endOffset());
    return node;
}

ExpressionNode* ASTBuilder::makeBinaryNode(int lineNumber, int token, pair<ExpressionNode*, BinaryOpInfo> lhs, pair<ExpressionNode*, BinaryOpInfo> rhs)
{
    switch (token) {
    case OR:
        return new (m_globalData) LogicalOpNode(lineNumber, lhs.first, rhs.first, OpLogicalOr);

    case AND:
        return new (m_globalData) LogicalOpNode(lineNumber, lhs.first, rhs.first, OpLogicalAnd);

    case BITOR:
        return makeBitOrNode(lineNumber, lhs.first, rhs.first, rhs.second.hasAssignment);

    case BITXOR:
        return makeBitXOrNode(lineNumber, lhs.first, rhs.first, rhs.second.hasAssignment);

    case BITAND:
        return makeBitAndNode(lineNumber, lhs.first, rhs.first, rhs.second.hasAssignment);

    case EQEQ:
        return new (m_globalData) EqualNode(lineNumber, lhs.first, rhs.first, rhs.second.hasAssignment);

    case NE:
        return new (m_globalData) NotEqualNode(lineNumber, lhs.first, rhs.first, rhs.second.hasAssignment);

    case STREQ:
        return new (m_globalData) StrictEqualNode(lineNumber, lhs.first, rhs.first, rhs.second.hasAssignment);

    case STRNEQ:
        return new (m_globalData) NotStrictEqualNode(lineNumber, lhs.first, rhs.first, rhs.second.hasAssignment);

    case LT:
        return new (m_globalData) LessNode(lineNumber, lhs.first, rhs.first, rhs.second.hasAssignment);

    case GT:
        return new (m_globalData) GreaterNode(lineNumber, lhs.first, rhs.first, rhs.second.hasAssignment);

    case LE:
        return new (m_globalData) LessEqNode(lineNumber, lhs.first, rhs.first, rhs.second.hasAssignment);

    case GE:
        return new (m_globalData) GreaterEqNode(lineNumber, lhs.first, rhs.first, rhs.second.hasAssignment);

    case INSTANCEOF: {
        InstanceOfNode* node = new (m_globalData) InstanceOfNode(lineNumber, lhs.first, rhs.first, rhs.second.hasAssignment);
        setExceptionLocation(node, lhs.second.start, rhs.second.start, rhs.second.end);
        return node;
    }

    case INTOKEN: {
        InNode* node = new (m_globalData) InNode(lineNumber, lhs.first, rhs.first, rhs.second.hasAssignment);
        setExceptionLocation(node, lhs.second.start, rhs.second.start, rhs.second.end);
        return node;
    }

    case LSHIFT:
        return makeLeftShiftNode(lineNumber, lhs.first, rhs.first, rhs.second.hasAssignment);

    case RSHIFT:
        return makeRightShiftNode(lineNumber, lhs.first, rhs.first, rhs.second.hasAssignment);

    case URSHIFT:
        return makeURightShiftNode(lineNumber, lhs.first, rhs.first, rhs.second.hasAssignment);

    case PLUS:
        return makeAddNode(lineNumber, lhs.first, rhs.first, rhs.second.hasAssignment);

    case MINUS:
        return makeSubNode(lineNumber, lhs.first, rhs.first, rhs.second.hasAssignment);

    case TIMES:
        return makeMultNode(lineNumber, lhs.first, rhs.first, rhs.second.hasAssignment);

    case DIVIDE:
        return makeDivNode(lineNumber, lhs.first, rhs.first, rhs.second.hasAssignment);

    case MOD:
        return makeModNode(lineNumber, lhs.first, rhs.first, rhs.second.hasAssignment);
    }
    CRASH();
    return 0;
}

ExpressionNode* ASTBuilder::makeAssignNode(int lineNumber, ExpressionNode* loc, Operator op, ExpressionNode* expr, bool locHasAssignments, bool exprHasAssignments, int start, int divot, int end)
{
    if (!loc->isLocation())
        return new (m_globalData) AssignErrorNode(lineNumber, loc, op, expr, divot, divot - start, end - divot);

    if (loc->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(loc);
        if (op == OpEqual) {
            if (expr->isFuncExprNode())
                static_cast<FuncExprNode*>(expr)->body()->setInferredName(resolve->identifier());
            AssignResolveNode* node = new (m_globalData) AssignResolveNode(lineNumber, resolve->identifier(), expr, exprHasAssignments);
            setExceptionLocation(node, start, divot, end);
            return node;
        }
        return new (m_globalData) ReadModifyResolveNode(lineNumber, resolve->identifier(), op, expr, exprHasAssignments, divot, divot - start, end - divot);
    }
    if (loc->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(loc);
        if (op == OpEqual)
            return new (m_globalData) AssignBracketNode(lineNumber, bracket->base(), bracket->subscript(), expr, locHasAssignments, exprHasAssignments, bracket->divot(), bracket->divot() - start, end - bracket->divot());
        ReadModifyBracketNode* node = new (m_globalData) ReadModifyBracketNode(lineNumber, bracket->base(), bracket->subscript(), op, expr, locHasAssignments, exprHasAssignments, divot, divot - start, end - divot);
        node->setSubexpressionInfo(bracket->divot(), bracket->endOffset());
        return node;
    }
    ASSERT(loc->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(loc);
    if (op == OpEqual) {
        if (expr->isFuncExprNode())
            static_cast<FuncExprNode*>(expr)->body()->setInferredName(dot->identifier());
        return new (m_globalData) AssignDotNode(lineNumber, dot->base(), dot->identifier(), expr, exprHasAssignments, dot->divot(), dot->divot() - start, end - dot->divot());
    }

    ReadModifyDotNode* node = new (m_globalData) ReadModifyDotNode(lineNumber, dot->base(), dot->identifier(), op, expr, exprHasAssignments, divot, divot - start, end - divot);
    node->setSubexpressionInfo(dot->divot(), dot->endOffset());
    return node;
}

ExpressionNode* ASTBuilder::makePrefixNode(int lineNumber, ExpressionNode* expr, Operator op, int start, int divot, int end)
{
    if (!expr->isLocation())
        return new (m_globalData) PrefixErrorNode(lineNumber, expr, op, divot, divot - start, end - divot);

    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        return new (m_globalData) PrefixResolveNode(lineNumber, resolve->identifier(), op, divot, divot - start, end - divot);
    }
    if (expr->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(expr);
        PrefixBracketNode* node = new (m_globalData) PrefixBracketNode(lineNumber, bracket->base(), bracket->subscript(), op, divot, divot - start, end - divot);
        node->setSubexpressionInfo(bracket->divot(), bracket->startOffset());
        return node;
    }
    ASSERT(expr->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(expr);
    PrefixDotNode* node = new (m_globalData) PrefixDotNode(lineNumber, dot->base(), dot->identifier(), op, divot, divot - start, end - divot);
    node->setSubexpressionInfo(dot->divot(), dot->startOffset());
    return node;
}

ExpressionNode* ASTBuilder::makePostfixNode(int lineNumber, ExpressionNode* expr, Operator op, int start, int divot, int end)
{
    if (!expr->isLocation())
        return new (m_globalData) PostfixErrorNode(lineNumber, expr, op, divot, divot - start, end - divot);

    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        return new (m_globalData) PostfixResolveNode(lineNumber, resolve->identifier(), op, divot, divot - start, end - divot);
    }
    if (expr->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(expr);
        PostfixBracketNode* node = new (m_globalData) PostfixBracketNode(lineNumber, bracket->base(), bracket->subscript(), op, divot, divot - start, end - divot);
        node->setSubexpressionInfo(bracket->divot(), bracket->endOffset());
        return node;

    }
    ASSERT(expr->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(expr);
    PostfixDotNode* node = new (m_globalData) PostfixDotNode(lineNumber, dot->base(), dot->identifier(), op, divot, divot - start, end - divot);
    node->setSubexpressionInfo(dot->divot(), dot->endOffset());
    return node;
}

}

#endif
