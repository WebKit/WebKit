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
    return TypedValue.box(program.intrinsics.int, castAndCheckValue(castToInt, value));
}

function makeUint(program, value)
{
    return TypedValue.box(program.intrinsics.uint, castAndCheckValue(castToUint, value));
}

function makeUchar(program, value)
{
    return TypedValue.box(program.intrinsics.uchar, castAndCheckValue(castToUchar, value));
}

function makeBool(program, value)
{
    return TypedValue.box(program.intrinsics.bool, castAndCheckValue(castToBool, value));
}

function makeFloat(program, value)
{
    return TypedValue.box(program.intrinsics.float, castAndCheckValue(castToFloat, value));
}

function makeCastedFloat(program, value)
{
    return TypedValue.box(program.intrinsics.float, castToFloat(value));
}

function makeHalf(program, value)
{
    return TypedValue.box(program.intrinsics.half, castAndCheckValue(castToHalf, value));
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

function makeSampler(program, options)
{
    // enum WebGPUAddressMode {
    //     "clampToEdge",
    //     "repeat",
    //     "mirrorRepeat",
    //     "clampToBorderColor"
    // }
    //
    // enum WebGPUFilterMode {
    //     "nearest",
    //     "linear"
    // }
    //
    // enum WebGPUCompareFunction {
    //     "never",
    //     "less",
    //     "equal",
    //     "lessEqual",
    //     "greater",
    //     "notEqual",
    //     "greaterEqual",
    //     "always"
    // }
    //
    // enum WebGPUBorderColor {
    //     "transparentBlack",
    //     "opaqueBlack",
    //     "opaqueWhite"
    // }
    //
    // dictionary WebGPUSamplerDescriptor {
    //     WebGPUddressMode rAddressMode = "clampToEdge";
    //     WebGPUddressMode sAddressMode = "clampToEdge";
    //     WebGPUddressMode tAddressMode = "clampToEdge";
    //     WebGPUFilterModeEnum magFilter = "nearest";
    //     WebGPUFilterModeEnum minFilter = "nearest";
    //     WebGPUFilterModeEnum mipmapFilter = "nearest";
    //     float lodMinClamp = 0;
    //     float lodMaxClamp = Number.MAX_VALUE;
    //     unsigned long maxAnisotropy = 1;
    //     WebGPUCompareFunction compareFunction = "never";
    //     WebGPUBorderColor borderColor = "transparentBlack";
    // };
    return TypedValue.box(program.intrinsics.sampler, new Sampler(options));
}

function make1DTexture(program, mipmaps, elementType)
{
    return TypedValue.box(program.intrinsics[`Texture1D<${elementType}>`], new Texture1D(elementType, mipmaps));
}

function make1DTextureArray(program, array, elementType)
{
    return TypedValue.box(program.intrinsics[`Texture1DArray<${elementType}>`], new Texture1DArray(elementType, array));
}

function make2DTexture(program, mipmaps, elementType)
{
    return TypedValue.box(program.intrinsics[`Texture2D<${elementType}>`], new Texture2D(elementType, mipmaps));
}

function make2DTextureArray(program, array, elementType)
{
    return TypedValue.box(program.intrinsics[`Texture2DArray<${elementType}>`], new Texture2DArray(elementType, array));
}

function make3DTexture(program, mipmaps, elementType)
{
    return TypedValue.box(program.intrinsics[`Texture3D<${elementType}>`], new Texture3D(elementType, mipmaps));
}

function makeTextureCube(program, array, elementType)
{
    return TypedValue.box(program.intrinsics[`TextureCube<${elementType}>`], new TextureCube(elementType, array));
}

function makeRW1DTexture(program, elements, elementType)
{
    return TypedValue.box(program.intrinsics[`RWTexture1D<${elementType}>`], new Texture1DRW(elementType, elements));
}

function makeRW1DTextureArray(program, array, elementType)
{
    return TypedValue.box(program.intrinsics[`RWTexture1DArray<${elementType}>`], new Texture1DArrayRW(elementType, array));
}

function makeRW2DTexture(program, rows, elementType)
{
    return TypedValue.box(program.intrinsics[`RWTexture2D<${elementType}>`], new Texture2DRW(elementType, rows));
}

function makeRW2DTextureArray(program, array, elementType)
{
    return TypedValue.box(program.intrinsics[`RWTexture2DArray<${elementType}>`], new Texture2DArrayRW(elementType, array));
}

function makeRW3DTexture(program, depthSlices, elementType)
{
    return TypedValue.box(program.intrinsics[`RWTexture3D<${elementType}>`], new Texture3DRW(elementType, depthSlices));
}

function make2DDepthTexture(program, mipmaps, elementType)
{
    return TypedValue.box(program.intrinsics[`TextureDepth2D<${elementType}>`], new TextureDepth2D(elementType, mipmaps));
}

function make2DDepthTextureArray(program, array, elementType)
{
    return TypedValue.box(program.intrinsics[`TextureDepth2DArray<${elementType}>`], new TextureDepth2DArray(elementType, array));
}

function makeDepthTextureCube(program, array, elementType)
{
    return TypedValue.box(program.intrinsics[`TextureDepthCube<${elementType}>`], new TextureDepthCube(elementType, array));
}

function makeRW2DDepthTexture(program, rows, elementType)
{
    return TypedValue.box(program.intrinsics[`RWTextureDepth2D<${elementType}>`], new TextureDepth2DRW(elementType, rows));
}

function makeRW2DDepthTextureArray(program, array, elementType)
{
    return TypedValue.box(program.intrinsics[`RWTextureDepth2DArray<${elementType}>`], new TextureDepth2DArrayRW(elementType, array));
}

function checkInt(program, result, expected)
{
    if (!result.type.equals(program.intrinsics.int))
        throw new Error("Wrong result type; result: " + result);
    if (result.value != expected)
        throw new Error(`Wrong result: ${result.value} (expected ${expected})`);
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

function checkFloat4(program, result, expected)
{
    if (!result.type.equals(program.intrinsics["vector<float, 4>"]))
        throw new Error("Wrong result type: " + result.type);
    if (result.ePtr.get(0) != expected[0] || result.ePtr.get(1) != expected[1] || result.ePtr.get(2) != expected[2] || result.ePtr.get(3) != expected[3])
        throw new Error("Wrong result: [" + result.ePtr.get(0) + ", " + result.ePtr.get(1) + ", " + result.ePtr.get(2) + ", " + result.ePtr.get(3) + "] (expected [" + expected[0] + ", " + expected[1] + ", " + expected[2] + ", " + expected[3] + "])");
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

function checkTrap(program, callback, checkFunction)
{
    const result = callback();
    // FIXME: Rewrite tests so that they return non-zero values in the case that they didn't trap.
    // The check function is optional in the case of a void return type.
    checkFunction(program, result, 0);
    if (!Evaluator.lastInvocationDidTrap)
        throw new Error("Did not trap");
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

tests.ternaryExpression = function() {
    let program = doPrep(`
        test int foo(int x)
        {
            return x < 3 ? 4 : 5;
        }
        test int bar(int x)
        {
            return x < 10 ? 11 : x < 12 ? 14 : 15;
        }
        test int baz(int x)
        {
            return 3 < 4 ? x : 5;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 767)]), 5);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 2)]), 4);
    checkInt(program, callFunction(program, "bar", [makeInt(program, 8)]), 11);
    checkInt(program, callFunction(program, "bar", [makeInt(program, 9)]), 11);
    checkInt(program, callFunction(program, "bar", [makeInt(program, 10)]), 14);
    checkInt(program, callFunction(program, "bar", [makeInt(program, 11)]), 14);
    checkInt(program, callFunction(program, "bar", [makeInt(program, 12)]), 15);
    checkInt(program, callFunction(program, "bar", [makeInt(program, 13)]), 15);
    checkInt(program, callFunction(program, "baz", [makeInt(program, 14)]), 14);
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
                int y;
                (0 < 1 ? x : y) = 42;
                return x;
            }
        `),
        (e) => e instanceof WTypeError && e.message.indexOf("not an LValue") != -1);
    checkFail(
        () => doPrep(`
            int foo()
            {
                int x;
                int y;
                thread int* z = &(0 < 1 ? x : y);
                return *z;
            }
        `),
        (e) => e instanceof WTypeError && e.message.indexOf("not an LValue") != -1);
    checkFail(
        () => doPrep(`
            int foo()
            {
                int x;
                int y;
                thread int[] z = @(0 < 1 ? x : y);
                return *z;
            }
        `),
        (e) => e instanceof WTypeError && e.message.indexOf("not an LValue") != -1);
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
    let program = doPrep("test bool foo() { return true; }");
    checkBool(program, callFunction(program, "foo", []), true);
}

tests.identityBool = function() {
    let program = doPrep("test bool foo(bool x) { return x; }");
    checkBool(program, callFunction(program, "foo", [makeBool(program, true)]), true);
    checkBool(program, callFunction(program, "foo", [makeBool(program, false)]), false);
}

tests.intSimpleMath = function() {
    let program = doPrep("test int foo(int x, int y) { return x + y; }");
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7), makeInt(program, 5)]), 12);
    program = doPrep("test int foo(int x, int y) { return x - y; }");
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7), makeInt(program, 5)]), 2);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 5), makeInt(program, 7)]), -2);
    program = doPrep("test int foo(int x, int y) { return x * y; }");
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7), makeInt(program, 5)]), 35);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7), makeInt(program, -5)]), -35);
    program = doPrep("test int foo(int x, int y) { return x / y; }");
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7), makeInt(program, 2)]), 3);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 7), makeInt(program, -2)]), -3);
}

tests.incrementAndDecrement = function() {
    let program = doPrep(`
    test int foo1() { int x = 0; return x++; }
    test int foo2() { int x = 0; x++; return x; }
    test int foo3() { int x = 0; return ++x; }
    test int foo4() { int x = 0; ++x; return x; }
    test int foo5() { int x = 0; return x--; }
    test int foo6() { int x = 0; x--; return x; }
    test int foo7() { int x = 0; return --x; }
    test int foo8() { int x = 0; --x; return x; }
    `);
    checkInt(program, callFunction(program, "foo1", []), 0);
    checkInt(program, callFunction(program, "foo2", []), 1);
    checkInt(program, callFunction(program, "foo3", []), 1);
    checkInt(program, callFunction(program, "foo4", []), 1);
    checkInt(program, callFunction(program, "foo5", []), 0);
    checkInt(program, callFunction(program, "foo6", []), -1);
    checkInt(program, callFunction(program, "foo7", []), -1);
    checkInt(program, callFunction(program, "foo8", []), -1);
}

tests.uintSimpleMath = function() {
    let program = doPrep("test uint foo(uint x, uint y) { return x + y; }");
    checkUint(program, callFunction(program, "foo", [makeUint(program, 7), makeUint(program, 5)]), 12);
    program = doPrep("test uint foo(uint x, uint y) { return x - y; }");
    checkUint(program, callFunction(program, "foo", [makeUint(program, 7), makeUint(program, 5)]), 2);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 5), makeUint(program, 7)]), 4294967294);
    program = doPrep("test uint foo(uint x, uint y) { return x * y; }");
    checkUint(program, callFunction(program, "foo", [makeUint(program, 7), makeUint(program, 5)]), 35);
    program = doPrep("test uint foo(uint x, uint y) { return x / y; }");
    checkUint(program, callFunction(program, "foo", [makeUint(program, 7), makeUint(program, 2)]), 3);
}

tests.ucharSimpleMath = function() {
    let program = doPrep("test uchar foo(uchar x, uchar y) { return x + y; }");
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 7), makeUchar(program, 5)]), 12);
    program = doPrep("test uchar foo(uchar x, uchar y) { return x - y; }");
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 7), makeUchar(program, 5)]), 2);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 5), makeUchar(program, 7)]), 254);
    program = doPrep("test uchar foo(uchar x, uchar y) { return x * y; }");
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 7), makeUchar(program, 5)]), 35);
    program = doPrep("test uchar foo(uchar x, uchar y) { return x / y; }");
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 7), makeUchar(program, 2)]), 3);
}

tests.equality = function() {
    let program = doPrep("test bool foo(uint x, uint y) { return x == y; }");
    checkBool(program, callFunction(program, "foo", [makeUint(program, 7), makeUint(program, 5)]), false);
    checkBool(program, callFunction(program, "foo", [makeUint(program, 7), makeUint(program, 7)]), true);
    program = doPrep("test bool foo(uchar x, uchar y) { return x == y; }");
    checkBool(program, callFunction(program, "foo", [makeUchar(program, 7), makeUchar(program, 5)]), false);
    checkBool(program, callFunction(program, "foo", [makeUchar(program, 7), makeUchar(program, 7)]), true);
    program = doPrep("test bool foo(int x, int y) { return x == y; }");
    checkBool(program, callFunction(program, "foo", [makeInt(program, 7), makeInt(program, 5)]), false);
    checkBool(program, callFunction(program, "foo", [makeInt(program, 7), makeInt(program, 7)]), true);
    program = doPrep("test bool foo(bool x, bool y) { return x == y; }");
    checkBool(program, callFunction(program, "foo", [makeBool(program, false), makeBool(program, true)]), false);
    checkBool(program, callFunction(program, "foo", [makeBool(program, true), makeBool(program, false)]), false);
    checkBool(program, callFunction(program, "foo", [makeBool(program, false), makeBool(program, false)]), true);
    checkBool(program, callFunction(program, "foo", [makeBool(program, true), makeBool(program, true)]), true);
}

tests.logicalNegation = function()
{
    let program = doPrep("test bool foo(bool x) { return !x; }");
    checkBool(program, callFunction(program, "foo", [makeBool(program, true)]), false);
    checkBool(program, callFunction(program, "foo", [makeBool(program, false)]), true);
}

tests.notEquality = function() {
    let program = doPrep("test bool foo(uint x, uint y) { return x != y; }");
    checkBool(program, callFunction(program, "foo", [makeUint(program, 7), makeUint(program, 5)]), true);
    checkBool(program, callFunction(program, "foo", [makeUint(program, 7), makeUint(program, 7)]), false);
    program = doPrep("test bool foo(uchar x, uchar y) { return x != y; }");
    checkBool(program, callFunction(program, "foo", [makeUchar(program, 7), makeUchar(program, 5)]), true);
    checkBool(program, callFunction(program, "foo", [makeUchar(program, 7), makeUchar(program, 7)]), false);
    program = doPrep("test bool foo(int x, int y) { return x != y; }");
    checkBool(program, callFunction(program, "foo", [makeInt(program, 7), makeInt(program, 5)]), true);
    checkBool(program, callFunction(program, "foo", [makeInt(program, 7), makeInt(program, 7)]), false);
    program = doPrep("test bool foo(bool x, bool y) { return x != y; }");
    checkBool(program, callFunction(program, "foo", [makeBool(program, false), makeBool(program, true)]), true);
    checkBool(program, callFunction(program, "foo", [makeBool(program, true), makeBool(program, false)]), true);
    checkBool(program, callFunction(program, "foo", [makeBool(program, false), makeBool(program, false)]), false);
    checkBool(program, callFunction(program, "foo", [makeBool(program, true), makeBool(program, true)]), false);
}

tests.equalityTypeFailure = function()
{
    checkFail(
        () => doPrep("test bool foo(int x, uint y) { return x == y; }"),
        (e) => e instanceof WTypeError && e.message.indexOf("/internal/test:1") != -1);
}

tests.generalNegation = function()
{
    let program = doPrep("test bool foo(int x) { return !x; }");
    checkBool(program, callFunction(program, "foo", [makeInt(program, 7)]), false);
    checkBool(program, callFunction(program, "foo", [makeInt(program, 0)]), true);
}

tests.add1 = function() {
    let program = doPrep("test int foo(int x) { return x + 1; }");
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
        test int foo(int p)
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
        test int foo(int p)
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
        test int foo()
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
        test int foo(device int* p)
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
        test void foo(device int* p)
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
        test thread int* foo()
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
        test int foo(thread int[] array)
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
        test int foo(thread int[] array)
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
        test int foo(device int[] array)
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
        test void foo(thread int[] array, int value)
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
        test void foo(device int[] array, int value)
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
        test void foo(device int[] array, int value)
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
        test Foo foo(Foo foo)
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
        test thread int* foo() { return null; }
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
        test int foo()
        {
            thread int* p;
            return *p;
        }
    `);
    checkTrap(program, () => callFunction(program, "foo", []), checkInt);
}

tests.defaultInitializedNull = function()
{
    let program = doPrep(`
        test int foo()
        {
            thread int* p = null;;
            return *p;
        }
    `);
    checkTrap(program, () => callFunction(program, "foo", []), checkInt);
}

tests.passNullToPtrMonomorphic = function()
{
    let program = doPrep(`
        int foo(thread int* ptr)
        {
            return *ptr;
        }
        test int bar()
        {
            return foo(null);
        }
    `);
    checkTrap(program, () => callFunction(program, "bar", []), checkInt);
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
        test thread int[] foo() { return null; }
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
        test int foo()
        {
            thread int[] p;
            return p[0u];
        }
    `);
    checkTrap(program, () => callFunction(program, "foo", []), checkInt);
}

tests.defaultInitializedNullArrayRef = function()
{
    let program = doPrep(`
        test int foo()
        {
            thread int[] p = null;
            return p[0u];
        }
    `);
    checkTrap(program, () => callFunction(program, "foo", []), checkInt);
}

tests.defaultInitializedNullArrayRefIntLiteral = function()
{
    let program = doPrep(`
        test int foo()
        {
            thread int[] p = null;
            return p[0];
        }
    `);
    checkTrap(program, () => callFunction(program, "foo", []), checkInt);
}

tests.passNullToPtrMonomorphicArrayRef = function()
{
    let program = doPrep(`
        int foo(thread int[] ptr)
        {
            return ptr[0u];
        }
        test int bar()
        {
            return foo(null);
        }
    `);
    checkTrap(program, () => callFunction(program, "bar", []), checkInt);
}

tests.returnIntLiteralUint = function()
{
    let program = doPrep("test uint foo() { return 42; }");
    checkUint(program, callFunction(program, "foo", []), 42);
}

tests.returnIntLiteralFloat = function()
{
    let program = doPrep("test float foo() { return 42; }");
    checkFloat(program, callFunction(program, "foo", []), 42);
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
        test bool foo(bool x)
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
        test int foo()
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
        test int foo()
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
        test int foo(int x)
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
        test int foo(int x)
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
        test int foo(int x)
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
        test int foo(int x)
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
        test int foo(int x)
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
        test int foo(int x)
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
        test int foo(int x)
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
        test int foo(int x)
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
        test int bar() { return foo(42); }
    `);
    checkInt(program, callFunction(program, "bar", []), 1);
}

tests.intOverloadResolutionReverseOrder = function()
{
    let program = doPrep(`
        int foo(float) { return 3; }
        int foo(uint) { return 2; }
        int foo(int) { return 1; }
        test int bar() { return foo(42); }
    `);
    checkInt(program, callFunction(program, "bar", []), 1);
}

tests.break = function()
{
    let program = doPrep(`
        test int foo1(int x)
        {
            while (true) {
                x = x * 2;
                if (x >= 7)
                    break;
            }
            return x;
        }
        test int foo2(int x)
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
        test int foo3(int x)
        {
            while (true) {
                if (x == 7) {
                    break;
                }
                x = x + 1;
            }
            return x;
        }
        test int foo4(int x)
        {
            while (true) {
                break;
            }
            return x;
        }
        test int foo5()
        {
            while (true) {
                return 7;
            }
        }
    `);
    checkInt(program, callFunction(program, "foo1", [makeInt(program, 1)]), 8);
    checkInt(program, callFunction(program, "foo1", [makeInt(program, 10)]), 20);
    checkInt(program, callFunction(program, "foo2", [makeInt(program, 1)]), 7);
    checkInt(program, callFunction(program, "foo2", [makeInt(program, 10)]), 19);
    checkInt(program, callFunction(program, "foo3", [makeInt(program, 1)]), 7);
    checkInt(program, callFunction(program, "foo4", [makeInt(program, 1)]), 1);
    checkInt(program, callFunction(program, "foo5", []), 7);
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
    checkFail(
        () => doPrep(`
            test int foo(int x)
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
        test int foo(int x)
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
        test int foo1(int x)
        {
            int y = 7;
            do {
                y = 8;
                break;
            } while (x < 10);
            return y;
        }
        test int foo2(int x)
        {
            int y = 7;
            do {
                y = 8;
                break;
            } while (y == 7);
            return y;
        }
        test int foo3(int x)
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
    checkInt(program, callFunction(program, "foo1", [makeInt(program, 1)]), 8);
    checkInt(program, callFunction(program, "foo1", [makeInt(program, 11)]), 8);
    checkInt(program, callFunction(program, "foo2", [makeInt(program, 1)]), 8);
    checkInt(program, callFunction(program, "foo3", [makeInt(program, 9)]), 19);
}

tests.forLoop = function()
{
    let program = doPrep(`
        test int foo1(int x)
        {
            int sum = 0;
            int i;
            for (i = 0; i < x; i = i + 1) {
                sum = sum + i;
            }
            return sum;
        }
        test int foo2(int x)
        {
            int sum = 0;
            for (int i = 0; i < x; i = i + 1) {
                sum = sum + i;
            }
            return sum;
        }
        test int foo3(int x)
        {
            int sum = 0;
            int i = 100;
            for (int i = 0; i < x; i = i + 1) {
                sum = sum + i;
            }
            return sum;
        }
        test int foo4(int x)
        {
            int sum = 0;
            for (int i = 0; i < x; i = i + 1) {
                if (i == 4)
                    continue;
                sum = sum + i;
            }
            return sum;
        }
        test int foo5(int x)
        {
            int sum = 0;
            for (int i = 0; i < x; i = i + 1) {
                if (i == 5)
                    break;
                sum = sum + i;
            }
            return sum;
        }
        test int foo6(int x)
        {
            int sum = 0;
            for (int i = 0; ; i = i + 1) {
                if (i >= x)
                    break;
                sum = sum + i;
            }
            return sum;
        }
        test int foo7(int x)
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
        test int foo8(int x)
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
        test int foo9(int x)
        {
            for ( ; ; ) {
                return 7;
            }
        }
        test int foo10(int x)
        {
            for ( ; true; ) {
                return 7;
            }
        }
    `);
    checkInt(program, callFunction(program, "foo1", [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo1", [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo1", [makeInt(program, 5)]), 10);
    checkInt(program, callFunction(program, "foo2", [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo2", [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo2", [makeInt(program, 5)]), 10);
    checkInt(program, callFunction(program, "foo3", [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo3", [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo3", [makeInt(program, 5)]), 10);
    checkInt(program, callFunction(program, "foo4", [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo4", [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo4", [makeInt(program, 5)]), 6);
    checkInt(program, callFunction(program, "foo4", [makeInt(program, 6)]), 11);
    checkInt(program, callFunction(program, "foo5", [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo5", [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo5", [makeInt(program, 5)]), 10);
    checkInt(program, callFunction(program, "foo5", [makeInt(program, 6)]), 10);
    checkInt(program, callFunction(program, "foo5", [makeInt(program, 7)]), 10);
    checkInt(program, callFunction(program, "foo6", [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo6", [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo6", [makeInt(program, 5)]), 10);
    checkInt(program, callFunction(program, "foo6", [makeInt(program, 6)]), 15);
    checkInt(program, callFunction(program, "foo6", [makeInt(program, 7)]), 21);
    checkInt(program, callFunction(program, "foo7", [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo7", [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo7", [makeInt(program, 5)]), 10);
    checkInt(program, callFunction(program, "foo7", [makeInt(program, 6)]), 15);
    checkInt(program, callFunction(program, "foo7", [makeInt(program, 7)]), 21);
    checkInt(program, callFunction(program, "foo8", [makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo8", [makeInt(program, 4)]), 6);
    checkInt(program, callFunction(program, "foo8", [makeInt(program, 5)]), 10);
    checkInt(program, callFunction(program, "foo8", [makeInt(program, 6)]), 15);
    checkInt(program, callFunction(program, "foo8", [makeInt(program, 7)]), 21);
    checkInt(program, callFunction(program, "foo9", [makeInt(program, 3)]), 7);
    checkInt(program, callFunction(program, "foo9", [makeInt(program, 4)]), 7);
    checkInt(program, callFunction(program, "foo9", [makeInt(program, 5)]), 7);
    checkInt(program, callFunction(program, "foo9", [makeInt(program, 6)]), 7);
    checkInt(program, callFunction(program, "foo9", [makeInt(program, 7)]), 7);
    checkInt(program, callFunction(program, "foo10", [makeInt(program, 3)]), 7);
    checkInt(program, callFunction(program, "foo10", [makeInt(program, 4)]), 7);
    checkInt(program, callFunction(program, "foo10", [makeInt(program, 5)]), 7);
    checkInt(program, callFunction(program, "foo10", [makeInt(program, 6)]), 7);
    checkInt(program, callFunction(program, "foo10", [makeInt(program, 7)]), 7);
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
}

tests.prefixPlusPlus = function()
{
    let program = doPrep(`
        test int foo(int x)
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
        test int foo(int x)
        {
            return ++x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 64)]), 65);
}

tests.postfixPlusPlus = function()
{
    let program = doPrep(`
        test int foo(int x)
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
        test int foo(int x)
        {
            return x++;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 64)]), 64);
}

tests.prefixMinusMinus = function()
{
    let program = doPrep(`
        test int foo(int x)
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
        test int foo(int x)
        {
            return --x;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 64)]), 63);
}

tests.postfixMinusMinus = function()
{
    let program = doPrep(`
        test int foo(int x)
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
        test int foo(int x)
        {
            return x--;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 64)]), 64);
}

tests.plusEquals = function()
{
    let program = doPrep(`
        test int foo(int x)
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
        test int foo(int x)
        {
            return x += 42;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 385)]), 385 + 42);
}

tests.minusEquals = function()
{
    let program = doPrep(`
        test int foo(int x)
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
        test int foo(int x)
        {
            return x -= 42;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 385)]), 385 - 42);
}

tests.timesEquals = function()
{
    let program = doPrep(`
        test int foo(int x)
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
        test int foo(int x)
        {
            return x *= 42;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 385)]), 385 * 42);
}

tests.divideEquals = function()
{
    let program = doPrep(`
        test int foo(int x)
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
        test int foo(int x)
        {
            return x /= 42;
        }
    `);
    checkInt(program, callFunction(program, "foo", [makeInt(program, 385)]), (385 / 42) | 0);
}

tests.twoIntLiterals = function()
{
    let program = doPrep(`
        test bool foo()
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
        test int foo()
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
        test int foo()
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
        test int foo()
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
        test int foo()
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
        test int foo()
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
        test int foo()
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
        test int foo()
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
        test uint foo()
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
        test uint foo()
        {
            float[754] array;
            return array.length;
        }
        test uint bar()
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
        test int foo()
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
        test int foo()
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
        test int testSetValuesAndSum()
        {
            Baz baz;
            setValues(&baz);
            return sum(baz);
        }
        test int testSetValuesMutateValuesAndSum()
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

tests.nestedSubscriptWithArraysInStructs = function()
{
    let program = doPrep(`
        struct Foo {
            int[7] array;
        }
        int sum(Foo foo)
        {
            int result = 0;
            for (uint i = 0; i < foo.array.length; i++)
                result += foo.array[i];
            return result;
        }
        struct Bar {
            Foo[6] array;
        }
        int sum(Bar bar)
        {
            int result = 0;
            for (uint i = 0; i < bar.array.length; i++)
                result += sum(bar.array[i]);
            return result;
        }
        struct Baz {
            Bar[5] array;
        }
        int sum(Baz baz)
        {
            int result = 0;
            for (uint i = 0; i < baz.array.length; i++)
                result += sum(baz.array[i]);
            return result;
        }
        void setValues(thread Baz* baz)
        {
            for (uint i = 0; i < baz->array.length; i++) {
                for (uint j = 0; j < baz->array[i].array.length; j++) {
                    for (uint k = 0; k < baz->array[i].array[j].array.length; k++)
                        baz->array[i].array[j].array[k] = int(i + j + k);
                }
            }
        }
        test int testSetValuesAndSum()
        {
            Baz baz;
            setValues(&baz);
            return sum(baz);
        }
        test int testSetValuesMutateValuesAndSum()
        {
            Baz baz;
            setValues(&baz);
            for (uint i = baz.array.length; i--;) {
                for (uint j = baz.array[i].array.length; j--;) {
                    for (uint k = baz.array[i].array[j].array.length; k--;)
                        baz.array[i].array[j].array[k] = baz.array[i].array[j].array[k] * int(k);
                }
            }
            return sum(baz);
        }
    `);
    checkInt(program, callFunction(program, "testSetValuesAndSum", []), 1575);
    checkInt(program, callFunction(program, "testSetValuesMutateValuesAndSum", []), 5565);
}

tests.nestedSubscript = function()
{
    let program = doPrep(`
        int sum(int[7] array)
        {
            int result = 0;
            for (uint i = array.length; i--;)
                result += array[i];
            return result;
        }
        int sum(int[6][7] array)
        {
            int result = 0;
            for (uint i = array.length; i--;)
                result += sum(array[i]);
            return result;
        }
        int sum(int[5][6][7] array)
        {
            int result = 0;
            for (uint i = array.length; i--;)
                result += sum(array[i]);
            return result;
        }
        void setValues(thread int[][6][7] array)
        {
            for (uint i = array.length; i--;) {
                for (uint j = array[i].length; j--;) {
                    for (uint k = array[i][j].length; k--;)
                        array[i][j][k] = int(i + j + k);
                }
            }
        }
        test int testSetValuesAndSum()
        {
            int[5][6][7] array;
            setValues(@array);
            return sum(array);
        }
        test int testSetValuesMutateValuesAndSum()
        {
            int[5][6][7] array;
            setValues(@array);
            for (uint i = array.length; i--;) {
                for (uint j = array[i].length; j--;) {
                    for (uint k = array[i][j].length; k--;)
                        array[i][j][k] = array[i][j][k] * int(k);
                }
            }
            return sum(array);
        }
    `);
    checkInt(program, callFunction(program, "testSetValuesAndSum", []), 1575);
    checkInt(program, callFunction(program, "testSetValuesMutateValuesAndSum", []), 5565);
}

tests.lotsOfLocalVariables = function()
{
    let src = "test int sum() {\n";
    src += "    int i = 0;\n";
    let target = 0;
    const numVars = 50;
    for (let i = 0; i < numVars; i++) {
        const value = i * 3;
        src += `   i = ${i};\n`;
        src += `   int V${i} = (i + 3) * (i + 3);\n`;
        target += (i + 3) * (i + 3);
    }
    src += "    int result = 0;\n";
    for (let i = 0; i < numVars; i++) {
        src += `    result += V${i};\n`;
    }
    src += "    return result;\n";
    src += "}";
    let program = doPrep(src);
    checkInt(program, callFunction(program, "sum", []), target);
}

tests.operatorBool = function()
{
    let program = doPrep(`
        test bool boolFromUcharFalse() { return bool(uchar(0)); }
        test bool boolFromUcharTrue() { return bool(uchar(1)); }

        test bool boolFromUintFalse() { return bool(uint(0)); }
        test bool boolFromUintTrue() { return bool(uint(1)); }

        test bool boolFromIntFalse() { return bool(int(0)); }
        test bool boolFromIntTrue() { return bool(int(1)); }

        test bool boolFromFloatFalse() { return bool(float(0)); }
        test bool boolFromFloatTrue() { return bool(float(1)); }
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
        test bool foo(bool a, bool b)
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
        test bool foo(bool a, bool b)
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
        test bool foo(bool a, bool b)
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
        test bool foo(bool a)
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
        test int foo(int a, int b)
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
        test int foo(int a, int b)
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
        test int foo(int a, int b)
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
        test int foo(int a)
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
        test int foo(int a, uint b)
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
        test int foo(int a, uint b)
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
        test uint foo(uint a, uint b)
        {
            return a & b;
        }
    `);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 1), makeUint(program, 7)]), 1);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 65535), makeUint(program, 42)]), 42);
    checkUint(program, callFunction(program, "foo", [makeUint(program, Math.pow(2, 32) - 1), makeUint(program, Math.pow(2, 32) - 7)]), 4294967289);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 0), makeUint(program, 85732)]), 0);
}

