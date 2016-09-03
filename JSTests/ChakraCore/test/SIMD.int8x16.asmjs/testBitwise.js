//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var i16 = stdlib.SIMD.Int8x16;
    var i16check = i16.check;

    var i16and = i16.and;
    var i16or = i16.or;
    var i16xor = i16.xor;
    var i16not = i16.not;

    var globImporti16 = i16check(imports.g1);   

    var i16g1 = i16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    var i16g2 = i16(128, 128, -128, -128, 0, 0, 1000, -1000, 5, 15, -3, -399, 299, 21, 12, 12);

    var loopCOUNT = 3;

    function testBitwiseOR()
    {
        var a = i16(127, -128, 0, 127, -128, 0, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var b = i16(127, -128, 0, 0, 0, 127, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i16or(a, b);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }
    
    function testBitwiseAND()
    {
        var a = i16(127, -128, 0, 127, -128, 0, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var b = i16(127, -128, 0, 0, 0, 127, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i16and(a, b);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }

    function testBitwiseXOR()
    {
        var a = i16(127, -128, 0, 127, -128, 0, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var b = i16(127, -128, 0, 0, 0, 127, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i16xor(a, b);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }
    
    function testBitwiseNOT() {
        var a = i16(127, -128, 0, 127, -128, 0, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i16not(a);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }
    
    return {testBitwiseOR:testBitwiseOR, testBitwiseAND:testBitwiseAND, testBitwiseXOR:testBitwiseXOR, testBitwiseNOT:testBitwiseNOT};
}

var m = asmModule(this, { g1: SIMD.Int8x16(100, -1073741824, -1028, -102, NaN, -38, -92929, Infinity, 52, 127, -127, -129, 129, 0, 88, 100234) });

equalSimd([127, -128, 0, 127, -128, 127, -9, -1, -7, -5, -5, -1, -3, -1, -1, -1], m.testBitwiseOR(), SIMD.Int8x16, "testBitwiseOR");
equalSimd([127, -128, 0, 0, 0, 0, 6, 0, 8, 8, 10, 8, 12, 12, 14, 16], m.testBitwiseAND(), SIMD.Int8x16, "testBitwiseAND");
equalSimd([0, 0, 0, 127, -128, 127, -15, -1, -15, -13, -15, -9, -15, -13, -15, -17], m.testBitwiseXOR(), SIMD.Int8x16, "testBitwiseXOR");
equalSimd([-128, 127, -1, -128, 127, -1, -8, -9, -10, -11, -12, -13, -14, -15, -16, -17], m.testBitwiseNOT(), SIMD.Int8x16, "testBitwiseNOT");

print("PASS");
