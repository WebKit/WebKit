//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SIMDJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var ui16 = stdlib.SIMD.Uint8x16;
    var ui16check = ui16.check;
    var ui16extractLane = ui16.extractLane;
    var ui16replaceLane = ui16.replaceLane;

    var globImportui16 = ui16check(imports.g1);   

    var ui16g1 = ui16(1, 2, 3, 4, 9999, 6, 0xFF, 0xFFF, -9292992, 256, 255, 0, -1, 0x1, 33333, 16);

    var vectorLength = 16;

    var loopCOUNT = 3;
    
    var i16 = stdlib.SIMD.Int8x16;
    var i16check = i16.check;
    var i16fu16 = i16.fromUint8x16Bits;

    function testLocal()
    {
        var a = ui16(100000, 2345345, -1, 0, -9999, 0xFFF, 0, 0, 00000, 8383838, 1, 55553, 128, 127, 0xA, 16);
        var result = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
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
            
            result = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

            a0 = ui16extractLane(a, 0);
            a1 = ui16extractLane(a, 1);
            a2 = ui16extractLane(a, 2);
            a3 = ui16extractLane(a, 3);
            a4 = ui16extractLane(a, 4);
            a5 = ui16extractLane(a, 5);
            a6 = ui16extractLane(a, 6);
            a7 = ui16extractLane(a, 7);
            a8 = ui16extractLane(a, 8);
            a9 = ui16extractLane(a, 9);
            a10 = ui16extractLane(a, 10);
            a11 = ui16extractLane(a, 11);
            a12 = ui16extractLane(a, 12);
            a13 = ui16extractLane(a, 13);
            a14 = ui16extractLane(a, 14);
            a15 = ui16extractLane(a, 15);

            result = ui16replaceLane(result, 0, a0);
            result = ui16replaceLane(result, 1, a1);
            result = ui16replaceLane(result, 2, a2);
            result = ui16replaceLane(result, 3, a3);
            result = ui16replaceLane(result, 4, a4);
            result = ui16replaceLane(result, 5, a5);
            result = ui16replaceLane(result, 6, a6);
            result = ui16replaceLane(result, 7, a7);
            result = ui16replaceLane(result, 8, a8);
            result = ui16replaceLane(result, 9, a9);
            result = ui16replaceLane(result, 10, a10);
            result = ui16replaceLane(result, 11, a11);
            result = ui16replaceLane(result, 12, a12);
            result = ui16replaceLane(result, 13, a13);
            result = ui16replaceLane(result, 14, a14);
            result = ui16replaceLane(result, 15, a15);

            ui16check(result);

            loopIndex = (loopIndex + 1) | 0;
        }
        return i16check(i16fu16(result));
    }
    
    function testGlobal()
    {
        var result = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
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

            result = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

            a0 = ui16extractLane(ui16g1, 0);
            a1 = ui16extractLane(ui16g1, 1);
            a2 = ui16extractLane(ui16g1, 2);
            a3 = ui16extractLane(ui16g1, 3);
            a4 = ui16extractLane(ui16g1, 4);
            a5 = ui16extractLane(ui16g1, 5);
            a6 = ui16extractLane(ui16g1, 6);
            a7 = ui16extractLane(ui16g1, 7);
            a8 = ui16extractLane(ui16g1, 8);
            a9 = ui16extractLane(ui16g1, 9);
            a10 = ui16extractLane(ui16g1, 10);
            a11 = ui16extractLane(ui16g1, 11);
            a12 = ui16extractLane(ui16g1, 12);
            a13 = ui16extractLane(ui16g1, 13);
            a14 = ui16extractLane(ui16g1, 14);
            a15 = ui16extractLane(ui16g1, 15);

            result = ui16replaceLane(result, 0, a0);
            result = ui16replaceLane(result, 1, a1);
            result = ui16replaceLane(result, 2, a2);
            result = ui16replaceLane(result, 3, a3);
            result = ui16replaceLane(result, 4, a4);
            result = ui16replaceLane(result, 5, a5);
            result = ui16replaceLane(result, 6, a6);
            result = ui16replaceLane(result, 7, a7);
            result = ui16replaceLane(result, 8, a8);
            result = ui16replaceLane(result, 9, a9);
            result = ui16replaceLane(result, 10, a10);
            result = ui16replaceLane(result, 11, a11);
            result = ui16replaceLane(result, 12, a12);
            result = ui16replaceLane(result, 13, a13);
            result = ui16replaceLane(result, 14, a14);
            result = ui16replaceLane(result, 15, a15);

            ui16check(result);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(i16fu16(result));
    }

    function testGlobalImport()
    {
        var result = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
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

            result = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

            a0 = ui16extractLane(globImportui16, 0);
            a1 = ui16extractLane(globImportui16, 1);
            a2 = ui16extractLane(globImportui16, 2);
            a3 = ui16extractLane(globImportui16, 3);
            a4 = ui16extractLane(globImportui16, 4);
            a5 = ui16extractLane(globImportui16, 5);
            a6 = ui16extractLane(globImportui16, 6);
            a7 = ui16extractLane(globImportui16, 7);
            a8 = ui16extractLane(globImportui16, 8);
            a9 = ui16extractLane(globImportui16, 9);
            a10 = ui16extractLane(globImportui16, 10);
            a11 = ui16extractLane(globImportui16, 11);
            a12 = ui16extractLane(globImportui16, 12);
            a13 = ui16extractLane(globImportui16, 13);
            a14 = ui16extractLane(globImportui16, 14);
            a15 = ui16extractLane(globImportui16, 15);

            result = ui16replaceLane(result, 0, a0);
            result = ui16replaceLane(result, 1, a1);
            result = ui16replaceLane(result, 2, a2);
            result = ui16replaceLane(result, 3, a3);
            result = ui16replaceLane(result, 4, a4);
            result = ui16replaceLane(result, 5, a5);
            result = ui16replaceLane(result, 6, a6);
            result = ui16replaceLane(result, 7, a7);
            result = ui16replaceLane(result, 8, a8);
            result = ui16replaceLane(result, 9, a9);
            result = ui16replaceLane(result, 10, a10);
            result = ui16replaceLane(result, 11, a11);
            result = ui16replaceLane(result, 12, a12);
            result = ui16replaceLane(result, 13, a13);
            result = ui16replaceLane(result, 14, a14);
            result = ui16replaceLane(result, 15, a15);

            ui16check(result);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(i16fu16(result));
    }
    
    return {testLocal:testLocal, testGlobal:testGlobal, testGlobalImport:testGlobalImport};
}

var m = asmModule(this, { g1: SIMD.Uint8x16(-1065353216, -1073741824, -1077936128, -1082130432, -383829393, -39283838, -92929, -109483922, -1065353216, -1073741824, -1077936128, -1082130432, -383829393, -39283838, -92929, -109483922) });

var ret1 = SIMD.Uint8x16.fromInt8x16Bits(m.testLocal());
var ret2 = SIMD.Uint8x16.fromInt8x16Bits(m.testGlobal());
var ret3 = SIMD.Uint8x16.fromInt8x16Bits(m.testGlobalImport());

equalSimd([160, 129, 255, 0, 241, 255, 0, 0, 0, 94, 1, 1, 128, 127, 10, 16], ret1, SIMD.Uint8x16, "testLocal");
equalSimd([1, 2, 3, 4, 15, 6, 255, 255, 64, 0, 255, 0, 255, 1, 53, 16], ret2, SIMD.Uint8x16, "testGlobal");
equalSimd([0, 0, 0, 0, 111, 130, 255, 110, 0, 0, 0, 0, 111, 130, 255, 110], ret3, SIMD.Uint8x16, "testGlobalImport");

print("PASS");