tests.uintBitOr = function()
{
    let program = doPrep(`
        test uint foo(uint a, uint b)
        {
            return a | b;
        }
    `);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 1), makeUint(program, 7)]), 7);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 65535), makeUint(program, 42)]), 65535);
    checkUint(program, callFunction(program, "foo", [makeUint(program, Math.pow(2, 32) - 1), makeUint(program, Math.pow(2, 32)  - 7)]), 4294967295);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 0), makeUint(program, 85732)]), 85732);
}

tests.uintBitXor = function()
{
    let program = doPrep(`
        test uint foo(uint a, uint b)
        {
            return a ^ b;
        }
    `);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 1), makeUint(program, 7)]), 6);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 65535), makeUint(program, 42)]), 65493);
    checkUint(program, callFunction(program, "foo", [makeUint(program, Math.pow(2, 32) - 1), makeUint(program, Math.pow(2, 32) - 7)]), 6);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 0), makeUint(program, 85732)]), 85732);
}

tests.uintBitNot = function()
{
    let program = doPrep(`
        test uint foo(uint a)
        {
            return ~a;
        }
    `);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 1)]), 4294967294);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 65535)]), 4294901760);
    checkUint(program, callFunction(program, "foo", [makeUint(program, Math.pow(2, 32) - 1)]), 0);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 0)]), 4294967295);
}

tests.uintLShift = function()
{
    let program = doPrep(`
        test uint foo(uint a, uint b)
        {
            return a << b;
        }
    `);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 1), makeUint(program, 7)]), 128);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 65535), makeUint(program, 2)]), 262140);
    checkUint(program, callFunction(program, "foo", [makeUint(program, Math.pow(2, 32) - 1), makeUint(program, 5)]), 4294967264);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 0), makeUint(program, 3)]), 0);
}

tests.uintRShift = function()
{
    let program = doPrep(`
        test uint foo(uint a, uint b)
        {
            return a >> b;
        }
    `);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 1), makeUint(program, 7)]), 0);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 65535), makeUint(program, 2)]), 16383);
    checkUint(program, callFunction(program, "foo", [makeUint(program, Math.pow(2, 32) - 1), makeUint(program, 5)]), 134217727);
    checkUint(program, callFunction(program, "foo", [makeUint(program, 0), makeUint(program, 3)]), 0);
}

tests.ucharBitAnd = function()
{
    let program = doPrep(`
        test uchar foo(uchar a, uchar b)
        {
            return a & b;
        }
    `);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 1), makeUchar(program, 7)]), 1);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 255), makeUchar(program, 42)]), 42);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 0), makeUchar(program, 255)]), 0);
}

tests.ucharBitOr = function()
{
    let program = doPrep(`
        test uchar foo(uchar a, uchar b)
        {
            return a | b;
        }
    `);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 1), makeUchar(program, 7)]), 7);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 255), makeUchar(program, 42)]), 255);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 0), makeUchar(program, 228)]), 228);
}

tests.ucharBitXor = function()
{
    let program = doPrep(`
        test uchar foo(uchar a, uchar b)
        {
            return a ^ b;
        }
    `);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 1), makeUchar(program, 7)]), 6);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 255), makeUchar(program, 42)]), 213);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 0), makeUchar(program, 255)]), 255);
}

tests.ucharBitNot = function()
{
    let program = doPrep(`
        test uchar foo(uchar a)
        {
            return ~a;
        }
    `);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 1)]), 254);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 255)]), 0);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 0)]), 255);
}

tests.ucharLShift = function()
{
    let program = doPrep(`
        test uchar foo(uchar a, uint b)
        {
            return a << b;
        }
    `);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 1), makeUint(program, 7)]), 128);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 255), makeUint(program, 2)]), 252);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 0), makeUint(program, 3)]), 0);
}

tests.ucharRShift = function()
{
    let program = doPrep(`
        test uchar foo(uchar a, uint b)
        {
            return a >> b;
        }
    `);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 1), makeUint(program, 7)]), 0);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 255), makeUint(program, 2)]), 63);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 255), makeUint(program, 5)]), 7);
    checkUchar(program, callFunction(program, "foo", [makeUchar(program, 0), makeUint(program, 3)]), 0);
}

