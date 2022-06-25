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

const _fail = (msg, extra) => {
    throw new Error(msg + (extra ? ": " + extra : ""));
};

const assert = {};

const isNotA = assert.isNotA = (v, t, msg) => {
    if (typeof v === t)
        _fail(`Shouldn't be ${t}`, msg);
};

const isA = assert.isA = (v, t, msg) => {
    if (typeof v !== t)
        _fail(`Should be ${t}, got ${typeof(v)}`, msg);
};

const isNotUndef = assert.isNotUndef = (v, msg) => isNotA(v, "undefined", msg);
assert.isUndef = (v, msg) => isA(v, "undefined", msg);
assert.notObject = (v, msg) => isNotA(v, "object", msg);
const isObject = assert.isObject = (v, msg) => isA(v, "object", msg);
assert.notString = (v, msg) => isNotA(v, "string", msg);
assert.isString = (v, msg) => isA(v, "string", msg);
assert.notNumber = (v, msg) => isNotA(v, "number", msg);
assert.isNumber = (v, msg) => isA(v, "number", msg);
assert.notFunction = (v, msg) => isNotA(v, "function", msg);
assert.isFunction = (v, msg) => isA(v, "function", msg);

assert.hasObjectProperty = (o, p, msg) => {
    isObject(o, msg);
    isNotUndef(o[p], msg, `expected object to have property ${p}`);
};

assert.isArray = (v, msg) => {
    if (!Array.isArray(v))
        _fail(`Expected an array, got ${typeof(v)}`, msg);
};

assert.isNotArray = (v, msg) => {
    if (Array.isArray(v))
        _fail(`Expected to not be an array`, msg);
};

assert.truthy = (v, msg) => {
    if (!v)
        _fail(`Expected truthy`, msg);
};

assert.falsy = (v, msg) => {
    if (v)
        _fail(`Expected falsy`, msg);
};

assert.eq = (lhs, rhs, msg) => {
    if (typeof lhs !== typeof rhs)
        _fail(`Not the same: "${lhs}" and "${rhs}"`, msg);
    if (Array.isArray(lhs) && Array.isArray(rhs) && (lhs.length === rhs.length)) {
        for (let i = 0; i !== lhs.length; ++i)
            eq(lhs[i], rhs[i], msg);
    } else if (lhs !== rhs) {
        if (typeof lhs === "number" && isNaN(lhs) && isNaN(rhs))
            return;
        _fail(`Not the same: "${lhs}" and "${rhs}"`, msg);
    } else {
        if (typeof lhs === "number" && (1.0 / lhs !== 1.0 / rhs)) // Distinguish -0.0 from 0.0.
            _fail(`Not the same: "${lhs}" and "${rhs}"`, msg);
    }
};

const canonicalizeI32 = (number) => {
    if (Math.round(number) === number && number >= 2 ** 31)
        number = number - 2 ** 32;
    return number;
}

assert.eqI32 = (lhs, rhs, msg) => {
    return eq(canonicalizeI32(lhs), canonicalizeI32(rhs), msg);
};

assert.ge = (lhs, rhs, msg) => {
    isNotUndef(lhs);
    isNotUndef(rhs);
    if (!(lhs >= rhs))
        _fail(`Expected: "${lhs}" < "${rhs}"`, msg);
};

assert.le = (lhs, rhs, msg) => {
    isNotUndef(lhs);
    isNotUndef(rhs);
    if (!(lhs <= rhs))
        _fail(`Expected: "${lhs}" > "${rhs}"`, msg);
};

const _throws = (func, type, message, ...args) => {
    try {
        func(...args);
    } catch (e) {
        if (e instanceof type) {
            if (e.message === message)
                return e;
            // Ignore source information at the end of the error message if the
            // expected message didn't specify that information. Sometimes it
            // changes, or it's tricky to get just right.
            const evaluatingIndex = e.message.indexOf(" (evaluating '");
            if (evaluatingIndex !== -1) {
                const cleanMessage = e.message.substring(0, evaluatingIndex);
                if (cleanMessage === message)
                    return e;
            }
        }
        _fail(`Expected to throw a ${type.name} with message "${message}", got ${e.name} with message "${e.message}"`);
    }
    _fail(`Expected to throw a ${type.name} with message "${message}"`);
};

const _instanceof = (obj, type, msg) => {
    if (!(obj instanceof type))
        _fail(`Expected a ${typeof(type)}, got ${typeof obj}`);
};

// Use underscore names to avoid clashing with builtin names.
assert.throws = _throws;
assert.instanceof = _instanceof;

const asyncTestImpl = (promise, thenFunc, catchFunc) => {
    asyncTestStart(1);
    promise.then(thenFunc).catch(catchFunc);
};

const printExn = (e) => {
    print("Failed: ", e);
    print(e.stack);
};

assert.asyncTest = (promise) => asyncTestImpl(promise, asyncTestPassed, printExn);
assert.asyncTestEq = (promise, expected) => {
    const thenCheck = (value) => {
        if (value === expected)
            return asyncTestPassed();
        print("Failed: got ", value, " but expected ", expected);

    }
    asyncTestImpl(promise, thenCheck, printExn);
};

