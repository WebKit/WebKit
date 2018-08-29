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

if (this.window) {
    this.print = (text) => {
        var span = document.createElement("pre");
        document.getElementById("messages").appendChild(span);
        span.innerText = text;
        window.scrollTo(0,document.body.scrollHeight);
    };
    this.preciseTime = () => performance.now() / 1000;
} else
    load("All.js");

function doPrep(code)
{
    return prepare("/internal/test", 0, code);
}

function doLex(code)
{
    let lexer = new Lexer("/internal/test", "native", 0, code);
    var result = [];
    for (;;) {
        let next = lexer.next();
        if (!next)
            return result;
        result.push(next);
    }
    return result;
}

function makeInt(program, value)
{
    return TypedValue.box(program.intrinsics.int, value);
}

function makeUint(program, value)
{
    return TypedValue.box(program.intrinsics.uint, value);
}

function makeUchar(program, value)
{
    return TypedValue.box(program.intrinsics.uchar, value);
}

function makeBool(program, value)
{
    return TypedValue.box(program.intrinsics.bool, value);
}

function makeFloat(program, value)
{
    return TypedValue.box(program.intrinsics.float, value);
}

function makeHalf(program, value)
{
    return TypedValue.box(program.intrinsics.half, value);
}

function makeEnum(program, enumName, value)
{
    let enumType = program.types.get(enumName);
    if (!enumType)
        throw new Error("No type named " + enumName);
    let enumMember = enumType.memberByName(value);
    if (!enumMember)
        throw new Error("No member named " + enumMember + " in " + enumType);
    return TypedValue.box(enumType, enumMember.value.unifyNode.valueForSelectedType);
}

function checkNumber(program, result, expected)
{
    if (!result.type.unifyNode.isNumber)
        throw new Error("Wrong result type; result: " + result);
    if (result.value != expected)
        throw new Error("Wrong result: " + result.value + " (expected " + expected + ")");
}

function checkInt(program, result, expected)
{
    if (!result.type.equals(program.intrinsics.int))
        throw new Error("Wrong result type; result: " + result);
    checkNumber(program, result, expected);
}

function checkEnum(program, result, expected)
{
    if (!(result.type.unifyNode instanceof EnumType))
        throw new Error("Wrong result type; result: " + result);
    if (result.value != expected)
        throw new Error("Wrong result: " + result.value + " (expected " + expected + ")");
}

function checkUint(program, result, expected)
{
    if (!result.type.equals(program.intrinsics.uint))
        throw new Error("Wrong result type: " + result.type);
    if (result.value != expected)
        throw new Error("Wrong result: " + result.value + " (expected " + expected + ")");
}

function checkUchar(program, result, expected)
{
    if (!result.type.equals(program.intrinsics.uchar))
        throw new Error("Wrong result type: " + result.type);
    if (result.value != expected)
        throw new Error("Wrong result: " + result.value + " (expected " + expected + ")");
}

function checkBool(program, result, expected)
{
    if (!result.type.equals(program.intrinsics.bool))
        throw new Error("Wrong result type: " + result.type);
    if (result.value != expected)
        throw new Error("Wrong result: " + result.value + " (expected " + expected + ")");
}

function checkFloat(program, result, expected)
{
    if (!result.type.equals(program.intrinsics.float))
        throw new Error("Wrong result type: " + result.type);
    if (result.value != expected)
        throw new Error("Wrong result: " + result.value + " (expected " + expected + ")");
}

function checkHalf(program, result, expected)
{
    if (!result.type.equals(program.intrinsics.half))
        throw new Error("Wrong result type: " + result.type);
    if (result.value != expected)
        throw new Error("Wrong result: " + result.value + " (expected " + expected + ")");
}

function checkLexerToken(result, expectedIndex, expectedKind, expectedText)
{
    if (result._index != expectedIndex)
        throw new Error("Wrong lexer index; result: " + result._index + " (expected " + expectedIndex + ")");
    if (result._kind != expectedKind)
        throw new Error("Wrong lexer kind; result: " + result._kind + " (expected " + expectedKind + ")");
    if (result._text != expectedText)
        throw new Error("Wrong lexer text; result: " + result._text + " (expected " + expectedText + ")");
}

function checkFail(callback, predicate)
{
    try {
        callback();
        throw new Error("Did not throw exception");
    } catch (e) {
        if (predicate(e)) {
            print("    Caught: " + e);
            return;
        }
        throw e;
    }
}

let tests;
let okToTest = false;

tests = new Proxy({}, {
    set(target, property, value, receiver)
    {
        if (property in target)
            throw new Error("Trying to declare duplicate test: " + property);
        target[property] = value;
        return true;
    }
});

tests.commentParsing = function() {
    let program = doPrep(`
        /* this comment
        runs over multiple lines */
        bool foo() { return true; }
    `);
    checkBool(program, callFunction(program, "foo", []), true);

    checkFail(
        () => doPrep(`
        /* this comment
        runs over multiple lines
        bool foo() { return true; }
        `),
        (e) => e instanceof WSyntaxError);
}

tests.ternaryExpression = function() {
    let program = doPrep(`
        int foo(int x)
        {
            return x < 3 ? 4 : 5;
        }
        int bar(int x)
        {
            int y = 1;
            int z = 2;
            (x < 3 ? y : z) = 7;
            return y;
        }
        int baz(int x)
        {
            return x < 10 ? 11 : x < 12 ? 14 : 15;
        }
        int quux(int x)
        {
            return 3 < 4 ? x : 5;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 767)]), 5);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 2)]), 4);
    checkInt(program, callFunction(program, "bar", [makeInt(program, 2)]), 7);
    checkInt(program, callFunction(program, "bar", [makeInt(program, 8)]), 1);
    checkInt(program, callFunction(program, "baz", [makeInt(program, 8)]), 11);
    checkInt(program, callFunction(program, "baz", [makeInt(program, 9)]), 11);
    checkInt(program, callFunction(program, "baz", [makeInt(program, 10)]), 14);
    checkInt(program, callFunction(program, "baz", [makeInt(program, 11)]), 14);
    checkInt(program, callFunction(program, "baz", [makeInt(program, 12)]), 15);
    checkInt(program, callFunction(program, "baz", [makeInt(program, 13)]), 15);
    checkInt(program, callFunction(program, "quux", [makeInt(program, 14)]), 14);
    checkFail(
        () => doPrep(`
            int foo()
            {
                int x;
                (4 < 5 ? x : 7) = 8;
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int foo()
            {
                int x;
                float y;
                return 4 < 5 ? x : y;
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int foo()
            {
                return 4 < 5 ? 6 : 7.0;
            }
        `),
        (e) => e instanceof WTypeError);
}

tests.literalBool = function() {
    let program = doPrep("bool foo() { return true; }");
    checkBool(program, callFunction(program, "foo", []), true);
}

tests.identityBool = function() {
    let program = doPrep("bool foo(bool x) { return x; }");
    checkBool(program, callFunction(program, "foo", [makeBool(program, true)]), true);
    checkBool(program, callFunction(program, "foo", [makeBool(program, false)]), false);
}

tests.intSimpleMath = function() {
    let program = doPrep("int foo(int x, int y) { return x + y; }");
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7), makeInt(program, 5)]), 12);
    program = doPrep("int foo(int x, int y) { return x - y; }");
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7), makeInt(program, 5)]), 2);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 5), makeInt(program, 7)]), -2);
    program = doPrep("int foo(int x, int y) { return x * y; }");
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7), makeInt(program, 5)]), 35);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7), makeInt(program, -5)]), -35);
    program = doPrep("int foo(int x, int y) { return x / y; }");
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7), makeInt(program, 2)]), 3);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7), makeInt(program, -2)]), -3);
}

tests.uintSimpleMath = function() {
    let program = doPrep("uint foo(uint x, uint y) { return x + y; }");
    checkUint(program, callFunction(program, "foo", [makeUint(program, 7), makeUint(program, 5)]), 12);
    program = doPrep("uint foo(uint x, uint y) { return x - y; }");
    checkUint(program, callFunction(program, "foo", [makeUint(program, 7), makeUint(program, 5)]), 2);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 5), makeUint(program, 7)]), 4294967294);
    program = doPrep("uint foo(uint x, uint y) { return x * y; }");
    checkUint(program, callFunction(program, "foo", [makeUint(program, 7), makeUint(program, 5)]), 35);
    program = doPrep("uint foo(uint x, uint y) { return x / y; }");
    checkUint(program, callFunction(program, "foo", [makeUint(program, 7), makeUint(program, 2)]), 3);
}

tests.ucharSimpleMath = function() {
    let program = doPrep("uchar foo(uchar x, uchar y) { return x + y; }");
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 7), makeUchar(program, 5)]), 12);
    program = doPrep("uchar foo(uchar x, uchar y) { return x - y; }");
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 7), makeUchar(program, 5)]), 2);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 5), makeUchar(program, 7)]), 254);
    program = doPrep("uchar foo(uchar x, uchar y) { return x * y; }");
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 7), makeUchar(program, 5)]), 35);
    program = doPrep("uchar foo(uchar x, uchar y) { return x / y; }");
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 7), makeUchar(program, 2)]), 3);
}

tests.equality = function() {
    let program = doPrep("bool foo(uint x, uint y) { return x == y; }");
    checkBool(program, callFunction(program, "foo", [makeUint(program, 7), makeUint(program, 5)]), false);
    checkBool(program, callFunction(program, "foo", [makeUint(program, 7), makeUint(program, 7)]), true);
    program = doPrep("bool foo(uchar x, uchar y) { return x == y; }");
    checkBool(program, callFunction(program, "foo", [makeUchar(program, 7), makeUchar(program, 5)]), false);
    checkBool(program, callFunction(program, "foo", [makeUchar(program, 7), makeUchar(program, 7)]), true);
    program = doPrep("bool foo(int x, int y) { return x == y; }");
    checkBool(program, callFunction(program, "foo", [makeInt(program, 7), makeInt(program, 5)]), false);
    checkBool(program, callFunction(program, "foo", [makeInt(program, 7), makeInt(program, 7)]), true);
    program = doPrep("bool foo(bool x, bool y) { return x == y; }");
    checkBool(program, callFunction(program, "foo", [makeBool(program, false), makeBool(program, true)]), false);
    checkBool(program, callFunction(program, "foo", [makeBool(program, true), makeBool(program, false)]), false);
    checkBool(program, callFunction(program, "foo", [makeBool(program, false), makeBool(program, false)]), true);
    checkBool(program, callFunction(program, "foo", [makeBool(program, true), makeBool(program, true)]), true);
}

tests.logicalNegation = function()
{
    let program = doPrep("bool foo(bool x) { return !x; }");
    checkBool(program, callFunction(program, "foo", [makeBool(program, true)]), false);
    checkBool(program, callFunction(program, "foo", [makeBool(program, false)]), true);
}

tests.notEquality = function() {
    let program = doPrep("bool foo(uint x, uint y) { return x != y; }");
    checkBool(program, callFunction(program, "foo", [makeUint(program, 7), makeUint(program, 5)]), true);
    checkBool(program, callFunction(program, "foo", [makeUint(program, 7), makeUint(program, 7)]), false);
    program = doPrep("bool foo(uchar x, uchar y) { return x != y; }");
    checkBool(program, callFunction(program, "foo", [makeUchar(program, 7), makeUchar(program, 5)]), true);
    checkBool(program, callFunction(program, "foo", [makeUchar(program, 7), makeUchar(program, 7)]), false);
    program = doPrep("bool foo(int x, int y) { return x != y; }");
    checkBool(program, callFunction(program, "foo", [makeInt(program, 7), makeInt(program, 5)]), true);
    checkBool(program, callFunction(program, "foo", [makeInt(program, 7), makeInt(program, 7)]), false);
    program = doPrep("bool foo(bool x, bool y) { return x != y; }");
    checkBool(program, callFunction(program, "foo", [makeBool(program, false), makeBool(program, true)]), true);
    checkBool(program, callFunction(program, "foo", [makeBool(program, true), makeBool(program, false)]), true);
    checkBool(program, callFunction(program, "foo", [makeBool(program, false), makeBool(program, false)]), false);
    checkBool(program, callFunction(program, "foo", [makeBool(program, true), makeBool(program, true)]), false);
}

tests.equalityTypeFailure = function()
{
    checkFail(
        () => doPrep("bool foo(int x, uint y) { return x == y; }"),
        (e) => e instanceof WTypeError && e.message.indexOf("/internal/test:1") != -1);
}

tests.generalNegation = function()
{
    let program = doPrep("bool foo(int x) { return !x; }");
    checkBool(program, callFunction(program, "foo", [makeInt(program, 7)]), false);
    checkBool(program, callFunction(program, "foo", [makeInt(program, 0)]), true);
}

tests.add1 = function() {
    let program = doPrep("int foo(int x) { return x + 1; }");
    checkInt(program, callFunction(program, "foo", [makeInt(program, 42)]), 43);
}

tests.nameResolutionFailure = function()
{
    checkFail(
        () => doPrep("int foo(int x) { return x + y; }"),
        (e) => e instanceof WTypeError && e.message.indexOf("/internal/test:1") != -1);
}

tests.simpleVariable = function()
{
    let program = doPrep(`
        int foo(int p)
        {
            int result = p;
            return result;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 42)]), 42);
}

