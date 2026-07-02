//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var u8 = stdlib.SIMD.Uint16x8;
    var u8check = u8.check;
    var u8swizzle = u8.swizzle;
    var u8add = u8.add;
    var u8mul = u8.mul;

    var globImportu8 = u8check(imports.g1);

    var u8g1 = u8(1, 2, 3, 4, 5, 6, 7, 8);

    var loopCOUNT = 3;

    var i8 = stdlib.SIMD.Int16x8
    var i8check = i8.check;
    var i8fu8 = i8.fromUint16x8Bits;

    function testswizzleLocal() {
        var a = u8(1, 2, 3, 4, 5, 6, 7, 8);
        var result = u8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = u8swizzle(a, 0, 1, 4, 5, 7, 4, 2, 3);
            loopIndex = (loopIndex + 1) | 0;
        }   
        return i8check(i8fu8(result));
    }

    function testswizzleGlobal() {
        var result = u8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = u8swizzle(u8g1, 0, 1, 4, 5, 7, 4, 2, 3);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i8check(i8fu8(result));
    }

    function testswizzleGlobalImport() {
        var result = u8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = u8swizzle(globImportu8, 0, 1, 4, 5, 7, 4, 2, 3);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i8check(i8fu8(result));
    }

    function testswizzleFunc() {
        var a = u8(1, 2, 3, 4, 5, 6, 7, 8);
        var result = u8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = u8swizzle(u8add(a, u8g1), 0, 1, 4, 5, 7, 4, 2, 3);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i8check(i8fu8(result));
    }

    return { testswizzleLocal: testswizzleLocal, testswizzleGlobal: testswizzleGlobal, testswizzleGlobalImport: testswizzleGlobalImport, testswizzleFunc: testswizzleFunc };
}

var m = asmModule(this, { g1: SIMD.Uint16x8(50, 1000, 3092, 3393, 8838, 63838, NaN, 838) });

var ret1 = SIMD.Uint16x8.fromInt16x8Bits(m.testswizzleLocal());
var ret2 = SIMD.Uint16x8.fromInt16x8Bits(m.testswizzleGlobal());
var ret3 = SIMD.Uint16x8.fromInt16x8Bits(m.testswizzleGlobalImport());
var ret4 = SIMD.Uint16x8.fromInt16x8Bits(m.testswizzleFunc());

equalSimd([1, 2, 5, 6, 8, 5, 3, 4], ret1, SIMD.Uint16x8, "");
equalSimd([1, 2, 5, 6, 8, 5, 3, 4], ret2, SIMD.Uint16x8, "");
equalSimd([50, 1000, 8838, 63838, 838, 8838, 3092, 3393], ret3, SIMD.Uint16x8, "");
equalSimd([2, 4, 10, 12, 16, 10, 6, 8], ret4, SIMD.Uint16x8, "");
print("PASS");



