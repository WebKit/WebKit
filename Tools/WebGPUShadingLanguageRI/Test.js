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
    return TypedValue.box(program.intrinsics.int32, value);
}

function checkNumber(program, result, expected)
{
    if (!result.type.isNumber)
        throw new Error("Wrong result type; result: " + result);
    if (result.value != expected)
        throw new Error("Wrong result: " + result.value + " (expected " + expected + ")");
}

function makeUint(program, value)
{
    return TypedValue.box(program.intrinsics.uint32, value);
}

function makeBool(program, value)
{
    return TypedValue.box(program.intrinsics.bool, value);
}

function makeFloat(program, value)
{
    return TypedValue.box(program.intrinsics.float, value);
}

function makeDouble(program, value)
{
    return TypedValue.box(program.intrinsics.double, value);
}

function checkNumber(program, result, expected)
{
    if (!result.type.isNumber)
        throw new Error("Wrong result type; result: " + result);
    if (result.value != expected)
        throw new Error("Wrong result: " + result + " (expected " + expected + ")");
}

function checkInt(program, result, expected)
{
    if (!result.type.equals(program.intrinsics.int32))
        throw new Error("Wrong result type; result: " + result);
    checkNumber(program, result, expected);
}

function checkUint(program, result, expected)
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

function checkFloat(program, result, expected)
{
    if (!result.type.equals(program.intrinsics.float))
        throw new Error("Wrong result type: " + result.type);
    if (result.value != expected)
        throw new Error("Wrong result: " + result.value + " (expected " + expected + ")");
}

function checkDouble(program, result, expected)
{
    if (!result.type.equals(program.intrinsics.double))
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
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 7), makeUint(program, 5)]), 12);
    program = doPrep("uint foo(uint x, uint y) { return x - y; }");
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 7), makeUint(program, 5)]), 2);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 5), makeUint(program, 7)]), 4294967294);
    program = doPrep("uint foo(uint x, uint y) { return x * y; }");
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 7), makeUint(program, 5)]), 35);
    program = doPrep("uint foo(uint x, uint y) { return x / y; }");
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 7), makeUint(program, 2)]), 3);
}

function TEST_equality() {
    let program = doPrep("bool foo(uint x, uint y) { return x == y; }");
    checkBool(program, callFunction(program, "foo", [], [makeUint(program, 7), makeUint(program, 5)]), false);
    checkBool(program, callFunction(program, "foo", [], [makeUint(program, 7), makeUint(program, 7)]), true);
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
    checkBool(program, callFunction(program, "foo", [], [makeUint(program, 7), makeUint(program, 5)]), true);
    checkBool(program, callFunction(program, "foo", [], [makeUint(program, 7), makeUint(program, 7)]), false);
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
        protocol MyAddable {
            MyAddable operator+(MyAddable, MyAddable);
        }
        T add<T:MyAddable>(T a, T b)
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
    let result = doLex("ident for while 123 123u { } {asd asd{ 1a3 1.2 + 3.4 + 1. + .2 1.2d 0.d .3d && ||");
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
    checkLexerToken(result[20], 63, "floatLiteral",  "1.2d");
    checkLexerToken(result[21], 68, "floatLiteral",  "0.d");
    checkLexerToken(result[22], 72, "floatLiteral",  ".3d");
    checkLexerToken(result[23], 76, "punctuation",   "&&");
    checkLexerToken(result[24], 79, "punctuation",   "||");
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
        (e) => e instanceof WTypeError && e.message.indexOf("LHS of assignment is not an LValue") != -1);
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
        (e) => e instanceof WSyntaxError);
}

function TEST_badIntLiteralForUint()
{
    checkFail(
        () => doPrep("void foo() { uint x = 5000000000; }"),
        (e) => e instanceof WSyntaxError);
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
            if (!unificationContext.verify().result)
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
    let program = doPrep(`
        int foo<T>(T x) { return 3478; }
        int bar() { return foo(42); }
    `);
    checkInt(program, callFunction(program, "bar", [], []), 3478);
}

function TEST_intLiteralGenericWithProtocols()
{
    let program = doPrep(`
        protocol MyConvertibleToInt {
            operator int(MyConvertibleToInt);
        }
        int foo<T:MyConvertibleToInt>(T x) { return int(x); }
        int bar() { return foo(42); }
    `);
    checkInt(program, callFunction(program, "bar", [], []), 42);
}

function TEST_uintLiteralGeneric()
{
    let program = doPrep(`
        int foo<T>(T x) { return 3478; }
        int bar() { return foo(42u); }
    `);
    checkInt(program, callFunction(program, "bar", [], []), 3478);
}

