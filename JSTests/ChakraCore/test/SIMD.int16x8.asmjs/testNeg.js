//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    var i8neg = i8.neg;

    var globImporti8 = i8check(imports.g1);

    var i8g1 = i8(1, 2, 3, 4, -1, -2, -3, -4);

    var loopCOUNT = 3;

    function testNegLocal()
    {
        var a = i8(5, -10, 15, -20, 25, -30, 35, -40);
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i8neg(a)
            loopIndex = (loopIndex + 1) | 0;
        }
        return i8check(result);
    }
    function testNegGlobal()
    {    
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i8neg(i8g1);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i8check(result);
    }

    function testNegGlobalImport()
    {
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i8neg(globImporti8);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i8check(result);
    }
    
    return {testNegLocal:testNegLocal, testNegGlobal:testNegGlobal, testNegGlobalImport:testNegGlobalImport};
}

var m = asmModule(this, { g1: SIMD.Int16x8(-10, 10, -20, 20, -30, 30, -40, 40) });

equalSimd([-5, 10, -15, 20, -25, 30, -35, 40], m.testNegLocal(), SIMD.Int16x8, "Test Neg");
equalSimd([-1, -2, -3, -4, 1, 2, 3, 4], m.testNegGlobal(), SIMD.Int16x8, "Test Neg");
equalSimd([10, -10, 20, -20, 30, -30, 40, -40], m.testNegGlobalImport(), SIMD.Int16x8, "Test Neg");
print("PASS");