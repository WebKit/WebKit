//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    var i8swizzle = i8.swizzle;
    var i8add = i8.add;
    var i8mul = i8.mul;

    var globImporti8 = i8check(imports.g1);

    var i8g1 = i8(1, 2, 3, 4, 5, 6, 7, 8);

    var loopCOUNT = 3;

    function testswizzleLocal() {
        var a = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i8swizzle(a, 0, 1, 4, 5, 7, 4, 2, 3);
            loopIndex = (loopIndex + 1) | 0;
        }   
        return i8check(result);
    }

    function testswizzleGlobal() {
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i8swizzle(i8g1, 0, 1, 4, 5, 7, 4, 2, 3);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i8check(result);
    }

    function testswizzleGlobalImport() {
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i8swizzle(globImporti8, 0, 1, 4, 5, 7, 4, 2, 3);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i8check(result);
    }

    function testswizzleFunc() {
        var a = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i8swizzle(i8add(a, i8g1), 0, 1, 4, 5, 7, 4, 2, 3);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i8check(result);
    }

    return { testswizzleLocal: testswizzleLocal, testswizzleGlobal: testswizzleGlobal, testswizzleGlobalImport: testswizzleGlobalImport, testswizzleFunc: testswizzleFunc };
}

var m = asmModule(this, { g1: SIMD.Int16x8(-50, 1000, 3092, -3393, Infinity, -39283838, NaN, -838) });

equalSimd([1, 2, 5, 6, 8, 5, 3, 4], m.testswizzleLocal(), SIMD.Int16x8, "");
equalSimd([1, 2, 5, 6, 8, 5, 3, 4], m.testswizzleGlobal(), SIMD.Int16x8, "");
equalSimd([-50, 1000, 0, -27774, -838, 0, 3092, -3393], m.testswizzleGlobalImport(), SIMD.Int16x8, "");
equalSimd([2, 4, 10, 12, 16, 10, 6, 8], m.testswizzleFunc(), SIMD.Int16x8, "");
print("PASS");