function TEST_uintLiteralGenericWithProtocols()
{
    let program = doPrep(`
        protocol MyConvertibleToUint {
            operator uint(MyConvertibleToUint);
        }
        uint foo<T:MyConvertibleToUint>(T x) { return uint(x); }
        uint bar() { return foo(42u); }
    `);
    checkUint(program, callFunction(program, "bar", [], []), 42);
}

function TEST_intLiteralGenericSpecific()
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

function TEST_chainConstexpr()
{
    let program = doPrep(`
        int foo<int a>(int b)
        {
            return a + b;
        }
        int bar<int a>(int b)
        {
            return foo<a>(b);
        }
        int baz(int b)
        {
            return bar<42>(b);
        }
    `);
    checkInt(program, callFunction(program, "baz", [], [makeInt(program, 58)]), 58 + 42);
}

function TEST_chainGeneric()
{
    let program = doPrep(`
        T foo<T>(T x)
        {
            return x;
        }
        T bar<T>(thread T^ ptr)
        {
            return ^foo(ptr);
        }
        int baz(int x)
        {
            return bar(&x);
        }
    `);
    checkInt(program, callFunction(program, "baz", [], [makeInt(program, 37)]), 37);
}

function TEST_chainStruct()
{
    let program = doPrep(`
        struct Foo<T> {
            T f;
        }
        struct Bar<T> {
            Foo<thread T^> f;
        }
        int foo(thread Bar<int>^ x)
        {
            return ^x->f.f;
        }
        int bar(int a)
        {
            Bar<int> x;
            x.f.f = &a;
            return foo(&x);
        }
    `);
    checkInt(program, callFunction(program, "bar", [], [makeInt(program, 4657)]), 4657);
}

function TEST_chainStructInvalid()
{
    checkFail(
        () => doPrep(`
            struct Foo<T> {
                T f;
            }
            struct Bar<T> {
                Foo<device T^> f;
            }
            int foo(thread Bar<int>^ x)
            {
                return ^x->f.f;
            }
            int bar(device int^ a)
            {
                Bar<int> x;
                x.f.f = a;
                return foo(&x);
            }
        `),
        (e) => e instanceof WTypeError);
}

function TEST_chainStructDevice()
{
    let program = doPrep(`
        struct Foo<T> {
            T f;
        }
        struct Bar<T:primitive> {
            Foo<device T^> f;
        }
        int foo(thread Bar<int>^ x)
        {
            return ^x->f.f;
        }
        int bar(device int^ a)
        {
            Bar<int> x;
            x.f.f = a;
            return foo(&x);
        }
    `);
    let buffer = new EBuffer(1);
    buffer.set(0, 79201);
    checkInt(program, callFunction(program, "bar", [], [TypedValue.box(new PtrType(null, "device", program.intrinsics.int32), new EPtr(buffer, 0))]), 79201);
}

function TEST_paramChainStructDevice()
{
    let program = doPrep(`
        struct Foo<T> {
            T f;
        }
        struct Bar<T> {
            Foo<T> f;
        }
        int foo(thread Bar<device int^>^ x)
        {
            return ^x->f.f;
        }
        int bar(device int^ a)
        {
            Bar<device int^> x;
            x.f.f = a;
            return foo(&x);
        }
    `);
    let buffer = new EBuffer(1);
    buffer.set(0, 79201);
    checkInt(program, callFunction(program, "bar", [], [TypedValue.box(new PtrType(null, "device", program.intrinsics.int32), new EPtr(buffer, 0))]), 79201);
}

function TEST_simpleProtocolExtends()
{
    let program = doPrep(`
        protocol Foo {
            void foo(thread Foo^);
        }
        protocol Bar : Foo {
            void bar(thread Bar^);
        }
        void fuzz<T:Foo>(thread T^ p)
        {
            foo(p);
        }
        void buzz<T:Bar>(thread T^ p)
        {
            fuzz(p);
            bar(p);
        }
        void foo(thread int^ p)
        {
            ^p = ^p + 743;
        }
        void bar(thread int^ p)
        {
            ^p = ^p + 91;
        }
        int thingy(int a)
        {
            buzz(&a);
            return a;
        }
    `);
    checkInt(program, callFunction(program, "thingy", [], [makeInt(program, 642)]), 642 + 743 + 91);
}

