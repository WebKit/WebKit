'use strict';/*
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

const Basic = {};

Basic.NumberApply = function(state)
{
    // I'd call this arguments but we're in strict mode.
    let parameters = this.parameters.map(value => value.evaluate(state));
    
    return state.getValue(this.name, parameters.length).apply(state, parameters);
};

Basic.Variable = function(state)
{
    let parameters = this.parameters.map(value => value.evaluate(state));
    
    return state.getValue(this.name, parameters.length).leftApply(state, parameters);
}

Basic.Const = function(state)
{
    return this.value;
}

Basic.NumberPow = function(state)
{
    return Math.pow(this.left.evaluate(state), this.right.evaluate(state));
}

Basic.NumberMul = function(state)
{
    return this.left.evaluate(state) * this.right.evaluate(state);
}

Basic.NumberDiv = function(state)
{
    return this.left.evaluate(state) / this.right.evaluate(state);
}

Basic.NumberNeg = function(state)
{
    return -this.term.evaluate(state);
}

Basic.NumberAdd = function(state)
{
    return this.left.evaluate(state) + this.right.evaluate(state);
}

Basic.NumberSub = function(state)
{
    return this.left.evaluate(state) - this.right.evaluate(state);
}

Basic.StringVar = function(state)
{
    let value = state.stringValues.get(this.name);
    if (value == null)
        state.abort("Could not find string variable " + this.name);
    return value;
}

Basic.Equals = function(state)
{
    return this.left.evaluate(state) == this.right.evaluate(state);
}

Basic.NotEquals = function(state)
{
    return this.left.evaluate(state) != this.right.evaluate(state);
}

Basic.LessThan = function(state)
{
    return this.left.evaluate(state) < this.right.evaluate(state);
}

Basic.GreaterThan = function(state)
{
    return this.left.evaluate(state) > this.right.evaluate(state);
}

Basic.LessEqual = function(state)
{
    return this.left.evaluate(state) <= this.right.evaluate(state);
}

Basic.GreaterEqual = function(state)
{
    return this.left.evaluate(state) >= this.right.evaluate(state);
}

Basic.GoTo = function*(state)
{
    state.nextLineNumber = this.target;
}

Basic.GoSub = function*(state)
{
    state.subStack.push(state.nextLineNumber);
    state.nextLineNumber = this.target;
}

Basic.Def = function*(state)
{
    state.validate(!state.values.has(this.name), "Cannot redefine function");
    state.values.set(this.name, new NumberFunction(this.parameters, this.expression));
}

Basic.Let = function*(state)
{
    this.variable.evaluate(state).assign(this.expression.evaluate(state));
}

Basic.If = function*(state)
{
    if (this.condition.evaluate(state))
        state.nextLineNumber = this.target;
}

Basic.Return = function*(state)
{
    this.validate(state.subStack.length, "Not in a subroutine");
    this.nextLineNumber = state.subStack.pop();
}

Basic.Stop = function*(state)
{
    state.nextLineNumber = null;
}

Basic.On = function*(state)
{
    let index = this.expression.evaluate(state);
    if (!(index >= 1) || !(index <= this.targets.length))
        state.abort("Index out of bounds: " + index);
    this.nextLineNumber = this.targets[Math.floor(index)];
}

Basic.For = function*(state)
{
    let sideState = state.getSideState(this);
    sideState.variable = state.getValue(this.variable, 0).leftApply(state, []);
    sideState.initialValue = this.initial.evaluate(state);
    sideState.limitValue = this.limit.evaluate(state);
    sideState.stepValue = this.step.evaluate(state);
    sideState.variable.assign(sideState.initialValue);
    sideState.shouldStop = function() {
        return (sideState.variable.value - sideState.limitValue) * Math.sign(sideState.stepValue) > 0;
    };
    if (sideState.shouldStop())
        this.nextLineNumber = this.target.lineNumber + 1;
}

Basic.Next = function*(state)
{
    let sideState = state.getSideState(this.target);
    sideState.variable.assign(sideState.variable.value + sideState.stepValue);
    if (sideState.shouldStop())
        return;
    state.nextLineNumber = this.target.lineNumber + 1;
}

Basic.Next.isBlockEnd = true;

Basic.Print = function*(state)
{
    let string = "";
    for (let item of this.items) {
        switch (item.kind) {
        case "comma":
            while (string.length % 14)
                string += " ";
            break;
        case "tab": {
            let value = item.value.evaluate(state);
            value = Math.max(Math.round(value), 1);
            while (string.length % value)
                string += " ";
            break;
        }
        case "string":
        case "number":
            string += item.value.evaluate(state);
            break;
        default:
            throw new Error("Bad item kind: " + item.kind);
        }
    }
    
    yield {kind: "output", string};
}

Basic.Input = function*(state)
{
    let results = yield {kind: "input", numItems: this.items.length};
    state.validate(results != null && results.length == this.items.length, "Input did not get the right number of items");
    for (let i = 0; i < results.length; ++i)
        this.items[i].evaluate(state).assign(results[i]);
}

Basic.Read = function*(state)
{
    for (let item of this.items) {
        state.validate(state.dataIndex < state.program.data.length, "Attempting to read past the end of data");
        item.assign(state.program.data[state.dataIndex++]);
    }
}

Basic.Restore = function*(state)
{
    state.dataIndex = 0;
}

Basic.Dim = function*(state)
{
    for (let item of this.items) {
        state.validate(!state.values.has(item.name), "Variable " + item.name + " already exists");
        state.validate(item.bounds.length, "Dim statement is for arrays");
        state.values.set(item.name, new NumberArray(item.bounds.map(bound => bound + 1)));
    }
}

Basic.Randomize = function*(state)
{
    state.rng = createRNGWithRandomSeed();
}

Basic.End = function*(state)
{
    state.nextLineNumber = null;
}

Basic.End.isBlockEnd = true;

Basic.Program = function* programGenerator(state)
{
    state.validate(state.program == this, "State must match program");
    let maxLineNumber = Math.max(...this.statements.keys());
    while (state.nextLineNumber != null) {
        state.validate(state.nextLineNumber <= maxLineNumber, "Went out of bounds of the program");
        let statement = this.statements.get(state.nextLineNumber++);
        if (statement == null || statement.process == null)
            continue;
        state.statement = statement;
        yield* statement.process(state);
    }
}

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

function prepare(string)
{
    let program = parse(lex(string)).program();
    return program.process(new State(program));
}

function simulate(program, inputs = [])
{
    let result = "";
    let args = [];
    for (;;) {
        let next = program.next(...args);
        args = [];
        if (next.done)
            break;
        if (next.value.kind == "output") {
            result += next.value.string + "\n";
            continue;
        }
        if (next.value.kind == "input") {
            args = inputs.splice(0, next.value.numItems);
            continue;
        }
    }
    return result;
}

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

class CaselessMap {
    constructor(otherMap)
    {
        if (otherMap == null)
            this._map = new Map();
        else
            this._map = new Map(otherMap._map);
    }
    
    set(key, value)
    {
        this._map.set(key.toLowerCase(), value);
    }
    
    has(key)
    {
        return this._map.has(key.toLowerCase());
    }
    
    get(key)
    {
        return this._map.get(key.toLowerCase());
    }

    [Symbol.iterator]()
    {
        return this._map[Symbol.iterator]();
    }
}

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

// Loosely based on ECMA 55 sections 4-8, but loosened to allow for modern conventions, like
// multi-character variable names. But this doesn't go too far - in particular, this doesn't do
// unicode, because that would require more thought.
function* lex(string)
{
    let sourceLineNumber = 0;
    for (let line of string.split("\n")) {
        ++sourceLineNumber;
        
        function consumeWhitespace()
        {
            if (/^\s+/.test(line))
                line = RegExp.rightContext;
        }
   
        function consume(kind)
        {
            line = RegExp.rightContext;
            return {kind, string: RegExp.lastMatch, sourceLineNumber, userLineNumber};
        }
        
        const isIdentifier = /^[a-z_]([a-z0-9_]*)/i;
        const isNumber = /^(([0-9]+(\.([0-9]*))?)|(\.[0-9]+)(e([+-]?)([0-9]+))?)/i;
        const isString = /^\"([^\"]|(\"\"))*\"/;
        const isKeyword = /^((base)|(data)|(def)|(dim)|(end)|(for)|(go)|(gosub)|(goto)|(if)|(input)|(let)|(next)|(on)|(option)|(print)|(randomize)|(read)|(restore)|(return)|(step)|(stop)|(sub)|(then)|(to))/i;
        const isOperator = /^(-|\+|\*|\/|\^|\(|\)|(<[>=]?)|(>=?)|=|,|\$|;)/;
        const isRem = /^rem\s.*/;
        
        consumeWhitespace();
        
        if (!/^[0-9]+/.test(line))
            throw new Error("At line " + sourceLineNumber + ": Expect line number: " + line);
        let userLineNumber = +RegExp.lastMatch;
        line = RegExp.rightContext;
        yield {kind: "userLineNumber", string: RegExp.lastMatch, sourceLineNumber, userLineNumber};
        
        consumeWhitespace();
        
        while (line.length) {
            if (isKeyword.test(line))
                yield consume("keyword");
            else if (isIdentifier.test(line))
                yield consume("identifier");
            else if (isNumber.test(line)) {
                let token = consume("number");
                token.value = +token.string;
                yield token;
            } else if (isString.test(line)) {
                let token = consume("string");
                token.value = "";
                for (let i = 1; i < token.string.length - 1; ++i) {
                    let char = token.string.charAt(i);
                    if (char == "\"")
                        i++;
                    token.value += char;
                }
                yield token;
            } else if (isOperator.test(line))
                yield consume("operator");
            else if (isRem.test(line))
                yield consume("remark");
            else
                throw new Error("At line " + sourceLineNumber + ": Cannot lex token: " + line);
            consumeWhitespace();
        }
        
        // Note: this is necessary for the parser, which may look-ahead without checking if we're
        // done. Fortunately, it won't look-ahead past a newLine.
        yield {kind: "newLine", string:"\n", sourceLineNumber, userLineNumber};
    }
}
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

