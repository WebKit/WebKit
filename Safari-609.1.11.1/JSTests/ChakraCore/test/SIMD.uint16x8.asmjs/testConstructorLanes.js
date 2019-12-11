//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SIMDJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var ui8 = stdlib.SIMD.Uint16x8;
    var ui8check = ui8.check;
    var ui8extractLane = ui8.extractLane;
    var ui8replaceLane = ui8.replaceLane;

    var globImportui8 = ui8check(imports.g1);

    var ui8g1 = ui8(106, 0, 10, 65535, 3192, 32, 19, 5);

    var vectorLength = 8;

    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    var i8fu8 = i8.fromUint16x8Bits;

    var loopCOUNT = 3;

    function testLocal() {
        var a = ui8(65535, -65535, 0, 0, -65536, 65536, 99999999, -9999999);
        var result = ui8(0, 0, 0, 0, 0, 0, 0, 0);
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
            
            result = ui8(0, 0, 0, 0, 0, 0, 0, 0);

            a0 = ui8extractLane(a, 0);
            a1 = ui8extractLane(a, 1);
            a2 = ui8extractLane(a, 2);
            a3 = ui8extractLane(a, 3);
            a4 = ui8extractLane(a, 4);
            a5 = ui8extractLane(a, 5);
            a6 = ui8extractLane(a, 6);
            a7 = ui8extractLane(a, 7);

            result = ui8replaceLane(result, 0, a0);
            result = ui8replaceLane(result, 1, a1);
            result = ui8replaceLane(result, 2, a2);
            result = ui8replaceLane(result, 3, a3);
            result = ui8replaceLane(result, 4, a4);
            result = ui8replaceLane(result, 5, a5);
            result = ui8replaceLane(result, 6, a6);
            result = ui8replaceLane(result, 7, a7);

            ui8check(result);

            loopIndex = (loopIndex + 1) | 0;
        }
        return i8check(i8fu8(result));
}
    
        function testGlobal() {
            var result = ui8(0, 0, 0, 0, 0, 0, 0, 0);
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

                result = ui8(0, 0, 0, 0, 0, 0, 0, 0);

                a0 = ui8extractLane(ui8g1, 0);
                a1 = ui8extractLane(ui8g1, 1);
                a2 = ui8extractLane(ui8g1, 2);
                a3 = ui8extractLane(ui8g1, 3);
                a4 = ui8extractLane(ui8g1, 4);
                a5 = ui8extractLane(ui8g1, 5);
                a6 = ui8extractLane(ui8g1, 6);
                a7 = ui8extractLane(ui8g1, 7);

                result = ui8replaceLane(result, 0, a0);
                result = ui8replaceLane(result, 1, a1);
                result = ui8replaceLane(result, 2, a2);
                result = ui8replaceLane(result, 3, a3);
                result = ui8replaceLane(result, 4, a4);
                result = ui8replaceLane(result, 5, a5);
                result = ui8replaceLane(result, 6, a6);
                result = ui8replaceLane(result, 7, a7);

                ui8check(result);

                loopIndex = (loopIndex + 1) | 0;
            }

            return i8check(i8fu8(result));
        }

        function testGlobalImport() {
            var result = ui8(0, 0, 0, 0, 0, 0, 0, 0);
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

                result = ui8(0, 0, 0, 0, 0, 0, 0, 0);

                a0 = ui8extractLane(globImportui8, 0);
                a1 = ui8extractLane(globImportui8, 1);
                a2 = ui8extractLane(globImportui8, 2);
                a3 = ui8extractLane(globImportui8, 3);
                a4 = ui8extractLane(globImportui8, 4);
                a5 = ui8extractLane(globImportui8, 5);
                a6 = ui8extractLane(globImportui8, 6);
                a7 = ui8extractLane(globImportui8, 7);

                result = ui8replaceLane(result, 0, a0);
                result = ui8replaceLane(result, 1, a1);
                result = ui8replaceLane(result, 2, a2);
                result = ui8replaceLane(result, 3, a3);
                result = ui8replaceLane(result, 4, a4);
                result = ui8replaceLane(result, 5, a5);
                result = ui8replaceLane(result, 6, a6);
                result = ui8replaceLane(result, 7, a7);

                ui8check(result);

                loopIndex = (loopIndex + 1) | 0;
            }

            return i8check(i8fu8(result));
        }
        
        return { testLocal: testLocal, testGlobal: testGlobal, testGlobalImport: testGlobalImport };
    }

    var m = asmModule(this, { g1: SIMD.Uint16x8(65535, 1000, 3092, -65535, 0, -39283838, 0, -838) });

    var ret1 = SIMD.Uint16x8.fromInt16x8Bits(m.testLocal());
    var ret2 = SIMD.Uint16x8.fromInt16x8Bits(m.testGlobal());
    var ret3 = SIMD.Uint16x8.fromInt16x8Bits(m.testGlobalImport());

    equalSimd([65535, 1, 0, 0, 0, 0, 57599, 27009], ret1, SIMD.Uint16x8, "testLocal");
    equalSimd([106, 0, 10, 65535, 3192, 32, 19, 5], ret2, SIMD.Uint16x8, "testGlobal");
    equalSimd([65535, 1000, 3092, 1, 0, 37762, 0, 64698], ret3, SIMD.Uint16x8, "testGlobalImport");

    print("PASS");