function TEST_protocolExtendsTwo()
{
    let program = doPrep(`
        protocol Foo {
            void foo(thread Foo^);
        }
        protocol Bar {
            void bar(thread Bar^);
        }
        protocol Baz : Foo, Bar {
            void baz(thread Baz^);
        }
        void fuzz<T:Foo>(thread T^ p)
        {
            foo(p);
        }
        void buzz<T:Bar>(thread T^ p)
        {
            bar(p);
        }
        void xuzz<T:Baz>(thread T^ p)
        {
            fuzz(p);
            buzz(p);
            baz(p);
        }
        void foo(thread int^ p)
        {
            ^p = ^p + 743;
        }
        void bar(thread int^ p)
        {
            ^p = ^p + 91;
        }
        void baz(thread int^ p)
        {
            ^p = ^p + 39;
        }
        int thingy(int a)
        {
            xuzz(&a);
            return a;
        }
    `);
    checkInt(program, callFunction(program, "thingy", [], [makeInt(program, 642)]), 642 + 743 + 91 + 39);
}

function TEST_prefixPlusPlus()
{
    let program = doPrep(`
        int foo(int x)
        {
            ++x;
            return x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 64)]), 65);
}

function TEST_prefixPlusPlusResult()
{
    let program = doPrep(`
        int foo(int x)
        {
            return ++x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 64)]), 65);
}

function TEST_postfixPlusPlus()
{
    let program = doPrep(`
        int foo(int x)
        {
            x++;
            return x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 64)]), 65);
}

function TEST_postfixPlusPlusResult()
{
    let program = doPrep(`
        int foo(int x)
        {
            return x++;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 64)]), 64);
}

function TEST_prefixMinusMinus()
{
    let program = doPrep(`
        int foo(int x)
        {
            --x;
            return x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 64)]), 63);
}

function TEST_prefixMinusMinusResult()
{
    let program = doPrep(`
        int foo(int x)
        {
            return --x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 64)]), 63);
}

function TEST_postfixMinusMinus()
{
    let program = doPrep(`
        int foo(int x)
        {
            x--;
            return x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 64)]), 63);
}

function TEST_postfixMinusMinusResult()
{
    let program = doPrep(`
        int foo(int x)
        {
            return x--;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 64)]), 64);
}

function TEST_plusEquals()
{
    let program = doPrep(`
        int foo(int x)
        {
            x += 42;
            return x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 385)]), 385 + 42);
}

function TEST_plusEqualsResult()
{
    let program = doPrep(`
        int foo(int x)
        {
            return x += 42;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 385)]), 385 + 42);
}

function TEST_minusEquals()
{
    let program = doPrep(`
        int foo(int x)
        {
            x -= 42;
            return x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 385)]), 385 - 42);
}

function TEST_minusEqualsResult()
{
    let program = doPrep(`
        int foo(int x)
        {
            return x -= 42;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 385)]), 385 - 42);
}

function TEST_timesEquals()
{
    let program = doPrep(`
        int foo(int x)
        {
            x *= 42;
            return x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 385)]), 385 * 42);
}

function TEST_timesEqualsResult()
{
    let program = doPrep(`
        int foo(int x)
        {
            return x *= 42;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 385)]), 385 * 42);
}

function TEST_divideEquals()
{
    let program = doPrep(`
        int foo(int x)
        {
            x /= 42;
            return x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 385)]), (385 / 42) | 0);
}

function TEST_divideEqualsResult()
{
    let program = doPrep(`
        int foo(int x)
        {
            return x /= 42;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 385)]), (385 / 42) | 0);
}

function TEST_twoIntLiterals()
{
    let program = doPrep(`
        bool foo()
        {
            return 42 == 42;
        }
    `);
    checkBool(program, callFunction(program, "foo", [], []), true);
}

function TEST_unifyDifferentLiterals()
{
    checkFail(
        () => doPrep(`
            void bar<T>(T, T)
            {
            }
            void foo()
            {
                bar(42, 42u);
            }
        `),
        (e) => e instanceof WTypeError);
}

function TEST_unifyDifferentLiteralsBackwards()
{
    checkFail(
        () => doPrep(`
            void bar<T>(T, T)
            {
            }
            void foo()
            {
                bar(42u, 42);
            }
        `),
        (e) => e instanceof WTypeError);
}

function TEST_unifyVeryDifferentLiterals()
{
    checkFail(
        () => doPrep(`
            void bar<T>(T, T)
            {
            }
            void foo()
            {
                bar(42, null);
            }
        `),
        (e) => e instanceof WTypeError);
}

function TEST_unifyVeryDifferentLiteralsBackwards()
{
    checkFail(
        () => doPrep(`
            void bar<T>(T, T)
            {
            }
            void foo()
            {
                bar(null, 42);
            }
        `),
        (e) => e instanceof WTypeError);
}

function TEST_assignUintToInt()
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

function TEST_buildArrayThenSumIt()
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
    checkInt(program, callFunction(program, "foo", [], []), 42 * 5 + 42 * 41 / 2);
}

function TEST_buildArrayThenSumItUsingArrayReference()
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
    checkInt(program, callFunction(program, "foo", [], []), 42 * 5 + 42 * 41 / 2);
}