function parse(tokenizer)
{
    let program;
    
    let pushBackBuffer = [];
    
    function nextToken()
    {
        if (pushBackBuffer.length)
            return pushBackBuffer.pop();
        let result = tokenizer.next();
        if (result.done)
            return {kind: "endOfFile", string: "<end of file>"};
        return result.value;
    }
    
    function pushToken(token)
    {
        pushBackBuffer.push(token);
    }
    
    function peekToken()
    {
        let result = nextToken();
        pushToken(result);
        return result;
    }
    
    function consumeKind(kind)
    {
        let token = nextToken();
        if (token.kind != kind) {
            throw new Error("At " + token.sourceLineNumber + ": expected " + kind + " but got: " + token.string);
        }
        return token;
    }
    
    function consumeToken(string)
    {
        let token = nextToken();
        if (token.string.toLowerCase() != string.toLowerCase())
            throw new Error("At " + token.sourceLineNumber + ": expected " + string + " but got: " + token.string);
        return token;
    }
    
    function parseVariable()
    {
        let name = consumeKind("identifier").string;
        let result = {evaluate: Basic.Variable, name, parameters: []};
        if (peekToken().string == "(") {
            do {
                nextToken();
                result.parameters.push(parseNumericExpression());
            } while (peekToken().string == ",");
            consumeToken(")");
        }
        return result;
    }
    
    function parseNumericExpression()
    {
        function parsePrimary()
        {
            let token = nextToken();
            switch (token.kind) {
            case "identifier": {
                let result = {evaluate: Basic.NumberApply, name: token.string, parameters: []};
                if (peekToken().string == "(") {
                    do {
                        nextToken();
                        result.parameters.push(parseNumericExpression());
                    } while (peekToken().string == ",");
                    consumeToken(")");
                }
                return result;
            }
                
            case "number":
                return {evaluate: Basic.Const, value: token.value};
                
            case "operator":
                switch (token.string) {
                case "(": {
                    let result = parseNumericExpression();
                    consumeToken(")");
                    return result;
                } }
                break;
            }
            throw new Error("At " + token.sourceLineNumber + ": expected identifier, number, or (, but got: " + token.string);
        }
        
        function parseFactor()
        {
            let primary = parsePrimary();
            
            let ok = true;
            while (ok) {
                switch (peekToken().string) {
                case "^":
                    nextToken();
                    primary = {evaluate: Basic.NumberPow, left: primary, right: parsePrimary()};
                    break;
                default:
                    ok = false;
                    break;
                }
            }
            
            return primary;
        }
        
        function parseTerm()
        {
            let factor = parseFactor();
            
            let ok = true;
            while (ok) {
                switch (peekToken().string) {
                case "*":
                    nextToken();
                    factor = {evaluate: Basic.NumberMul, left: factor, right: parseFactor()};
                    break;
                case "/":
                    nextToken();
                    factor = {evaluate: Basic.NumberDiv, left: factor, right: parseFactor()};
                    break;
                default:
                    ok = false;
                    break;
                }
            }
            
            return factor;
        }
        
        // Only the leading term in Basic can have a sign.
        let negate = false;
        switch (peekToken().string) {
        case "+":
            nextToken();
            break;
        case "-":
            negate = true;
            nextToken()
            break;
        }
        
        let term = parseTerm();
        if (negate)
            term = {evaluate: Basic.NumberNeg, term: term};
        
        let ok = true;
        while (ok) {
            switch (peekToken().string) {
            case "+":
                nextToken();
                term = {evaluate: Basic.NumberAdd, left: term, right: parseTerm()};
                break;
            case "-":
                nextToken();
                term = {evaluate: Basic.NumberSub, left: term, right: parseTerm()};
                break;
            default:
                ok = false;
                break;
            }
        }
        
        return term;
    }
    
    function parseConstant()
    {
        switch (peekToken().string) {
        case "+":
            nextToken();
            return consumeKind("number").value;
        case "-":
            nextToken();
            return -consumeKind("number").value;
        default:
            if (isStringExpression())
                return consumeKind("string").value;
            return consumeKind("number").value;
        }
    }
    
    function parseStringExpression()
    {
        let token = nextToken();
        switch (token.kind) {
        case "string":
            return {evaluate: Basic.Const, value: token.value};
        case "identifier":
            consumeToken("$");
            return {evaluate: Basic.StringVar, name: token.string};
        default:
            throw new Error("At " + token.sourceLineNumber + ": expected string expression but got " + token.string);
        }
    }
    
    function isStringExpression()
    {
        // A string expression must start with a string variable or a string constant.
        let token = nextToken();
        if (token.kind == "string") {
            pushToken(token);
            return true;
        }
        if (token.kind == "identifier") {
            let result = peekToken().string == "$";
            pushToken(token);
            return result;
        }
        pushToken(token);
        return false;
    }
    
    function parseRelationalExpression()
    {
        if (isStringExpression()) {
            let left = parseStringExpression();
            let operator = nextToken();
            let evaluate;
            switch (operator.string) {
            case "=":
                evaluate = Basic.Equals;
                break;
            case "<>":
                evaluate = Basic.NotEquals;
                break;
            default:
                throw new Error("At " + operator.sourceLineNumber + ": expected a string comparison operator but got: " + operator.string);
            }
            return {evaluate, left, right: parseStringExpression()};
        }
        
        let left = parseNumericExpression();
        let operator = nextToken();
        let evaluate;
        switch (operator.string) {
        case "=":
            evaluate = Basic.Equals;
            break;
        case "<>":
            evaluate = Basic.NotEquals;
            break;
        case "<":
            evaluate = Basic.LessThan;
            break;
        case ">":
            evaluate = Basic.GreaterThan;
            break;
        case "<=":
            evaluate = Basic.LessEqual;
            break;
        case ">=":
            evaluate = Basic.GreaterEqual;
            break;
        default:
            throw new Error("At " + operator.sourceLineNumber + ": expected a numeric comparison operator but got: " + operator.string);
        }
        return {evaluate, left, right: parseNumericExpression()};
    }
    
    function parseNonNegativeInteger()
    {
        let token = nextToken();
        if (!/^[0-9]+$/.test(token.string))
            throw new Error("At ", token.sourceLineNumber + ": expected a line number but got: " + token.string);
        return token.value;
    }
    
    function parseGoToStatement()
    {
        statement.kind = Basic.GoTo;
        statement.target = parseNonNegativeInteger();
    }
    
    function parseGoSubStatement()
    {
        statement.kind = Basic.GoSub;
        statement.target = parseNonNegativeInteger();
    }
    
    function parseStatement()
    {
        let statement = {};
        statement.lineNumber = consumeKind("userLineNumber").userLineNumber;
        program.statements.set(statement.lineNumber, statement);
        
        let command = nextToken();
        statement.sourceLineNumber = command.sourceLineNumber;
        switch (command.kind) {
        case "keyword":
            switch (command.string.toLowerCase()) {
            case "def":
                statement.process = Basic.Def;
                statement.name = consumeKind("identifier");
                statement.parameters = [];
                if (peekToken().string == "(") {
                    do {
                        nextToken();
                        statement.parameters.push(consumeKind("identifier"));
                    } while (peekToken().string == ",");
                }
                statement.expression = parseNumericExpression();
                break;
            case "let":
                statement.process = Basic.Let;
                statement.variable = parseVariable();
                consumeToken("=");
                if (statement.process == Basic.Let)
                    statement.expression = parseNumericExpression();
                else
                    statement.expression = parseStringExpression();
                break;
            case "go": {
                let next = nextToken();
                if (next.string == "to")
                    parseGoToStatement();
                else if (next.string == "sub")
                    parseGoSubStatement();
                else
                    throw new Error("At " + next.sourceLineNumber + ": expected to or sub but got: " + next.string);
                break;
            }
            case "goto":
                parseGoToStatement();
                break;
            case "gosub":
                parseGoSubStatement();
                break;
            case "if":
                statement.process = Basic.If;
                statement.condition = parseRelationalExpression();
                consumeToken("then");
                statement.target = parseNonNegativeInteger();
                break;
            case "return":
                statement.process = Basic.Return;
                break;
            case "stop":
                statement.process = Basic.Stop;
                break;
            case "on":
                statement.process = Basic.On;
                statement.expression = parseNumericExpression();
                if (peekToken().string == "go") {
                    consumeToken("go");
                    consumeToken("to");
                } else
                    consumeToken("goto");
                statement.targets = [];
                for (;;) {
                    statement.targets.push(parseNonNegativeInteger());
                    if (peekToken().string != ",")
                        break;
                    nextToken();
                }
                break;
            case "for":
                statement.process = Basic.For;
                statement.variable = consumeKind("identifier").string;
                consumeToken("=");
                statement.initial = parseNumericExpression();
                consumeToken("to");
                statement.limit = parseNumericExpression();
                if (peekToken().string == "step") {
                    nextToken();
                    statement.step = parseNumericExpression();
                } else
                    statement.step = {evaluate: Basic.Const, value: 1};
                consumeKind("newLine");
                let lastStatement = parseStatements();
                if (lastStatement.process != Basic.Next)
                    throw new Error("At " + lastStatement.sourceLineNumber + ": expected next statement");
                if (lastStatement.variable != statement.variable)
                    throw new Error("At " + lastStatement.sourceLineNumber + ": expected next for " + statement.variable + " but got " + lastStatement.variable);
                lastStatement.target = statement;
                statement.target = lastStatement;
                return statement;
            case "next":
                statement.process = Basic.Next;
                statement.variable = consumeKind("identifier").string;
                break;
            case "print": {
                statement.process = Basic.Print;
                statement.items = [];
                let ok = true;
                while (ok) {
                    switch (peekToken().string) {
                    case ",":
                        nextToken();
                        statement.items.push({kind: "comma"});
                        break;
                    case ";":
                        nextToken();
                        break;
                    case "tab":
                        nextToken();
                        consumeToken("(");
                        statement.items.push({kind: "tab", value: parseNumericExpression()});
                        break;
                    case "\n":
                        ok = false;
                        break;
                    default:
                        if (isStringExpression()) {
                            statement.items.push({kind: "string", value: parseStringExpression()});
                            break;
                        }
                        statement.items.push({kind: "number", value: parseNumericExpression()});
                        break;
                    }
                }
                break;
            }
            case "input":
                statement.process = Basic.Input;
                statement.items = [];
                for (;;) {
                    stament.items.push(parseVariable());
                    if (peekToken().string != ",")
                        break;
                    nextToken();
                }
                break;
            case "read":
                statement.process = Basic.Read;
                statement.items = [];
                for (;;) {
                    stament.items.push(parseVariable());
                    if (peekToken().string != ",")
                        break;
                    nextToken();
                }
                break;
            case "restore":
                statement.process = Basic.Restore;
                break;
            case "data":
                for (;;) {
                    program.data.push(parseConstant());
                    if (peekToken().string != ",")
                        break;
                    nextToken();
                }
                break;
            case "dim":
                statement.process = Basic.Dim;
                statement.items = [];
                for (;;) {
                    let name = consumeKind("identifier").string;
                    consumeToken("(");
                    let bounds = [];
                    bounds.push(parseNonNegativeInteger());
                    if (peekToken().string == ",") {
                        nextToken();
                        bounds.push(parseNonNegativeInteger());
                    }
                    consumeToken(")");
                    statement.items.push({name, bounds});
                    
                    if (peekToken().string != ",")
                        break;
                    consumeToken(",");
                }
                break;
            case "option": {
                consumeToken("base");
                let base = parseNonNegativeInteger();
                if (base != 0 && base != 1)
                    throw new Error("At " + command.sourceLineNumber + ": unexpected base: " + base);
                program.base = base;
                break;
            }
            case "randomize":
                statement.process = Basic.Randomize;
                break;
            case "end":
                statement.process = Basic.End;
                break;
            default:
                throw new Error("At " + command.sourceLineNumber + ": unexpected command but got: " + command.string);
            }
            break;
        case "remark":
            // Just ignore it.
            break;
        default:
            throw new Error("At " + command.sourceLineNumber + ": expected command but got: " + command.string + " (of kind " + command.kind + ")");
        }
        
        consumeKind("newLine");
        return statement;
    }
    
    function parseStatements()
    {
        let statement;
        do {
            statement = parseStatement();
        } while (!statement.process || !statement.process.isBlockEnd);
        return statement;
    }
    
    return {
        program()
        {
            program = {
                process: Basic.Program,
                statements: new Map(),
                data: [],
                base: 0
            };
            let lastStatement = parseStatements(program.statements);
            if (lastStatement.process != Basic.End)
                throw new Error("At " + lastStatement.sourceLineNumber + ": expected end");
            
            return program;
        },
        
        statement(program_)
        {
            program = program_;
            return parseStatement();
        }
    };
}

