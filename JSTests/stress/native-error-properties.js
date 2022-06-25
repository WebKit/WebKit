"use strict";

function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}

function shouldNotThrow(expr) {
    let testFunc = new Function(expr);
    let error;
    try {
        testFunc();
    } catch (e) {
        error = e;
    }
    assert(!error);
}

function checkEmptyErrorPropertiesDescriptors(error) {
    let descriptor = Object.getOwnPropertyDescriptor(error, "message");
    assert(descriptor === undefined);
}

function checkNonEmptyErrorPropertiesDescriptors(error) {
    let descriptor = Object.getOwnPropertyDescriptor(error, "message");
    assert(descriptor.configurable);
    assert(!descriptor.enumerable);
    assert(descriptor.writable);
}

function checkErrorPropertiesWritable(error) {
    let properties = ["name", "message", "line", "lineNumber", "column", "columnNumber", "sourceURL", "stack"];
    for (let p of properties) {
        assert(error[p] !== 999);
        error[p] = 999;
        assert(error[p] === 999);
    }
}

// User created error instances.
let errorConstructors = [Error, EvalError, RangeError, ReferenceError, SyntaxError, TypeError, URIError];
for (let constructor of errorConstructors) {
    shouldNotThrow(`checkErrorPropertiesWritable(new ${constructor.name})`);
    shouldNotThrow(`checkEmptyErrorPropertiesDescriptors(new ${constructor.name})`);
    shouldNotThrow(`checkNonEmptyErrorPropertiesDescriptors(new ${constructor.name}('message'))`);
}

// Engine created error instances.
var globalError = null;

try {
    eval("{");
} catch (e) {
    globalError = e;
    assert(e.name === "SyntaxError");
    assert(e.message.length);
    shouldNotThrow("checkNonEmptyErrorPropertiesDescriptors(globalError)");
    shouldNotThrow("checkErrorPropertiesWritable(globalError)");
}

try {
    a.b.c;
} catch (e) {
    globalError = e;
    assert(e.name === "ReferenceError");
    assert(e.message.length);
    shouldNotThrow("checkNonEmptyErrorPropertiesDescriptors(globalError)");
    shouldNotThrow("checkErrorPropertiesWritable(globalError)");
}

try {
    undefined.x;
} catch (e) {
    globalError = e;
    assert(e.name === "TypeError");
    assert(e.message.length);
    shouldNotThrow("checkNonEmptyErrorPropertiesDescriptors(globalError)");
    shouldNotThrow("checkErrorPropertiesWritable(globalError)");
}