function TEST_overrideSubscriptStruct()
{
    let program = doPrep(`
        struct Foo {
            int x;
            int y;
        }
        thread int^ operator&[](thread Foo^ foo, uint index)
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
    checkInt(program, callFunction(program, "foo", [], []), 498 + 19 * 3);
}

function TEST_overrideSubscriptStructAndDoStores()
{
    let program = doPrep(`
        struct Foo {
            int x;
            int y;
        }
        thread int^ operator&[](thread Foo^ foo, uint index)
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
    checkInt(program, callFunction(program, "foo", [], []), 498 + 19);
}

function TEST_overrideSubscriptStructAndUsePointers()
{
    let program = doPrep(`
        struct Foo {
            int x;
            int y;
        }
        thread int^ operator&[](thread Foo^ foo, uint index)
        {
            if (index == 0)
                return &foo->x;
            if (index == 1)
                return &foo->y;
            return null;
        }
        int bar(thread Foo^ foo)
        {
            return (^foo)[0] + (^foo)[1];
        }
        int foo()
        {
            Foo foo;
            foo.x = 498;
            foo.y = 19;
            return bar(&foo);
        }
    `);
    checkInt(program, callFunction(program, "foo", [], []), 498 + 19);
}

function TEST_overrideSubscriptStructAndUsePointersIncorrectly()
{
    checkFail(
        () => doPrep(`
            struct Foo {
                int x;
                int y;
            }
            thread int^ operator&[](thread Foo^ foo, uint index)
            {
                if (index == 0)
                    return &foo->x;
                if (index == 1)
                    return &foo->y;
                return null;
            }
            int bar(thread Foo^ foo)
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

function TEST_makeArrayRefFromLocal()
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
    checkInt(program, callFunction(program, "foo", [], []), 48);
}

function TEST_makeArrayRefFromPointer()
{
    let program = doPrep(`
        int bar(thread int[] ref)
        {
            return ref[0];
        }
        int baz(thread int^ ptr)
        {
            return bar(@ptr);
        }
        int foo()
        {
            int x = 48;
            return baz(&x);
        }
    `);
    checkInt(program, callFunction(program, "foo", [], []), 48);
}

function TEST_makeArrayRefFromArrayRef()
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

function TEST_simpleLength()
{
    let program = doPrep(`
        uint foo()
        {
            double[754] array;
            return (@array).length;
        }
    `);
    checkUint(program, callFunction(program, "foo", [], []), 754);
}

function TEST_nonArrayRefArrayLengthSucceed()
{
    let program = doPrep(`
        uint foo()
        {
            double[754] array;
            return array.length;
        }
    `);
    checkUint(program, callFunction(program, "foo", [], []), 754);
}

function TEST_nonArrayRefArrayLengthFail()
{
    checkFail(
        () => doPrep(`
            thread uint^ lengthPtr()
            {
                int[42] array;
                return &(array.length);
            }
        `),
        e => e instanceof WTypeError);
}

function TEST_constexprIsNotLValuePtr()
{
    checkFail(
        () => doPrep(`
            thread int^ foo<int x>()
            {
                return &x;
            }
        `),
        e => e instanceof WTypeError);
}

function TEST_constexprIsNotLValueAssign()
{
    checkFail(
        () => doPrep(`
            void foo<int x>()
            {
                x = 42;
            }
        `),
        e => e instanceof WTypeError);
}

function TEST_constexprIsNotLValueRMW()
{
    checkFail(
        () => doPrep(`
            void foo<int x>()
            {
                x += 42;
            }
        `),
        e => e instanceof WTypeError);
}

function TEST_assignLength()
{
    checkFail(
        () => doPrep(`
            void foo()
            {
                double[754] array;
                (@array).length = 42;
            }
        `),
        (e) => e instanceof WTypeError && e.message.indexOf("LHS of assignment is not an LValue") != -1);
}

function TEST_assignLengthHelper()
{
    checkFail(
        () => doPrep(`
            void bar(thread double[] array)
            {
                array.length = 42;
            }
            void foo()
            {
                double[754] array;
                bar(@array);
            }
        `),
        (e) => e instanceof WTypeError && e.message.indexOf("Cannot emit set because: Did not find any functions named operator.length=") != -1);
}

function TEST_simpleGetter()
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
    checkInt(program, callFunction(program, "foo", [], []), 7804);
}

function TEST_simpleSetter()
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
    checkInt(program, callFunction(program, "foo", [], []), 7804);
}

function TEST_genericAccessors()
{
    let program = doPrep(`
        struct Foo<T> {
            T x;
            T[3] y;
        }
        struct Bar<T> {
            T x;
            T y;
        }
        Bar<T> operator.z<T>(Foo<T> foo)
        {
            Bar<T> result;
            result.x = foo.x;
            result.y = foo.y[1];
            return result;
        }
        Foo<T> operator.z=<T>(Foo<T> foo, Bar<T> bar)
        {
            foo.x = bar.x;
            foo.y[1] = bar.y;
            return foo;
        }
        T operator.sum<T:Addable>(Foo<T> foo)
        {
            return foo.x + foo.y[0] + foo.y[1] + foo.y[2];
        }
        T operator.sum<T:Addable>(Bar<T> bar)
        {
            return bar.x + bar.y;
        }
        operator<T> Bar<T>(T x, T y)
        {
            Bar<T> result;
            result.x = x;
            result.y = y;
            return result;
        }
        void setup(thread Foo<int>^ foo)
        {
            foo->x = 1;
            foo->y[0] = 2;
            foo->y[1] = 3;
            foo->y[2] = 4;
        }
        int testSuperBasic()
        {
            Foo<int> foo;
            setup(&foo);
            return foo.sum;
        }
        int testZSetterDidSetY()
        {
            Foo<int> foo;
            foo.z = Bar<int>(53, 932);
            return foo.y[1];
        }
        int testZSetter()
        {
            Foo<int> foo;
            foo.z = Bar<int>(53, 932);
            return foo.sum;
        }
        int testZGetter()
        {
            Foo<int> foo;
            // This deliberately does not call setup() just so we test this syntax.
            foo.x = 1;
            foo.y[0] = 2;
            foo.y[1] = 3;
            foo.y[2] = 4;
            return foo.z.sum;
        }
        int testLValueEmulation()
        {
            Foo<int> foo;
            setup(&foo);
            foo.z.y *= 5;
            return foo.sum;
        }
    `);
    checkInt(program, callFunction(program, "testSuperBasic", [], []), 1 + 2 + 3 + 4);
    checkInt(program, callFunction(program, "testZSetterDidSetY", [], []), 932);
    checkInt(program, callFunction(program, "testZSetter", [], []), 53 + 932);
    checkInt(program, callFunction(program, "testZGetter", [], []), 1 + 3);
    checkInt(program, callFunction(program, "testLValueEmulation", [], []), 1 + 2 + 3 * 5 + 4);
}

function TEST_bitSubscriptAccessor()
{
    let program = doPrep(`
        protocol MyBitmaskable : Equatable {
            MyBitmaskable operator&(MyBitmaskable, MyBitmaskable);
            MyBitmaskable operator|(MyBitmaskable, MyBitmaskable);
            MyBitmaskable operator~(MyBitmaskable);
            MyBitmaskable operator<<(MyBitmaskable, uint);
            MyBitmaskable operator>>(MyBitmaskable, uint);
            operator MyBitmaskable(int);
        }
        T maskForBitIndex<T:MyBitmaskable>(uint index)
        {
            return T(1) << index;
        }
        bool operator[]<T:MyBitmaskable>(T value, uint index)
        {
            return bool(value & maskForBitIndex<T>(index));
        }
        T operator[]=<T:MyBitmaskable>(T value, uint index, bool bit)
        {
            T mask = maskForBitIndex<T>(index);
            if (bit)
                value |= mask;
            else
                value &= ~mask;
            return value;
        }
        uint operator.length(int)
        {
            return 32;
        }
        uint operator.length(uint)
        {
            return 32;
        }
        int testIntSetBit3()
        {
            int foo;
            foo[3] = true;
            return foo;
        }
        bool testIntSetGetBit5()
        {
            int foo;
            foo[5] = true;
            return foo[5];
        }
        bool testIntGetBit1()
        {
            int foo;
            return foo[1];
        }
        int testUintSumBits()
        {
            int foo = 42;
            int result;
            for (uint i = 0; i < foo.length; ++i) {
                if (foo[i])
                    result++;
            }
            return result;
        }
        int testUintSwapBits()
        {
            int foo = 42;
            for (uint i = 0; i < foo.length / 2; ++i) {
                bool tmp = foo[i];
                foo[i] = foo[foo.length - i - 1];
                foo[foo.length - i - 1] = tmp;
            }
            return foo;
        }
        struct Foo {
            uint f;
            uint g;
        }
        operator Foo(uint f, uint g)
        {
            Foo result;
            result.f = f;
            result.g = g;
            return result;
        }
        int operator.h(Foo foo)
        {
            return int((foo.f & 0xffff) | ((foo.g & 0xffff) << 16));
        }
        Foo operator.h=(Foo foo, int value)
        {
            foo.f &= ~0xffffu;
            foo.f |= uint(value) & 0xffff;
            foo.g &= ~0xffffu;
            foo.g |= (uint(value) >> 16) & 0xffff;
            return foo;
        }
        int testLValueEmulation()
        {
            Foo foo;
            foo.f = 42;
            foo.g = 37;
            for (uint i = 0; i < foo.h.length; ++i)
                foo.h[i] ^= true;
            return int(foo.f + foo.g);
        }
        struct Bar {
            Foo a;
            Foo b;
        }
        Foo operator.c(Bar bar)
        {
            return Foo(uint(bar.a.h), uint(bar.b.h));
        }
        Bar operator.c=(Bar bar, Foo foo)
        {
            bar.a.h = int(foo.f);
            bar.b.h = int(foo.g);
            return bar;
        }
        int testCrazyLValueEmulation()
        {
            Bar bar;
            bar.a.f = 1;
            bar.a.g = 2;
            bar.b.f = 3;
            bar.b.g = 4;
            for (uint i = 0; i < bar.c.h.length; i += 2)
                bar.c.h[i] ^= true;
            return int(bar.a.f + bar.a.g + bar.b.f + bar.b.g);
        }
    `);
    checkInt(program, callFunction(program, "testIntSetBit3", [], []), 8);
    checkBool(program, callFunction(program, "testIntSetGetBit5", [], []), true);
    checkBool(program, callFunction(program, "testIntGetBit1", [], []), false);
    checkInt(program, callFunction(program, "testUintSumBits", [], []), 3);
    checkInt(program, callFunction(program, "testUintSwapBits", [], []), 1409286144);
    checkInt(program, callFunction(program, "testLValueEmulation", [], []), 130991);
    checkInt(program, callFunction(program, "testCrazyLValueEmulation", [], []), 43696);
}

function TEST_nestedSubscriptLValueEmulationSimple()
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
        void setValues(thread Baz^ baz)
        {
            for (uint i = baz->length; i--;) {
                for (uint j = (^baz)[i].length; j--;) {
                    for (uint k = (^baz)[i][j].length; k--;)
                        (^baz)[i][j][k] = int(i + j + k);
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
    checkInt(program, callFunction(program, "testSetValuesAndSum", [], []), 1575);
    checkInt(program, callFunction(program, "testSetValuesMutateValuesAndSum", [], []), 5565);
}

function TEST_nestedSubscriptLValueEmulationGeneric()
{
    let program = doPrep(`
        struct Foo<T> {
            T[7] array;
        }
        T operator[]<T>(Foo<T> foo, uint index)
        {
            return foo.array[index];
        }
        Foo<T> operator[]=<T>(Foo<T> foo, uint index, T value)
        {
            foo.array[index] = value;
            return foo;
        }
        uint operator.length<T>(Foo<T> foo)
        {
            return foo.array.length;
        }
        protocol MyAddable {
            MyAddable operator+(MyAddable, MyAddable);
        }
        T sum<T:MyAddable>(Foo<T> foo)
        {
            T result;
            for (uint i = foo.length; i--;)
                result += foo[i];
            return result;
        }
        struct Bar<T> {
            Foo<T>[6] array;
        }
        uint operator.length<T>(Bar<T> bar)
        {
            return bar.array.length;
        }
        Foo<T> operator[]<T>(Bar<T> bar, uint index)
        {
            return bar.array[index];
        }
        Bar<T> operator[]=<T>(Bar<T> bar, uint index, Foo<T> value)
        {
            bar.array[index] = value;
            return bar;
        }
        T sum<T:MyAddable>(Bar<T> bar)
        {
            T result;
            for (uint i = bar.length; i--;)
                result += sum(bar[i]);
            return result;
        }
        struct Baz<T> {
            Bar<T>[5] array;
        }
        Bar<T> operator[]<T>(Baz<T> baz, uint index)
        {
            return baz.array[index];
        }
        Baz<T> operator[]=<T>(Baz<T> baz, uint index, Bar<T> value)
        {
            baz.array[index] = value;
            return baz;
        }
        uint operator.length<T>(Baz<T> baz)
        {
            return baz.array.length;
        }
        T sum<T:MyAddable>(Baz<T> baz)
        {
            T result;
            for (uint i = baz.length; i--;)
                result += sum(baz[i]);
            return result;
        }
        protocol MyConvertibleFromUint {
            operator MyConvertibleFromUint(uint);
        }
        protocol SetValuable : MyAddable, MyConvertibleFromUint { }
        void setValues<T:SetValuable>(thread Baz<T>^ baz)
        {
            for (uint i = baz->length; i--;) {
                for (uint j = (^baz)[i].length; j--;) {
                    for (uint k = (^baz)[i][j].length; k--;)
                        (^baz)[i][j][k] = T(i + j + k);
                }
            }
        }
        int testSetValuesAndSum()
        {
            Baz<int> baz;
            setValues(&baz);
            return sum(baz);
        }
        int testSetValuesMutateValuesAndSum()
        {
            Baz<int> baz;
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
    checkInt(program, callFunction(program, "testSetValuesAndSum", [], []), 1575);
    checkInt(program, callFunction(program, "testSetValuesMutateValuesAndSum", [], []), 5565);
}

function TEST_boolBitAnd()
{
    let program = doPrep(`
        bool foo(bool a, bool b)
        {
            return a & b;
        }
    `);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, false), makeBool(program, false)]), false);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, true), makeBool(program, false)]), false);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, false), makeBool(program, true)]), false);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, true), makeBool(program, true)]), true);
}

