//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var i16 = stdlib.SIMD.Int8x16;
    var i16check = i16.check;
    var i16swizzle = i16.swizzle;
    var i16add = i16.add;
    var i16mul = i16.mul;

    var globImporti16 = i16check(imports.g1);

    var i16g1 = i16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    var i16g2 = i16(128, 128, -128, -128, 0, 0, 1000, -1000, 5, 15, -3, -399, 299, 21, 12 ,12);

    var loopCOUNT = 3;

    function testswizzleLocal() {
        var a = i16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i16swizzle(a, 0, 1, 4, 5, 8, 10, 11, 12, 4, 2, 1, 6, 2, 8, 14, 0);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i16check(result);
    }

    function testswizzleGlobal() {
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i16swizzle(i16g1, 0, 1, 4, 5, 8, 10, 11, 12, 4, 2, 1, 6, 2, 8, 14, 0);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i16check(result);
    }

    function testswizzleGlobalImport() {
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i16swizzle(globImporti16, 0, 1, 4, 5, 8, 10, 11, 12, 4, 2, 1, 6, 2, 8, 14, 0);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i16check(result);
    }

    function testswizzleFunc() {
        var a = i16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i16swizzle(i16add(a, i16g1), 0, 1, 4, 5, 8, 10, 11, 12, 4, 2, 1, 6, 2, 8, 14, 0);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i16check(result);
    }

    return { testswizzleLocal: testswizzleLocal, testswizzleGlobal: testswizzleGlobal, testswizzleGlobalImport: testswizzleGlobalImport, testswizzleFunc: testswizzleFunc };
}

var m = asmModule(this, { g1: SIMD.Int8x16(100, -10, -128, -102, 83, -38, -9, 12, 52, 127, -127, -129, 129, 0, 88, 100234) });

equalSimd([1, 2, 5, 6, 9, 11, 12, 13, 5, 3, 2, 7, 3, 9, 15, 1], m.testswizzleLocal(), SIMD.Int8x16, "");
equalSimd([1, 2, 5, 6, 9, 11, 12, 13, 5, 3, 2, 7, 3, 9, 15, 1], m.testswizzleGlobal(), SIMD.Int8x16, "");
equalSimd([100, -10, 83, -38, 52, -127, 127, -127, 83, -128, -10, -9, -128, 52, 88, 100], m.testswizzleGlobalImport(), SIMD.Int8x16, "");
equalSimd([2, 4, 10, 12, 18, 22, 24, 26, 10, 6, 4, 14, 6, 18, 30, 2], m.testswizzleFunc(), SIMD.Int8x16, "");

print("PASS");



