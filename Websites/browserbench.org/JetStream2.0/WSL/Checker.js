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
        this._vertexEntryPoints = new Set();
        this._fragmentEntryPoints = new Set();
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

    _checkShaderType(node)
    {
        // FIXME: Relax these checks once we have implemented support for textures and samplers.
        if (node.typeParameters.length != 0)
            throw new WTypeError(node.origin.originString, "Entry point " + node.name + " must not have type arguments.");
        let shaderFunc = node;
        switch (node.shaderType) {
        case "vertex":
            if (this._vertexEntryPoints.has(node.name))
                throw new WTypeError(node.origin.originString, "Duplicate vertex entry point name " + node.name);
            this._vertexEntryPoints.add(node.name);
            break;
        case "fragment":
            if (this._fragmentEntryPoints.has(node.name))
                throw new WTypeError(node.origin.originString, "Duplicate fragment entry point name " + node.name);
            this._fragmentEntryPoints.add(node.name);
            break;
        }
    }

    _checkOperatorOverload(func, resolveFuncs)
    {
        if (Lexer.textIsIdentifier(func.name))
            return; // Not operator!

        if (!func.name.startsWith("operator"))
            throw new Error("Bad operator overload name: " + func.name);
        
        let typeVariableTracker = new TypeVariableTracker();
        for (let parameterType of func.parameterTypes)
            parameterType.visit(typeVariableTracker);
        Node.visit(func.returnTypeForOverloadResolution, typeVariableTracker);
        for (let typeParameter of func.typeParameters) {
            if (!typeVariableTracker.set.has(typeParameter))
                throw new WTypeError(typeParameter.origin.originString, "Type parameter " + typeParameter + " to operator " + func.toDeclString() + " is not inferrable from value parameters");
        }
        
        let checkGetter = (kind) => {
            let numExpectedParameters = kind == "index" ? 2 : 1;
            if (func.parameters.length != numExpectedParameters)
                throw new WTypeError(func.origin.originString, "Incorrect number of parameters for " + func.name + " (expected " + numExpectedParameters + ", got " + func.parameters.length + ")");
            if (func.parameterTypes[0].unifyNode.isPtr)
                throw new WTypeError(func.origin.originString, "Cannot have getter for pointer type: " + func.parameterTypes[0]);
        };
        
        let checkSetter = (kind) => {
            let numExpectedParameters = kind == "index" ? 3 : 2;
            if (func.parameters.length != numExpectedParameters)
                throw new WTypeError(func.origin.originString, "Incorrect number of parameters for " + func.name + " (expected " + numExpectedParameters + ", got " + func.parameters.length + ")");
            if (func.parameterTypes[0].unifyNode.isPtr)
                throw new WTypeError(func.origin.originString, "Cannot have setter for pointer type: " + func.parameterTypes[0]);
            if (!func.returnType.equals(func.parameterTypes[0]))
                throw new WTypeError(func.origin.originString, "First parameter type and return type of setter must match (parameter was " + func.parameterTypes[0] + " but return was " + func.returnType + ")");
            let valueType = func.parameterTypes[numExpectedParameters - 1];
            let getterName = func.name.substr(0, func.name.length - 1);
            let getterFuncs = resolveFuncs(getterName);
            if (!getterFuncs)
                throw new WTypeError(func.origin.originString, "Every setter must have a matching getter, but did not find any function named " + getterName + " to match " + func.name);
            let argumentTypes = func.parameterTypes.slice(0, numExpectedParameters - 1);
            let overload = resolveOverloadImpl(getterFuncs, [], argumentTypes, null);
            if (!overload.func)
                throw new WTypeError(func.origin.originString, "Did not find function named " + func.name + " with arguments " + argumentTypes + (overload.failures.length ? "; tried:\n" + overload.failures.join("\n") : ""));
            let resultType = overload.func.returnType.substituteToUnification(
                overload.func.typeParameters, overload.unificationContext);
            if (!resultType.equals(valueType))
                throw new WTypeError(func.origin.originString, "Setter and getter must agree on value type (getter at " + overload.func.origin.originString + " says " + resultType + " while this setter says " + valueType + ")");
        };
        
        let checkAnder = (kind) => {
            let numExpectedParameters = kind == "index" ? 2 : 1;
            if (func.parameters.length != numExpectedParameters)
                throw new WTypeError(func.origin.originString, "Incorrect number of parameters for " + func.name + " (expected " + numExpectedParameters + ", got " + func.parameters.length + ")");
            if (!func.returnType.unifyNode.isPtr)
                throw new WTypeError(func.origin.originString, "Return type of ander is not a pointer: " + func.returnType);
            if (!func.parameterTypes[0].unifyNode.isRef)
                throw new WTypeError(func.origin.originString, "Parameter to ander is not a reference: " + func.parameterTypes[0]);
        };
        
        switch (func.name) {
        case "operator cast":
            break;
        case "operator++":
        case "operator--":
            if (func.parameters.length != 1)
                throw new WTypeError(func.origin.originString, "Incorrect number of parameters for " + func.name + " (expected 1, got " + func.parameters.length + ")");
            if (!func.parameterTypes[0].equals(func.returnType))
                throw new WTypeError(func.origin.originString, "Parameter type and return type must match for " + func.name + " (parameter is " + func.parameterTypes[0] + " while return is " + func.returnType + ")");
            break;
        case "operator+":
        case "operator-":
            if (func.parameters.length != 1 && func.parameters.length != 2)
                throw new WTypeError(func.origin.originString, "Incorrect number of parameters for " + func.name + " (expected 1 or 2, got " + func.parameters.length + ")");
            break;
        case "operator*":
        case "operator/":
        case "operator%":
        case "operator&":
        case "operator|":
        case "operator^":
        case "operator<<":
        case "operator>>":
            if (func.parameters.length != 2)
                throw new WTypeError(func.origin.originString, "Incorrect number of parameters for " + func.name + " (expected 2, got " + func.parameters.length + ")");
            break;
        case "operator~":
            if (func.parameters.length != 1)
                throw new WTypeError(func.origin.originString, "Incorrect number of parameters for " + func.name + " (expected 1, got " + func.parameters.length + ")");
            break;
        case "operator==":
        case "operator<":
        case "operator<=":
        case "operator>":
        case "operator>=":
            if (func.parameters.length != 2)
                throw new WTypeError(func.origin.originString, "Incorrect number of parameters for " + func.name + " (expected 2, got " + func.parameters.length + ")");
            if (!func.returnType.equals(this._program.intrinsics.bool))
                throw new WTypeError(func.origin.originString, "Return type of " + func.name + " must be bool but was " + func.returnType);
            break;
        case "operator[]":
            checkGetter("index");
            break;
        case "operator[]=":
            checkSetter("index");
            break;
        case "operator&[]":
            checkAnder("index");
            break;
        default:
            if (func.name.startsWith("operator.")) {
                if (func.name.endsWith("="))
                    checkSetter("dot");
                else
                    checkGetter("dot");
                break;
            }
            if (func.name.startsWith("operator&.")) {
                checkAnder("dot");
                break;
            }
            throw new Error("Parser accepted unrecognized operator: " + func.name);
        }
    }
    
    visitFuncDef(node)
    {
        if (node.shaderType)
            this._checkShaderType(node);
        this._checkOperatorOverload(node, name => this._program.functions.get(name));
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
            this._checkOperatorOverload(signature, name => node.signaturesByName(name));
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
        if (!(node.type instanceof StructType))
            node.type.visit(this);
        this._checkTypeArguments(node.origin, node.type.typeParameters, node.typeArguments);
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
        let lhsType = node.lhs.visit(this);
        if (!node.lhs.isLValue)
            throw new WTypeError(node.origin.originString, "LHS of assignment is not an LValue: " + node.lhs + node.lhs.notLValueReasonString);
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
        let lhsType = node.lValue.visit(this);
        if (!node.lValue.isLValue)
            throw new WTypeError(node.origin.originString, "LHS of read-modify-write is not an LValue: " + node.lValue + node.lValue.notLValueReasonString);
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
        let elementType = node.lValue.visit(this).unifyNode;
        if (!node.lValue.isLValue)
            throw new WTypeError(node.origin.originString, "Operand to & is not an LValue: " + node.lValue + node.lValue.notLValueReasonString);
        
        return new PtrType(node.origin, node.lValue.addressSpace, elementType);
    }
    
    visitMakeArrayRefExpression(node)
    {
        let elementType = node.lValue.visit(this).unifyNode;
        if (elementType.isPtr) {
            node.become(new ConvertPtrToArrayRefExpression(node.origin, node.lValue));
            return new ArrayRefType(node.origin, elementType.addressSpace, elementType.elementType);
        }
        
        if (!node.lValue.isLValue)
            throw new WTypeError(node.origin.originString, "Operand to @ is not an LValue: " + node.lValue + node.lValue.notLValueReasonString);
        
        if (elementType.isArray) {
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
                errorForGet.typeErrorMessage + "\n" +
                "and tried by-pointer:\n" +
                errorForAnd.typeErrorMessage);
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
        
        // OK, now we need to determine if we are an lvalue. We are an lvalue if we can be assigned to. We can
        // be assigned to if we have an ander or setter. But it's weirder than that. We also need the base to be
        // an lvalue, except unless the base is an array reference.
        if (!node.callForAnd && !node.callForSet) {
            node.isLValue = false;
            node.notLValueReason =
                "Have neither ander nor setter. Tried setter:\n" +
                node.errorForSet.typeErrorMessage + "\n" +
                "and tried ander:\n" +
                errorForAnd.typeErrorMessage;
        } else if (!node.base.isLValue && !baseType.isArrayRef) {
            node.isLValue = false;
            node.notLValueReason = "Base of property access is neither a lvalue nor an array reference";
        } else {
            node.isLValue = true;
            node.addressSpace = node.base.isLValue ? node.base.addressSpace : baseType.addressSpace;
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
            throw new WTypeError("Expression isn't a bool: " + expression);
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

    visitSwitchStatement(node)
    {
        let type = node.value.visit(this).commit();
        
        if (!type.unifyNode.isInt && !(type.unifyNode instanceof EnumType))
            throw new WTypeError(node.origin.originString, "Cannot switch on non-integer/non-enum type: " + type);

        node.type = type;
        
        let hasDefault = false;
        
        for (let switchCase of node.switchCases) {
            switchCase.body.visit(this);

            if (switchCase.isDefault) {
                hasDefault = true;
                continue;
            }
            
            if (!switchCase.value.isConstexpr)
                throw new WTypeError(switchCase.origin.originString, "Switch case not constexpr: " + switchCase.value);
            
            let caseType = switchCase.value.visit(this);
            if (!type.equalsWithCommit(caseType))
                throw new WTypeError(switchCase.origin.originString, "Switch case type does not match switch value type (case type is " + caseType + " but switch value type is " + type + ")");
        }
        
        for (let i = 0; i < node.switchCases.length; ++i) {
            let firstCase = node.switchCases[i];
            for (let j = i + 1; j < node.switchCases.length; ++j) {
                let secondCase = node.switchCases[j];
                
                if (firstCase.isDefault != secondCase.isDefault)
                    continue;
                
                if (firstCase.isDefault)
                    throw new WTypeError(secondCase.origin.originString, "Duplicate default case in switch statement");
                
                let valuesEqual = type.unifyNode.valuesEqual(
                    firstCase.value.unifyNode.valueForSelectedType,
                    secondCase.value.unifyNode.valueForSelectedType);
                if (valuesEqual)
                    throw new WTypeError(secondCase.origin.originString, "Duplicate case in switch statement for value " + firstCase.value.unifyNode.valueForSelectedType);
            }
        }
        
        if (!hasDefault) {
            let includedValues = new Set();
            for (let switchCase of node.switchCases)
                includedValues.add(switchCase.value.unifyNode.valueForSelectedType);
            
            for (let {value, name} of type.unifyNode.allValues()) {
                if (!includedValues.has(value))
                    throw new WTypeError(node.origin.originString, "Value not handled by switch statement: " + name);
            }
        }
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

