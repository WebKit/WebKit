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

const NotFunc = Symbol();

function isWildcardKind(kind)
{
    return kind == NotFunc;
}

class NameContext {
    constructor(delegate)
    {
        this._map = new Map();
        this._set = new Set();
        this._defined = null;
        this._currentStatement = null;
        this._delegate = delegate;
        this._intrinsics = null;
        this._program = null;
    }
    
    mapFor(kind)
    {
        switch (kind) {
        case NotFunc:
        case Value:
        case Type:
        case Protocol:
        case Func:
            return this._map;
        default:
            throw new Error("Bad kind: " + kind);
        }
    }
    
    add(thing)
    {
        if (!thing.name)
            return;
        
        if (thing.isNative) {
            if (!this._intrinsics)
                throw new Error("Native function in a scope that does not recognize intrinsics");
            this._intrinsics.add(thing);
        }
        
        if (thing.kind == Func) {
            this._set.add(thing);
            let array = this._map.get(thing.name);
            if (!array) {
                array = [];
                array.kind = Func;
                this._map.set(thing.name, array);
            }
            if (array.kind != Func)
                throw new WTypeError(thing.origin.originString, "Cannot reuse type name for function: " + thing.name);
            array.push(thing);
            return;
        }
        if (this._map.has(thing.name))
            throw new WTypeError(thing.origin.originString, "Duplicate name: " + thing.name);
        this._set.add(thing);
        this._map.set(thing.name, thing);
    }
    
    get(kind, name)
    {
        let result = this.mapFor(kind).get(name);
        if (!result && this._delegate)
            return this._delegate.get(kind, name);
        if (result && !isWildcardKind(kind) && result.kind != kind)
            return null;
        return result;
    }
    
    resolveFuncOverload(name, typeArguments, argumentTypes, returnType)
    {
        let functions = this.get(Func, name);
        if (!functions)
            return {failures: []};
        
        return resolveOverloadImpl(functions, typeArguments, argumentTypes, returnType);
    }
    
    get currentStatement()
    {
        if (this._currentStatement)
            return this._currentStatement;
        if (this._delegate)
            return this._delegate.currentStatement;
        return null;
    }
    
    handleDefining()
    {
        this._defined = new Set();
    }
    
    isDefined(thing)
    {
        if (this._set.has(thing)) {
            return !this._defined
                || this._defined.has(thing);
        }
        return this._delegate && this._delegate.isDefined(thing);
    }
    
    define(thing)
    {
        this._defined.add(thing);
    }
    
    defineAll()
    {
        this._defined = null;
    }
    
    doStatement(statement, callback)
    {
        this._currentStatement = statement;
        callback();
        this._currentStatement = null;
        this.define(statement);
    }
    
    recognizeIntrinsics()
    {
        this._intrinsics = new Intrinsics(this);
    }
    
    get intrinsics()
    {
        if (this._intrinsics)
            return this._intrinsics;
        if (this._delegate)
            return this._delegate.intrinsics;
        return null;
    }
    
    set program(value)
    {
        this._program = value;
    }
    
    get program()
    {
        if (this._program)
            return this._program;
        if (this._delegate)
            return this._delegate.program;
        return null;
    }
    
    *[Symbol.iterator]()
    {
        for (let value of this._map.values()) {
            if (value instanceof Array) {
                for (let func of value)
                    yield func;
                continue;
            }
            yield value;
        }
    }
}