const WASM_JSON = `
{
    "comments": ["This file describes the WebAssembly ISA.",
                 "Scripts in this folder auto-generate C++ code for JavaScriptCore as well as the testing DSL which WebKit's WebAssembly tests use."
                ],
    "preamble": [
        { "name": "magic number", "type": "uint32", "value": 1836278016, "description": "NULL character followed by 'asm'" },
        { "name": "version",      "type": "uint32", "value":          1, "description": "Version number" }
    ],
    "type" : {
        "i32":     { "type": "varint7", "value":  -1, "b3type": "B3::Int32" },
        "i64":     { "type": "varint7", "value":  -2, "b3type": "B3::Int64" },
        "f32":     { "type": "varint7", "value":  -3, "b3type": "B3::Float" },
        "f64":     { "type": "varint7", "value":  -4, "b3type": "B3::Double" },
        "anyfunc": { "type": "varint7", "value": -16, "b3type": "B3::Void" },
        "func":    { "type": "varint7", "value": -32, "b3type": "B3::Void" },
        "void":    { "type": "varint7", "value": -64, "b3type": "B3::Void" }
    },
    "value_type": ["i32", "i64", "f32", "f64"],
    "block_type": ["i32", "i64", "f32", "f64", "void"],
    "elem_type": ["anyfunc"],
    "external_kind": {
        "Function": { "type": "uint8", "value": 0 },
        "Table":    { "type": "uint8", "value": 1 },
        "Memory":   { "type": "uint8", "value": 2 },
        "Global":   { "type": "uint8", "value": 3 }
    },
    "section" : {
        "Type":     { "type": "varuint7", "value":  1, "description": "Function signature declarations" },
        "Import":   { "type": "varuint7", "value":  2, "description": "Import declarations" },
        "Function": { "type": "varuint7", "value":  3, "description": "Function declarations" },
        "Table":    { "type": "varuint7", "value":  4, "description": "Indirect function table and other tables" },
        "Memory":   { "type": "varuint7", "value":  5, "description": "Memory attributes" },
        "Global":   { "type": "varuint7", "value":  6, "description": "Global declarations" },
        "Export":   { "type": "varuint7", "value":  7, "description": "Exports" },
        "Start":    { "type": "varuint7", "value":  8, "description": "Start function declaration" },
        "Element":  { "type": "varuint7", "value":  9, "description": "Elements section" },
        "Code":     { "type": "varuint7", "value": 10, "description": "Function bodies (code)" },
        "Data":     { "type": "varuint7", "value": 11, "description": "Data segments" }
    },
    "opcode": {
        "unreachable":         { "category": "control",    "value":   0, "return": [],           "parameter": [],                      "immediate": [],                                                                                         "description": "trap immediately" },
        "block":               { "category": "control",    "value":   2, "return": ["control"],  "parameter": [],                      "immediate": [{"name": "sig", "type": "block_type"}],                                                    "description": "begin a sequence of expressions, yielding 0 or 1 values" },
        "loop":                { "category": "control",    "value":   3, "return": ["control"],  "parameter": [],                      "immediate": [{"name": "sig", "type": "block_type"}],                                                    "description": "begin a block which can also form control flow loops" },
        "if":                  { "category": "control",    "value":   4, "return": ["control"],  "parameter": ["bool"],                "immediate": [{"name": "sig", "type": "block_type"}],                                                    "description": "begin if expression" },
        "else":                { "category": "control",    "value":   5, "return": ["control"],  "parameter": [],                      "immediate": [],                                                                                         "description": "begin else expression of if" },
        "select":              { "category": "control",    "value":  27, "return": ["prev"],     "parameter": ["any", "prev", "bool"], "immediate": [],                                                                                         "description": "select one of two values based on condition" },
        "br":                  { "category": "control",    "value":  12, "return": [],           "parameter": [],                      "immediate": [{"name": "relative_depth", "type": "varuint32"}],                                          "description": "break that targets an outer nested block" },
        "br_if":               { "category": "control",    "value":  13, "return": [],           "parameter": [],                      "immediate": [{"name": "relative_depth", "type": "varuint32"}],                                          "description": "conditional break that targets an outer nested block" },
        "br_table":            { "category": "control",    "value":  14, "return": [],           "parameter": [],                      "immediate": [{"name": "target_count",   "type": "varuint32",                                            "description": "number of entries in the target_table"},
                                                                                                                                                     {"name": "target_table",   "type": "varuint32*",                                           "description": "target entries that indicate an outer block or loop to which to break"},
                                                                                                                                                     {"name": "default_target", "type": "varuint32",                                            "description": "an outer block or loop to which to break in the default case"}],
                                                                                                                                                                                                                                                "description": "branch table control flow construct" },
        "return":              { "category": "control",    "value":  15, "return": [],           "parameter": [],                       "immediate": [],                                                                                         "description": "return zero or one value from this function" },
        "drop":                { "category": "control",    "value":  26, "return": [],           "parameter": ["any"],                  "immediate": [],                                                                                         "description": "ignore value" },
        "nop":                 { "category": "control",    "value":   1, "return": [],           "parameter": [],                       "immediate": [],                                                                                         "description": "no operation" },
        "end":                 { "category": "control",    "value":  11, "return": [],           "parameter": [],                       "immediate": [],                                                                                         "description": "end a block, loop, or if" },
        "i32.const":           { "category": "special",    "value":  65, "return": ["i32"],      "parameter": [],                       "immediate": [{"name": "value",          "type": "varint32"}],                                           "description": "a constant value interpreted as i32" },
        "i64.const":           { "category": "special",    "value":  66, "return": ["i64"],      "parameter": [],                       "immediate": [{"name": "value",          "type": "varint64"}],                                           "description": "a constant value interpreted as i64" },
        "f64.const":           { "category": "special",    "value":  68, "return": ["f64"],      "parameter": [],                       "immediate": [{"name": "value",          "type": "double"}],                                             "description": "a constant value interpreted as f64" },
        "f32.const":           { "category": "special",    "value":  67, "return": ["f32"],      "parameter": [],                       "immediate": [{"name": "value",          "type": "float"}],                                             "description": "a constant value interpreted as f32" },
        "get_local":           { "category": "special",    "value":  32, "return": ["any"],    "parameter": [],                       "immediate": [{"name": "local_index",    "type": "varuint32"}],                                          "description": "read a local variable or parameter" },
        "set_local":           { "category": "special",    "value":  33, "return": [],           "parameter": ["any"],                "immediate": [{"name": "local_index",    "type": "varuint32"}],                                          "description": "write a local variable or parameter" },
        "tee_local":           { "category": "special",    "value":  34, "return": ["any"],     "parameter": ["any"],                  "immediate": [{"name": "local_index",    "type": "varuint32"}],                                          "description": "write a local variable or parameter and return the same value" },
        "get_global":          { "category": "special",    "value":  35, "return": ["any"],   "parameter": [],                       "immediate": [{"name": "global_index",   "type": "varuint32"}],                                          "description": "read a global variable" },
        "set_global":          { "category": "special",    "value":  36, "return": [],         "parameter": ["any"],               "immediate": [{"name": "global_index",   "type": "varuint32"}],                                          "description": "write a global variable" },
        "call":                { "category": "call",       "value":  16, "return": ["call"],     "parameter": ["call"],                 "immediate": [{"name": "function_index", "type": "varuint32"}],                                          "description": "call a function by its index" },
        "call_indirect":       { "category": "call",       "value":  17, "return": ["call"],     "parameter": ["call"],                 "immediate": [{"name": "type_index",     "type": "varuint32"}, {"name": "reserved",     "type": "varuint1"}], "description": "call a function indirect with an expected signature" },
        "i32.load8_s":         { "category": "memory",     "value":  44, "return": ["i32"],      "parameter": ["addr"],                 "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "load from memory" },
        "i32.load8_u":         { "category": "memory",     "value":  45, "return": ["i32"],      "parameter": ["addr"],                 "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "load from memory" },
        "i32.load16_s":        { "category": "memory",     "value":  46, "return": ["i32"],      "parameter": ["addr"],                 "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "load from memory" },
        "i32.load16_u":        { "category": "memory",     "value":  47, "return": ["i32"],      "parameter": ["addr"],                 "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "load from memory" },
        "i64.load8_s":         { "category": "memory",     "value":  48, "return": ["i64"],      "parameter": ["addr"],                 "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "load from memory" },
        "i64.load8_u":         { "category": "memory",     "value":  49, "return": ["i64"],      "parameter": ["addr"],                 "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "load from memory" },
        "i64.load16_s":        { "category": "memory",     "value":  50, "return": ["i64"],      "parameter": ["addr"],                 "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "load from memory" },
        "i64.load16_u":        { "category": "memory",     "value":  51, "return": ["i64"],      "parameter": ["addr"],                 "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "load from memory" },
        "i64.load32_s":        { "category": "memory",     "value":  52, "return": ["i64"],      "parameter": ["addr"],                 "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "load from memory" },
        "i64.load32_u":        { "category": "memory",     "value":  53, "return": ["i64"],      "parameter": ["addr"],                 "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "load from memory" },
        "i32.load":            { "category": "memory",     "value":  40, "return": ["i32"],      "parameter": ["addr"],                 "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "load from memory" },
        "i64.load":            { "category": "memory",     "value":  41, "return": ["i64"],      "parameter": ["addr"],                 "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "load from memory" },
        "f32.load":            { "category": "memory",     "value":  42, "return": ["f32"],      "parameter": ["addr"],                 "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "load from memory" },
        "f64.load":            { "category": "memory",     "value":  43, "return": ["f64"],      "parameter": ["addr"],                 "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "load from memory" },
        "i32.store8":          { "category": "memory",     "value":  58, "return": [],           "parameter": ["addr", "i32"],          "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "store to memory" },
        "i32.store16":         { "category": "memory",     "value":  59, "return": [],           "parameter": ["addr", "i32"],          "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "store to memory" },
        "i64.store8":          { "category": "memory",     "value":  60, "return": [],           "parameter": ["addr", "i64"],          "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "store to memory" },
        "i64.store16":         { "category": "memory",     "value":  61, "return": [],           "parameter": ["addr", "i64"],          "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "store to memory" },
        "i64.store32":         { "category": "memory",     "value":  62, "return": [],           "parameter": ["addr", "i64"],          "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "store to memory" },
        "i32.store":           { "category": "memory",     "value":  54, "return": [],           "parameter": ["addr", "i32"],          "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "store to memory" },
        "i64.store":           { "category": "memory",     "value":  55, "return": [],           "parameter": ["addr", "i64"],          "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "store to memory" },
        "f32.store":           { "category": "memory",     "value":  56, "return": [],           "parameter": ["addr", "f32"],          "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "store to memory" },
        "f64.store":           { "category": "memory",     "value":  57, "return": [],           "parameter": ["addr", "f64"],          "immediate": [{"name": "flags",          "type": "varuint32"}, {"name": "offset", "type": "varuint32"}], "description": "store to memory" },
        "current_memory":      { "category": "operation",  "value":  63, "return": ["size"],     "parameter": [],                       "immediate": [{"name": "flags", "type": "varuint32"}],                                                                                         "description": "query the size of memory" },
        "grow_memory":         { "category": "operation",  "value":  64, "return": ["size"],     "parameter": ["size"],                 "immediate": [{"name": "flags", "type": "varuint32"}],                                                                                         "description": "grow the size of memory" },
        "i32.add":             { "category": "arithmetic", "value": 106, "return": ["i32"],      "parameter": ["i32", "i32"],           "immediate": [], "b3op": "Add"          },
        "i32.sub":             { "category": "arithmetic", "value": 107, "return": ["i32"],      "parameter": ["i32", "i32"],           "immediate": [], "b3op": "Sub"          },
        "i32.mul":             { "category": "arithmetic", "value": 108, "return": ["i32"],      "parameter": ["i32", "i32"],           "immediate": [], "b3op": "Mul"          },
        "i32.div_s":           { "category": "arithmetic", "value": 109, "return": ["i32"],      "parameter": ["i32", "i32"],           "immediate": []                         },
        "i32.div_u":           { "category": "arithmetic", "value": 110, "return": ["i32"],      "parameter": ["i32", "i32"],           "immediate": []                         },
        "i32.rem_s":           { "category": "arithmetic", "value": 111, "return": ["i32"],      "parameter": ["i32", "i32"],           "immediate": []                         },
        "i32.rem_u":           { "category": "arithmetic", "value": 112, "return": ["i32"],      "parameter": ["i32", "i32"],           "immediate": []                         },
        "i32.and":             { "category": "arithmetic", "value": 113, "return": ["i32"],      "parameter": ["i32", "i32"],           "immediate": [], "b3op": "BitAnd"       },
        "i32.or":              { "category": "arithmetic", "value": 114, "return": ["i32"],      "parameter": ["i32", "i32"],           "immediate": [], "b3op": "BitOr"        },
        "i32.xor":             { "category": "arithmetic", "value": 115, "return": ["i32"],      "parameter": ["i32", "i32"],           "immediate": [], "b3op": "BitXor"       },
        "i32.shl":             { "category": "arithmetic", "value": 116, "return": ["i32"],      "parameter": ["i32", "i32"],           "immediate": [], "b3op": "Shl"          },
        "i32.shr_u":           { "category": "arithmetic", "value": 118, "return": ["i32"],      "parameter": ["i32", "i32"],           "immediate": [], "b3op": "ZShr"         },
        "i32.shr_s":           { "category": "arithmetic", "value": 117, "return": ["i32"],      "parameter": ["i32", "i32"],           "immediate": [], "b3op": "SShr"         },
        "i32.rotr":            { "category": "arithmetic", "value": 120, "return": ["i32"],      "parameter": ["i32", "i32"],           "immediate": [], "b3op": "RotR"         },
        "i32.rotl":            { "category": "arithmetic", "value": 119, "return": ["i32"],      "parameter": ["i32", "i32"],           "immediate": [], "b3op": "RotL"         },
        "i32.eq":              { "category": "comparison", "value":  70, "return": ["bool"],     "parameter": ["i32", "i32"],           "immediate": [], "b3op": "Equal"        },
        "i32.ne":              { "category": "comparison", "value":  71, "return": ["bool"],     "parameter": ["i32", "i32"],           "immediate": [], "b3op": "NotEqual"     },
        "i32.lt_s":            { "category": "comparison", "value":  72, "return": ["bool"],     "parameter": ["i32", "i32"],           "immediate": [], "b3op": "LessThan"     },
        "i32.le_s":            { "category": "comparison", "value":  76, "return": ["bool"],     "parameter": ["i32", "i32"],           "immediate": [], "b3op": "LessEqual"    },
        "i32.lt_u":            { "category": "comparison", "value":  73, "return": ["bool"],     "parameter": ["i32", "i32"],           "immediate": [], "b3op": "Below"        },
        "i32.le_u":            { "category": "comparison", "value":  77, "return": ["bool"],     "parameter": ["i32", "i32"],           "immediate": [], "b3op": "BelowEqual"   },
        "i32.gt_s":            { "category": "comparison", "value":  74, "return": ["bool"],     "parameter": ["i32", "i32"],           "immediate": [], "b3op": "GreaterThan"  },
        "i32.ge_s":            { "category": "comparison", "value":  78, "return": ["bool"],     "parameter": ["i32", "i32"],           "immediate": [], "b3op": "GreaterEqual" },
        "i32.gt_u":            { "category": "comparison", "value":  75, "return": ["bool"],     "parameter": ["i32", "i32"],           "immediate": [], "b3op": "Above"        },
        "i32.ge_u":            { "category": "comparison", "value":  79, "return": ["bool"],     "parameter": ["i32", "i32"],           "immediate": [], "b3op": "AboveEqual"   },
        "i32.clz":             { "category": "arithmetic", "value": 103, "return": ["i32"],      "parameter": ["i32"],                  "immediate": [], "b3op": "Clz"          },
        "i32.ctz":             { "category": "arithmetic", "value": 104, "return": ["i32"],      "parameter": ["i32"],                  "immediate": []                         },
        "i32.popcnt":          { "category": "arithmetic", "value": 105, "return": ["i32"],      "parameter": ["i32"],                  "immediate": []                         },
        "i32.eqz":             { "category": "comparison", "value":  69, "return": ["bool"],     "parameter": ["i32"],                  "immediate": [], "b3op": "Equal(i32(0), @0)" },
        "i64.add":             { "category": "arithmetic", "value": 124, "return": ["i64"],      "parameter": ["i64", "i64"],           "immediate": [], "b3op": "Add"          },
        "i64.sub":             { "category": "arithmetic", "value": 125, "return": ["i64"],      "parameter": ["i64", "i64"],           "immediate": [], "b3op": "Sub"          },
        "i64.mul":             { "category": "arithmetic", "value": 126, "return": ["i64"],      "parameter": ["i64", "i64"],           "immediate": [], "b3op": "Mul"          },
        "i64.div_s":           { "category": "arithmetic", "value": 127, "return": ["i64"],      "parameter": ["i64", "i64"],           "immediate": []                         },
        "i64.div_u":           { "category": "arithmetic", "value": 128, "return": ["i64"],      "parameter": ["i64", "i64"],           "immediate": []                         },
        "i64.rem_s":           { "category": "arithmetic", "value": 129, "return": ["i64"],      "parameter": ["i64", "i64"],           "immediate": []                         },
        "i64.rem_u":           { "category": "arithmetic", "value": 130, "return": ["i64"],      "parameter": ["i64", "i64"],           "immediate": []                         },
        "i64.and":             { "category": "arithmetic", "value": 131, "return": ["i64"],      "parameter": ["i64", "i64"],           "immediate": [], "b3op": "BitAnd"       },
        "i64.or":              { "category": "arithmetic", "value": 132, "return": ["i64"],      "parameter": ["i64", "i64"],           "immediate": [], "b3op": "BitOr"        },
        "i64.xor":             { "category": "arithmetic", "value": 133, "return": ["i64"],      "parameter": ["i64", "i64"],           "immediate": [], "b3op": "BitXor"       },
        "i64.shl":             { "category": "arithmetic", "value": 134, "return": ["i64"],      "parameter": ["i64", "i64"],           "immediate": [], "b3op": "Shl(@0, Trunc(@1))" },
        "i64.shr_u":           { "category": "arithmetic", "value": 136, "return": ["i64"],      "parameter": ["i64", "i64"],           "immediate": [], "b3op": "ZShr(@0, Trunc(@1))" },
        "i64.shr_s":           { "category": "arithmetic", "value": 135, "return": ["i64"],      "parameter": ["i64", "i64"],           "immediate": [], "b3op": "SShr(@0, Trunc(@1))" },
        "i64.rotr":            { "category": "arithmetic", "value": 138, "return": ["i64"],      "parameter": ["i64", "i64"],           "immediate": [], "b3op": "RotR(@0, Trunc(@1))" },
        "i64.rotl":            { "category": "arithmetic", "value": 137, "return": ["i64"],      "parameter": ["i64", "i64"],           "immediate": [], "b3op": "RotL(@0, Trunc(@1))" },
        "i64.eq":              { "category": "comparison", "value":  81, "return": ["bool"],     "parameter": ["i64", "i64"],           "immediate": [], "b3op": "Equal"        },
        "i64.ne":              { "category": "comparison", "value":  82, "return": ["bool"],     "parameter": ["i64", "i64"],           "immediate": [], "b3op": "NotEqual"     },
        "i64.lt_s":            { "category": "comparison", "value":  83, "return": ["bool"],     "parameter": ["i64", "i64"],           "immediate": [], "b3op": "LessThan"     },
        "i64.le_s":            { "category": "comparison", "value":  87, "return": ["bool"],     "parameter": ["i64", "i64"],           "immediate": [], "b3op": "LessEqual"    },
        "i64.lt_u":            { "category": "comparison", "value":  84, "return": ["bool"],     "parameter": ["i64", "i64"],           "immediate": [], "b3op": "Below"        },
        "i64.le_u":            { "category": "comparison", "value":  88, "return": ["bool"],     "parameter": ["i64", "i64"],           "immediate": [], "b3op": "BelowEqual"   },
        "i64.gt_s":            { "category": "comparison", "value":  85, "return": ["bool"],     "parameter": ["i64", "i64"],           "immediate": [], "b3op": "GreaterThan"  },
        "i64.ge_s":            { "category": "comparison", "value":  89, "return": ["bool"],     "parameter": ["i64", "i64"],           "immediate": [], "b3op": "GreaterEqual" },
        "i64.gt_u":            { "category": "comparison", "value":  86, "return": ["bool"],     "parameter": ["i64", "i64"],           "immediate": [], "b3op": "Above"        },
        "i64.ge_u":            { "category": "comparison", "value":  90, "return": ["bool"],     "parameter": ["i64", "i64"],           "immediate": [], "b3op": "AboveEqual"   },
        "i64.clz":             { "category": "arithmetic", "value": 121, "return": ["i64"],      "parameter": ["i64"],                  "immediate": [], "b3op": "Clz"          },
        "i64.ctz":             { "category": "arithmetic", "value": 122, "return": ["i64"],      "parameter": ["i64"],                  "immediate": []                         },
        "i64.popcnt":          { "category": "arithmetic", "value": 123, "return": ["i64"],      "parameter": ["i64"],                  "immediate": []                         },
        "i64.eqz":             { "category": "comparison", "value":  80, "return": ["bool"],     "parameter": ["i64"],                  "immediate": [], "b3op": "Equal(i64(0), @0)" },
        "f32.add":             { "category": "arithmetic", "value": 146, "return": ["f32"],      "parameter": ["f32", "f32"],           "immediate": [], "b3op": "Add"          },
        "f32.sub":             { "category": "arithmetic", "value": 147, "return": ["f32"],      "parameter": ["f32", "f32"],           "immediate": [], "b3op": "Sub"          },
        "f32.mul":             { "category": "arithmetic", "value": 148, "return": ["f32"],      "parameter": ["f32", "f32"],           "immediate": [], "b3op": "Mul"          },
        "f32.div":             { "category": "arithmetic", "value": 149, "return": ["f32"],      "parameter": ["f32", "f32"],           "immediate": [], "b3op": "Div"          },
        "f32.min":             { "category": "arithmetic", "value": 150, "return": ["f32"],      "parameter": ["f32", "f32"],           "immediate": [], "b3op": "Select(Equal(@0, @1), BitOr(@0, @1), Select(LessThan(@0, @1), @0, @1))" },
        "f32.max":             { "category": "arithmetic", "value": 151, "return": ["f32"],      "parameter": ["f32", "f32"],           "immediate": [], "b3op": "Select(Equal(@0, @1), BitAnd(@0, @1), Select(LessThan(@0, @1), @1, @0))" },
        "f32.abs":             { "category": "arithmetic", "value": 139, "return": ["f32"],      "parameter": ["f32"],                  "immediate": [], "b3op": "Abs"          },
        "f32.neg":             { "category": "arithmetic", "value": 140, "return": ["f32"],      "parameter": ["f32"],                  "immediate": [], "b3op": "Neg"          },
        "f32.copysign":        { "category": "arithmetic", "value": 152, "return": ["f32"],      "parameter": ["f32", "f32"],           "immediate": [], "b3op": "BitwiseCast(BitOr(BitAnd(BitwiseCast(@1), i32(0x80000000)), BitAnd(BitwiseCast(@0), i32(0x7fffffff))))" },
        "f32.ceil":            { "category": "arithmetic", "value": 141, "return": ["f32"],      "parameter": ["f32"],                  "immediate": [], "b3op": "Ceil"         },
        "f32.floor":           { "category": "arithmetic", "value": 142, "return": ["f32"],      "parameter": ["f32"],                  "immediate": [], "b3op": "Floor"        },
        "f32.trunc":           { "category": "arithmetic", "value": 143, "return": ["f32"],      "parameter": ["f32"],                  "immediate": []                         },
        "f32.nearest":         { "category": "arithmetic", "value": 144, "return": ["f32"],      "parameter": ["f32"],                  "immediate": []                         },
        "f32.sqrt":            { "category": "arithmetic", "value": 145, "return": ["f32"],      "parameter": ["f32"],                  "immediate": [], "b3op": "Sqrt"         },
        "f32.eq":              { "category": "comparison", "value":  91, "return": ["bool"],     "parameter": ["f32", "f32"],           "immediate": [], "b3op": "Equal"        },
        "f32.ne":              { "category": "comparison", "value":  92, "return": ["bool"],     "parameter": ["f32", "f32"],           "immediate": [], "b3op": "NotEqual"     },
        "f32.lt":              { "category": "comparison", "value":  93, "return": ["bool"],     "parameter": ["f32", "f32"],           "immediate": [], "b3op": "LessThan"     },
        "f32.le":              { "category": "comparison", "value":  95, "return": ["bool"],     "parameter": ["f32", "f32"],           "immediate": [], "b3op": "LessEqual"    },
        "f32.gt":              { "category": "comparison", "value":  94, "return": ["bool"],     "parameter": ["f32", "f32"],           "immediate": [], "b3op": "GreaterThan"  },
        "f32.ge":              { "category": "comparison", "value":  96, "return": ["bool"],     "parameter": ["f32", "f32"],           "immediate": [], "b3op": "GreaterEqual" },
        "f64.add":             { "category": "arithmetic", "value": 160, "return": ["f64"],      "parameter": ["f64", "f64"],           "immediate": [], "b3op": "Add"          },
        "f64.sub":             { "category": "arithmetic", "value": 161, "return": ["f64"],      "parameter": ["f64", "f64"],           "immediate": [], "b3op": "Sub"          },
        "f64.mul":             { "category": "arithmetic", "value": 162, "return": ["f64"],      "parameter": ["f64", "f64"],           "immediate": [], "b3op": "Mul"          },
        "f64.div":             { "category": "arithmetic", "value": 163, "return": ["f64"],      "parameter": ["f64", "f64"],           "immediate": [], "b3op": "Div"          },
        "f64.min":             { "category": "arithmetic", "value": 164, "return": ["f64"],      "parameter": ["f64", "f64"],           "immediate": [], "b3op": "Select(Equal(@0, @1), BitOr(@0, @1), Select(LessThan(@0, @1), @0, @1))" },
        "f64.max":             { "category": "arithmetic", "value": 165, "return": ["f64"],      "parameter": ["f64", "f64"],           "immediate": [], "b3op": "Select(Equal(@0, @1), BitAnd(@0, @1), Select(LessThan(@0, @1), @1, @0))" },
        "f64.abs":             { "category": "arithmetic", "value": 153, "return": ["f64"],      "parameter": ["f64"],                  "immediate": [], "b3op": "Abs"          },
        "f64.neg":             { "category": "arithmetic", "value": 154, "return": ["f64"],      "parameter": ["f64"],                  "immediate": [], "b3op": "Neg"          },
        "f64.copysign":        { "category": "arithmetic", "value": 166, "return": ["f64"],      "parameter": ["f64", "f64"],           "immediate": [], "b3op": "BitwiseCast(BitOr(BitAnd(BitwiseCast(@1), i64(0x8000000000000000)), BitAnd(BitwiseCast(@0), i64(0x7fffffffffffffff))))" },
        "f64.ceil":            { "category": "arithmetic", "value": 155, "return": ["f64"],      "parameter": ["f64"],                  "immediate": [], "b3op": "Ceil"         },
        "f64.floor":           { "category": "arithmetic", "value": 156, "return": ["f64"],      "parameter": ["f64"],                  "immediate": [], "b3op": "Floor"        },
        "f64.trunc":           { "category": "arithmetic", "value": 157, "return": ["f64"],      "parameter": ["f64"],                  "immediate": []                         },
        "f64.nearest":         { "category": "arithmetic", "value": 158, "return": ["f64"],      "parameter": ["f64"],                  "immediate": []                         },
        "f64.sqrt":            { "category": "arithmetic", "value": 159, "return": ["f64"],      "parameter": ["f64"],                  "immediate": [], "b3op": "Sqrt"         },
        "f64.eq":              { "category": "comparison", "value":  97, "return": ["bool"],     "parameter": ["f64", "f64"],           "immediate": [], "b3op": "Equal"        },
        "f64.ne":              { "category": "comparison", "value":  98, "return": ["bool"],     "parameter": ["f64", "f64"],           "immediate": [], "b3op": "NotEqual"     },
        "f64.lt":              { "category": "comparison", "value":  99, "return": ["bool"],     "parameter": ["f64", "f64"],           "immediate": [], "b3op": "LessThan"     },
        "f64.le":              { "category": "comparison", "value": 101, "return": ["bool"],     "parameter": ["f64", "f64"],           "immediate": [], "b3op": "LessEqual"    },
        "f64.gt":              { "category": "comparison", "value": 100, "return": ["bool"],     "parameter": ["f64", "f64"],           "immediate": [], "b3op": "GreaterThan"  },
        "f64.ge":              { "category": "comparison", "value": 102, "return": ["bool"],     "parameter": ["f64", "f64"],           "immediate": [], "b3op": "GreaterEqual" },
        "i32.trunc_s/f32":     { "category": "conversion", "value": 168, "return": ["i32"],      "parameter": ["f32"],                  "immediate": []                         },
        "i32.trunc_s/f64":     { "category": "conversion", "value": 170, "return": ["i32"],      "parameter": ["f64"],                  "immediate": []                         },
        "i32.trunc_u/f32":     { "category": "conversion", "value": 169, "return": ["i32"],      "parameter": ["f32"],                  "immediate": []                         },
        "i32.trunc_u/f64":     { "category": "conversion", "value": 171, "return": ["i32"],      "parameter": ["f64"],                  "immediate": []                         },
        "i32.wrap/i64":        { "category": "conversion", "value": 167, "return": ["i32"],      "parameter": ["i64"],                  "immediate": [], "b3op": "Trunc"        },
        "i64.trunc_s/f32":     { "category": "conversion", "value": 174, "return": ["i64"],      "parameter": ["f32"],                  "immediate": []                         },
        "i64.trunc_s/f64":     { "category": "conversion", "value": 176, "return": ["i64"],      "parameter": ["f64"],                  "immediate": []                         },
        "i64.trunc_u/f32":     { "category": "conversion", "value": 175, "return": ["i64"],      "parameter": ["f32"],                  "immediate": []                         },
        "i64.trunc_u/f64":     { "category": "conversion", "value": 177, "return": ["i64"],      "parameter": ["f64"],                  "immediate": []                         },
        "i64.extend_s/i32":    { "category": "conversion", "value": 172, "return": ["i64"],      "parameter": ["i32"],                  "immediate": [], "b3op": "SExt32"       },
        "i64.extend_u/i32":    { "category": "conversion", "value": 173, "return": ["i64"],      "parameter": ["i32"],                  "immediate": [], "b3op": "ZExt32"       },
        "f32.convert_s/i32":   { "category": "conversion", "value": 178, "return": ["f32"],      "parameter": ["i32"],                  "immediate": [], "b3op": "IToF"         },
        "f32.convert_u/i32":   { "category": "conversion", "value": 179, "return": ["f32"],      "parameter": ["i32"],                  "immediate": [], "b3op": "IToF(ZExt32(@0))" },
        "f32.convert_s/i64":   { "category": "conversion", "value": 180, "return": ["f32"],      "parameter": ["i64"],                  "immediate": [], "b3op": "IToF"         },
        "f32.convert_u/i64":   { "category": "conversion", "value": 181, "return": ["f32"],      "parameter": ["i64"],                  "immediate": []                         },
        "f32.demote/f64":      { "category": "conversion", "value": 182, "return": ["f32"],      "parameter": ["f64"],                  "immediate": [], "b3op": "DoubleToFloat"},
        "f32.reinterpret/i32": { "category": "conversion", "value": 190, "return": ["f32"],      "parameter": ["i32"],                  "immediate": [], "b3op": "BitwiseCast"  },
        "f64.convert_s/i32":   { "category": "conversion", "value": 183, "return": ["f64"],      "parameter": ["i32"],                  "immediate": [], "b3op": "IToD"         },
        "f64.convert_u/i32":   { "category": "conversion", "value": 184, "return": ["f64"],      "parameter": ["i32"],                  "immediate": [], "b3op": "IToD(ZExt32(@0))" },
        "f64.convert_s/i64":   { "category": "conversion", "value": 185, "return": ["f64"],      "parameter": ["i64"],                  "immediate": [], "b3op": "IToD"         },
        "f64.convert_u/i64":   { "category": "conversion", "value": 186, "return": ["f64"],      "parameter": ["i64"],                  "immediate": []                         },
        "f64.promote/f32":     { "category": "conversion", "value": 187, "return": ["f64"],      "parameter": ["f32"],                  "immediate": [], "b3op": "FloatToDouble"},
        "f64.reinterpret/i64": { "category": "conversion", "value": 191, "return": ["f64"],      "parameter": ["i64"],                  "immediate": [], "b3op": "BitwiseCast"  },
        "i32.reinterpret/f32": { "category": "conversion", "value": 188, "return": ["i32"],      "parameter": ["f32"],                  "immediate": [], "b3op": "BitwiseCast"  },
        "i64.reinterpret/f64": { "category": "conversion", "value": 189, "return": ["i64"],      "parameter": ["f64"],                  "immediate": [], "b3op": "BitwiseCast"  }
    }
}
`;