tests.floatMath = function()
{
    let program = doPrep(`
        test bool foo()
        {
            return 42.5 == 42.5;
        }
        test bool foo2()
        {
            return 42.5f == 42.5;
        }
        test bool foo3()
        {
            return 42.5 == 42.5f;
        }
        test bool foo4()
        {
            return 42.5f == 42.5f;
        }
        test bool foo5()
        {
            return 42.5f == 42.5f;
        }
        float bar(float x)
        {
            return x;
        }
        test float foo6()
        {
            return bar(7.5);
        }
        test float foo7()
        {
            return bar(7.5f);
        }
        test float foo9()
        {
            return float(7.5);
        }
        test float foo10()
        {
            return float(7.5f);
        }
        test float foo12()
        {
            return float(7);
        }
        test float foo13()
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
        test bool foo()
        {
            return true && true;
        }
        test bool foo2()
        {
            return true && false;
        }
        test bool foo3()
        {
            return false && true;
        }
        test bool foo4()
        {
            return false && false;
        }
        test bool foo5()
        {
            return true || true;
        }
        test bool foo6()
        {
            return true || false;
        }
        test bool foo7()
        {
            return false || true;
        }
        test bool foo8()
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

        test int andTrue()
        {
            int x;
            bool y = set(&x, 1, true) && set(&x, 2, false);
            return x; 
        }

        test int andFalse()
        {
            int x;
            bool y = set(&x, 1, false) && set(&x, 2, false);
            return x; 
        }

        test int orTrue()
        {
            int x;
            bool y = set(&x, 1, true) || set(&x, 2, false);
            return x; 
        }

        test int orFalse()
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
        test int foo()
        {
            ArrayTypedef arrayTypedef;
            return arrayTypedef[0];
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 0);
}

tests.shaderTypes = function()
{
    doPrep(`
        vertex float4 foo() : SV_Position {
            return float4(0, 1, 2, 3);
        }`);
    doPrep(`
        struct R {
            float4 x : SV_Position;
            int4 y : attribute(1);
        }
        vertex R foo() {
            R z;
            z.x = float4(1, 2, 3, 4);
            z.y = int4(5, 6, 7, 8);
            return z;
        }`);
    doPrep(`
        struct R {
            float4 x : SV_Position;
            int4 y : attribute(1);
        }
        struct S {
            R r;
            float3 z : attribute(2);
        }
        vertex S foo() {
            S w;
            w.r.x = float4(1, 2, 3, 4);
            w.r.y = int4(5, 6, 7, 8);
            w.z = float3(9, 10, 11);
            return w;
        }`);
    doPrep(`
        vertex float4 foo(constant float* buffer : register(b0)) : SV_Position {
            return float4(*buffer, *buffer, *buffer, *buffer);
        }`);
    doPrep(`
        vertex float4 foo(constant float* buffer : register(b0, space0)) : SV_Position {
            return float4(*buffer, *buffer, *buffer, *buffer);
        }`);
    doPrep(`
        vertex float4 foo(constant float* buffer : register(b0, space1)) : SV_Position {
            return float4(*buffer, *buffer, *buffer, *buffer);
        }`);
    doPrep(`
        vertex float4 foo(constant float[] buffer : register(b0)) : SV_Position {
            return float4(buffer[0], buffer[1], buffer[2], buffer[3]);
        }`);
    doPrep(`
        vertex float4 foo(float[5] buffer : register(b0)) : SV_Position {
            return float4(buffer[0], buffer[1], buffer[2], buffer[3]);
        }`);
    doPrep(`
        vertex float4 foo(device float* buffer : register(u0)) : SV_Position {
            return float4(*buffer, *buffer, *buffer, *buffer);
        }`);
    doPrep(`
        vertex float4 foo(device float[] buffer : register(u0)) : SV_Position {
            return float4(buffer[0], buffer[1], buffer[2], buffer[3]);
        }`);
    doPrep(`
        vertex float4 foo(uint x : SV_InstanceID) : SV_Position {
            return float4(float(x), float(x), float(x), float(x));
        }`);
    doPrep(`
        fragment float4 foo(bool x : SV_IsFrontFace) : SV_Target0 {
            return float4(1, 2, 3, 4);
        }`);
    doPrep(`
        fragment float4 foo(int x : specialized) : SV_Target0 {
            return float4(1, 2, 3, 4);
        }`);
    doPrep(`
        fragment float4 foo(Texture1D<float4> t : register(t0), sampler s : register(s0)) : SV_Target0 {
            return Sample(t, s, 0.4);
        }`);
    checkFail(
        () => doPrep(`
            vertex void foo() : SV_Position {
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            vertex float4 foo(float x : PSIZE) : SV_Position {
                return float4(x, x, x, x);
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            vertex float4 foo(int x : SV_InstanceID) : SV_Position {
                return float4(float(x), float(x), float(x), float(x));
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            vertex float4 foo(float x) : SV_Position {
                return float4(x, x, x, x);
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            fragment float4 foo(bool x : SV_IsFrontFace, bool y : SV_IsFrontFace) : SV_Target0 {
                return float4(1, 2, 3, 4);
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct R {
                float4 x : SV_Target0;
            }
            fragment R foo(bool x : SV_IsFrontFace) : SV_Depth {
                R y;
                y.x = float4(1, 2, 3, 4);
                return y;
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct R {
                bool x : SV_IsFrontFace;
            }
            fragment float4 foo(R x : SV_SampleIndex) : SV_Target0 {
                return float4(1, 2, 3, 4);
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct R {
                bool x : SV_IsFrontFace;
            }
            struct S {
                R r;
                bool y : SV_IsFrontFace;
            }
            fragment float4 foo(S x) : SV_Target0 {
                return float4(1, 2, 3, 4);
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct R {
                float x : SV_IsFrontFace;
            }
            fragment float4 foo(R x) : SV_Target0 {
                return float4(1, 2, 3, 4);
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct R {
                float x : SV_IsFrontFace;
            }
            vertex uint foo() : SV_VertexID {
                return 7;
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            typedef A = thread float*;
            vertex float4 foo(device A[] x : register(u0)) : SV_Position {
                return float4(1, 2, 3, 4);
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            typedef A = thread float*;
            vertex float4 foo(device A* x : register(u0)) : SV_Position {
                return float4(1, 2, 3, 4);
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            typedef A = thread float*;
            vertex float4 foo(A[4] x : register(u0)) : SV_Position {
                return float4(1, 2, 3, 4);
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            enum Foo {
                f, g
            }
            vertex float4 foo(Foo x : specialized) : SV_Position {
                return float4(1, 2, 3, 4);
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            [numthreads(1, 1, 1)]
            compute float foo() : attribute(0) {
                return 5;
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            fragment float foo() : attribute(0) {
                return 5;
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            vertex float foo(device float* x : attribute(0)) : attribute(0) {
                return 5;
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            vertex float4 foo(device float* x : register(b0)) : SV_Position {
                return float4(1, 2, 3, 4);
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            vertex float4 foo(float x : register(b0)) : SV_Position {
                return float4(1, 2, 3, 4);
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            vertex float4 foo(device float* x : register(t0)) : SV_Position {
                return float4(1, 2, 3, 4);
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            vertex float4 foo(Texture2D<float> x : register(b0)) : SV_Position {
                return float4(1, 2, 3, 4);
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            vertex float4 foo(constant float[] x : register(s0)) : SV_Position {
                return float4(1, 2, 3, 4);
            }
        `),
        e => e instanceof WTypeError);
}

tests.vectorTypeSyntax = function()
{
    let program = doPrep(`
        test int foo2()
        {
            int2 x;
            vector<int, 2> z = int2(3, 4);
            x = z;
            return x.y;
        }

        test int foo3()
        {
            int3 x;
            vector<int, 3> z = int3(3, 4, 5);
            x = z;
            return x.z;
        }

        test int foo4()
        {
            int4 x;
            vector<int,4> z = int4(3, 4, 5, 6);
            x = z;
            return x.w;
        }

        test bool vec2OperatorCast()
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
        test int foo2()
        {
            int2 x;
            vector<i, 2> z = int2(3, 4);
            x = z;
            return x.y;
        }

        test bool vec2OperatorCast()
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
        test int foo()
        {
            int2 a = int2(3, 4);
            return a[0];
        }
        test int foo2()
        {
            int2 a = int2(3, 4);
            int3 b = int3(a, 5);
            return b[1];
        }
        test int foo3()
        {
            int3 a = int3(3, 4, 5);
            int4 b = int4(6, a);
            return b[1];
        }
        test int foo4()
        {
            int2 a = int2(3, 4);
            int2 b = int2(5, 6);
            int4 c = int4(a, b);
            return c[2];
        }
        test bool foo5()
        {
            int4 a = int4(3, 4, 5, 6);
            int2 b = int2(4, 5);
            int4 c = int4(3, b, 6);
            return a == c;
        }
        test bool foo6()
        {
            int2 a = int2(4, 5);
            int3 b = int3(3, a);
            int3 c = int3(3, 4, 6);
            return b == c;
        }
        test uint foou()
        {
            uint2 a = uint2(3, 4);
            return a[0];
        }
        test uint foou2()
        {
            uint2 a = uint2(3, 4);
            uint3 b = uint3(a, 5);
            return b[1];
        }
        test uint foou3()
        {
            uint3 a = uint3(3, 4, 5);
            uint4 b = uint4(6, a);
            return b[1];
        }
        test uint foou4()
        {
            uint2 a = uint2(3, 4);
            uint2 b = uint2(5, 6);
            uint4 c = uint4(a, b);
            return c[2];
        }
        test bool foou5()
        {
            uint4 a = uint4(3, 4, 5, 6);
            uint2 b = uint2(4, 5);
            uint4 c = uint4(3, b, 6);
            return a == c;
        }
        test bool foou6()
        {
            uint2 a = uint2(4, 5);
            uint3 b = uint3(3, a);
            uint3 c = uint3(3, 4, 6);
            return b == c;
        }
        test float foof()
        {
            float2 a = float2(3., 4.);
            return a[0];
        }
        test float foof2()
        {
            float2 a = float2(3., 4.);
            float3 b = float3(a, 5.);
            return b[1];
        }
        test float foof3()
        {
            float3 a = float3(3., 4., 5.);
            float4 b = float4(6., a);
            return b[1];
        }
        test float foof4()
        {
            float2 a = float2(3., 4.);
            float2 b = float2(5., 6.);
            float4 c = float4(a, b);
            return c[2];
        }
        test bool foof5()
        {
            float4 a = float4(3., 4., 5., 6.);
            float2 b = float2(4., 5.);
            float4 c = float4(3., b, 6.);
            return a == c;
        }
        test bool foof6()
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
                src += `test ${typeName} ${functionName}()
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
                src += `test ${typeName} ${functionName}()
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
                src += `test ${typeName} ${functionName}()
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

        Foo _war()
        {
            return Foo.War;
        }

        test Foo war()
        {
            return _war();
        }

        Foo _famine()
        {
            return Foo.Famine;
        }

        test Foo famine()
        {
            return _famine();
        }

        Foo _pestilence()
        {
            return Foo.Pestilence;
        }

        test Foo pestilence()
        {
            return _pestilence();
        }

        Foo _death()
        {
            return Foo.Death;
        }

        test Foo death()
        {
            return _death();
        }

        bool _equals(Foo a, Foo b)
        {
            return a == b;
        }

        bool _notEquals(Foo a, Foo b)
        {
            return a != b;
        }

        test bool testSimpleEqual()
        {
            return _equals(Foo.War, Foo.War);
        }

        test bool testAnotherEqual()
        {
            return _equals(Foo.Pestilence, Foo.Pestilence);
        }

        test bool testNotEqual()
        {
            return _equals(Foo.Famine, Foo.Death);
        }

        test bool testSimpleNotEqual()
        {
            return _notEquals(Foo.War, Foo.War);
        }

        test bool testAnotherNotEqual()
        {
            return _notEquals(Foo.Pestilence, Foo.Pestilence);
        }

        test bool testNotNotEqual()
        {
            return _notEquals(Foo.Famine, Foo.Death);
        }

        int _intWar()
        {
            return int(_war());
        }

        test int intWar()
        {
            return _intWar();
        }

        int _intFamine()
        {
            return int(_famine());
        }

        test int intFamine()
        {
            return _intFamine();
        }

        int _intPestilence()
        {
            return int(_pestilence());
        }

        test int intPestilence()
        {
            return _intPestilence();
        }

        int _intDeath()
        {
            return int(_death());
        }

        test int intDeath()
        {
            return _intDeath();
        }

        test int warValue()
        {
            return _war().value;
        }

        test int famineValue()
        {
            return _famine().value;
        }

        test int pestilenceValue()
        {
            return _pestilence().value;
        }

        test int deathValue()
        {
            return _death().value;
        }

        test int warValueLiteral()
        {
            return Foo.War.value;
        }

        test int famineValueLiteral()
        {
            return Foo.Famine.value;
        }

        test int pestilenceValueLiteral()
        {
            return Foo.Pestilence.value;
        }

        test int deathValueLiteral()
        {
            return Foo.Death.value;
        }

        test Foo intWarBackwards()
        {
            return Foo(_intWar());
        }

        test Foo intFamineBackwards()
        {
            return Foo(_intFamine());
        }

        test Foo intPestilenceBackwards()
        {
            return Foo(_intPestilence());
        }

        test Foo intDeathBackwards()
        {
            return Foo(_intDeath());
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
        test Foo war()
        {
            return Foo.War;
        }
        test Foo famine()
        {
            return Foo.Famine;
        }
        test Foo pestilence()
        {
            return Foo.Pestilence;
        }
        test Foo death()
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
        test Foo war()
        {
            return Foo.War;
        }
        test Foo famine()
        {
            return Foo.Famine;
        }
        test Foo pestilence()
        {
            return Foo.Pestilence;
        }
        test Foo death()
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
        test int foo()
        {
            trap;
        }
        test int foo2(int x)
        {
            if (x == 3)
                trap;
            return 4;
        }
        struct Bar {
            int3 x;
            float y;
        }
        test Bar foo3()
        {
            trap;
        }
    `);
    checkTrap(program, () => callFunction(program, "foo", []), checkInt);
    checkInt(program, callFunction(program, "foo2", [makeInt(program, 1)]), 4);
    checkTrap(program, () => callFunction(program, "foo2", [makeInt(program, 3)]), checkInt);
    checkTrap(program, () => callFunction(program, "foo3", []), (progam, result, expected) => {
        for (let i = 0; i < 4; i++) {
            if (result.ePtr.get(i) != expected)
                throw new Error(`Non-zero return value at offset ${i}`);
        }
    });
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

        Foo _war()
        {
            return Foo.War;
        }

        test Foo war()
        {
            return _war();
        }

        Foo _famine()
        {
            return Foo.Famine;
        }

        test Foo famine()
        {
            return _famine();
        }

        Foo _pestilence()
        {
            return Foo.Pestilence;
        }

        test Foo pestilence()
        {
            return _pestilence();
        }

        Foo _death()
        {
            return Foo.Death;
        }

        test Foo death()
        {
            return _death();
        }

        bool _equals(Foo a, Foo b)
        {
            return a == b;
        }

        bool _notEquals(Foo a, Foo b)
        {
            return a != b;
        }

        test bool testSimpleEqual()
        {
            return _equals(Foo.War, Foo.War);
        }

        test bool testAnotherEqual()
        {
            return _equals(Foo.Pestilence, Foo.Pestilence);
        }

        test bool testNotEqual()
        {
            return _equals(Foo.Famine, Foo.Death);
        }

        test bool testSimpleNotEqual()
        {
            return _notEquals(Foo.War, Foo.War);
        }

        test bool testAnotherNotEqual()
        {
            return _notEquals(Foo.Pestilence, Foo.Pestilence);
        }

        test bool testNotNotEqual()
        {
            return _notEquals(Foo.Famine, Foo.Death);
        }

        int _intWar()
        {
            return int(_war());
        }

        test int intWar()
        {
            return _intWar();
        }

        int _intFamine()
        {
            return int(_famine());
        }

        test int intFamine()
        {
            return _intFamine();
        }

        int _intPestilence()
        {
            return int(_pestilence());
        }

        test int intPestilence()
        {
            return _intPestilence();
        }

        int _intDeath()
        {
            return int(_death());
        }

        test int intDeath()
        {
            return _intDeath();
        }

        test int warValue()
        {
            return _war().value;
        }

        test int famineValue()
        {
            return _famine().value;
        }

        test int pestilenceValue()
        {
            return _pestilence().value;
        }

        test int deathValue()
        {
            return _death().value;
        }

        test int warValueLiteral()
        {
            return Foo.War.value;
        }

        test int famineValueLiteral()
        {
            return Foo.Famine.value;
        }

        test int pestilenceValueLiteral()
        {
            return Foo.Pestilence.value;
        }

        test int deathValueLiteral()
        {
            return Foo.Death.value;
        }

        test Foo intWarBackwards()
        {
            return Foo(_intWar());
        }

        test Foo intFamineBackwards()
        {
            return Foo(_intFamine());
        }

        test Foo intPestilenceBackwards()
        {
            return Foo(_intPestilence());
        }

        test Foo intDeathBackwards()
        {
            return Foo(_intDeath());
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

        Foo _war()
        {
            return Foo.War;
        }

        test Foo war()
        {
            return _war();
        }

        Foo _famine()
        {
            return Foo.Famine;
        }

        test Foo famine()
        {
            return _famine();
        }

        Foo _pestilence()
        {
            return Foo.Pestilence;
        }

        test Foo pestilence()
        {
            return _pestilence();
        }

        Foo _death()
        {
            return Foo.Death;
        }

        test Foo death()
        {
            return _death();
        }

        bool _equals(Foo a, Foo b)
        {
            return a == b;
        }

        bool _notEquals(Foo a, Foo b)
        {
            return a != b;
        }

        test bool testSimpleEqual()
        {
            return _equals(Foo.War, Foo.War);
        }

        test bool testAnotherEqual()
        {
            return _equals(Foo.Pestilence, Foo.Pestilence);
        }

        test bool testNotEqual()
        {
            return _equals(Foo.Famine, Foo.Death);
        }

        test bool testSimpleNotEqual()
        {
            return _notEquals(Foo.War, Foo.War);
        }

        test bool testAnotherNotEqual()
        {
            return _notEquals(Foo.Pestilence, Foo.Pestilence);
        }

        test bool testNotNotEqual()
        {
            return _notEquals(Foo.Famine, Foo.Death);
        }

        uint _uintWar()
        {
            return uint(_war());
        }

        test uint uintWar()
        {
            return _uintWar();
        }

        uint _uintFamine()
        {
            return uint(_famine());
        }

        test uint uintFamine()
        {
            return _uintFamine();
        }

        uint _uintPestilence()
        {
            return uint(_pestilence());
        }

        test uint uintPestilence()
        {
            return _uintPestilence();
        }

        uint _uintDeath()
        {
            return uint(_death());
        }

        test uint uintDeath()
        {
            return _uintDeath();
        }

        test uint warValue()
        {
            return _war().value;
        }

        test uint famineValue()
        {
            return _famine().value;
        }

        test uint pestilenceValue()
        {
            return _pestilence().value;
        }

        test uint deathValue()
        {
            return _death().value;
        }

        test uint warValueLiteral()
        {
            return Foo.War.value;
        }

        test uint famineValueLiteral()
        {
            return Foo.Famine.value;
        }

        test uint pestilenceValueLiteral()
        {
            return Foo.Pestilence.value;
        }

        test uint deathValueLiteral()
        {
            return Foo.Death.value;
        }

        test Foo uintWarBackwards()
        {
            return Foo(_uintWar());
        }

        test Foo uintFamineBackwards()
        {
            return Foo(_uintFamine());
        }

        test Foo uintPestilenceBackwards()
        {
            return Foo(_uintPestilence());
        }

        test Foo uintDeathBackwards()
        {
            return Foo(_uintDeath());
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
        test int foo()
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
        test int foo(int x)
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
    let text = "test float foo(uchar x) { switch (uchar(x)) {"
    for (let i = 0; i <= 0xff; ++i)
        text += "case " + i + ": return " + i * 1.5 + ";";
    text += "} }";
    let program = doPrep(text);
    for (let i = 0; i < 0xff; ++i)
        checkFloat(program, callFunction(program, "foo", [makeUchar(program, i)]), i * 1.5);
}

tests.notQuiteExhaustiveUcharSwitch = function()
{
    let text = "test float foo(uchar x) { switch (uchar(x)) {"
    for (let i = 0; i <= 0xfe; ++i)
        text += "case " + i + ": return " + i * 1.5 + ";";
    text += "} }";
    checkFail(() => doPrep(text), e => e instanceof WTypeError);
}

tests.notQuiteExhaustiveUcharSwitchWithDefault = function()
{
    let text = "test float foo(uchar x) { switch (uchar(x)) {"
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
        test int foo(int x)
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
        test int foo(int x)
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
        test int foo(Foo x)
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
        test int foo(Foo x)
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
        test int foo()
        {
            Foo foo;
            Bar bar;
            foo.foo = 564;
            bar.bar = 53;
            return foo.bar->bar - bar.foo->foo;
        }
    `);
    checkTrap(program, () => callFunction(program, "foo", []), checkInt);
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
        test int foo()
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
        test int foo()
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
        test int foo()
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
        test int foo()
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
        test int foo()
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
        test int foo()
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
        test int foo()
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
        test int foo()
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
        test int foo()
        {
            Foo x;
            x.x = 13;
            return (@x)[float(-1)];
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 13);
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
        test int foo()
        {
            thread int* ptr = &getArray()[5];
            return *ptr;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 354);
}

tests.shaderStages = function()
{
    doPrep(`
        struct Result {
            float4 output : SV_Target0;
        }
        fragment Result foo()
        {
            float x = 7;
            float dx = ddx(x);
            float dy = ddy(x);
            Result r;
            r.output = float4(1, 2, 3, 4);
            return r;
        }
    `);
    doPrep(`
        [numthreads(1, 1, 1)]
        compute void foo()
        {
            AllMemoryBarrierWithGroupSync();
            DeviceMemoryBarrierWithGroupSync();
            GroupMemoryBarrierWithGroupSync();
        }
    `);
    checkFail(
        () => doPrep(`
            struct Result {
                float4 output : SV_Position;
            }
            vertex Result foo()
            {
                float x = 7;
                float dx = ddx(x);
                float dy = ddy(x);
                Result r;
                r.output = float4(1, 2, 3, 4);
                return r;
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct Result {
                float4 output : SV_Position;
            }
            vertex Result foo()
            {
                AllMemoryBarrierWithGroupSync();
                Result r;
                r.output = float4(1, 2, 3, 4);
                return r;
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct Result {
                float4 output : SV_Target0;
            }
            fragment Result foo()
            {
                AllMemoryBarrierWithGroupSync();
                Result r;
                r.output = float4(1, 2, 3, 4);
                return r;
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct Result {
                float4 output : SV_Position;
            }
            vertex Result foo()
            {
                DeviceMemoryBarrierWithGroupSync();
                Result r;
                r.output = float4(1, 2, 3, 4);
                return r;
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct Result {
                float4 output : SV_Target0;
            }
            fragment Result foo()
            {
                DeviceMemoryBarrierWithGroupSync();
                Result r;
                r.output = float4(1, 2, 3, 4);
                return r;
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct Result {
                float4 output : SV_Position;
            }
            vertex Result foo()
            {
                GroupMemoryBarrierWithGroupSync();
                Result r;
                r.output = float4(1, 2, 3, 4);
                return r;
            }
        `),
        e => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            struct Result {
                float4 output : SV_Target0;
            }
            fragment Result foo()
            {
                GroupMemoryBarrierWithGroupSync();
                Result r;
                r.output = float4(1, 2, 3, 4);
                return r;
            }
        `),
        e => e instanceof WTypeError);
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
        test int baz(int z) {
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
        test int baz(int z) {
            Foo foo;
            foo.x = z;
            thread Bar* b = thread Bar*(&foo);
            return (*b).y;
        }
    `);
    checkInt(program, callFunction(program, "baz", [makeInt(program, 6)]), 14);
}

tests.atomics = function()
{
    let program = doPrep(`
        test int foo1(int z) {
            atomic_int x;
            int result;
            InterlockedAdd(&x, z, &result);
            return result;
        }
        test int foo2(int z) {
            atomic_int x;
            int result;
            InterlockedAdd(&x, z, &result);
            InterlockedAdd(&x, z, &result);
            return result;
        }
        test int foo3(int z) {
            atomic_int x;
            int result;
            InterlockedAdd(&x, z, &result);
            return int(x);
        }
        test int foo4(int z) {
            atomic_int x;
            int result;
            InterlockedAdd(&x, z, &result);
            InterlockedAdd(&x, z, &result);
            return int(x);
        }
        test uint foo5(uint z) {
            atomic_uint x;
            uint result;
            InterlockedAdd(&x, z, &result);
            return result;
        }
        test uint foo6(uint z) {
            atomic_uint x;
            uint result;
            InterlockedAdd(&x, z, &result);
            InterlockedAdd(&x, z, &result);
            return result;
        }
        test uint foo7(uint z) {
            atomic_uint x;
            uint result;
            InterlockedAdd(&x, z, &result);
            return uint(x);
        }
        test uint foo8(uint z) {
            atomic_uint x;
            uint result;
            InterlockedAdd(&x, z, &result);
            InterlockedAdd(&x, z, &result);
            return uint(x);
        }
        test uint foo9(uint x, uint y) {
            atomic_uint z;
            uint result;
            InterlockedAdd(&z, x, &result);
            InterlockedAnd(&z, y, &result);
            return uint(z);
        }
        test uint foo10(uint x, uint y) {
            atomic_uint z;
            uint result;
            InterlockedAdd(&z, x, &result);
            InterlockedAnd(&z, y, &result);
            return result;
        }
        test int foo11(int x, int y) {
            atomic_int z;
            int result;
            InterlockedAdd(&z, x, &result);
            InterlockedAnd(&z, y, &result);
            return int(z);
        }
        test int foo12(int x, int y) {
            atomic_int z;
            int result;
            InterlockedAdd(&z, x, &result);
            InterlockedAnd(&z, y, &result);
            return result;
        }
        test uint foo13(uint x, uint y) {
            atomic_uint z;
            uint result;
            InterlockedAdd(&z, x, &result);
            InterlockedExchange(&z, y, &result);
            return uint(z);
        }
        test uint foo14(uint x, uint y) {
            atomic_uint z;
            uint result;
            InterlockedAdd(&z, x, &result);
            InterlockedExchange(&z, y, &result);
            return result;
        }
        test int foo15(int x, int y) {
            atomic_int z;
            int result;
            InterlockedAdd(&z, x, &result);
            InterlockedExchange(&z, y, &result);
            return int(z);
        }
        test int foo16(int x, int y) {
            atomic_int z;
            int result;
            InterlockedAdd(&z, x, &result);
            InterlockedExchange(&z, y, &result);
            return result;
        }
        test uint foo17(uint x, uint y) {
            atomic_uint z;
            uint result;
            InterlockedAdd(&z, x, &result);
            InterlockedMax(&z, y, &result);
            return uint(z);
        }
        test uint foo18(uint x, uint y) {
            atomic_uint z;
            uint result;
            InterlockedAdd(&z, x, &result);
            InterlockedMax(&z, y, &result);
            return result;
        }
        test int foo19(int x, int y) {
            atomic_int z;
            int result;
            InterlockedAdd(&z, x, &result);
            InterlockedMax(&z, y, &result);
            return int(z);
        }
        test int foo20(int x, int y) {
            atomic_int z;
            int result;
            InterlockedAdd(&z, x, &result);
            InterlockedMax(&z, y, &result);
            return result;
        }
        test uint foo21(uint x, uint y) {
            atomic_uint z;
            uint result;
            InterlockedAdd(&z, x, &result);
            InterlockedMin(&z, y, &result);
            return uint(z);
        }
        test uint foo22(uint x, uint y) {
            atomic_uint z;
            uint result;
            InterlockedAdd(&z, x, &result);
            InterlockedMin(&z, y, &result);
            return result;
        }
        test int foo23(int x, int y) {
            atomic_int z;
            int result;
            InterlockedAdd(&z, x, &result);
            InterlockedMin(&z, y, &result);
            return int(z);
        }
        test int foo24(int x, int y) {
            atomic_int z;
            int result;
            InterlockedAdd(&z, x, &result);
            InterlockedMin(&z, y, &result);
            return result;
        }
        test uint foo25(uint x, uint y) {
            atomic_uint z;
            uint result;
            InterlockedAdd(&z, x, &result);
            InterlockedOr(&z, y, &result);
            return uint(z);
        }
        test uint foo26(uint x, uint y) {
            atomic_uint z;
            uint result;
            InterlockedAdd(&z, x, &result);
            InterlockedOr(&z, y, &result);
            return result;
        }
        test int foo27(int x, int y) {
            atomic_int z;
            int result;
            InterlockedAdd(&z, x, &result);
            InterlockedOr(&z, y, &result);
            return int(z);
        }
        test int foo28(int x, int y) {
            atomic_int z;
            int result;
            InterlockedAdd(&z, x, &result);
            InterlockedOr(&z, y, &result);
            return result;
        }
        test uint foo29(uint x, uint y) {
            atomic_uint z;
            uint result;
            InterlockedAdd(&z, x, &result);
            InterlockedXor(&z, y, &result);
            return uint(z);
        }
        test uint foo30(uint x, uint y) {
            atomic_uint z;
            uint result;
            InterlockedAdd(&z, x, &result);
            InterlockedXor(&z, y, &result);
            return result;
        }
        test int foo31(int x, int y) {
            atomic_int z;
            int result;
            InterlockedAdd(&z, x, &result);
            InterlockedXor(&z, y, &result);
            return int(z);
        }
        test int foo32(int x, int y) {
            atomic_int z;
            int result;
            InterlockedAdd(&z, x, &result);
            InterlockedXor(&z, y, &result);
            return result;
        }
        test uint foo33(uint x, uint y, uint z) {
            atomic_uint w;
            uint result;
            InterlockedAdd(&w, x, &result);
            InterlockedCompareExchange(&w, y, z, &result);
            return uint(w);
        }
        test uint foo34(uint x, uint y, uint z) {
            atomic_uint w;
            uint result;
            InterlockedAdd(&w, x, &result);
            InterlockedCompareExchange(&w, y, z, &result);
            return result;
        }
        test int foo35(int x, int y, int z) {
            atomic_int w;
            int result;
            InterlockedAdd(&w, x, &result);
            InterlockedCompareExchange(&w, y, z, &result);
            return int(w);
        }
        test int foo36(int x, int y, int z) {
            atomic_int w;
            int result;
            InterlockedAdd(&w, x, &result);
            InterlockedCompareExchange(&w, y, z, &result);
            return result;
        }
    `);
    checkInt(program, callFunction(program, "foo1", [makeInt(program, 6)]), 0);
    checkInt(program, callFunction(program, "foo2", [makeInt(program, 6)]), 6);
    checkInt(program, callFunction(program, "foo3", [makeInt(program, 6)]), 6);
    checkInt(program, callFunction(program, "foo4", [makeInt(program, 6)]), 12);
    checkUint(program, callFunction(program, "foo5", [makeUint(program, 6)]), 0);
    checkUint(program, callFunction(program, "foo6", [makeUint(program, 6)]), 6);
    checkUint(program, callFunction(program, "foo7", [makeUint(program, 6)]), 6);
    checkUint(program, callFunction(program, "foo8", [makeUint(program, 6)]), 12);
    checkUint(program, callFunction(program, "foo9", [makeUint(program, 3), makeUint(program, 5)]), 1);
    checkUint(program, callFunction(program, "foo10", [makeUint(program, 3), makeUint(program, 5)]), 3);
    checkInt(program, callFunction(program, "foo11", [makeInt(program, 3), makeInt(program, 5)]), 1);
    checkInt(program, callFunction(program, "foo12", [makeInt(program, 3), makeInt(program, 5)]), 3);
    checkUint(program, callFunction(program, "foo13", [makeUint(program, 3), makeUint(program, 5)]), 5);
    checkUint(program, callFunction(program, "foo14", [makeUint(program, 3), makeUint(program, 5)]), 3);
    checkInt(program, callFunction(program, "foo15", [makeInt(program, 3), makeInt(program, 5)]), 5);
    checkInt(program, callFunction(program, "foo16", [makeInt(program, 3), makeInt(program, 5)]), 3);
    checkUint(program, callFunction(program, "foo17", [makeUint(program, 3), makeUint(program, 5)]), 5);
    checkUint(program, callFunction(program, "foo17", [makeUint(program, 5), makeUint(program, 3)]), 5);
    checkUint(program, callFunction(program, "foo18", [makeUint(program, 3), makeUint(program, 5)]), 3);
    checkUint(program, callFunction(program, "foo18", [makeUint(program, 5), makeUint(program, 3)]), 5);
    checkInt(program, callFunction(program, "foo19", [makeInt(program, 3), makeInt(program, 5)]), 5);
    checkInt(program, callFunction(program, "foo19", [makeInt(program, 5), makeInt(program, 3)]), 5);
    checkInt(program, callFunction(program, "foo20", [makeInt(program, 3), makeInt(program, 5)]), 3);
    checkInt(program, callFunction(program, "foo20", [makeInt(program, 5), makeInt(program, 3)]), 5);
    checkUint(program, callFunction(program, "foo21", [makeUint(program, 3), makeUint(program, 5)]), 3);
    checkUint(program, callFunction(program, "foo21", [makeUint(program, 5), makeUint(program, 3)]), 3);
    checkUint(program, callFunction(program, "foo22", [makeUint(program, 3), makeUint(program, 5)]), 3);
    checkUint(program, callFunction(program, "foo22", [makeUint(program, 5), makeUint(program, 3)]), 5);
    checkInt(program, callFunction(program, "foo23", [makeInt(program, 3), makeInt(program, 5)]), 3);
    checkInt(program, callFunction(program, "foo23", [makeInt(program, 5), makeInt(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo24", [makeInt(program, 3), makeInt(program, 5)]), 3);
    checkInt(program, callFunction(program, "foo24", [makeInt(program, 5), makeInt(program, 3)]), 5);
    checkUint(program, callFunction(program, "foo25", [makeUint(program, 3), makeUint(program, 5)]), 7);
    checkUint(program, callFunction(program, "foo26", [makeUint(program, 3), makeUint(program, 5)]), 3);
    checkInt(program, callFunction(program, "foo27", [makeInt(program, 3), makeInt(program, 5)]), 7);
    checkInt(program, callFunction(program, "foo28", [makeInt(program, 3), makeInt(program, 5)]), 3);
    checkUint(program, callFunction(program, "foo29", [makeUint(program, 3), makeUint(program, 5)]), 6);
    checkUint(program, callFunction(program, "foo30", [makeUint(program, 3), makeUint(program, 5)]), 3);
    checkInt(program, callFunction(program, "foo31", [makeInt(program, 3), makeInt(program, 5)]), 6);
    checkInt(program, callFunction(program, "foo32", [makeInt(program, 3), makeInt(program, 5)]), 3);
    checkUint(program, callFunction(program, "foo33", [makeUint(program, 3), makeUint(program, 3), makeUint(program, 5)]), 5);
    checkUint(program, callFunction(program, "foo33", [makeUint(program, 3), makeUint(program, 4), makeUint(program, 5)]), 3);
    checkUint(program, callFunction(program, "foo34", [makeUint(program, 3), makeUint(program, 3), makeUint(program, 5)]), 3);
    checkUint(program, callFunction(program, "foo34", [makeUint(program, 3), makeUint(program, 4), makeUint(program, 5)]), 3);
    checkInt(program, callFunction(program, "foo35", [makeInt(program, 3), makeInt(program, 3), makeInt(program, 5)]), 5);
    checkInt(program, callFunction(program, "foo35", [makeInt(program, 3), makeInt(program, 4), makeInt(program, 5)]), 3);
    checkInt(program, callFunction(program, "foo36", [makeInt(program, 3), makeInt(program, 3), makeInt(program, 5)]), 3);
    checkInt(program, callFunction(program, "foo36", [makeInt(program, 3), makeInt(program, 4), makeInt(program, 5)]), 3);
}

tests.atomicsNull = function()
{
    let program = doPrep(`
        test int foo1(int z) {
            atomic_int x;
            InterlockedAdd(&x, z, null);
            return int(x);
        }
        test int foo2(int z) {
            atomic_int x;
            InterlockedAdd(&x, z, null);
            InterlockedAdd(&x, z, null);
            return int(x);
        }
        test uint foo3(uint z) {
            atomic_uint x;
            InterlockedAdd(&x, z, null);
            return uint(x);
        }
        test uint foo4(uint z) {
            atomic_uint x;
            InterlockedAdd(&x, z, null);
            InterlockedAdd(&x, z, null);
            return uint(x);
        }
        test uint foo5(uint x, uint y) {
            atomic_uint z;
            InterlockedAdd(&z, x, null);
            InterlockedAnd(&z, y, null);
            return uint(z);
        }
        test int foo6(int x, int y) {
            atomic_int z;
            InterlockedAdd(&z, x, null);
            InterlockedAnd(&z, y, null);
            return int(z);
        }
        test uint foo7(uint x, uint y) {
            atomic_uint z;
            InterlockedAdd(&z, x, null);
            InterlockedExchange(&z, y, null);
            return uint(z);
        }
        test int foo8(int x, int y) {
            atomic_int z;
            InterlockedAdd(&z, x, null);
            InterlockedExchange(&z, y, null);
            return int(z);
        }
        test uint foo9(uint x, uint y) {
            atomic_uint z;
            InterlockedAdd(&z, x, null);
            InterlockedMax(&z, y, null);
            return uint(z);
        }
        test int foo10(int x, int y) {
            atomic_int z;
            InterlockedAdd(&z, x, null);
            InterlockedMax(&z, y, null);
            return int(z);
        }
        test uint foo11(uint x, uint y) {
            atomic_uint z;
            InterlockedAdd(&z, x, null);
            InterlockedMin(&z, y, null);
            return uint(z);
        }
        test int foo12(int x, int y) {
            atomic_int z;
            InterlockedAdd(&z, x, null);
            InterlockedMin(&z, y, null);
            return int(z);
        }
        test uint foo13(uint x, uint y) {
            atomic_uint z;
            InterlockedAdd(&z, x, null);
            InterlockedOr(&z, y, null);
            return uint(z);
        }
        test int foo14(int x, int y) {
            atomic_int z;
            InterlockedAdd(&z, x, null);
            InterlockedOr(&z, y, null);
            return int(z);
        }
        test uint foo15(uint x, uint y) {
            atomic_uint z;
            InterlockedAdd(&z, x, null);
            InterlockedXor(&z, y, null);
            return uint(z);
        }
        test int foo16(int x, int y) {
            atomic_int z;
            InterlockedAdd(&z, x, null);
            InterlockedXor(&z, y, null);
            return int(z);
        }
        test uint foo17(uint x, uint y, uint z) {
            atomic_uint w;
            InterlockedAdd(&w, x, null);
            InterlockedCompareExchange(&w, y, z, null);
            return uint(w);
        }
        test int foo18(int x, int y, int z) {
            atomic_int w;
            InterlockedAdd(&w, x, null);
            InterlockedCompareExchange(&w, y, z, null);
            return int(w);
        }
        test int foo19() {
            thread atomic_int* x = null;
            InterlockedAdd(x, 1, null);
            return 1;
        }
        test int foo20() {
            thread atomic_uint* x = null;
            InterlockedAdd(x, 1, null);
            return 1;
        }
        test int foo21() {
            thread atomic_int* x = null;
            InterlockedAnd(x, 1, null);
            return 1;
        }
        test int foo22() {
            thread atomic_uint* x = null;
            InterlockedAnd(x, 1, null);
            return 1;
        }
        test int foo23() {
            thread atomic_int* x = null;
            InterlockedExchange(x, 1, null);
            return 1;
        }
        test int foo24() {
            thread atomic_uint* x = null;
            InterlockedExchange(x, 1, null);
            return 1;
        }
        test int foo25() {
            thread atomic_int* x = null;
            InterlockedMax(x, 1, null);
            return 1;
        }
        test int foo26() {
            thread atomic_uint* x = null;
            InterlockedMax(x, 1, null);
            return 1;
        }
        test int foo27() {
            thread atomic_int* x = null;
            InterlockedMin(x, 1, null);
            return 1;
        }
        test int foo28() {
            thread atomic_uint* x = null;
            InterlockedMin(x, 1, null);
            return 1;
        }
        test int foo29() {
            thread atomic_int* x = null;
            InterlockedOr(x, 1, null);
            return 1;
        }
        test int foo30() {
            thread atomic_uint* x = null;
            InterlockedOr(x, 1, null);
            return 1;
        }
        test int foo31() {
            thread atomic_int* x = null;
            InterlockedXor(x, 1, null);
            return 1;
        }
        test int foo32() {
            thread atomic_uint* x = null;
            InterlockedXor(x, 1, null);
            return 1;
        }
        test int foo33() {
            thread atomic_int* x = null;
            InterlockedCompareExchange(x, 1, 2, null);
            return 1;
        }
        test int foo34() {
            thread atomic_uint* x = null;
            InterlockedCompareExchange(x, 1, 2, null);
            return 1;
        }
    `);
    checkInt(program, callFunction(program, "foo1", [makeInt(program, 6)]), 6);
    checkInt(program, callFunction(program, "foo2", [makeInt(program, 6)]), 12);
    checkUint(program, callFunction(program, "foo3", [makeUint(program, 6)]), 6);
    checkUint(program, callFunction(program, "foo4", [makeUint(program, 6)]), 12);
    checkUint(program, callFunction(program, "foo5", [makeUint(program, 3), makeUint(program, 5)]), 1);
    checkInt(program, callFunction(program, "foo6", [makeInt(program, 3), makeInt(program, 5)]), 1);
    checkUint(program, callFunction(program, "foo7", [makeUint(program, 3), makeUint(program, 5)]), 5);
    checkInt(program, callFunction(program, "foo8", [makeInt(program, 3), makeInt(program, 5)]), 5);
    checkUint(program, callFunction(program, "foo9", [makeUint(program, 3), makeUint(program, 5)]), 5);
    checkUint(program, callFunction(program, "foo9", [makeUint(program, 5), makeUint(program, 3)]), 5);
    checkInt(program, callFunction(program, "foo10", [makeInt(program, 3), makeInt(program, 5)]), 5);
    checkInt(program, callFunction(program, "foo10", [makeInt(program, 5), makeInt(program, 3)]), 5);
    checkUint(program, callFunction(program, "foo11", [makeUint(program, 3), makeUint(program, 5)]), 3);
    checkUint(program, callFunction(program, "foo11", [makeUint(program, 5), makeUint(program, 3)]), 3);
    checkInt(program, callFunction(program, "foo12", [makeInt(program, 3), makeInt(program, 5)]), 3);
    checkInt(program, callFunction(program, "foo12", [makeInt(program, 5), makeInt(program, 3)]), 3);
    checkUint(program, callFunction(program, "foo13", [makeUint(program, 3), makeUint(program, 5)]), 7);
    checkInt(program, callFunction(program, "foo14", [makeInt(program, 3), makeInt(program, 5)]), 7);
    checkUint(program, callFunction(program, "foo15", [makeUint(program, 3), makeUint(program, 5)]), 6);
    checkInt(program, callFunction(program, "foo16", [makeInt(program, 3), makeInt(program, 5)]), 6);
    checkUint(program, callFunction(program, "foo17", [makeUint(program, 3), makeUint(program, 3), makeUint(program, 5)]), 5);
    checkUint(program, callFunction(program, "foo17", [makeUint(program, 3), makeUint(program, 4), makeUint(program, 5)]), 3);
    checkInt(program, callFunction(program, "foo18", [makeInt(program, 3), makeInt(program, 3), makeInt(program, 5)]), 5);
    checkInt(program, callFunction(program, "foo18", [makeInt(program, 3), makeInt(program, 4), makeInt(program, 5)]), 3);
    checkTrap(program, () => callFunction(program, "foo19", []), checkInt);
    checkTrap(program, () => callFunction(program, "foo20", []), checkInt);
    checkTrap(program, () => callFunction(program, "foo21", []), checkInt);
    checkTrap(program, () => callFunction(program, "foo22", []), checkInt);
    checkTrap(program, () => callFunction(program, "foo23", []), checkInt);
    checkTrap(program, () => callFunction(program, "foo24", []), checkInt);
    checkTrap(program, () => callFunction(program, "foo25", []), checkInt);
    checkTrap(program, () => callFunction(program, "foo26", []), checkInt);
    checkTrap(program, () => callFunction(program, "foo27", []), checkInt);
    checkTrap(program, () => callFunction(program, "foo28", []), checkInt);
    checkTrap(program, () => callFunction(program, "foo29", []), checkInt);
    checkTrap(program, () => callFunction(program, "foo30", []), checkInt);
    checkTrap(program, () => callFunction(program, "foo31", []), checkInt);
    checkTrap(program, () => callFunction(program, "foo32", []), checkInt);
    checkTrap(program, () => callFunction(program, "foo33", []), checkInt);
    checkTrap(program, () => callFunction(program, "foo34", []), checkInt);
}

tests.selfCasts = function()
{
    let program = doPrep(`
        struct Foo {
            int x;
        }
        test int foo()
        {
            Foo foo;
            foo.x = 21;
            Foo bar = Foo(foo);
            bar.x = 42;
            return foo.x + bar.x;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 21 + 42);
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
        test float foo()
        {
            float2x2 a;
            a[0][0] = 1;
            a[0][1] = 2;
            a[1][0] = 3;
            a[1][1] = 4;
            return a[0][0];
        }
        test float foo2()
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
        test float foo3()
        {
            float2x2 a;
            return a[0][0];
        }
        test bool foo4()
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
        test bool foo5()
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
        test bool foo6()
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
        test float foo7()
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
    let program = doPrep("test half foo(half x, half y) { return x + y; }");
    checkHalf(program, callFunction(program, "foo", [makeHalf(program, 7), makeHalf(program, 5)]), 12);
    program = doPrep("test half foo(half x, half y) { return x - y; }");
    checkHalf(program, callFunction(program, "foo", [makeHalf(program, 7), makeHalf(program, 5)]), 2);
    checkHalf(program, callFunction(program, "foo", [makeHalf(program, 5), makeHalf(program, 7)]), -2);
    program = doPrep("test half foo(half x, half y) { return x * y; }");
    checkHalf(program, callFunction(program, "foo", [makeHalf(program, 7), makeHalf(program, 5)]), 35);
    checkHalf(program, callFunction(program, "foo", [makeHalf(program, 7), makeHalf(program, -5)]), -35);
    program = doPrep("test half foo(half x, half y) { return x / y; }");
    checkHalf(program, callFunction(program, "foo", [makeHalf(program, 7), makeHalf(program, 2)]), 3.5);
    checkHalf(program, callFunction(program, "foo", [makeHalf(program, 7), makeHalf(program, -2)]), -3.5);
}

tests.matrixMultiplication = function() {
    let program = doPrep(`
        float2x4 multiply(float2x3 x, float3x4 y) {
            // Copied and pasted from the standard library
            float2x4 result;
            result[0][0] = 0;
            result[0][0] += x[0][0] * y[0][0];
            result[0][0] += x[0][1] * y[1][0];
            result[0][0] += x[0][2] * y[2][0];
            result[0][1] = 0;
            result[0][1] += x[0][0] * y[0][1];
            result[0][1] += x[0][1] * y[1][1];
            result[0][1] += x[0][2] * y[2][1];
            result[0][2] = 0;
            result[0][2] += x[0][0] * y[0][2];
            result[0][2] += x[0][1] * y[1][2];
            result[0][2] += x[0][2] * y[2][2];
            result[0][3] = 0;
            result[0][3] += x[0][0] * y[0][3];
            result[0][3] += x[0][1] * y[1][3];
            result[0][3] += x[0][2] * y[2][3];
            result[1][0] = 0;
            result[1][0] += x[1][0] * y[0][0];
            result[1][0] += x[1][1] * y[1][0];
            result[1][0] += x[1][2] * y[2][0];
            result[1][1] = 0;
            result[1][1] += x[1][0] * y[0][1];
            result[1][1] += x[1][1] * y[1][1];
            result[1][1] += x[1][2] * y[2][1];
            result[1][2] = 0;
            result[1][2] += x[1][0] * y[0][2];
            result[1][2] += x[1][1] * y[1][2];
            result[1][2] += x[1][2] * y[2][2];
            result[1][3] = 0;
            result[1][3] += x[1][0] * y[0][3];
            result[1][3] += x[1][1] * y[1][3];
            result[1][3] += x[1][2] * y[2][3];
            return result;
        }
        float2x3 matrix1() {
            float2x3 x;
            x[0][0] = 2;
            x[0][1] = 3;
            x[0][2] = 5;
            x[1][0] = 7;
            x[1][1] = 11;
            x[1][2] = 13;
            return x;
        }
        float3x4 matrix2() {
            float3x4 y;
            y[0][0] = 17;
            y[0][1] = 19;
            y[0][2] = 23;
            y[0][3] = 29;
            y[1][0] = 31;
            y[1][1] = 37;
            y[1][2] = 41;
            y[1][3] = 43;
            y[2][0] = 47;
            y[2][1] = 53;
            y[2][2] = 59;
            y[2][3] = 61;
            return y;
        }
        test float foo00() {
            return multiply(matrix1(), matrix2())[0][0];
        }
        test float foo01() {
            return multiply(matrix1(), matrix2())[0][1];
        }
        test float foo02() {
            return multiply(matrix1(), matrix2())[0][2];
        }
        test float foo03() {
            return multiply(matrix1(), matrix2())[0][3];
        }
        test float foo10() {
            return multiply(matrix1(), matrix2())[1][0];
        }
        test float foo11() {
            return multiply(matrix1(), matrix2())[1][1];
        }
        test float foo12() {
            return multiply(matrix1(), matrix2())[1][2];
        }
        test float foo13() {
            return multiply(matrix1(), matrix2())[1][3];
        }
    `);
    checkFloat(program, callFunction(program, "foo00", []), 17 * 2 + 31 * 3 + 47 * 5);
    checkFloat(program, callFunction(program, "foo01", []), 19 * 2 + 37 * 3 + 53 * 5);
    checkFloat(program, callFunction(program, "foo02", []), 23 * 2 + 41 * 3 + 59 * 5);
    checkFloat(program, callFunction(program, "foo03", []), 29 * 2 + 43 * 3 + 61 * 5);
    checkFloat(program, callFunction(program, "foo10", []), 17 * 7 + 31 * 11 + 47 * 13);
    checkFloat(program, callFunction(program, "foo11", []), 19 * 7 + 37 * 11 + 53 * 13);
    checkFloat(program, callFunction(program, "foo12", []), 23 * 7 + 41 * 11 + 59 * 13);
    checkFloat(program, callFunction(program, "foo13", []), 29 * 7 + 43 * 11 + 61 * 13);
}

tests.arrayIndex = function() {
    let program = doPrep(`
        test uint innerArrayLength() {
            int[2][3] array;
            return array[0].length;
        }

        test uint outerArrayLength() {
            int[2][3] array;
            return array.length;
        }

        test int arrayIndexing(uint i, uint j) {
            int[2][3] array;
            array[0][0] = 1;
            array[0][1] = 2;
            array[0][2] = 3;
            array[1][0] = 4;
            array[1][1] = 5;
            array[1][2] = 6;
            return array[i][j];
        }
    `);

    checkUint(program, callFunction(program, "innerArrayLength", []), 3);
    checkUint(program, callFunction(program, "outerArrayLength", []), 2);
    checkInt(program, callFunction(program, "arrayIndexing", [ makeUint(program, 0), makeUint(program, 0) ]), 1);
    checkInt(program, callFunction(program, "arrayIndexing", [ makeUint(program, 0), makeUint(program, 1) ]), 2);
    checkInt(program, callFunction(program, "arrayIndexing", [ makeUint(program, 0), makeUint(program, 2) ]), 3);
    checkInt(program, callFunction(program, "arrayIndexing", [ makeUint(program, 1), makeUint(program, 0) ]), 4);
    checkInt(program, callFunction(program, "arrayIndexing", [ makeUint(program, 1), makeUint(program, 1) ]), 5);
    checkInt(program, callFunction(program, "arrayIndexing", [ makeUint(program, 1), makeUint(program, 2) ]), 6);
}

tests.numThreads = function() {
    let program = doPrep(`
        [numthreads(3, 4, 5)]
        compute void foo() {
        }

        struct R {
            float4 position : SV_Position;
        }
        vertex R bar() {
            R r;
            r.position = float4(1, 2, 3, 4);
            return r;
        }
    `);

    if (program.functions.get("foo").length != 1)
        throw new Error("Cannot find function named 'foo'");
    let foo = program.functions.get("foo")[0];
    if (foo.attributeBlock.length != 1)
        throw new Error("'foo' doesn't have numthreads attribute");
    if (foo.attributeBlock[0].x != 3)
        throw new Error("'foo' numthreads x is not 3");
    if (foo.attributeBlock[0].y != 4)
        throw new Error("'foo' numthreads y is not 4");
    if (foo.attributeBlock[0].z != 5)
        throw new Error("'foo' numthreads z is not 5");

    if (program.functions.get("bar").length != 1)
        throw new Error("Cannot find function named 'bar'");
    let bar = program.functions.get("bar")[0];
    if (bar.attributeBlock != null)
        throw new Error("'baz' has attribute block");

    checkFail(() => doPrep(`
        [numthreads(3, 4)]
        compute void foo() {
        }
    `), e => e instanceof WSyntaxError);

    checkFail(() => doPrep(`
        []
        compute void foo() {
        }
    `), e => e instanceof WSyntaxError);

    checkFail(() => doPrep(`
        [numthreads(3, 4, 5), numthreads(3, 4, 5)]
        compute void foo() {
        }
    `), e => e instanceof WSyntaxError);

    checkFail(() => doPrep(`
        compute void foo() {
        }
    `), e => e instanceof WSyntaxError);


    checkFail(() => doPrep(`
        struct R {
            float4 position;
        }
        [numthreads(3, 4, 5)]
        vertex R baz() {
            R r;
            r.position = float4(1, 2, 3, 4);
            return r;
        }
    `), e => e instanceof WSyntaxError);
}

function createTexturesForTesting(program)
{
    let texture1D = make1DTexture(program, [[1, 7, 14, 79], [13, 16], [15]], "float");
    let texture1DArray = make1DTextureArray(program, [[[1, 7, 14, 79], [13, 16], [15]], [[16, 17, 18, 19], [20, 21], [22]]], "float");
    let texture2D = make2DTexture(program, [
        [[1, 2, 3, 4, 5, 6, 7, 8],
        [9, 10, 11, 12, 13, 14, 15, 16],
        [17, 18, 19, 20, 21, 22, 23, 24],
        [25, 26, 27, 28, 29, 30, 31, 32]],
        [[33, 34, 35, 36],
        [37, 38, 39, 40]],
        [[41, 42]]
    ], "float");
    let texture2DArray = make2DTextureArray(program, [[
        [[1, 2, 3, 4, 5, 6, 7, 8],
        [9, 10, 11, 12, 13, 14, 15, 16],
        [17, 18, 19, 20, 21, 22, 23, 24],
        [25, 26, 27, 28, 29, 30, 31, 32]],
        [[33, 34, 35, 36],
        [37, 38, 39, 40]],
        [[41, 42]]
    ], [
        [[43, 44, 45, 46, 47, 48, 49, 50],
        [51, 52, 53, 54, 55, 56, 57, 58],
        [59, 60, 61, 62, 63, 64, 65, 66],
        [67, 68, 69, 70, 71, 72, 73, 74]],
        [[75, 76, 77, 78],
        [79, 80, 81, 82]],
        [[83, 84]]
    ]], "float");
    let texture3D = make3DTexture(program, [
        [[[1, 2, 3, 4, 5, 6, 7, 8],
        [9, 10, 11, 12, 13, 14, 15, 16],
        [17, 18, 19, 20, 21, 22, 23, 24],
        [25, 26, 27, 28, 29, 30, 31, 32]],
        [[33, 34, 35, 36, 37, 38, 39, 40],
        [41, 42, 43, 44, 45, 46, 47, 48],
        [49, 50, 51, 52, 53, 54, 55, 56],
        [57, 58, 59, 60, 61, 62, 63, 64]]],
        [[[65, 66, 67, 68],
        [69, 70, 71, 72]]]
    ], "float");
    let textureCube = makeTextureCube(program, [[
        [[1, 2],
        [3, 4],
        [5, 6],
        [7, 8]],
        [[9],
        [10]],
    ], [
        [[11, 12],
        [13, 14],
        [15, 16],
        [17, 18]],
        [[19],
        [20]],
    ], [
        [[21, 22],
        [23, 24],
        [25, 26],
        [27, 28]],
        [[29],
        [30]],
    ], [
        [[31, 32],
        [33, 34],
        [35, 36],
        [37, 38]],
        [[39],
        [40]],
    ], [
        [[41, 42],
        [43, 44],
        [45, 46],
        [47, 48]],
        [[49],
        [50]],
    ], [
        [[51, 52],
        [53, 54],
        [55, 56],
        [57, 58]],
        [[59],
        [60]],
    ]], "float");
    let rwTexture1D = makeRW1DTexture(program, [1, 2, 3, 4, 5, 6, 7, 8], "float");
    let rwTexture1DArray = makeRW1DTextureArray(program, [[1, 2, 3, 4, 5, 6, 7, 8], [9, 10, 11, 12, 13, 14, 15, 16], [17, 18, 19, 20, 21, 22, 23, 24]], "float");
    let rwTexture2D = makeRW2DTexture(program, [
        [1, 2, 3, 4, 5, 6, 7, 8],
        [9, 10, 11, 12, 13, 14, 15, 16],
        [17, 18, 19, 20, 21, 22, 23, 24],
        [25, 26, 27, 28, 29, 30, 31, 32]], "float");
    let rwTexture2DArray = makeRW2DTextureArray(program, [
        [[1, 2, 3, 4, 5, 6, 7, 8],
        [9, 10, 11, 12, 13, 14, 15, 16],
        [17, 18, 19, 20, 21, 22, 23, 24],
        [25, 26, 27, 28, 29, 30, 31, 32]],
        [[33, 34, 35, 36, 37, 38, 39, 40],
        [41, 42, 43, 44, 45, 46, 47, 48],
        [49, 50, 51, 52, 53, 54, 55, 56],
        [57, 58, 59, 60, 61, 62, 63, 64]]], "float");
    let rwTexture3D = makeRW3DTexture(program, [
        [[1, 2, 3, 4, 5, 6, 7, 8],
        [9, 10, 11, 12, 13, 14, 15, 16],
        [17, 18, 19, 20, 21, 22, 23, 24],
        [25, 26, 27, 28, 29, 30, 31, 32]],
        [[33, 34, 35, 36, 37, 38, 39, 40],
        [41, 42, 43, 44, 45, 46, 47, 48],
        [49, 50, 51, 52, 53, 54, 55, 56],
        [57, 58, 59, 60, 61, 62, 63, 64]]], "float");
    let textureDepth2D = make2DDepthTexture(program, [
        [[1, 2, 3, 4, 5, 6, 7, 8],
        [9, 10, 11, 12, 13, 14, 15, 16],
        [17, 18, 19, 20, 21, 22, 23, 24],
        [25, 26, 27, 28, 29, 30, 31, 32]],
        [[33, 34, 35, 36],
        [37, 38, 39, 40]],
        [[41, 42]]
    ], "float");
    let textureDepth2DArray = make2DDepthTextureArray(program, [[
        [[1, 2, 3, 4, 5, 6, 7, 8],
        [9, 10, 11, 12, 13, 14, 15, 16],
        [17, 18, 19, 20, 21, 22, 23, 24],
        [25, 26, 27, 28, 29, 30, 31, 32]],
        [[33, 34, 35, 36],
        [37, 38, 39, 40]],
        [[41, 42]]
    ], [
        [[43, 44, 45, 46, 47, 48, 49, 50],
        [51, 52, 53, 54, 55, 56, 57, 58],
        [59, 60, 61, 62, 63, 64, 65, 66],
        [67, 68, 69, 70, 71, 72, 73, 74]],
        [[75, 76, 77, 78],
        [79, 80, 81, 82]],
        [[83, 84]]
    ]], "float");
    let textureDepthCube = makeDepthTextureCube(program, [[
        [[1, 2],
        [3, 4],
        [5, 6],
        [7, 8]],
        [[9],
        [10]],
    ], [
        [[11, 12],
        [13, 14],
        [15, 16],
        [17, 18]],
        [[19],
        [20]],
    ], [
        [[21, 22],
        [23, 24],
        [25, 26],
        [27, 28]],
        [[29],
        [30]],
    ], [
        [[31, 32],
        [33, 34],
        [35, 36],
        [37, 38]],
        [[39],
        [40]],
    ], [
        [[41, 42],
        [43, 44],
        [45, 46],
        [47, 48]],
        [[49],
        [50]],
    ], [
        [[51, 52],
        [53, 54],
        [55, 56],
        [57, 58]],
        [[59],
        [60]],
    ]], "float");
    let rwTextureDepth2D = makeRW2DDepthTexture(program, [
        [1, 2, 3, 4, 5, 6, 7, 8],
        [9, 10, 11, 12, 13, 14, 15, 16],
        [17, 18, 19, 20, 21, 22, 23, 24],
        [25, 26, 27, 28, 29, 30, 31, 32]], "float");
    let rwTextureDepth2DArray = makeRW2DDepthTextureArray(program, [
        [[1, 2, 3, 4, 5, 6, 7, 8],
        [9, 10, 11, 12, 13, 14, 15, 16],
        [17, 18, 19, 20, 21, 22, 23, 24],
        [25, 26, 27, 28, 29, 30, 31, 32]],
        [[33, 34, 35, 36, 37, 38, 39, 40],
        [41, 42, 43, 44, 45, 46, 47, 48],
        [49, 50, 51, 52, 53, 54, 55, 56],
        [57, 58, 59, 60, 61, 62, 63, 64]]], "float");
    return [texture1D, texture1DArray, texture2D, texture2DArray, texture3D, textureCube, rwTexture1D, rwTexture1DArray, rwTexture2D, rwTexture2DArray, rwTexture3D, textureDepth2D, textureDepth2DArray, textureDepthCube, rwTextureDepth2D, rwTextureDepth2DArray];
}

tests.textureDimensions = function() {
    let program = doPrep(`
        test uint foo1(Texture1D<float> texture) {
            uint width;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &numberOfLevels);
            return width;
        }
        test uint foo2(Texture1D<float> texture) {
            uint width;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &numberOfLevels);
            return numberOfLevels;
        }
        test uint foo3(Texture1DArray<float> texture) {
            uint width;
            uint elements;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &elements, &numberOfLevels);
            return width;
        }
        test uint foo4(Texture1DArray<float> texture) {
            uint width;
            uint elements;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &elements, &numberOfLevels);
            return elements;
        }
        test uint foo5(Texture1DArray<float> texture) {
            uint width;
            uint elements;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &elements, &numberOfLevels);
            return numberOfLevels;
        }
        test uint foo6(Texture2D<float> texture) {
            uint width;
            uint height;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &numberOfLevels);
            return width;
        }
        test uint foo7(Texture2D<float> texture) {
            uint width;
            uint height;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &numberOfLevels);
            return height;
        }
        test uint foo8(Texture2D<float> texture) {
            uint width;
            uint height;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &numberOfLevels);
            return numberOfLevels;
        }
        test uint foo9(Texture2DArray<float> texture) {
            uint width;
            uint height;
            uint elements;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &elements, &numberOfLevels);
            return width;
        }
        test uint foo10(Texture2DArray<float> texture) {
            uint width;
            uint height;
            uint elements;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &elements, &numberOfLevels);
            return height;
        }
        test uint foo11(Texture2DArray<float> texture) {
            uint width;
            uint height;
            uint elements;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &elements, &numberOfLevels);
            return elements;
        }
        test uint foo12(Texture2DArray<float> texture) {
            uint width;
            uint height;
            uint elements;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &elements, &numberOfLevels);
            return numberOfLevels;
        }
        test uint foo13(Texture3D<float> texture) {
            uint width;
            uint height;
            uint depth;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &depth, &numberOfLevels);
            return width;
        }
        test uint foo14(Texture3D<float> texture) {
            uint width;
            uint height;
            uint depth;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &depth, &numberOfLevels);
            return height;
        }
        test uint foo15(Texture3D<float> texture) {
            uint width;
            uint height;
            uint depth;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &depth, &numberOfLevels);
            return depth;
        }
        test uint foo16(Texture3D<float> texture) {
            uint width;
            uint height;
            uint depth;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &depth, &numberOfLevels);
            return numberOfLevels;
        }
        test uint foo17(TextureCube<float> texture) {
            uint width;
            uint height;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &numberOfLevels);
            return width;
        }
        test uint foo18(TextureCube<float> texture) {
            uint width;
            uint height;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &numberOfLevels);
            return height;
        }
        test uint foo19(TextureCube<float> texture) {
            uint width;
            uint height;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &numberOfLevels);
            return numberOfLevels;
        }
        test uint foo20(RWTexture1D<float> texture) {
            uint width;
            GetDimensions(texture, &width);
            return width;
        }
        test uint foo21(RWTexture1DArray<float> texture) {
            uint width;
            uint elements;
            GetDimensions(texture, &width, &elements);
            return width;
        }
        test uint foo22(RWTexture1DArray<float> texture) {
            uint width;
            uint elements;
            GetDimensions(texture, &width, &elements);
            return elements;
        }
        test uint foo23(RWTexture2D<float> texture) {
            uint width;
            uint height;
            GetDimensions(texture, &width, &height);
            return width;
        }
        test uint foo24(RWTexture2D<float> texture) {
            uint width;
            uint height;
            GetDimensions(texture, &width, &height);
            return height;
        }
        test uint foo25(RWTexture2DArray<float> texture) {
            uint width;
            uint height;
            uint elements;
            GetDimensions(texture, &width, &height, &elements);
            return width;
        }
        test uint foo26(RWTexture2DArray<float> texture) {
            uint width;
            uint height;
            uint elements;
            GetDimensions(texture, &width, &height, &elements);
            return height;
        }
        test uint foo27(RWTexture2DArray<float> texture) {
            uint width;
            uint height;
            uint elements;
            GetDimensions(texture, &width, &height, &elements);
            return elements;
        }
        test uint foo28(RWTexture3D<float> texture) {
            uint width;
            uint height;
            uint depth;
            GetDimensions(texture, &width, &height, &depth);
            return width;
        }
        test uint foo29(RWTexture3D<float> texture) {
            uint width;
            uint height;
            uint depth;
            GetDimensions(texture, &width, &height, &depth);
            return height;
        }
        test uint foo30(RWTexture3D<float> texture) {
            uint width;
            uint height;
            uint depth;
            GetDimensions(texture, &width, &height, &depth);
            return depth;
        }
        test uint foo31(TextureDepth2D<float> texture) {
            uint width;
            uint height;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &numberOfLevels);
            return width;
        }
        test uint foo32(TextureDepth2D<float> texture) {
            uint width;
            uint height;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &numberOfLevels);
            return height;
        }
        test uint foo33(TextureDepth2D<float> texture) {
            uint width;
            uint height;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &numberOfLevels);
            return numberOfLevels;
        }
        test uint foo34(TextureDepth2DArray<float> texture) {
            uint width;
            uint height;
            uint elements;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &elements, &numberOfLevels);
            return width;
        }
        test uint foo35(TextureDepth2DArray<float> texture) {
            uint width;
            uint height;
            uint elements;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &elements, &numberOfLevels);
            return height;
        }
        test uint foo36(TextureDepth2DArray<float> texture) {
            uint width;
            uint height;
            uint elements;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &elements, &numberOfLevels);
            return elements;
        }
        test uint foo37(TextureDepth2DArray<float> texture) {
            uint width;
            uint height;
            uint elements;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &elements, &numberOfLevels);
            return numberOfLevels;
        }
        test uint foo38(TextureDepthCube<float> texture) {
            uint width;
            uint height;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &numberOfLevels);
            return width;
        }
        test uint foo39(TextureDepthCube<float> texture) {
            uint width;
            uint height;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &numberOfLevels);
            return height;
        }
        test uint foo40(TextureDepthCube<float> texture) {
            uint width;
            uint height;
            uint numberOfLevels;
            GetDimensions(texture, 0, &width, &height, &numberOfLevels);
            return numberOfLevels;
        }
        test uint foo41(RWTextureDepth2D<float> texture) {
            uint width;
            uint height;
            GetDimensions(texture, &width, &height);
            return width;
        }
        test uint foo42(RWTextureDepth2D<float> texture) {
            uint width;
            uint height;
            GetDimensions(texture, &width, &height);
            return height;
        }
        test uint foo43(RWTextureDepth2DArray<float> texture) {
            uint width;
            uint height;
            uint elements;
            GetDimensions(texture, &width, &height, &elements);
            return width;
        }
        test uint foo44(RWTextureDepth2DArray<float> texture) {
            uint width;
            uint height;
            uint elements;
            GetDimensions(texture, &width, &height, &elements);
            return height;
        }
        test uint foo45(RWTextureDepth2DArray<float> texture) {
            uint width;
            uint height;
            uint elements;
            GetDimensions(texture, &width, &height, &elements);
            return elements;
        }
    `);
    let [texture1D, texture1DArray, texture2D, texture2DArray, texture3D, textureCube, rwTexture1D, rwTexture1DArray, rwTexture2D, rwTexture2DArray, rwTexture3D, textureDepth2D, textureDepth2DArray, textureDepthCube, rwTextureDepth2D, rwTextureDepth2DArray] = createTexturesForTesting(program);
    checkUint(program, callFunction(program, "foo1", [texture1D]), 4);
    checkUint(program, callFunction(program, "foo2", [texture1D]), 3);
    checkUint(program, callFunction(program, "foo3", [texture1DArray]), 4);
    checkUint(program, callFunction(program, "foo4", [texture1DArray]), 2);
    checkUint(program, callFunction(program, "foo5", [texture1DArray]), 3);
    checkUint(program, callFunction(program, "foo6", [texture2D]), 8);
    checkUint(program, callFunction(program, "foo7", [texture2D]), 4);
    checkUint(program, callFunction(program, "foo8", [texture2D]), 3);
    checkUint(program, callFunction(program, "foo9", [texture2DArray]), 8);
    checkUint(program, callFunction(program, "foo10", [texture2DArray]), 4);
    checkUint(program, callFunction(program, "foo11", [texture2DArray]), 2);
    checkUint(program, callFunction(program, "foo12", [texture2DArray]), 3);
    checkUint(program, callFunction(program, "foo13", [texture3D]), 8);
    checkUint(program, callFunction(program, "foo14", [texture3D]), 4);
    checkUint(program, callFunction(program, "foo15", [texture3D]), 2);
    checkUint(program, callFunction(program, "foo16", [texture3D]), 2);
    checkUint(program, callFunction(program, "foo17", [textureCube]), 2);
    checkUint(program, callFunction(program, "foo18", [textureCube]), 4);
    checkUint(program, callFunction(program, "foo19", [textureCube]), 2);
    checkUint(program, callFunction(program, "foo20", [rwTexture1D]), 8);
    checkUint(program, callFunction(program, "foo21", [rwTexture1DArray]), 8);
    checkUint(program, callFunction(program, "foo22", [rwTexture1DArray]), 3);
    checkUint(program, callFunction(program, "foo23", [rwTexture2D]), 8);
    checkUint(program, callFunction(program, "foo24", [rwTexture2D]), 4);
    checkUint(program, callFunction(program, "foo25", [rwTexture2DArray]), 8);
    checkUint(program, callFunction(program, "foo26", [rwTexture2DArray]), 4);
    checkUint(program, callFunction(program, "foo27", [rwTexture2DArray]), 2);
    checkUint(program, callFunction(program, "foo28", [rwTexture3D]), 8);
    checkUint(program, callFunction(program, "foo29", [rwTexture3D]), 4);
    checkUint(program, callFunction(program, "foo30", [rwTexture3D]), 2);
    checkUint(program, callFunction(program, "foo31", [textureDepth2D]), 8);
    checkUint(program, callFunction(program, "foo32", [textureDepth2D]), 4);
    checkUint(program, callFunction(program, "foo33", [textureDepth2D]), 3);
    checkUint(program, callFunction(program, "foo34", [textureDepth2DArray]), 8);
    checkUint(program, callFunction(program, "foo35", [textureDepth2DArray]), 4);
    checkUint(program, callFunction(program, "foo36", [textureDepth2DArray]), 2);
    checkUint(program, callFunction(program, "foo37", [textureDepth2DArray]), 3);
    checkUint(program, callFunction(program, "foo38", [textureDepthCube]), 2);
    checkUint(program, callFunction(program, "foo39", [textureDepthCube]), 4);
    checkUint(program, callFunction(program, "foo40", [textureDepthCube]), 2);
    checkUint(program, callFunction(program, "foo41", [rwTextureDepth2D]), 8);
    checkUint(program, callFunction(program, "foo42", [rwTextureDepth2D]), 4);
    checkUint(program, callFunction(program, "foo43", [rwTextureDepth2DArray]), 8);
    checkUint(program, callFunction(program, "foo44", [rwTextureDepth2DArray]), 4);
    checkUint(program, callFunction(program, "foo45", [rwTextureDepth2DArray]), 2);
}

tests.textureDimensionsNull = function() {
    let program = doPrep(`
        test bool foo1(Texture1D<float> texture) {
            GetDimensions(texture, 0, null, null);
            return true;
        }
        test bool foo2(Texture1DArray<float> texture) {
            GetDimensions(texture, 0, null, null, null);
            return true;
        }
        test bool foo3(Texture2D<float> texture) {
            GetDimensions(texture, 0, null, null, null);
            return true;
        }
        test bool foo4(Texture2DArray<float> texture) {
            GetDimensions(texture, 0, null, null, null, null);
            return true;
        }
        test bool foo5(Texture3D<float> texture) {
            GetDimensions(texture, 0, null, null, null, null);
            return true;
        }
        test bool foo6(TextureCube<float> texture) {
            GetDimensions(texture, 0, null, null, null);
            return true;
        }
        test bool foo7(RWTexture1D<float> texture) {
            thread float* ptr = null;
            GetDimensions(texture, ptr);
            return true;
        }
        test bool foo8(RWTexture1DArray<float> texture) {
            thread uint* ptr = null;
            GetDimensions(texture, ptr, ptr);
            return true;
        }
        test bool foo9(RWTexture2D<float> texture) {
            thread uint* ptr = null;
            GetDimensions(texture, ptr, ptr);
            return true;
        }
        test bool foo10(RWTexture2DArray<float> texture) {
            thread uint* ptr = null;
            GetDimensions(texture, ptr, ptr, ptr);
            return true;
        }
        test bool foo11(RWTexture3D<float> texture) {
            thread uint* ptr = null;
            GetDimensions(texture, ptr, ptr, ptr);
            return true;
        }
        test bool foo12(TextureDepth2D<float> texture) {
            GetDimensions(texture, 0, null, null, null);
            return true;
        }
        test bool foo13(TextureDepth2DArray<float> texture) {
            GetDimensions(texture, 0, null, null, null, null);
            return true;
        }
        test bool foo14(TextureDepthCube<float> texture) {
            GetDimensions(texture, 0, null, null, null);
            return true;
        }
        test bool foo15(RWTextureDepth2D<float> texture) {
            thread uint* ptr = null;
            GetDimensions(texture, ptr, ptr);
            return true;
        }
        test bool foo16(RWTextureDepth2DArray<float> texture) {
            thread uint* ptr = null;
            GetDimensions(texture, ptr, ptr, ptr);
            return true;
        }
    `);
    let [texture1D, texture1DArray, texture2D, texture2DArray, texture3D, textureCube, rwTexture1D, rwTexture1DArray, rwTexture2D, rwTexture2DArray, rwTexture3D, textureDepth2D, textureDepth2DArray, textureDepthCube, rwTextureDepth2D, rwTextureDepth2DArray] = createTexturesForTesting(program);
    checkBool(program, callFunction(program, "foo1", [texture1D]), true);
    checkBool(program, callFunction(program, "foo2", [texture1DArray]), true);
    checkBool(program, callFunction(program, "foo3", [texture2D]), true);
    checkBool(program, callFunction(program, "foo4", [texture2DArray]), true);
    checkBool(program, callFunction(program, "foo5", [texture3D]), true);
    checkBool(program, callFunction(program, "foo6", [textureCube]), true);
    checkBool(program, callFunction(program, "foo7", [rwTexture1D]), true);
    checkBool(program, callFunction(program, "foo8", [rwTexture1DArray]), true);
    checkBool(program, callFunction(program, "foo9", [rwTexture2D]), true);
    checkBool(program, callFunction(program, "foo10", [rwTexture2DArray]), true);
    checkBool(program, callFunction(program, "foo11", [rwTexture3D]), true);
    checkBool(program, callFunction(program, "foo12", [textureDepth2D]), true);
    checkBool(program, callFunction(program, "foo13", [textureDepth2DArray]), true);
    checkBool(program, callFunction(program, "foo14", [textureDepthCube]), true);
    checkBool(program, callFunction(program, "foo15", [rwTextureDepth2D]), true);
    checkBool(program, callFunction(program, "foo16", [rwTextureDepth2DArray]), true);
}

tests.textureLoad = function() {
    let program = doPrep(`
        test float foo1(Texture1D<float> texture, int location, int mipmap) {
            return Load(texture, int2(location, mipmap));
        }
        test float foo2(Texture1D<float> texture, int location, int mipmap, int offset) {
            return Load(texture, int2(location, mipmap), offset);
        }
        test float foo3(Texture1DArray<float> texture, int location, int mipmap, int layer) {
            return Load(texture, int3(location, mipmap, layer));
        }
        test float foo4(Texture1DArray<float> texture, int location, int mipmap, int layer, int offset) {
            return Load(texture, int3(location, mipmap, layer), offset);
        }
        test float foo5(Texture2D<float> texture, int x, int y, int mipmap) {
            return Load(texture, int3(x, y, mipmap));
        }
        test float foo6(Texture2D<float> texture, int x, int y, int mipmap, int offsetX, int offsetY) {
            return Load(texture, int3(x, y, mipmap), int2(offsetX, offsetY));
        }
        test float foo7(Texture2DArray<float> texture, int x, int y, int mipmap, int layer) {
            return Load(texture, int4(x, y, mipmap, layer));
        }
        test float foo8(Texture2DArray<float> texture, int x, int y, int mipmap, int layer, int offsetX, int offsetY) {
            return Load(texture, int4(x, y, mipmap, layer), int2(offsetX, offsetY));
        }
        test float foo9(Texture3D<float> texture, int x, int y, int z, int mipmap) {
            return Load(texture, int4(x, y, z, mipmap));
        }
        test float foo10(Texture3D<float> texture, int x, int y, int z, int mipmap, int offsetX, int offsetY, int offsetZ) {
            return Load(texture, int4(x, y, z, mipmap), int3(offsetX, offsetY, offsetZ));
        }
        test float foo11(RWTexture1D<float> texture, int location) {
            return Load(texture, location);
        }
        test float foo12(RWTexture1DArray<float> texture, int location, int layer) {
            return Load(texture, int2(location, layer));
        }
        test float foo13(RWTexture2D<float> texture, int x, int y) {
            return Load(texture, int2(x, y));
        }
        test float foo14(RWTexture2DArray<float> texture, int x, int y, int layer) {
            return Load(texture, int3(x, y, layer));
        }
        test float foo15(RWTexture3D<float> texture, int x, int y, int z) {
            return Load(texture, int3(x, y, z));
        }
        test float foo16(TextureDepth2D<float> texture, int x, int y, int mipmap) {
            return Load(texture, int3(x, y, mipmap));
        }
        test float foo17(TextureDepth2D<float> texture, int x, int y, int mipmap, int offsetX, int offsetY) {
            return Load(texture, int3(x, y, mipmap), int2(offsetX, offsetY));
        }
        test float foo18(TextureDepth2DArray<float> texture, int x, int y, int mipmap, int layer) {
            return Load(texture, int4(x, y, mipmap, layer));
        }
        test float foo19(TextureDepth2DArray<float> texture, int x, int y, int mipmap, int layer, int offsetX, int offsetY) {
            return Load(texture, int4(x, y, mipmap, layer), int2(offsetX, offsetY));
        }
        test float foo20(RWTextureDepth2D<float> texture, int x, int y) {
            return Load(texture, int2(x, y));
        }
        test float foo21(RWTextureDepth2DArray<float> texture, int x, int y, int layer) {
            return Load(texture, int3(x, y, layer));
        }
    `);
    let [texture1D, texture1DArray, texture2D, texture2DArray, texture3D, textureCube, rwTexture1D, rwTexture1DArray, rwTexture2D, rwTexture2DArray, rwTexture3D, textureDepth2D, textureDepth2DArray, textureDepthCube, rwTextureDepth2D, rwTextureDepth2DArray] = createTexturesForTesting(program);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeInt(program, 0), makeInt(program, 0)]), 1);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeInt(program, 1), makeInt(program, 0)]), 7);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeInt(program, 0), makeInt(program, 1)]), 13);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeInt(program, 1), makeInt(program, 1)]), 16);
    checkFloat(program, callFunction(program, "foo2", [texture1D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1)]), 7);
    checkFloat(program, callFunction(program, "foo2", [texture1D, makeInt(program, 1), makeInt(program, 0), makeInt(program, -1)]), 1);
    checkFloat(program, callFunction(program, "foo2", [texture1D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 16);
    checkFloat(program, callFunction(program, "foo2", [texture1D, makeInt(program, 1), makeInt(program, 1), makeInt(program, -1)]), 13);
    checkFloat(program, callFunction(program, "foo3", [texture1DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0)]), 1);
    checkFloat(program, callFunction(program, "foo3", [texture1DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 7);
    checkFloat(program, callFunction(program, "foo3", [texture1DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 13);
    checkFloat(program, callFunction(program, "foo3", [texture1DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 16);
    checkFloat(program, callFunction(program, "foo3", [texture1DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1)]), 16);
    checkFloat(program, callFunction(program, "foo3", [texture1DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1)]), 17);
    checkFloat(program, callFunction(program, "foo3", [texture1DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 20);
    checkFloat(program, callFunction(program, "foo3", [texture1DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 21);
    checkFloat(program, callFunction(program, "foo4", [texture1DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1)]), 7);
    checkFloat(program, callFunction(program, "foo4", [texture1DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, -1)]), 1);
    checkFloat(program, callFunction(program, "foo4", [texture1DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1)]), 16);
    checkFloat(program, callFunction(program, "foo4", [texture1DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, -1)]), 13);
    checkFloat(program, callFunction(program, "foo4", [texture1DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 17);
    checkFloat(program, callFunction(program, "foo4", [texture1DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, -1)]), 16);
    checkFloat(program, callFunction(program, "foo4", [texture1DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 21);
    checkFloat(program, callFunction(program, "foo4", [texture1DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, -1)]), 20);
    checkFloat(program, callFunction(program, "foo5", [texture2D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0)]), 1);
    checkFloat(program, callFunction(program, "foo5", [texture2D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 2);
    checkFloat(program, callFunction(program, "foo5", [texture2D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 9);
    checkFloat(program, callFunction(program, "foo5", [texture2D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 10);
    checkFloat(program, callFunction(program, "foo5", [texture2D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1)]), 33);
    checkFloat(program, callFunction(program, "foo5", [texture2D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1)]), 34);
    checkFloat(program, callFunction(program, "foo5", [texture2D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 37);
    checkFloat(program, callFunction(program, "foo5", [texture2D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 38);
    checkFloat(program, callFunction(program, "foo6", [texture2D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 2);
    checkFloat(program, callFunction(program, "foo6", [texture2D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 10);
    checkFloat(program, callFunction(program, "foo6", [texture2D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, -1), makeInt(program, 0)]), 1);
    checkFloat(program, callFunction(program, "foo6", [texture2D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, -1), makeInt(program, 1)]), 9);
    checkFloat(program, callFunction(program, "foo6", [texture2D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 10);
    checkFloat(program, callFunction(program, "foo6", [texture2D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 18);
    checkFloat(program, callFunction(program, "foo6", [texture2D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, -1), makeInt(program, 0)]), 9);
    checkFloat(program, callFunction(program, "foo6", [texture2D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, -1), makeInt(program, 1)]), 17);
    checkFloat(program, callFunction(program, "foo6", [texture2D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 34);
    checkFloat(program, callFunction(program, "foo6", [texture2D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 38);
    checkFloat(program, callFunction(program, "foo6", [texture2D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, -1), makeInt(program, 0)]), 33);
    checkFloat(program, callFunction(program, "foo6", [texture2D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, -1), makeInt(program, 1)]), 37);
    checkFloat(program, callFunction(program, "foo6", [texture2D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 38);
    checkFloat(program, callFunction(program, "foo6", [texture2D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, -1)]), 34);
    checkFloat(program, callFunction(program, "foo6", [texture2D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, -1), makeInt(program, 0)]), 37);
    checkFloat(program, callFunction(program, "foo6", [texture2D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, -1), makeInt(program, -1)]), 33);
    checkFloat(program, callFunction(program, "foo7", [texture2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0)]), 1);
    checkFloat(program, callFunction(program, "foo7", [texture2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0)]), 2);
    checkFloat(program, callFunction(program, "foo7", [texture2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 9);
    checkFloat(program, callFunction(program, "foo7", [texture2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 10);
    checkFloat(program, callFunction(program, "foo7", [texture2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 33);
    checkFloat(program, callFunction(program, "foo7", [texture2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 34);
    checkFloat(program, callFunction(program, "foo7", [texture2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 37);
    checkFloat(program, callFunction(program, "foo7", [texture2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 38);
    checkFloat(program, callFunction(program, "foo7", [texture2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1)]), 43);
    checkFloat(program, callFunction(program, "foo7", [texture2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1)]), 44);
    checkFloat(program, callFunction(program, "foo7", [texture2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1)]), 51);
    checkFloat(program, callFunction(program, "foo7", [texture2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1)]), 52);
    checkFloat(program, callFunction(program, "foo7", [texture2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 75);
    checkFloat(program, callFunction(program, "foo7", [texture2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 76);
    checkFloat(program, callFunction(program, "foo7", [texture2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 79);
    checkFloat(program, callFunction(program, "foo7", [texture2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 80);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 2);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 10);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 3);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 11);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 10);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 18);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 11);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 19);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 34);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 38);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 35);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 39);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 38);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, -1)]), 34);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 39);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, -1)]), 35);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 44);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 52);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 45);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 53);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 52);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 60);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 53);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 61);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 76);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 80);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 77);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 81);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 80);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, -1)]), 76);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 81);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, -1)]), 77);
    checkFloat(program, callFunction(program, "foo9", [texture3D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0)]), 1);
    checkFloat(program, callFunction(program, "foo9", [texture3D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0)]), 2);
    checkFloat(program, callFunction(program, "foo9", [texture3D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 9);
    checkFloat(program, callFunction(program, "foo9", [texture3D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 10);
    checkFloat(program, callFunction(program, "foo9", [texture3D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 33);
    checkFloat(program, callFunction(program, "foo9", [texture3D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 34);
    checkFloat(program, callFunction(program, "foo9", [texture3D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 41);
    checkFloat(program, callFunction(program, "foo9", [texture3D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 42);
    checkFloat(program, callFunction(program, "foo9", [texture3D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1)]), 65);
    checkFloat(program, callFunction(program, "foo9", [texture3D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1)]), 66);
    checkFloat(program, callFunction(program, "foo9", [texture3D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1)]), 69);
    checkFloat(program, callFunction(program, "foo9", [texture3D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1)]), 70);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 2);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 9);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1)]), 33);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 3);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 10);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1)]), 34);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 10);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 17);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1)]), 41);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 11);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 18);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1)]), 42);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 34);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 41);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, -1)]), 1);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 35);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 42);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, -1)]), 2);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 42);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 49);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, -1)]), 9);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 43);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 50);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, -1)]), 10);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 66);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 69);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 67);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 70);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 70);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, -1), makeInt(program, 0)]), 65);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 71);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, -1), makeInt(program, 0)]), 66);
    checkFloat(program, callFunction(program, "foo11", [rwTexture1D, makeInt(program, 0)]), 1);
    checkFloat(program, callFunction(program, "foo11", [rwTexture1D, makeInt(program, 1)]), 2);
    checkFloat(program, callFunction(program, "foo11", [rwTexture1D, makeInt(program, 2)]), 3);
    checkFloat(program, callFunction(program, "foo11", [rwTexture1D, makeInt(program, 3)]), 4);
    checkFloat(program, callFunction(program, "foo11", [rwTexture1D, makeInt(program, 4)]), 5);
    checkFloat(program, callFunction(program, "foo11", [rwTexture1D, makeInt(program, 5)]), 6);
    checkFloat(program, callFunction(program, "foo11", [rwTexture1D, makeInt(program, 6)]), 7);
    checkFloat(program, callFunction(program, "foo11", [rwTexture1D, makeInt(program, 7)]), 8);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 0), makeInt(program, 0)]), 1);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 1), makeInt(program, 0)]), 2);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 2), makeInt(program, 0)]), 3);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 3), makeInt(program, 0)]), 4);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 4), makeInt(program, 0)]), 5);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 5), makeInt(program, 0)]), 6);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 6), makeInt(program, 0)]), 7);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 7), makeInt(program, 0)]), 8);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 0), makeInt(program, 1)]), 9);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 1), makeInt(program, 1)]), 10);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 2), makeInt(program, 1)]), 11);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 3), makeInt(program, 1)]), 12);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 4), makeInt(program, 1)]), 13);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 5), makeInt(program, 1)]), 14);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 6), makeInt(program, 1)]), 15);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 7), makeInt(program, 1)]), 16);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 0), makeInt(program, 2)]), 17);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 1), makeInt(program, 2)]), 18);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 2), makeInt(program, 2)]), 19);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 3), makeInt(program, 2)]), 20);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 4), makeInt(program, 2)]), 21);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 5), makeInt(program, 2)]), 22);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 6), makeInt(program, 2)]), 23);
    checkFloat(program, callFunction(program, "foo12", [rwTexture1DArray, makeInt(program, 7), makeInt(program, 2)]), 24);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 0), makeInt(program, 0)]), 1);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 1), makeInt(program, 0)]), 2);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 2), makeInt(program, 0)]), 3);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 3), makeInt(program, 0)]), 4);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 4), makeInt(program, 0)]), 5);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 5), makeInt(program, 0)]), 6);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 6), makeInt(program, 0)]), 7);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 7), makeInt(program, 0)]), 8);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 0), makeInt(program, 1)]), 9);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 1), makeInt(program, 1)]), 10);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 2), makeInt(program, 1)]), 11);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 3), makeInt(program, 1)]), 12);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 4), makeInt(program, 1)]), 13);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 5), makeInt(program, 1)]), 14);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 6), makeInt(program, 1)]), 15);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 7), makeInt(program, 1)]), 16);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 0), makeInt(program, 2)]), 17);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 1), makeInt(program, 2)]), 18);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 2), makeInt(program, 2)]), 19);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 3), makeInt(program, 2)]), 20);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 4), makeInt(program, 2)]), 21);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 5), makeInt(program, 2)]), 22);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 6), makeInt(program, 2)]), 23);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 7), makeInt(program, 2)]), 24);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 0), makeInt(program, 3)]), 25);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 1), makeInt(program, 3)]), 26);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 2), makeInt(program, 3)]), 27);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 3), makeInt(program, 3)]), 28);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 4), makeInt(program, 3)]), 29);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 5), makeInt(program, 3)]), 30);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 6), makeInt(program, 3)]), 31);
    checkFloat(program, callFunction(program, "foo13", [rwTexture2D, makeInt(program, 7), makeInt(program, 3)]), 32);
    checkFloat(program, callFunction(program, "foo14", [rwTexture2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0)]), 1);
    checkFloat(program, callFunction(program, "foo14", [rwTexture2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 2);
    checkFloat(program, callFunction(program, "foo14", [rwTexture2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 9);
    checkFloat(program, callFunction(program, "foo14", [rwTexture2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 10);
    checkFloat(program, callFunction(program, "foo14", [rwTexture2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1)]), 33);
    checkFloat(program, callFunction(program, "foo14", [rwTexture2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1)]), 34);
    checkFloat(program, callFunction(program, "foo14", [rwTexture2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 41);
    checkFloat(program, callFunction(program, "foo14", [rwTexture2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 42);
    checkFloat(program, callFunction(program, "foo15", [rwTexture3D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0)]), 1);
    checkFloat(program, callFunction(program, "foo15", [rwTexture3D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 2);
    checkFloat(program, callFunction(program, "foo15", [rwTexture3D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 9);
    checkFloat(program, callFunction(program, "foo15", [rwTexture3D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 10);
    checkFloat(program, callFunction(program, "foo15", [rwTexture3D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1)]), 33);
    checkFloat(program, callFunction(program, "foo15", [rwTexture3D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1)]), 34);
    checkFloat(program, callFunction(program, "foo15", [rwTexture3D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 41);
    checkFloat(program, callFunction(program, "foo15", [rwTexture3D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 42);
    checkFloat(program, callFunction(program, "foo16", [textureDepth2D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0)]), 1);
    checkFloat(program, callFunction(program, "foo16", [textureDepth2D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 2);
    checkFloat(program, callFunction(program, "foo16", [textureDepth2D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 9);
    checkFloat(program, callFunction(program, "foo16", [textureDepth2D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 10);
    checkFloat(program, callFunction(program, "foo16", [textureDepth2D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1)]), 33);
    checkFloat(program, callFunction(program, "foo16", [textureDepth2D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1)]), 34);
    checkFloat(program, callFunction(program, "foo16", [textureDepth2D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 37);
    checkFloat(program, callFunction(program, "foo16", [textureDepth2D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 38);
    checkFloat(program, callFunction(program, "foo17", [textureDepth2D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 2);
    checkFloat(program, callFunction(program, "foo17", [textureDepth2D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 10);
    checkFloat(program, callFunction(program, "foo17", [textureDepth2D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, -1), makeInt(program, 0)]), 1);
    checkFloat(program, callFunction(program, "foo17", [textureDepth2D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, -1), makeInt(program, 1)]), 9);
    checkFloat(program, callFunction(program, "foo17", [textureDepth2D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 10);
    checkFloat(program, callFunction(program, "foo17", [textureDepth2D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 18);
    checkFloat(program, callFunction(program, "foo17", [textureDepth2D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, -1), makeInt(program, 0)]), 9);
    checkFloat(program, callFunction(program, "foo17", [textureDepth2D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, -1), makeInt(program, 1)]), 17);
    checkFloat(program, callFunction(program, "foo17", [textureDepth2D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 34);
    checkFloat(program, callFunction(program, "foo17", [textureDepth2D, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 38);
    checkFloat(program, callFunction(program, "foo17", [textureDepth2D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, -1), makeInt(program, 0)]), 33);
    checkFloat(program, callFunction(program, "foo17", [textureDepth2D, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, -1), makeInt(program, 1)]), 37);
    checkFloat(program, callFunction(program, "foo17", [textureDepth2D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 38);
    checkFloat(program, callFunction(program, "foo17", [textureDepth2D, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, -1)]), 34);
    checkFloat(program, callFunction(program, "foo17", [textureDepth2D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, -1), makeInt(program, 0)]), 37);
    checkFloat(program, callFunction(program, "foo17", [textureDepth2D, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, -1), makeInt(program, -1)]), 33);
    checkFloat(program, callFunction(program, "foo18", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0)]), 1);
    checkFloat(program, callFunction(program, "foo18", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0)]), 2);
    checkFloat(program, callFunction(program, "foo18", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 9);
    checkFloat(program, callFunction(program, "foo18", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 10);
    checkFloat(program, callFunction(program, "foo18", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 33);
    checkFloat(program, callFunction(program, "foo18", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 34);
    checkFloat(program, callFunction(program, "foo18", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 37);
    checkFloat(program, callFunction(program, "foo18", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 38);
    checkFloat(program, callFunction(program, "foo18", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1)]), 43);
    checkFloat(program, callFunction(program, "foo18", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1)]), 44);
    checkFloat(program, callFunction(program, "foo18", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1)]), 51);
    checkFloat(program, callFunction(program, "foo18", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1)]), 52);
    checkFloat(program, callFunction(program, "foo18", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 75);
    checkFloat(program, callFunction(program, "foo18", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 76);
    checkFloat(program, callFunction(program, "foo18", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 79);
    checkFloat(program, callFunction(program, "foo18", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 80);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 2);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 10);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 3);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 11);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 10);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 18);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 11);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 19);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 34);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 38);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 35);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 39);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 38);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, -1)]), 34);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 39);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, -1)]), 35);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 44);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 52);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 45);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 53);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 52);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 60);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 53);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 61);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 76);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 80);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 77);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 81);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 80);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, -1)]), 76);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 81);
    checkFloat(program, callFunction(program, "foo19", [textureDepth2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, 1), makeInt(program, -1)]), 77);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 0), makeInt(program, 0)]), 1);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 1), makeInt(program, 0)]), 2);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 2), makeInt(program, 0)]), 3);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 3), makeInt(program, 0)]), 4);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 4), makeInt(program, 0)]), 5);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 5), makeInt(program, 0)]), 6);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 6), makeInt(program, 0)]), 7);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 7), makeInt(program, 0)]), 8);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 0), makeInt(program, 1)]), 9);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 1), makeInt(program, 1)]), 10);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 2), makeInt(program, 1)]), 11);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 3), makeInt(program, 1)]), 12);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 4), makeInt(program, 1)]), 13);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 5), makeInt(program, 1)]), 14);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 6), makeInt(program, 1)]), 15);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 7), makeInt(program, 1)]), 16);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 0), makeInt(program, 2)]), 17);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 1), makeInt(program, 2)]), 18);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 2), makeInt(program, 2)]), 19);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 3), makeInt(program, 2)]), 20);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 4), makeInt(program, 2)]), 21);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 5), makeInt(program, 2)]), 22);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 6), makeInt(program, 2)]), 23);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 7), makeInt(program, 2)]), 24);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 0), makeInt(program, 3)]), 25);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 1), makeInt(program, 3)]), 26);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 2), makeInt(program, 3)]), 27);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 3), makeInt(program, 3)]), 28);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 4), makeInt(program, 3)]), 29);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 5), makeInt(program, 3)]), 30);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 6), makeInt(program, 3)]), 31);
    checkFloat(program, callFunction(program, "foo20", [rwTextureDepth2D, makeInt(program, 7), makeInt(program, 3)]), 32);
    checkFloat(program, callFunction(program, "foo21", [rwTextureDepth2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 0)]), 1);
    checkFloat(program, callFunction(program, "foo21", [rwTextureDepth2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), 2);
    checkFloat(program, callFunction(program, "foo21", [rwTextureDepth2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), 9);
    checkFloat(program, callFunction(program, "foo21", [rwTextureDepth2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 0)]), 10);
    checkFloat(program, callFunction(program, "foo21", [rwTextureDepth2DArray, makeInt(program, 0), makeInt(program, 0), makeInt(program, 1)]), 33);
    checkFloat(program, callFunction(program, "foo21", [rwTextureDepth2DArray, makeInt(program, 1), makeInt(program, 0), makeInt(program, 1)]), 34);
    checkFloat(program, callFunction(program, "foo21", [rwTextureDepth2DArray, makeInt(program, 0), makeInt(program, 1), makeInt(program, 1)]), 41);
    checkFloat(program, callFunction(program, "foo21", [rwTextureDepth2DArray, makeInt(program, 1), makeInt(program, 1), makeInt(program, 1)]), 42);
}

