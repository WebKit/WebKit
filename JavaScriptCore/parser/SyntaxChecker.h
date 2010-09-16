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

#ifndef SyntaxChecker_h
#define SyntaxChecker_h

namespace JSC {
class SyntaxChecker {
public:
    SyntaxChecker(JSGlobalData* , Lexer*)
    {
    }

    typedef SyntaxChecker FunctionBodyBuilder;

    typedef int Expression;
    typedef int SourceElements;
    typedef int Arguments;
    typedef int Comma;
    struct Property {
        ALWAYS_INLINE Property(void* = 0)
            : type((PropertyNode::Type)0)
        {
        }
        ALWAYS_INLINE Property(const Identifier* ident, PropertyNode::Type ty)
        : name(ident)
        , type(ty)
        {
        }
        ALWAYS_INLINE Property(PropertyNode::Type ty)
            : name(0)
            , type(ty)
        {
        }
        ALWAYS_INLINE bool operator!() { return !type; }
        const Identifier* name;
        PropertyNode::Type type;
    };
    typedef int PropertyList;
    typedef int ElementList;
    typedef int ArgumentsList;
    typedef int FormalParameterList;
    typedef int FunctionBody;
    typedef int Statement;
    typedef int ClauseList;
    typedef int Clause;
    typedef int ConstDeclList;
    typedef int BinaryOperand;
    
    static const bool CreatesAST = false;
    static const bool NeedsFreeVariableInfo = false;

    int createSourceElements() { return 1; }
    int makeFunctionCallNode(int, int, int, int, int) { return 1; }
    void appendToComma(int, int) { }
    int createCommaExpr(int, int) { return 1; }
    int makeAssignNode(int, Operator, int, bool, bool, int, int, int) { return 1; }
    int makePrefixNode(int, Operator, int, int, int) { return 1; }
    int makePostfixNode(int, Operator, int, int, int) { return 1; }
    int makeTypeOfNode(int) { return 1; }
    int makeDeleteNode(int, int, int, int) { return 1; }
    int makeNegateNode(int) { return 1; }
    int makeBitwiseNotNode(int) { return 1; }
    int createLogicalNot(int) { return 1; }
    int createUnaryPlus(int) { return 1; }
    int createVoid(int) { return 1; }
    int thisExpr() { return 1; }
    int createResolve(const Identifier*, int) { return 1; }
    int createObjectLiteral() { return 1; }
    int createObjectLiteral(int) { return 1; }
    int createArray(int) { return 1; }
    int createArray(int, int) { return 1; }
    int createNumberExpr(double) { return 1; }
    int createString(const Identifier*) { return 1; }
    int createBoolean(bool) { return 1; }
    int createNull() { return 1; }
    int createBracketAccess(int, int, bool, int, int, int) { return 1; }
    int createDotAccess(int, const Identifier&, int, int, int) { return 1; }
    int createRegex(const Identifier&, const Identifier&, int) { return 1; }
    int createNewExpr(int, int, int, int, int) { return 1; }
    int createNewExpr(int, int, int) { return 1; }
    int createConditionalExpr(int, int, int) { return 1; }
    int createAssignResolve(const Identifier&, int, bool, int, int, int) { return 1; }
    int createFunctionExpr(const Identifier*, int, int, int, int, int, int) { return 1; }
    int createFunctionBody() { return 1; }
    int createArguments() { return 1; }
    int createArguments(int) { return 1; }
    int createArgumentsList(int) { return 1; }
    int createArgumentsList(int, int) { return 1; }
    template <bool complete> Property createProperty(const Identifier* name, int, PropertyNode::Type type)
    {
        ASSERT(name);
        if (!complete)
            return Property(type);
        return Property(name, type);
    }
    template <bool complete> Property createProperty(JSGlobalData* globalData, double name, int, PropertyNode::Type type)
    {
        if (!complete)
            return Property(type);
        return Property(&globalData->parser->arena().identifierArena().makeNumericIdentifier(globalData, name), type);
    }
    int createPropertyList(Property) { return 1; }
    int createPropertyList(Property, int) { return 1; }
    int createElementList(int, int) { return 1; }
    int createElementList(int, int, int) { return 1; }
    int createFormalParameterList(const Identifier&) { return 1; }
    int createFormalParameterList(int, const Identifier&) { return 1; }
    int createClause(int, int) { return 1; }
    int createClauseList(int) { return 1; }
    int createClauseList(int, int) { return 1; }
    void setUsesArguments(int) { }
    int createFuncDeclStatement(const Identifier*, int, int, int, int, int, int) { return 1; }
    int createBlockStatement(int, int, int) { return 1; }
    int createExprStatement(int, int, int) { return 1; }
    int createIfStatement(int, int, int, int) { return 1; }
    int createIfStatement(int, int, int, int, int) { return 1; }
    int createForLoop(int, int, int, int, bool, int, int) { return 1; }
    int createForInLoop(const Identifier*, int, int, int, int, int, int, int, int, int, int) { return 1; }
    int createForInLoop(int, int, int, int, int, int, int, int) { return 1; }
    int createEmptyStatement() { return 1; }
    int createVarStatement(int, int, int) { return 1; }
    int createReturnStatement(int, int, int, int, int) { return 1; }
    int createBreakStatement(int, int, int, int) { return 1; }
    int createBreakStatement(const Identifier*, int, int, int, int) { return 1; }
    int createContinueStatement(int, int, int, int) { return 1; }
    int createContinueStatement(const Identifier*, int, int, int, int) { return 1; }
    int createTryStatement(int, const Identifier*, bool, int, int, int, int) { return 1; }
    int createSwitchStatement(int, int, int, int, int, int) { return 1; }
    int createWhileStatement(int, int, int, int) { return 1; }
    int createWithStatement(int, int, int, int, int, int) { return 1; }
    int createDoWhileStatement(int, int, int, int) { return 1; }
    int createLabelStatement(const Identifier*, int, int, int) { return 1; }
    int createThrowStatement(int, int, int, int, int) { return 1; }
    int createDebugger(int, int) { return 1; }
    int createConstStatement(int, int, int) { return 1; }
    int appendConstDecl(int, const Identifier*, int) { return 1; }
    template <bool strict> Property createGetterOrSetterProperty(PropertyNode::Type type, const Identifier* name, int, int, int, int, int, int)
    {
        ASSERT(name);
        if (!strict)
            return Property(type);
        return Property(name, type);
    }