const _environment =
    ((typeof process === "object" && typeof require === "function") ? "node"
     : (typeof window === "object" ? "web"
        : (typeof importScripts === "function" ? "worker"
           : "shell")));

let _global = (typeof global !== 'object' || !global || global.Math !== Math || global.Array !== Array)
    ? ((typeof self !== 'undefined') ? self
       : (typeof window !== 'undefined') ? window
       : (typeof global !== 'undefined') ? global
       : Function('return this')())
    : global;

const _eval = x => eval.call(null, x);

const _read = filename => {
    switch (_environment) {
    case "node":   return read(filename);
    case "web":    // fallthrough
    case "worker": let xhr = new XMLHttpRequest(); xhr.open("GET", filename, /*async=*/false); return xhr.responseText;
    case "shell":  return read(filename);
    }
}

const _load = filename => {
    switch (_environment) {
    case "node":   // fallthrough
    case "web":    // fallthrough
    case "shell":  return _eval(_read(filename));
    case "worker": return importScripts(filename);
    }
}

const _json = filename => {
    assert.eq(filename, "wasm.json");
    return JSON.parse(WASM_JSON);

    switch (_environment) {
    case "node":   // fallthrough
    case "shell":  return JSON.parse(_read(filename));
    case "web":    // fallthrough
    case "worker": let xhr = new XMLHttpRequest(); xhr.overrideMimeType("application/json"); xhr.open("GET", filename, /*async=*/false); return xhr.response;
    }
}

