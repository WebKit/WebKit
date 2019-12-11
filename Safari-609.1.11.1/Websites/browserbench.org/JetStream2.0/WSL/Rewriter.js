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

// FIXME: This should have sensible behavior when it encounters definitions that it cannot handle. Right
// now we are hackishly preventing this by wrapping things in TypeRef. That's probably wrong.
// https://bugs.webkit.org/show_bug.cgi?id=176208
class Rewriter {
    constructor()
    {
        this._mapping = new Map();
    }
    
    _mapNode(oldItem, newItem)
    {
        this._mapping.set(oldItem, newItem);
        return newItem;
    }
    
    _getMapping(oldItem)
    {
        let result = this._mapping.get(oldItem);
        if (result)
            return result;
        return oldItem;
    }
    
    // We return identity for anything that is not inside a function/struct body. When processing
    // function bodies, we only recurse into them and never out of them - for example if there is a
    // function call to another function then we don't rewrite the other function. This is how we stop
    // that.
    visitFuncDef(node) { return node; }
    visitNativeFunc(node) { return node; }
    visitNativeFuncInstance(node) { return node; }
    visitNativeType(node) { return node; }
    visitTypeDef(node) { return node; }
    visitStructType(node) { return node; }
    visitConstexprTypeParameter(node) { return node; }
    visitProtocolDecl(node) { return node; }
    visitEnumType(node) { return node; }

    // This is almost wrong. We instantiate Func in Substitution in ProtocolDecl. Then, we end up
    // not rewriting type variables. I think that just works because not rewriting them there is OK.
    // Everywhere else, it's mandatory that we don't rewrite these because we always assume that
    // type variables are outside the scope of rewriting.
    visitTypeVariable(node) { return node; }

    visitProtocolFuncDecl(node)
    {
        let result = new ProtocolFuncDecl(
            node.origin, node.name,
            node.returnType.visit(this),
            node.typeParameters.map(parameter => parameter.visit(this)),
            node.parameters.map(parameter => parameter.visit(this)),
            node.isCast,
            node.shaderType);
        result.protocolDecl = node.protocolDecl;
        result.possibleOverloads = node.possibleOverloads;
        return result;
    }
    
    visitNativeTypeInstance(node)
    {
        return new NativeTypeInstance(
            node.type.visit(this),
            node.typeArguments.map(argument => argument.visit(this)));
    }
    
    visitFuncParameter(node)
    {
        let result = new FuncParameter(node.origin, node.name, node.type.visit(this));
        this._mapNode(node, result);
        result.ePtr = node.ePtr;
        return result;
    }
    
    visitVariableDecl(node)
    {
        let result = new VariableDecl(
            node.origin, node.name,
            node.type.visit(this),
            Node.visit(node.initializer, this));
        this._mapNode(node, result);
        result.ePtr = node.ePtr;
        return result;
    }
    
    visitBlock(node)
    {
        let result = new Block(node.origin);
        for (let statement of node.statements)
            result.add(statement.visit(this));
        return result;
    }
    
    visitCommaExpression(node)
    {
        return new CommaExpression(node.origin, node.list.map(expression => {
            let result = expression.visit(this);
            if (!result)
                throw new Error("Null result from " + expression);
            return result;
        }));
    }
    
    visitProtocolRef(node)
    {
        return node;
    }
    
    visitTypeRef(node)
    {
        let result = new TypeRef(node.origin, node.name, node.typeArguments.map(typeArgument => typeArgument.visit(this)));
        result.type = Node.visit(node.type, this);
        return result;
    }
    
    visitField(node)
    {
        return new Field(node.origin, node.name, node.type.visit(this));
    }
    
    visitEnumMember(node)
    {
        return new EnumMember(node.origin, node.name, node.value.visit(this));
    }
    
    visitEnumLiteral(node)
    {
        let result = new EnumLiteral(node.origin, node.member);
        result.ePtr = node.ePtr;
        return result;
    }
    