function TEST_boolBitOr()
{
    let program = doPrep(`
        bool foo(bool a, bool b)
        {
            return a | b;
        }
    `);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, false), makeBool(program, false)]), false);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, true), makeBool(program, false)]), true);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, false), makeBool(program, true)]), true);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, true), makeBool(program, true)]), true);
}

function TEST_boolBitXor()
{
    let program = doPrep(`
        bool foo(bool a, bool b)
        {
            return a ^ b;
        }
    `);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, false), makeBool(program, false)]), false);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, true), makeBool(program, false)]), true);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, false), makeBool(program, true)]), true);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, true), makeBool(program, true)]), false);
}

function TEST_boolBitNot()
{
    let program = doPrep(`
        bool foo(bool a)
        {
            return ~a;
        }
    `);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, false)]), true);
    checkBool(program, callFunction(program, "foo", [], [makeBool(program, true)]), false);
}

function TEST_intBitAnd()
{
    let program = doPrep(`
        int foo(int a, int b)
        {
            return a & b;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 1), makeInt(program, 7)]), 1);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 65535), makeInt(program, 42)]), 42);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, -1), makeInt(program, -7)]), -7);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 0), makeInt(program, 85732)]), 0);
}

function TEST_intBitOr()
{
    let program = doPrep(`
        int foo(int a, int b)
        {
            return a | b;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 1), makeInt(program, 7)]), 7);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 65535), makeInt(program, 42)]), 65535);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, -1), makeInt(program, -7)]), -1);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 0), makeInt(program, 85732)]), 85732);
}