const _dump = (what, name, pad = '    ') => {
    const value = v => {
        try { return `"${v}"`; }
        catch (e) { return `Error: "${e.message}"`; }
    };
    let s = `${pad}${name} ${typeof what}: ${value(what)}`;
    for (let p in what) {
        s += `\n${pad}${pad}${p}: ${value(what[p])} ${typeof v}`;
        s += '\n' + _dump(what[p], p, pad + pad);
    }
    return s;
};

// Use underscore names to avoid clashing with builtin names.
const utilities = {
    dump : _dump,
    eval : _eval,
    read : _read,
    load : _load,
    json : _json,
    global : _global
};


const _mapValues = from => {
    let values = {};
    for (const key in from)
        values[key] = from[key].value;
    return values;
};


const WASM = {};
{
    const description = WASM.description = utilities.json("wasm.json");
    const type = WASM.type = Object.keys(description.type);
    const _typeSet = new Set(type);
    WASM.isValidType = v => _typeSet.has(v);
    WASM.typeValue = _mapValues(description.type);
    const _valueTypeSet = new Set(description.value_type);
    WASM.isValidValueType = v => _valueTypeSet.has(v);
    const _blockTypeSet = new Set(description.block_type);
    WASM.isValidBlockType = v => _blockTypeSet.has(v);
    WASM.externalKindValue = _mapValues(description.external_kind);
    const sections = WASM.sections = Object.keys(description.section);
    WASM.sectionEncodingType = description.section[sections[0]].type;
}


// LowLevelBinary.js
const _initialAllocationSize = 1024;
const _growAllocationSize = allocated => allocated * 2;


const varuint32Min = 0;
const varint7Min = -0b1000000;
const varint7Max = 0b111111;
const varuint7Max = 0b1111111;
const varuint32Max = ((((1 << 31) >>> 0) - 1) * 2) + 1;
const varint32Min = -((1 << 31) >>> 0);
const varint32Max = ((1 << 31) - 1) >>> 0;
const varBitsMax = 5;

const _getterRangeCheck = (llb, at, size) => {
    if (0 > at || at + size > llb._used)
        throw new RangeError(`[${at}, ${at + size}) is out of buffer range [0, ${llb._used})`);
};

const _hexdump = (buf, size) => {
    let s = "";
    const width = 16;
    const base = 16;
    for (let row = 0; row * width < size; ++row) {
        const address = (row * width).toString(base);
        s += "0".repeat(8 - address.length) + address;
        let chars = "";
        for (let col = 0; col !== width; ++col) {
            const idx = row * width + col;
            if (idx < size) {
                const byte = buf[idx];
                const bytestr = byte.toString(base);
                s += " " + (bytestr.length === 1 ? "0" + bytestr : bytestr);
                chars += 0x20 <= byte && byte < 0x7F ? String.fromCharCode(byte) : "·";
            } else {
                s += "   ";
                chars += " ";
            }
        }
        s+= "  |" + chars + "|\n";
    }
    return s;
};

class LowLevelBinary {
    constructor() {
        this._buf = new Uint8Array(_initialAllocationSize);
        this._used = 0;
    }

    newPatchable(type) { return new PatchableLowLevelBinary(type, this); }

    // Utilities.
    get() { return this._buf; }
    trim() { this._buf = this._buf.slice(0, this._used); }

    hexdump() { return _hexdump(this._buf, this._used); }
    _maybeGrow(bytes) {
        const allocated = this._buf.length;
        if (allocated - this._used < bytes) {
            let buf = new Uint8Array(_growAllocationSize(allocated));
            buf.set(this._buf);
            this._buf = buf;
        }
    }
    _push8(v) { this._buf[this._used] = v & 0xFF; this._used += 1; }

    // Data types.
    uint8(v) {
        if ((v & 0xFF) >>> 0 !== v)
            throw new RangeError(`Invalid uint8 ${v}`);
        this._maybeGrow(1);
        this._push8(v);
    }
    uint16(v) {
        if ((v & 0xFFFF) >>> 0 !== v)
            throw new RangeError(`Invalid uint16 ${v}`);
        this._maybeGrow(2);
        this._push8(v);
        this._push8(v >>> 8);
    }
    uint24(v) {
        if ((v & 0xFFFFFF) >>> 0 !== v)
            throw new RangeError(`Invalid uint24 ${v}`);
        this._maybeGrow(3);
        this._push8(v);
        this._push8(v >>> 8);
        this._push8(v >>> 16);
    }
    uint32(v) {
        if ((v & 0xFFFFFFFF) >>> 0 !== v)
            throw new RangeError(`Invalid uint32 ${v}`);
        this._maybeGrow(4);
        this._push8(v);
        this._push8(v >>> 8);
        this._push8(v >>> 16);
        this._push8(v >>> 24);
    }
    float(v) {
        if (isNaN(v))
            throw new RangeError("unimplemented, NaNs");
        // Unfortunately, we cannot just view the actual buffer as a Float32Array since it needs to be 4 byte aligned
        let buffer = new ArrayBuffer(4);
        let floatView = new Float32Array(buffer);
        let int8View = new Uint8Array(buffer);
        floatView[0] = v;
        for (let byte of int8View)
            this._push8(byte);
    }

    double(v) {
        if (isNaN(v))
            throw new RangeError("unimplemented, NaNs");
        // Unfortunately, we cannot just view the actual buffer as a Float64Array since it needs to be 4 byte aligned
        let buffer = new ArrayBuffer(8);
        let floatView = new Float64Array(buffer);
        let int8View = new Uint8Array(buffer);
        floatView[0] = v;
        for (let byte of int8View)
            this._push8(byte);
    }

