/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
"use strict";

class Visitor {
    visitProgram(node)
    {
        for (let statement of node.topLevelStatements)
            statement.visit(this);
    }
    
    visitFunc(node)
    {
        node.returnType.visit(this);
        for (let typeParameter of node.typeParameters)
            typeParameter.visit(this);
        for (let parameter of node.parameters)
            parameter.visit(this);
    }
    
    visitProtocolFuncDecl(node)
    {
        this.visitFunc(node);
    }
    
    visitFuncParameter(node)
    {
        node.type.visit(this);
    }
    
    visitFuncDef(node)
    {
        this.visitFunc(node);
        node.body.visit(this);
    }
    
    visitNativeFunc(node)
    {
        this.visitFunc(node);
    }
    
    visitNativeFuncInstance(node)
    {
        this.visitFunc(node);
        node.func.visitImplementationData(node.implementationData, this);
    }
    
    visitBlock(node)
    {
        for (let statement of node.statements)
            statement.visit(this);
    }
    
    visitCommaExpression(node)
    {
        for (let expression of node.list)
            expression.visit(this);
    }
    
    visitProtocolRef(node)
    {
    }
    
    visitProtocolDecl(node)
    {
        for (let protocol of node.extends)
            protocol.visit(this);
        for (let signature of node.signatures)
            signature.visit(this);
    }
    
    visitTypeRef(node)
    {
        for (let typeArgument of node.typeArguments)
            typeArgument.visit(this);
    }
    
    visitNativeType(node)
    {
        for (let typeParameter of node.typeParameters)
            typeParameter.visit(this);
    }
    
    visitNativeTypeInstance(node)
    {
        node.type.visit(this);
        for (let typeArgument of node.typeArguments)
            typeArgument.visit(this);
    }
    
    visitTypeDef(node)
    {
        for (let typeParameter of node.typeParameters)
            typeParameter.visit(this);
        node.type.visit(this);
    }
    
    visitStructType(node)
    {
        for (let typeParameter of node.typeParameters)
            typeParameter.visit(this);
        for (let field of node.fields)
            field.visit(this);
    }
    
    visitTypeVariable(node)
    {
        Node.visit(node.protocol, this);
    }
    
    visitConstexprTypeParameter(node)
    {
        node.type.visit(this);
    }
    
    visitField(node)
    {
        node.type.visit(this);
    }
    
    visitEnumType(node)
    {
        node.baseType.visit(this);
        for (let member of node.members)
            member.visit(this);
    }
    
    visitEnumMember(node)
    {
        Node.visit(node.value, this);
    }
    
    visitEnumLiteral(node)
    {
    }
    
    visitElementalType(node)
    {
        node.elementType.visit(this);
    }
    
    visitReferenceType(node)
    {
        this.visitElementalType(node);
    }
    
    visitPtrType(node)
    {
        this.visitReferenceType(node);
    }
    
    visitArrayRefType(node)
    {
        this.visitReferenceType(node);
    }
    
    visitArrayType(node)
    {
        this.visitElementalType(node);
        node.numElements.visit(this);
    }
    
    visitVariableDecl(node)
    {
        node.type.visit(this);
        Node.visit(node.initializer, this);
    }
    
    visitAssignment(node)
    {
        node.lhs.visit(this);
        node.rhs.visit(this);
        Node.visit(node.type, this);
    }
    
    visitReadModifyWriteExpression(node)
    {
        node.lValue.visit(this);
        node.oldValueVar.visit(this);
        node.newValueVar.visit(this);
        node.newValueExp.visit(this);
        node.resultExp.visit(this);
    }
    
    visitDereferenceExpression(node)
    {
        node.ptr.visit(this);
    }
    
    _handlePropertyAccessExpression(node)
    {
        Node.visit(node.baseType, this);
        Node.visit(node.callForGet, this);
        Node.visit(node.resultTypeForGet, this);
        Node.visit(node.callForAnd, this);
        Node.visit(node.resultTypeForAnd, this);
        Node.visit(node.callForSet, this);
    }
    
    visitDotExpression(node)
    {
        node.struct.visit(this);
        this._handlePropertyAccessExpression(node);
    }
    
    visitIndexExpression(node)
    {
        node.array.visit(this);
        node.index.visit(this);
        this._handlePropertyAccessExpression(node);
    }
    
    visitMakePtrExpression(node)
    {
        node.lValue.visit(this);
    }
    
    visitMakeArrayRefExpression(node)
    {
        node.lValue.visit(this);
        Node.visit(node.numElements, this);
    }
    
    visitConvertPtrToArrayRefExpression(node)
    {
        node.lValue.visit(this);
    }
    
    visitVariableRef(node)
    {
    }
    
    visitIfStatement(node)
    {
        node.conditional.visit(this);
        node.body.visit(this);
        Node.visit(node.elseBody, this);
    }
    
    visitWhileLoop(node)
    {
        node.conditional.visit(this);
        node.body.visit(this);
    }
    
    visitDoWhileLoop(node)
    {
        node.body.visit(this);
        node.conditional.visit(this);
    }
    
    visitForLoop(node)
    {
        Node.visit(node.initialization, this);
        Node.visit(node.condition, this);
        Node.visit(node.increment, this);
        node.body.visit(this);
    }
    
    visitSwitchStatement(node)
    {
        node.value.visit(this);
        for (let switchCase of node.switchCases)
            switchCase.visit(this);
    }
    
    visitSwitchCase(node)
    {
        Node.visit(node.value, this);
        node.body.visit(this);
    }

    visitReturn(node)
    {
        Node.visit(node.value, this);
    }

    visitContinue(node)
    {
    }

    visitBreak(node)
    {
    }

    visitTrapStatement(node)
    {
    }
    
    visitGenericLiteral(node)
    {
        node.type.visit(this);
    }
    
    visitGenericLiteralType(node)
    {
        Node.visit(node.type, this);
        node.preferredType.visit(this);
    }
    
    visitNullLiteral(node)
    {
        node.type.visit(this);
    }
    
    visitBoolLiteral(node)
    {
    }
    
    visitNullType(node)
    {
        Node.visit(node.type, this);
    }
    
    visitCallExpression(node)
    {
        for (let typeArgument of node.typeArguments)
            typeArgument.visit(this);
        for (let argument of node.argumentList)
            Node.visit(argument, this);
        let handleTypeArguments = actualTypeArguments => {
            if (actualTypeArguments) {
                for (let argument of actualTypeArguments)
                    argument.visit(this);
            }
        };
        handleTypeArguments(node.actualTypeArguments);
        handleTypeArguments(node.instantiatedActualTypeArguments);
        Node.visit(node.nativeFuncInstance, this);
        Node.visit(node.returnType, this);
        Node.visit(node.resultType, this);
    }
    
    visitLogicalNot(node)
    {
        node.operand.visit(this);
    }
    
    visitLogicalExpression(node)
    {
        node.left.visit(this);
        node.right.visit(this);
    }
    
    visitFunctionLikeBlock(node)
    {
        Node.visit(node.returnType, this);
        for (let argument of node.argumentList)
            argument.visit(this);
        for (let parameter of node.parameters)
            parameter.visit(this);
        node.body.visit(this);
        Node.visit(node.resultType, this);
    }
    
    visitAnonymousVariable(node)
    {
        Node.visit(node.type, this);
    }
    
    visitIdentityExpression(node)
    {
        node.target.visit(this);
    }
}

