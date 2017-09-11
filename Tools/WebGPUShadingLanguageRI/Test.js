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

if (this.window) {
    this.print = (text) => {
        var span = document.createElement("span");
        document.getElementById("messages").appendChild(span);
        span.innerHTML = text.replace(/ /g, "&nbsp;").replace(/\n/g, "<br>") + "<br>";
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
    return TypedValue.box(program.intrinsics.int32, value);
}

function checkNumber(program, result, expected)
{
    if (!result.type.isNumber)
        throw new Error("Wrong result type; result: " + result);
    if (result.value != expected)
        throw new Error("Wrong result: " + result + " (expected " + expected + ")");
}

function makeUInt(program, value)
{
    return TypedValue.box(program.intrinsics.uint32, value);
}

function makeBool(program, value)
{
    return TypedValue.box(program.intrinsics.bool, value);
}

function checkInt(program, result, expected)
{
    if (!result.type.equals(program.intrinsics.int32))
        throw new Error("Wrong result type; result: " + result);
    checkNumber(program, result, expected);
}

function checkUInt(program, result, expected)
{
    if (!result.type.equals(program.intrinsics.uint32))
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

function TEST_literalBool() {
    let program = doPrep("bool foo() { return true; }");
    checkBool(program, callFunction(program, "foo", [], []), true);
}

function TEST_identityBool() {
    let program = doPrep("bool foo(bool x) { return x; }");
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, true)]), true);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, false)]), false);
}

function TEST_intSimpleMath() {
    let program = doPrep("int foo(int x, int y) { return x + y; }");
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 7), makeInt(program, 5)]), 12);
    program = doPrep("int foo(int x, int y) { return x - y; }");
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 7), makeInt(program, 5)]), 2);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 5), makeInt(program, 7)]), -2);
    program = doPrep("int foo(int x, int y) { return x * y; }");
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 7), makeInt(program, 5)]), 35);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 7), makeInt(program, -5)]), -35);
    program = doPrep("int foo(int x, int y) { return x / y; }");
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 7), makeInt(program, 2)]), 3);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 7), makeInt(program, -2)]), -3);
}

function TEST_uintSimpleMath() {
    let program = doPrep("uint foo(uint x, uint y) { return x + y; }");
    checkUInt(program, callFunction(program, "foo", [], [makeUInt(program, 7), makeUInt(program, 5)]), 12);
    program = doPrep("uint foo(uint x, uint y) { return x - y; }");
    checkUInt(program, callFunction(program, "foo", [], [makeUInt(program, 7), makeUInt(program, 5)]), 2);
    checkUInt(program, callFunction(program, "foo", [], [makeUInt(program, 5), makeUInt(program, 7)]), 4294967294);
    program = doPrep("uint foo(uint x, uint y) { return x * y; }");
    checkUInt(program, callFunction(program, "foo", [], [makeUInt(program, 7), makeUInt(program, 5)]), 35);
    program = doPrep("uint foo(uint x, uint y) { return x / y; }");
    checkUInt(program, callFunction(program, "foo", [], [makeUInt(program, 7), makeUInt(program, 2)]), 3);
}

function TEST_equality() {
    let program = doPrep("bool foo(uint x, uint y) { return x == y; }");
    checkBool(program, callFunction(program, "foo", [], [makeUInt(program, 7), makeUInt(program, 5)]), false);
    checkBool(program, callFunction(program, "foo", [], [makeUInt(program, 7), makeUInt(program, 7)]), true);
    program = doPrep("bool foo(int x, int y) { return x == y; }");
    checkBool(program, callFunction(program, "foo", [], [makeInt(program, 7), makeInt(program, 5)]), false);
    checkBool(program, callFunction(program, "foo", [], [makeInt(program, 7), makeInt(program, 7)]), true);
    program = doPrep("bool foo(bool x, bool y) { return x == y; }");
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, false), makeBool(program, true)]), false);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, true), makeBool(program, false)]), false);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, false), makeBool(program, false)]), true);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, true), makeBool(program, true)]), true);
}

function TEST_logicalNegation()
{
    let program = doPrep("bool foo(bool x) { return !x; }");
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, true)]), false);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, false)]), true);
}

