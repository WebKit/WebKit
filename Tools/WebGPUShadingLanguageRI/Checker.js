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
        let doStatement = statement => {
            this._currentStatement = statement;
            statement.visit(this);
        }
        
        for (let type of node.types.values())
            doStatement(type);
        for (let protocol of node.protocols.values())
            doStatement(protocol);
        for (let funcs of node.functions.values()) {
            for (let func of funcs) {
                this.visitFunc(func);
            }
        }
        for (let funcs of node.functions.values()) {
            for (let func of funcs)
                doStatement(func);
        }
    }
    
    visitFuncDef(node)
    {
        node.body.visit(this);
    }
    
    visitNativeFunc(node)
    {
    }
    
    visitProtocolDecl(node)
    {
        for (let signature of node.signatures) {
            let typeVariableTracker = new TypeVariableTracker();
            for (let parameterType of signature.parameterTypes)
                parameterType.visit(typeVariableTracker);
            Node.visit(signature.returnTypeForOverloadResolution, typeVariableTracker);
            for (let typeParameter of signature.typeParameters) {
                if (!typeVariableTracker.set.has(typeParameter))
                    throw WTypeError(typeParameter.origin.originString, "Type parameter to protocol signature not inferrable from value parameters");
            }
            if (!typeVariableTracker.set.has(node.typeVariable))
                throw new WTypeError(signature.origin.originString, "Protocol's type variable (" + node.name + ") not mentioned in signature: " + signature);
        }
    }
    
    visitEnumType(node)
    {
        node.baseType.visit(this);
        
        let baseType = node.baseType.unifyNode;
        
        if (!baseType.isInt)
            throw new WTypeError(node.origin.originString, "Base type of enum is not an integer: " + node.baseType);
        
        for (let member of node.members) {
            if (!member.value)
                continue;
            
            let memberType = member.value.visit(this);
            if (!baseType.equalsWithCommit(memberType))
                throw new WTypeError(member.origin.originString, "Type of enum member " + member.value.name + " does not patch enum base type (member type is " + memberType + ", enum base type is " + node.baseType + ")");
        }
        
        let nextValue = baseType.defaultValue;
        for (let member of node.members) {
            if (member.value) {
                nextValue = baseType.successorValue(member.value.unifyNode.valueForSelectedType);
                continue;
            }
            
            member.value = baseType.createLiteral(member.origin, nextValue);
            nextValue = baseType.successorValue(nextValue);
        }
        
        let memberArray = Array.from(node.members);
        for (let i = 0; i < memberArray.length; ++i) {
            let member = memberArray[i];
            for (let j = i + 1; j < memberArray.length; ++j) {
                let otherMember = memberArray[j];
                if (baseType.valuesEqual(member.value.unifyNode.valueForSelectedType, otherMember.value.unifyNode.valueForSelectedType))
                    throw new WTypeError(otherMember.origin.originString, "Duplicate enum member value (" + member.name + " has " + member.value + " while " + otherMember.name + " has " + otherMember.value + ")");
            }
        }
        
        let foundZero = false;
        for (let member of node.members) {
            if (baseType.valuesEqual(member.value.unifyNode.valueForSelectedType, baseType.defaultValue)) {
                foundZero = true;
                break;
            }
        }
        if (!foundZero)
            throw new WTypeError(node.origin.originString, "Enum does not have a member with the value zero");
    }
    
    _checkTypeArguments(origin, typeParameters, typeArguments)
    {
        for (let i = 0; i < typeParameters.length; ++i) {
            let argumentIsType = typeArguments[i] instanceof Type;
            let result = typeArguments[i].visit(this);
            if (argumentIsType) {
                let result = typeArguments[i].inherits(typeParameters[i].protocol);
                if (!result.result)
                    throw new WTypeError(origin.originString, "Type argument does not inherit protocol: " + result.reason);
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
        node.type.visit(this);
        this._checkTypeArguments(node.origin, node.type.typeParameters, node.typeArguments);
    }
    
    visitReferenceType(node)
    {
        node.elementType.visit(this);
        
        if (node.addressSpace == "thread")
            return;
        
        let instantiatedType = node.elementType.instantiatedType;
        if (!instantiatedType.isPrimitive)
            throw new WTypeError(node.origin.originString, "Illegal pointer to non-primitive type: " + node.elementType + " (instantiated to " + node.elementType.instantiatedType + ")");
    }
    
    visitArrayType(node)
    {
        node.elementType.visit(this);
        
        if (!node.numElements.isConstexpr)
            throw new WTypeError(node.origin.originString, "Array length must be constexpr");
        
        let type = node.numElements.visit(this);
        
        if (!type.equalsWithCommit(this._program.intrinsics.uint32))
            throw new WTypeError(node.origin.originString, "Array length must be a uint32");
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
    
    visitIdentityExpression(node)
    {
        return node.target.visit(this);
    }
    
    visitReadModifyWriteExpression(node)
    {
        if (!node.lValue.isLValue)
            throw new WTypeError(node.origin.originString, "LHS of read-modify-write is not an LValue: " + node.lValue);
        let lhsType = node.lValue.visit(this);
        node.oldValueVar.type = lhsType;
        node.newValueVar.type = lhsType;
        node.oldValueVar.visit(this);
        node.newValueVar.visit(this);
        let newValueType = node.newValueExp.visit(this);
        if (!lhsType.equalsWithCommit(newValueType))
            return new WTypeError(node.origin.originString, "Type mismatch in read-modify-write: " + lhsType + " versus " + newValueType);
        return node.resultExp.visit(this);
    }
    
    visitAnonymousVariable(node)
    {
        if (!node.type)
            throw new Error("Anonymous variable must know type before first appearance");
    }
    
    visitDereferenceExpression(node)
    {
        let type = node.ptr.visit(this).unifyNode;
        if (!type.isPtr)
            throw new WTypeError(node.origin.originString, "Type passed to dereference is not a pointer: " + type);
        node.type = type.elementType;
        node.addressSpace = type.addressSpace;
        if (!node.addressSpace)
            throw new Error("Null address space in type: " + type);
        return node.type;
    }
    
    visitMakePtrExpression(node)
    {
        if (!node.lValue.isLValue)
            throw new WTypeError(node.origin.originString, "Operand to & is not an LValue: " + node.lValue);
        
        let elementType = node.lValue.visit(this).unifyNode;
        
        return new PtrType(node.origin, node.lValue.addressSpace, elementType);
    }
    
    visitMakeArrayRefExpression(node)
    {
        let elementType = node.lValue.visit(this).unifyNode;
        if (elementType instanceof PtrType) {
            node.become(new ConvertPtrToArrayRefExpression(node.origin, node.lValue));
            return new ArrayRefType(node.origin, elementType.addressSpace, elementType.elementType);
        }
        
        if (!node.lValue.isLValue)
            throw new WTypeError(node.origin.originString, "Operand to @ is not an LValue: " + node.lValue);
        
        if (elementType instanceof ArrayRefType)
            throw new WTypeError(node.origin.originStrimg, "Operand to @ is an array reference: " + elementType);
        
        if (elementType instanceof ArrayType) {
            node.numElements = elementType.numElements;
            elementType = elementType.elementType;
        } else
            node.numElements = UintLiteral.withType(node.origin, 1, this._program.intrinsics.uint32);
            
        return new ArrayRefType(node.origin, node.lValue.addressSpace, elementType);
    }
    
    visitConvertToArrayRefExpression(node)
    {
        throw new Error("Should not exist yet.");
    }
    
    _finishVisitingPropertyAccess(node, baseType, extraArgs, extraArgTypes)
    {
        baseType = baseType.visit(new AutoWrapper())
        node.baseType = baseType;
        
        // Such a type must exist. This may throw if it doesn't.
        let typeForAnd = baseType.argumentTypeForAndOverload(node.origin);
        if (!typeForAnd)
            throw new Error("Cannot get typeForAnd");
        
        let errorForGet;
        let errorForAnd;
        
        try {
            let result = CallExpression.resolve(
                node.origin, node.possibleGetOverloads, this._currentStatement.typeParameters,
                node.getFuncName, [], [node.base, ...extraArgs], [baseType, ...extraArgTypes], null);
            node.callForGet = result.call;
            node.resultTypeForGet = result.resultType;
        } catch (e) {
            if (!(e instanceof WTypeError))
                throw e;
            errorForGet = e;
        }
        
        try {
            let baseForAnd = baseType.argumentForAndOverload(node.origin, node.base);
            
            let result = CallExpression.resolve(
                node.origin, node.possibleAndOverloads, this._currentStatement.typeParameters,
                node.andFuncName, [], [baseForAnd, ...extraArgs], [typeForAnd, ...extraArgTypes],
                null);
            node.callForAnd = result.call;
            node.resultTypeForAnd = result.resultType.unifyNode.returnTypeFromAndOverload(node.origin);
        } catch (e) {
            if (!(e instanceof WTypeError))
                throw e;
            errorForAnd = e;
        }
        
        if (!node.resultTypeForGet && !node.resultTypeForAnd) {
            throw new WTypeError(
                node.origin.originString,
                "Cannot resolve access; tried by-value:\n" +
                errorForGet.message + "\n" +
                "and tried by-pointer:\n" +
                errorForAnd.message);
        }
        
        if (node.resultTypeForGet && node.resultTypeForAnd
            && !node.resultTypeForGet.equals(node.resultTypeForAnd))
            throw new WTypeError(node.origin.originString, "Result type resolved by-value (" + node.resultTypeForGet + ") does not match result type resolved by-pointer (" + node.resultTypeForAnd + ")");
        
        try {
            let result = CallExpression.resolve(
                node.origin, node.possibleSetOverloads, this._currentStatement.typeParameters,
                node.setFuncName, [], [node.base, ...extraArgs, null], [baseType, ...extraArgTypes, node.resultType], null);
            node.callForSet = result.call;
            if (!result.resultType.equals(baseType))
                throw new WTypeError(node.origin.originString, "Result type of setter " + result.call.func + " is not the base type " + baseType);
        } catch (e) {
            if (!(e instanceof WTypeError))
                throw e;
            node.errorForSet = e;
        }
        
        return node.resultType;
    }
    
    visitDotExpression(node)
    {
        let structType = node.struct.visit(this).unifyNode;
        return this._finishVisitingPropertyAccess(node, structType, [], []);
    }
    
    visitIndexExpression(node)
    {
        let arrayType = node.array.visit(this).unifyNode;
        let indexType = node.index.visit(this);
        
        return this._finishVisitingPropertyAccess(node, arrayType, [node.index], [indexType]);
    }
    
    visitVariableRef(node)
    {
        if (!node.variable.type)
            throw new Error("Variable has no type: " + node.variable);
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
    
    visitGenericLiteral(node)
    {
        return node.type;
    }
    
    visitNullLiteral(node)
    {
        return node.type;
    }
    
    visitBoolLiteral(node)
    {
        return this._program.intrinsics.bool;
    }
    
    visitEnumLiteral(node)
    {
        return node.member.enumType;
    }

    _requireBool(expression)
    {
        let type = expression.visit(this);
        if (!type)
            throw new Error("Expression has no type, but should be bool: " + expression);
        if (!type.equals(this._program.intrinsics.bool))
            throw new WError("Expression isn't a bool: " + expression);
    }

    visitLogicalNot(node)
    {
        this._requireBool(node.operand);
        return this._program.intrinsics.bool;
    }

    visitLogicalExpression(node)
    {
        this._requireBool(node.left);
        this._requireBool(node.right);
        return this._program.intrinsics.bool;
    }

    visitIfStatement(node)
    {
        this._requireBool(node.conditional);
        node.body.visit(this);
        if (node.elseBody)
            node.elseBody.visit(this);
    }

    visitWhileLoop(node)
    {
        this._requireBool(node.conditional);
        node.body.visit(this);
    }

    visitDoWhileLoop(node)
    {
        node.body.visit(this);
        this._requireBool(node.conditional);
    }

    visitForLoop(node)
    {
        if (node.initialization)
            node.initialization.visit(this);
        if (node.condition)
            this._requireBool(node.condition);
        if (node.increment)
            node.increment.visit(this);
        node.body.visit(this);
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
        let typeArguments = node.typeArguments.map(typeArgument => typeArgument.visit(this));
        let argumentTypes = node.argumentList.map(argument => {
            let newArgument = argument.visit(this);
            if (!newArgument)
                throw new Error("visitor returned null for " + argument);
            return newArgument.visit(new AutoWrapper());
        });
        
        node.argumentTypes = argumentTypes;
        if (node.returnType)
            node.returnType.visit(this);

        let result = node.resolve(node.possibleOverloads, this._currentStatement.typeParameters, typeArguments);
        return result;
    }
}