    visitReferenceType(node)
    {
        return new node.constructor(node.origin, node.addressSpace, node.elementType.visit(this));
    }
    
    visitPtrType(node)
    {
        return this.visitReferenceType(node);
    }
    
    visitArrayRefType(node)
    {
        return this.visitReferenceType(node);
    }
    
    visitArrayType(node)
    {
        return new ArrayType(node.origin, node.elementType.visit(this), node.numElements.visit(this));
    }
    
    visitAssignment(node)
    {
        let result = new Assignment(node.origin, node.lhs.visit(this), node.rhs.visit(this));
        result.type = Node.visit(node.type, this);
        return result;
    }
    
    visitReadModifyWriteExpression(node)
    {
        let result = new ReadModifyWriteExpression(node.origin, node.lValue.visit(this));
        result.oldValueVar = node.oldValueVar.visit(this);
        result.newValueVar = node.newValueVar.visit(this);
        result.newValueExp = node.newValueExp.visit(this);
        result.resultExp = node.resultExp.visit(this);
        return result;
    }
    
    visitDereferenceExpression(node)
    {
        let result = new DereferenceExpression(node.origin, node.ptr.visit(this));
        result.type = Node.visit(node.type, this);
        result.addressSpace = node.addressSpace;
        return result;
    }
    
    _handlePropertyAccessExpression(result, node)
    {
        result.possibleGetOverloads = node.possibleGetOverloads;
        result.possibleSetOverloads = node.possibleSetOverloads;
        result.possibleAndOverloads = node.possibleAndOverloads;
        result.baseType = Node.visit(node.baseType, this);
        result.callForGet = Node.visit(node.callForGet, this);
        result.resultTypeForGet = Node.visit(node.resultTypeForGet, this);
        result.callForAnd = Node.visit(node.callForAnd, this);
        result.resultTypeForAnd = Node.visit(node.resultTypeForAnd, this);
        result.callForSet = Node.visit(node.callForSet, this);
        result.errorForSet = node.errorForSet;
        result.updateCalls();
    }
    
    visitDotExpression(node)
    {
        let result = new DotExpression(node.origin, node.struct.visit(this), node.fieldName);
        this._handlePropertyAccessExpression(result, node);
        return result;
    }
    
    visitIndexExpression(node)
    {
        let result = new IndexExpression(node.origin, node.array.visit(this), node.index.visit(this));
        this._handlePropertyAccessExpression(result, node);
        return result;
    }
    
    visitMakePtrExpression(node)
    {
        let result = new MakePtrExpression(node.origin, node.lValue.visit(this));
        result.ePtr = node.ePtr;
        return result;
    }
    
    visitMakeArrayRefExpression(node)
    {
        let result = new MakeArrayRefExpression(node.origin, node.lValue.visit(this));
        if (node.numElements)
            result.numElements = node.numElements.visit(this);
        result.ePtr = node.ePtr;
        return result;
    }
    
    visitConvertPtrToArrayRefExpression(node)
    {
        let result = new ConvertPtrToArrayRefExpression(node.origin, node.lValue.visit(this));
        result.ePtr = node.ePtr;
        return result;
    }
    
    visitVariableRef(node)
    {
        let result = new VariableRef(node.origin, node.name);
        result.variable = this._getMapping(node.variable);
        return result;
    }
    
    visitReturn(node)
    {
        return new Return(node.origin, Node.visit(node.value, this));
    }
    
    visitContinue(node)
    {
        return new Continue(node.origin);
    }
    
    visitBreak(node)
    {
        return new Break(node.origin);
    }

    visitTrapStatement(node)
    {
        return new TrapStatement(node.origin);
    }
    
    visitGenericLiteral(node)
    {
        // FIXME: This doesn't seem right.
        let result = new IntLiteral(node.origin, node.value);
        result.type = node.type.visit(this);
        result.ePtr = node.ePtr;
        return result;
    }
    