function TEST_notEquality() {
    let program = doPrep("bool foo(uint x, uint y) { return x != y; }");
    checkBool(program, callFunction(program, "foo", [], [makeUInt(program, 7), makeUInt(program, 5)]), true);
    checkBool(program, callFunction(program, "foo", [], [makeUInt(program, 7), makeUInt(program, 7)]), false);
    program = doPrep("bool foo(int x, int y) { return x != y; }");
    checkBool(program, callFunction(program, "foo", [], [makeInt(program, 7), makeInt(program, 5)]), true);
    checkBool(program, callFunction(program, "foo", [], [makeInt(program, 7), makeInt(program, 7)]), false);
    program = doPrep("bool foo(bool x, bool y) { return x != y; }");
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, false), makeBool(program, true)]), true);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, true), makeBool(program, false)]), true);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, false), makeBool(program, false)]), false);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, true), makeBool(program, true)]), false);
}

function TEST_equalityTypeFailure()
{
    checkFail(
        () => doPrep("bool foo(int x, uint y) { return x == y; }"),
        (e) => e instanceof WTypeError && e.message.indexOf("/internal/test:1") != -1);
}

function TEST_generalNegation()
{
    let program = doPrep("bool foo(int x) { return !x; }");
    checkBool(program, callFunction(program, "foo", [], [makeInt(program, 7)]), false);
    checkBool(program, callFunction(program, "foo", [], [makeInt(program, 0)]), true);
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
        (e) => e instanceof WTypeError && e.message.indexOf("/internal/test:1") != -1);
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