tests.textureStore = function() {
    let program = doPrep(`
        test float foo1(RWTexture1D<float> texture, uint location, float value) {
            Store(texture, value, location);
            return Load(texture, int(location));
        }
        test float foo2(RWTexture1DArray<float> texture, uint location, uint layer, float value) {
            Store(texture, value, uint2(location, layer));
            return Load(texture, int2(int(location), int(layer)));
        }
        test float foo3(RWTexture2D<float> texture, uint x, uint y, float value) {
            Store(texture, value, uint2(x, y));
            return Load(texture, int2(int(x), int(y)));
        }
        test float foo4(RWTexture2DArray<float> texture, uint x, uint y, uint layer, float value) {
            Store(texture, value, uint3(x, y, layer));
            return Load(texture, int3(int(x), int(y), int(layer)));
        }
        test float foo5(RWTexture3D<float> texture, uint x, uint y, uint z, float value) {
            Store(texture, value, uint3(x, y, z));
            return Load(texture, int3(int(x), int(y), int(z)));
        }
        test float foo6(RWTextureDepth2D<float> texture, uint x, uint y, float value) {
            Store(texture, value, uint2(x, y));
            return Load(texture, int2(int(x), int(y)));
        }
        test float foo7(RWTextureDepth2DArray<float> texture, uint x, uint y, uint layer, float value) {
            Store(texture, value, uint3(x, y, layer));
            return Load(texture, int3(int(x), int(y), int(layer)));
        }
    `);
    let [texture1D, texture1DArray, texture2D, texture2DArray, texture3D, textureCube, rwTexture1D, rwTexture1DArray, rwTexture2D, rwTexture2DArray, rwTexture3D, textureDepth2D, textureDepth2DArray, textureDepthCube, rwTextureDepth2D, rwTextureDepth2DArray] = createTexturesForTesting(program);
    checkFloat(program, callFunction(program, "foo1", [rwTexture1D, makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo1", [rwTexture1D, makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo1", [rwTexture1D, makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo1", [rwTexture1D, makeUint(program, 3), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo1", [rwTexture1D, makeUint(program, 4), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo1", [rwTexture1D, makeUint(program, 5), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo1", [rwTexture1D, makeUint(program, 6), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo1", [rwTexture1D, makeUint(program, 7), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 0), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 1), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 2), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 3), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 4), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 5), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 6), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 7), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 0), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 1), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 2), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 3), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 4), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 5), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 6), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 7), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 0), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 1), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 2), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 3), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 4), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 5), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 6), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo2", [rwTexture1DArray, makeUint(program, 7), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 0), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 1), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 2), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 3), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 4), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 5), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 6), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 7), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 0), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 1), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 2), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 3), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 4), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 5), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 6), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 7), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 0), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 1), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 2), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 3), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 4), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 5), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 6), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 7), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 0), makeUint(program, 3), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 1), makeUint(program, 3), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 2), makeUint(program, 3), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 3), makeUint(program, 3), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 4), makeUint(program, 3), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 5), makeUint(program, 3), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 6), makeUint(program, 3), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo3", [rwTexture2D, makeUint(program, 7), makeUint(program, 3), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo4", [rwTexture2DArray, makeUint(program, 0), makeUint(program, 0), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo4", [rwTexture2DArray, makeUint(program, 1), makeUint(program, 0), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo4", [rwTexture2DArray, makeUint(program, 0), makeUint(program, 1), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo4", [rwTexture2DArray, makeUint(program, 1), makeUint(program, 1), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo4", [rwTexture2DArray, makeUint(program, 0), makeUint(program, 0), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo4", [rwTexture2DArray, makeUint(program, 1), makeUint(program, 0), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo4", [rwTexture2DArray, makeUint(program, 0), makeUint(program, 1), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo4", [rwTexture2DArray, makeUint(program, 1), makeUint(program, 1), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo5", [rwTexture3D, makeUint(program, 0), makeUint(program, 0), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo5", [rwTexture3D, makeUint(program, 1), makeUint(program, 0), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo5", [rwTexture3D, makeUint(program, 0), makeUint(program, 1), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo5", [rwTexture3D, makeUint(program, 1), makeUint(program, 1), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo5", [rwTexture3D, makeUint(program, 0), makeUint(program, 0), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo5", [rwTexture3D, makeUint(program, 1), makeUint(program, 0), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo5", [rwTexture3D, makeUint(program, 0), makeUint(program, 1), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo5", [rwTexture3D, makeUint(program, 1), makeUint(program, 1), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 0), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 1), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 2), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 3), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 4), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 5), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 6), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 7), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 0), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 1), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 2), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 3), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 4), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 5), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 6), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 7), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 0), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 1), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 2), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 3), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 4), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 5), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 6), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 7), makeUint(program, 2), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 0), makeUint(program, 3), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 1), makeUint(program, 3), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 2), makeUint(program, 3), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 3), makeUint(program, 3), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 4), makeUint(program, 3), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 5), makeUint(program, 3), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 6), makeUint(program, 3), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo6", [rwTextureDepth2D, makeUint(program, 7), makeUint(program, 3), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo7", [rwTextureDepth2DArray, makeUint(program, 0), makeUint(program, 0), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo7", [rwTextureDepth2DArray, makeUint(program, 1), makeUint(program, 0), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo7", [rwTextureDepth2DArray, makeUint(program, 0), makeUint(program, 1), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo7", [rwTextureDepth2DArray, makeUint(program, 1), makeUint(program, 1), makeUint(program, 0), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo7", [rwTextureDepth2DArray, makeUint(program, 0), makeUint(program, 0), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo7", [rwTextureDepth2DArray, makeUint(program, 1), makeUint(program, 0), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo7", [rwTextureDepth2DArray, makeUint(program, 0), makeUint(program, 1), makeUint(program, 1), makeFloat(program, 999)]), 999);
    checkFloat(program, callFunction(program, "foo7", [rwTextureDepth2DArray, makeUint(program, 1), makeUint(program, 1), makeUint(program, 1), makeFloat(program, 999)]), 999);
}

tests.textureSample = function() {
    let program = doPrep(`
        test float foo1(Texture1D<float> texture, sampler s, float location) {
            return Sample(texture, s, location);
        }
        test float foo2(Texture1D<float> texture, sampler s, float location, int offset) {
            return Sample(texture, s, location, offset);
        }
        test float foo3(Texture1DArray<float> texture, sampler s, float x, float layer) {
            return Sample(texture, s, float2(x, layer));
        }
        test float foo4(Texture1DArray<float> texture, sampler s, float x, float layer, int offset) {
            return Sample(texture, s, float2(x, layer), offset);
        }
        test float foo5(Texture2D<float> texture, sampler s, float x, float y) {
            return Sample(texture, s, float2(x, y));
        }
        test float foo6(Texture2D<float> texture, sampler s, float x, float y, int offsetX, int offsetY) {
            return Sample(texture, s, float2(x, y), int2(offsetX, offsetY));
        }
        test float foo7(Texture2DArray<float> texture, sampler s, float x, float y, float layer) {
            return Sample(texture, s, float3(x, y, layer));
        }
        test float foo8(Texture2DArray<float> texture, sampler s, float x, float y, float layer, int offsetX, int offsetY) {
            return Sample(texture, s, float3(x, y, layer), int2(offsetX, offsetY));
        }
        test float foo9(Texture3D<float> texture, sampler s, float x, float y, float z) {
            return Sample(texture, s, float3(x, y, z));
        }
        test float foo10(Texture3D<float> texture, sampler s, float x, float y, float z, int offsetX, int offsetY, int offsetZ) {
            return Sample(texture, s, float3(x, y, z), int3(offsetX, offsetY, offsetZ));
        }
        test float foo11(TextureCube<float> texture, sampler s, float x, float y, float z) {
            return Sample(texture, s, float3(x, y, z));
        }
        test float foo12(Texture2D<float> texture, sampler s, float x, float y, float Bias) {
            return SampleBias(texture, s, float2(x, y), Bias);
        }
        test float foo13(Texture2D<float> texture, sampler s, float x, float y, float Bias, int offsetX, int offsetY) {
            return SampleBias(texture, s, float2(x, y), Bias, int2(offsetX, offsetY));
        }
        test float foo14(Texture2D<float> texture, sampler s, float x, float y, float ddx0, float ddx1, float ddy0, float ddy1) {
            return SampleGrad(texture, s, float2(x, y), float2(ddx0, ddx1), float2(ddy0, ddy1));
        }
        test float foo15(Texture2D<float> texture, sampler s, float x, float y, float ddx0, float ddx1, float ddy0, float ddy1, int offsetX, int offsetY) {
            return SampleGrad(texture, s, float2(x, y), float2(ddx0, ddx1), float2(ddy0, ddy1), int2(offsetX, offsetY));
        }
        test float foo16(Texture2D<float> texture, sampler s, float x, float y, float LOD) {
            return SampleLevel(texture, s, float2(x, y), LOD);
        }
        test float foo17(Texture2D<float> texture, sampler s, float x, float y, float LOD, int offsetX, int offsetY) {
            return SampleLevel(texture, s, float2(x, y), LOD, int2(offsetX, offsetY));
        }
        test float foo18(Texture2DArray<float> texture, sampler s, float x, float y, float layer, float Bias) {
            return SampleBias(texture, s, float3(x, y, layer), Bias);
        }
        test float foo19(Texture2DArray<float> texture, sampler s, float x, float y, float layer, float Bias, int offsetX, int offsetY) {
            return SampleBias(texture, s, float3(x, y, layer), Bias, int2(offsetX, offsetY));
        }
        test float foo20(Texture2DArray<float> texture, sampler s, float x, float y, float layer, float ddx0, float ddx1, float ddy0, float ddy1) {
            return SampleGrad(texture, s, float3(x, y, layer), float2(ddx0, ddx1), float2(ddy0, ddy1));
        }
        test float foo21(Texture2DArray<float> texture, sampler s, float x, float y, float layer, float ddx0, float ddx1, float ddy0, float ddy1, int offsetX, int offsetY) {
            return SampleGrad(texture, s, float3(x, y, layer), float2(ddx0, ddx1), float2(ddy0, ddy1), int2(offsetX, offsetY));
        }
        test float foo22(Texture2DArray<float> texture, sampler s, float x, float y, float layer, float LOD) {
            return SampleLevel(texture, s, float3(x, y, layer), LOD);
        }
        test float foo23(Texture2DArray<float> texture, sampler s, float x, float y, float layer, float LOD, int offsetX, int offsetY) {
            return SampleLevel(texture, s, float3(x, y, layer), LOD, int2(offsetX, offsetY));
        }
        test int foo24(Texture1D<int4> texture, sampler s, float location) {
            return Sample(texture, s, location).x;
        }
        test int foo25(Texture1D<int4> texture, sampler s, float location) {
            return Sample(texture, s, location).y;
        }
        test int foo26(Texture1D<int4> texture, sampler s, float location) {
            return Sample(texture, s, location).z;
        }
        test int foo27(Texture1D<int4> texture, sampler s, float location) {
            return Sample(texture, s, location).w;
        }
        test float foo28(TextureDepth2D<float> texture, sampler s, float x, float y) {
            return Sample(texture, s, float2(x, y));
        }
        test float foo29(TextureDepth2D<float> texture, sampler s, float x, float y, int offsetX, int offsetY) {
            return Sample(texture, s, float2(x, y), int2(offsetX, offsetY));
        }
        test float foo30(TextureDepth2DArray<float> texture, sampler s, float x, float y, float layer) {
            return Sample(texture, s, float3(x, y, layer));
        }
        test float foo31(TextureDepth2DArray<float> texture, sampler s, float x, float y, float layer, int offsetX, int offsetY) {
            return Sample(texture, s, float3(x, y, layer), int2(offsetX, offsetY));
        }
        test float foo32(TextureDepthCube<float> texture, sampler s, float x, float y, float z) {
            return Sample(texture, s, float3(x, y, z));
        }
        test float foo33(TextureDepth2D<float> texture, sampler s, float x, float y, float Bias) {
            return SampleBias(texture, s, float2(x, y), Bias);
        }
        test float foo34(TextureDepth2D<float> texture, sampler s, float x, float y, float Bias, int offsetX, int offsetY) {
            return SampleBias(texture, s, float2(x, y), Bias, int2(offsetX, offsetY));
        }
        test float foo35(TextureDepth2D<float> texture, sampler s, float x, float y, float ddx0, float ddx1, float ddy0, float ddy1) {
            return SampleGrad(texture, s, float2(x, y), float2(ddx0, ddx1), float2(ddy0, ddy1));
        }
        test float foo36(TextureDepth2D<float> texture, sampler s, float x, float y, float ddx0, float ddx1, float ddy0, float ddy1, int offsetX, int offsetY) {
            return SampleGrad(texture, s, float2(x, y), float2(ddx0, ddx1), float2(ddy0, ddy1), int2(offsetX, offsetY));
        }
        test float foo37(TextureDepth2D<float> texture, sampler s, float x, float y, float LOD) {
            return SampleLevel(texture, s, float2(x, y), LOD);
        }
        test float foo38(TextureDepth2D<float> texture, sampler s, float x, float y, float LOD, int offsetX, int offsetY) {
            return SampleLevel(texture, s, float2(x, y), LOD, int2(offsetX, offsetY));
        }
        test float foo39(TextureDepth2DArray<float> texture, sampler s, float x, float y, float layer, float Bias) {
            return SampleBias(texture, s, float3(x, y, layer), Bias);
        }
        test float foo40(TextureDepth2DArray<float> texture, sampler s, float x, float y, float layer, float Bias, int offsetX, int offsetY) {
            return SampleBias(texture, s, float3(x, y, layer), Bias, int2(offsetX, offsetY));
        }
        test float foo41(TextureDepth2DArray<float> texture, sampler s, float x, float y, float layer, float ddx0, float ddx1, float ddy0, float ddy1) {
            return SampleGrad(texture, s, float3(x, y, layer), float2(ddx0, ddx1), float2(ddy0, ddy1));
        }
        test float foo42(TextureDepth2DArray<float> texture, sampler s, float x, float y, float layer, float ddx0, float ddx1, float ddy0, float ddy1, int offsetX, int offsetY) {
            return SampleGrad(texture, s, float3(x, y, layer), float2(ddx0, ddx1), float2(ddy0, ddy1), int2(offsetX, offsetY));
        }
        test float foo43(TextureDepth2DArray<float> texture, sampler s, float x, float y, float layer, float LOD) {
            return SampleLevel(texture, s, float3(x, y, layer), LOD);
        }
        test float foo44(TextureDepth2DArray<float> texture, sampler s, float x, float y, float layer, float LOD, int offsetX, int offsetY) {
            return SampleLevel(texture, s, float3(x, y, layer), LOD, int2(offsetX, offsetY));
        }
        test float foo45(TextureDepth2D<float> texture, sampler s, float x, float y, float compareValue) {
            return SampleCmp(texture, s, float2(x, y), compareValue);
        }
        test float foo46(TextureDepth2D<float> texture, sampler s, float x, float y, float compareValue, int offsetX, int offsetY) {
            return SampleCmp(texture, s, float2(x, y), compareValue, int2(offsetX, offsetY));
        }
        test float foo47(TextureDepth2DArray<float> texture, sampler s, float x, float y, float layer, float compareValue) {
            return SampleCmp(texture, s, float3(x, y, layer), compareValue);
        }
        test float foo48(TextureDepth2DArray<float> texture, sampler s, float x, float y, float layer, float compareValue, int offsetX, int offsetY) {
            return SampleCmp(texture, s, float3(x, y, layer), compareValue, int2(offsetX, offsetY));
        }
        test float foo49(TextureCube<float> texture, sampler s, float x, float y, float z, float bias) {
            return SampleBias(texture, s, float3(x, y, z), bias);
        }
        test float foo50(TextureCube<float> texture, sampler s, float x, float y, float z, float ddx0, float ddx1, float ddx2, float ddy0, float ddy1, float ddy2) {
            return SampleGrad(texture, s, float3(x, y, z), float3(ddx0, ddx1, ddx2), float3(ddy0, ddy1, ddy2));
        }
        test float foo51(TextureCube<float> texture, sampler s, float x, float y, float z, float lod) {
            return SampleLevel(texture, s, float3(x, y, z), lod);
        }
        test float foo52(TextureDepthCube<float> texture, sampler s, float x, float y, float z, float bias) {
            return SampleBias(texture, s, float3(x, y, z), bias);
        }
        test float foo53(TextureDepthCube<float> texture, sampler s, float x, float y, float z, float ddx0, float ddx1, float ddx2, float ddy0, float ddy1, float ddy2) {
            return SampleGrad(texture, s, float3(x, y, z), float3(ddx0, ddx1, ddx2), float3(ddy0, ddy1, ddy2));
        }
        test float foo54(TextureDepthCube<float> texture, sampler s, float x, float y, float z, float lod) {
            return SampleLevel(texture, s, float3(x, y, z), lod);
        }
        test float foo55(TextureDepth2D<float> texture, sampler s, float x, float y, float compareValue) {
            return SampleCmpLevelZero(texture, s, float2(x, y), compareValue);
        }
        test float foo56(TextureDepth2D<float> texture, sampler s, float x, float y, float compareValue, int offsetX, int offsetY) {
            return SampleCmpLevelZero(texture, s, float2(x, y), compareValue, int2(offsetX, offsetY));
        }
        test float foo57(TextureDepth2DArray<float> texture, sampler s, float x, float y, float layer, float compareValue) {
            return SampleCmpLevelZero(texture, s, float3(x, y, layer), compareValue);
        }
        test float foo58(TextureDepth2DArray<float> texture, sampler s, float x, float y, float layer, float compareValue, int offsetX, int offsetY) {
            return SampleCmpLevelZero(texture, s, float3(x, y, layer), compareValue, int2(offsetX, offsetY));
        }
        test float foo59(TextureDepthCube<float> texture, sampler s, float x, float y, float z, float compareValue) {
            return SampleCmp(texture, s, float3(x, y, z), compareValue);
        }
        test float foo60(TextureDepthCube<float> texture, sampler s, float x, float y, float z, float compareValue) {
            return SampleCmpLevelZero(texture, s, float3(x, y, z), compareValue);
        }
    `);
    let [texture1D, texture1DArray, texture2D, texture2DArray, texture3D, textureCube, rwTexture1D, rwTexture1DArray, rwTexture2D, rwTexture2DArray, rwTexture3D, textureDepth2D, textureDepth2DArray, textureDepthCube, rwTextureDepth2D, rwTextureDepth2DArray] = createTexturesForTesting(program);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {}), makeFloat(program, 0.375)]), 7);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {}), makeFloat(program, 0.4375)]), 7);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {}), makeFloat(program, 0.5625)]), 14);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {}), makeFloat(program, 0.625)]), 14);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.375)]), 7);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.4375)]), (3 * 7 + 1 * 14) / 4);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5)]), (7 + 14) / 2);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5625)]), (1 * 7 + 3 * 14) / 4);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.625)]), 14);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "clampToBorderColor"}), makeFloat(program, 0.375)]), 7);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "clampToBorderColor"}), makeFloat(program, 0.4375)]), (3 * 7 + 1 * 14) / 4);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "clampToBorderColor"}), makeFloat(program, 0.5)]), (7 + 14) / 2);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "clampToBorderColor"}), makeFloat(program, 0.5625)]), (1 * 7 + 3 * 14) / 4);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "clampToBorderColor"}), makeFloat(program, 0.625)]), 14);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "repeat"}), makeFloat(program, 1 + 0.375)]), 7);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "repeat"}), makeFloat(program, 1 + 0.4375)]), (3 * 7 + 1 * 14) / 4);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "repeat"}), makeFloat(program, 1 + 0.5)]), (7 + 14) / 2);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "repeat"}), makeFloat(program, 1 + 0.5625)]), (1 * 7 + 3 * 14) / 4);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "repeat"}), makeFloat(program, 1 + 0.625)]), 14);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "repeat"}), makeFloat(program, 2 + 0.375)]), 7);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "repeat"}), makeFloat(program, 2 + 0.4375)]), (3 * 7 + 1 * 14) / 4);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "repeat"}), makeFloat(program, 2 + 0.5)]), (7 + 14) / 2);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "repeat"}), makeFloat(program, 2 + 0.5625)]), (1 * 7 + 3 * 14) / 4);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "repeat"}), makeFloat(program, 2 + 0.625)]), 14);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "mirrorRepeat"}), makeFloat(program, 1 + 0.625)]), 7);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "mirrorRepeat"}), makeFloat(program, 1 + 0.5625)]), (3 * 7 + 1 * 14) / 4);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "mirrorRepeat"}), makeFloat(program, 1 + 0.5)]), (7 + 14) / 2);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "mirrorRepeat"}), makeFloat(program, 1 + 0.4375)]), (1 * 7 + 3 * 14) / 4);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "mirrorRepeat"}), makeFloat(program, 1 + 0.375)]), 14);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "mirrorRepeat"}), makeFloat(program, 2 + 0.375)]), 7);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "mirrorRepeat"}), makeFloat(program, 2 + 0.4375)]), (3 * 7 + 1 * 14) / 4);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "mirrorRepeat"}), makeFloat(program, 2 + 0.5)]), (7 + 14) / 2);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "mirrorRepeat"}), makeFloat(program, 2 + 0.5625)]), (1 * 7 + 3 * 14) / 4);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "mirrorRepeat"}), makeFloat(program, 2 + 0.625)]), 14);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, -0.375)]), 1);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 1.375)]), 79);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "clampToBorderColor"}), makeFloat(program, -0.375)]), 0);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "clampToBorderColor"}), makeFloat(program, 1.375)]), 0);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "clampToBorderColor", borderColor: "opaqueBlack"}), makeFloat(program, -0.375)]), 0);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "clampToBorderColor", borderColor: "opaqueBlack"}), makeFloat(program, 1.375)]), 0);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "clampToBorderColor", borderColor: "opaqueWhite"}), makeFloat(program, -0.375)]), 1);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "clampToBorderColor", borderColor: "opaqueWhite"}), makeFloat(program, 1.375)]), 1);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {magFilter: "linear", rAddressMode: "clampToBorderColor", borderColor: "opaqueWhite"}), makeFloat(program, 1)]), (79 + 1) / 2);
    checkFloat(program, callFunction(program, "foo1", [texture1D, makeSampler(program, {minFilter: "linear", lodMinClamp: 1}), makeFloat(program, 0.5)]), (13 + 16) / 2);
    checkFloat(program, callFunction(program, "foo2", [texture1D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeInt(program, 1)]), (14 + 79) / 2);
    checkFloat(program, callFunction(program, "foo3", [texture1DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0)]), (7 + 14) / 2);
    checkFloat(program, callFunction(program, "foo3", [texture1DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 1)]), (17 + 18) / 2);
    checkFloat(program, callFunction(program, "foo4", [texture1DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0), makeInt(program, 1)]), (14 + 79) / 2);
    checkFloat(program, callFunction(program, "foo4", [texture1DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 1), makeInt(program, 1)]), (18 + 19) / 2);
    checkFloat(program, callFunction(program, "foo5", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo5", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 10 / 16), makeFloat(program, 0.5)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo5", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 6 / 8)]), (20 + 21 + 28 + 29) / 4);
    checkFloat(program, callFunction(program, "foo6", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo6", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeInt(program, 0), makeInt(program, 1)]), (20 + 21 + 28 + 29) / 4);
    checkFloat(program, callFunction(program, "foo7", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo7", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1)]), (54 + 55 + 62 + 63) / 4);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 1)]), (20 + 21 + 28 + 29) / 4);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeInt(program, 1), makeInt(program, 0)]), (55 + 56 + 63 + 64) / 4);
    checkFloat(program, callFunction(program, "foo8", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeInt(program, 0), makeInt(program, 1)]), (62 + 63 + 70 + 71) / 4);
    checkFloat(program, callFunction(program, "foo9", [texture3D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0.5)]), (12 + 13 + 20 + 21 + 44 + 45 + 52 + 53) / 8);
    checkFloat(program, callFunction(program, "foo9", [texture3D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 10 / 16), makeFloat(program, 0.5), makeFloat(program, 0.5)]), (13 + 14 + 21 + 22 + 45 + 46 + 53 + 54) / 8);
    checkFloat(program, callFunction(program, "foo9", [texture3D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 6 / 8), makeFloat(program, 0.5)]), (20 + 21 + 28 + 29 + 52 + 53 + 60 + 61) / 8);
    checkFloat(program, callFunction(program, "foo9", [texture3D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 5 / 8)]), (12 + 13 + 20 + 21 + (44 + 45 + 52 + 53) * 3) / 16);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0.5), makeInt(program, 1), makeInt(program, 0), makeInt(program, 0)]), (13 + 14 + 21 + 22 + 45 + 46 + 53 + 54) / 8);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0.5), makeInt(program, 0), makeInt(program, 1), makeInt(program, 0)]), (20 + 21 + 28 + 29 + 52 + 53 + 60 + 61) / 8);
    checkFloat(program, callFunction(program, "foo10", [texture3D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0.25), makeInt(program, 0), makeInt(program, 0), makeInt(program, 1)]), (44 + 45 + 52 + 53) / 4);
    checkFloat(program, callFunction(program, "foo11", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0)]), (3 + 4 + 5 + 6) / 4);
    checkFloat(program, callFunction(program, "foo11", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 0)]), (13 + 14 + 15 + 16) / 4);
    checkFloat(program, callFunction(program, "foo11", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 0)]), (23 + 24 + 25 + 26) / 4);
    checkFloat(program, callFunction(program, "foo11", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, 0)]), (33 + 34 + 35 + 36) / 4);
    checkFloat(program, callFunction(program, "foo11", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1)]), (43 + 44 + 45 + 46) / 4);
    checkFloat(program, callFunction(program, "foo11", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, -1)]), (53 + 54 + 55 + 56) / 4);
    // Unfortunately, we can't actually test the "Bias" argument, because it is supposed to be applied
    // to the unclamped LOD, which for us is -Infinity (because our derivatives are all 0)
    checkFloat(program, callFunction(program, "foo12", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo12", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 10 / 16), makeFloat(program, 0.5), makeFloat(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo12", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 6 / 8), makeFloat(program, 0)]), (20 + 21 + 28 + 29) / 4);
    checkFloat(program, callFunction(program, "foo13", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo13", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 1)]), (20 + 21 + 28 + 29) / 4);
    checkFloat(program, callFunction(program, "foo14", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo14", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1 / texture2D.ePtr.loadValue().height)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo14", [texture2D, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 2 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 2 / texture2D.ePtr.loadValue().height)]), (34 + 35 + 38 + 39) / 4);
    checkFloat(program, callFunction(program, "foo14", [texture2D, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().height)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo14", [texture2D, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().height)]), (((12 + 13 + 20 + 21) / 4) + ((34 + 35 + 38 + 39) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo15", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo15", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo15", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1 / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo15", [texture2D, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 2 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 2 / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (35 + 36 + 39 + 40) / 4);
    checkFloat(program, callFunction(program, "foo15", [texture2D, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo15", [texture2D, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (((13 + 14 + 21 + 22) / 4) + ((35 + 36 + 39 + 40) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo16", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo16", [texture2D, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1)]), (34 + 35 + 38 + 39) / 4);
    checkFloat(program, callFunction(program, "foo16", [texture2D, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0.5)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo16", [texture2D, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0.5)]), (((12 + 13 + 20 + 21) / 4) + ((34 + 35 + 38 + 39) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo17", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo17", [texture2D, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeInt(program, 0), makeInt(program, 0)]), (34 + 35 + 38 + 39) / 4);
    checkFloat(program, callFunction(program, "foo17", [texture2D, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0.5), makeInt(program, 0), makeInt(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo17", [texture2D, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0.5), makeInt(program, 0), makeInt(program, 0)]), (((12 + 13 + 20 + 21) / 4) + ((34 + 35 + 38 + 39) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo17", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo17", [texture2D, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeInt(program, 1), makeInt(program, 0)]), (35 + 36 + 39 + 40) / 4);
    checkFloat(program, callFunction(program, "foo17", [texture2D, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0.5), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo17", [texture2D, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0.5), makeInt(program, 1), makeInt(program, 0)]), (((13 + 14 + 21 + 22) / 4) + ((35 + 36 + 39 + 40) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo18", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo18", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 10 / 16), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo18", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 6 / 8), makeFloat(program, 0), makeFloat(program, 0)]), (20 + 21 + 28 + 29) / 4);
    checkFloat(program, callFunction(program, "foo18", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0)]), (54 + 55 + 62 + 63) / 4);
    checkFloat(program, callFunction(program, "foo18", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 10 / 16), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0)]), (55 + 56 + 63 + 64) / 4);
    checkFloat(program, callFunction(program, "foo18", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 6 / 8), makeFloat(program, 1), makeFloat(program, 0)]), (62 + 63 + 70 + 71) / 4);
    checkFloat(program, callFunction(program, "foo19", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo19", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 1)]), (20 + 21 + 28 + 29) / 4);
    checkFloat(program, callFunction(program, "foo19", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), (55 + 56 + 63 + 64) / 4);
    checkFloat(program, callFunction(program, "foo19", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 1)]), (62 + 63 + 70 + 71) / 4);
    checkFloat(program, callFunction(program, "foo20", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo20", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 1 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1 / texture2D.ePtr.loadValue().height)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo20", [texture2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 2 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 2 / texture2D.ePtr.loadValue().height)]), (34 + 35 + 38 + 39) / 4);
    checkFloat(program, callFunction(program, "foo20", [texture2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().height)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo20", [texture2DArray, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().height)]), (((12 + 13 + 20 + 21) / 4) + ((34 + 35 + 38 + 39) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo20", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (54 + 55 + 62 + 63) / 4);
    checkFloat(program, callFunction(program, "foo20", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 1 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1 / texture2D.ePtr.loadValue().height)]), (54 + 55 + 62 + 63) / 4);
    checkFloat(program, callFunction(program, "foo20", [texture2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 2 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 2 / texture2D.ePtr.loadValue().height)]), (76 + 77 + 80 + 81) / 4);
    checkFloat(program, callFunction(program, "foo20", [texture2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().height)]), (54 + 55 + 62 + 63) / 4);
    checkFloat(program, callFunction(program, "foo20", [texture2DArray, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().height)]), (((54 + 55 + 62 + 63) / 4) + ((76 + 77 + 80 + 81) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo21", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo21", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo21", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 1 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1 / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo21", [texture2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 2 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 2 / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (35 + 36 + 39 + 40) / 4);
    checkFloat(program, callFunction(program, "foo21", [texture2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo21", [texture2DArray, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (((13 + 14 + 21 + 22) / 4) + ((35 + 36 + 39 + 40) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo21", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 0)]), (54 + 55 + 62 + 63) / 4);
    checkFloat(program, callFunction(program, "foo21", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), (55 + 56 + 63 + 64) / 4);
    checkFloat(program, callFunction(program, "foo21", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 1 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1 / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (55 + 56 + 63 + 64) / 4);
    checkFloat(program, callFunction(program, "foo21", [texture2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 2 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 2 / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (77 + 78 + 81 + 82) / 4);
    checkFloat(program, callFunction(program, "foo21", [texture2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (55 + 56 + 63 + 64) / 4);
    checkFloat(program, callFunction(program, "foo21", [texture2DArray, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (((55 + 56 + 63 + 64) / 4) + ((77 + 78 + 81 + 82) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo22", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo22", [texture2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 1)]), (34 + 35 + 38 + 39) / 4);
    checkFloat(program, callFunction(program, "foo22", [texture2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0.5)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo22", [texture2DArray, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0.5)]), (((12 + 13 + 20 + 21) / 4) + ((34 + 35 + 38 + 39) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo22", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0)]), (54 + 55 + 62 + 63) / 4);
    checkFloat(program, callFunction(program, "foo22", [texture2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 1)]), (76 + 77 + 80 + 81) / 4);
    checkFloat(program, callFunction(program, "foo22", [texture2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0.5)]), (54 + 55 + 62 + 63) / 4);
    checkFloat(program, callFunction(program, "foo22", [texture2DArray, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0.5)]), (((54 + 55 + 62 + 63) / 4) + ((76 + 77 + 80 + 81) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo23", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo23", [texture2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 1), makeInt(program, 0), makeInt(program, 0)]), (34 + 35 + 38 + 39) / 4);
    checkFloat(program, callFunction(program, "foo23", [texture2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0.5), makeInt(program, 0), makeInt(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo23", [texture2DArray, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0.5), makeInt(program, 0), makeInt(program, 0)]), (((12 + 13 + 20 + 21) / 4) + ((34 + 35 + 38 + 39) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo23", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo23", [texture2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 1), makeInt(program, 1), makeInt(program, 0)]), (35 + 36 + 39 + 40) / 4);
    checkFloat(program, callFunction(program, "foo23", [texture2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0.5), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo23", [texture2DArray, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0.5), makeInt(program, 1), makeInt(program, 0)]), (((13 + 14 + 21 + 22) / 4) + ((35 + 36 + 39 + 40) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo23", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 0)]), (54 + 55 + 62 + 63) / 4);
    checkFloat(program, callFunction(program, "foo23", [texture2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 1), makeInt(program, 0), makeInt(program, 0)]), (76 + 77 + 80 + 81) / 4);
    checkFloat(program, callFunction(program, "foo23", [texture2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0.5), makeInt(program, 0), makeInt(program, 0)]), (54 + 55 + 62 + 63) / 4);
    checkFloat(program, callFunction(program, "foo23", [texture2DArray, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0.5), makeInt(program, 0), makeInt(program, 0)]), (((54 + 55 + 62 + 63) / 4) + ((76 + 77 + 80 + 81) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo23", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), (55 + 56 + 63 + 64) / 4);
    checkFloat(program, callFunction(program, "foo23", [texture2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 1), makeInt(program, 1), makeInt(program, 0)]), (77 + 78 + 81 + 82) / 4);
    checkFloat(program, callFunction(program, "foo23", [texture2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0.5), makeInt(program, 1), makeInt(program, 0)]), (55 + 56 + 63 + 64) / 4);
    checkFloat(program, callFunction(program, "foo23", [texture2DArray, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0.5), makeInt(program, 1), makeInt(program, 0)]), (((55 + 56 + 63 + 64) / 4) + ((77 + 78 + 81 + 82) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo16", [texture2D, makeSampler(program, {minFilter: "linear", lodMinClamp: 1, lodMaxClamp: 1}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 2)]), (34 + 35 + 38 + 39) / 4);
    checkFloat(program, callFunction(program, "foo16", [texture2D, makeSampler(program, {minFilter: "linear", lodMinClamp: 1, lodMaxClamp: 1}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0)]), (34 + 35 + 38 + 39) / 4);
    checkFloat(program, callFunction(program, "foo14", [texture2D, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 2 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1 / texture2D.ePtr.loadValue().height)]), (34 + 35 + 38 + 39) / 4);
    checkFloat(program, callFunction(program, "foo14", [texture2D, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear", maxAnisotropy: 2}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 4 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, (4/3) / texture2D.ePtr.loadValue().height)]), (34 + 35 + 38 + 39) / 4);
    let texture1DInt4 = make1DTexture(program, [[[1, 2, 3, 4], [100, 200, 300, 400], [101, 202, 301, 401], [13, 14, 15, 16]], [[17, 18, 19, 20], [21, 22, 23, 24]], [[25, 26, 27, 28]]], "int4");
    checkInt(program, callFunction(program, "foo24", [texture1DInt4, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5)]), 100);
    checkInt(program, callFunction(program, "foo25", [texture1DInt4, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5)]), 201);
    checkInt(program, callFunction(program, "foo26", [texture1DInt4, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5)]), 300);
    checkInt(program, callFunction(program, "foo27", [texture1DInt4, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5)]), 400);
    checkFloat(program, callFunction(program, "foo28", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo28", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 10 / 16), makeFloat(program, 0.5)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo28", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 6 / 8)]), (20 + 21 + 28 + 29) / 4);
    checkFloat(program, callFunction(program, "foo29", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo29", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeInt(program, 0), makeInt(program, 1)]), (20 + 21 + 28 + 29) / 4);
    checkFloat(program, callFunction(program, "foo30", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo30", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1)]), (54 + 55 + 62 + 63) / 4);
    checkFloat(program, callFunction(program, "foo31", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo31", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 1)]), (20 + 21 + 28 + 29) / 4);
    checkFloat(program, callFunction(program, "foo31", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeInt(program, 1), makeInt(program, 0)]), (55 + 56 + 63 + 64) / 4);
    checkFloat(program, callFunction(program, "foo31", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeInt(program, 0), makeInt(program, 1)]), (62 + 63 + 70 + 71) / 4);
    checkFloat(program, callFunction(program, "foo32", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0)]), (3 + 4 + 5 + 6) / 4);
    checkFloat(program, callFunction(program, "foo32", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 0)]), (13 + 14 + 15 + 16) / 4);
    checkFloat(program, callFunction(program, "foo32", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 0)]), (23 + 24 + 25 + 26) / 4);
    checkFloat(program, callFunction(program, "foo32", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, 0)]), (33 + 34 + 35 + 36) / 4);
    checkFloat(program, callFunction(program, "foo32", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1)]), (43 + 44 + 45 + 46) / 4);
    checkFloat(program, callFunction(program, "foo32", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, -1)]), (53 + 54 + 55 + 56) / 4);
    checkFloat(program, callFunction(program, "foo28", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo28", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 10 / 16), makeFloat(program, 0.5)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo28", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 6 / 8)]), (20 + 21 + 28 + 29) / 4);
    checkFloat(program, callFunction(program, "foo29", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo29", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeInt(program, 0), makeInt(program, 1)]), (20 + 21 + 28 + 29) / 4);
    checkFloat(program, callFunction(program, "foo30", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo30", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1)]), (54 + 55 + 62 + 63) / 4);
    checkFloat(program, callFunction(program, "foo31", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo31", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 1)]), (20 + 21 + 28 + 29) / 4);
    checkFloat(program, callFunction(program, "foo31", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeInt(program, 1), makeInt(program, 0)]), (55 + 56 + 63 + 64) / 4);
    checkFloat(program, callFunction(program, "foo31", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeInt(program, 0), makeInt(program, 1)]), (62 + 63 + 70 + 71) / 4);
    checkFloat(program, callFunction(program, "foo33", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo33", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 10 / 16), makeFloat(program, 0.5), makeFloat(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo33", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 6 / 8), makeFloat(program, 0)]), (20 + 21 + 28 + 29) / 4);
    checkFloat(program, callFunction(program, "foo34", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo34", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 1)]), (20 + 21 + 28 + 29) / 4);
    checkFloat(program, callFunction(program, "foo35", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo35", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1 / texture2D.ePtr.loadValue().height)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo35", [textureDepth2D, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 2 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 2 / texture2D.ePtr.loadValue().height)]), (34 + 35 + 38 + 39) / 4);
    checkFloat(program, callFunction(program, "foo35", [textureDepth2D, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().height)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo35", [textureDepth2D, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().height)]), (((12 + 13 + 20 + 21) / 4) + ((34 + 35 + 38 + 39) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo36", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo36", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo36", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1 / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo36", [textureDepth2D, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 2 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 2 / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (35 + 36 + 39 + 40) / 4);
    checkFloat(program, callFunction(program, "foo36", [textureDepth2D, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo36", [textureDepth2D, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (((13 + 14 + 21 + 22) / 4) + ((35 + 36 + 39 + 40) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo37", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo37", [textureDepth2D, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1)]), (34 + 35 + 38 + 39) / 4);
    checkFloat(program, callFunction(program, "foo37", [textureDepth2D, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0.5)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo37", [textureDepth2D, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0.5)]), (((12 + 13 + 20 + 21) / 4) + ((34 + 35 + 38 + 39) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo38", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo38", [textureDepth2D, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeInt(program, 0), makeInt(program, 0)]), (34 + 35 + 38 + 39) / 4);
    checkFloat(program, callFunction(program, "foo38", [textureDepth2D, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0.5), makeInt(program, 0), makeInt(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo38", [textureDepth2D, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0.5), makeInt(program, 0), makeInt(program, 0)]), (((12 + 13 + 20 + 21) / 4) + ((34 + 35 + 38 + 39) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo38", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo38", [textureDepth2D, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeInt(program, 1), makeInt(program, 0)]), (35 + 36 + 39 + 40) / 4);
    checkFloat(program, callFunction(program, "foo38", [textureDepth2D, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0.5), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo38", [textureDepth2D, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0.5), makeInt(program, 1), makeInt(program, 0)]), (((13 + 14 + 21 + 22) / 4) + ((35 + 36 + 39 + 40) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo39", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo39", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 10 / 16), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo39", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 6 / 8), makeFloat(program, 0), makeFloat(program, 0)]), (20 + 21 + 28 + 29) / 4);
    checkFloat(program, callFunction(program, "foo39", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0)]), (54 + 55 + 62 + 63) / 4);
    checkFloat(program, callFunction(program, "foo39", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 10 / 16), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0)]), (55 + 56 + 63 + 64) / 4);
    checkFloat(program, callFunction(program, "foo39", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 6 / 8), makeFloat(program, 1), makeFloat(program, 0)]), (62 + 63 + 70 + 71) / 4);
    checkFloat(program, callFunction(program, "foo40", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo40", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 1)]), (20 + 21 + 28 + 29) / 4);
    checkFloat(program, callFunction(program, "foo40", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), (55 + 56 + 63 + 64) / 4);
    checkFloat(program, callFunction(program, "foo40", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 1)]), (62 + 63 + 70 + 71) / 4);
    checkFloat(program, callFunction(program, "foo41", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo41", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 1 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1 / texture2D.ePtr.loadValue().height)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo41", [textureDepth2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 2 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 2 / texture2D.ePtr.loadValue().height)]), (34 + 35 + 38 + 39) / 4);
    checkFloat(program, callFunction(program, "foo41", [textureDepth2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().height)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo41", [textureDepth2DArray, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().height)]), (((12 + 13 + 20 + 21) / 4) + ((34 + 35 + 38 + 39) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo41", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (54 + 55 + 62 + 63) / 4);
    checkFloat(program, callFunction(program, "foo41", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 1 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1 / texture2D.ePtr.loadValue().height)]), (54 + 55 + 62 + 63) / 4);
    checkFloat(program, callFunction(program, "foo41", [textureDepth2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 2 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 2 / texture2D.ePtr.loadValue().height)]), (76 + 77 + 80 + 81) / 4);
    checkFloat(program, callFunction(program, "foo41", [textureDepth2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().height)]), (54 + 55 + 62 + 63) / 4);
    checkFloat(program, callFunction(program, "foo41", [textureDepth2DArray, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().height)]), (((54 + 55 + 62 + 63) / 4) + ((76 + 77 + 80 + 81) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo42", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo42", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo42", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 1 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1 / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo42", [textureDepth2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 2 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 2 / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (35 + 36 + 39 + 40) / 4);
    checkFloat(program, callFunction(program, "foo42", [textureDepth2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo42", [textureDepth2DArray, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (((13 + 14 + 21 + 22) / 4) + ((35 + 36 + 39 + 40) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo42", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 0)]), (54 + 55 + 62 + 63) / 4);
    checkFloat(program, callFunction(program, "foo42", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), (55 + 56 + 63 + 64) / 4);
    checkFloat(program, callFunction(program, "foo42", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 1 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1 / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (55 + 56 + 63 + 64) / 4);
    checkFloat(program, callFunction(program, "foo42", [textureDepth2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 2 / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 2 / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (77 + 78 + 81 + 82) / 4);
    checkFloat(program, callFunction(program, "foo42", [textureDepth2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.25) / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (55 + 56 + 63 + 64) / 4);
    checkFloat(program, callFunction(program, "foo42", [textureDepth2DArray, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeCastedFloat(program, Math.pow(2, 0.5) / texture2D.ePtr.loadValue().height), makeInt(program, 1), makeInt(program, 0)]), (((55 + 56 + 63 + 64) / 4) + ((77 + 78 + 81 + 82) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo43", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo43", [textureDepth2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 1)]), (34 + 35 + 38 + 39) / 4);
    checkFloat(program, callFunction(program, "foo43", [textureDepth2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0.5)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo43", [textureDepth2DArray, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0.5)]), (((12 + 13 + 20 + 21) / 4) + ((34 + 35 + 38 + 39) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo43", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0)]), (54 + 55 + 62 + 63) / 4);
    checkFloat(program, callFunction(program, "foo43", [textureDepth2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 1)]), (76 + 77 + 80 + 81) / 4);
    checkFloat(program, callFunction(program, "foo43", [textureDepth2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0.5)]), (54 + 55 + 62 + 63) / 4);
    checkFloat(program, callFunction(program, "foo43", [textureDepth2DArray, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0.5)]), (((54 + 55 + 62 + 63) / 4) + ((76 + 77 + 80 + 81) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo44", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo44", [textureDepth2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 1), makeInt(program, 0), makeInt(program, 0)]), (34 + 35 + 38 + 39) / 4);
    checkFloat(program, callFunction(program, "foo44", [textureDepth2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0.5), makeInt(program, 0), makeInt(program, 0)]), (12 + 13 + 20 + 21) / 4);
    checkFloat(program, callFunction(program, "foo44", [textureDepth2DArray, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0.5), makeInt(program, 0), makeInt(program, 0)]), (((12 + 13 + 20 + 21) / 4) + ((34 + 35 + 38 + 39) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo44", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo44", [textureDepth2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 1), makeInt(program, 1), makeInt(program, 0)]), (35 + 36 + 39 + 40) / 4);
    checkFloat(program, callFunction(program, "foo44", [textureDepth2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0.5), makeInt(program, 1), makeInt(program, 0)]), (13 + 14 + 21 + 22) / 4);
    checkFloat(program, callFunction(program, "foo44", [textureDepth2DArray, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 0.5), makeInt(program, 1), makeInt(program, 0)]), (((13 + 14 + 21 + 22) / 4) + ((35 + 36 + 39 + 40) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo44", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 0)]), (54 + 55 + 62 + 63) / 4);
    checkFloat(program, callFunction(program, "foo44", [textureDepth2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 1), makeInt(program, 0), makeInt(program, 0)]), (76 + 77 + 80 + 81) / 4);
    checkFloat(program, callFunction(program, "foo44", [textureDepth2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0.5), makeInt(program, 0), makeInt(program, 0)]), (54 + 55 + 62 + 63) / 4);
    checkFloat(program, callFunction(program, "foo44", [textureDepth2DArray, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0.5), makeInt(program, 0), makeInt(program, 0)]), (((54 + 55 + 62 + 63) / 4) + ((76 + 77 + 80 + 81) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo44", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), (55 + 56 + 63 + 64) / 4);
    checkFloat(program, callFunction(program, "foo44", [textureDepth2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 1), makeInt(program, 1), makeInt(program, 0)]), (77 + 78 + 81 + 82) / 4);
    checkFloat(program, callFunction(program, "foo44", [textureDepth2DArray, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0.5), makeInt(program, 1), makeInt(program, 0)]), (55 + 56 + 63 + 64) / 4);
    checkFloat(program, callFunction(program, "foo44", [textureDepth2DArray, makeSampler(program, {minFilter: "linear", mipmapFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 0.5), makeInt(program, 1), makeInt(program, 0)]), (((55 + 56 + 63 + 64) / 4) + ((77 + 78 + 81 + 82) / 4)) / 2);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "never"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 11)]), 0);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "never"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 17)]), 0);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "never"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 23)]), 0);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "less"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 11)]), 1);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "less"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 17)]), 0);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "less"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 23)]), 0);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "equal"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 11)]), 0);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "equal"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 17)]), 1);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "equal"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 23)]), 0);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "lessEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 11)]), 1);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "lessEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 17)]), 1);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "lessEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 23)]), 0);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "greater"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 11)]), 0);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "greater"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 17)]), 0);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "greater"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 23)]), 1);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "notEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 11)]), 1);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "notEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 17)]), 0);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "notEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 23)]), 1);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "greaterEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 11)]), 0);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "greaterEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 17)]), 1);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "greaterEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 23)]), 1);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "always"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 11)]), 1);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "always"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 17)]), 1);
    checkFloat(program, callFunction(program, "foo45", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "always"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 23)]), 1);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "never"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 11)]), 0);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "never"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 17)]), 0);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "never"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 23)]), 0);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "less"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 11)]), 1);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "less"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 17)]), 0);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "less"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 23)]), 0);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "equal"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 11)]), 0);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "equal"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 17)]), 1);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "equal"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 23)]), 0);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "lessEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 11)]), 1);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "lessEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 17)]), 1);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "lessEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 23)]), 0);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "greater"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 11)]), 0);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "greater"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 17)]), 0);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "greater"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 23)]), 1);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "notEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 11)]), 1);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "notEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 17)]), 0);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "notEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 23)]), 1);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "greaterEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 11)]), 0);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "greaterEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 17)]), 1);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "greaterEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 23)]), 1);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "always"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 11)]), 1);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "always"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 17)]), 1);
    checkFloat(program, callFunction(program, "foo47", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "always"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 23)]), 1);
    checkFloat(program, callFunction(program, "foo49", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (3 + 4 + 5 + 6) / 4);
    checkFloat(program, callFunction(program, "foo49", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (13 + 14 + 15 + 16) / 4);
    checkFloat(program, callFunction(program, "foo49", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0)]), (23 + 24 + 25 + 26) / 4);
    checkFloat(program, callFunction(program, "foo49", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 0)]), (33 + 34 + 35 + 36) / 4);
    checkFloat(program, callFunction(program, "foo49", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 0)]), (43 + 44 + 45 + 46) / 4);
    checkFloat(program, callFunction(program, "foo49", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, 0)]), (53 + 54 + 55 + 56) / 4);
    checkFloat(program, callFunction(program, "foo50", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (3 + 4 + 5 + 6) / 4);
    checkFloat(program, callFunction(program, "foo50", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (13 + 14 + 15 + 16) / 4);
    checkFloat(program, callFunction(program, "foo50", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (23 + 24 + 25 + 26) / 4);
    checkFloat(program, callFunction(program, "foo50", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (33 + 34 + 35 + 36) / 4);
    checkFloat(program, callFunction(program, "foo50", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (43 + 44 + 45 + 46) / 4);
    checkFloat(program, callFunction(program, "foo50", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (53 + 54 + 55 + 56) / 4);
    checkFloat(program, callFunction(program, "foo50", [textureCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, -4 / textureCube.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, -4 / textureCube.ePtr.loadValue().height), makeFloat(program, 0)]), (9 + 10) / 2);
    checkFloat(program, callFunction(program, "foo50", [textureCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0, 0, 4 / textureCube.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, -4 / textureCube.ePtr.loadValue().height), makeFloat(program, 0)]), (19 + 20) / 2);
    checkFloat(program, callFunction(program, "foo50", [textureCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 4 / textureCube.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 4 / textureCube.ePtr.loadValue().height)]), (29 + 30) / 2);
    checkFloat(program, callFunction(program, "foo50", [textureCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 4 / textureCube.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, -4 / textureCube.ePtr.loadValue().height)]), (39 + 40) / 2);
    checkFloat(program, callFunction(program, "foo50", [textureCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 4 / textureCube.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, -4 / textureCube.ePtr.loadValue().height), makeFloat(program, 0)]), (49 + 50) / 2);
    checkFloat(program, callFunction(program, "foo50", [textureCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, -4 / textureCube.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, -4 / textureCube.ePtr.loadValue().height), makeFloat(program, 0)]), (59 + 60) / 2);
    checkFloat(program, callFunction(program, "foo51", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (3 + 4 + 5 + 6) / 4);
    checkFloat(program, callFunction(program, "foo51", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (13 + 14 + 15 + 16) / 4);
    checkFloat(program, callFunction(program, "foo51", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0)]), (23 + 24 + 25 + 26) / 4);
    checkFloat(program, callFunction(program, "foo51", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 0)]), (33 + 34 + 35 + 36) / 4);
    checkFloat(program, callFunction(program, "foo51", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 0)]), (43 + 44 + 45 + 46) / 4);
    checkFloat(program, callFunction(program, "foo51", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, 0)]), (53 + 54 + 55 + 56) / 4);
    checkFloat(program, callFunction(program, "foo51", [textureCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1)]), (9 + 10) / 2);
    checkFloat(program, callFunction(program, "foo51", [textureCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1)]), (19 + 20) / 2);
    checkFloat(program, callFunction(program, "foo51", [textureCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 1)]), (29 + 30) / 2);
    checkFloat(program, callFunction(program, "foo51", [textureCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 1)]), (39 + 40) / 2);
    checkFloat(program, callFunction(program, "foo51", [textureCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 1)]), (49 + 50) / 2);
    checkFloat(program, callFunction(program, "foo51", [textureCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, 1)]), (59 + 60) / 2);
    checkFloat(program, callFunction(program, "foo52", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (3 + 4 + 5 + 6) / 4);
    checkFloat(program, callFunction(program, "foo52", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (13 + 14 + 15 + 16) / 4);
    checkFloat(program, callFunction(program, "foo52", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0)]), (23 + 24 + 25 + 26) / 4);
    checkFloat(program, callFunction(program, "foo52", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 0)]), (33 + 34 + 35 + 36) / 4);
    checkFloat(program, callFunction(program, "foo52", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 0)]), (43 + 44 + 45 + 46) / 4);
    checkFloat(program, callFunction(program, "foo52", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, 0)]), (53 + 54 + 55 + 56) / 4);
    checkFloat(program, callFunction(program, "foo53", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (3 + 4 + 5 + 6) / 4);
    checkFloat(program, callFunction(program, "foo53", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (13 + 14 + 15 + 16) / 4);
    checkFloat(program, callFunction(program, "foo53", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (23 + 24 + 25 + 26) / 4);
    checkFloat(program, callFunction(program, "foo53", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (33 + 34 + 35 + 36) / 4);
    checkFloat(program, callFunction(program, "foo53", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (43 + 44 + 45 + 46) / 4);
    checkFloat(program, callFunction(program, "foo53", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (53 + 54 + 55 + 56) / 4);
    checkFloat(program, callFunction(program, "foo53", [textureDepthCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, -4 / textureCube.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, -4 / textureCube.ePtr.loadValue().height), makeFloat(program, 0)]), (9 + 10) / 2);
    checkFloat(program, callFunction(program, "foo53", [textureDepthCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0, 0, 4 / textureDepthCube.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, -4 / textureDepthCube.ePtr.loadValue().height), makeFloat(program, 0)]), (19 + 20) / 2);
    checkFloat(program, callFunction(program, "foo53", [textureDepthCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 4 / textureDepthCube.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 4 / textureDepthCube.ePtr.loadValue().height)]), (29 + 30) / 2);
    checkFloat(program, callFunction(program, "foo53", [textureDepthCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 4 / textureDepthCube.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, -4 / textureDepthCube.ePtr.loadValue().height)]), (39 + 40) / 2);
    checkFloat(program, callFunction(program, "foo53", [textureDepthCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 4 / textureDepthCube.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, -4 / textureDepthCube.ePtr.loadValue().height), makeFloat(program, 0)]), (49 + 50) / 2);
    checkFloat(program, callFunction(program, "foo53", [textureDepthCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, -4 / textureDepthCube.ePtr.loadValue().width), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, -4 / textureDepthCube.ePtr.loadValue().height), makeFloat(program, 0)]), (59 + 60) / 2);
    checkFloat(program, callFunction(program, "foo54", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (3 + 4 + 5 + 6) / 4);
    checkFloat(program, callFunction(program, "foo54", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 0)]), (13 + 14 + 15 + 16) / 4);
    checkFloat(program, callFunction(program, "foo54", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0)]), (23 + 24 + 25 + 26) / 4);
    checkFloat(program, callFunction(program, "foo54", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 0)]), (33 + 34 + 35 + 36) / 4);
    checkFloat(program, callFunction(program, "foo54", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 0)]), (43 + 44 + 45 + 46) / 4);
    checkFloat(program, callFunction(program, "foo54", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, 0)]), (53 + 54 + 55 + 56) / 4);
    checkFloat(program, callFunction(program, "foo54", [textureDepthCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1)]), (9 + 10) / 2);
    checkFloat(program, callFunction(program, "foo54", [textureDepthCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1)]), (19 + 20) / 2);
    checkFloat(program, callFunction(program, "foo54", [textureDepthCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 1)]), (29 + 30) / 2);
    checkFloat(program, callFunction(program, "foo54", [textureDepthCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 1)]), (39 + 40) / 2);
    checkFloat(program, callFunction(program, "foo54", [textureDepthCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 1)]), (49 + 50) / 2);
    checkFloat(program, callFunction(program, "foo54", [textureDepthCube, makeSampler(program, {minFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, 1)]), (59 + 60) / 2);

    
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "never"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 11)]), 0);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "never"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 17)]), 0);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "never"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 23)]), 0);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "less"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 11)]), 1);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "less"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 17)]), 0);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "less"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 23)]), 0);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "equal"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 11)]), 0);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "equal"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 17)]), 1);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "equal"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 23)]), 0);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "lessEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 11)]), 1);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "lessEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 17)]), 1);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "lessEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 23)]), 0);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "greater"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 11)]), 0);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "greater"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 17)]), 0);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "greater"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 23)]), 1);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "notEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 11)]), 1);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "notEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 17)]), 0);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "notEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 23)]), 1);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "greaterEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 11)]), 0);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "greaterEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 17)]), 1);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "greaterEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 23)]), 1);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "always"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 11)]), 1);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "always"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 17)]), 1);
    checkFloat(program, callFunction(program, "foo55", [make2DDepthTexture(program, [[[17]]], "float"), makeSampler(program, {compareFunction: "always"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 23)]), 1);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "never"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 11)]), 0);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "never"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 17)]), 0);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "never"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 23)]), 0);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "less"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 11)]), 1);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "less"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 17)]), 0);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "less"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 23)]), 0);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "equal"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 11)]), 0);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "equal"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 17)]), 1);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "equal"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 23)]), 0);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "lessEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 11)]), 1);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "lessEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 17)]), 1);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "lessEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 23)]), 0);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "greater"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 11)]), 0);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "greater"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 17)]), 0);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "greater"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 23)]), 1);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "notEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 11)]), 1);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "notEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 17)]), 0);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "notEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 23)]), 1);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "greaterEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 11)]), 0);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "greaterEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 17)]), 1);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "greaterEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 23)]), 1);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "always"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 11)]), 1);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "always"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 17)]), 1);
    checkFloat(program, callFunction(program, "foo57", [make2DDepthTextureArray(program, [[[[17]]]], "float"), makeSampler(program, {compareFunction: "always"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeFloat(program, 23)]), 1);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "never"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 - 1)]), 0);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "never"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4)]), 0);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "never"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 + 1)]), 0);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "less"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 - 1)]), 1);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "less"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4)]), 0);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "less"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 + 1)]), 0);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "equal"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 - 1)]), 0);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "equal"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4)]), 1);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "equal"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 + 1)]), 0);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "lessEqual"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 - 1)]), 1);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "lessEqual"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4)]), 1);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "lessEqual"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 + 1)]), 0);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "greater"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 - 1)]), 0);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "greater"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4)]), 0);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "greater"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 + 1)]), 1);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "notEqual"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 - 1)]), 1);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "notEqual"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4)]), 0);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "notEqual"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 + 1)]), 1);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "greaterEqual"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 - 1)]), 0);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "greaterEqual"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4)]), 1);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "greaterEqual"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 + 1)]), 1);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "always"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 - 1)]), 1);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "always"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4)]), 1);
    checkFloat(program, callFunction(program, "foo59", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "always"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 + 1)]), 1);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "never"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 - 1)]), 0);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "never"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4)]), 0);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "never"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 + 1)]), 0);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "less"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 - 1)]), 1);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "less"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4)]), 0);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "less"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 + 1)]), 0);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "equal"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 - 1)]), 0);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "equal"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4)]), 1);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "equal"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 + 1)]), 0);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "lessEqual"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 - 1)]), 1);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "lessEqual"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4)]), 1);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "lessEqual"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 + 1)]), 0);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "greater"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 - 1)]), 0);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "greater"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4)]), 0);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "greater"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 + 1)]), 1);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "notEqual"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 - 1)]), 1);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "notEqual"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4)]), 0);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "notEqual"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 + 1)]), 1);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "greaterEqual"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 - 1)]), 0);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "greaterEqual"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4)]), 1);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "greaterEqual"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 + 1)]), 1);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "always"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 - 1)]), 1);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "always"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4)]), 1);
    checkFloat(program, callFunction(program, "foo60", [textureDepthCube, makeSampler(program, {magFilter: "linear", compareFunction: "always"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, (3 + 4 + 5 + 6) / 4 + 1)]), 1);
}