tests.simpleAssignment = function()
{
    let program = doPrep(`
        int foo(int p)
        {
            int result;
            result = p;
            return result;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 42)]), 42);
}

tests.simpleDefault = function()
{
    let program = doPrep(`
        int foo()
        {
            int result;
            return result;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 0);
}

tests.simpleDereference = function()
{
    let program = doPrep(`
        int foo(device int* p)
        {
            return *p;
        }
    `);
    let buffer = new EBuffer(1);
    buffer.set(0, 13);
    checkInt(program, callFunction(program, "foo", [TypedValue.box(new PtrType(externalOrigin, "device", program.intrinsics.int), new EPtr(buffer, 0))]), 13);
}

tests.dereferenceStore = function()
{
    let program = doPrep(`
        void foo(device int* p)
        {
            *p = 52;
        }
    `);
    let buffer = new EBuffer(1);
    buffer.set(0, 13);
    callFunction(program, "foo", [TypedValue.box(new PtrType(externalOrigin, "device", program.intrinsics.int), new EPtr(buffer, 0))]);
    if (buffer.get(0) != 52)
        throw new Error("Expected buffer to contain 52 but it contains: " + buffer.get(0));
}

tests.simpleMakePtr = function()
{
    let program = doPrep(`
        thread int* foo()
        {
            int x = 42;
            return &x;
        }
    `);
    let result = callFunction(program, "foo", []);
    if (!result.type.isPtr)
        throw new Error("Return type is not a pointer: " + result.type);
    if (!result.type.elementType.equals(program.intrinsics.int))
        throw new Error("Return type is not a pointer to an int: " + result.type);
    if (!(result.value instanceof EPtr))
        throw new Error("Return value is not an EPtr: " + result.value);
    let value = result.value.loadValue();
    if (value != 42)
        throw new Error("Expected 42 but got: " + value);
}

tests.threadArrayLoad = function()
{
    let program = doPrep(`
        int foo(thread int[] array)
        {
            return array[0u];
        }
    `);
    let buffer = new EBuffer(1);
    buffer.set(0, 89);
    let result = callFunction(program, "foo", [TypedValue.box(new ArrayRefType(externalOrigin, "thread", program.intrinsics.int), new EArrayRef(new EPtr(buffer, 0), 1))]);
    checkInt(program, result, 89);
}

tests.threadArrayLoadIntLiteral = function()
{
    let program = doPrep(`
        int foo(thread int[] array)
        {
            return array[0];
        }
    `);
    let buffer = new EBuffer(1);
    buffer.set(0, 89);
    let result = callFunction(program, "foo", [TypedValue.box(new ArrayRefType(externalOrigin, "thread", program.intrinsics.int), new EArrayRef(new EPtr(buffer, 0), 1))]);
    checkInt(program, result, 89);
}

tests.deviceArrayLoad = function()
{
    let program = doPrep(`
        int foo(device int[] array)
        {
            return array[0u];
        }
    `);
    let buffer = new EBuffer(1);
    buffer.set(0, 89);
    let result = callFunction(program, "foo", [TypedValue.box(new ArrayRefType(externalOrigin, "device", program.intrinsics.int), new EArrayRef(new EPtr(buffer, 0), 1))]);
    checkInt(program, result, 89);
}

tests.threadArrayStore = function()
{
    let program = doPrep(`
        void foo(thread int[] array, int value)
        {
            array[0u] = value;
        }
    `);
    let buffer = new EBuffer(1);
    buffer.set(0, 15);
    let arrayRef = TypedValue.box(
        new ArrayRefType(externalOrigin, "thread", program.intrinsics.int),
        new EArrayRef(new EPtr(buffer, 0), 1));
    callFunction(program, "foo", [arrayRef, makeInt(program, 65)]);
    if (buffer.get(0) != 65)
        throw new Error("Bad value stored into buffer (expected 65): " + buffer.get(0));
    callFunction(program, "foo", [arrayRef, makeInt(program, -111)]);
    if (buffer.get(0) != -111)
        throw new Error("Bad value stored into buffer (expected -111): " + buffer.get(0));
}

tests.deviceArrayStore = function()
{
    let program = doPrep(`
        void foo(device int[] array, int value)
        {
            array[0u] = value;
        }
    `);
    let buffer = new EBuffer(1);
    buffer.set(0, 15);
    let arrayRef = TypedValue.box(
        new ArrayRefType(externalOrigin, "device", program.intrinsics.int),
        new EArrayRef(new EPtr(buffer, 0), 1));
    callFunction(program, "foo", [arrayRef, makeInt(program, 65)]);
    if (buffer.get(0) != 65)
        throw new Error("Bad value stored into buffer (expected 65): " + buffer.get(0));
    callFunction(program, "foo", [arrayRef, makeInt(program, -111)]);
    if (buffer.get(0) != -111)
        throw new Error("Bad value stored into buffer (expected -111): " + buffer.get(0));
}

tests.deviceArrayStoreIntLiteral = function()
{
    let program = doPrep(`
        void foo(device int[] array, int value)
        {
            array[0] = value;
        }
    `);
    let buffer = new EBuffer(1);
    buffer.set(0, 15);
    let arrayRef = TypedValue.box(
        new ArrayRefType(externalOrigin, "device", program.intrinsics.int),
        new EArrayRef(new EPtr(buffer, 0), 1));
    callFunction(program, "foo", [arrayRef, makeInt(program, 65)]);
    if (buffer.get(0) != 65)
        throw new Error("Bad value stored into buffer (expected 65): " + buffer.get(0));
    callFunction(program, "foo", [arrayRef, makeInt(program, -111)]);
    if (buffer.get(0) != -111)
        throw new Error("Bad value stored into buffer (expected -111): " + buffer.get(0));
}

tests.typeMismatchReturn = function()
{
    checkFail(
        () => doPrep(`
            int foo()
            {
                return 0u;
            }
        `),
        (e) => e instanceof WTypeError);
}

tests.typeMismatchVariableDecl = function()
{
    checkFail(
        () => doPrep(`
            void foo(uint x)
            {
                int y = x;
            }
        `),
        (e) => e instanceof WTypeError);
}

tests.typeMismatchAssignment = function()
{
    checkFail(
        () => doPrep(`
            void foo(uint x)
            {
                int y;
                y = x;
            }
        `),
        (e) => e instanceof WTypeError);
}

tests.typeMismatchReturnParam = function()
{
    checkFail(
        () => doPrep(`
            int foo(uint x)
            {
                return x;
            }
        `),
        (e) => e instanceof WTypeError);
}

tests.badAdd = function()
{
    checkFail(
        () => doPrep(`
            void foo(int x, uint y)
            {
                uint z = x + y;
            }
        `),
        (e) => e instanceof WTypeError && e.message.indexOf("native int operator+(int,int)") != -1);
}

tests.lexerKeyword = function()
{
    let result = doLex("ident for while 123 123u { } {asd asd{ 1a3 1.2 + 3.4 + 1. + .2 1.2f 0.f .3f && ||");
    if (result.length != 25)
        throw new Error("Lexer emitted an incorrect number of tokens (expected 23): " + result.length);
    checkLexerToken(result[0],  0,  "identifier",    "ident");
    checkLexerToken(result[1],  6,  "keyword",       "for");
    checkLexerToken(result[2],  10, "keyword",       "while");
    checkLexerToken(result[3],  16, "intLiteral",    "123");
    checkLexerToken(result[4],  20, "uintLiteral",   "123u");
    checkLexerToken(result[5],  25, "punctuation",   "{");
    checkLexerToken(result[6],  27, "punctuation",   "}");
    checkLexerToken(result[7],  29, "punctuation",   "{");
    checkLexerToken(result[8],  30, "identifier",    "asd");
    checkLexerToken(result[9],  34, "identifier",    "asd");
    checkLexerToken(result[10], 37, "punctuation",   "{");
    checkLexerToken(result[11], 39, "intLiteral",    "1");
    checkLexerToken(result[12], 40, "identifier",    "a3");
    checkLexerToken(result[13], 43, "floatLiteral",  "1.2");
    checkLexerToken(result[14], 47, "punctuation",   "+");
    checkLexerToken(result[15], 49, "floatLiteral",  "3.4");
    checkLexerToken(result[16], 53, "punctuation",   "+");
    checkLexerToken(result[17], 55, "floatLiteral",  "1.");
    checkLexerToken(result[18], 58, "punctuation",   "+");
    checkLexerToken(result[19], 60, "floatLiteral",  ".2");
    checkLexerToken(result[20], 63, "floatLiteral",  "1.2f");
    checkLexerToken(result[21], 68, "floatLiteral",  "0.f");
    checkLexerToken(result[22], 72, "floatLiteral",  ".3f");
    checkLexerToken(result[23], 76, "punctuation",   "&&");
    checkLexerToken(result[24], 79, "punctuation",   "||");
}

tests.simpleNoReturn = function()
{
    checkFail(
        () => doPrep("int foo() { }"),
        (e) => e instanceof WTypeError);
}

tests.simpleUnreachableCode = function()
{
    checkFail(
        () => doPrep(`
            void foo()
            {
                return;
                int x;
            }
        `),
        (e) => e instanceof WTypeError);
}

tests.simpleStruct = function()
{
    let program = doPrep(`
        struct Foo {
            int x;
            int y;
        }
        Foo foo(Foo foo)
        {
            Foo result;
            result.x = foo.y;
            result.y = foo.x;
            return result;
        }
    `);
    let structType = program.types.get("Foo");
    if (!structType)
        throw new Error("Did not find Foo type");
    let buffer = new EBuffer(2);
    buffer.set(0, 62);
    buffer.set(1, 24);
    let result = callFunction(program, "foo", [new TypedValue(structType, new EPtr(buffer, 0))]);
    if (!result.type.equals(structType))
        throw new Error("Wrong result type: " + result.type);
    let x = result.ePtr.get(0);
    let y = result.ePtr.get(1);
    if (x != 24)
        throw new Error("Wrong result for x: " + x + " (y = " + y + ")");
    if (y != 62)
        throw new Error("Wrong result for y: " + y + " (x + " + x + ")");
}

tests.loadNull = function()
{
    checkFail(
        () => doPrep(`
            void sink(thread int* x) { }
            void foo() { sink(*null); }
        `),
        (e) => e instanceof WTypeError && e.message.indexOf("Type passed to dereference is not a pointer: null") != -1);
}

tests.storeNull = function()
{
    checkFail(
        () => doPrep(`
            void foo() { *null = 42; }
        `),
        (e) => e instanceof WTypeError && e.message.indexOf("Type passed to dereference is not a pointer: null") != -1);
}

tests.returnNull = function()
{
    let program = doPrep(`
        thread int* foo() { return null; }
    `);
    let result = callFunction(program, "foo", []);
    if (!result.type.isPtr)
        throw new Error("Return type is not a pointer: " + result.type);
    if (!result.type.elementType.equals(program.intrinsics.int))
        throw new Error("Return type is not a pointer to an int: " + result.type);
    if (result.value != null)
        throw new Error("Return value is not null: " + result.value);
}

tests.dereferenceDefaultNull = function()
{
    let program = doPrep(`
        int foo()
        {
            thread int* p;
            return *p;
        }
    `);
    checkFail(
        () => callFunction(program, "foo", []),
        (e) => e instanceof WTrapError);
}

tests.defaultInitializedNull = function()
{
    let program = doPrep(`
        int foo()
        {
            thread int* p = null;;
            return *p;
        }
    `);
    checkFail(
        () => callFunction(program, "foo", []),
        (e) => e instanceof WTrapError);
}

tests.passNullToPtrMonomorphic = function()
{
    let program = doPrep(`
        int foo(thread int* ptr)
        {
            return *ptr;
        }
        int bar()
        {
            return foo(null);
        }
    `);
    checkFail(
        () => callFunction(program, "bar", []),
        (e) => e instanceof WTrapError);
}

tests.loadNullArrayRef = function()
{
    checkFail(
        () => doPrep(`
            void sink(thread int* x) { }
            void foo() { sink(null[0u]); }
        `),
        (e) => e instanceof WTypeError && e.message.indexOf("Cannot resolve access") != -1);
}

tests.storeNullArrayRef = function()
{
    checkFail(
        () => doPrep(`
            void foo() { null[0u] = 42; }
        `),
        (e) => e instanceof WTypeError && e.message.indexOf("Cannot resolve access") != -1);
}

tests.returnNullArrayRef = function()
{
    let program = doPrep(`
        thread int[] foo() { return null; }
    `);
    let result = callFunction(program, "foo", []);
    if (!result.type.isArrayRef)
        throw new Error("Return type is not an array reference: " + result.type);
    if (!result.type.elementType.equals(program.intrinsics.int))
        throw new Error("Return type is not an int array reference: " + result.type);
    if (result.value != null)
        throw new Error("Return value is not null: " + result.value);
}

tests.dereferenceDefaultNullArrayRef = function()
{
    let program = doPrep(`
        int foo()
        {
            thread int[] p;
            return p[0u];
        }
    `);
    checkFail(
        () => callFunction(program, "foo", []),
        (e) => e instanceof WTrapError);
}

tests.defaultInitializedNullArrayRef = function()
{
    let program = doPrep(`
        int foo()
        {
            thread int[] p = null;
            return p[0u];
        }
    `);
    checkFail(
        () => callFunction(program, "foo", []),
        (e) => e instanceof WTrapError);
}

tests.defaultInitializedNullArrayRefIntLiteral = function()
{
    let program = doPrep(`
        int foo()
        {
            thread int[] p = null;
            return p[0];
        }
    `);
    checkFail(
        () => callFunction(program, "foo", []),
        (e) => e instanceof WTrapError);
}

tests.passNullToPtrMonomorphicArrayRef = function()
{
    let program = doPrep(`
        int foo(thread int[] ptr)
        {
            return ptr[0u];
        }
        int bar()
        {
            return foo(null);
        }
    `);
    checkFail(
        () => callFunction(program, "bar", []),
        (e) => e instanceof WTrapError);
}

tests.returnIntLiteralUint = function()
{
    let program = doPrep("uint foo() { return 42; }");
    checkNumber(program, callFunction(program, "foo", []), 42);
}

tests.returnIntLiteralFloat = function()
{
    let program = doPrep("float foo() { return 42; }");
    checkNumber(program, callFunction(program, "foo", []), 42);
}

tests.badIntLiteralForInt = function()
{
    checkFail(
        () => doPrep("void foo() { int x = 3000000000; }"),
        (e) => e instanceof WSyntaxError);
}

tests.badIntLiteralForUint = function()
{
    checkFail(
        () => doPrep("void foo() { uint x = 5000000000; }"),
        (e) => e instanceof WSyntaxError);
}

tests.badIntLiteralForFloat = function()
{
    checkFail(
        () => doPrep("void foo() { float x = 5000000000000000000000000000000000000; }"),
        (e) => e instanceof WSyntaxError);
}

tests.doubleNot = function()
{
    let program = doPrep(`
        bool foo(bool x)
        {
            return !!x;
        }
    `);
    checkBool(program, callFunction(program, "foo", [makeBool(program, true)]), true);
    checkBool(program, callFunction(program, "foo", [makeBool(program, false)]), false);
}

tests.simpleRecursion = function()
{
    checkFail(
        () => doPrep(`
            void foo(int x)
            {
                foo(x);
            }
        `),
        (e) => e instanceof WTypeError);
}

tests.variableShadowing = function()
{
    let program = doPrep(`
        int foo()
        {
            int y;
            int x = 7;
            {
                int x = 8;
                y = x;
            }
            return y;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 8);
    program = doPrep(`
        int foo()
        {
            int y;
            int x = 7;
            {
                int x = 8;
            }
            y = x;
            return y;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 7);
}

tests.ifStatement = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            int y = 6;
            if (x == 7) {
                y = 8;
            }
            return y;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 3)]), 6);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 5)]), 6);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 6)]), 6);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7)]), 8);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 8)]), 6);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 9)]), 6);
}

tests.ifElseStatement = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            int y = 6;
            if (x == 7) {
                y = 8;
            } else {
                y = 9;
            }
            return y;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 3)]), 9);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 4)]), 9);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 5)]), 9);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 6)]), 9);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7)]), 8);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 8)]), 9);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 9)]), 9);
}