function TEST_threadArrayLoadIntLiteral()
{
    let program = doPrep(`
        int foo(thread int[] array)
        {
            return array[0];
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

function TEST_deviceArrayStoreIntLiteral()
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

function TEST_simpleNoReturn()
{
    checkFail(
        () => doPrep("int foo() { }"),
        (e) => e instanceof WTypeError);
}

function TEST_simpleUnreachableCode()
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

function TEST_simpleStruct()
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
    let result = callFunction(program, "foo", [], [new TypedValue(structType, new EPtr(buffer, 0))]);
    if (!result.type.equals(structType))
        throw new Error("Wrong result type: " + result.type);
    let x = result.ePtr.get(0);
    let y = result.ePtr.get(1);
    if (x != 24)
        throw new Error("Wrong result for x: " + x + " (y = " + y + ")");
    if (y != 62)
        throw new Error("Wrong result for y: " + y + " (x + " + x + ")");
}

function TEST_genericStructInstance()
{
    let program = doPrep(`
        struct Foo<T> {
            T x;
            T y;
        }
        Foo<int> foo(Foo<int> foo)
        {
            Foo<int> result;
            result.x = foo.y;
            result.y = foo.x;
            return result;
        }
    `);
    let structType = TypeRef.instantiate(program.types.get("Foo"), [program.intrinsics.int32]);
    let buffer = new EBuffer(2);
    buffer.set(0, 62);
    buffer.set(1, 24);
    let result = callFunction(program, "foo", [], [new TypedValue(structType, new EPtr(buffer, 0))]);
    let x = result.ePtr.get(0);
    let y = result.ePtr.get(1);
    if (x != 24)
        throw new Error("Wrong result for x: " + x + " (y = " + y + ")");
    if (y != 62)
        throw new Error("Wrong result for y: " + y + " (x + " + x + ")");
}

function TEST_doubleGenericCallsDoubleGeneric()
{
    doPrep(`
        void foo<T, U>(T, U) { }
        void bar<V, W>(V x, W y) { foo(x, y); }
    `);
}

function TEST_doubleGenericCallsSingleGeneric()
{
    checkFail(
        () => doPrep(`
            void foo<T>(T, T) { }
            void bar<V, W>(V x, W y) { foo(x, y); }
        `),
        (e) => e instanceof WTypeError);
}

function TEST_loadNull()
{
    checkFail(
        () => doPrep(`
            void sink<T>(T) { }
            void foo() { sink(^null); }
        `),
        (e) => e instanceof WTypeError && e.message.indexOf("Type passed to dereference is not a pointer: null") != -1);
}

function TEST_storeNull()
{
    checkFail(
        () => doPrep(`
            void foo() { ^null = 42; }
        `),
        (e) => e instanceof WTypeError && e.message.indexOf("Type passed to dereference is not a pointer: null") != -1);
}

function TEST_returnNull()
{
    let program = doPrep(`
        thread int^ foo() { return null; }
    `);
    let result = callFunction(program, "foo", [], []);
    if (!result.type.isPtr)
        throw new Error("Return type is not a pointer: " + result.type);
    if (!result.type.elementType.equals(program.intrinsics.int32))
        throw new Error("Return type is not a pointer to an int: " + result.type);
    if (result.value != null)
        throw new Error("Return value is not null: " + result.value);
}

function TEST_dereferenceDefaultNull()
{
    let program = doPrep(`
        int foo()
        {
            thread int^ p;
            return ^p;
        }
    `);
    checkFail(
        () => callFunction(program, "foo", [], []),
        (e) => e instanceof WTrapError);
}

function TEST_defaultInitializedNull()
{
    let program = doPrep(`
        int foo()
        {
            thread int^ p = null;;
            return ^p;
        }
    `);
    checkFail(
        () => callFunction(program, "foo", [], []),
        (e) => e instanceof WTrapError);
}

function TEST_passNullToPtrMonomorphic()
{
    let program = doPrep(`
        int foo(thread int^ ptr)
        {
            return ^ptr;
        }
        int bar()
        {
            return foo(null);
        }
    `);
    checkFail(
        () => callFunction(program, "bar", [], []),
        (e) => e instanceof WTrapError);
}

function TEST_passNullToPtrPolymorphic()
{
    checkFail(
        () => doPrep(`
            T foo<T>(thread T^ ptr)
            {
                return ^ptr;
            }
            int bar()
            {
                return foo(null);
            }
        `),
        (e) => e instanceof WTypeError);
}

function TEST_passNullToPolymorphic()
{
    checkFail(
        () => doPrep(`
            T foo<T>(T ptr)
            {
                return ptr;
            }
            int bar()
            {
                return foo(null);
            }
        `),
        (e) => e instanceof WTypeError);
}

function TEST_loadNullArrayRef()
{
    checkFail(
        () => doPrep(`
            void sink<T>(T) { }
            void foo() { sink(null[0u]); }
        `),
        (e) => e instanceof WTypeError && e.message.indexOf("Did not find function for call") != -1);
}

function TEST_storeNullArrayRef()
{
    checkFail(
        () => doPrep(`
            void foo() { null[0u] = 42; }
        `),
        (e) => e instanceof WTypeError && e.message.indexOf("Did not find function for call") != -1);
}

function TEST_returnNullArrayRef()
{
    let program = doPrep(`
        thread int[] foo() { return null; }
    `);
    let result = callFunction(program, "foo", [], []);
    if (!result.type.isArrayRef)
        throw new Error("Return type is not an array reference: " + result.type);
    if (!result.type.elementType.equals(program.intrinsics.int32))
        throw new Error("Return type is not an int array reference: " + result.type);
    if (result.value != null)
        throw new Error("Return value is not null: " + result.value);
}

function TEST_dereferenceDefaultNullArrayRef()
{
    let program = doPrep(`
        int foo()
        {
            thread int[] p;
            return p[0u];
        }
    `);
    checkFail(
        () => callFunction(program, "foo", [], []),
        (e) => e instanceof WTrapError);
}

function TEST_defaultInitializedNullArrayRef()
{
    let program = doPrep(`
        int foo()
        {
            thread int[] p = null;
            return p[0u];
        }
    `);
    checkFail(
        () => callFunction(program, "foo", [], []),
        (e) => e instanceof WTrapError);
}

function TEST_defaultInitializedNullArrayRefIntLiteral()
{
    let program = doPrep(`
        int foo()
        {
            thread int[] p = null;
            return p[0];
        }
    `);
    checkFail(
        () => callFunction(program, "foo", [], []),
        (e) => e instanceof WTrapError);
}

function TEST_passNullToPtrMonomorphicArrayRef()
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
        () => callFunction(program, "bar", [], []),
        (e) => e instanceof WTrapError);
}

function TEST_passNullToPtrPolymorphicArrayRef()
{
    checkFail(
        () => doPrep(`
            T foo<T>(thread T[] ptr)
            {
                return ptr[0u];
            }
            int bar()
            {
                return foo(null);
            }
        `),
        (e) => e instanceof WTypeError);
}

function TEST_returnIntLiteralUint()
{
    let program = doPrep("uint foo() { return 42; }");
    checkNumber(program, callFunction(program, "foo", [], []), 42);
}

function TEST_returnIntLiteralDouble()
{
    let program = doPrep("double foo() { return 42; }");
    checkNumber(program, callFunction(program, "foo", [], []), 42);
}

function TEST_badIntLiteralForInt()
{
    checkFail(
        () => doPrep("void foo() { int x = 3000000000; }"),
        (e) => e instanceof WTypeError);
}

function TEST_badIntLiteralForUint()
{
    checkFail(
        () => doPrep("void foo() { uint x = 5000000000; }"),
        (e) => e instanceof WTypeError);
}

function TEST_badIntLiteralForDouble()
{
    checkFail(
        () => doPrep("void foo() { double x = 5000000000000000000000000000000000000; }"),
        (e) => e instanceof WSyntaxError);
}

function TEST_passNullAndNotNull()
{
    let program = doPrep(`
        T bar<T:primitive>(device T^ p, device T^)
        {
            return ^p;
        }
        int foo(device int^ p)
        {
            return bar(p, null);
        }
    `);
    let buffer = new EBuffer(1);
    buffer.set(0, 13);
    checkInt(program, callFunction(program, "foo", [], [TypedValue.box(new PtrType(null, "device", program.intrinsics.int32), new EPtr(buffer, 0))]), 13);
}

function TEST_passNullAndNotNullFullPoly()
{
    let program = doPrep(`
        T bar<T:primitive>(T p, T)
        {
            return p;
        }
        int foo(device int^ p)
        {
            return ^bar(p, null);
        }
    `);
    let buffer = new EBuffer(1);
    buffer.set(0, 13);
    checkInt(program, callFunction(program, "foo", [], [TypedValue.box(new PtrType(null, "device", program.intrinsics.int32), new EPtr(buffer, 0))]), 13);
}

function TEST_passNullAndNotNullFullPolyReverse()
{
    let program = doPrep(`
        T bar<T:primitive>(T, T p)
        {
            return p;
        }
        int foo(device int^ p)
        {
            return ^bar(null, p);
        }
    `);
    let buffer = new EBuffer(1);
    buffer.set(0, 13);
    checkInt(program, callFunction(program, "foo", [], [TypedValue.box(new PtrType(null, "device", program.intrinsics.int32), new EPtr(buffer, 0))]), 13);
}

function TEST_nullTypeVariableUnify()
{
    let left = new NullType(null);
    let right = new TypeVariable(null, "T", null);
    if (left.equals(right))
        throw new Error("Should not be equal but are: " + left + " and " + right);
    if (right.equals(left))
        throw new Error("Should not be equal but are: " + left + " and " + right);
    
    function everyOrder(array, callback)
    {
        function recurse(array, callback, order)
        {
            if (!array.length)
                return callback.call(null, order);
            
            for (let i = 0; i < array.length; ++i) {
                let nextArray = array.concat();
                nextArray.splice(i, 1);
                recurse(nextArray, callback, order.concat([array[i]]));
            }
        }
        
        recurse(array, callback, []);
    }
    
    function everyPair(things)
    {
        let result = [];
        for (let i = 0; i < things.length; ++i) {
            for (let j = 0; j < things.length; ++j) {
                if (i != j)
                    result.push([things[i], things[j]]);
            }
        }
        return result;
    }
    
    everyOrder(
        everyPair(["nullType", "variableType", "ptrType"]),
        order => {
            let types = {};
            types.nullType = new NullType(null);
            types.variableType = new TypeVariable(null, "T", null);
            types.ptrType = new PtrType(null, "constant", new NativeType(null, "foo_t", true, []));
            let unificationContext = new UnificationContext([types.variableType]);
            for (let [leftName, rightName] of order) {
                let left = types[leftName];
                let right = types[rightName];
                let result = left.unify(unificationContext, right);
                if (!result)
                    throw new Error("In order " + order + " cannot unify " + left + " with " + right);
            }
            if (!unificationContext.verify())
                throw new Error("In order " + order.map(value => "(" + value + ")") + " cannot verify");
        });
}

function TEST_doubleNot()
{
    let program = doPrep(`
        bool foo(bool x)
        {
            return !!x;
        }
    `);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, true)]), true);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, false)]), false);
}

function TEST_simpleRecursion()
{
    checkFail(
        () => doPrep(`
            void foo<T>(T x)
            {
                foo(&x);
            }
        `),
        (e) => e instanceof WTypeError);
}

function TEST_protocolMonoSigPolyDef()
{
    let program = doPrep(`
        struct IntAnd<T> {
            int first;
            T second;
        }
        IntAnd<T> intAnd<T>(int first, T second)
        {
            IntAnd<T> result;
            result.first = first;
            result.second = second;
            return result;
        }
        protocol IntAndable {
            IntAnd<int> intAnd(IntAndable, int);
        }
        int foo<T:IntAndable>(T first, int second)
        {
            IntAnd<int> result = intAnd(first, second);
            return result.first + result.second;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 54), makeInt(program, 12)]), 54 + 12);
}