    void appendStatement(int, int) { }
    void addVar(const Identifier*, bool) { }
    int combineCommaNodes(int, int) { return 1; }
    int evalCount() const { return 0; }
    void appendBinaryExpressionInfo(int& operandStackDepth, int, int, int, int, bool) { operandStackDepth++; }
    
    // Logic to handle datastructures used during parsing of binary expressions
    void operatorStackPop(int& operatorStackDepth) { operatorStackDepth--; }
    bool operatorStackHasHigherPrecedence(int&, int) { return true; }
    BinaryOperand getFromOperandStack(int) { return 1; }
    void shrinkOperandStackBy(int& operandStackDepth, int amount) { operandStackDepth -= amount; }
    void appendBinaryOperation(int& operandStackDepth, int&, BinaryOperand, BinaryOperand) { operandStackDepth++; }
    void operatorStackAppend(int& operatorStackDepth, int, int) { operatorStackDepth++; }
    int popOperandStack(int&) { return 1; }
    
    void appendUnaryToken(int&, int, int) { }
    int unaryTokenStackLastType(int&) { ASSERT_NOT_REACHED(); return 1; }
    int unaryTokenStackLastStart(int&) { ASSERT_NOT_REACHED(); return 1; }
    void unaryTokenStackRemoveLast(int&) { }
    
    void assignmentStackAppend(int, int, int, int, int, Operator) { }
    int createAssignment(int, int, int, int, int) { ASSERT_NOT_REACHED(); return 1; }
    const Identifier& getName(const Property& property) { ASSERT(property.name); return *property.name; }
    PropertyNode::Type getType(const Property& property) { return property.type; }
};

}

#endif
