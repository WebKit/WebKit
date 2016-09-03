//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var i16 = stdlib.SIMD.Int8x16;
    var i16check = i16.check;
    var i16shiftLeftByScalar = i16.shiftLeftByScalar;
    var i16shiftRightByScalar = i16.shiftRightByScalar;

    var globImporti16 = i16check(imports.g1);    

    var i16g1 = i16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    var i16g2 = i16(128, 128, -128, -128, 0, 0, 1000, -1000, 5, 15, -3, -399, 299, 21, 12 ,12);

    var loopCOUNT = 4;

    function testShiftLeftScalarLocal()
    {
        var a = i16(-40, 40, -40, 40, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            a = i16shiftLeftByScalar(a, 1);
        }

        return i16check(a);
    }
    
    function testShiftRightScalarLocal()
    {
        var a = i16(-40, 40, -40, 40, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {
            a = i16shiftRightByScalar(a, 1);
            
        }

        return i16check(a);
    }

    function testShiftLeftScalarGlobal()
    {
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {
            i16g1 = i16shiftLeftByScalar(i16g1, 1);
        }
        return i16check(i16g1);
    }

    function testShiftRightScalarGlobal()
    {
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {
            i16g1 = i16shiftRightByScalar(i16g1, 1);
        }
        return i16check(i16g1);
    }

    function testShiftLeftScalarGlobalImport()
    {
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {
            globImporti16 = i16shiftLeftByScalar(globImporti16, 1);
        }
        return i16check(globImporti16);
    }

    function testShiftRightScalarGlobalImport()
    {
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {
            globImporti16 = i16shiftRightByScalar(globImporti16, 1);
        }
        return i16check(globImporti16);
    }

    function testShiftLeftScalarLocal1()
    {
        var a = i16(-40, 40, -40, 40, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            a = i16shiftLeftByScalar(a, 8);
            a = i16shiftLeftByScalar(a, 9);
        }

        return i16check(a);
    }
    
    function testShiftRightScalarLocal1()
    {
        var a = i16(-40, 40, -40, 40, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {
            a = i16shiftRightByScalar(a, 8);
            a = i16shiftRightByScalar(a, 9);
        }

        return i16check(a);
    }

    function testShiftLeftScalarGlobal1()
    {
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {
            i16g1 = i16shiftLeftByScalar(i16g1, 8);
            i16g1 = i16shiftLeftByScalar(i16g1, 9);
        }
        return i16check(i16g1);
    }

    function testShiftRightScalarGlobal1()
    {
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {
            i16g1 = i16shiftRightByScalar(i16g1, 8);
            i16g1 = i16shiftRightByScalar(i16g1, 9);
        }
        return i16check(i16g1);
    }

    function testShiftLeftScalarGlobalImport1()
    {
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {
            globImporti16 = i16shiftLeftByScalar(globImporti16, 8);
            globImporti16 = i16shiftLeftByScalar(globImporti16, 9);
        }
        return i16check(globImporti16);
    }

    function testShiftRightScalarGlobalImport1()
    {
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {
            globImporti16 = i16shiftRightByScalar(globImporti16, 8);
            globImporti16 = i16shiftRightByScalar(globImporti16, 9);
        }
        return i16check(globImporti16);
    }

    return {
        testShiftLeftScalarLocal: testShiftLeftScalarLocal, testShiftRightScalarLocal: testShiftRightScalarLocal, testShiftLeftScalarGlobal: testShiftLeftScalarGlobal,
        testShiftRightScalarGlobal: testShiftRightScalarGlobal, testShiftLeftScalarGlobalImport: testShiftLeftScalarGlobalImport, testShiftRightScalarGlobalImport: testShiftRightScalarGlobalImport,
        testShiftLeftScalarLocal1: testShiftLeftScalarLocal1, testShiftRightScalarLocal1: testShiftRightScalarLocal1, testShiftLeftScalarGlobal1: testShiftLeftScalarGlobal1,
        testShiftRightScalarGlobal1: testShiftRightScalarGlobal1, testShiftLeftScalarGlobalImport1: testShiftLeftScalarGlobalImport1, testShiftRightScalarGlobalImport1: testShiftRightScalarGlobalImport1
    };
}

var m = asmModule(this, { g1: SIMD.Int8x16(100, -1073741824, -1028, -102, 3883, -38, -92929, 1442, 52, 127, -127, -129, 129, 0, 88, 100234) });

equalSimd([-128, -128, -128, -128, 80, 96, 112, -128, -112, -96, -80, -64, -48, -32, -16, 0], m.testShiftLeftScalarLocal(), SIMD.Int8x16, "testMinLocal");
equalSimd([-3, 2, -3, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1], m.testShiftRightScalarLocal(), SIMD.Int8x16, "testMaxLocal");
equalSimd([16, 32, 48, 64, 80, 96, 112, -128, -112, -96, -80, -64, -48, -32, -16, 0], m.testShiftLeftScalarGlobal(), SIMD.Int8x16, "testMinGlobal");
equalSimd([1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1, 0], m.testShiftRightScalarGlobal(), SIMD.Int8x16, "testMaxGlobal");
equalSimd([64, 0, -64, -96, -80, -96, -16, 32, 64, -16, 16, -16, 16, 0, -128, -96], m.testShiftLeftScalarGlobalImport(), SIMD.Int8x16, "testMinGlobalImport");
equalSimd([4, 0, -4, -6, -5, -6, -1, 2, 4, -1, 1, -1, 1, 0, -8, -6], m.testShiftRightScalarGlobalImport(), SIMD.Int8x16, "testMaxGlobalImport");

equalSimd([-128, -128, -128, -128, 80, 96, 112, -128, -112, -96, -80, -64, -48, -32, -16, 0], m.testShiftLeftScalarLocal1(), SIMD.Int8x16, "testMinLocal1");
equalSimd([-3, 2, -3, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1], m.testShiftRightScalarLocal1(), SIMD.Int8x16, "testMaxLocal1");
equalSimd([16, 32, 48, 64, 80, 96, 112, -128, -112, -96, -80, -64, -48, -32, -16, 0], m.testShiftLeftScalarGlobal1(), SIMD.Int8x16, "testMinGlobal1");
equalSimd([1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1, 0], m.testShiftRightScalarGlobal1(), SIMD.Int8x16, "testMaxGlobal1");
equalSimd([64, 0, -64, -96, -80, -96, -16, 32, 64, -16, 16, -16, 16, 0, -128, -96], m.testShiftLeftScalarGlobalImport1(), SIMD.Int8x16, "testMinGlobalImport1");
equalSimd([4, 0, -4, -6, -5, -6, -1, 2, 4, -1, 1, -1, 1, 0, -8, -6], m.testShiftRightScalarGlobalImport1(), SIMD.Int8x16, "testMaxGlobalImport1");

print("PASS");