tests.ifElseIfStatement = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            int y = 6;
            if (x == 7) {
                y = 8;
            } else if (x == 8) {
                y = 9;
            }
            return y;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 3)]), 6);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 5)]), 6);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 6)]), 6);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7)]), 8);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 8)]), 9);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 9)]), 6);
}

tests.ifElseIfElseStatement = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            int y = 6;
            if (x == 7) {
                y = 8;
            } else if (x == 8) {
                y = 9;
            } else {
                y = 10;
            }
            return y;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 3)]), 10);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 4)]), 10);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 5)]), 10);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 6)]), 10);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7)]), 8);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 8)]), 9);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 9)]), 10);
}

tests.returnIf = function()
{
    checkFail(
        () => doPrep(`
            int foo(int x)
            {
                int y = 6;
                if (x == 7) {
                    return y;
                }
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int foo(int x)
            {
                int y = 6;
                if (x == 7) {
                    return y;
                } else {
                    y = 8;
                }
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int foo(int x)
            {
                int y = 6;
                if (x == 7) {
                    y = 8;
                } else {
                    return y;
                }
            }
        `),
        (e) => e instanceof WTypeError);
    let program = doPrep(`
        int foo(int x)
        {
            int y = 6;
            if (x == 7) {
                return 8;
            } else {
                return 10;
            }
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 3)]), 10);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 4)]), 10);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 5)]), 10);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 6)]), 10);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7)]), 8);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 8)]), 10);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 9)]), 10);
    checkFail(
        () => doPrep(`
            int foo(int x)
            {
                int y = 6;
                if (x == 7) {
                    return 8;
                } else if (x == 9) {
                    return 10;
                }
            }
        `),
        (e) => e instanceof WTypeError);
    program = doPrep(`
        int foo(int x)
        {
            int y = 6;
            if (x == 7) {
                return 8;
            } else {
                y = 9;
            }
            return y;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 3)]), 9);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 4)]), 9);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 5)]), 9);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 6)]), 9);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7)]), 8);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 8)]), 9);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 9)]), 9);
    checkFail(
        () => doPrep(`
            int foo(int x)
            {
                int y = 6;
                if (x == 7) {
                    return 8;
                } else {
                    return 10;
                }
                return 11;
            }
        `),
        (e) => e instanceof WTypeError);
    program = doPrep(`
        int foo(int x)
        {
            int y = 6;
            if (x == 7)
                int y = 8;
            return y;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7)]), 6);
}

tests.simpleWhile = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            while (x < 13)
                x = x * 2;
            return x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 1)]), 16);
}

tests.intOverloadResolution = function()
{
    let program = doPrep(`
        int foo(int) { return 1; }
        int foo(uint) { return 2; }
        int foo(float) { return 3; }
        int bar() { return foo(42); }
    `);
    checkInt(program, callFunction(program, "bar", []), 1);
}

tests.intOverloadResolutionReverseOrder = function()
{
    let program = doPrep(`
        int foo(float) { return 3; }
        int foo(uint) { return 2; }
        int foo(int) { return 1; }
        int bar() { return foo(42); }
    `);
    checkInt(program, callFunction(program, "bar", []), 1);
}

tests.break = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            while (true) {
                x = x * 2;
                if (x >= 7)
                    break;
            }
            return x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 1)]), 8);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 10)]), 20);
    program = doPrep(`
        int foo(int x)
        {
            while (true) {
                while (true) {
                    x = x * 2;
                    if (x >= 7)
                        break;
                }
                x = x - 1;
                break;
            }
            return x;
            
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 1)]), 7);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 10)]), 19);
    checkFail(
        () => doPrep(`
            int foo(int x)
            {
                while (true) {
                    {
                        break;
                    }
                    x = x + 1;
                }
                return x;
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int foo(int x)
            {
                break;
                return x;
            }
        `),
        (e) => e instanceof WTypeError);
    program = doPrep(`
            int foo(int x)
            {
                while (true) {
                    if (x == 7) {
                        break;
                    }
                    x = x + 1;
                }
                return x;
            }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 1)]), 7);
    program = doPrep(`
            int foo(int x)
            {
                while (true) {
                    break;
                }
                return x;
            }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 1)]), 1);
    program = doPrep(`
            int foo()
            {
                while (true) {
                    return 7;
                }
            }
    `);
    checkInt(program, callFunction(program, "foo", []), 7);
    checkFail(
        () => doPrep(`
            int foo(int x)
            {
                while(true) {
                    break;
                    return 7;
                }
            }
        `),
        (e) => e instanceof WTypeError);
}

tests.continue = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            while (x < 10) {
                if (x == 8) {
                    x = x + 1;
                    continue;
                }
                x = x * 2;
            }
            return x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 1)]), 18);
    checkFail(
        () => doPrep(`
            int foo(int x)
            {
                continue;
                return x;
                
            }
        `),
        (e) => e instanceof WTypeError);
}

tests.doWhile = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            int y = 7;
            do {
                y = 8;
                break;
            } while (x < 10);
            return y;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 1)]), 8);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 11)]), 8);
    program = doPrep(`
        int foo(int x)
        {
            int y = 7;
            do {
                y = 8;
                break;
            } while (y == 7);
            return y;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 1)]), 8);
    program = doPrep(`
        int foo(int x)
        {
            int sum = 0;
            do {
                if (x == 11) {
                    x = 15;
                    continue;
                }
                sum = sum + x;
                x = x + 1;
            } while (x < 13);
            return sum;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 9)]), 19);
}

tests.forLoop = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            int sum = 0;
            int i;
            for (i = 0; i < x; i = i + 1) {
                sum = sum + i;
            }
            return sum;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 5)]), 10);
    program = doPrep(`
        int foo(int x)
        {
            int sum = 0;
            for (int i = 0; i < x; i = i + 1) {
                sum = sum + i;
            }
            return sum;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 5)]), 10);
    program = doPrep(`
        int foo(int x)
        {
            int sum = 0;
            int i = 100;
            for (int i = 0; i < x; i = i + 1) {
                sum = sum + i;
            }
            return sum;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 5)]), 10);
    program = doPrep(`
        int foo(int x)
        {
            int sum = 0;
            for (int i = 0; i < x; i = i + 1) {
                if (i == 4)
                    continue;
                sum = sum + i;
            }
            return sum;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 5)]), 6);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 6)]), 11);
    program = doPrep(`
        int foo(int x)
        {
            int sum = 0;
            for (int i = 0; i < x; i = i + 1) {
                if (i == 5)
                    break;
                sum = sum + i;
            }
            return sum;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 5)]), 10);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 6)]), 10);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7)]), 10);
    program = doPrep(`
        int foo(int x)
        {
            int sum = 0;
            for (int i = 0; ; i = i + 1) {
                if (i >= x)
                    break;
                sum = sum + i;
            }
            return sum;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 5)]), 10);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 6)]), 15);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7)]), 21);
    program = doPrep(`
        int foo(int x)
        {
            int sum = 0;
            int i = 0;
            for ( ; ; i = i + 1) {
                if (i >= x)
                    break;
                sum = sum + i;
            }
            return sum;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 5)]), 10);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 6)]), 15);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7)]), 21);
    program = doPrep(`
        int foo(int x)
        {
            int sum = 0;
            int i = 0;
            for ( ; ; ) {
                if (i >= x)
                    break;
                sum = sum + i;
                i = i + 1;
            }
            return sum;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 5)]), 10);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 6)]), 15);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7)]), 21);
    checkFail(
        () => doPrep(`
            void foo(int x)
            {
                for (int i = 0; ; i = i + 1) {
                    break;
                    x = i;
                }
            }
        `),
        (e) => e instanceof WTypeError);
    program = doPrep(`
        int foo(int x)
        {
            for ( ; ; ) {
                return 7;
            }
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 3)]), 7);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 4)]), 7);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 5)]), 7);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 6)]), 7);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7)]), 7);
    checkFail(
        () => doPrep(`
            int foo(int x)
            {
                for ( ; x < 10; ) {
                    return 7;
                }
            }
        `),
        (e) => e instanceof WTypeError);
    program = doPrep(`
        int foo(int x)
        {
            for ( ; true; ) {
                return 7;
            }
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 3)]), 7);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 4)]), 7);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 5)]), 7);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 6)]), 7);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7)]), 7);
}

tests.prefixPlusPlus = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            ++x;
            return x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 64)]), 65);
}

tests.prefixPlusPlusResult = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            return ++x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 64)]), 65);
}

tests.postfixPlusPlus = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            x++;
            return x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 64)]), 65);
}

tests.postfixPlusPlusResult = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            return x++;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 64)]), 64);
}

tests.prefixMinusMinus = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            --x;
            return x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 64)]), 63);
}

tests.prefixMinusMinusResult = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            return --x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 64)]), 63);
}

tests.postfixMinusMinus = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            x--;
            return x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 64)]), 63);
}

tests.postfixMinusMinusResult = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            return x--;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 64)]), 64);
}

tests.plusEquals = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            x += 42;
            return x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 385)]), 385 + 42);
}

tests.plusEqualsResult = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            return x += 42;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 385)]), 385 + 42);
}

tests.minusEquals = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            x -= 42;
            return x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 385)]), 385 - 42);
}

tests.minusEqualsResult = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            return x -= 42;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 385)]), 385 - 42);
}

tests.timesEquals = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            x *= 42;
            return x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 385)]), 385 * 42);
}

tests.timesEqualsResult = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            return x *= 42;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 385)]), 385 * 42);
}

tests.divideEquals = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            x /= 42;
            return x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 385)]), (385 / 42) | 0);
}

tests.divideEqualsResult = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            return x /= 42;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 385)]), (385 / 42) | 0);
}

tests.twoIntLiterals = function()
{
    let program = doPrep(`
        bool foo()
        {
            return 42 == 42;
        }
    `);
    checkBool(program, callFunction(program, "foo", []), true);
}

tests.assignUintToInt = function()
{
    checkFail(
        () => doPrep(`
            void foo()
            {
                int x = 42u;
            }
        `),
        (e) => e instanceof WTypeError && e.message.indexOf("Type mismatch in variable initialization") != -1);
}

tests.buildArrayThenSumIt = function()
{
    let program = doPrep(`
        int foo()
        {
            int[42] array;
            for (uint i = 0; i < 42; i = i + 1)
                array[i] = int(i + 5);
            int result;
            for (uint i = 0; i < 42; i = i + 1)
                result = result + array[i];
            return result;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 42 * 5 + 42 * 41 / 2);
}

