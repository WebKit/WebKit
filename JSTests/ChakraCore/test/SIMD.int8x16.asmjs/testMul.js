//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var i16 = stdlib.SIMD.Int8x16;
    var i16check = i16.check;
    var i16mul = i16.mul;

    var globImporti16 = i16check(imports.g1);

    var i16g1 = i16(1, 2, 3, 4, 5, 6, 7, 8, -1, -2, -3, -4, -5, -6, -7, -8);
    var i16g2 = i16(10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10);

    var loopCOUNT = 3;

    function testMulLocal()
    {
        var a = i16(5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80);
        var b = i16(2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i16mul(a, b);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }
    function testMulGlobal()
    {    
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i16mul(i16g1, i16g2);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }

    function testMulGlobalImport()
    {
        var a = i16(2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i16mul(globImporti16, a);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }
    
    return {testMulLocal:testMulLocal, testMulGlobal:testMulGlobal, testMulGlobalImport:testMulGlobalImport};
}

var m = asmModule(this, { g1: SIMD.Int8x16(-10, 10, -20, 20, -30, 30, -40, 40, -50, 50, -60, 60, -70, 70, -80, 80) });

equalSimd([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, -126, -116, -106, -96], m.testMulLocal(), SIMD.Int8x16, "Test Neg");
equalSimd([10, 20, 30, 40, 50, 60, 70, 80, -10, -20, -30, -40, -50, -60, -70, -80], m.testMulGlobal(), SIMD.Int8x16, "Test Neg");
equalSimd([-20, 20, -40, 40, -60, 60, -80, 80, -100, 100, -120, 120, 116, -116, 96, -96], m.testMulGlobalImport(), SIMD.Int8x16, "Test Neg");
print("PASS");