    visitGenericLiteralType(node)
    {
        let result = new node.constructor(node.origin, node.value);
        result.type = Node.visit(node.type, this);
        result.preferredType = node.preferredType.visit(this);
        return result;
    }

    visitBoolLiteral(node)
    {
        return node;
    }
    
    visitNullLiteral(node)
    {
        let result = new NullLiteral(node.origin);
        result.type = node.type.visit(this);
        result.ePtr = node.ePtr;
        return result;
    }
    
    visitNullType(node)
    {
        let result = new NullType(node.origin);
        result.type = Node.visit(node.type, this);
        return result;
    }

    processDerivedCallData(node, result)
    {
        let handleTypeArguments = actualTypeArguments => {
            if (actualTypeArguments)
                return actualTypeArguments.map(actualTypeArgument => actualTypeArgument.visit(this));
            else
                return null;
        }
        result.actualTypeArguments = handleTypeArguments(node.actualTypeArguments);
        result.instantiatedActualTypeArguments = handleTypeArguments(node.instantiatedActualTypeArguments);
        let argumentTypes = node.argumentTypes;
        if (argumentTypes)
            result.argumentTypes = argumentTypes.map(argumentType => argumentType.visit(this));
        result.func = node.func;
        result.nativeFuncInstance = node.nativeFuncInstance;
        result.possibleOverloads = node.possibleOverloads;
        if (node.isCast)
            result.setCastData(node.returnType.visit(this));
        result.resultType = Node.visit(node.resultType, this);
        result.resultEPtr = node.resultEPtr;
        return result;
    }

    visitCallExpression(node)
    {
        let result = new CallExpression(
            node.origin, node.name,
            node.typeArguments.map(typeArgument => typeArgument.visit(this)),
            node.argumentList.map(argument => Node.visit(argument, this)));
        return this.processDerivedCallData(node, result);
    }
    
    visitFunctionLikeBlock(node)
    {
        let result = new FunctionLikeBlock(
            node.origin,
            Node.visit(node.returnType, this),
            node.argumentList.map(argument => argument.visit(this)),
            node.parameters.map(parameter => parameter.visit(this)),
            node.body.visit(this));
        result.returnEPtr = node.returnEPtr;
        return result;
    }
    
    visitLogicalNot(node)
    {
        let result = new LogicalNot(node.origin, node.operand.visit(this));
        result.ePtr = node.ePtr;
        return result;
    }
    
    visitLogicalExpression(node)
    {
        let result = new LogicalExpression(node.origin, node.text, node.left.visit(this), node.right.visit(this));
        result.ePtr = node.ePtr;
        return result;
    }

    visitIfStatement(node)
    {
        return new IfStatement(node.origin, node.conditional.visit(this), node.body.visit(this), Node.visit(node.elseBody, this));
    }

    visitWhileLoop(node)
    {
        return new WhileLoop(node.origin, node.conditional.visit(this), node.body.visit(this));
    }

    visitDoWhileLoop(node)
    {
        return new DoWhileLoop(node.origin, node.body.visit(this), node.conditional.visit(this));
    }

    visitForLoop(node)
    {
        return new ForLoop(node.origin,
            Node.visit(node.initialization, this),
            Node.visit(node.condition, this),
            Node.visit(node.increment, this),
            node.body.visit(this));
    }
    
    visitSwitchStatement(node)
    {
        let result = new SwitchStatement(node.origin, Node.visit(node.value, this));
        for (let switchCase of node.switchCases)
            result.add(switchCase.visit(this));
        result.type = Node.visit(node.type, this);
        return result;
    }
    
    visitSwitchCase(node)
    {
        return new SwitchCase(node.origin, Node.visit(node.value, this), node.body.visit(this));
    }
    
    visitAnonymousVariable(node)
    {
        let result = new AnonymousVariable(node.origin, node.type.visit(this));
        result._index = node._index;
        this._mapNode(node, result);
        result.ePtr = node.ePtr;
        return result;
    }
    
    visitIdentityExpression(node)
    {
        return new IdentityExpression(node.target.visit(this));
    }
}

