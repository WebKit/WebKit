//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SIMDJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    var i8extractLane = i8.extractLane;
    var i8replaceLane = i8.replaceLane;

    var globImporti8 = i8check(imports.g1);   

    var i8g1 = i8(106, 0, -10, 1082, 3192, 32, 19, 5);

    var vectorLength = 8;

    var loopCOUNT = 3;

    function testLocal()
    {
        var a = i8(5000, -3401, 665, -3224, -948, 2834, 7748, -25);
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        var a0 = 0;
        var a1 = 0;
        var a2 = 0;
        var a3 = 0;
        var a4 = 0;
        var a5 = 0;
        var a6 = 0;
        var a7 = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            
            result = i8(0, 0, 0, 0, 0, 0, 0, 0);

            a0 = i8extractLane(a, 0);
            a1 = i8extractLane(a, 1);
            a2 = i8extractLane(a, 2);
            a3 = i8extractLane(a, 3);
            a4 = i8extractLane(a, 4);
            a5 = i8extractLane(a, 5);
            a6 = i8extractLane(a, 6);
            a7 = i8extractLane(a, 7);

            result = i8replaceLane(result, 0, a0);
            result = i8replaceLane(result, 1, a1);
            result = i8replaceLane(result, 2, a2);
            result = i8replaceLane(result, 3, a3);
            result = i8replaceLane(result, 4, a4);
            result = i8replaceLane(result, 5, a5);
            result = i8replaceLane(result, 6, a6);
            result = i8replaceLane(result, 7, a7);
      
            i8check(result);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(result);
    }

    function testGlobal() {
        var a = i8(5000, -3401, 665, -3224, -948, 2834, 7748, -25);
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        var a0 = 0;
        var a1 = 0;
        var a2 = 0;
        var a3 = 0;
        var a4 = 0;
        var a5 = 0;
        var a6 = 0;
        var a7 = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            result = i8(0, 0, 0, 0, 0, 0, 0, 0);

            a0 = i8extractLane(i8g1, 0);
            a1 = i8extractLane(i8g1, 1);
            a2 = i8extractLane(i8g1, 2);
            a3 = i8extractLane(i8g1, 3);
            a4 = i8extractLane(i8g1, 4);
            a5 = i8extractLane(i8g1, 5);
            a6 = i8extractLane(i8g1, 6);
            a7 = i8extractLane(i8g1, 7);

            result = i8replaceLane(result, 0, a0);
            result = i8replaceLane(result, 1, a1);
            result = i8replaceLane(result, 2, a2);
            result = i8replaceLane(result, 3, a3);
            result = i8replaceLane(result, 4, a4);
            result = i8replaceLane(result, 5, a5);
            result = i8replaceLane(result, 6, a6);
            result = i8replaceLane(result, 7, a7);

            i8check(result);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(result);
    }

    function testGlobalImport() {
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        var a0 = 0;
        var a1 = 0;
        var a2 = 0;
        var a3 = 0;
        var a4 = 0;
        var a5 = 0;
        var a6 = 0;
        var a7 = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            result = i8(0, 0, 0, 0, 0, 0, 0, 0);

            a0 = i8extractLane(globImporti8, 0);
            a1 = i8extractLane(globImporti8, 1);
            a2 = i8extractLane(globImporti8, 2);
            a3 = i8extractLane(globImporti8, 3);
            a4 = i8extractLane(globImporti8, 4);
            a5 = i8extractLane(globImporti8, 5);
            a6 = i8extractLane(globImporti8, 6);
            a7 = i8extractLane(globImporti8, 7);

            result = i8replaceLane(result, 0, a0);
            result = i8replaceLane(result, 1, a1);
            result = i8replaceLane(result, 2, a2);
            result = i8replaceLane(result, 3, a3);
            result = i8replaceLane(result, 4, a4);
            result = i8replaceLane(result, 5, a5);
            result = i8replaceLane(result, 6, a6);
            result = i8replaceLane(result, 7, a7);

            i8check(result);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(result);
    }
    
    return {testLocal:testLocal, testGlobal:testGlobal, testGlobalImport:testGlobalImport};
}

var m = asmModule(this, { g1: SIMD.Int16x8(-50, 1000, 3092, -3393, 0, -39283838, 0, -838) });

equalSimd([5000, -3401, 665, -3224, -948, 2834, 7748, -25], m.testLocal(), SIMD.Int16x8, "testLocal");
equalSimd([106, 0, -10, 1082, 3192, 32, 19, 5], m.testGlobal(), SIMD.Int16x8, "testGlobal");
equalSimd([-50, 1000, 3092, -3393, 0, -27774, 0, -838], m.testGlobalImport(), SIMD.Int16x8, "testGlobalImport");

print("PASS");