tests.buildArrayThenSumItUsingArrayReference = function()
{
    let program = doPrep(`
        int bar(thread int[] array)
        {
            for (uint i = 0; i < 42; i = i + 1)
                array[i] = int(i + 5);
            int result;
            for (uint i = 0; i < 42; i = i + 1)
                result = result + array[i];
            return result;
        }
        int foo()
        {
            int[42] array;
            return bar(@array);
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 42 * 5 + 42 * 41 / 2);
}

tests.overrideSubscriptStruct = function()
{
    let program = doPrep(`
        struct Foo {
            int x;
            int y;
        }
        thread int* operator&[](thread Foo* foo, uint index)
        {
            if (index == 0)
                return &foo->x;
            if (index == 1)
                return &foo->y;
            return null;
        }
        int foo()
        {
            Foo foo;
            foo.x = 498;
            foo.y = 19;
            return foo[0] + foo[1] * 3;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 498 + 19 * 3);
}

tests.overrideSubscriptStructAndDoStores = function()
{
    let program = doPrep(`
        struct Foo {
            int x;
            int y;
        }
        thread int* operator&[](thread Foo* foo, uint index)
        {
            if (index == 0)
                return &foo->x;
            if (index == 1)
                return &foo->y;
            return null;
        }
        int foo()
        {
            Foo foo;
            foo[0] = 498;
            foo[1] = 19;
            return foo.x + foo.y;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 498 + 19);
}

tests.overrideSubscriptStructAndUsePointers = function()
{
    let program = doPrep(`
        struct Foo {
            int x;
            int y;
        }
        thread int* operator&[](thread Foo* foo, uint index)
        {
            if (index == 0)
                return &foo->x;
            if (index == 1)
                return &foo->y;
            return null;
        }
        int bar(thread Foo* foo)
        {
            return (*foo)[0] + (*foo)[1];
        }
        int foo()
        {
            Foo foo;
            foo.x = 498;
            foo.y = 19;
            return bar(&foo);
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 498 + 19);
}

tests.overrideSubscriptStructAndUsePointersIncorrectly = function()
{
    checkFail(
        () => doPrep(`
            struct Foo {
                int x;
                int y;
            }
            thread int* operator&[](thread Foo* foo, uint index)
            {
                if (index == 0)
                    return &foo->x;
                if (index == 1)
                    return &foo->y;
                return null;
            }
            int bar(thread Foo* foo)
            {
                return foo[0] + foo[1];
            }
            int foo()
            {
                Foo foo;
                foo.x = 498;
                foo.y = 19;
                return bar(&foo);
            }
        `),
        (e) => e instanceof WTypeError);
}

tests.makeArrayRefFromLocal = function()
{
    let program = doPrep(`
        int bar(thread int[] ref)
        {
            return ref[0];
        }
        int foo()
        {
            int x = 48;
            return bar(@x);
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 48);
}

tests.makeArrayRefFromPointer = function()
{
    let program = doPrep(`
        int bar(thread int[] ref)
        {
            return ref[0];
        }
        int baz(thread int* ptr)
        {
            return bar(@ptr);
        }
        int foo()
        {
            int x = 48;
            return baz(&x);
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 48);
}

tests.makeArrayRefFromArrayRef = function()
{
    checkFail(
        () => doPrep(`
            int bar(thread int[] ref)
            {
                return ref[0];
            }
            int baz(thread int[] ptr)
            {
                return bar(@ptr);
            }
            int foo()
            {
                int x = 48;
                return baz(@x);
            }
        `),
        (e) => e instanceof WTypeError);
}

tests.simpleLength = function()
{
    let program = doPrep(`
        uint foo()
        {
            float[754] array;
            return (@array).length;
        }
    `);
    checkUint(program, callFunction(program, "foo", []), 754);
}

tests.nonArrayRefArrayLengthSucceed = function()
{
    let program = doPrep(`
        uint foo()
        {
            float[754] array;
            return array.length;
        }
        uint bar()
        {
            int[754] array;
            return array.length;
        }
    `);
    checkUint(program, callFunction(program, "foo", []), 754);
    checkUint(program, callFunction(program, "bar", []), 754);
}

tests.nonArrayRefArrayLengthFail = function()
{
    checkFail(
        () => doPrep(`
            thread uint* lengthPtr()
            {
                int[42] array;
                return &(array.length);
            }
        `),
        e => e instanceof WTypeError);
}

tests.assignLength = function()
{
    checkFail(
        () => doPrep(`
            void foo()
            {
                float[754] array;
                (@array).length = 42;
            }
        `),
        (e) => e instanceof WTypeError);
}

tests.assignLengthHelper = function()
{
    checkFail(
        () => doPrep(`
            void bar(thread float[] array)
            {
                array.length = 42;
            }
            void foo()
            {
                float[754] array;
                bar(@array);
            }
        `),
        (e) => e instanceof WTypeError);
}

tests.simpleGetter = function()
{
    let program = doPrep(`
        struct Foo {
            int x;
        }
        int operator.y(Foo foo)
        {
            return foo.x;
        }
        int foo()
        {
            Foo foo;
            foo.x = 7804;
            return foo.y;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 7804);
}

tests.simpleSetter = function()
{
    let program = doPrep(`
        struct Foo {
            int x;
        }
        int operator.y(Foo foo)
        {
            return foo.x;
        }
        Foo operator.y=(Foo foo, int value)
        {
            foo.x = value;
            return foo;
        }
        int foo()
        {
            Foo foo;
            foo.y = 7804;
            return foo.x;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 7804);
}

tests.nestedSubscriptLValueEmulationSimple = function()
{
    let program = doPrep(`
        struct Foo {
            int[7] array;
        }
        int operator[](Foo foo, uint index)
        {
            return foo.array[index];
        }
        Foo operator[]=(Foo foo, uint index, int value)
        {
            foo.array[index] = value;
            return foo;
        }
        uint operator.length(Foo foo)
        {
            return foo.array.length;
        }
        int sum(Foo foo)
        {
            int result = 0;
            for (uint i = foo.length; i--;)
                result += foo[i];
            return result;
        }
        struct Bar {
            Foo[6] array;
        }
        uint operator.length(Bar bar)
        {
            return bar.array.length;
        }
        Foo operator[](Bar bar, uint index)
        {
            return bar.array[index];
        }
        Bar operator[]=(Bar bar, uint index, Foo value)
        {
            bar.array[index] = value;
            return bar;
        }
        int sum(Bar bar)
        {
            int result = 0;
            for (uint i = bar.length; i--;)
                result += sum(bar[i]);
            return result;
        }
        struct Baz {
            Bar[5] array;
        }
        Bar operator[](Baz baz, uint index)
        {
            return baz.array[index];
        }
        Baz operator[]=(Baz baz, uint index, Bar value)
        {
            baz.array[index] = value;
            return baz;
        }
        uint operator.length(Baz baz)
        {
            return baz.array.length;
        }
        int sum(Baz baz)
        {
            int result = 0;
            for (uint i = baz.length; i--;)
                result += sum(baz[i]);
            return result;
        }
        void setValues(thread Baz* baz)
        {
            for (uint i = baz->length; i--;) {
                for (uint j = (*baz)[i].length; j--;) {
                    for (uint k = (*baz)[i][j].length; k--;)
                        (*baz)[i][j][k] = int(i + j + k);
                }
            }
        }
        int testSetValuesAndSum()
        {
            Baz baz;
            setValues(&baz);
            return sum(baz);
        }
        int testSetValuesMutateValuesAndSum()
        {
            Baz baz;
            setValues(&baz);
            for (uint i = baz.length; i--;) {
                for (uint j = baz[i].length; j--;) {
                    for (uint k = baz[i][j].length; k--;)
                        baz[i][j][k] *= int(k);
                }
            }
            return sum(baz);
        }
    `);
    checkInt(program, callFunction(program, "testSetValuesAndSum", []), 1575);
    checkInt(program, callFunction(program, "testSetValuesMutateValuesAndSum", []), 5565);
}

tests.operatorBool = function()
{
    let program = doPrep(`
        bool boolFromUcharFalse() { return bool(uchar(0)); }
        bool boolFromUcharTrue() { return bool(uchar(1)); }

        bool boolFromUintFalse() { return bool(uint(0)); }
        bool boolFromUintTrue() { return bool(uint(1)); }

        bool boolFromIntFalse() { return bool(int(0)); }
        bool boolFromIntTrue() { return bool(int(1)); }

        bool boolFromFloatFalse() { return bool(float(0)); }
        bool boolFromFloatTrue() { return bool(float(1)); }
    `);

    checkBool(program, callFunction(program, "boolFromUcharFalse", []), false);
    checkBool(program, callFunction(program, "boolFromUcharTrue", []), true);

    checkBool(program, callFunction(program, "boolFromUintFalse", []), false);
    checkBool(program, callFunction(program, "boolFromUintTrue", []), true);

    checkBool(program, callFunction(program, "boolFromIntFalse", []), false);
    checkBool(program, callFunction(program, "boolFromIntTrue", []), true);

    checkBool(program, callFunction(program, "boolFromFloatFalse", []), false);
    checkBool(program, callFunction(program, "boolFromFloatTrue", []), true);

    checkFail(
        () => doPrep(`
            void foo()
            {
                float3 x;
                bool r = bool(x);
            }
        `),
        e => e instanceof WTypeError);

    checkFail(
        () => doPrep(`
            void foo()
            {
                float3x3 x;
                bool r = bool(x);
            }
        `),
        e => e instanceof WTypeError);
}

tests.boolBitAnd = function()
{
    let program = doPrep(`
        bool foo(bool a, bool b)
        {
            return a & b;
        }
    `);
    checkBool(program, callFunction(program, "foo", [makeBool(program, false), makeBool(program, false)]), false);
    checkBool(program, callFunction(program, "foo", [makeBool(program, true), makeBool(program, false)]), false);
    checkBool(program, callFunction(program, "foo", [makeBool(program, false), makeBool(program, true)]), false);
    checkBool(program, callFunction(program, "foo", [makeBool(program, true), makeBool(program, true)]), true);
}

tests.boolBitOr = function()
{
    let program = doPrep(`
        bool foo(bool a, bool b)
        {
            return a | b;
        }
    `);
    checkBool(program, callFunction(program, "foo", [makeBool(program, false), makeBool(program, false)]), false);
    checkBool(program, callFunction(program, "foo", [makeBool(program, true), makeBool(program, false)]), true);
    checkBool(program, callFunction(program, "foo", [makeBool(program, false), makeBool(program, true)]), true);
    checkBool(program, callFunction(program, "foo", [makeBool(program, true), makeBool(program, true)]), true);
}

tests.boolBitXor = function()
{
    let program = doPrep(`
        bool foo(bool a, bool b)
        {
            return a ^ b;
        }
    `);
    checkBool(program, callFunction(program, "foo", [makeBool(program, false), makeBool(program, false)]), false);
    checkBool(program, callFunction(program, "foo", [makeBool(program, true), makeBool(program, false)]), true);
    checkBool(program, callFunction(program, "foo", [makeBool(program, false), makeBool(program, true)]), true);
    checkBool(program, callFunction(program, "foo", [makeBool(program, true), makeBool(program, true)]), false);
}

tests.boolBitNot = function()
{
    let program = doPrep(`
        bool foo(bool a)
        {
            return ~a;
        }
    `);
    checkBool(program, callFunction(program, "foo", [makeBool(program, false)]), true);
    checkBool(program, callFunction(program, "foo", [makeBool(program, true)]), false);
}

tests.intBitAnd = function()
{
    let program = doPrep(`
        int foo(int a, int b)
        {
            return a & b;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 1), makeInt(program, 7)]), 1);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 65535), makeInt(program, 42)]), 42);
    checkInt(program, callFunction(program, "foo", [makeInt(program, -1), makeInt(program, -7)]), -7);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 0), makeInt(program, 85732)]), 0);
}

tests.intBitOr = function()
{
    let program = doPrep(`
        int foo(int a, int b)
        {
            return a | b;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 1), makeInt(program, 7)]), 7);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 65535), makeInt(program, 42)]), 65535);
    checkInt(program, callFunction(program, "foo", [makeInt(program, -1), makeInt(program, -7)]), -1);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 0), makeInt(program, 85732)]), 85732);
}

tests.intBitXor = function()
{
    let program = doPrep(`
        int foo(int a, int b)
        {
            return a ^ b;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 1), makeInt(program, 7)]), 6);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 65535), makeInt(program, 42)]), 65493);
    checkInt(program, callFunction(program, "foo", [makeInt(program, -1), makeInt(program, -7)]), 6);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 0), makeInt(program, 85732)]), 85732);
}

tests.intBitNot = function()
{
    let program = doPrep(`
        int foo(int a)
        {
            return ~a;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 1)]), -2);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 65535)]), -65536);
    checkInt(program, callFunction(program, "foo", [makeInt(program, -1)]), 0);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 0)]), -1);
}

tests.intLShift = function()
{
    let program = doPrep(`
        int foo(int a, uint b)
        {
            return a << b;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 1), makeUint(program, 7)]), 128);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 65535), makeUint(program, 2)]), 262140);
    checkInt(program, callFunction(program, "foo", [makeInt(program, -1), makeUint(program, 5)]), -32);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 0), makeUint(program, 3)]), 0);
}

tests.intRShift = function()
{
    let program = doPrep(`
        int foo(int a, uint b)
        {
            return a >> b;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 1), makeUint(program, 7)]), 0);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 65535), makeUint(program, 2)]), 16383);
    checkInt(program, callFunction(program, "foo", [makeInt(program, -1), makeUint(program, 5)]), -1);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 0), makeUint(program, 3)]), 0);
}

tests.uintBitAnd = function()
{
    let program = doPrep(`
        uint foo(uint a, uint b)
        {
            return a & b;
        }
    `);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 1), makeUint(program, 7)]), 1);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 65535), makeUint(program, 42)]), 42);
    checkUint(program, callFunction(program, "foo", [makeUint(program, -1), makeUint(program, -7)]), 4294967289);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 0), makeUint(program, 85732)]), 0);
}

tests.uintBitOr = function()
{
    let program = doPrep(`
        uint foo(uint a, uint b)
        {
            return a | b;
        }
    `);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 1), makeUint(program, 7)]), 7);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 65535), makeUint(program, 42)]), 65535);
    checkUint(program, callFunction(program, "foo", [makeUint(program, -1), makeUint(program, -7)]), 4294967295);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 0), makeUint(program, 85732)]), 85732);
}

tests.uintBitXor = function()
{
    let program = doPrep(`
        uint foo(uint a, uint b)
        {
            return a ^ b;
        }
    `);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 1), makeUint(program, 7)]), 6);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 65535), makeUint(program, 42)]), 65493);
    checkUint(program, callFunction(program, "foo", [makeUint(program, -1), makeUint(program, -7)]), 6);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 0), makeUint(program, 85732)]), 85732);
}

tests.uintBitNot = function()
{
    let program = doPrep(`
        uint foo(uint a)
        {
            return ~a;
        }
    `);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 1)]), 4294967294);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 65535)]), 4294901760);
    checkUint(program, callFunction(program, "foo", [makeUint(program, -1)]), 0);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 0)]), 4294967295);
}

tests.uintLShift = function()
{
    let program = doPrep(`
        uint foo(uint a, uint b)
        {
            return a << b;
        }
    `);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 1), makeUint(program, 7)]), 128);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 65535), makeUint(program, 2)]), 262140);
    checkUint(program, callFunction(program, "foo", [makeUint(program, -1), makeUint(program, 5)]), 4294967264);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 0), makeUint(program, 3)]), 0);
}

tests.uintRShift = function()
{
    let program = doPrep(`
        uint foo(uint a, uint b)
        {
            return a >> b;
        }
    `);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 1), makeUint(program, 7)]), 0);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 65535), makeUint(program, 2)]), 16383);
    checkUint(program, callFunction(program, "foo", [makeUint(program, -1), makeUint(program, 5)]), 134217727);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 0), makeUint(program, 3)]), 0);
}

tests.ucharBitAnd = function()
{
    let program = doPrep(`
        uchar foo(uchar a, uchar b)
        {
            return a & b;
        }
    `);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 1), makeUchar(program, 7)]), 1);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 65535), makeUchar(program, 42)]), 42);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, -1), makeUchar(program, -7)]), 249);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 0), makeUchar(program, 85732)]), 0);
}

tests.ucharBitOr = function()
{
    let program = doPrep(`
        uchar foo(uchar a, uchar b)
        {
            return a | b;
        }
    `);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 1), makeUchar(program, 7)]), 7);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 65535), makeUchar(program, 42)]), 255);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, -1), makeUchar(program, -7)]), 255);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 0), makeUchar(program, 85732)]), 228);
}

tests.ucharBitXor = function()
{
    let program = doPrep(`
        uchar foo(uchar a, uchar b)
        {
            return a ^ b;
        }
    `);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 1), makeUchar(program, 7)]), 6);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 65535), makeUchar(program, 42)]), 213);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, -1), makeUchar(program, -7)]), 6);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 0), makeUchar(program, 85732)]), 228);
}

tests.ucharBitNot = function()
{
    let program = doPrep(`
        uchar foo(uchar a)
        {
            return ~a;
        }
    `);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 1)]), 254);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 65535)]), 0);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, -1)]), 0);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 0)]), 255);
}

tests.ucharLShift = function()
{
    let program = doPrep(`
        uchar foo(uchar a, uint b)
        {
            return a << b;
        }
    `);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 1), makeUint(program, 7)]), 128);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 65535), makeUint(program, 2)]), 252);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, -1), makeUint(program, 5)]), 224);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 0), makeUint(program, 3)]), 0);
}

tests.ucharRShift = function()
{
    let program = doPrep(`
        uchar foo(uchar a, uint b)
        {
            return a >> b;
        }
    `);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 1), makeUint(program, 7)]), 0);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 65535), makeUint(program, 2)]), 255);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, -1), makeUint(program, 5)]), 255);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 0), makeUint(program, 3)]), 0);
}

tests.floatMath = function()
{
    let program = doPrep(`
        bool foo()
        {
            return 42.5 == 42.5;
        }
        bool foo2()
        {
            return 42.5f == 42.5;
        }
        bool foo3()
        {
            return 42.5 == 42.5f;
        }
        bool foo4()
        {
            return 42.5f == 42.5f;
        }
        bool foo5()
        {
            return 42.5f == 42.5f;
        }
        float bar(float x)
        {
            return x;
        }
        float foo6()
        {
            return bar(7.5);
        }
        float foo7()
        {
            return bar(7.5f);
        }
        float foo9()
        {
            return float(7.5);
        }
        float foo10()
        {
            return float(7.5f);
        }
        float foo12()
        {
            return float(7);
        }
        float foo13()
        {
            float x = 7.5f;
            return float(x);
        }
    `);
    checkBool(program, callFunction(program, "foo", []), true);
    checkBool(program, callFunction(program, "foo2", []), true);
    checkBool(program, callFunction(program, "foo3", []), true);
    checkBool(program, callFunction(program, "foo4", []), true);
    checkBool(program, callFunction(program, "foo5", []), true);
    checkFloat(program, callFunction(program, "foo6", []), 7.5);
    checkFloat(program, callFunction(program, "foo7", []), 7.5);
    checkFloat(program, callFunction(program, "foo9", []), 7.5);
    checkFloat(program, callFunction(program, "foo10", []), 7.5);
    checkFloat(program, callFunction(program, "foo12", []), 7);
    checkFloat(program, callFunction(program, "foo13", []), 7.5);
    checkFail(
        () => doPrep(`
            int bar(int x)
            {
                return x;
            }
            int foo()
            {
                bar(4.);
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int bar(int x)
            {
                return x;
            }
            int foo()
            {
                bar(4.f);
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int bar(int x)
            {
                return x;
            }
            int foo()
            {
                bar(4.f);
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            uint bar(uint x)
            {
                return x;
            }
            int foo()
            {
                bar(4.);
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            uint bar(uint x)
            {
                return x;
            }
            int foo()
            {
                bar(4.f);
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            uint bar(uint x)
            {
                return x;
            }
            int foo()
            {
                bar(4.f);
            }
        `),
        (e) => e instanceof WTypeError);
}

