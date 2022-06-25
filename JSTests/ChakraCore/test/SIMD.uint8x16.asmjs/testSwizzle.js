//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var ui16 = stdlib.SIMD.Uint8x16;
    var ui16check = ui16.check;
    var ui16swizzle = ui16.swizzle;
    var ui16add = ui16.add;
    var ui16mul = ui16.mul;

    var globImportui16 = ui16check(imports.g1);

    var ui16g1 = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    var ui16g2 = ui16(256, 255, 128, 127, 0, 0, 1000, 1000, 5, 15, 3, 399, 299, 21, 45, 22);

    var loopCOUNT = 3;
    
    var i16 = stdlib.SIMD.Int8x16;
    var i16check = i16.check;
    var i16fu16 = i16.fromUint8x16Bits;    

    function testswizzleLocal() {
        var a = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var result = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = ui16swizzle(a, 0, 1, 4, 5, 8, 10, 11, 12, 4, 2, 1, 6, 2, 8, 14, 0);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i16check(i16fu16(result));
    }
    
    function testswizzleGlobal() {
        var result = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = ui16swizzle(ui16g1, 0, 1, 4, 5, 8, 10, 11, 12, 4, 2, 1, 6, 2, 8, 14, 0);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i16check(i16fu16(result));
    }

    function testswizzleGlobalImport() {
        var result = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = ui16swizzle(globImportui16, 0, 1, 4, 5, 8, 10, 11, 12, 4, 2, 1, 6, 2, 8, 14, 0);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i16check(i16fu16(result));
    }

    function testswizzleFunc() {
        var a = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var result = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = ui16swizzle(ui16add(a, ui16g1), 0, 1, 4, 5, 8, 10, 11, 12, 4, 2, 1, 6, 2, 8, 14, 0);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i16check(i16fu16(result));
    }

    return { testswizzleLocal: testswizzleLocal, testswizzleGlobal: testswizzleGlobal, testswizzleGlobalImport: testswizzleGlobalImport, testswizzleFunc: testswizzleFunc };
}

var m = asmModule(this, { g1: SIMD.Uint8x16(100, 10824, 1028, 82, 3883, 8, 2929, 1442, 52, 127, 128, 129, 129, 0, 88, 100234) });

var ret1 = SIMD.Uint8x16.fromInt8x16Bits(m.testswizzleLocal());
var ret2 = SIMD.Uint8x16.fromInt8x16Bits(m.testswizzleGlobal());
var ret3 = SIMD.Uint8x16.fromInt8x16Bits(m.testswizzleGlobalImport());
var ret4 = SIMD.Uint8x16.fromInt8x16Bits(m.testswizzleFunc());


equalSimd([1, 2, 5, 6, 9, 11, 12, 13, 5, 3, 2, 7, 3, 9, 15, 1], ret1, SIMD.Uint8x16, "");
equalSimd([1, 2, 5, 6, 9, 11, 12, 13, 5, 3, 2, 7, 3, 9, 15, 1], ret2, SIMD.Uint8x16, "");
equalSimd([100, 72, 43, 8, 52, 128, 129, 129, 43, 4, 72, 113, 4, 52, 88, 100], ret3, SIMD.Uint8x16, "");
equalSimd([2, 4, 10, 12, 18, 22, 24, 26, 10, 6, 4, 14, 6, 18, 30, 2], ret4, SIMD.Uint8x16, "");

print("PASS");