function TEST_protocolPolySigPolyDef()
{
    let program = doPrep(`
        struct IntAnd<T> {
            int first;
            T second;
        }
        IntAnd<T> intAnd<T>(int first, T second)
        {
            IntAnd<T> result;
            result.first = first;
            result.second = second;
            return result;
        }
        protocol IntAndable {
            IntAnd<T> intAnd<T>(IntAndable, T);
        }
        int foo<T:IntAndable>(T first, int second)
        {
            IntAnd<int> result = intAnd(first, second);
            return result.first + result.second;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 54), makeInt(program, 12)]), 54 + 12);
}

function TEST_protocolDoublePolySigDoublePolyDef()
{
    let program = doPrep(`
        struct IntAnd<T, U> {
            int first;
            T second;
            U third;
        }
        IntAnd<T, U> intAnd<T, U>(int first, T second, U third)
        {
            IntAnd<T, U> result;
            result.first = first;
            result.second = second;
            result.third = third;
            return result;
        }
        protocol IntAndable {
            IntAnd<T, U> intAnd<T, U>(IntAndable, T, U);
        }
        int foo<T:IntAndable>(T first, int second, int third)
        {
            IntAnd<int, int> result = intAnd(first, second, third);
            return result.first + result.second + result.third;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 54), makeInt(program, 12), makeInt(program, 39)]), 54 + 12 + 39);
}