tests.booleanMath = function()
{
    let program = doPrep(`
        bool foo()
        {
            return true && true;
        }
        bool foo2()
        {
            return true && false;
        }
        bool foo3()
        {
            return false && true;
        }
        bool foo4()
        {
            return false && false;
        }
        bool foo5()
        {
            return true || true;
        }
        bool foo6()
        {
            return true || false;
        }
        bool foo7()
        {
            return false || true;
        }
        bool foo8()
        {
            return false || false;
        }
    `);
    checkBool(program, callFunction(program, "foo", []), true);
    checkBool(program, callFunction(program, "foo2", []), false);
    checkBool(program, callFunction(program, "foo3", []), false);
    checkBool(program, callFunction(program, "foo4", []), false);
    checkBool(program, callFunction(program, "foo5", []), true);
    checkBool(program, callFunction(program, "foo6", []), true);
    checkBool(program, callFunction(program, "foo7", []), true);
    checkBool(program, callFunction(program, "foo8", []), false);
}

tests.booleanShortcircuiting = function()
{
    let program = doPrep(`
        bool set(thread int* ptr, int value, bool retValue) 
        { 
            *ptr = value; 
            return retValue; 
        }
        
        int andTrue()
        {
            int x;
            bool y = set(&x, 1, true) && set(&x, 2, false);
            return x; 
        }
        
        int andFalse()
        {
            int x;
            bool y = set(&x, 1, false) && set(&x, 2, false);
            return x; 
        }
        
        int orTrue()
        {
            int x;
            bool y = set(&x, 1, true) || set(&x, 2, false);
            return x; 
        }
        
        int orFalse()
        {
            int x;
            bool y = set(&x, 1, false) || set(&x, 2, false);
            return x; 
        }
    `);

    checkInt(program, callFunction(program, "andTrue", []), 2);
    checkInt(program, callFunction(program, "andFalse", []), 1);
    checkInt(program, callFunction(program, "orTrue", []), 1);
    checkInt(program, callFunction(program, "orFalse", []), 2);
}

