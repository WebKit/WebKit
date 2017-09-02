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

load("All.js");

function doPrep(code)
{
    return prepare("<test>", 0, code);
}

function doLex(code)
{
    let lexer = new Lexer("<test>", 0, code);
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
    return TypedValue.box(program.intrinsics.int32, value);
}

function checkInt(program, result, expected)
{
    if (!result.type.equals(program.intrinsics.int32))
        throw new Error("Wrong result type; result: " + result);
    if (result.value != expected)
        throw new Error("Wrong result: " + result + " (expected " + expected + ")");
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

function TEST_add1() {
    let program = doPrep("int foo(int x) { return x + 1; }");
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 42)]), 43);
}

function TEST_simpleGeneric() {
    let program = doPrep(`
        T id<T>(T x) { return x; }
        int foo(int x) { return id(x) + 1; }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 42)]), 43);
}

function TEST_nameResolutionFailure()
{
    checkFail(
        () => doPrep("int foo(int x) { return x + y; }"),
        (e) => e instanceof WTypeError && e.message.indexOf("<test>:1") != -1);
}

function TEST_simpleVariable()
{
    let program = doPrep(`
        int foo(int p)
        {
            int result = p;
            return result;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 42)]), 42);
}

function TEST_simpleAssignment()
{
    let program = doPrep(`
        int foo(int p)
        {
            int result;
            result = p;
            return result;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 42)]), 42);
}

function TEST_simpleDefault()
{
    let program = doPrep(`
        int foo()
        {
            int result;
            return result;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], []), 0);
}

function TEST_simpleDereference()
{
    let program = doPrep(`
        int foo(device int^ p)
        {
            return ^p;
        }
    `);
    let buffer = new EBuffer(1);
    buffer.set(0, 13);
    checkInt(program, callFunction(program, "foo", [], [TypedValue.box(new PtrType(null, "device", program.intrinsics.int32), new EPtr(buffer, 0))]), 13);
}

function TEST_dereferenceStore()
{
    let program = doPrep(`
        void foo(device int^ p)
        {
            ^p = 52;
        }
    `);
    let buffer = new EBuffer(1);
    buffer.set(0, 13);
    callFunction(program, "foo", [], [TypedValue.box(new PtrType(null, "device", program.intrinsics.int32), new EPtr(buffer, 0))]);
    if (buffer.get(0) != 52)
        throw new Error("Expected buffer to contain 52 but it contains: " + buffer.get(0));
}

function TEST_simpleMakePtr()
{
    let program = doPrep(`
        thread int^ foo()
        {
            int x = 42;
            return &x;
        }
    `);
    let result = callFunction(program, "foo", [], []);
    if (!result.type.isPtr)
        throw new Error("Return type is not a pointer: " + result.type);
    if (!result.type.elementType.equals(program.intrinsics.int32))
        throw new Error("Return type is not a pointer to an int: " + result.type);
    if (!(result.value instanceof EPtr))
        throw new Error("Return value is not an EPtr: " + result.value);
    let value = result.value.loadValue();
    if (value != 42)
        throw new Error("Expected 42 but got: " + value);
}

function TEST_threadArrayLoad()
{
    let program = doPrep(`
        int foo(thread int[] array)
        {
            return array[0u];
        }
    `);
    let buffer = new EBuffer(1);
    buffer.set(0, 89);
    let result = callFunction(program, "foo", [], [TypedValue.box(new ArrayRefType(null, "thread", program.intrinsics.int32), new EArrayRef(new EPtr(buffer, 0), 1))]);
    checkInt(program, result, 89);
}

function TEST_deviceArrayLoad()
{
    let program = doPrep(`
        int foo(device int[] array)
        {
            return array[0u];
        }
    `);
    let buffer = new EBuffer(1);
    buffer.set(0, 89);
    let result = callFunction(program, "foo", [], [TypedValue.box(new ArrayRefType(null, "device", program.intrinsics.int32), new EArrayRef(new EPtr(buffer, 0), 1))]);
    checkInt(program, result, 89);
}

