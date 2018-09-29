/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

class CallExpression extends Expression {
    constructor(origin, name, argumentList)
    {
        super(origin);
        this._name = name;
        this._argumentList = argumentList;
        this.func = null;
        this._isCast = false;
        this._returnType = null;
    }
    
    get name() { return this._name; }

    get argumentList() { return this._argumentList; }

    set argumentList(newValue)
    {
        this._argumentList = newValue;
    }

    get isCast() { return this._isCast; }
    get returnType() { return this._returnType; }
    
    static resolve(origin, possibleOverloads, name, argumentList, argumentTypes, returnType, program)
    {
        let call = new CallExpression(origin, name, argumentList);
        call.argumentTypes = argumentTypes.map(argument => argument.visit(new AutoWrapper()));
        call.possibleOverloads = possibleOverloads;
        if (returnType)
            call.setCastData(returnType);
        return {call, resultType: call.resolve(possibleOverloads, program)};
    }

    resolve(possibleOverloads, program)
    {
        let failures = [];
        let overload;
        if (possibleOverloads)
            overload = resolveOverloadImpl(possibleOverloads, this.argumentTypes, this.returnType);

        if (!overload || !overload.func) {
            if (!overload)
                overload = {};
            const func = this._resolveByInstantiation(program);
            if (func)
                overload.func = func;
        }

        if (!overload.func) {
            if (!overload.failures)
                overload.failures = [];
            failures.push(...overload.failures);
            let message = "Did not find function named " + this.name + " for call with ";
            message += "argument types (" + this.argumentTypes + ")";
            if (this.returnType)
                message +=" and return type " + this.returnType;
            if (failures.length)
                message += ", but considered:\n" + failures.join("\n")
            throw new WTypeError(this.origin.originString, message);
        }

        for (let i = 0; i < this.argumentTypes.length; ++i) {
            let argumentType = this.argumentTypes[i];
            let parameterType = overload.func.parameters[i].type;
            let result = argumentType.equalsWithCommit(parameterType);
            if (!result)
                throw new Error("At " + this.origin.originString + " argument and parameter types not equal after type argument substitution: argument = " + argumentType + ", parameter = " + parameterType);
        }
        return this.resolveToOverload(overload);
    }

    _resolveByInstantiation(program)
    {
        let func;
        if (this.name == "operator&[]")
            func = this._resolveWithOperatorAnderIndexer(program);
        else if (this.name == "operator.length")
            func = this._resolveWithOperatorLength(program);
        else if (this.name == "operator==" && this.argumentTypes.length == 2
            && (this.argumentTypes[0] instanceof NullType || this.argumentTypes[0] instanceof ReferenceType)
            && (this.argumentTypes[1] instanceof NullType || this.argumentTypes[1] instanceof ReferenceType)
            && this.argumentTypes[0].equals(this.argumentTypes[1]))
                func = this._resolveWithReferenceComparator(program);
        else
            return null;

        program.add(func);
        return func;
    }

    _resolveWithOperatorAnderIndexer(program)
    {
        let arrayRefType = this.argumentTypes[0];
        if (!arrayRefType.isArrayRef)
            throw new WTypeError(this.origin.originString, `Expected ${arrayRefType} to be an array ref type for operator&[]`);

        let indexType = this.argumentTypes[1];
        const addressSpace = arrayRefType.addressSpace;

        // The later checkLiteralTypes stage will verify that the literal can be represented as a uint.
        const uintType = TypeRef.wrap(program.intrinsics.uint);
        indexType.type = uintType;

        const elementType = this.argumentTypes[0].elementType;
        this.resultType = TypeRef.wrap(new PtrType(this.origin, addressSpace, TypeRef.wrap(elementType)))

        let arrayRefAccessor = new OperatorAnderIndexer(this.resultType.toString(), addressSpace);
        const func = new NativeFunc(this.origin, "operator&[]", this.resultType, [
            new FuncParameter(this.origin, null, arrayRefType),
            new FuncParameter(this.origin, null, uintType)
        ]);

        arrayRefAccessor.instantiateImplementation(func);

        return func;
    }

    _resolveWithOperatorLength(program)
    {
        this.resultType = TypeRef.wrap(program.intrinsics.uint);

        if (this.argumentTypes[0].isArray) {
            const arrayType = this.argumentTypes[0];
            const func = new NativeFunc(this.origin, "operator.length", this.resultType, [
                new FuncParameter(this.origin, null, arrayType)
            ]);
            func.implementation = (args) => EPtr.box(arrayType.numElementsValue);
            return func;
        } else if (this.argumentTypes[0].isArrayRef) {
            const arrayRefType = this.argumentTypes[0];
            const addressSpace = arrayRefType.addressSpace;
            const operatorLength = new OperatorArrayRefLength(arrayRefType.toString(), addressSpace);
            const func = new NativeFunc(this.origin, "operator.length", this.resultType, [
                new FuncParameter(this.origin, null, arrayRefType)
            ]);
            operatorLength.instantiateImplementation(func);
            return func;
        } else
            throw new WTypeError(this.origin.originString, `Expected ${this.argumentTypes[0]} to be array/array ref type for operator.length`);
    }

    _resolveWithReferenceComparator(program)
    {
        let argumentType = this.argumentTypes[0];
        if (argumentType instanceof NullType)
            argumentType = this.argumentTypes[1];
        if (argumentType instanceof NullType) {
            // We encountered "null == null".
            // The type isn't observable, so we can pick whatever we want.
            // FIXME: This can probably be generalized, using the "preferred type" infrastructure used by generic literals
            argumentType = new PtrType(this.origin, "thread", program.intrinsics.int);
        }
        this.resultType = TypeRef.wrap(program.intrinsics.bool);
        const func = new NativeFunc(this.origin, "operator==", this.resultType, [
            new FuncParameter(this.origin, null, argumentType),
            new FuncParameter(this.origin, null, argumentType)
        ]);
        func.implementation = ([lhs, rhs]) => {
            let left = lhs.loadValue();
            let right = rhs.loadValue();
            if (left && right)
                return EPtr.box(left.equals(right));
            return EPtr.box(left == right);
        };
        return func;
    }
    
    resolveToOverload(overload)
    {
        this.func = overload.func;
        let result = overload.func.returnType;
        if (!result)
            throw new Error("Null return type");
        result = result.visit(new AutoWrapper());
        this.resultType = result;
        return result;
    }
    
    setCastData(returnType)
    {
        this._returnType = returnType;
        this._isCast = true;
    }
    
    toString()
    {
        return (this.isCast ? "operator " + this.returnType : this.name) +
            "(" + this.argumentList + ")";
    }
}