tests.typedefArray = function()
{
    let program = doPrep(`
        typedef ArrayTypedef = int[2];
        int foo()
        {
            ArrayTypedef arrayTypedef;
            return arrayTypedef[0];
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 0);
}

tests.shaderTypes = function()
{
    checkFail(
        () => doPrep(`
            struct Foo {
                float4 x;
            }
            vertex Foo bar()
            {
                Foo result;
                result.x = float4();
                return result;
            }
            Foo foo() {
                return bar();
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            vertex float bar()
            {
                return 4.;
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct Foo {
                float4 x;
            }
            vertex Foo bar(device Foo* x)
            {
                return Foo();
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct Boo {
                float4 x;
            }
            struct Foo {
                float4 x;
                device Boo* y;
            }
            vertex Foo bar()
            {
                return Foo();
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct Foo {
                float4 x;
            }
            struct Boo {
                device Foo* y;
            }
            vertex Foo bar(Boo b)
            {
                return Foo();
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct Foo {
                float4 x;
            }
            vertex Foo bar(device Foo* x)
            {
                return Foo();
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct Foo {
                float4 x;
            }
            fragment Foo bar(Foo foo)
            {
                return Foo();
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct Foo {
                float4 x;
            }
            fragment Foo bar(device Foo* stageIn)
            {
                return Foo();
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct Boo {
                float4 x;
            }
            struct Foo {
                float4 x;
                device Boo* y;
            }
            fragment Boo bar(Foo stageIn)
            {
                return Boo();
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct Boo {
                float4 x;
            }
            struct Foo {
                float4 x;
                device Boo* y;
            }
            fragment Foo bar(Boo stageIn)
            {
                return Foo();
            }
        `),
        (e) => e instanceof WTypeError);
}

tests.vectorTypeSyntax = function()
{
    let program = doPrep(`
        int foo2()
        {
            int2 x;
            vector<int, 2> z = int2(3, 4);
            x = z;
            return x.y;
        }

        int foo3()
        {
            int3 x;
            vector<int, 3> z = int3(3, 4, 5);
            x = z;
            return x.z;
        }

        int foo4()
        {
            int4 x;
            vector<int,4> z = int4(3, 4, 5, 6);
            x = z;
            return x.w;
        }

        bool vec2OperatorCast()
        {
            int2 x = vector<int,2>(1, 2);
            vector<int, 2> y = int2(1, 2);
            return x == y && x.x == 1 && x.y == 2 && y.x == 1 && y.y == 2;
        }`);

    checkInt(program, callFunction(program, "foo2", []), 4);
    checkInt(program, callFunction(program, "foo3", []), 5);
    checkInt(program, callFunction(program, "foo4", []), 6);
    checkBool(program, callFunction(program, "vec2OperatorCast", []), true);

    program = doPrep(`
        typedef i = int;
        int foo2()
        {
            int2 x;
            vector<i, 2> z = int2(3, 4);
            x = z;
            return x.y;
        }

        bool vec2OperatorCast()
        {
            int2 x = vector<i,2>(1, 2);
            vector<i, 2> y = int2(1, 2);
            return x == y && x.x == 1 && x.y == 2 && y.x == 1 && y.y == 2;
        }`);

    checkInt(program, callFunction(program, "foo2", []), 4);
    checkBool(program, callFunction(program, "vec2OperatorCast", []), true);
}

tests.builtinVectors = function()
{
    let program = doPrep(`
        int foo()
        {
            int2 a = int2(3, 4);
            return a[0];
        }
        int foo2()
        {
            int2 a = int2(3, 4);
            int3 b = int3(a, 5);
            return b[1];
        }
        int foo3()
        {
            int3 a = int3(3, 4, 5);
            int4 b = int4(6, a);
            return b[1];
        }
        int foo4()
        {
            int2 a = int2(3, 4);
            int2 b = int2(5, 6);
            int4 c = int4(a, b);
            return c[2];
        }
        bool foo5()
        {
            int4 a = int4(3, 4, 5, 6);
            int2 b = int2(4, 5);
            int4 c = int4(3, b, 6);
            return a == c;
        }
        bool foo6()
        {
            int2 a = int2(4, 5);
            int3 b = int3(3, a);
            int3 c = int3(3, 4, 6);
            return b == c;
        }
        uint foou()
        {
            uint2 a = uint2(3, 4);
            return a[0];
        }
        uint foou2()
        {
            uint2 a = uint2(3, 4);
            uint3 b = uint3(a, 5);
            return b[1];
        }
        uint foou3()
        {
            uint3 a = uint3(3, 4, 5);
            uint4 b = uint4(6, a);
            return b[1];
        }
        uint foou4()
        {
            uint2 a = uint2(3, 4);
            uint2 b = uint2(5, 6);
            uint4 c = uint4(a, b);
            return c[2];
        }
        bool foou5()
        {
            uint4 a = uint4(3, 4, 5, 6);
            uint2 b = uint2(4, 5);
            uint4 c = uint4(3, b, 6);
            return a == c;
        }
        bool foou6()
        {
            uint2 a = uint2(4, 5);
            uint3 b = uint3(3, a);
            uint3 c = uint3(3, 4, 6);
            return b == c;
        }
        float foof()
        {
            float2 a = float2(3., 4.);
            return a[0];
        }
        float foof2()
        {
            float2 a = float2(3., 4.);
            float3 b = float3(a, 5.);
            return b[1];
        }
        float foof3()
        {
            float3 a = float3(3., 4., 5.);
            float4 b = float4(6., a);
            return b[1];
        }
        float foof4()
        {
            float2 a = float2(3., 4.);
            float2 b = float2(5., 6.);
            float4 c = float4(a, b);
            return c[2];
        }
        bool foof5()
        {
            float4 a = float4(3., 4., 5., 6.);
            float2 b = float2(4., 5.);
            float4 c = float4(3., b, 6.);
            return a == c;
        }
        bool foof6()
        {
            float2 a = float2(4., 5.);
            float3 b = float3(3., a);
            float3 c = float3(3., 4., 6.);
            return b == c;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 3);
    checkInt(program, callFunction(program, "foo2", []), 4);
    checkInt(program, callFunction(program, "foo3", []), 3);
    checkInt(program, callFunction(program, "foo4", []), 5);
    checkBool(program, callFunction(program, "foo5", []), true);
    checkBool(program, callFunction(program, "foo6", []), false);
    checkUint(program, callFunction(program, "foou", []), 3);
    checkUint(program, callFunction(program, "foou2", []), 4);
    checkUint(program, callFunction(program, "foou3", []), 3);
    checkUint(program, callFunction(program, "foou4", []), 5);
    checkBool(program, callFunction(program, "foou5", []), true);
    checkBool(program, callFunction(program, "foou6", []), false);
    checkFloat(program, callFunction(program, "foof", []), 3);
    checkFloat(program, callFunction(program, "foof2", []), 4);
    checkFloat(program, callFunction(program, "foof3", []), 3);
    checkFloat(program, callFunction(program, "foof4", []), 5);
    checkBool(program, callFunction(program, "foof5", []), true);
    checkBool(program, callFunction(program, "foof6", []), false);
}

tests.builtinVectorGetters = function()
{
    const typeNames = [ "uint", "int", "float" ];
    const sizes = [ 2, 3, 4 ];
    const elements = [ "x", "y", "z", "w" ];
    const initializerList = [ 1, 2, 3, 4 ];

    let tests = [];
    let src = "";
    for (let typeName of typeNames) {
        for (let size of sizes) {
            for (let i = 0; i < size; i++) {
                const functionName = `${typeName}${size}${elements[i]}`;
                src += `${typeName} ${functionName}()
                        {
                            ${typeName}${size} x = ${typeName}${size}(${initializerList.slice(0, size).join(", ")});
                            return x.${elements[i]};
                        }
                        `;
                tests.push({ type: typeName, name: functionName, expectation: initializerList[i] });
            }
        }
    }

    let program = doPrep(src);
    const checkFuncs = {
        "uint": checkUint,
        "int": checkInt,
        "float": checkFloat
    };
    for (let test of tests) {
        const checkFunc = checkFuncs[test.type];
        checkFunc(program, callFunction(program, test.name, [], []), test.expectation);
    }
}

tests.builtinVectorSetters = function()
{
    const typeNames = [ "uint", "int", "float" ];
    const sizes = [ 2, 3, 4 ];
    const elements = [ "x", "y", "z", "w" ];
    const initializerList = [ 1, 2, 3, 4 ];

    let tests = [];
    let src = "";
    for (let typeName of typeNames) {
        for (let size of sizes) {
            for (let i = 0; i < size; i++) {
                const functionName = `${typeName}${size}${elements[i]}`;
                src += `${typeName} ${functionName}()
                        {
                            ${typeName}${size} x = ${typeName}${size}(${initializerList.slice(0, size).join(", ")});
                            x.${elements[i]} = 34;
                            return x.${elements[i]};
                        }
                        `;
                tests.push({ type: typeName, name: functionName, expectation: 34 });
            }
        }
    }

    let program = doPrep(src);
    const checkFuncs = {
        "uint": checkUint,
        "int": checkInt,
        "float": checkFloat
    };
    for (let test of tests) {
        const checkFunc = checkFuncs[test.type];
        checkFunc(program, callFunction(program, test.name, [], []), test.expectation);
    }
}

tests.builtinVectorIndexSetters = function()
{
    const typeNames = [ "uint", "int", "float" ];
    const sizes = [ 2, 3, 4 ];
    const elements = [ "x", "y", "z", "w" ];
    const initializerList = [ 1, 2, 3, 4 ];

    let tests = [];
    let src = "";
    for (let typeName of typeNames) {
        for (let size of sizes) {
            for (let i = 0; i < size; i++) {
                const functionName = `${typeName}${size}${elements[i]}`;
                src += `${typeName} ${functionName}()
                        {
                            ${typeName}${size} x = ${typeName}${size}(${initializerList.slice(0, size).join(", ")});
                            x[${i}] = 34;
                            return x[${i}];
                        }
                        `;
                tests.push({ type: typeName, name: functionName, expectation: 34 });
            }
        }
    }

    let program = doPrep(src);
    const checkFuncs = {
        "uint": checkUint,
        "int": checkInt,
        "float": checkFloat
    };
    for (let test of tests) {
        const checkFunc = checkFuncs[test.type];
        checkFunc(program, callFunction(program, test.name, [], []), test.expectation);
    }
}

tests.simpleEnum = function()
{
    let program = doPrep(`
        enum Foo {
            War,
            Famine,
            Pestilence,
            Death
        }
        Foo war()
        {
            return Foo.War;
        }
        Foo famine()
        {
            return Foo.Famine;
        }
        Foo pestilence()
        {
            return Foo.Pestilence;
        }
        Foo death()
        {
            return Foo.Death;
        }
        bool equals(Foo a, Foo b)
        {
            return a == b;
        }
        bool notEquals(Foo a, Foo b)
        {
            return a != b;
        }
        bool testSimpleEqual()
        {
            return equals(Foo.War, Foo.War);
        }
        bool testAnotherEqual()
        {
            return equals(Foo.Pestilence, Foo.Pestilence);
        }
        bool testNotEqual()
        {
            return equals(Foo.Famine, Foo.Death);
        }
        bool testSimpleNotEqual()
        {
            return notEquals(Foo.War, Foo.War);
        }
        bool testAnotherNotEqual()
        {
            return notEquals(Foo.Pestilence, Foo.Pestilence);
        }
        bool testNotNotEqual()
        {
            return notEquals(Foo.Famine, Foo.Death);
        }
        int intWar()
        {
            return int(war());
        }
        int intFamine()
        {
            return int(famine());
        }
        int intPestilence()
        {
            return int(pestilence());
        }
        int intDeath()
        {
            return int(death());
        }
        int warValue()
        {
            return war().value;
        }
        int famineValue()
        {
            return famine().value;
        }
        int pestilenceValue()
        {
            return pestilence().value;
        }
        int deathValue()
        {
            return death().value;
        }
        int warValueLiteral()
        {
            return Foo.War.value;
        }
        int famineValueLiteral()
        {
            return Foo.Famine.value;
        }
        int pestilenceValueLiteral()
        {
            return Foo.Pestilence.value;
        }
        int deathValueLiteral()
        {
            return Foo.Death.value;
        }
        Foo intWarBackwards()
        {
            return Foo(intWar());
        }
        Foo intFamineBackwards()
        {
            return Foo(intFamine());
        }
        Foo intPestilenceBackwards()
        {
            return Foo(intPestilence());
        }
        Foo intDeathBackwards()
        {
            return Foo(intDeath());
        }
    `);
    checkEnum(program, callFunction(program, "war", []), 0);
    checkEnum(program, callFunction(program, "famine", []), 1);
    checkEnum(program, callFunction(program, "pestilence", []), 2);
    checkEnum(program, callFunction(program, "death", []), 3);
    checkBool(program, callFunction(program, "testSimpleEqual", []), true);
    checkBool(program, callFunction(program, "testAnotherEqual", []), true);
    checkBool(program, callFunction(program, "testNotEqual", []), false);
    checkBool(program, callFunction(program, "testSimpleNotEqual", []), false);
    checkBool(program, callFunction(program, "testAnotherNotEqual", []), false);
    checkBool(program, callFunction(program, "testNotNotEqual", []), true);
    checkInt(program, callFunction(program, "intWar", []), 0);
    checkInt(program, callFunction(program, "intFamine", []), 1);
    checkInt(program, callFunction(program, "intPestilence", []), 2);
    checkInt(program, callFunction(program, "intDeath", []), 3);
    checkInt(program, callFunction(program, "warValue", []), 0);
    checkInt(program, callFunction(program, "famineValue", []), 1);
    checkInt(program, callFunction(program, "pestilenceValue", []), 2);
    checkInt(program, callFunction(program, "deathValue", []), 3);
    checkInt(program, callFunction(program, "warValueLiteral", []), 0);
    checkInt(program, callFunction(program, "famineValueLiteral", []), 1);
    checkInt(program, callFunction(program, "pestilenceValueLiteral", []), 2);
    checkInt(program, callFunction(program, "deathValueLiteral", []), 3);
    checkEnum(program, callFunction(program, "intWarBackwards", []), 0);
    checkEnum(program, callFunction(program, "intFamineBackwards", []), 1);
    checkEnum(program, callFunction(program, "intPestilenceBackwards", []), 2);
    checkEnum(program, callFunction(program, "intDeathBackwards", []), 3);
}

tests.enumWithManualValues = function()
{
    let program = doPrep(`
        enum Foo {
            War = 72,
            Famine = 0,
            Pestilence = 23,
            Death = -42
        }
        Foo war()
        {
            return Foo.War;
        }
        Foo famine()
        {
            return Foo.Famine;
        }
        Foo pestilence()
        {
            return Foo.Pestilence;
        }
        Foo death()
        {
            return Foo.Death;
        }
    `);
    checkEnum(program, callFunction(program, "war", []), 72);
    checkEnum(program, callFunction(program, "famine", []), 0);
    checkEnum(program, callFunction(program, "pestilence", []), 23);
    checkEnum(program, callFunction(program, "death", []), -42);
}

tests.enumWithoutZero = function()
{
    checkFail(
        () => doPrep(`
            enum Foo {
                War = 72,
                Famine = 64,
                Pestilence = 23,
                Death = -42
            }
        `),
        e => e instanceof WTypeError);
}

tests.enumDuplicates = function()
{
    checkFail(
        () => doPrep(`
            enum Foo {
                War = -42,
                Famine = 0,
                Pestilence = 23,
                Death = -42
            }
        `),
        e => e instanceof WTypeError);
}

tests.enumWithSomeManualValues = function()
{
    let program = doPrep(`
        enum Foo {
            War = 72,
            Famine,
            Pestilence = 0,
            Death
        }
        Foo war()
        {
            return Foo.War;
        }
        Foo famine()
        {
            return Foo.Famine;
        }
        Foo pestilence()
        {
            return Foo.Pestilence;
        }
        Foo death()
        {
            return Foo.Death;
        }
    `);
    checkEnum(program, callFunction(program, "war", []), 72);
    checkEnum(program, callFunction(program, "famine", []), 73);
    checkEnum(program, callFunction(program, "pestilence", []), 0);
    checkEnum(program, callFunction(program, "death", []), 1);
}

tests.trap = function()
{
    let program = doPrep(`
        int foo()
        {
            trap;
        }
        int foo2(int x)
        {
            if (x == 3)
                trap;
            return 4;
        }
        struct Bar {
            int3 x;
            float y;
        }
        Bar foo3()
        {
            trap;
        }
    `);
    checkFail(
        () => callFunction(program, "foo", []),
        e => e instanceof WTrapError);
    checkInt(program, callFunction(program, "foo2", [makeInt(program, 1)]), 4);
    checkFail(
        () => callFunction(program, "foo2", [makeInt(program, 3)]),
        e => e instanceof WTrapError);
    checkFail(
        () => callFunction(program, "foo3", []),
        e => e instanceof WTrapError);
}

/*
tests.swizzle = function()
{
    let program = doPrep(`
        float foo() {
            float4 bar = float4(3., 4., 5., 6.);
            float3 baz = bar.zzx;
            return baz.z;
        }
        float foo2() {
            float4 bar = float4(3., 4., 5., 6.);
            float3 baz = bar.wyz;
            return baz.x;
        }
        float foo3() {
            float3 bar = float3(3., 4., 5.);
            float2 baz = bar.yz;
            float4 quix = baz.yyxx;
            return quix.z;
        }
    `);
    checkFloat(program, callFunction(program, "foo", []), 3);
    checkFloat(program, callFunction(program, "foo2", []), 6);
    checkFloat(program, callFunction(program, "foo3", []), 4);
}
*/

tests.enumWithExplicitIntBase = function()
{
    let program = doPrep(`
        enum Foo : int {
            War,
            Famine,
            Pestilence,
            Death
        }
        Foo war()
        {
            return Foo.War;
        }
        Foo famine()
        {
            return Foo.Famine;
        }
        Foo pestilence()
        {
            return Foo.Pestilence;
        }
        Foo death()
        {
            return Foo.Death;
        }
        bool equals(Foo a, Foo b)
        {
            return a == b;
        }
        bool notEquals(Foo a, Foo b)
        {
            return a != b;
        }
        bool testSimpleEqual()
        {
            return equals(Foo.War, Foo.War);
        }
        bool testAnotherEqual()
        {
            return equals(Foo.Pestilence, Foo.Pestilence);
        }
        bool testNotEqual()
        {
            return equals(Foo.Famine, Foo.Death);
        }
        bool testSimpleNotEqual()
        {
            return notEquals(Foo.War, Foo.War);
        }
        bool testAnotherNotEqual()
        {
            return notEquals(Foo.Pestilence, Foo.Pestilence);
        }
        bool testNotNotEqual()
        {
            return notEquals(Foo.Famine, Foo.Death);
        }
        int intWar()
        {
            return int(war());
        }
        int intFamine()
        {
            return int(famine());
        }
        int intPestilence()
        {
            return int(pestilence());
        }
        int intDeath()
        {
            return int(death());
        }
        int warValue()
        {
            return war().value;
        }
        int famineValue()
        {
            return famine().value;
        }
        int pestilenceValue()
        {
            return pestilence().value;
        }
        int deathValue()
        {
            return death().value;
        }
        int warValueLiteral()
        {
            return Foo.War.value;
        }
        int famineValueLiteral()
        {
            return Foo.Famine.value;
        }
        int pestilenceValueLiteral()
        {
            return Foo.Pestilence.value;
        }
        int deathValueLiteral()
        {
            return Foo.Death.value;
        }
        Foo intWarBackwards()
        {
            return Foo(intWar());
        }
        Foo intFamineBackwards()
        {
            return Foo(intFamine());
        }
        Foo intPestilenceBackwards()
        {
            return Foo(intPestilence());
        }
        Foo intDeathBackwards()
        {
            return Foo(intDeath());
        }
    `);
    checkEnum(program, callFunction(program, "war", []), 0);
    checkEnum(program, callFunction(program, "famine", []), 1);
    checkEnum(program, callFunction(program, "pestilence", []), 2);
    checkEnum(program, callFunction(program, "death", []), 3);
    checkBool(program, callFunction(program, "testSimpleEqual", []), true);
    checkBool(program, callFunction(program, "testAnotherEqual", []), true);
    checkBool(program, callFunction(program, "testNotEqual", []), false);
    checkBool(program, callFunction(program, "testSimpleNotEqual", []), false);
    checkBool(program, callFunction(program, "testAnotherNotEqual", []), false);
    checkBool(program, callFunction(program, "testNotNotEqual", []), true);
    checkInt(program, callFunction(program, "intWar", []), 0);
    checkInt(program, callFunction(program, "intFamine", []), 1);
    checkInt(program, callFunction(program, "intPestilence", []), 2);
    checkInt(program, callFunction(program, "intDeath", []), 3);
    checkInt(program, callFunction(program, "warValue", []), 0);
    checkInt(program, callFunction(program, "famineValue", []), 1);
    checkInt(program, callFunction(program, "pestilenceValue", []), 2);
    checkInt(program, callFunction(program, "deathValue", []), 3);
    checkInt(program, callFunction(program, "warValueLiteral", []), 0);
    checkInt(program, callFunction(program, "famineValueLiteral", []), 1);
    checkInt(program, callFunction(program, "pestilenceValueLiteral", []), 2);
    checkInt(program, callFunction(program, "deathValueLiteral", []), 3);
    checkEnum(program, callFunction(program, "intWarBackwards", []), 0);
    checkEnum(program, callFunction(program, "intFamineBackwards", []), 1);
    checkEnum(program, callFunction(program, "intPestilenceBackwards", []), 2);
    checkEnum(program, callFunction(program, "intDeathBackwards", []), 3);
}

tests.enumWithUintBase = function()
{
    let program = doPrep(`
        enum Foo : uint {
            War,
            Famine,
            Pestilence,
            Death
        }
        Foo war()
        {
            return Foo.War;
        }
        Foo famine()
        {
            return Foo.Famine;
        }
        Foo pestilence()
        {
            return Foo.Pestilence;
        }
        Foo death()
        {
            return Foo.Death;
        }
        bool equals(Foo a, Foo b)
        {
            return a == b;
        }
        bool notEquals(Foo a, Foo b)
        {
            return a != b;
        }
        bool testSimpleEqual()
        {
            return equals(Foo.War, Foo.War);
        }
        bool testAnotherEqual()
        {
            return equals(Foo.Pestilence, Foo.Pestilence);
        }
        bool testNotEqual()
        {
            return equals(Foo.Famine, Foo.Death);
        }
        bool testSimpleNotEqual()
        {
            return notEquals(Foo.War, Foo.War);
        }
        bool testAnotherNotEqual()
        {
            return notEquals(Foo.Pestilence, Foo.Pestilence);
        }
        bool testNotNotEqual()
        {
            return notEquals(Foo.Famine, Foo.Death);
        }
        uint uintWar()
        {
            return uint(war());
        }
        uint uintFamine()
        {
            return uint(famine());
        }
        uint uintPestilence()
        {
            return uint(pestilence());
        }
        uint uintDeath()
        {
            return uint(death());
        }
        uint warValue()
        {
            return war().value;
        }
        uint famineValue()
        {
            return famine().value;
        }
        uint pestilenceValue()
        {
            return pestilence().value;
        }
        uint deathValue()
        {
            return death().value;
        }
        uint warValueLiteral()
        {
            return Foo.War.value;
        }
        uint famineValueLiteral()
        {
            return Foo.Famine.value;
        }
        uint pestilenceValueLiteral()
        {
            return Foo.Pestilence.value;
        }
        uint deathValueLiteral()
        {
            return Foo.Death.value;
        }
        Foo uintWarBackwards()
        {
            return Foo(uintWar());
        }
        Foo uintFamineBackwards()
        {
            return Foo(uintFamine());
        }
        Foo uintPestilenceBackwards()
        {
            return Foo(uintPestilence());
        }
        Foo uintDeathBackwards()
        {
            return Foo(uintDeath());
        }
    `);
    checkEnum(program, callFunction(program, "war", []), 0);
    checkEnum(program, callFunction(program, "famine", []), 1);
    checkEnum(program, callFunction(program, "pestilence", []), 2);
    checkEnum(program, callFunction(program, "death", []), 3);
    checkBool(program, callFunction(program, "testSimpleEqual", []), true);
    checkBool(program, callFunction(program, "testAnotherEqual", []), true);
    checkBool(program, callFunction(program, "testNotEqual", []), false);
    checkBool(program, callFunction(program, "testSimpleNotEqual", []), false);
    checkBool(program, callFunction(program, "testAnotherNotEqual", []), false);
    checkBool(program, callFunction(program, "testNotNotEqual", []), true);
    checkUint(program, callFunction(program, "uintWar", []), 0);
    checkUint(program, callFunction(program, "uintFamine", []), 1);
    checkUint(program, callFunction(program, "uintPestilence", []), 2);
    checkUint(program, callFunction(program, "uintDeath", []), 3);
    checkUint(program, callFunction(program, "warValue", []), 0);
    checkUint(program, callFunction(program, "famineValue", []), 1);
    checkUint(program, callFunction(program, "pestilenceValue", []), 2);
    checkUint(program, callFunction(program, "deathValue", []), 3);
    checkUint(program, callFunction(program, "warValueLiteral", []), 0);
    checkUint(program, callFunction(program, "famineValueLiteral", []), 1);
    checkUint(program, callFunction(program, "pestilenceValueLiteral", []), 2);
    checkUint(program, callFunction(program, "deathValueLiteral", []), 3);
    checkEnum(program, callFunction(program, "uintWarBackwards", []), 0);
    checkEnum(program, callFunction(program, "uintFamineBackwards", []), 1);
    checkEnum(program, callFunction(program, "uintPestilenceBackwards", []), 2);
    checkEnum(program, callFunction(program, "uintDeathBackwards", []), 3);
}

tests.enumFloatBase = function()
{
    checkFail(
        () => doPrep(`
            enum Foo : float {
                Bar
            }
        `),
        e => e instanceof WTypeError);
}

tests.enumPtrBase = function()
{
    checkFail(
        () => doPrep(`
            enum Foo : thread int* {
                Bar
            }
        `),
        e => e instanceof WTypeError);
}

tests.enumArrayRefBase = function()
{
    checkFail(
        () => doPrep(`
            enum Foo : thread int[] {
                Bar
            }
        `),
        e => e instanceof WTypeError);
}

tests.emptyStruct = function()
{
    let program = doPrep(`
        struct Thingy { }
        int foo()
        {
            Thingy thingy;
            return 46;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 46);
}

tests.enumStructBase = function()
{
    checkFail(
        () => doPrep(`
            struct Thingy { }
            enum Foo : Thingy {
                Bar
            }
        `),
        e => e instanceof WTypeError);
}

tests.enumNoMembers = function()
{
    checkFail(
        () => doPrep(`
            enum Foo { }
        `),
        e => e instanceof WTypeError);
}

tests.simpleSwitch = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            switch (x) {
            case 767:
                return 27;
            case 69:
                return 7624;
            default:
                return 49;
            }
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 767)]), 27);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 69)]), 7624);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 0)]), 49);
}

tests.exhaustiveUcharSwitch = function()
{
    let text = "float foo(uchar x) { switch (uchar(x)) {"
    for (let i = 0; i <= 0xff; ++i)
        text += "case " + i + ": return " + i * 1.5 + ";";
    text += "} }";
    let program = doPrep(text);
    for (let i = 0; i < 0xff; ++i)
        checkFloat(program, callFunction(program, "foo", [makeUchar(program, i)]), i * 1.5);
}

tests.notQuiteExhaustiveUcharSwitch = function()
{
    let text = "float foo(uchar x) { switch (uchar(x)) {"
    for (let i = 0; i <= 0xfe; ++i)
        text += "case " + i + ": return " + i * 1.5 + ";";
    text += "} }";
    checkFail(() => doPrep(text), e => e instanceof WTypeError);
}

tests.notQuiteExhaustiveUcharSwitchWithDefault = function()
{
    let text = "float foo(uchar x) { switch (uchar(x)) {"
    for (let i = 0; i <= 0xfe; ++i)
        text += "case " + i + ": return " + i * 1.5 + ";";
    text += "default: return " + 0xff * 1.5 + ";";
    text += "} }";
    let program = doPrep(text);
    for (let i = 0; i < 0xff; ++i)
        checkFloat(program, callFunction(program, "foo", [makeUchar(program, i)]), i * 1.5);
}

tests.switchFallThrough = function()
{
    // FIXME: This might become an error in future versions.
    // https://bugs.webkit.org/show_bug.cgi?id=177172
    let program = doPrep(`
        int foo(int x)
        {
            int result = 0;
            switch (x) {
            case 767:
                result += 27;
            case 69:
                result += 7624;
            default:
                result += 49;
            }
            return result;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 767)]), 27 + 7624 + 49);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 69)]), 7624 + 49);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 0)]), 49);
}