// Copyright 2013 the V8 project authors. All rights reserved.
// Copyright (C) 2015-2016 Apple Inc. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"use strict";

// This is based on Octane's RNG.
function createRNG(seed)
{
    return function() {
        // Robert Jenkins' 32 bit integer hash function.
        seed = ((seed + 0x7ed55d16) + (seed << 12))  & 0xffffffff;
        seed = ((seed ^ 0xc761c23c) ^ (seed >>> 19)) & 0xffffffff;
        seed = ((seed + 0x165667b1) + (seed << 5))   & 0xffffffff;
        seed = ((seed + 0xd3a2646c) ^ (seed << 9))   & 0xffffffff;
        seed = ((seed + 0xfd7046c5) + (seed << 3))   & 0xffffffff;
        seed = ((seed ^ 0xb55a4f09) ^ (seed >>> 16)) & 0xffffffff;
        return (seed & 0xfffffff) / 0x10000000;
    };
}

function createRNGWithFixedSeed()
{
    // This uses Octane's initial seed.
    return createRNG(49734321);
}

function createRNGWithRandomSeed()
{
    return createRNG((Math.random() * 4294967296) | 0);
}
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

class State {
    constructor(program)
    {
        this.values = new CaselessMap();
        this.stringValues = new CaselessMap();
        this.sideState = new WeakMap();
        this.statement = null;
        this.nextLineNumber = 0;
        this.subStack = [];
        this.dataIndex = 0;
        this.program = program;
        this.rng = createRNGWithFixedSeed();
        
        let addNative = (name, callback) => {
            this.values.set(name, new NativeFunction(callback));
        };
        
        addNative("abs", x => Math.abs(x));
        addNative("atn", x => Math.atan(x));
        addNative("cos", x => Math.cos(x));
        addNative("exp", x => Math.exp(x));
        addNative("int", x => Math.floor(x));
        addNative("log", x => Math.log(x));
        addNative("rnd", () => this.rng());
        addNative("sgn", x => Math.sign(x));
        addNative("sin", x => Math.sin(x));
        addNative("sqr", x => Math.sqrt(x));
        addNative("tan", x => Math.tan(x));
    }
    