    varuint32(v) {
        assert.isNumber(v);
        if (v < varuint32Min || varuint32Max < v)
            throw new RangeError(`Invalid varuint32 ${v} range is [${varuint32Min}, ${varuint32Max}]`);
        while (v >= 0x80) {
            this.uint8(0x80 | (v & 0x7F));
            v >>>= 7;
        }
        this.uint8(v);
    }
    varint32(v) {
        assert.isNumber(v);
        if (v < varint32Min || varint32Max < v)
            throw new RangeError(`Invalid varint32 ${v} range is [${varint32Min}, ${varint32Max}]`);
        do {
            const b = v & 0x7F;
            v >>= 7;
            if ((v === 0 && ((b & 0x40) === 0)) || (v === -1 && ((b & 0x40) === 0x40))) {
                this.uint8(b & 0x7F);
                break;
            }
            this.uint8(0x80 | b);
        } while (true);
    }
    varuint64(v) {
        assert.isNumber(v);
        if (v < varuint32Min || varuint32Max < v)
            throw new RangeError(`unimplemented: varuint64 larger than 32-bit`);
        this.varuint32(v); // FIXME implement 64-bit var{u}int https://bugs.webkit.org/show_bug.cgi?id=163420
    }
    varint64(v) {
        assert.isNumber(v);
        if (v < varint32Min || varint32Max < v)
            throw new RangeError(`unimplemented: varint64 larger than 32-bit`);
        this.varint32(v); // FIXME implement 64-bit var{u}int https://bugs.webkit.org/show_bug.cgi?id=163420
    }
    varuint1(v) {
        if (v !== 0 && v !== 1)
            throw new RangeError(`Invalid varuint1 ${v} range is [0, 1]`);
        this.varuint32(v);
    }
    varint7(v) {
        if (v < varint7Min || varint7Max < v)
            throw new RangeError(`Invalid varint7 ${v} range is [${varint7Min}, ${varint7Max}]`);
        this.varint32(v);
    }
    varuint7(v) {
        if (v < varuint32Min || varuint7Max < v)
            throw new RangeError(`Invalid varuint7 ${v} range is [${varuint32Min}, ${varuint7Max}]`);
        this.varuint32(v);
    }
    block_type(v) {
        if (!WASM.isValidBlockType(v))
            throw new Error(`Invalid block type ${v}`);
        this.varint7(WASM.typeValue[v]);
    }
    string(str) {
        let patch = this.newPatchable("varuint32");
        for (const char of str) {
            // Encode UTF-8 2003 code points.
            const code = char.codePointAt();
            if (code <= 0x007F) {
                const utf8 = code;
                patch.uint8(utf8);
            } else if (code <= 0x07FF) {
                const utf8 = 0x80C0 | ((code & 0x7C0) >> 6) | ((code & 0x3F) << 8);
                patch.uint16(utf8);
            } else if (code <= 0xFFFF) {
                const utf8 = 0x8080E0 | ((code & 0xF000) >> 12) | ((code & 0xFC0) << 2) | ((code & 0x3F) << 16);
                patch.uint24(utf8);
            } else if (code <= 0x10FFFF) {
                const utf8 = (0x808080F0 | ((code & 0x1C0000) >> 18) | ((code & 0x3F000) >> 4) | ((code & 0xFC0) << 10) | ((code & 0x3F) << 24)) >>> 0;
                patch.uint32(utf8);
            } else
                throw new Error(`Unexpectedly large UTF-8 character code point '${char}' 0x${code.toString(16)}`);
        }
        patch.apply();
    }

    // Getters.
    getSize() { return this._used; }
    getUint8(at) {
        _getterRangeCheck(this, at, 1);
        return this._buf[at];
    }
    getUint16(at) {
        _getterRangeCheck(this, at, 2);
        return this._buf[at] | (this._buf[at + 1] << 8);
    }
    getUint24(at) {
        _getterRangeCheck(this, at, 3);
        return this._buf[at] | (this._buf[at + 1] << 8) | (this._buf[at + 2] << 16);
    }
    getUint32(at) {
        _getterRangeCheck(this, at, 4);
        return (this._buf[at] | (this._buf[at + 1] << 8) | (this._buf[at + 2] << 16) | (this._buf[at + 3] << 24)) >>> 0;
    }
    getVaruint32(at) {
        let v = 0;
        let shift = 0;
        let byte = 0;
        do {
            byte = this.getUint8(at++);
            ++read;
            v = (v | ((byte & 0x7F) << shift) >>> 0) >>> 0;
            shift += 7;
        } while ((byte & 0x80) !== 0);
        if (shift - 7 > 32) throw new RangeError(`Shifting too much at ${at}`);
        if ((shift == 35) && ((byte & 0xF0) != 0)) throw new Error(`Unexpected non-significant varuint32 bits in last byte 0x${byte.toString(16)}`);
        return { value: v, next: at };
    }
    getVarint32(at) {
        let v = 0;
        let shift = 0;
        let byte = 0;
        do {
            byte = this.getUint8(at++);
            v = (v | ((byte & 0x7F) << shift) >>> 0) >>> 0;
            shift += 7;
        } while ((byte & 0x80) !== 0);
        if (shift - 7 > 32) throw new RangeError(`Shifting too much at ${at}`);
        if ((shift == 35) && (((byte << 26) >> 30) != ((byte << 25) >> 31))) throw new Error(`Unexpected non-significant varint32 bits in last byte 0x${byte.toString(16)}`);
        if ((byte & 0x40) === 0x40) {
            const sext = shift < 32 ? 32 - shift : 0;
            v = (v << sext) >> sext;
        }
        return { value: v, next: at };
    }
    getVaruint1(at) {
        const res = this.getVaruint32(at);
        if (res.value !== 0 && res.value !== 1) throw new Error(`Expected a varuint1, got value ${res.value}`);
        return res;
    }
    getVaruint7(at) {
        const res = this.getVaruint32(at);
        if (res.value > varuint7Max) throw new Error(`Expected a varuint7, got value ${res.value}`);
        return res;
    }
    getString(at) {
        const size = this.getVaruint32(at);
        const last = size.next + size.value;
        let i = size.next;
        let str = "";
        while (i < last) {
            // Decode UTF-8 2003 code points.
            const peek = this.getUint8(i);
            let code;
            if ((peek & 0x80) === 0x0) {
                const utf8 = this.getUint8(i);
                assert.eq(utf8 & 0x80, 0x00);
                i += 1;
                code = utf8;
            } else if ((peek & 0xE0) === 0xC0) {
                const utf8 = this.getUint16(i);
                assert.eq(utf8 & 0xC0E0, 0x80C0);
                i += 2;
                code = ((utf8 & 0x1F) << 6) | ((utf8 & 0x3F00) >> 8);
            } else if ((peek & 0xF0) === 0xE0) {
                const utf8 = this.getUint24(i);
                assert.eq(utf8 & 0xC0C0F0, 0x8080E0);
                i += 3;
                code = ((utf8 & 0xF) << 12) | ((utf8 & 0x3F00) >> 2) | ((utf8 & 0x3F0000) >> 16);
            } else if ((peek & 0xF8) === 0xF0) {
                const utf8 = this.getUint32(i);
                assert.eq((utf8 & 0xC0C0C0F8) | 0, 0x808080F0 | 0);
                i += 4;
                code = ((utf8 & 0x7) << 18) | ((utf8 & 0x3F00) << 4) | ((utf8 & 0x3F0000) >> 10) | ((utf8 & 0x3F000000) >> 24);
            } else
                throw new Error(`Unexpectedly large UTF-8 initial byte 0x${peek.toString(16)}`);
            str += String.fromCodePoint(code);
        }
        if (i !== last)
            throw new Error(`String decoding read up to ${i}, expected ${last}, UTF-8 decoding was too greedy`);
        return str;
    }
};

class PatchableLowLevelBinary extends LowLevelBinary {
    constructor(type, lowLevelBinary) {
        super();
        this.type = type;
        this.target = lowLevelBinary;
        this._buffered_bytes = 0;
    }
    _push8(v) { ++this._buffered_bytes; super._push8(v); }
    apply() {
        this.target[this.type](this._buffered_bytes);
        for (let i = 0; i < this._buffered_bytes; ++i)
            this.target.uint8(this._buf[i]);
    }
};

LowLevelBinary.varuint32Min = 0;
LowLevelBinary.varint7Min = -0b1000000;
LowLevelBinary.varint7Max = 0b111111;
LowLevelBinary.varuint7Max = 0b1111111;
LowLevelBinary.varuint32Max = ((((1 << 31) >>> 0) - 1) * 2) + 1;
LowLevelBinary.varint32Min = -((1 << 31) >>> 0);
LowLevelBinary.varint32Max = ((1 << 31) - 1) >>> 0;
LowLevelBinary.varBitsMax = 5;


// Builder_WebAssemblyBinary.js
const BuildWebAssembly = {};
{
    const put = (bin, type, value) => bin[type](value);

    const putResizableLimits = (bin, initial, maximum) => {
        assert.truthy(typeof initial === "number", "We expect 'initial' to be an integer");
        let hasMaximum = 0;
        if (typeof maximum === "number") {
            hasMaximum = 1;
        } else {
            assert.truthy(typeof maximum === "undefined", "We expect 'maximum' to be an integer if it's defined");
        }

        put(bin, "varuint1", hasMaximum);
        put(bin, "varuint32", initial);
        if (hasMaximum)
            put(bin, "varuint32", maximum);
    };

    const putTable = (bin, {initial, maximum, element}) => {
        assert.truthy(WASM.isValidType(element), "We expect 'element' to be a valid type. It was: " + element);
        put(bin, "varint7", WASM.typeValue[element]);

        putResizableLimits(bin, initial, maximum);
    };

    const valueType = WASM.description.type.i32.type

    const putGlobalType = (bin, global) => {
        put(bin, valueType, WASM.typeValue[global.type]);
        put(bin, "varuint1", global.mutability);
    };

    const putOp = (bin, op) => {
        put(bin, "uint8", op.value);
        if (op.arguments.length !== 0)
            throw new Error(`Unimplemented: arguments`); // FIXME https://bugs.webkit.org/show_bug.cgi?id=162706

        switch (op.name) {
        default:
            for (let i = 0; i < op.immediates.length; ++i) {
                const type = WASM.description.opcode[op.name].immediate[i].type
                if (!bin[type])
                    throw new TypeError(`Unknown type: ${type} in op: ${op.name}`);
                put(bin, type, op.immediates[i]);
            }
            break;
        case "i32.const": {
            assert.eq(op.immediates.length, 1);
            let imm = op.immediates[0];
            // Do a static cast to make large int32s signed.
            if (imm >= 2 ** 31)
                imm = imm - (2 ** 32);
            put(bin, "varint32", imm);
            break;
        }
        case "br_table":
            put(bin, "varuint32", op.immediates.length - 1);
            for (let imm of op.immediates)
                put(bin, "varuint32", imm);
            break;
        }
    };

    const putInitExpr = (bin, expr) => {
        putOp(bin, { value: WASM.description.opcode[expr.op].value, name: expr.op, immediates: [expr.initValue], arguments: [] });
        putOp(bin, { value: WASM.description.opcode.end.value, name: "end", immediates: [], arguments: [] });
    };

    const emitters = {
        Type: (section, bin) => {
            put(bin, "varuint32", section.data.length);
            for (const entry of section.data) {
                put(bin, "varint7", WASM.typeValue["func"]);
                put(bin, "varuint32", entry.params.length);
                for (const param of entry.params)
                    put(bin, "varint7", WASM.typeValue[param]);
                if (entry.ret === "void")
                    put(bin, "varuint1", 0);
                else {
                    put(bin, "varuint1", 1);
                    put(bin, "varint7", WASM.typeValue[entry.ret]);
                }
            }
        },
        Import: (section, bin) => {
            put(bin, "varuint32", section.data.length);
            for (const entry of section.data) {
                put(bin, "string", entry.module);
                put(bin, "string", entry.field);
                put(bin, "uint8", WASM.externalKindValue[entry.kind]);
                switch (entry.kind) {
                default: throw new Error(`Implementation problem: unexpected kind ${entry.kind}`);
                case "Function": {
                    put(bin, "varuint32", entry.type);
                    break;
                }
                case "Table": {
                    putTable(bin, entry.tableDescription);
                    break;
                }
                case "Memory": {
                    let {initial, maximum} = entry.memoryDescription;
                    putResizableLimits(bin, initial, maximum);
                    break;
                };
                case "Global":
                    putGlobalType(bin, entry.globalDescription);
                    break;
                }
            }
        },

        Function: (section, bin) => {
            put(bin, "varuint32", section.data.length);
            for (const signature of section.data)
                put(bin, "varuint32", signature);
        },

        Table: (section, bin) => {
            put(bin, "varuint32", section.data.length);
            for (const {tableDescription} of section.data) {
                putTable(bin, tableDescription);
            }
        },

        Memory: (section, bin) => {
            // Flags, currently can only be [0,1]
            put(bin, "varuint1", section.data.length);
            for (const memory of section.data) {
                put(bin, "varuint32", memory.max ? 1 : 0);
                put(bin, "varuint32", memory.initial);
                if (memory.max)
                    put(bin, "varuint32", memory.max);
            }
        },

        Global: (section, bin) => {
            put(bin, "varuint32", section.data.length);
            for (const global of section.data) {
                putGlobalType(bin, global);
                putInitExpr(bin, global)
            }
        },

        Export: (section, bin) => {
            put(bin, "varuint32", section.data.length);
            for (const entry of section.data) {
                put(bin, "string", entry.field);
                put(bin, "uint8", WASM.externalKindValue[entry.kind]);
                switch (entry.kind) {
                case "Global":
                case "Function":
                case "Memory":
                case "Table":
                    put(bin, "varuint32", entry.index);
                    break;
                default: throw new Error(`Implementation problem: unexpected kind ${entry.kind}`);
                }
            }
        },
        Start: (section, bin) => {
            put(bin, "varuint32", section.data[0]);
        },
        Element: (section, bin) => {
            const data = section.data;
            put(bin, "varuint32", data.length);
            for (const {tableIndex, offset, functionIndices} of data) {
                put(bin, "varuint32", tableIndex);

                let initExpr;
                if (typeof offset === "number")
                    initExpr = {op: "i32.const", initValue: offset};
                else
                    initExpr = offset;
                putInitExpr(bin, initExpr);

                put(bin, "varuint32", functionIndices.length);
                for (const functionIndex of functionIndices)
                    put(bin, "varuint32", functionIndex);
            }
        },

        Code: (section, bin) => {
            put(bin, "varuint32", section.data.length);
            for (const func of section.data) {
                let funcBin = bin.newPatchable("varuint32");
                const localCount = func.locals.length - func.parameterCount;
                put(funcBin, "varuint32", localCount);
                for (let i = func.parameterCount; i < func.locals.length; ++i) {
                    put(funcBin, "varuint32", 1);
                    put(funcBin, "varint7", WASM.typeValue[func.locals[i]]);
                }

                for (const op of func.code)
                    putOp(funcBin, op);

                funcBin.apply();
            }
        },

        Data: (section, bin) => {
            put(bin, "varuint32", section.data.length);
            for (const datum of section.data) {
                put(bin, "varuint32", datum.index);
                // FIXME allow complex init_expr here. https://bugs.webkit.org/show_bug.cgi?id=165700
                // For now we only handle i32.const as offset.
                put(bin, "uint8", WASM.description.opcode["i32.const"].value);
                put(bin, WASM.description.opcode["i32.const"].immediate[0].type, datum.offset);
                put(bin, "uint8", WASM.description.opcode["end"].value);
                put(bin, "varuint32", datum.data.length);
                for (const byte of datum.data)
                    put(bin, "uint8", byte);
            }
        },
    };

    BuildWebAssembly.Binary = (preamble, sections) => {
        let wasmBin = new LowLevelBinary();
        for (const p of WASM.description.preamble)
            put(wasmBin, p.type, preamble[p.name]);
        for (const section of sections) {
            put(wasmBin, WASM.sectionEncodingType, section.id);
            let sectionBin = wasmBin.newPatchable("varuint32");
            const emitter = emitters[section.name];
            if (emitter)
                emitter(section, sectionBin);
            else {
                // Unknown section.
                put(sectionBin, "string", section.name);
                for (const byte of section.data)
                    put(sectionBin, "uint8", byte);
            }
            sectionBin.apply();
        }
        wasmBin.trim();
        return wasmBin;
    };
}