function TEST_protocolDoublePolySigDoublePolyDefExplicit()
{
    let program = doPrep(`
        struct IntAnd<T, U> {
            int first;
            T second;
            U third;
        }
        IntAnd<T, U> intAnd<T, U>(int first, T second, U third)
        {
            IntAnd<T, U> result;
            result.first = first;
            result.second = second;
            result.third = third;
            return result;
        }
        protocol IntAndable {
            IntAnd<T, U> intAnd<T, U>(IntAndable, T, U);
        }
        int foo<T:IntAndable>(T first, int second, int third)
        {
            IntAnd<int, int> result = intAnd<int, int>(first, second, third);
            return result.first + result.second + result.third;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 54), makeInt(program, 12), makeInt(program, 39)]), 54 + 12 + 39);
}

function TEST_variableShadowing()
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
    checkInt(program, callFunction(program, "foo", [], []), 8);
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
    checkInt(program, callFunction(program, "foo", [], []), 7);
}

function TEST_ifStatement()
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 3)]), 6);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 5)]), 6);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 6)]), 6);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 7)]), 8);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 8)]), 6);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 9)]), 6);
}

function TEST_ifElseStatement()
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 3)]), 9);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 4)]), 9);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 5)]), 9);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 6)]), 9);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 7)]), 8);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 8)]), 9);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 9)]), 9);
}

function TEST_ifElseIfStatement()
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 3)]), 6);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 5)]), 6);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 6)]), 6);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 7)]), 8);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 8)]), 9);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 9)]), 6);
}

function TEST_ifElseIfElseStatement()
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 3)]), 10);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 4)]), 10);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 5)]), 10);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 6)]), 10);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 7)]), 8);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 8)]), 9);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 9)]), 10);
}

function TEST_returnIf()
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 3)]), 10);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 4)]), 10);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 5)]), 10);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 6)]), 10);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 7)]), 8);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 8)]), 10);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 9)]), 10);
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 3)]), 9);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 4)]), 9);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 5)]), 9);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 6)]), 9);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 7)]), 8);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 8)]), 9);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 9)]), 9);
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 7)]), 6);
}

function TEST_simpleWhile()
{
    let program = doPrep(`
        int foo(int x)
        {
            while (x < 13)
                x = x * 2;
            return x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 1)]), 16);
}

function TEST_protocolMonoPolySigDoublePolyDefExplicit()
{
    checkFail(
        () => {
            let program = doPrep(`
                struct IntAnd<T, U> {
                    int first;
                    T second;
                    U third;
                }
                IntAnd<T, U> intAnd<T, U>(int first, T second, U third)
                {
                    IntAnd<T, U> result;
                    result.first = first;
                    result.second = second;
                    result.third = third;
                    return result;
                }
                protocol IntAndable {
                    IntAnd<T, int> intAnd<T>(IntAndable, T, int);
                }
                int foo<T:IntAndable>(T first, int second, int third)
                {
                    IntAnd<int, int> result = intAnd<int>(first, second, third);
                    return result.first + result.second + result.third;
                }
            `);
            callFunction(program, "foo", [], [makeInt(program, 54), makeInt(program, 12), makeInt(program, 39)]);
        },
        (e) => e instanceof WTypeError);
}