tests.textureGather = function() {
    let program = doPrep(`
        test float4 foo1(Texture2D<float> texture, sampler s, float x, float y) {
            return Gather(texture, s, float2(x, y));
        }
        test float4 foo2(Texture2D<float> texture, sampler s, float x, float y, int offsetX, int offsetY) {
            return Gather(texture, s, float2(x, y), int2(offsetX, offsetY));
        }
        test float4 foo3(Texture2DArray<float> texture, sampler s, float x, float y, float layer) {
            return Gather(texture, s, float3(x, y, layer));
        }
        test float4 foo4(Texture2DArray<float> texture, sampler s, float x, float y, float layer, int offsetX, int offsetY) {
            return Gather(texture, s, float3(x, y, layer), int2(offsetX, offsetY));
        }
        test float4 foo5(TextureCube<float> texture, sampler s, float x, float y, float z) {
            return Gather(texture, s, float3(x, y, z));
        }
        test float4 foo6(TextureDepth2D<float> texture, sampler s, float x, float y) {
            return Gather(texture, s, float2(x, y));
        }
        test float4 foo7(TextureDepth2D<float> texture, sampler s, float x, float y, int offsetX, int offsetY) {
            return Gather(texture, s, float2(x, y), int2(offsetX, offsetY));
        }
        test float4 foo8(TextureDepth2DArray<float> texture, sampler s, float x, float y, float layer) {
            return Gather(texture, s, float3(x, y, layer));
        }
        test float4 foo9(TextureDepth2DArray<float> texture, sampler s, float x, float y, float layer, int offsetX, int offsetY) {
            return Gather(texture, s, float3(x, y, layer), int2(offsetX, offsetY));
        }
        test float4 foo10(TextureDepthCube<float> texture, sampler s, float x, float y, float z) {
            return Gather(texture, s, float3(x, y, z));
        }
        test float4 foo11(TextureDepth2D<float> texture, sampler s, float x, float y, float compareValue) {
            return GatherCmp(texture, s, float2(x, y), compareValue);
        }
        test float4 foo12(TextureDepth2D<float> texture, sampler s, float x, float y, float compareValue, int offsetX, int offsetY) {
            return GatherCmp(texture, s, float2(x, y), compareValue, int2(offsetX, offsetY));
        }
        test float4 foo13(TextureDepth2DArray<float> texture, sampler s, float x, float y, float layer, float compareValue) {
            return GatherCmp(texture, s, float3(x, y, layer), compareValue);
        }
        test float4 foo14(TextureDepth2DArray<float> texture, sampler s, float x, float y, float layer, float compareValue, int offsetX, int offsetY) {
            return GatherCmp(texture, s, float3(x, y, layer), compareValue, int2(offsetX, offsetY));
        }
    `);
    let [texture1D, texture1DArray, texture2D, texture2DArray, texture3D, textureCube, rwTexture1D, rwTexture1DArray, rwTexture2D, rwTexture2DArray, rwTexture3D, textureDepth2D, textureDepth2DArray, textureDepthCube, rwTextureDepth2D, rwTextureDepth2DArray] = createTexturesForTesting(program);
    checkFloat4(program, callFunction(program, "foo1", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5)]), [20, 21, 13, 12]);
    checkFloat4(program, callFunction(program, "foo1", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 10 / 16), makeFloat(program, 0.5)]), [21, 22, 14, 13]);
    checkFloat4(program, callFunction(program, "foo1", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 6 / 8)]), [28, 29, 21, 20]);
    checkFloat4(program, callFunction(program, "foo2", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeInt(program, 1), makeInt(program, 0)]), [21, 22, 14, 13]);
    checkFloat4(program, callFunction(program, "foo2", [texture2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeInt(program, 0), makeInt(program, 1)]), [28, 29, 21, 20]);
    checkFloat4(program, callFunction(program, "foo3", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0)]), [20, 21, 13, 12]);
    checkFloat4(program, callFunction(program, "foo3", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1)]), [62, 63, 55, 54]);
    checkFloat4(program, callFunction(program, "foo4", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), [21, 22, 14, 13]);
    checkFloat4(program, callFunction(program, "foo4", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 1)]), [28, 29, 21, 20]);
    checkFloat4(program, callFunction(program, "foo4", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeInt(program, 1), makeInt(program, 0)]), [63, 64, 56, 55]);
    checkFloat4(program, callFunction(program, "foo4", [texture2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeInt(program, 0), makeInt(program, 1)]), [70, 71, 63, 62]);
    checkFloat4(program, callFunction(program, "foo5", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0)]), [5, 6, 4, 3]);
    checkFloat4(program, callFunction(program, "foo5", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 0)]), [15, 16, 14, 13]);
    checkFloat4(program, callFunction(program, "foo5", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 0)]), [25, 26, 24, 23]);
    checkFloat4(program, callFunction(program, "foo5", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, 0)]), [35, 36, 34, 33]);
    checkFloat4(program, callFunction(program, "foo5", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1)]), [45, 46, 44, 43]);
    checkFloat4(program, callFunction(program, "foo5", [textureCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, -1)]), [55, 56, 54, 53]);
    checkFloat4(program, callFunction(program, "foo6", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5)]), [20, 21, 13, 12]);
    checkFloat4(program, callFunction(program, "foo6", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 10 / 16), makeFloat(program, 0.5)]), [21, 22, 14, 13]);
    checkFloat4(program, callFunction(program, "foo6", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 6 / 8)]), [28, 29, 21, 20]);
    checkFloat4(program, callFunction(program, "foo7", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeInt(program, 1), makeInt(program, 0)]), [21, 22, 14, 13]);
    checkFloat4(program, callFunction(program, "foo7", [textureDepth2D, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeInt(program, 0), makeInt(program, 1)]), [28, 29, 21, 20]);
    checkFloat4(program, callFunction(program, "foo8", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0)]), [20, 21, 13, 12]);
    checkFloat4(program, callFunction(program, "foo8", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1)]), [62, 63, 55, 54]);
    checkFloat4(program, callFunction(program, "foo9", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeInt(program, 1), makeInt(program, 0)]), [21, 22, 14, 13]);
    checkFloat4(program, callFunction(program, "foo9", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 0), makeInt(program, 0), makeInt(program, 1)]), [28, 29, 21, 20]);
    checkFloat4(program, callFunction(program, "foo9", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeInt(program, 1), makeInt(program, 0)]), [63, 64, 56, 55]);
    checkFloat4(program, callFunction(program, "foo9", [textureDepth2DArray, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeInt(program, 0), makeInt(program, 1)]), [70, 71, 63, 62]);
    checkFloat4(program, callFunction(program, "foo10", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 1), makeFloat(program, 0), makeFloat(program, 0)]), [5, 6, 4, 3]);
    checkFloat4(program, callFunction(program, "foo10", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, -1), makeFloat(program, 0), makeFloat(program, 0)]), [15, 16, 14, 13]);
    checkFloat4(program, callFunction(program, "foo10", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 1), makeFloat(program, 0)]), [25, 26, 24, 23]);
    checkFloat4(program, callFunction(program, "foo10", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, -1), makeFloat(program, 0)]), [35, 36, 34, 33]);
    checkFloat4(program, callFunction(program, "foo10", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, 1)]), [45, 46, 44, 43]);
    checkFloat4(program, callFunction(program, "foo10", [textureDepthCube, makeSampler(program, {magFilter: "linear"}), makeFloat(program, 0), makeFloat(program, 0), makeFloat(program, -1)]), [55, 56, 54, 53]);
    checkFloat4(program, callFunction(program, "foo11", [textureDepth2D, makeSampler(program, {magFilter: "linear", compareFunction: "never"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 13)]), [0, 0, 0, 0]);
    checkFloat4(program, callFunction(program, "foo11", [textureDepth2D, makeSampler(program, {magFilter: "linear", compareFunction: "less"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 13)]), [1, 1, 0, 0]);
    checkFloat4(program, callFunction(program, "foo11", [textureDepth2D, makeSampler(program, {magFilter: "linear", compareFunction: "equal"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 13)]), [0, 0, 1, 0]);
    checkFloat4(program, callFunction(program, "foo11", [textureDepth2D, makeSampler(program, {magFilter: "linear", compareFunction: "lessEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 13)]), [1, 1, 1, 0]);
    checkFloat4(program, callFunction(program, "foo11", [textureDepth2D, makeSampler(program, {magFilter: "linear", compareFunction: "greater"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 13)]), [0, 0, 0, 1]);
    checkFloat4(program, callFunction(program, "foo11", [textureDepth2D, makeSampler(program, {magFilter: "linear", compareFunction: "notEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 13)]), [1, 1, 0, 1]);
    checkFloat4(program, callFunction(program, "foo11", [textureDepth2D, makeSampler(program, {magFilter: "linear", compareFunction: "greaterEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 13)]), [0, 0, 1, 1]);
    checkFloat4(program, callFunction(program, "foo11", [textureDepth2D, makeSampler(program, {magFilter: "linear", compareFunction: "always"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 13)]), [1, 1, 1, 1]);
    checkFloat4(program, callFunction(program, "foo12", [textureDepth2D, makeSampler(program, {magFilter: "linear", compareFunction: "never"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 14), makeInt(program, 1), makeInt(program, 0)]), [0, 0, 0, 0]);
    checkFloat4(program, callFunction(program, "foo12", [textureDepth2D, makeSampler(program, {magFilter: "linear", compareFunction: "less"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 14), makeInt(program, 1), makeInt(program, 0)]), [1, 1, 0, 0]);
    checkFloat4(program, callFunction(program, "foo12", [textureDepth2D, makeSampler(program, {magFilter: "linear", compareFunction: "equal"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 14), makeInt(program, 1), makeInt(program, 0)]), [0, 0, 1, 0]);
    checkFloat4(program, callFunction(program, "foo12", [textureDepth2D, makeSampler(program, {magFilter: "linear", compareFunction: "lessEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 14), makeInt(program, 1), makeInt(program, 0)]), [1, 1, 1, 0]);
    checkFloat4(program, callFunction(program, "foo12", [textureDepth2D, makeSampler(program, {magFilter: "linear", compareFunction: "greater"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 14), makeInt(program, 1), makeInt(program, 0)]), [0, 0, 0, 1]);
    checkFloat4(program, callFunction(program, "foo12", [textureDepth2D, makeSampler(program, {magFilter: "linear", compareFunction: "notEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 14), makeInt(program, 1), makeInt(program, 0)]), [1, 1, 0, 1]);
    checkFloat4(program, callFunction(program, "foo12", [textureDepth2D, makeSampler(program, {magFilter: "linear", compareFunction: "greaterEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 14), makeInt(program, 1), makeInt(program, 0)]), [0, 0, 1, 1]);
    checkFloat4(program, callFunction(program, "foo12", [textureDepth2D, makeSampler(program, {magFilter: "linear", compareFunction: "always"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 14), makeInt(program, 1), makeInt(program, 0)]), [1, 1, 1, 1]);
    checkFloat4(program, callFunction(program, "foo13", [textureDepth2DArray, makeSampler(program, {magFilter: "linear", compareFunction: "never"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 55)]), [0, 0, 0, 0]);
    checkFloat4(program, callFunction(program, "foo13", [textureDepth2DArray, makeSampler(program, {magFilter: "linear", compareFunction: "less"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 55)]), [1, 1, 0, 0]);
    checkFloat4(program, callFunction(program, "foo13", [textureDepth2DArray, makeSampler(program, {magFilter: "linear", compareFunction: "equal"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 55)]), [0, 0, 1, 0]);
    checkFloat4(program, callFunction(program, "foo13", [textureDepth2DArray, makeSampler(program, {magFilter: "linear", compareFunction: "lessEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 55)]), [1, 1, 1, 0]);
    checkFloat4(program, callFunction(program, "foo13", [textureDepth2DArray, makeSampler(program, {magFilter: "linear", compareFunction: "greater"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 55)]), [0, 0, 0, 1]);
    checkFloat4(program, callFunction(program, "foo13", [textureDepth2DArray, makeSampler(program, {magFilter: "linear", compareFunction: "notEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 55)]), [1, 1, 0, 1]);
    checkFloat4(program, callFunction(program, "foo13", [textureDepth2DArray, makeSampler(program, {magFilter: "linear", compareFunction: "greaterEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 55)]), [0, 0, 1, 1]);
    checkFloat4(program, callFunction(program, "foo13", [textureDepth2DArray, makeSampler(program, {magFilter: "linear", compareFunction: "always"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 55)]), [1, 1, 1, 1]);
    checkFloat4(program, callFunction(program, "foo14", [textureDepth2DArray, makeSampler(program, {magFilter: "linear", compareFunction: "never"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 56), makeInt(program, 1), makeInt(program, 0)]), [0, 0, 0, 0]);
    checkFloat4(program, callFunction(program, "foo14", [textureDepth2DArray, makeSampler(program, {magFilter: "linear", compareFunction: "less"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 56), makeInt(program, 1), makeInt(program, 0)]), [1, 1, 0, 0]);
    checkFloat4(program, callFunction(program, "foo14", [textureDepth2DArray, makeSampler(program, {magFilter: "linear", compareFunction: "equal"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 56), makeInt(program, 1), makeInt(program, 0)]), [0, 0, 1, 0]);
    checkFloat4(program, callFunction(program, "foo14", [textureDepth2DArray, makeSampler(program, {magFilter: "linear", compareFunction: "lessEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 56), makeInt(program, 1), makeInt(program, 0)]), [1, 1, 1, 0]);
    checkFloat4(program, callFunction(program, "foo14", [textureDepth2DArray, makeSampler(program, {magFilter: "linear", compareFunction: "greater"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 56), makeInt(program, 1), makeInt(program, 0)]), [0, 0, 0, 1]);
    checkFloat4(program, callFunction(program, "foo14", [textureDepth2DArray, makeSampler(program, {magFilter: "linear", compareFunction: "notEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 56), makeInt(program, 1), makeInt(program, 0)]), [1, 1, 0, 1]);
    checkFloat4(program, callFunction(program, "foo14", [textureDepth2DArray, makeSampler(program, {magFilter: "linear", compareFunction: "greaterEqual"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 56), makeInt(program, 1), makeInt(program, 0)]), [0, 0, 1, 1]);
    checkFloat4(program, callFunction(program, "foo14", [textureDepth2DArray, makeSampler(program, {magFilter: "linear", compareFunction: "always"}), makeFloat(program, 0.5), makeFloat(program, 0.5), makeFloat(program, 1), makeFloat(program, 56), makeInt(program, 1), makeInt(program, 0)]), [1, 1, 1, 1]);
    // FIXME: Gather other components
}

tests.referenceEquality = function() {
    let program = doPrep(`
        test bool foo1() {
            int x;
            thread int* y = &x;
            thread int* z = &x;
            return y == z;
        }
        test bool foo2() {
            int x;
            thread int* z = &x;
            return &x == z;
        }
        test bool foo3() {
            int x;
            thread int* y = &x;
            return y == &x;
        }
        test bool foo4() {
            int x;
            return &x == &x;
        }
        test bool foo5() {
            int x;
            return &x != &x;
        }
        test bool foo6() {
            int x = 7;
            int y = 7;
            return &x == &y;
        }
        test bool foo7() {
            return null == null;
        }
        test bool foo8() {
            int x;
            thread int* y = &x;
            thread int* z = &x;
            return &y == &z;
        }
        test bool foo9() {
            int x;
            thread int* y = &x;
            return &y == &y;
        }
        test bool foo10() {
            thread int* y;
            return null == y;
        }
        test bool foo11() {
            thread int* y;
            return y == null;
        }
        test bool foo12() {
            int x;
            thread int* y = &x;
            return null == y;
        }
        test bool foo13() {
            int x;
            thread int* y = &x;
            return y == null;
        }
        test bool foo14() {
            int x;
            thread int[] y = @x;
            thread int[] z = @x;
            return y == z;
        }
        test bool foo15() {
            int x;
            thread int[] z = @x;
            return @x == z;
        }
        test bool foo16() {
            int x;
            thread int[] y = @x;
            return y == @x;
        }
        test bool foo17() {
            int x;
            return @x == @x;
        }
        test bool foo18() {
            int x;
            return @x != @x;
        }
        test bool foo19() {
            int x = 7;
            int y = 7;
            return @x == @y;
        }
        test bool foo21() {
            int x;
            thread int[] y = @x;
            thread int[] z = @x;
            return @y == @z;
        }
        test bool foo22() {
            int x;
            thread int[] y = @x;
            return @y == @y;
        }
        test bool foo23() {
            thread int[] y;
            return null == y;
        }
        test bool foo24() {
            thread int[] y;
            return y == null;
        }
        test bool foo25() {
            int x;
            thread int[] y = @x;
            return null == y;
        }
        test bool foo26() {
            int x;
            thread int[] y = @x;
            return y == null;
        }
        test bool foo27() {
            int[3] x;
            thread int[] y = @x;
            thread int[] z = @x;
            return y == z;
        }
        test bool foo28() {
            int[3] x;
            thread int[] z = @x;
            return @x == z;
        }
        test bool foo29() {
            int[3] x;
            thread int[] y = @x;
            return y == @x;
        }
        test bool foo30() {
            int[3] x;
            return @x == @x;
        }
        test bool foo31() {
            int[3] x;
            return @x != @x;
        }
        test bool foo32() {
            int[3] x;
            x[0] = 7;
            x[1] = 8;
            x[2] = 9;
            int[3] y;
            y[0] = 7;
            y[1] = 8;
            y[2] = 9;
            return @x == @y;
        }
        test bool foo33() {
            int[3] x;
            thread int[] y = @x;
            thread int[] z = @x;
            return @y == @z;
        }
        test bool foo34() {
            int[3] x;
            thread int[] y = @x;
            return @y == @y;
        }
        test bool foo35() {
            thread int[] y;
            return null == y;
        }
        test bool foo36() {
            thread int[] y;
            return y == null;
        }
        test bool foo37() {
            int[3] x;
            thread int[] y = @x;
            return null == y;
        }
        test bool foo38() {
            int[3] x;
            thread int[] y = @x;
            return y == null;
        }
    `);
    checkBool(program, callFunction(program, "foo1", []), true);
    checkBool(program, callFunction(program, "foo2", []), true);
    checkBool(program, callFunction(program, "foo3", []), true);
    checkBool(program, callFunction(program, "foo4", []), true);
    checkBool(program, callFunction(program, "foo5", []), false);
    checkBool(program, callFunction(program, "foo6", []), false);
    checkBool(program, callFunction(program, "foo7", []), true);
    checkBool(program, callFunction(program, "foo8", []), false);
    checkBool(program, callFunction(program, "foo9", []), true);
    checkBool(program, callFunction(program, "foo10", []), true);
    checkBool(program, callFunction(program, "foo11", []), true);
    checkBool(program, callFunction(program, "foo12", []), false);
    checkBool(program, callFunction(program, "foo13", []), false);
    checkBool(program, callFunction(program, "foo14", []), true);
    checkBool(program, callFunction(program, "foo15", []), true);
    checkBool(program, callFunction(program, "foo16", []), true);
    checkBool(program, callFunction(program, "foo17", []), true);
    checkBool(program, callFunction(program, "foo18", []), false);
    checkBool(program, callFunction(program, "foo19", []), false);
    checkBool(program, callFunction(program, "foo21", []), false);
    checkBool(program, callFunction(program, "foo22", []), true);
    checkBool(program, callFunction(program, "foo23", []), true);
    checkBool(program, callFunction(program, "foo24", []), true);
    checkBool(program, callFunction(program, "foo25", []), false);
    checkBool(program, callFunction(program, "foo26", []), false);
    checkBool(program, callFunction(program, "foo27", []), true);
    checkBool(program, callFunction(program, "foo28", []), true);
    checkBool(program, callFunction(program, "foo29", []), true);
    checkBool(program, callFunction(program, "foo30", []), true);
    checkBool(program, callFunction(program, "foo31", []), false);
    checkBool(program, callFunction(program, "foo32", []), false);
    checkBool(program, callFunction(program, "foo33", []), false);
    checkBool(program, callFunction(program, "foo34", []), true);
    checkBool(program, callFunction(program, "foo35", []), true);
    checkBool(program, callFunction(program, "foo36", []), true);
    checkBool(program, callFunction(program, "foo37", []), false);
    checkBool(program, callFunction(program, "foo38", []), false);
    checkFail(
        () => doPrep(`
            void foo()
            {
                int x;
                float y;
                bool z = (&x == &y);
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            void foo()
            {
                int x;
                thread int* y = &x;
                bool z = (&x == &y);
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            void foo()
            {
                int x;
                float y;
                bool z = (@x == @y);
            }
        `),
        (e) => e instanceof WTypeError);
    checkFail(
        () => doPrep(`
            void foo()
            {
                int x;
                thread int[] y = @x;
                bool z = (@x == @y);
            }
        `),
        (e) => e instanceof WTypeError);
}

tests.standardLibraryDevicePointers = function() {
    let program = doPrep(`
        test float foo1() {
            float s;
            float c;
            sincos(0, &s, &c);
            return c;
        }
        test float foo2() {
            float s;
            float c;
            sincos(0, &s, &c);
            return s;
        }
        test float foo3() {
            thread float* s = null;
            float c;
            sincos(0, s, &c);
            return c;
        }
        test float foo4() {
            float s;
            thread float* c = null;
            sincos(0, &s, c);
            return s;
        }
    `);
    checkFloat(program, callFunction(program, "foo1", []), 1);
    checkFloat(program, callFunction(program, "foo2", []), 0);
    checkFloat(program, callFunction(program, "foo3", []), 1);
    checkFloat(program, callFunction(program, "foo4", []), 0);
}

tests.commentParsing = function() {
    let program = doPrep(`
        /* this comment
        runs over multiple lines */
        test bool foo() { return true; }
    `);
    checkBool(program, callFunction(program, "foo", []), true);

    checkFail(
        () => doPrep(`
        /* this comment
        runs over multiple lines
        bool foo() { return true; }
        `),
        (e) => e instanceof WLexicalError);
}

tests.callArgumentsAreCopiedImmediatelyAfterEvaluation = () => {
    let program = doPrep(`
        test int foo()
        {
            return *bar(5) + *bar(7);
        }

        thread int* bar(int value)
        {
            int x = value;
            return &x;
        }
    `);

    checkInt(program, callFunction(program, "foo", []), 12);
};

tests.evaluationOrderForArguments = () => {
    const program = doPrep(`
        test int foo() { return *bar(10) + *bar(20); }

        thread int* bar(int value)
        {
            int x = value;
            return &x;
        }

        test int baz() { return plus(bar(10), bar(20)); }

        int plus(thread int* x, thread int* y)
        {
            return *x + *y;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 30);
    checkInt(program, callFunction(program, "baz", []), 20);
}

tests.cannotCallAnotherEntryPoint = () => {
    checkFail(() => doPrep(`
        struct Foo { int x : attribute(0); }
        vertex Foo foo() { return bar(); }
        vertex Foo bar() { return Foo(); }
    `), e => e instanceof WTypeError && e.message.indexOf("it cannot be called from within an existing shader") !== -1);
}

tests.inliningDoesNotAffectNestedCalls = () => {
    const program = prepare("/internal/test", 0, `
        test int foo()
        {
            return plus(bar(), baz());
        }

        int plus(int a, int b)
        {
            return a + b;
        }

        int bar()
        {
            return 17;
        }

        int baz()
        {
            return 3;
        }
    `, true);
    checkInt(program, callFunction(program, "foo", []), 20)
}

tests.inliningDoesntProduceAliasingIssues = () => {
    const program = prepare("/internal/test", 0, `
        test int foo()
        {
            return bar(2);
        }

        int bar(int x)
        {
            int y = x * x;
            return baz(y);
        }

        int baz(int x)
        {
            int y = 4 * x;
            return x + y;
        }
    `, true);
    checkInt(program, callFunction(program, "foo", []), 20);
}

tests.returnReferenceToParameter = () => {
    let program = doPrep(`
        test int foo(bool condition)
        {
            return *bar(condition, 1, 2);
        }

        thread int* bar(bool condition, int a, int b)
        {
            return condition ? &a : &b;
        }
    `);

    checkInt(program, callFunction(program, "foo", [ makeBool(program, true) ]), 1);
    checkInt(program, callFunction(program, "foo", [ makeBool(program, false) ]), 2);
}

tests.returnReferenceToParameterWithDifferentFunctions = () => {
    let program = doPrep(`
        test int foo()
        {
            return *bar(10) + *baz(20);
        }

        thread int* bar(int x)
        {
            return &x;
        }

        thread int* baz(int y)
        {
            return &y;
        }
    `);

    checkInt(program, callFunction(program, "foo", []), 30);
}

tests.returnReferenceToSameParameter = () => {
    let program = doPrep(`
        test int foo()
        {
            return plus(bar(5), bar(7));
        }

        int plus(thread int* x, thread int* y)
        {
            return *x + *y;
        }

        thread int* bar(int x)
        {
            return &x;
        }
    `);

    checkInt(program, callFunction(program, "foo", []), 10);
}

tests.returnReferenceToLocalVariable = () => {
    let program = doPrep(`
        test int foo()
        {
            return *bar();
        }

        thread int* bar()
        {
            int a = 42;
            return &a;
        }
    `);

    checkInt(program, callFunction(program, "foo", []), 42);
}

tests.returnReferenceToLocalVariableWithNesting = () => {
    let program = doPrep(`
        test int foo()
        {
            return *bar() + *baz();
        }

        thread int* bar()
        {
            int a = 20;
            return &a;
        }

        thread int* baz()
        {
            int a = 22;
            return &a;
        }
    `);

    checkInt(program, callFunction(program, "foo", []), 42);
}

tests.convertPtrToArrayRef = () => {
    let program = doPrep(`
        test int foo()
        {
            return bar()[0];
        }

        thread int[] bar()
        {
            int x = 42;
            return @(&x);
        }
    `);

    checkInt(program, callFunction(program, "foo", []), 42);
}

tests.convertLocalVariableToArrayRef = () => {
    let program = doPrep(`
        test int foo()
        {
            return bar()[0];
        }

        thread int[] bar()
        {
            int x = 42;
            return @x;
        }
    `);

    checkInt(program, callFunction(program, "foo", []), 42);
}

tests.referenceTakenToLocalVariableInEntryPointShouldntMoveAnything = () => {
    let program = doPrep(`
        test int foo()
        {
            int a = 42;
            thread int* b = &a;
            return *b;
        }
    `);

    checkInt(program, callFunction(program, "foo", []), 42);
};

tests.passingArrayToFunction = function()
{
    let program = doPrep(`
        test int foo()
        {
            int[10] arr;
            for (uint i = 0; i < arr.length; i++)
                arr[i] = int(i) + 1;
            return sum(arr);
        }

        int sum(int[10] xs)
        {
            int t = 0;
            for (uint i = 0; i < xs.length; i++)
                t = t + xs[i];
            return t;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 55);
}

tests.returnAnArrayFromAFunction = function()
{
    let program = doPrep(`
        test int foo()
        {
            int[5] ys = bar();
            return ys[0] + ys[1] + ys[2] + ys[3] + ys[4];
        }

        int[5] bar()
        {
            int[5] xs;
            xs[0] = 1;
            xs[1] = 2;
            xs[2] = 3;
            xs[3] = 4;
            xs[4] = 5;
            return xs;
        }
    `);
}

tests.copyArray = function()
{
    let program = doPrep(`
        test int foo()
        {
            int[10] xs;
            for (uint i = 0; i < xs.length; i++)
                xs[i] = int(i) + 1;
            int[10] ys = xs;
            for (uint i = 0; i < xs.length; i++)
                xs[i] = 0;
            int sum = 0;
            for (uint i = 0; i < ys.length; i++)
                sum = sum + ys[i];
            return sum;
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 55);
}

tests.settingAnArrayInsideAStruct = function()
{
    let program = doPrep(`
        struct Foo {
            int[1] array;
        }
        test int foo()
        {
            Foo foo;
            thread Foo* bar = &foo;
            bar->array[0] = 21;
            return foo.array[0];
        }
    `);
    checkInt(program, callFunction(program, "foo", []), 21);
}

tests.trapBecauseOfIndexAcesss = () => {
    const program = doPrep(`
        test int foo()
        {
            int[5] array;
            array[6] = 42;
            return array[6];
        }
    `);
    checkTrap(program, () => callFunction(program, "foo", []), checkInt);
}

tests.trapTransitively = () => {
    const program = doPrep(`
        test int foo()
        {
            willTrapTransitively();
            return 42;
        }

        void willTrapTransitively()
        {
            doTrap();
        }

        void doTrap()
        {
            trap;
        }
    `);
    checkTrap(program, () => callFunction(program, "foo", []), checkInt);
};

okToTest = true;

let testFilter = /.*/; // run everything by default
let testExclusionFilter = /^DISABLED_/;
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

    let names = [];
    for (let s in tests)
        names.push(s);
    names.sort();
    for (let s of names) {
        if (s.match(testFilter)) {
            if (s.match(testExclusionFilter)) {
                print(`Skipping ${s} because it is disabled.`);
            } else {
                print("TEST: " + s + "...");
                yield;
                const testBefore = preciseTime();
                tests[s]();
                const testAfter = preciseTime();
                print(`    OK, took ${Math.round((testAfter - testBefore) * 1000)} ms`);
            }
        }
    }

    let after = preciseTime();

    print("Success!");
    print("That took " + (after - before) * 1000 + " ms.");
}

if (!this.window && !this.runningInCocoaHost) {
    Error.stackTraceLimit = Infinity;
    for (let _ of doTest(testFilter)) { }
}

