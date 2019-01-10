/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

class NumberValue {
    constructor(value = 0)
    {
        this._value = value;
    }
    
    get value() { return this._value; }
    
    apply(state, parameters)
    {
        state.validate(parameters.length == 0, "Should not pass arguments to simple numeric variables");
        return this._value;
    }
    
    leftApply(state, parameters)
    {
        state.validate(parameters.length == 0, "Should not pass arguments to simple numeric variables");
        return this;
    }
    
    assign(value)
    {
        this._value = value;
    }
}

class NumberArray {
    constructor(dim = [11])
    {
        function allocateDim(index)
        {
            let result = new Array(dim[index]);
            if (index + 1 < dim.length) {
                for (let i = 0; i < dim[index]; ++i)
                    result[i] = allocateDim(index + 1);
            } else {
                for (let i = 0; i < dim[index]; ++i)
                    result[i] = new NumberValue();
            }
            return result;
        }
        
        this._array = allocateDim(0);
        this._dim = dim;
    }
    
    apply(state, parameters)
    {
        return this.leftApply(state, parameters).apply(state, []);
    }
    
    leftApply(state, parameters)
    {
        if (this._dim.length != parameters.length)
            state.abort("Expected " + this._dim.length + " arguments but " + parameters.length + " were passed.");
        let result = this._array;
        for (var i = 0; i < parameters.length; ++i) {
            let index = Math.floor(parameters[i]);
            if (!(index >= state.program.base) || !(index < result.length))
                state.abort("Index out of bounds: " + index);
            result = result[index];
        }
        return result;
    }
}

class NumberFunction {
    constructor(parameters, code)
    {
        this._parameters = parameters;
        this._code = code;
    }
    
    apply(state, parameters)
    {
        if (this._parameters.length != parameters.length)
            state.abort("Expected " + this._parameters.length + " arguments but " + parameters.length + " were passed");
        let oldValues = state.values;
        state.values = new Map(oldValues);
        for (let i = 0; i < parameters.length; ++i)
            state.values.set(this._parameters[i], parameters[i]);
        let result = this.code.evaluate(state);
        state.values = oldValues;
        return result;
    }
    
    leftApply(state, parameters)
    {
        state.abort("Cannot use a function as an lvalue");
    }
}

class NativeFunction {
    constructor(callback)
    {
        this._callback = callback;
    }
    
    apply(state, parameters)
    {
        if (this._callback.length != parameters.length)
            state.abort("Expected " + this._callback.length + " arguments but " + parameters.length + " were passed");
        return this._callback(...parameters);
    }
    
    leftApply(state, parameters)
    {
        state.abort("Cannot use a native function as an lvalue");
    }
}

