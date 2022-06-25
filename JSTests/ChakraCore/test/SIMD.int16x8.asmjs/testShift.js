//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    var i8shiftLeftByScalar = i8.shiftLeftByScalar;
    var i8shiftRightByScalar = i8.shiftRightByScalar;

    var globImporti8 = i8check(imports.g1);    

    var i8g1 = i8(106, 0, -10, 1082, 3192, 32, 19, 5);                 

    var loopCOUNT = 8;

    function testShiftLeftScalarLocal()
    {
        var a = i8(5000, -3401, 665, -3224, -948, 2834, 7748, -25);
        var loopIndex = 0;

        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            a = i8shiftLeftByScalar(a, 1);
        }

        return i8check(a);
    }

    function testShiftRightScalarLocal()
    {
        var a = i8(5000, -3401, 665, -3224, -948, 2834, 7748, -25);
        var loopIndex = 0;

        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {
            a = i8shiftRightByScalar(a, 1);
        }

        return i8check(a);
    }

    function testShiftLeftScalarGlobal()
    {
        var loopIndex = 0;

        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {
            i8g1 = i8shiftLeftByScalar(i8g1, 1);
        }
        return i8check(i8g1);
    }

    function testShiftRightScalarGlobal()
    {
        var loopIndex = 0;

        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {
            i8g1 = i8shiftRightByScalar(i8g1, 1);
        }
        return i8check(i8g1);
    }

    function testShiftLeftScalarGlobalImport()
    {
        var loopIndex = 0;

        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {
            globImporti8 = i8shiftLeftByScalar(globImporti8, 1);
        }
        return i8check(globImporti8);
    }

    function testShiftRightScalarGlobalImport()
    {
        var loopIndex = 0;

        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {
            globImporti8 = i8shiftRightByScalar(globImporti8, 1);
        }
        return i8check(globImporti8);
    }
    function testShiftLeftScalarLocal1()
    {
        var a = i8(5000, -3401, 665, -3224, -948, 2834, 7748, -25);
        var loopIndex = 0;

        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            a = i8shiftLeftByScalar(a, 16);
            a = i8shiftLeftByScalar(a, 17);
        }

        return i8check(a);
    }

    function testShiftRightScalarLocal1()
    {
        var a = i8(5000, -3401, 665, -3224, -948, 2834, 7748, -25);
        var loopIndex = 0;

        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {
            a = i8shiftRightByScalar(a, 16);
            a = i8shiftRightByScalar(a, 17);
        }

        return i8check(a);
    }

    function testShiftLeftScalarGlobal1()
    {
        var loopIndex = 0;

        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {
            i8g1 = i8shiftLeftByScalar(i8g1, 16);
            i8g1 = i8shiftLeftByScalar(i8g1, 17);
        }
        return i8check(i8g1);
    }

    function testShiftRightScalarGlobal1()
    {
        var loopIndex = 0;

        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {
            i8g1 = i8shiftRightByScalar(i8g1, 16);
            i8g1 = i8shiftRightByScalar(i8g1, 17);
        }
        return i8check(i8g1);
    }

    function testShiftLeftScalarGlobalImport1()
    {
        var loopIndex = 0;

        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {
            globImporti8 = i8shiftLeftByScalar(globImporti8, 16);
            globImporti8 = i8shiftLeftByScalar(globImporti8, 17);
        }
        return i8check(globImporti8);
    }

    function testShiftRightScalarGlobalImport1()
    {
        var loopIndex = 0;

        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {
            globImporti8 = i8shiftRightByScalar(globImporti8, 16);
            globImporti8 = i8shiftRightByScalar(globImporti8, 17);
        }
        return i8check(globImporti8);
    }
    return {
        testShiftLeftScalarLocal: testShiftLeftScalarLocal, testShiftRightScalarLocal: testShiftRightScalarLocal, testShiftLeftScalarGlobal: testShiftLeftScalarGlobal,
        testShiftRightScalarGlobal: testShiftRightScalarGlobal, testShiftLeftScalarGlobalImport: testShiftLeftScalarGlobalImport, testShiftRightScalarGlobalImport: testShiftRightScalarGlobalImport,
        testShiftLeftScalarLocal1: testShiftLeftScalarLocal1, testShiftRightScalarLocal1: testShiftRightScalarLocal1, testShiftLeftScalarGlobal1: testShiftLeftScalarGlobal1,
        testShiftRightScalarGlobal1: testShiftRightScalarGlobal1, testShiftLeftScalarGlobalImport1: testShiftLeftScalarGlobalImport1, testShiftRightScalarGlobalImport1: testShiftRightScalarGlobalImport1
    };
}

var m = asmModule(this, {g1:SIMD.Int16x8(-50, 1000, 3092, -3393, 0, -39283838, 0, -838)});

equalSimd([-30720, -18688, -26368, 26624, 19456, 4608, 17408, -6400], m.testShiftLeftScalarLocal(), SIMD.Int16x8, "testMinLocal");
equalSimd([19, -14, 2, -13, -4, 11, 30, -1], m.testShiftRightScalarLocal(), SIMD.Int16x8, "testMaxLocal");
equalSimd([27136, 0, -2560, 14848, 30720, 8192, 4864, 1280], m.testShiftLeftScalarGlobal(), SIMD.Int16x8, "testMinGlobal");
equalSimd([106, 0, -10, 58, 120, 32, 19, 5], m.testShiftRightScalarGlobal(), SIMD.Int16x8, "testMaxGlobal");
equalSimd([-12800, -6144, 5120, -16640, 0, -32256, 0, -17920], m.testShiftLeftScalarGlobalImport(), SIMD.Int16x8, "testMinGlobalImport");
equalSimd([-50, -24, 20, -65, 0, -126, 0, -70], m.testShiftRightScalarGlobalImport(), SIMD.Int16x8, "testMaxGlobalImport");

equalSimd([-30720, -18688, -26368, 26624, 19456, 4608, 17408, -6400], m.testShiftLeftScalarLocal1(), SIMD.Int16x8, "testMinLocal1");
equalSimd([19, -14, 2, -13, -4, 11, 30, -1], m.testShiftRightScalarLocal1(), SIMD.Int16x8, "testMaxLocal1");
equalSimd([27136, 0, -2560, 14848, 30720, 8192, 4864, 1280], m.testShiftLeftScalarGlobal1(), SIMD.Int16x8, "testMinGlobal1");
equalSimd([106, 0, -10, 58, 120, 32, 19, 5], m.testShiftRightScalarGlobal(), SIMD.Int16x8, "testMaxGlobal1");
equalSimd([-12800, -6144, 5120, -16640, 0, -32256, 0, -17920], m.testShiftLeftScalarGlobalImport1(), SIMD.Int16x8, "testMinGlobalImport1");
equalSimd([-50, -24, 20, -65, 0, -126, 0, -70], m.testShiftRightScalarGlobalImport1(), SIMD.Int16x8, "testMaxGlobalImport1");
print("PASS");