tests.switchBreak = function()
{
    let program = doPrep(`
        int foo(int x)
        {
            int result = 0;
            switch (x) {
            case 767:
                result += 27;
                break;
            case 69:
                result += 7624;
                break;
            default:
                result += 49;
                break;
            }
            return result;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 767)]), 27);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 69)]), 7624);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 0)]), 49);
}

tests.enumSwitchBreakExhaustive = function()
{
    let program = doPrep(`
        enum Foo {
            A, B, C
        }
        int foo(Foo x)
        {
            int result = 0;
            switch (x) {
            case Foo.A:
                result += 27;
                break;
            case Foo.B:
                result += 7624;
                break;
            case Foo.C:
                result += 49;
                break;
            }
            return result;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeEnum(program, "Foo", "A")]), 27);
    checkInt(program, callFunction(program, "foo", [makeEnum(program, "Foo", "B")]), 7624);
    checkInt(program, callFunction(program, "foo", [makeEnum(program, "Foo", "C")]), 49);
}

tests.enumSwitchBreakNotQuiteExhaustive = function()
{
    checkFail(
        () => doPrep(`
            enum Foo {
                A, B, C, D
            }
            int foo(Foo x)
            {
                int result = 0;
                switch (x) {
                case Foo.A:
                    result += 27;
                    break;
                case Foo.B:
                    result += 7624;
                    break;
                case Foo.C:
                    result += 49;
                    break;
                }
                return result;
            }
        `),
        e => e instanceof WTypeError);
}

tests.enumSwitchBreakNotQuiteExhaustiveWithDefault = function()
{
    let program = doPrep(`
        enum Foo {
            A, B, C
        }
        int foo(Foo x)
        {
            int result = 0;
            switch (x) {
            case Foo.A:
                result += 27;
                break;
            case Foo.B:
                result += 7624;
                break;
            default:
                result += 49;
                break;
            }
            return result;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeEnum(program, "Foo", "A")]), 27);
    checkInt(program, callFunction(program, "foo", [makeEnum(program, "Foo", "B")]), 7624);
    checkInt(program, callFunction(program, "foo", [makeEnum(program, "Foo", "C")]), 49);
}

tests.simpleRecursiveStruct = function()
{
    checkFail(
        () => doPrep(`
            struct Foo {
                Foo foo;
            }
        `),
        e => e instanceof WTypeError);
}

tests.mutuallyRecursiveStruct = function()
{
    checkFail(
        () => doPrep(`
            struct Foo {
                Bar bar;
            }
            struct Bar {
                Foo foo;
            }
        `),
        e => e instanceof WTypeError);
}

tests.mutuallyRecursiveStructWithPointersBroken = function()
{
    let program = doPrep(`
        struct Foo {
            thread Bar* bar;
            int foo;
        }
        struct Bar {
            thread Foo* foo;
            int bar;
        }
        int foo()
        {
            Foo foo;
            Bar bar;
            foo.foo = 564;
            bar.bar = 53;
            return foo.bar->bar - bar.foo->foo;
        }
    `);
    checkFail(
        () => checkInt(program, callFunction(program, "foo", []), -511),
        e => e instanceof WTrapError);
}

tests.mutuallyRecursiveStructWithPointers = function()
{
    let program = doPrep(`
        struct Foo {
            thread Bar* bar;
            int foo;
        }
        struct Bar {
            thread Foo* foo;
            int bar;
        }
        int foo()
        {
            Foo foo;
            Bar bar;
            foo.bar = &bar;
            bar.foo = &foo;
            foo.foo = 564;
            bar.bar = 53;
            return foo.bar->bar - bar.foo->foo;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), -511);
}

