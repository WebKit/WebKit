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

