//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    var i8mul = i8.mul;

    var globImporti8 = i8check(imports.g1);

    var i8g1 = i8(1, 2, 3, 4, -1, -2, -3, -4);
    var i8g2 = i8(10, 10, 10, 10, 10, 10, 10, 10);

    var loopCOUNT = 3;

    function testMulLocal()
    {
        var a = i8(5, 10, 15, 20, 25, 30, 35, 40);
        var b = i8(2, 2, 2, 2, 2, 2, 2, 2);
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i8mul(a, b);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(result);
    }
    function testMulGlobal()
    {    
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i8mul(i8g1, i8g2);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(result);
    }

    function testMulGlobalImport()
    {
        var a = i8(2, 2, 2, 2, 2, 2, 2, 2);
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i8mul(globImporti8, a);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(result);
    }
    
    return {testMulLocal:testMulLocal, testMulGlobal:testMulGlobal, testMulGlobalImport:testMulGlobalImport};
}

var m = asmModule(this, { g1: SIMD.Int16x8(-10, 10, -20, 20, -30, 30, -40, 40) });

equalSimd([10, 20, 30, 40, 50, 60, 70, 80], m.testMulLocal(), SIMD.Int16x8, "Test Neg");
equalSimd([10, 20, 30, 40, -10, -20, -30, -40], m.testMulGlobal(), SIMD.Int16x8, "Test Neg");
equalSimd([-20, 20, -40, 40, -60, 60, -80, 80], m.testMulGlobalImport(), SIMD.Int16x8, "Test Neg");
print("PASS");