function TEST_intBitXor()
{
    let program = doPrep(`
        int foo(int a, int b)
        {
            return a ^ b;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 1), makeInt(program, 7)]), 6);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 65535), makeInt(program, 42)]), 65493);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, -1), makeInt(program, -7)]), 6);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 0), makeInt(program, 85732)]), 85732);
}

function TEST_intBitNot()
{
    let program = doPrep(`
        int foo(int a)
        {
            return ~a;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 1)]), -2);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 65535)]), -65536);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, -1)]), 0);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 0)]), -1);
}

function TEST_intLShift()
{
    let program = doPrep(`
        int foo(int a, uint b)
        {
            return a << b;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 1), makeUint(program, 7)]), 128);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 65535), makeUint(program, 2)]), 262140);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, -1), makeUint(program, 5)]), -32);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 0), makeUint(program, 3)]), 0);
}

function TEST_intRShift()
{
    let program = doPrep(`
        int foo(int a, uint b)
        {
            return a >> b;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 1), makeUint(program, 7)]), 0);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 65535), makeUint(program, 2)]), 16383);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, -1), makeUint(program, 5)]), -1);
    checkInt(program, callFunction(program, "foo", [], [makeInt(program, 0), makeUint(program, 3)]), 0);
}

function TEST_uintBitAnd()
{
    let program = doPrep(`
        uint foo(uint a, uint b)
        {
            return a & b;
        }
    `);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 1), makeUint(program, 7)]), 1);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 65535), makeUint(program, 42)]), 42);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, -1), makeUint(program, -7)]), 4294967289);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 0), makeUint(program, 85732)]), 0);
}

