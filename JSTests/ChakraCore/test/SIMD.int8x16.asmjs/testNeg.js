//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var i16 = stdlib.SIMD.Int8x16;
    var i16check = i16.check;
    var i16neg = i16.neg;

    var globImporti16 = i16check(imports.g1);

    var i16g1 = i16(1, 2, 3, 4, 5, 6, 7, 8, -1, -2, -3, -4, -5, -6, -7, -8);

    var loopCOUNT = 3;

    function testNegLocal()
    {
        var a = i16(127, -128, 128, -129, 0, 55, 5, 4, 3, 2, 1, -5, -4, -3, -2, -1);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i16neg(a)
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }
    function testNegGlobal()
    {    
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i16neg(i16g1);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }

    function testNegGlobalImport()
    {
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i16neg(globImporti16);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }
    
    return {testNegLocal:testNegLocal, testNegGlobal:testNegGlobal, testNegGlobalImport:testNegGlobalImport};
}

var m = asmModule(this, { g1: SIMD.Int8x16(100, -1073741824, -1028, -102, NaN, -38, -92929, Infinity, 52, 127, -127, -129, 129, 0, 88, 100234) });

equalSimd([-127, -128, -128, -127, 0, -55, -5, -4, -3, -2, -1, 5, 4, 3, 2, 1], m.testNegLocal(), SIMD.Int8x16, "Test Neg");
equalSimd([-1, -2, -3, -4, -5, -6, -7, -8, 1, 2, 3, 4, 5, 6, 7, 8], m.testNegGlobal(), SIMD.Int8x16, "Test Neg");
equalSimd([-100, 0, 4, 102, 0, 38, 1, 0, -52, -127, 127, -127, 127, 0, -88, 118], m.testNegGlobalImport(), SIMD.Int8x16, "Test Neg");
print("PASS");