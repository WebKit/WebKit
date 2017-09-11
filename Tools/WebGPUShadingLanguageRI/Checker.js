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
                    throw WTypeError(typeParameter.origin.originString, "Type parameter to protocol signature not inferrable from value parameters");
            }
            if (!set.has(node.typeVariable))
                throw new WTypeError(signature.origin.originString, "Protocol's type variable not mentioned in signature");
        }
    }
    
    _checkTypeArguments(origin, typeParameters, typeArguments)
    {
        for (let i = 0; i < typeParameters.length; ++i) {
            let argumentIsType = typeArguments[i] instanceof Type;
            let result = typeArguments[i].visit(this);
            if (argumentIsType) {
                if (!typeArguments[i].inherits(typeParameters[i].protocol))
                    throw new WTypeError(origin.originString, "Type argument does not inherit protocol");
            } else {
                if (!result.equalsWithCommit(typeParameters[i].type))
                    throw new WTypeError(origin.originString, "Wrong type for constexpr");
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
        
        if (!node.elementType.instantiatedType.isPrimitive)
            throw new WTypeError(node.origin.originString, "Illegal pointer to non-primitive type: " + node.elementType);
    }
    
    visitArrayType(node)
    {
        if (!node.numElements.isConstexpr)
            throw new WTypeError(node.origin.originString, "Array length must be constexpr");
    }
    
    visitVariableDecl(node)
    {
        node.type.visit(this);
        if (node.initializer) {
            let lhsType = node.type;
            let rhsType = node.initializer.visit(this);
            if (!lhsType.equalsWithCommit(rhsType))
                throw new WTypeError(node.origin.originString, "Type mismatch in variable initialization: " + lhsType + " versus " + rhsType);
        }
    }
    
    visitAssignment(node)
    {
        if (!node.lhs.isLValue)
            throw new WTypeError(node.origin.originString, "LHS of assignment is not an LValue: " + node.lhs);
        let lhsType = node.lhs.visit(this);
        let rhsType = node.rhs.visit(this);
        if (!lhsType.equalsWithCommit(rhsType))
            throw new WTypeError(node.origin.originString, "Type mismatch in assignment: " + lhsType + " versus " + rhsType);
        node.type = lhsType;
        return lhsType;
    }
    
    visitDereferenceExpression(node)
    {
        let type = node.ptr.visit(this).unifyNode;
        if (!type.isPtr)
            throw new WTypeError(node.origin.originString, "Type passed to dereference is not a pointer: " + type);
        node.type = type.elementType;
        node.addressSpace = type.addressSpace;
        return node.type;
    }
    
    visitMakePtrExpression(node)
    {
        if (!node.lValue.isLValue)
            throw new WTypeError(node.origin.originString, "Operand to \\ is not an LValue: " + node.lValue);
        
        let elementType = node.lValue.visit(this).unifyNode;
        
        return new PtrType(node.origin, node.lValue.addressSpace, elementType);
    }
    
    visitDotExpression(node)
    {
        let structType = node.struct.visit(this).unifyNode;
        
        node.structType = structType;
        
        let underlyingStruct = structType;
        
        if (structType instanceof TypeRef)
            underlyingStruct = underlyingStruct.type;
        
        if (!(underlyingStruct instanceof StructType))
            throw new WTypeError(node.origin.originString, "Operand to dot expression is not a struct type: " + structType);
        
        if (structType instanceof TypeRef) 
            underlyingStruct = underlyingStruct.instantiate(structType.typeArguments, "shallow");
        
        let field = underlyingStruct.fieldByName(node.fieldName);
        if (!field)
            throw new WTypeError(node.origin.originString, "Field " + node.fieldName + " not found in " + structType);
        return field.type;
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
            if (!node.func.returnType.equalsWithCommit(resultType))
                throw new WTypeError(node.origin.originString, "Trying to return " + resultType + " in a function that returns " + node.func.returnType);
            return;
        }
        
        if (!node.func.returnType.equalsWithCommit(this._program.intrinsics.void))
            throw new WTypeError(node.origin.originString, "Non-void function must return a value");
    }
    
    visitIntLiteral(node)
    {
        return node.type;
    }
    
    visitUintLiteral(node)
    {
        return this._program.intrinsics.uint32;
    }
    
    visitNullLiteral(node)
    {
        return node.type;
    }
    
    visitBoolLiteral(node)
    {
        return this._program.intrinsics.bool;
    }

    visitLogicalNot(node)
    {
        let resultType = node.operand.visit(this);
        if (!resultType)
            throw new Error("Trying to negate something with no type: " + node.operand);
        if (!resultType.equals(this._program.intrinsics.bool))
            throw new WError("Trying to negate something that isn't a bool: " + node.operand);
        return this._program.intrinsics.bool;
    }

    visitIfStatement(node)
    {
        let conditionalResultType = node.conditional.visit(this);
        if (!conditionalResultType)
            throw new Error("Trying to negate something with no type: " + node.conditional);
        if (!conditionalResultType.equals(this._program.intrinsics.bool))
            throw new WError("Trying to negate something that isn't a bool: " + node.conditional);
        node.body.visit(this);
        if (node.elseBody)
            node.elseBody.visit(this);
    }

    visitWhileLoop(node)
    {
        let conditionalResultType = node.conditional.visit(this);
        if (!conditionalResultType)
            throw new Error("While loop conditional has no type: " + node.conditional);
        if (!conditionalResultType.equals(this._program.intrinsics.bool))
            throw new WError("While loop conditional isn't a bool: " + node.conditional);
        node.body.visit(this);
    }

    visitDoWhileLoop(node)
    {
        node.body.visit(this);
        let conditionalResultType = node.conditional.visit(this);
        if (!conditionalResultType)
            throw new Error("Do-While loop conditional has no type: " + node.conditional);
        if (!conditionalResultType.equals(this._program.intrinsics.bool))
            throw new WError("Do-While loop conditional isn't a bool: " + node.conditional);
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
            return TypeRef.wrap(newArgument);
        });
        node.argumentTypes = argumentTypes;
        if (node.returnType)
            node.returnType.visit(this);
        
        let overload = null;
        let failures = [];
        for (let typeParameter of this._currentStatement.typeParameters) {
            if (!(typeParameter instanceof TypeVariable))
                continue;
            if (!typeParameter.protocol)
                continue;
            let signatures =
                typeParameter.protocol.protocolDecl.signaturesByNameWithTypeVariable(node.name, typeParameter);
            if (!signatures)
                continue;
            overload = resolveOverloadImpl(signatures, node.typeArguments, argumentTypes, node.returnType);
            if (overload.func)
                break;
            failures.push(...overload.failures);
            overload = null;
        }
        if (!overload) {
            overload = resolveOverloadImpl(
                node.possibleOverloads, node.typeArguments, argumentTypes, node.returnType);
            if (!overload.func) {
                failures.push(...overload.failures);
                let message = "Did not find function for call with ";
                if (node.typeArguments.length)
                    message += "type arguments <" + node.typeArguments + "> and ";
                message += "argument types (" + argumentTypes + ")";
                if (node.returnType)
                    message +=" and return type " + node.returnType;
                if (failures.length)
                    message += ", but considered:\n" + failures.join("\n")
                throw new WTypeError(node.origin.originString, message);
            }
        }
        for (let i = 0; i < argumentTypes.length; ++i) {
            let argumentType = argumentTypes[i];
            let parameterType = overload.func.parameters[i].type.substituteToUnification(
                overload.func.typeParameters, overload.unificationContext);
            let result = argumentType.equalsWithCommit(parameterType);
            if (!result)
                throw new Error("At " + node.origin.originString + " argument and parameter types not equal after type argument substitution: argument = " + argumentType + ", parameter = " + parameterType);
        }
        return node.resolve(overload);
    }
}