const LLB = LowLevelBinary;

const _toJavaScriptName = name => {
    const camelCase = name.replace(/([^a-z0-9].)/g, c => c[1].toUpperCase());
    const CamelCase = camelCase.charAt(0).toUpperCase() + camelCase.slice(1);
    return CamelCase;
};

const _isValidValue = (value, type) => {
    switch (type) {
    // We allow both signed and unsigned numbers.
    case "i32": return Math.round(value) === value && LLB.varint32Min <= value && value <= LLB.varuint32Max;
    case "i64": return true; // FIXME https://bugs.webkit.org/show_bug.cgi?id=163420 64-bit values
    case "f32": return typeof(value) === "number" && isFinite(value);
    case "f64": return typeof(value) === "number" && isFinite(value);
    default: throw new Error(`Implementation problem: unknown type ${type}`);
    }
};
const _unknownSectionId = 0;

const _normalizeFunctionSignature = (params, ret) => {
    assert.isArray(params);
    for (const p of params)
        assert.truthy(WASM.isValidValueType(p), `Type parameter ${p} needs a valid value type`);
    if (typeof(ret) === "undefined")
        ret = "void";
    assert.isNotArray(ret, `Multiple return values not supported by WebAssembly yet`);
    assert.truthy(WASM.isValidBlockType(ret), `Type return ${ret} must be valid block type`);
    return [params, ret];
};

const _errorHandlingProxyFor = builder => builder["__isProxy"] ? builder : new Proxy(builder, {
    get: (target, property, receiver) => {
        if (property === "__isProxy")
            return true;
        if (target[property] === undefined)
            throw new Error(`WebAssembly builder received unknown property '${property}'`);
        return target[property];
    }
});

const _maybeRegisterType = (builder, type) => {
    const typeSection = builder._getSection("Type");
    if (typeof(type) === "number") {
        // Type numbers already refer to the type section, no need to register them.
        if (builder._checked) {
            assert.isNotUndef(typeSection, `Can not use type ${type} if a type section is not present`);
            assert.isNotUndef(typeSection.data[type], `Type ${type} does not exist in type section`);
        }
        return type;
    }
    assert.hasObjectProperty(type, "params", `Expected type to be a number or object with 'params' and optionally 'ret' fields`);
    const [params, ret] = _normalizeFunctionSignature(type.params, type.ret);
    assert.isNotUndef(typeSection, `Can not add type if a type section is not present`);
    // Try reusing an equivalent type from the type section.
    types:
    for (let i = 0; i !== typeSection.data.length; ++i) {
        const t = typeSection.data[i];
        if (t.ret === ret && params.length === t.params.length) {
            for (let j = 0; j !== t.params.length; ++j) {
                if (params[j] !== t.params[j])
                    continue types;
            }
            type = i;
            break;
        }
    }
    if (typeof(type) !== "number") {
        // Couldn't reuse a pre-existing type, register this type in the type section.
        typeSection.data.push({ params: params, ret: ret });
        type = typeSection.data.length - 1;
    }
    return type;
};

const _importFunctionContinuation = (builder, section, nextBuilder) => {
    return (module, field, type) => {
        assert.isString(module, `Import function module should be a string, got "${module}"`);
        assert.isString(field, `Import function field should be a string, got "${field}"`);
        const typeSection = builder._getSection("Type");
        type = _maybeRegisterType(builder, type);
        section.data.push({ field: field, type: type, kind: "Function", module: module });
        // Imports also count in the function index space. Map them as objects to avoid clashing with Code functions' names.
        builder._registerFunctionToIndexSpace({ module: module, field: field });
        return _errorHandlingProxyFor(nextBuilder);
    };
};

const _importMemoryContinuation = (builder, section, nextBuilder) => {
    return (module, field, {initial, maximum}) => {
        assert.isString(module, `Import Memory module should be a string, got "${module}"`);
        assert.isString(field, `Import Memory field should be a string, got "${field}"`);
        section.data.push({module, field, kind: "Memory", memoryDescription: {initial, maximum}});
        return _errorHandlingProxyFor(nextBuilder);
    };
};

const _importTableContinuation = (builder, section, nextBuilder) => {
    return (module, field, {initial, maximum, element}) => {
        assert.isString(module, `Import Table module should be a string, got "${module}"`);
        assert.isString(field, `Import Table field should be a string, got "${field}"`);
        section.data.push({module, field, kind: "Table", tableDescription: {initial, maximum, element}});
        return _errorHandlingProxyFor(nextBuilder);
    };
};

const _exportFunctionContinuation = (builder, section, nextBuilder) => {
    return (field, index, type) => {
        assert.isString(field, `Export function field should be a string, got "${field}"`);
        const typeSection = builder._getSection("Type");
        if (typeof(type) !== "undefined") {
            // Exports can leave the type unspecified, letting the Code builder patch them up later.
            type = _maybeRegisterType(builder, type);
        }

        // We can't check much about "index" here because the Code section succeeds the Export section. More work is done at Code().End() time.
        switch (typeof(index)) {
        case "string": break; // Assume it's a function name which will be revealed in the Code section.
        case "number": break; // Assume it's a number in the "function index space".
        case "object":
            // Re-exporting an import.
            assert.hasObjectProperty(index, "module", `Re-exporting "${field}" from an import`);
            assert.hasObjectProperty(index, "field", `Re-exporting "${field}" from an import`);
            break;
        case "undefined":
            // Assume it's the same as the field (i.e. it's not being renamed).
            index = field;
            break;
        default: throw new Error(`Export section's index must be a string or a number, got ${index}`);
        }

        const correspondingImport = builder._getFunctionFromIndexSpace(index);
        const importSection = builder._getSection("Import");
        if (typeof(index) === "object") {
            // Re-exporting an import using its module+field name.
            assert.isNotUndef(correspondingImport, `Re-exporting "${field}" couldn't find import from module "${index.module}" field "${index.field}"`);
            index = correspondingImport;
            if (typeof(type) === "undefined")
                type = importSection.data[index].type;
            if (builder._checked)
                assert.eq(type, importSection.data[index].type, `Re-exporting import "${importSection.data[index].field}" as "${field}" has mismatching type`);
        } else if (typeof(correspondingImport) !== "undefined") {
            // Re-exporting an import using its index.
            let exportedImport;
            for (const i of importSection.data) {
                if (i.module === correspondingImport.module && i.field === correspondingImport.field) {
                    exportedImport = i;
                    break;
                }
            }
            if (typeof(type) === "undefined")
                type = exportedImport.type;
            if (builder._checked)
                assert.eq(type, exportedImport.type, `Re-exporting import "${exportedImport.field}" as "${field}" has mismatching type`);
        }
        section.data.push({ field: field, type: type, kind: "Function", index: index });
        return _errorHandlingProxyFor(nextBuilder);
    };
};

const _normalizeMutability = (mutability) => {
    if (mutability === "mutable")
        return 1;
    else if (mutability === "immutable")
        return 0;
    else
        throw new Error(`mutability should be either "mutable" or "immutable", but got ${mutability}`);
};

const _exportGlobalContinuation = (builder, section, nextBuilder) => {
    return (field, index) => {
        assert.isNumber(index, `Global exports only support number indices right now`);
        section.data.push({ field, kind: "Global", index });
        return _errorHandlingProxyFor(nextBuilder);
    }
};

const _exportMemoryContinuation = (builder, section, nextBuilder) => {
    return (field, index) => {
        assert.isNumber(index, `Memory exports only support number indices`);
        section.data.push({field, kind: "Memory", index});
        return _errorHandlingProxyFor(nextBuilder);
    }
};

const _exportTableContinuation = (builder, section, nextBuilder) => {
    return (field, index) => {
        assert.isNumber(index, `Table exports only support number indices`);
        section.data.push({field, kind: "Table", index});
        return _errorHandlingProxyFor(nextBuilder);
    }
};