    getValue(name, numParameters)
    {
        if (this.values.has(name))
            return this.values.get(name);

        let result;
        if (numParameters == 0)
            result = new NumberValue();
        else {
            let dim = [];
            while (numParameters--)
                dim.push(11);
            result = new NumberArray(dim);
        }
        this.values.set(name, result);
        return result;
    }
    
    getSideState(key)
    {
        if (!this.sideState.has(key)) {
            let result = {};
            this.sideState.set(key, result);
            return result;
        }
        return this.sideState.get(key);
    }
    
    abort(text)
    {
        if (!this.statement)
            throw new Error("At beginning of execution: " + text);
        throw new Error("At " + this.statement.sourceLineNumber + ": " + text);
    }
    
    validate(predicate, text)
    {
        if (!predicate)
            this.abort(text);
    }
}

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

let currentTime;
if (this.performance && performance.now)
    currentTime = function() { return performance.now() };
else if (this.preciseTime)
    currentTime = function() { return preciseTime() * 1000; };
else
    currentTime = function() { return +new Date(); };

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

class BasicBenchmark {
    constructor(verbose = 0)
    {
        this._verbose = verbose;
    }
    
    runIteration()
    {
        function expect(program, expected, ...inputs)
        {
            let result = simulate(prepare(program, inputs));
            if (result != expected)
                throw new Error("Program " + JSON.stringify(program) + " with inputs " + JSON.stringify(inputs) + " produced " + JSON.stringify(result) + " but we expected " + JSON.stringify(expected));
        }
        
        expect("10 print \"hello, world!\"\n20 end", "hello, world!\n");
        expect("10 let x = 0\n20 let x = x + 1\n30 print x\n40 if x < 10 then 20\n50 end", "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n");
        expect("10 print int(rnd * 100)\n20 end\n", "98\n");
        expect("10 let value = int(rnd * 2000)\n20 print value\n30 if value <> 100 then 10\n40 end", "1974\n697\n1126\n1998\n1658\n264\n1650\n1677\n226\n117\n492\n861\n877\n1969\n38\n1039\n197\n1261\n1102\n1522\n916\n1683\n1943\n1835\n476\n1898\n939\n176\n966\n908\n474\n614\n1326\n564\n1916\n728\n524\n162\n1303\n758\n832\n1279\n1856\n1876\n982\n6\n1613\n1781\n681\n1238\n494\n1583\n1953\n788\n1026\n347\n1116\n1465\n514\n583\n463\n1970\n1573\n412\n1256\n1453\n838\n1538\n1984\n1598\n209\n411\n1700\n546\n861\n91\n132\n884\n378\n693\n11\n433\n1719\n860\n164\n472\n231\n1786\n806\n811\n106\n1697\n118\n980\n890\n1199\n227\n1667\n1933\n1903\n1390\n1595\n923\n1746\n39\n1361\n117\n1297\n923\n901\n1180\n818\n1444\n269\n933\n327\n1744\n1082\n1527\n1260\n622\n528\n318\n856\n296\n1796\n1574\n585\n1871\n111\n827\n1725\n1320\n1868\n1695\n1914\n216\n63\n1847\n156\n671\n893\n127\n1867\n811\n279\n913\n310\n814\n907\n1363\n1624\n1670\n478\n714\n436\n355\n1484\n1628\n1208\n800\n611\n917\n829\n830\n273\n1791\n340\n214\n992\n1444\n442\n1555\n144\n1194\n282\n180\n1228\n1251\n1883\n678\n1555\n347\n72\n1661\n1828\n1090\n1183\n957\n1685\n930\n475\n103\n759\n1725\n1902\n1662\n1587\n61\n614\n863\n1418\n321\n1050\n505\n1622\n1425\n803\n589\n1511\n1098\n1051\n1554\n1898\n27\n747\n813\n1544\n332\n728\n1363\n771\n759\n1145\n1098\n1991\n385\n230\n520\n1369\n1840\n1285\n1562\n1845\n102\n760\n1874\n748\n361\n575\n277\n1661\n1764\n1117\n332\n757\n1766\n1722\n143\n474\n1507\n1294\n1180\n1578\n904\n845\n321\n496\n1911\n1784\n1116\n938\n1591\n1403\n1374\n533\n1085\n452\n708\n1096\n1634\n522\n564\n1397\n1357\n980\n978\n1760\n1088\n1361\n1184\n314\n1242\n217\n133\n1187\n1723\n646\n605\n591\n46\n135\n1420\n1821\n1147\n1211\n61\n244\n1307\n1551\n449\n1122\n1336\n140\n880\n22\n1155\n1326\n590\n1499\n1376\n112\n1771\n1897\n1071\n938\n1685\n1963\n1203\n1296\n804\n1275\n453\n1387\n482\n1262\n1883\n1381\n418\n1417\n1222\n1208\n1263\n632\n450\n1422\n1285\n1408\n644\n665\n275\n363\n1012\n165\n354\n80\n609\n291\n1661\n1724\n117\n407\n59\n906\n1224\n136\n855\n1275\n1468\n482\n1537\n1283\n1784\n1568\n1832\n452\n867\n1546\n1467\n800\n45\n1225\n1890\n465\n1372\n47\n1608\n193\n1345\n1847\n1059\n1788\n518\n52\n1052\n1003\n1210\n1135\n1433\n519\n1558\n39\n1249\n1017\n39\n1713\n1449\n1245\n1354\n82\n1140\n916\n1595\n838\n607\n389\n1270\n821\n247\n1692\n1305\n1211\n1960\n429\n1703\n1635\n575\n1618\n1490\n1495\n682\n1256\n964\n420\n1520\n1429\n1997\n396\n382\n856\n1182\n296\n1295\n298\n1892\n990\n711\n934\n1939\n1339\n682\n1631\n1533\n742\n1520\n1281\n1332\n1042\n656\n1576\n1253\n1608\n375\n169\n14\n414\n1586\n1562\n1508\n1245\n303\n715\n1053\n340\n915\n160\n1796\n111\n925\n1872\n735\n350\n107\n1913\n1653\n987\n825\n1893\n1601\n460\n1228\n1526\n1613\n1359\n1854\n1352\n542\n665\n109\n1874\n467\n533\n1188\n1629\n851\n630\n1060\n1530\n1853\n743\n765\n126\n1540\n1411\n858\n1741\n284\n299\n577\n1848\n1495\n283\n1886\n284\n129\n1077\n1245\n1364\n1505\n176\n1012\n1663\n1306\n1586\n410\n315\n660\n256\n1102\n1289\n1292\n939\n762\n601\n1140\n574\n1851\n44\n560\n1948\n1142\n1787\n947\n948\n280\n1210\n1139\n1072\n1033\n92\n1244\n1589\n1079\n22\n1514\n163\n157\n1742\n1058\n514\n196\n1858\n565\n354\n1413\n792\n183\n526\n1724\n1007\n158\n1229\n1802\n99\n1514\n708\n1276\n1802\n1564\n1387\n1235\n1132\n715\n1584\n617\n1664\n1559\n1625\n1037\n601\n1175\n1713\n107\n88\n384\n1634\n904\n1835\n1472\n212\n1145\n443\n1617\n866\n1963\n937\n1917\n855\n1215\n1867\n520\n892\n1483\n1898\n1747\n1441\n289\n1609\n328\n566\n271\n458\n1616\n843\n1107\n507\n1090\n854\n1094\n806\n166\n408\n661\n334\n230\n1917\n1323\n927\n1912\n673\n311\n952\n1783\n1549\n1714\n1500\n450\n1498\n530\n442\n607\n609\n1226\n370\n1769\n1815\n788\n536\n293\n115\n947\n290\n1764\n243\n1219\n1851\n289\n599\n1528\n150\n1859\n297\n279\n1542\n1719\n1910\n551\n401\n952\n1764\n946\n1835\n647\n1309\n271\n275\n70\n129\n1518\n972\n1164\n816\n1125\n575\n588\n1456\n1154\n290\n1681\n1133\n561\n343\n1360\n1035\n1158\n1365\n744\n781\n58\n531\n271\n1612\n1774\n28\n1480\n1312\n1855\n666\n1574\n613\n42\n456\n351\n727\n1503\n1115\n333\n1972\n822\n1575\n848\n1087\n1262\n1671\n710\n460\n1816\n287\n172\n492\n1079\n582\n1236\n1756\n1792\n1095\n1205\n1894\n22\n1930\n1529\n1547\n1383\n1768\n364\n1108\n1972\n287\n200\n230\n1335\n187\n486\n1722\n20\n963\n792\n1114\n633\n1862\n1433\n829\n737\n215\n1570\n378\n1677\n944\n1301\n1160\n500\n150\n886\n1337\n662\n1062\n290\n460\n592\n1867\n872\n155\n1613\n1913\n1548\n1847\n855\n1702\n952\n1894\n587\n1813\n1021\n21\n654\n254\n910\n1696\n1606\n679\n1222\n696\n1319\n368\n447\n549\n905\n1194\n189\n1766\n616\n278\n1418\n1965\n872\n998\n1268\n1673\n1647\n1163\n533\n1650\n1849\n1124\n1252\n1412\n703\n944\n468\n1485\n1352\n681\n864\n1432\n1771\n497\n956\n1794\n363\n1099\n1804\n457\n1227\n1487\n446\n1993\n1576\n272\n709\n1810\n330\n876\n1107\n1187\n122\n1625\n472\n676\n314\n1257\n1509\n350\n741\n366\n33\n536\n293\n1663\n1039\n1527\n126\n923\n1937\n1767\n1302\n1510\n1518\n1343\n91\n1551\n1614\n1687\n1748\n137\n75\n738\n1977\n751\n237\n313\n566\n24\n202\n889\n1716\n1460\n129\n1760\n1597\n96\n1057\n1323\n1188\n1373\n537\n955\n65\n1679\n1441\n1315\n398\n647\n1470\n1335\n617\n331\n796\n129\n1635\n1497\n836\n855\n1472\n1828\n568\n862\n690\n1370\n1657\n819\n45\n420\n258\n1980\n672\n615\n358\n852\n1148\n1897\n1306\n1092\n1405\n719\n1752\n1456\n1338\n332\n351\n479\n747\n249\n1977\n1671\n1061\n1685\n306\n254\n1060\n764\n420\n1139\n1452\n426\n835\n929\n1424\n1336\n697\n191\n1697\n1897\n644\n546\n982\n359\n1201\n1095\n1623\n1947\n215\n10\n855\n297\n551\n1037\n945\n396\n211\n1059\n423\n1521\n1770\n203\n1828\n879\n1179\n1912\n1028\n1416\n1845\n698\n715\n1857\n817\n50\n473\n1122\n126\n70\n1773\n40\n1970\n1311\n826\n355\n1921\n23\n526\n1717\n1397\n1932\n1075\n1652\n997\n1039\n1481\n779\n415\n49\n1330\n317\n1701\n690\n245\n1824\n639\n799\n1240\n422\n344\n1639\n20\n546\n912\n1930\n1368\n1541\n1109\n369\n66\n1564\n444\n1928\n1963\n1899\n744\n1593\n1702\n100\n");

        expect("10 dim a(2000)\n20 for i = 2 to 2000\n30 let a(i) = 1\n40 next i\n50 for i = 2 to sqr(2000)\n60 if a(i) = 0 then 100\n70 for j = i ^ 2 to 2000 step i\n80 let a(j) = 0\n90 next j\n100 next i\n110 for i = 2 to 2000\n120 if a(i) = 0 then 140\n130 print i\n140 next i\n150 end\n", "2\n3\n5\n7\n11\n13\n17\n19\n23\n29\n31\n37\n41\n43\n47\n53\n59\n61\n67\n71\n73\n79\n83\n89\n97\n101\n103\n107\n109\n113\n127\n131\n137\n139\n149\n151\n157\n163\n167\n173\n179\n181\n191\n193\n197\n199\n211\n223\n227\n229\n233\n239\n241\n251\n257\n263\n269\n271\n277\n281\n283\n293\n307\n311\n313\n317\n331\n337\n347\n349\n353\n359\n367\n373\n379\n383\n389\n397\n401\n409\n419\n421\n431\n433\n439\n443\n449\n457\n461\n463\n467\n479\n487\n491\n499\n503\n509\n521\n523\n541\n547\n557\n563\n569\n571\n577\n587\n593\n599\n601\n607\n613\n617\n619\n631\n641\n643\n647\n653\n659\n661\n673\n677\n683\n691\n701\n709\n719\n727\n733\n739\n743\n751\n757\n761\n769\n773\n787\n797\n809\n811\n821\n823\n827\n829\n839\n853\n857\n859\n863\n877\n881\n883\n887\n907\n911\n919\n929\n937\n941\n947\n953\n967\n971\n977\n983\n991\n997\n1009\n1013\n1019\n1021\n1031\n1033\n1039\n1049\n1051\n1061\n1063\n1069\n1087\n1091\n1093\n1097\n1103\n1109\n1117\n1123\n1129\n1151\n1153\n1163\n1171\n1181\n1187\n1193\n1201\n1213\n1217\n1223\n1229\n1231\n1237\n1249\n1259\n1277\n1279\n1283\n1289\n1291\n1297\n1301\n1303\n1307\n1319\n1321\n1327\n1361\n1367\n1373\n1381\n1399\n1409\n1423\n1427\n1429\n1433\n1439\n1447\n1451\n1453\n1459\n1471\n1481\n1483\n1487\n1489\n1493\n1499\n1511\n1523\n1531\n1543\n1549\n1553\n1559\n1567\n1571\n1579\n1583\n1597\n1601\n1607\n1609\n1613\n1619\n1621\n1627\n1637\n1657\n1663\n1667\n1669\n1693\n1697\n1699\n1709\n1721\n1723\n1733\n1741\n1747\n1753\n1759\n1777\n1783\n1787\n1789\n1801\n1811\n1823\n1831\n1847\n1861\n1867\n1871\n1873\n1877\n1879\n1889\n1901\n1907\n1913\n1931\n1933\n1949\n1951\n1973\n1979\n1987\n1993\n1997\n1999\n");
    }
}
    
function runBenchmark()
{
    const numIterations = 4;
    let benchmark = new BasicBenchmark();
    for (let iteration = 0; iteration < numIterations; ++iteration)
        benchmark.runIteration();
    
}
runBenchmark();