tests.linkedList = function()
{
    let program = doPrep(`
        struct Node {
            thread Node* next;
            int value;
        }
        int foo()
        {
            Node x, y, z;
            x.next = &y;
            y.next = &z;
            x.value = 1;
            y.value = 2;
            z.value = 3;
            return x.next->next->value;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 3);
}

tests.pointerToPointer = function()
{
    let program = doPrep(`
        int foo()
        {
            int x;
            thread int* p = &x;
            thread int** pp = &p;
            int*thread*thread qq = pp;
            int result = 0;
            x = 42;
            *p = 76;
            result += x;
            **pp = 39;
            result += x;
            **qq = 83;
            result += x;
            return result;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 76 + 39 + 83);
}

tests.arrayRefToArrayRef = function()
{
    let program = doPrep(`
        int foo()
        {
            int x;
            thread int[] p = @x;
            thread int[][] pp = @p;
            int[]thread[]thread qq = pp;
            int result = 0;
            x = 42;
            p[0] = 76;
            result += x;
            pp[0][0] = 39;
            result += x;
            qq[0][0] = 83;
            result += x;
            return result;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 76 + 39 + 83);
}

tests.pointerGetter = function()
{
    checkFail(
        () => doPrep(`
            int operator.foo(device int*)
            {
                return 543;
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator.foo(thread int*)
            {
                return 543;
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator.foo(threadgroup int*)
            {
                return 543;
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator.foo(constant int*)
            {
                return 543;
            }
        `),
        e => e instanceof WTypeError);
}

tests.loneSetter = function()
{
    checkFail(
        () => doPrep(`
            int operator.foo=(int, int)
            {
                return 543;
            }
        `),
        e => e instanceof WTypeError);
}

tests.setterWithMismatchedType = function()
{
    checkFail(
        () => doPrep(`
            float operator.foo(int)
            {
                return 5.43;
            }
            int operator.foo=(int, int)
            {
                return 543;
            }
        `),
        e => e instanceof WTypeError);
}

tests.setterWithMatchedType = function()
{
    doPrep(`
        int operator.foo(int)
        {
            return 5;
        }
        int operator.foo=(int, int)
        {
            return 543;
        }
    `);
}

tests.operatorWithoutUninferrableTypeVariable = function()
{
    let program = doPrep(`
        struct Foo {
            int x;
        }
        Foo operator+(Foo a, Foo b)
        {
            Foo result;
            result.x = a.x + b.x;
            return result;
        }
        int foo()
        {
            Foo a;
            a.x = 645;
            Foo b;
            b.x = -35;
            return (a + b).x;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 645 - 35);
}

tests.incWrongArgumentLength = function()
{
    checkFail(
        () => doPrep(`
            int operator++() { return 32; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator++(int, int) { return 76; }
        `),
        e => e instanceof WTypeError);
}

tests.decWrongArgumentLength = function()
{
    checkFail(
        () => doPrep(`
            int operator--() { return 32; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator--(int, int) { return 76; }
        `),
        e => e instanceof WTypeError);
}

tests.incWrongTypes = function()
{
    checkFail(
        () => doPrep(`
            int operator++(float) { return 32; }
        `),
        e => e instanceof WTypeError);
}

tests.decWrongTypes = function()
{
    checkFail(
        () => doPrep(`
            int operator--(float) { return 32; }
        `),
        e => e instanceof WTypeError);
}

tests.plusWrongArgumentLength = function()
{
    checkFail(
        () => doPrep(`
            int operator+() { return 32; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator+(int, int, int) { return 76; }
        `),
        e => e instanceof WTypeError);
}

tests.minusWrongArgumentLength = function()
{
    checkFail(
        () => doPrep(`
            int operator-() { return 32; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator-(int, int, int) { return 76; }
        `),
        e => e instanceof WTypeError);
}

tests.timesWrongArgumentLength = function()
{
    checkFail(
        () => doPrep(`
            int operator*() { return 32; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator*(int) { return 534; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator*(int, int, int) { return 76; }
        `),
        e => e instanceof WTypeError);
}

tests.divideWrongArgumentLength = function()
{
    checkFail(
        () => doPrep(`
            int operator/() { return 32; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator/(int) { return 534; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator/(int, int, int) { return 76; }
        `),
        e => e instanceof WTypeError);
}

tests.moduloWrongArgumentLength = function()
{
    checkFail(
        () => doPrep(`
            int operator%() { return 32; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator%(int) { return 534; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator%(int, int, int) { return 76; }
        `),
        e => e instanceof WTypeError);
}

tests.bitAndWrongArgumentLength = function()
{
    checkFail(
        () => doPrep(`
            int operator&() { return 32; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator&(int) { return 534; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator&(int, int, int) { return 76; }
        `),
        e => e instanceof WTypeError);
}

tests.bitOrWrongArgumentLength = function()
{
    checkFail(
        () => doPrep(`
            int operator|() { return 32; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator|(int) { return 534; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator|(int, int, int) { return 76; }
        `),
        e => e instanceof WTypeError);
}

tests.bitXorWrongArgumentLength = function()
{
    checkFail(
        () => doPrep(`
            int operator^() { return 32; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator^(int) { return 534; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator^(int, int, int) { return 76; }
        `),
        e => e instanceof WTypeError);
}

tests.lShiftWrongArgumentLength = function()
{
    checkFail(
        () => doPrep(`
            int operator<<() { return 32; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator<<(int) { return 534; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator<<(int, int, int) { return 76; }
        `),
        e => e instanceof WTypeError);
}

tests.rShiftWrongArgumentLength = function()
{
    checkFail(
        () => doPrep(`
            int operator>>() { return 32; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator>>(int) { return 534; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator>>(int, int, int) { return 76; }
        `),
        e => e instanceof WTypeError);
}

tests.bitNotWrongArgumentLength = function()
{
    checkFail(
        () => doPrep(`
            int operator~() { return 32; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator~(int, int) { return 534; }
        `),
        e => e instanceof WTypeError);
}

tests.equalsWrongArgumentLength = function()
{
    checkFail(
        () => doPrep(`
            bool operator==() { return true; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            bool operator==(int) { return true; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            bool operator==(int, int, int) { return true; }
        `),
        e => e instanceof WTypeError);
}

tests.lessThanWrongArgumentLength = function()
{
    checkFail(
        () => doPrep(`
            bool operator<() { return true; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            bool operator<(int) { return true; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            bool operator<(int, int, int) { return true; }
        `),
        e => e instanceof WTypeError);
}

tests.lessEqualWrongArgumentLength = function()
{
    checkFail(
        () => doPrep(`
            bool operator<=() { return true; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            bool operator<=(int) { return true; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            bool operator<=(int, int, int) { return true; }
        `),
        e => e instanceof WTypeError);
}

tests.greaterWrongArgumentLength = function()
{
    checkFail(
        () => doPrep(`
            bool operator>() { return true; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            bool operator>(int) { return true; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            bool operator>(int, int, int) { return true; }
        `),
        e => e instanceof WTypeError);
}

tests.greaterEqualWrongArgumentLength = function()
{
    checkFail(
        () => doPrep(`
            bool operator>=() { return true; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            bool operator>=(int) { return true; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            bool operator>=(int, int, int) { return true; }
        `),
        e => e instanceof WTypeError);
}

tests.equalsWrongReturnType = function()
{
    checkFail(
        () => doPrep(`
            int operator==(int a, int b) { return a + b; }
        `),
        e => e instanceof WTypeError);
}

tests.notEqualsOverload = function()
{
    checkFail(
        () => doPrep(`
            struct Foo { }
            bool operator!=(Foo, Foo) { return true; }
        `),
        e => e instanceof WSyntaxError);
}

tests.lessThanWrongReturnType = function()
{
    checkFail(
        () => doPrep(`
            int operator<(int a, int b) { return a + b; }
        `),
        e => e instanceof WTypeError);
}

tests.lessEqualWrongReturnType = function()
{
    checkFail(
        () => doPrep(`
            int operator<=(int a, int b) { return a + b; }
        `),
        e => e instanceof WTypeError);
}

tests.greaterThanWrongReturnType = function()
{
    checkFail(
        () => doPrep(`
            int operator>(int a, int b) { return a + b; }
        `),
        e => e instanceof WTypeError);
}

tests.greaterEqualWrongReturnType = function()
{
    checkFail(
        () => doPrep(`
            int operator>=(int a, int b) { return a + b; }
        `),
        e => e instanceof WTypeError);
}

tests.dotOperatorWrongArgumentLength = function()
{
    checkFail(
        () => doPrep(`
            int operator.foo() { return 42; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct Foo { }
            int operator.foo(Foo, int) { return 42; }
        `),
        e => e instanceof WTypeError);
}

tests.dotOperatorSetterWrongArgumentLength = function()
{
    checkFail(
        () => doPrep(`
            struct Foo { }
            Foo operator.foo=() { return 42; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct Foo { }
            Foo operator.foo=(Foo) { return 42; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct Foo { }
            Foo operator.foo=(Foo, int, int) { return 42; }
        `),
        e => e instanceof WTypeError);
}

tests.loneSetterPointer = function()
{
    checkFail(
        () => doPrep(`
            thread int* operator.foo=(thread int* ptr, int)
            {
                return ptr;
            }
        `),
        e => e instanceof WTypeError);
}

tests.setterWithNoGetterOverload = function()
{
    checkFail(
        () => doPrep(`
            struct Foo { }
            struct Bar { }
            int operator.foo(Foo)
            {
                return 534;
            }
            Bar operator.foo=(Bar, int)
            {
                return Bar();
            }
        `),
        e => e instanceof WTypeError);
}

tests.setterWithNoGetterOverloadFixed = function()
{
    doPrep(`
        struct Bar { }
        int operator.foo(Bar)
        {
            return 534;
        }
        Bar operator.foo=(Bar, int)
        {
            return Bar();
        }
    `);
}

tests.anderWithNothingWrong = function()
{
    let program = doPrep(`
        struct Foo {
            int x;
        }
        thread int* operator&.foo(thread Foo* foo)
        {
            return &foo->x;
        }
        int foo()
        {
            Foo x;
            x.x = 13;
            return x.foo;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 13);
}

tests.anderWithWrongNumberOfArguments = function()
{
    checkFail(
        () => doPrep(`
            thread int* operator&.foo()
            {
                int x;
                return &x;
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct Foo {
                int x;
            }
            thread int* operator&.foo(thread Foo* foo, int blah)
            {
                return &foo->x;
            }
        `),
        e => e instanceof WTypeError);
}

tests.anderDoesntReturnPointer = function()
{
    checkFail(
        () => doPrep(`
            struct Foo {
                int x;
            }
            int operator&.foo(thread Foo* foo)
            {
                return foo->x;
            }
        `),
        e => e instanceof WTypeError);
}

tests.anderDoesntTakeReference = function()
{
    checkFail(
        () => doPrep(`
            struct Foo {
                int x;
            }
            thread int* operator&.foo(Foo foo)
            {
                return &foo.x;
            }
        `),
        e => e instanceof WTypeError);
}

tests.anderWithArrayRef = function()
{
    let program = doPrep(`
        struct Foo {
            int x;
        }
        thread int* operator&.foo(thread Foo[] foo)
        {
            return &foo[0].x;
        }
        int foo()
        {
            Foo x;
            x.x = 13;
            return (@x).foo;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 13);
}

tests.anderWithBadIndex = function()
{
    checkFail(() => doPrep(`
        int foo(thread int[] x) { return x[-1]; }
    `), e => e instanceof Error);

    checkFail(() => doPrep(`
        int foo(thread int[] x) { return x[1.f]; }
    `), e => e instanceof Error);

    checkFail(() => doPrep(`
        int foo(thread int[] x, int y) { return x[y]; }
    `), e => e instanceof Error);

    checkFail(() => doPrep(`
        int foo(thread int[] x, float y) { return x[y]; }
    `), e => e instanceof Error);
}

tests.pointerIndexGetter = function()
{
    checkFail(
        () => doPrep(`
            int operator[](device int*, uint)
            {
                return 543;
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator[](thread int*, uint)
            {
                return 543;
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator[](threadgroup int*, uint)
            {
                return 543;
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator[](constant int*, uint)
            {
                return 543;
            }
        `),
        e => e instanceof WTypeError);
}

tests.loneIndexSetter = function()
{
    checkFail(
        () => doPrep(`
            int operator[]=(int, uint, int)
            {
                return 543;
            }
        `),
        e => e instanceof WTypeError);
}

tests.notLoneIndexSetter = function()
{
    doPrep(`
        int operator[](int, uint)
        {
            return 65;
        }
        int operator[]=(int, uint, int)
        {
            return 543;
        }
    `);
}

tests.indexSetterWithMismatchedType = function()
{
    checkFail(
        () => doPrep(`
            float operator[](int, uint)
            {
                return 5.43;
            }
            int operator[]=(int, uint, int)
            {
                return 543;
            }
        `),
        e => e instanceof WTypeError && e.message.indexOf("Setter and getter must agree on value type") != -1);
}

tests.indexOperatorWrongArgumentLength = function()
{
    checkFail(
        () => doPrep(`
            int operator[]() { return 42; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator[](int) { return 42; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator[](int, int, int) { return 42; }
        `),
        e => e instanceof WTypeError);
}

tests.indexOperatorSetterWrongArgumentLength = function()
{
    checkFail(
        () => doPrep(`
            int operator[]=() { return 42; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator[]=(int) { return 42; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator[]=(int, int) { return 42; }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            int operator[]=(int, int, int, int) { return 42; }
        `),
        e => e instanceof WTypeError);
}

tests.loneIndexSetterPointer = function()
{
    checkFail(
        () => doPrep(`
            thread int* operator[]=(thread int* ptr, uint, int)
            {
                return ptr;
            }
        `),
        e => e instanceof WTypeError);
}

tests.indexSetterWithNoGetterOverload = function()
{
    checkFail(
        () => doPrep(`
            struct Foo { }
            struct Bar { }
            int operator[](Foo, uint)
            {
                return 534;
            }
            Bar operator[]=(Bar, uint, int)
            {
                return Bar();
            }
        `),
        e => e instanceof WTypeError);
}

tests.indexSetterWithNoGetterOverloadFixed = function()
{
    doPrep(`
        struct Bar { }
        int operator[](Bar, uint)
        {
            return 534;
        }
        Bar operator[]=(Bar, uint, int)
        {
            return Bar();
        }
    `);
}

tests.indexAnderWithNothingWrong = function()
{
    let program = doPrep(`
        struct Foo {
            int x;
        }
        thread int* operator&[](thread Foo* foo, uint)
        {
            return &foo->x;
        }
        int foo()
        {
            Foo x;
            x.x = 13;
            return x[666];
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 13);
}

tests.indexAnderWithWrongNumberOfArguments = function()
{
    checkFail(
        () => doPrep(`
            thread int* operator&[]()
            {
                int x;
                return &x;
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct Foo {
                int x;
            }
            thread int* operator&[](thread Foo* foo)
            {
                return &foo->x;
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct Foo {
                int x;
            }
            thread int* operator&[](thread Foo* foo, uint, uint)
            {
                return &foo->x;
            }
        `),
        e => e instanceof WTypeError);
}

tests.indexAnderDoesntReturnPointer = function()
{
    checkFail(
        () => doPrep(`
            struct Foo {
                int x;
            }
            int operator&[](thread Foo* foo, uint)
            {
                return foo->x;
            }
        `),
        e => e instanceof WTypeError && e.message.indexOf("Return type of ander is not a pointer") != -1);
}

tests.indexAnderDoesntTakeReference = function()
{
    checkFail(
        () => doPrep(`
            struct Foo {
                int x;
            }
            thread int* operator&[](Foo foo, uint)
            {
                return &foo.x;
            }
        `),
        e => e instanceof WTypeError && e.message.indexOf("Parameter to ander is not a reference") != -1);
}

tests.indexAnderWithArrayRef = function()
{
    let program = doPrep(`
        struct Foo {
            int x;
        }
        thread int* operator&[](thread Foo[] array, float index)
        {
            return &array[uint(index + 1)].x;
        }
        int foo()
        {
            Foo x;
            x.x = 13;
            return (@x)[float(-1)];
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 13);
}

tests.devicePtrPtr = function()
{
    checkFail(
        () => doPrep(`
            void foo()
            {
                device int** p;
            }
        `),
        e => e instanceof WTypeError && e.message.indexOf("Illegal pointer to non-primitive type: int* device* device") != -1);
}

tests.threadgroupPtrPtr = function()
{
    checkFail(
        () => doPrep(`
            void foo()
            {
                threadgroup int** p;
            }
        `),
        e => e instanceof WTypeError && e.message.indexOf("Illegal pointer to non-primitive type: int* threadgroup* threadgroup") != -1);
}

tests.constantPtrPtr = function()
{
    checkFail(
        () => doPrep(`
            void foo()
            {
                constant int** p;
            }
        `),
        e => e instanceof WTypeError && e.message.indexOf("Illegal pointer to non-primitive type: int* constant* constant") != -1);
}

tests.andReturnedArrayRef = function()
{
    let program = doPrep(`
        thread int[] getArray()
        {
            int[10] x;
            x[5] = 354;
            return @x;
        }
        int foo()
        {
            thread int* ptr = &getArray()[5];
            return *ptr;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 354);
}

tests.casts = function()
{
    let program = doPrep(`
        struct Foo {
            int x;
        }
        struct Bar {
            int y;
        }
        operator Bar(Foo foo) {
            Bar b;
            b.y = foo.x + 7;
            return b;
        }
        int baz(int z) {
            Foo foo;
            foo.x = z;
            Bar b = Bar(foo);
            return b.y;
        }
    `);
    checkInt(program, callFunction(program, "baz", [makeInt(program, 6)]), 13);
    program = doPrep(`
        struct Foo {
            int x;
        }
        struct Bar {
            int y;
        }
        operator thread Bar*(thread Foo* foo) {
            Bar b;
            b.y = (*foo).x + 8;
            return &b;
        }
        int baz(int z) {
            Foo foo;
            foo.x = z;
            thread Bar* b = thread Bar*(&foo);
            return (*b).y;
        }
    `);
    checkInt(program, callFunction(program, "baz", [makeInt(program, 6)]), 14);
}

tests.pointerToMember = function()
{
    checkFail(
        () => doPrep(`
            void foo()
            {
                float3 x;
                thread float* y = &x[1];
            }
        `),
        e => e instanceof WTypeError);

    checkFail(
        () => doPrep(`
            void foo()
            {
                float3x3 x;
                thread float3* y = &x[1];
            }
        `),
        e => e instanceof WTypeError);
}

tests.builtinMatrices = function()
{
    let program = doPrep(`
        float foo()
        {
            float2x2 a;
            a[0][0] = 1;
            a[0][1] = 2;
            a[1][0] = 3;
            a[1][1] = 4;
            return a[0][0];
        }
        float foo2()
        {
            float2x3 a;
            a[0][0] = 1;
            a[0][1] = 2;
            a[0][2] = 3;
            a[1][0] = 4;
            a[1][1] = 5;
            a[1][2] = 6;
            return a[1][2];
        }
        float foo3()
        {
            float2x2 a;
            return a[0][0];
        }
        bool foo4()
        {
            float2x2 a;
            a[0][0] = 1;
            a[0][1] = 2;
            a[1][0] = 3;
            a[1][1] = 4;
            float2x2 b;
            b[0][0] = 5;
            b[0][1] = 6;
            b[1][0] = 7;
            b[1][1] = 8;
            for (uint i = 0; i < 2; ++i) {
                for (uint j = 0; j < 2; ++j) {
                    a[i][j] += 4;
                }
            }
            return a == b;
        }
        bool foo5()
        {
            float2x2 a;
            a[0] = float2(1, 2);
            a[1] = float2(3, 4);
            float2x2 b;
            b[0][0] = 1;
            b[0][1] = 2;
            b[1][0] = 3;
            b[1][1] = 4;
            return a == b;
        }
        bool foo6()
        {
            float2x2 a;
            a[0][0] = 1;
            a[0][1] = 2;
            a[1][0] = 3;
            a[1][1] = 4;
            float2x2 b;
            b[0][0] = 5;
            b[0][1] = 10;
            b[1][0] = 18;
            b[1][1] = 24;
            a[0] *= 5;
            a[1] *= 6;
            return a == b;
        }
        float foo7()
        {
            float2x3 a = float2x3(float3(3, 4, 5), float3(6, 7, 8));
            return a[0][2];
        }
    `);
    checkFloat(program, callFunction(program, "foo", []), 1);
    checkFloat(program, callFunction(program, "foo2", []), 6);
    checkFloat(program, callFunction(program, "foo3", []), 0);
    checkBool(program, callFunction(program, "foo4", []), true);
    checkBool(program, callFunction(program, "foo5", []), true);
    checkBool(program, callFunction(program, "foo6", []), true);
    checkFloat(program, callFunction(program, "foo7", []), 5);
}

tests.halfSimpleMath = function() {
    let program = doPrep("half foo(half x, half y) { return x + y; }");
    checkHalf(program, callFunction(program, "foo", [makeHalf(program, 7), makeHalf(program, 5)]), 12);
    program = doPrep("half foo(half x, half y) { return x - y; }");
    checkHalf(program, callFunction(program, "foo", [makeHalf(program, 7), makeHalf(program, 5)]), 2);
    checkHalf(program, callFunction(program, "foo", [makeHalf(program, 5), makeHalf(program, 7)]), -2);
    program = doPrep("half foo(half x, half y) { return x * y; }");
    checkHalf(program, callFunction(program, "foo", [makeHalf(program, 7), makeHalf(program, 5)]), 35);
    checkHalf(program, callFunction(program, "foo", [makeHalf(program, 7), makeHalf(program, -5)]), -35);
    program = doPrep("half foo(half x, half y) { return x / y; }");
    checkHalf(program, callFunction(program, "foo", [makeHalf(program, 7), makeHalf(program, 2)]), 3.5);
    checkHalf(program, callFunction(program, "foo", [makeHalf(program, 7), makeHalf(program, -2)]), -3.5);
}

okToTest = true;

let testFilter = /.*/; // run everything by default
if (this["arguments"]) {
    for (let i = 0; i < arguments.length; i++) {
        switch (arguments[0]) {
        case "--filter":
            testFilter = new RegExp(arguments[++i]);
            break;
        default:
            throw new Error("Unknown argument: ", arguments[i]);
        }
    }
}

function* doTest(testFilter)
{
    if (!okToTest)
        throw new Error("Test setup is incomplete.");
    let before = preciseTime();
    
    print("Compiling standard library...");
    yield;
    prepare();
    print("    OK!");

    let names = [];
    for (let s in tests)
        names.push(s);
    names.sort();
    for (let s of names) {
        if (s.match(testFilter)) {
            print("TEST: " + s + "...");
            yield;
            const testBefore = preciseTime();
            tests[s]();
            const testAfter = preciseTime();
            print(`    OK, took ${Math.round((testAfter - testBefore) * 1000)} ms`);
        }
    }

    let after = preciseTime();
    
    print("Success!");
    print("That took " + (after - before) * 1000 + " ms.");
}

if (!this.window) {
    Error.stackTraceLimit = Infinity;
    for (let _ of doTest(testFilter)) { }
}