const _importGlobalContinuation = (builder, section, nextBuilder) => {
    return () => {
        const globalBuilder = {
            End: () => nextBuilder
        };
        for (let op of WASM.description.value_type) {
            globalBuilder[_toJavaScriptName(op)] = (module, field, mutability) => {
                assert.isString(module, `Import global module should be a string, got "${module}"`);
                assert.isString(field, `Import global field should be a string, got "${field}"`);
                assert.isString(mutability, `Import global mutability should be a string, got "${mutability}"`);
                section.data.push({ globalDescription: { type: op, mutability: _normalizeMutability(mutability) }, module, field, kind: "Global" });
                return _errorHandlingProxyFor(globalBuilder);
            };
        }
        return _errorHandlingProxyFor(globalBuilder);
    };
};

const _checkStackArgs = (op, param) => {
    for (let expect of param) {
        if (WASM.isValidType(expect)) {
            // FIXME implement stack checks for arguments. https://bugs.webkit.org/show_bug.cgi?id=163421
        } else {
            // Handle our own meta-types.
            switch (expect) {
            case "addr": break; // FIXME implement addr. https://bugs.webkit.org/show_bug.cgi?id=163421
            case "any": break; // FIXME implement any. https://bugs.webkit.org/show_bug.cgi?id=163421
            case "bool": break; // FIXME implement bool. https://bugs.webkit.org/show_bug.cgi?id=163421
            case "call": break; // FIXME implement call stack argument checks based on function signature. https://bugs.webkit.org/show_bug.cgi?id=163421
            case "global": break; // FIXME implement global. https://bugs.webkit.org/show_bug.cgi?id=163421
            case "local": break; // FIXME implement local. https://bugs.webkit.org/show_bug.cgi?id=163421
            case "prev": break; // FIXME implement prev, checking for whetever the previous value was. https://bugs.webkit.org/show_bug.cgi?id=163421
            case "size": break; // FIXME implement size. https://bugs.webkit.org/show_bug.cgi?id=163421
            default: throw new Error(`Implementation problem: unhandled meta-type "${expect}" on "${op}"`);
            }
        }
    }
}

const _checkStackReturn = (op, ret) => {
    for (let expect of ret) {
        if (WASM.isValidType(expect)) {
            // FIXME implement stack checks for return. https://bugs.webkit.org/show_bug.cgi?id=163421
        } else {
            // Handle our own meta-types.
            switch (expect) {
            case "any": break;
            case "bool": break; // FIXME implement bool. https://bugs.webkit.org/show_bug.cgi?id=163421
            case "call": break; // FIXME implement call stack return check based on function signature. https://bugs.webkit.org/show_bug.cgi?id=163421
            case "control": break; // FIXME implement control. https://bugs.webkit.org/show_bug.cgi?id=163421
            case "global": break; // FIXME implement global. https://bugs.webkit.org/show_bug.cgi?id=163421
            case "local": break; // FIXME implement local. https://bugs.webkit.org/show_bug.cgi?id=163421
            case "prev": break; // FIXME implement prev, checking for whetever the parameter type was. https://bugs.webkit.org/show_bug.cgi?id=163421
            case "size": break; // FIXME implement size. https://bugs.webkit.org/show_bug.cgi?id=163421
            default: throw new Error(`Implementation problem: unhandled meta-type "${expect}" on "${op}"`);
            }
        }
    }
};

const _checkImms = (op, imms, expectedImms, ret) => {
    const minExpectedImms = expectedImms.filter(i => i.type.slice(-1) !== '*').length;
    if (minExpectedImms !== expectedImms.length)
        assert.ge(imms.length, minExpectedImms, `"${op}" expects at least ${minExpectedImms} immediate${minExpectedImms !== 1 ? 's' : ''}, got ${imms.length}`);
    else
         assert.eq(imms.length, minExpectedImms, `"${op}" expects exactly ${minExpectedImms} immediate${minExpectedImms !== 1 ? 's' : ''}, got ${imms.length}`);
    for (let idx = 0; idx !== expectedImms.length; ++idx) {
        const got = imms[idx];
        const expect = expectedImms[idx];
        switch (expect.name) {
        case "function_index":
            assert.truthy(_isValidValue(got, "i32") && got >= 0, `Invalid value on ${op}: got "${got}", expected non-negative i32`);
            // FIXME check function indices. https://bugs.webkit.org/show_bug.cgi?id=163421
            break;
        case "local_index": break; // improve checking https://bugs.webkit.org/show_bug.cgi?id=163421
        case "global_index": break; // improve checking https://bugs.webkit.org/show_bug.cgi?id=163421
        case "type_index": break; // improve checking https://bugs.webkit.org/show_bug.cgi?id=163421
        case "value":
            assert.truthy(_isValidValue(got, ret[0]), `Invalid value on ${op}: got "${got}", expected ${ret[0]}`);
            break;
        case "flags": break; // improve checking https://bugs.webkit.org/show_bug.cgi?id=163421
        case "offset": break; // improve checking https://bugs.webkit.org/show_bug.cgi?id=163421
            // Control:
        case "default_target": break; // improve checking https://bugs.webkit.org/show_bug.cgi?id=163421
        case "relative_depth": break; // improve checking https://bugs.webkit.org/show_bug.cgi?id=163421
        case "sig":
            assert.truthy(WASM.isValidBlockType(imms[idx]), `Invalid block type on ${op}: "${imms[idx]}"`);
            break;
        case "target_count": break; // improve checking https://bugs.webkit.org/show_bug.cgi?id=163421
        case "target_table": break; // improve checking https://bugs.webkit.org/show_bug.cgi?id=163421
        case "reserved": break; // improve checking https://bugs.webkit.org/show_bug.cgi?id=163421
        default: throw new Error(`Implementation problem: unhandled immediate "${expect.name}" on "${op}"`);
        }
    }
};

const _createFunctionBuilder = (func, builder, previousBuilder) => {
    let functionBuilder = {};
    for (const op in WASM.description.opcode) {
        const name = _toJavaScriptName(op);
        const value = WASM.description.opcode[op].value;
        const ret = WASM.description.opcode[op]["return"];
        const param = WASM.description.opcode[op].parameter;
        const imm = WASM.description.opcode[op].immediate;

        const checkStackArgs = builder._checked ? _checkStackArgs : () => {};
        const checkStackReturn = builder._checked ? _checkStackReturn : () => {};
        const checkImms = builder._checked ? _checkImms : () => {};

        functionBuilder[name] = (...args) => {
            let nextBuilder;
            switch (name) {
            default:
                nextBuilder = functionBuilder;
                break;
            case "End":
                nextBuilder = previousBuilder;
                break;
            case "Block":
            case "Loop":
            case "If":
                nextBuilder = _createFunctionBuilder(func, builder, functionBuilder);
                break;
            }

            // Passing a function as the argument is a way to nest blocks lexically.
            const continuation = args[args.length - 1];
            const hasContinuation = typeof(continuation) === "function";
            const imms = hasContinuation ? args.slice(0, -1) : args; // FIXME: allow passing in stack values, as-if code were a stack machine. Just check for a builder to this function, and drop. https://bugs.webkit.org/show_bug.cgi?id=163422
            checkImms(op, imms, imm, ret);
            checkStackArgs(op, param);
            checkStackReturn(op, ret);
            const stackArgs = []; // FIXME https://bugs.webkit.org/show_bug.cgi?id=162706
            func.code.push({ name: op, value: value, arguments: stackArgs, immediates: imms });
            if (hasContinuation)
                return _errorHandlingProxyFor(continuation(nextBuilder).End());
            return _errorHandlingProxyFor(nextBuilder);
        };
    };

    return _errorHandlingProxyFor(functionBuilder);
}

const _createFunction = (section, builder, previousBuilder) => {
    return (...args) => {
        const nameOffset = (typeof(args[0]) === "string") >>> 0;
        const functionName = nameOffset ? args[0] : undefined;
        let signature = args[0 + nameOffset];
        const locals = args[1 + nameOffset] === undefined ? [] : args[1 + nameOffset];
        for (const local of locals)
            assert.truthy(WASM.isValidValueType(local), `Type of local: ${local} needs to be a valid value type`);

        if (typeof(signature) === "undefined")
            signature = { params: [] };

        let type;
        let params;
        if (typeof signature === "object") {
            assert.hasObjectProperty(signature, "params", `Expect function signature to be an object with a "params" field, got "${signature}"`);
            let ret;
            ([params, ret] = _normalizeFunctionSignature(signature.params, signature.ret));
            signature = {params, ret};
            type = _maybeRegisterType(builder, signature);
        } else {
            assert.truthy(typeof signature === "number");
            const typeSection = builder._getSection("Type");
            assert.truthy(!!typeSection);
            // FIXME: we should allow indices that exceed this to be able to
            // test JSCs validator is correct. https://bugs.webkit.org/show_bug.cgi?id=165786
            assert.truthy(signature < typeSection.data.length);
            type = signature;
            signature = typeSection.data[signature];
            assert.hasObjectProperty(signature, "params", `Expect function signature to be an object with a "params" field, got "${signature}"`);
            params = signature.params;
        }

        const func = {
            name: functionName,
            type,
            signature: signature,
            locals: params.concat(locals), // Parameters are the first locals.
            parameterCount: params.length,
            code: []
        };

        const functionSection = builder._getSection("Function");
        if (functionSection)
            functionSection.data.push(func.type);

        section.data.push(func);
        builder._registerFunctionToIndexSpace(functionName);

        return _createFunctionBuilder(func, builder, previousBuilder);
    }
};