function TEST_threadArrayStore()
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
        new ArrayRefType(null, "thread", program.intrinsics.int32),
        new EArrayRef(new EPtr(buffer, 0), 1));
    callFunction(program, "foo", [], [arrayRef, makeInt(program, 65)]);
    if (buffer.get(0) != 65)
        throw new Error("Bad value stored into buffer (expected 65): " + buffer.get(0));
    callFunction(program, "foo", [], [arrayRef, makeInt(program, -111)]);
    if (buffer.get(0) != -111)
        throw new Error("Bad value stored into buffer (expected -111): " + buffer.get(0));
}

function TEST_deviceArrayStore()
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
        new ArrayRefType(null, "device", program.intrinsics.int32),
        new EArrayRef(new EPtr(buffer, 0), 1));
    callFunction(program, "foo", [], [arrayRef, makeInt(program, 65)]);
    if (buffer.get(0) != 65)
        throw new Error("Bad value stored into buffer (expected 65): " + buffer.get(0));
    callFunction(program, "foo", [], [arrayRef, makeInt(program, -111)]);
    if (buffer.get(0) != -111)
        throw new Error("Bad value stored into buffer (expected -111): " + buffer.get(0));
}

function TEST_simpleProtocol()
{
    let program = doPrep(`
        protocol Addable {
            Addable operator+(Addable, Addable);
        }
        T add<T:Addable>(T a, T b)
        {
            return a + b;
        }
        int foo(int x)
        {
            return add(x, 73);
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 45)]), 45 + 73);
}

function TEST_typeMismatchReturn()
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

function TEST_typeMismatchVariableDecl()
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

function TEST_typeMismatchAssignment()
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

function TEST_typeMismatchReturnParam()
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

function TEST_badAdd()
{
    checkFail(
        () => doPrep(`
            void bar<T>(T) { }
            void foo(int x, uint y)
            {
                bar(x + y);
            }
        `),
        (e) => e instanceof WTypeError && e.message.indexOf("native int32 operator+<>(int32,int32)") != -1);
}

function TEST_lexerKeyword()
{
    let result = doLex("ident for while 123 123u { } {asd asd{ 1a3");
    if (result.length != 13)
        throw new Error("Lexer emitted an incorrect number of tokens (expected 12): " + result.length);
    checkLexerToken(result[0],  0,  "identifier", "ident");
    checkLexerToken(result[1],  6,  "keyword",     "for");
    checkLexerToken(result[2],  10, "keyword",     "while");
    checkLexerToken(result[3],  16, "intLiteral",  "123");
    checkLexerToken(result[4],  20, "uintLiteral", "123u");
    checkLexerToken(result[5],  25, "punctuation", "{");
    checkLexerToken(result[6],  27, "punctuation", "}");
    checkLexerToken(result[7],  29, "punctuation", "{");
    checkLexerToken(result[8],  30, "identifier",  "asd");
    checkLexerToken(result[9],  34, "identifier",  "asd");
    checkLexerToken(result[10], 37, "punctuation", "{");
    checkLexerToken(result[11], 39, "intLiteral",  "1");
    checkLexerToken(result[12], 40, "identifier",  "a3");
}

let before = preciseTime();

let filter = /.*/; // run everything by default
if (this["arguments"]) {
    for (let i = 0; i < arguments.length; i++) {
        switch (arguments[0]) {
        case "--filter":
            filter = new RegExp(arguments[++i]);
            break;
        default:
            throw new Error("Unknown argument: ", arguments[i]);
        }
    }
}


for (let s in this) {
    if (s.startsWith("TEST_") && s.match(filter)) {
        print(s + "...");
        this[s]();
        print("    OK!");
    }
}

let after = preciseTime();

print("That took " + (after - before) * 1000 + " ms.");
