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

class Checker extends Visitor {
    constructor(program)
    {
        super();
        this._program = program;
        this._currentStatement = null;
    }
    
    visitProgram(node)
    {
        for (let statement of node.topLevelStatements) {
            this._currentStatement = statement;
            statement.visit(this);
        }
    }
    
    visitProtocolDecl(node)
    {
        for (let signature of node.signatures) {
            let set = new Set();
            function consider(thing)
            {
                if (thing.isUnifiable)
                    set.add(thing);
            }
            class NoticeTypeVariable extends Visitor {
                visitTypeRef(node)
                {
                    consider(node.type);
                }
                visitVariableRef(node)
                {
                    consider(node.variable);
                }
            }
            let noticeTypeVariable = new NoticeTypeVariable();
            for (let parameterType of signature.parameterTypes)
                parameterType.visit(noticeTypeVariable);
            for (let typeParameter of signature.typeParameters) {
                if (!set.has(typeParameter))
                    throw ALTypeError(typeParameter.origin.originString, "Type parameter to protocol signature not inferrable from value parameters");
            }
            if (!set.has(node.typeVariable))
                throw new ALTypeError(signature.origin.originString, "Protocol's type variable not mentioned in signature");
        }
    }
    
    _checkTypeArguments(origin, typeParameters, typeArguments)
    {
        for (let i = 0; i < typeParameters.length; ++i) {
            let argumentIsType = typeArguments[i] instanceof Type;
            let result = node.typeArguments[i].visit(this);
            if (argumentIsType) {
                if (!typeArguments[i].inherits(typeArguments[i]))
                    throw new ALTypeError(origin.originString, "Type argument does not inherit protocol");
            } else {
                if (!result.equals(typeParameters[i].type))
                    throw new ALTypeError(origin.originString, "Wrong type for constexpr");
            }
        }
    }
    
    visitTypeRef(node)
    {
        if (!node.type)
            throw new Error("Type reference without a type in checker: " + node + " at " + node.origin);
        this._checkTypeArguments(node.origin, node.type.typeParameters, node.typeArguments);
    }
    
    visitReferenceType(node)
    {
        node.elementType.visit(this);
        
        if (node.addressSpace == "thread")
            return;
        
        if (!node.elementType.withRecursivelyInstantiatedImmediates.isPrimitive)
            throw new ALTypeError(node.origin.originString, "Illegal pointer to non-primitive type");
    }
    
    visitArrayType(node)
    {
        if (!node.numElements.isConstexpr)
            throw new ALTypeError(node.origin.originString, "Array length must be constexpr");
    }
    
    visitAssignment(node)
    {
        // FIXME: We need to check that the lhs is assignable.
        let lhsType = node.lhs.visit(this);
        let rhsType = node.rhs.visit(this);
        if (!lhsType.equals(rhsType))
            throw new ALTypeError(node.origin.originString, "Type mismatch in assignment: " + lhsType + " versus " + rhsType);
        return lhsType;
    }
    
    visitVariableRef(node)
    {
        return node.variable.type;
    }
    
    visitReturn(node)
    {
        if (node.value) {
            let resultType = node.value.visit(this);
            if (!resultType)
                throw new Error("Null result type from " + node.value);
            if (!node.func.returnType.equals(resultType))
                throw new ALTypeError(node.origin.originString, "Trying to return " + resultType + " in a function that returns " + func.returnType);
            return;
        }
        
        if (!func.returnType.equals(this._program.intrinsics.void))
            throw new ALTypeError(node.origin.originString, "Non-void function must return a value");
    }
    
    visitIntLiteral(node)
    {
        return this._program.intrinsics.int32;
    }
    
    visitCommaExpression(node)
    {
        let result = null;
        for (let expression of node.list)
            result = expression.visit(this);
        return result;
    }
    
    visitCallExpression(node)
    {
        for (let typeArgument of node.typeArguments)
            typeArgument.visit(this);
        let argumentTypes = node.argumentList.map(argument => {
            let newArgument = argument.visit(this);
            if (!newArgument)
                throw new Error("visitor returned null for " + argument);
            return newArgument;
        });
        
        let overload = null;
        for (let typeParameter of this._currentStatement.typeParameters) {
            if (!(typeParameter instanceof TypeVariable))
                continue;
            if (!typeParameter.protocol)
                continue;
            let signatures =
                typeParameter.protocol.signaturesByNameWithTypeVariable(node.name, typeParameter);
            if (!signatures)
                continue;
            overload = resolveOverloadImpl(signatures, node.typeArguments, argumentTypes);
            if (overload)
                break;
        }
        if (!overload) {
            overload = this._program.resolveFuncOverload(
                node.name, node.typeArguments, argumentTypes);
        }
        if (!overload)
            throw new ALTypeError(node.origin.originString, "Did not find function for call");
        node.func = overload.func;
        node.actualTypeArguments = overload.typeArguments.map(TypeRef.wrap);
        let result = overload.func.returnType.substituteToUnification(
            overload.func.typeParameters, overload.unificationContext);
        if (!result)
            throw new Error("Null result from CallExpression");
        return result;
    }
}