class Builder {
    constructor() {
        this.setChecked(true);
        let preamble = {};
        for (const p of WASM.description.preamble)
            preamble[p.name] = p.value;
        this.setPreamble(preamble);
        this._sections = [];
        this._functionIndexSpace = new Map();
        this._functionIndexSpaceCount = 0;
        this._registerSectionBuilders();
    }
    setChecked(checked) {
        this._checked = checked;
        return this;
    }
    setPreamble(p) {
        this._preamble = Object.assign(this._preamble || {}, p);
        return this;
    }
    _functionIndexSpaceKeyHash(obj) {
        // We don't need a full hash, just something that has a defined order for Map. Objects we insert aren't nested.
        if (typeof(obj) !== 'object')
            return obj;
        const keys = Object.keys(obj).sort();
        let entries = [];
        for (let k in keys)
            entries.push([k, obj[k]]);
        return JSON.stringify(entries);
    }
    _registerFunctionToIndexSpace(name) {
        if (typeof(name) === "undefined") {
            // Registering a nameless function still adds it to the function index space. Register it as something that can't normally be registered.
            name = {};
        }
        const value = this._functionIndexSpaceCount++;
        // Collisions are fine: we'll simply count the function and forget the previous one.
        this._functionIndexSpace.set(this._functionIndexSpaceKeyHash(name), value);
        // Map it both ways, the number space is distinct from the name space.
        this._functionIndexSpace.set(this._functionIndexSpaceKeyHash(value), name);
    }
    _getFunctionFromIndexSpace(name) {
        return this._functionIndexSpace.get(this._functionIndexSpaceKeyHash(name));
    }
    _registerSectionBuilders() {
        for (const section in WASM.description.section) {
            switch (section) {
            case "Type":
                this[section] = function() {
                    const s = this._addSection(section);
                    const builder = this;
                    const typeBuilder = {
                        End: () => builder,
                        Func: (params, ret) => {
                            [params, ret] = _normalizeFunctionSignature(params, ret);
                            s.data.push({ params: params, ret: ret });
                            return _errorHandlingProxyFor(typeBuilder);
                        },
                    };
                    return _errorHandlingProxyFor(typeBuilder);
                };
                break;

            case "Import":
                this[section] = function() {
                    const s = this._addSection(section);
                    const importBuilder = {
                        End: () => this,
                    };
                    importBuilder.Global = _importGlobalContinuation(this, s, importBuilder);
                    importBuilder.Function = _importFunctionContinuation(this, s, importBuilder);
                    importBuilder.Memory = _importMemoryContinuation(this, s, importBuilder);
                    importBuilder.Table = _importTableContinuation(this, s, importBuilder);
                    return _errorHandlingProxyFor(importBuilder);
                };
                break;

            case "Function":
                this[section] = function() {
                    const s = this._addSection(section);
                    const functionBuilder = {
                        End: () => this
                        // FIXME: add ability to add this with whatever.
                    };
                    return _errorHandlingProxyFor(functionBuilder);
                };
                break;

            case "Table":
                this[section] = function() {
                    const s = this._addSection(section);
                    const tableBuilder = {
                        End: () => this,
                        Table: ({initial, maximum, element}) => {
                            s.data.push({tableDescription: {initial, maximum, element}});
                            return _errorHandlingProxyFor(tableBuilder);
                        }
                    };
                    return _errorHandlingProxyFor(tableBuilder);
                };
                break;

            case "Memory":
                this[section] = function() {
                    const s = this._addSection(section);
                    const memoryBuilder = {
                        End: () => this,
                        InitialMaxPages: (initial, max) => {
                            s.data.push({ initial, max });
                            return _errorHandlingProxyFor(memoryBuilder);
                        }
                    };
                    return _errorHandlingProxyFor(memoryBuilder);
                };
                break;

            case "Global":
                this[section] = function() {
                    const s = this._addSection(section);
                    const globalBuilder = {
                        End: () => this,
                        GetGlobal: (type, initValue, mutability) => {
                            s.data.push({ type, op: "get_global", mutability: _normalizeMutability(mutability), initValue });
                            return _errorHandlingProxyFor(globalBuilder);
                        }
                    };
                    for (let op of WASM.description.value_type) {
                        globalBuilder[_toJavaScriptName(op)] = (initValue, mutability) => {
                            s.data.push({ type: op, op: op + ".const", mutability: _normalizeMutability(mutability), initValue });
                            return _errorHandlingProxyFor(globalBuilder);
                        };
                    }
                    return _errorHandlingProxyFor(globalBuilder);
                };
                break;

            case "Export":
                this[section] = function() {
                    const s = this._addSection(section);
                    const exportBuilder = {
                        End: () => this,
                    };
                    exportBuilder.Global = _exportGlobalContinuation(this, s, exportBuilder);
                    exportBuilder.Function = _exportFunctionContinuation(this, s, exportBuilder);
                    exportBuilder.Memory = _exportMemoryContinuation(this, s, exportBuilder);
                    exportBuilder.Table = _exportTableContinuation(this, s, exportBuilder);
                    return _errorHandlingProxyFor(exportBuilder);
                };
                break;

            case "Start":
                this[section] = function(functionIndexOrName) {
                    const s = this._addSection(section);
                    const startBuilder = {
                        End: () => this,
                    };
                    if (typeof(functionIndexOrName) !== "number" && typeof(functionIndexOrName) !== "string")
                        throw new Error(`Start section's function index  must either be a number or a string`);
                    s.data.push(functionIndexOrName);
                    return _errorHandlingProxyFor(startBuilder);
                };
                break;

            case "Element":
                this[section] = function(...args) {
                    if (args.length !== 0)
                        throw new Error("You're doing it wrong. This element does not take arguments. You must chain the call with another Element()");

                    const s = this._addSection(section);
                    const elementBuilder = {
                        End: () => this,
                        Element: ({tableIndex = 0, offset, functionIndices}) => {
                            s.data.push({tableIndex, offset, functionIndices});
                            return _errorHandlingProxyFor(elementBuilder);
                        }
                    };

                    return _errorHandlingProxyFor(elementBuilder);
                };
                break;

            case "Code":
                this[section] = function() {
                    const s = this._addSection(section);
                    const builder = this;
                    const codeBuilder =  {
                        End: () => {
                            // We now have enough information to remap the export section's "type" and "index" according to the Code section we are currently ending.
                            const typeSection = builder._getSection("Type");
                            const importSection = builder._getSection("Import");
                            const exportSection = builder._getSection("Export");
                            const startSection = builder._getSection("Start");
                            const codeSection = s;
                            if (exportSection) {
                                for (const e of exportSection.data) {
                                    if (e.kind !== "Function" || typeof(e.type) !== "undefined")
                                        continue;
                                    switch (typeof(e.index)) {
                                    default: throw new Error(`Unexpected export index "${e.index}"`);
                                    case "string": {
                                        const index = builder._getFunctionFromIndexSpace(e.index);
                                        assert.isNumber(index, `Export section contains undefined function "${e.index}"`);
                                        e.index = index;
                                    } // Fallthrough.
                                    case "number": {
                                        const index = builder._getFunctionFromIndexSpace(e.index);
                                        if (builder._checked)
                                            assert.isNotUndef(index, `Export "${e.field}" does not correspond to a defined value in the function index space`);
                                    } break;
                                    case "undefined":
                                        throw new Error(`Unimplemented: Function().End() with undefined export index`); // FIXME
                                    }
                                    if (typeof(e.type) === "undefined") {
                                        // This must be a function export from the Code section (re-exports were handled earlier).
                                        let functionIndexSpaceOffset = 0;
                                        if (importSection) {
                                            for (const {kind} of importSection.data) {
                                                if (kind === "Function")
                                                    ++functionIndexSpaceOffset;
                                            }
                                        }
                                        const functionIndex = e.index - functionIndexSpaceOffset;
                                        e.type = codeSection.data[functionIndex].type;
                                    }
                                }
                            }
                            if (startSection) {
                                const start = startSection.data[0];
                                let mapped = builder._getFunctionFromIndexSpace(start);
                                if (!builder._checked) {
                                    if (typeof(mapped) === "undefined")
                                        mapped = start; // In unchecked mode, simply use what was provided if it's nonsensical.
                                    assert.isA(start, "number"); // It can't be too nonsensical, otherwise we can't create a binary.
                                    startSection.data[0] = start;
                                } else {
                                    if (typeof(mapped) === "undefined")
                                        throw new Error(`Start section refers to non-existant function '${start}'`);
                                    if (typeof(start) === "string" || typeof(start) === "object")
                                        startSection.data[0] = mapped;
                                    // FIXME in checked mode, test that the type is acceptable for start function. We probably want _registerFunctionToIndexSpace to also register types per index. https://bugs.webkit.org/show_bug.cgi?id=165658
                                }
                            }
                            return _errorHandlingProxyFor(builder);
                        },

                    };
                    codeBuilder.Function = _createFunction(s, builder, codeBuilder);
                    return _errorHandlingProxyFor(codeBuilder);
                };
                break;

            case "Data":
                this[section] = function() {
                    const s = this._addSection(section);
                    const dataBuilder = {
                        End: () => this,
                        Segment: data => {
                            assert.isArray(data);
                            for (const datum of data) {
                                assert.isNumber(datum);
                                assert.ge(datum, 0);
                                assert.le(datum, 0xff);
                            }
                            s.data.push({ data: data, index: 0, offset: 0 });
                            let thisSegment = s.data[s.data.length - 1];
                            const segmentBuilder = {
                                End: () => dataBuilder,
                                Index: index => {
                                    assert.eq(index, 0); // Linear memory index must be zero in MVP.
                                    thisSegment.index = index;
                                    return _errorHandlingProxyFor(segmentBuilder);
                                },
                                Offset: offset => {
                                    // FIXME allow complex init_expr here. https://bugs.webkit.org/show_bug.cgi?id=165700
                                    assert.isNumber(offset);
                                    thisSegment.offset = offset;
                                    return _errorHandlingProxyFor(segmentBuilder);
                                },
                            };
                            return _errorHandlingProxyFor(segmentBuilder);
                        },
                    };
                    return _errorHandlingProxyFor(dataBuilder);
                };
                break;

            default:
                this[section] = () => { throw new Error(`Unknown section type "${section}"`); };
                break;
            }
        }

        this.Unknown = function(name) {
            const s = this._addSection(name);
            const builder = this;
            const unknownBuilder =  {
                End: () => builder,
                Byte: b => {
                    assert.eq(b & 0xFF, b, `Unknown section expected byte, got: "${b}"`);
                    s.data.push(b);
                    return _errorHandlingProxyFor(unknownBuilder);
                }
            };
            return _errorHandlingProxyFor(unknownBuilder);
        };
    }
    _addSection(nameOrNumber, extraObject) {
        const name = typeof(nameOrNumber) === "string" ? nameOrNumber : "";
        const number = typeof(nameOrNumber) === "number" ? nameOrNumber : (WASM.description.section[name] ? WASM.description.section[name].value : _unknownSectionId);
        if (this._checked) {
            // Check uniqueness.
            for (const s of this._sections)
                if (number !== _unknownSectionId)
                    assert.falsy(s.name === name && s.id === number, `Cannot have two sections with the same name "${name}" and ID ${number}`);
            // Check ordering.
            if ((number !== _unknownSectionId) && (this._sections.length !== 0)) {
                for (let i = this._sections.length - 1; i >= 0; --i) {
                    if (this._sections[i].id === _unknownSectionId)
                        continue;
                    assert.le(this._sections[i].id, number, `Bad section ordering: "${this._sections[i].name}" cannot precede "${name}"`);
                    break;
                }
            }
        }
        const s = Object.assign({ name: name, id: number, data: [] }, extraObject || {});
        this._sections.push(s);
        return s;
    }
    _getSection(nameOrNumber) {
        switch (typeof(nameOrNumber)) {
        default: throw new Error(`Implementation problem: can not get section "${nameOrNumber}"`);
        case "string":
            for (const s of this._sections)
                if (s.name === nameOrNumber)
                    return s;
            return undefined;
        case "number":
            for (const s of this._sections)
                if (s.id === nameOrNumber)
                    return s;
            return undefined;
        }
    }
    optimize() {
        // FIXME Add more optimizations. https://bugs.webkit.org/show_bug.cgi?id=163424
        return this;
    }
    json() {
        const obj = {
            preamble: this._preamble,
            section: this._sections
        };
        // JSON.stringify serializes -0.0 as 0.0.
        const replacer = (key, value) => {
            if (value === 0.0 && 1.0 / value === -Infinity)
                return "NEGATIVE_ZERO";
            return value;
        };
        return JSON.stringify(obj, replacer);
    }
    AsmJS() {
        "use asm"; // For speed.
        // FIXME Create an asm.js equivalent string which can be eval'd. https://bugs.webkit.org/show_bug.cgi?id=163425
        throw new Error("asm.js not implemented yet");
    }
    WebAssembly() { return BuildWebAssembly.Binary(this._preamble, this._sections); }
};


// ------ end builder

if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test that expected exceptions are thrown when storing a Wasm Module into a DB.");

indexedDBTest(prepareDatabase, testDatabase);
function prepareDatabase()
{
    db = event.target.result;

    evalAndLog("store = db.createObjectStore('store')");

    let builder = new Builder();
    builder = builder.Type().End()
        .Function().End()
        .Export().Function("f1").End()
        .Code()
            .Function("f1", { params: ["i32"], ret: "i32" })
                .GetLocal(0)
                .GetLocal(0)
                .I32Add();

    const count = 2;
    for (let i = 0; i < count; i++) {
        builder = builder.GetLocal(0).I32Add();
    }
    builder = builder.Return().End().End();

    module = new WebAssembly.Module(builder.WebAssembly().get());
    evalAndExpectException("store.add(module, 'key')", "DOMException.DATA_CLONE_ERR");
}

function testDatabase()
{
    finishJSTest();
}