function TEST_ambiguousOverloadSimple()
{
    checkFail(
        () => doPrep(`
            void foo<T>(int, T) { }
            void foo<T>(T, int) { }
            void bar(int a, int b) { foo(a, b); }
        `),
        (e) => e instanceof WTypeError);
}

function TEST_ambiguousOverloadOverlapping()
{
    checkFail(
        () => doPrep(`
            void foo<T>(int, T) { }
            void foo<T>(T, T) { }
            void bar(int a, int b) { foo(a, b); }
        `),
        (e) => e instanceof WTypeError);
}

function TEST_ambiguousOverloadTieBreak()
{
    doPrep(`
        void foo<T>(int, T) { }
        void foo<T>(T, T) { }
        void foo(int, int) { }
        void bar(int a, int b) { foo(a, b); }
    `);
}

function TEST_intOverloadResolution()
{
    let program = doPrep(`
        int foo(int) { return 1; }
        int foo(uint) { return 2; }
        int foo(double) { return 3; }
        int bar() { return foo(42); }
    `);
    checkInt(program, callFunction(program, "bar", [], []), 1);
}

function TEST_intOverloadResolutionReverseOrder()
{
    let program = doPrep(`
        int foo(double) { return 3; }
        int foo(uint) { return 2; }
        int foo(int) { return 1; }
        int bar() { return foo(42); }
    `);
    checkInt(program, callFunction(program, "bar", [], []), 1);
}

function TEST_intOverloadResolutionGeneric()
{
    let program = doPrep(`
        int foo(int) { return 1; }
        int foo<T>(T) { return 2; }
        int bar() { return foo(42); }
    `);
    checkInt(program, callFunction(program, "bar", [], []), 1);
}

function TEST_intLiteralGeneric()
{
    checkFail(
        () => doPrep(`
            int foo<T>(T) { return 1; }
            int bar() { return foo(42); }
        `),
        (e) => e instanceof WTypeError);
}

function TEST_intLiteralGeneric()
{
    let program = doPrep(`
        T foo<T>(T x) { return x; }
        int bar() { return foo(int(42)); }
    `);
    checkInt(program, callFunction(program, "bar", [], []), 42);
}

function TEST_simpleConstexpr()
{
    let program = doPrep(`
        int foo<int a>(int b)
        {
            return a + b;
        }
        int bar(int b)
        {
            return foo<42>(b);
        }
    `);
    checkInt(program, callFunction(program, "bar", [], [makeInt(program, 58)]), 58 + 42);
}

function TEST_break()
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 1)]), 8);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 10)]), 20);
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 1)]), 7);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 10)]), 19);
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 1)]), 7);
    program = doPrep(`
            int foo(int x)
            {
                while (true) {
                    break;
                }
                return x;
            }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 1)]), 1);
    program = doPrep(`
            int foo()
            {
                while (true) {
                    return 7;
                }
            }
    `);
    checkInt(program, callFunction(program, "foo", [], []), 7);
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

function TEST_continue()
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 1)]), 18);
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

function TEST_doWhile()
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 1)]), 8);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 11)]), 8);
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 1)]), 8);
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 9)]), 19);
}

function TEST_forLoop()
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 5)]), 10);
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 5)]), 10);
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 5)]), 10);
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 5)]), 6);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 6)]), 11);
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 5)]), 10);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 6)]), 10);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 7)]), 10);
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 5)]), 10);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 6)]), 15);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 7)]), 21);
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 5)]), 10);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 6)]), 15);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 7)]), 21);
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 5)]), 10);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 6)]), 15);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 7)]), 21);
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 3)]), 7);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 4)]), 7);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 5)]), 7);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 6)]), 7);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 7)]), 7);
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
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 3)]), 7);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 4)]), 7);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 5)]), 7);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 6)]), 7);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 7)]), 7);
}

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

function doTest(object)
{
    let before = preciseTime();

    for (let s in object) {
        if (s.startsWith("TEST_") && s.match(filter)) {
            print(s + "...");
            object[s]();
            print("    OK!");
        }
    }

    let after = preciseTime();
    
    print("That took " + (after - before) * 1000 + " ms.");
}

if (!this.window)
    doTest(this);