function TEST_uintBitOr()
{
    let program = doPrep(`
        uint foo(uint a, uint b)
        {
            return a | b;
        }
    `);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 1), makeUint(program, 7)]), 7);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 65535), makeUint(program, 42)]), 65535);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, -1), makeUint(program, -7)]), 4294967295);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 0), makeUint(program, 85732)]), 85732);
}

function TEST_uintBitXor()
{
    let program = doPrep(`
        uint foo(uint a, uint b)
        {
            return a ^ b;
        }
    `);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 1), makeUint(program, 7)]), 6);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 65535), makeUint(program, 42)]), 65493);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, -1), makeUint(program, -7)]), 6);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 0), makeUint(program, 85732)]), 85732);
}

function TEST_uintBitNot()
{
    let program = doPrep(`
        uint foo(uint a)
        {
            return ~a;
        }
    `);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 1)]), 4294967294);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 65535)]), 4294901760);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, -1)]), 0);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 0)]), 4294967295);
}

function TEST_uintLShift()
{
    let program = doPrep(`
        uint foo(uint a, uint b)
        {
            return a << b;
        }
    `);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 1), makeUint(program, 7)]), 128);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 65535), makeUint(program, 2)]), 262140);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, -1), makeUint(program, 5)]), 4294967264);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 0), makeUint(program, 3)]), 0);
}

