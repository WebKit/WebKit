//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SIMDJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var i16 = stdlib.SIMD.Int8x16;
    var i16check = i16.check;
    var i16extractLane = i16.extractLane;
    var i16replaceLane = i16.replaceLane;

    var globImporti16 = i16check(imports.g1);   

    var i16g1 = i16(1, 2, 3, 4, -9999, 6, 0xFF, 0xFFF, -9292992, 128, 127, -128, -129, 0x1, 33333, 16);

    var vectorLength = 16;

    var loopCOUNT = 3;

    function testLocal()
    {
        var a = i16(100000, 2345345, -128, -129, -9999, 0xFFF, 0, 0, 00000, 8383838, 1, 55553, 128, 127, 0xA, 16);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);;
        var loopIndex = 0;

        var a0 = 0;
        var a1 = 0;
        var a2 = 0;
        var a3 = 0;
        var a4 = 0;
        var a5 = 0;
        var a6 = 0;
        var a7 = 0;
        var a8 = 0;
        var a9 = 0;
        var a10 = 0;
        var a11 = 0;
        var a12 = 0;
        var a13 = 0;
        var a14 = 0;
        var a15 = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

            a0 = i16extractLane(a, 0);
            a1 = i16extractLane(a, 1);
            a2 = i16extractLane(a, 2);
            a3 = i16extractLane(a, 3);
            a4 = i16extractLane(a, 4);
            a5 = i16extractLane(a, 5);
            a6 = i16extractLane(a, 6);
            a7 = i16extractLane(a, 7);
            a8 = i16extractLane(a, 8);
            a9 = i16extractLane(a, 9);
            a10 = i16extractLane(a, 10);
            a11 = i16extractLane(a, 11);
            a12 = i16extractLane(a, 12);
            a13 = i16extractLane(a, 13);
            a14 = i16extractLane(a, 14);
            a15 = i16extractLane(a, 15);

            result = i16replaceLane(result, 0, a0);
            result = i16replaceLane(result, 1, a1);
            result = i16replaceLane(result, 2, a2);
            result = i16replaceLane(result, 3, a3);
            result = i16replaceLane(result, 4, a4);
            result = i16replaceLane(result, 5, a5);
            result = i16replaceLane(result, 6, a6);
            result = i16replaceLane(result, 7, a7);
            result = i16replaceLane(result, 8, a8);
            result = i16replaceLane(result, 9, a9);
            result = i16replaceLane(result, 10, a10);
            result = i16replaceLane(result, 11, a11);
            result = i16replaceLane(result, 12, a12);
            result = i16replaceLane(result, 13, a13);
            result = i16replaceLane(result, 14, a14);
            result = i16replaceLane(result, 15, a15);

            i16check(result);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }
    
    function testGlobal()
    {
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        var a0 = 0;
        var a1 = 0;
        var a2 = 0;
        var a3 = 0;
        var a4 = 0;
        var a5 = 0;
        var a6 = 0;
        var a7 = 0;
        var a8 = 0;
        var a9 = 0;
        var a10 = 0;
        var a11 = 0;
        var a12 = 0;
        var a13 = 0;
        var a14 = 0;
        var a15 = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

            a0 = i16extractLane(i16g1, 0);
            a1 = i16extractLane(i16g1, 1);
            a2 = i16extractLane(i16g1, 2);
            a3 = i16extractLane(i16g1, 3);
            a4 = i16extractLane(i16g1, 4);
            a5 = i16extractLane(i16g1, 5);
            a6 = i16extractLane(i16g1, 6);
            a7 = i16extractLane(i16g1, 7);
            a8 = i16extractLane(i16g1, 8);
            a9 = i16extractLane(i16g1, 9);
            a10 = i16extractLane(i16g1, 10);
            a11 = i16extractLane(i16g1, 11);
            a12 = i16extractLane(i16g1, 12);
            a13 = i16extractLane(i16g1, 13);
            a14 = i16extractLane(i16g1, 14);
            a15 = i16extractLane(i16g1, 15);

            result = i16replaceLane(result, 0, a0);
            result = i16replaceLane(result, 1, a1);
            result = i16replaceLane(result, 2, a2);
            result = i16replaceLane(result, 3, a3);
            result = i16replaceLane(result, 4, a4);
            result = i16replaceLane(result, 5, a5);
            result = i16replaceLane(result, 6, a6);
            result = i16replaceLane(result, 7, a7);
            result = i16replaceLane(result, 8, a8);
            result = i16replaceLane(result, 9, a9);
            result = i16replaceLane(result, 10, a10);
            result = i16replaceLane(result, 11, a11);
            result = i16replaceLane(result, 12, a12);
            result = i16replaceLane(result, 13, a13);
            result = i16replaceLane(result, 14, a14);
            result = i16replaceLane(result, 15, a15);

            i16check(result);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }

    function testGlobalImport()
    {
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        var a0 = 0;
        var a1 = 0;
        var a2 = 0;
        var a3 = 0;
        var a4 = 0;
        var a5 = 0;
        var a6 = 0;
        var a7 = 0;
        var a8 = 0;
        var a9 = 0;
        var a10 = 0;
        var a11 = 0;
        var a12 = 0;
        var a13 = 0;
        var a14 = 0;
        var a15 = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

            a0 = i16extractLane(globImporti16, 0);
            a1 = i16extractLane(globImporti16, 1);
            a2 = i16extractLane(globImporti16, 2);
            a3 = i16extractLane(globImporti16, 3);
            a4 = i16extractLane(globImporti16, 4);
            a5 = i16extractLane(globImporti16, 5);
            a6 = i16extractLane(globImporti16, 6);
            a7 = i16extractLane(globImporti16, 7);
            a8 = i16extractLane(globImporti16, 8);
            a9 = i16extractLane(globImporti16, 9);
            a10 = i16extractLane(globImporti16, 10);
            a11 = i16extractLane(globImporti16, 11);
            a12 = i16extractLane(globImporti16, 12);
            a13 = i16extractLane(globImporti16, 13);
            a14 = i16extractLane(globImporti16, 14);
            a15 = i16extractLane(globImporti16, 15);

            result = i16replaceLane(result, 0, a0);
            result = i16replaceLane(result, 1, a1);
            result = i16replaceLane(result, 2, a2);
            result = i16replaceLane(result, 3, a3);
            result = i16replaceLane(result, 4, a4);
            result = i16replaceLane(result, 5, a5);
            result = i16replaceLane(result, 6, a6);
            result = i16replaceLane(result, 7, a7);
            result = i16replaceLane(result, 8, a8);
            result = i16replaceLane(result, 9, a9);
            result = i16replaceLane(result, 10, a10);
            result = i16replaceLane(result, 11, a11);
            result = i16replaceLane(result, 12, a12);
            result = i16replaceLane(result, 13, a13);
            result = i16replaceLane(result, 14, a14);
            result = i16replaceLane(result, 15, a15);

            i16check(result);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }
    
    return {testLocal:testLocal, testGlobal:testGlobal, testGlobalImport:testGlobalImport};
}

var m = asmModule(this, { g1: SIMD.Int8x16(-1065353216, -1073741824, -1077936128, -1082130432, -383829393, -39283838, -92929, -109483922, -1065353216, -1073741824, -1077936128, -1082130432, -383829393, -39283838, -92929, -109483922) });

equalSimd([-96, -127, -128, 127, -15, -1, 0, 0, 0, 94, 1, 1, -128, 127, 10, 16], m.testLocal(), SIMD.Int8x16, "testLocal");
equalSimd([1, 2, 3, 4, -15, 6, -1, -1, 64, -128, 127, -128, 127, 1, 53, 16], m.testGlobal(), SIMD.Int8x16, "testGlobal");
equalSimd([0, 0, 0, 0, 111, -126, -1, 110, 0, 0, 0, 0, 111, -126, -1, 110], m.testGlobalImport(), SIMD.Int8x16, "testGlobalImport");

print("PASS");