function TEST_uintRShift()
{
    let program = doPrep(`
        uint foo(uint a, uint b)
        {
            return a >> b;
        }
    `);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 1), makeUint(program, 7)]), 0);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 65535), makeUint(program, 2)]), 16383);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, -1), makeUint(program, 5)]), 134217727);
    checkUint(program, callFunction(program, "foo", [], [makeUint(program, 0), makeUint(program, 3)]), 0);
}

function TEST_floatMath()
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
            return 42.5d == 42.5d;
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
        float foo8()
        {
            return bar(7.5d);
        }
        float foo9()
        {
            return float(7.5);
        }
        float foo10()
        {
            return float(7.5f);
        }
        float foo11()
        {
            return float(7.5d);
        }
        float foo12()
        {
            return float(7);
        }
        float foo13()
        {
            double x = 7.5d;
            return float(x);
        }
        double foo14()
        {
            double x = 7.5f;
            return double(x);
        }
    `);
    checkBool(program, callFunction(program, "foo", [], []), true);
    checkBool(program, callFunction(program, "foo2", [], []), true);
    checkBool(program, callFunction(program, "foo3", [], []), true);
    checkBool(program, callFunction(program, "foo4", [], []), true);
    checkBool(program, callFunction(program, "foo5", [], []), true);
    checkFloat(program, callFunction(program, "foo6", [], []), 7.5);
    checkFloat(program, callFunction(program, "foo7", [], []), 7.5);
    checkFloat(program, callFunction(program, "foo8", [], []), 7.5);
    checkFloat(program, callFunction(program, "foo9", [], []), 7.5);
    checkFloat(program, callFunction(program, "foo10", [], []), 7.5);
    checkFloat(program, callFunction(program, "foo11", [], []), 7.5);
    checkFloat(program, callFunction(program, "foo12", [], []), 7);
    checkFloat(program, callFunction(program, "foo13", [], []), 7.5);
    checkDouble(program, callFunction(program, "foo14", [], []), 7.5);
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
                bar(4.d);
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
                bar(4.d);
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
            float bar(float x)
            {
                return x;
            }
            void foo()
            {
                bar(16777217.d);
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            float bar(float x)
            {
                return x;
            }
            float foo()
            {
                double x = 7.;
                return bar(x);
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            float foo()
            {
                double x = 7.;
                return x;
            }
        `),
        (e) => e instanceof WTypeError);
}

function TEST_genericCastInfer()
{
    let program = doPrep(`
        struct Complex<T> {
            T real;
            T imag;
        }
        operator<T> Complex<T>(T real, T imag)
        {
            Complex<T> result;
            result.real = real;
            result.imag = imag;
            return result;
        }
        int foo()
        {
            Complex<int> x = Complex<int>(1, 2);
            return x.real + x.imag;
        }
    `);
    checkInt(program, callFunction(program, "foo", [], []), 3);
}

function TEST_booleanMath()
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
    checkBool(program, callFunction(program, "foo", [], []), true);
    checkBool(program, callFunction(program, "foo2", [], []), false);
    checkBool(program, callFunction(program, "foo3", [], []), false);
    checkBool(program, callFunction(program, "foo4", [], []), false);
    checkBool(program, callFunction(program, "foo5", [], []), true);
    checkBool(program, callFunction(program, "foo6", [], []), true);
    checkBool(program, callFunction(program, "foo7", [], []), true);
    checkBool(program, callFunction(program, "foo8", [], []), false);
}

function TEST_typedefArray()
{
    let program = doPrep(`
        typedef ArrayTypedef = int[2];
        int foo()
        {
            ArrayTypedef arrayTypedef;
            return arrayTypedef[0];
        }
    `);
    checkInt(program, callFunction(program, "foo", [], []), 0);
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

function* doTest(object)
{
    let before = preciseTime();

    for (let s in object) {
        if (s.startsWith("TEST_") && s.match(filter)) {
            print(s + "...");
            object[s]();
            print("    OK!");
            yield;
        }
    }

    let after = preciseTime();
    
    print("Success!");
    print("That took " + (after - before) * 1000 + " ms.");
}

if (!this.window)
    for (let _ of doTest(this)) { }


