//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SIMDJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var ui4 = stdlib.SIMD.Uint32x4;
    var ui4check = ui4.check;
    var ui4extractLane = ui4.extractLane;
    var ui4replaceLane = ui4.replaceLane;

    var globImportui4 = ui4check(imports.g1);

    var ui4g1 = ui4(1065353216, 1073741824, 1077936128, 1082130432);

    var vectorLength = 8;

    var loopCOUNT = 3;
       
    var i4 = stdlib.SIMD.Int32x4;
    var i4check = i4.check;
    var i4fu4 = i4.fromUint32x4Bits;

    function testLocal() {
        var a = ui4(8488484, 4848848, 29975939, 9493872);
        var result = ui4(0, 0, 0, 0);
        var loopIndex = 0;

        var a0 = 0;
        var a1 = 0;
        var a2 = 0;
        var a3 = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            
            result = ui4(0, 0, 0, 0);

            a0 = ui4extractLane(a, 0);
            a1 = ui4extractLane(a, 1);
            a2 = ui4extractLane(a, 2);
            a3 = ui4extractLane(a, 3);

            result = ui4replaceLane(result, 0, a0);
            result = ui4replaceLane(result, 1, a1);
            result = ui4replaceLane(result, 2, a2);
            result = ui4replaceLane(result, 3, a3);

            ui4check(result);

            loopIndex = (loopIndex + 1) | 0;
        }
        return i4check(i4fu4(result));
    }
    
        function testGlobal() {
            var result = ui4(0, 0, 0, 0);
            var loopIndex = 0;

            var a0 = 0;
            var a1 = 0;
            var a2 = 0;
            var a3 = 0;

            while ((loopIndex | 0) < (loopCOUNT | 0)) {

                result = ui4(0, 0, 0, 0);

                a0 = ui4extractLane(ui4g1, 0);
                a1 = ui4extractLane(ui4g1, 1);
                a2 = ui4extractLane(ui4g1, 2);
                a3 = ui4extractLane(ui4g1, 3);

                result = ui4replaceLane(result, 0, a0);
                result = ui4replaceLane(result, 1, a1);
                result = ui4replaceLane(result, 2, a2);
                result = ui4replaceLane(result, 3, a3);

                ui4check(result);

                loopIndex = (loopIndex + 1) | 0;
            }

        return i4check(i4fu4(result));
        }

        function testGlobalImport() {
            var result = ui4(0, 0, 0, 0);
            var loopIndex = 0;

            var a0 = 0;
            var a1 = 0;
            var a2 = 0;
            var a3 = 0;

            while ((loopIndex | 0) < (loopCOUNT | 0)) {

                result = ui4(0, 0, 0, 0);

                a0 = ui4extractLane(globImportui4, 0);
                a1 = ui4extractLane(globImportui4, 1);
                a2 = ui4extractLane(globImportui4, 2);
                a3 = ui4extractLane(globImportui4, 3);

                result = ui4replaceLane(result, 0, a0);
                result = ui4replaceLane(result, 1, a1);
                result = ui4replaceLane(result, 2, a2);
                result = ui4replaceLane(result, 3, a3);

                ui4check(result);

                loopIndex = (loopIndex + 1) | 0;
            }

        return i4check(i4fu4(result));
        }
        
        return { testLocal: testLocal, testGlobal: testGlobal, testGlobalImport: testGlobalImport };
    }

    var m = asmModule(this, { g1: SIMD.Uint32x4(100, 1073741824, 1028, 102) });
    
    var ret1 = SIMD.Uint32x4.fromInt32x4Bits(m.testLocal());
    var ret2 = SIMD.Uint32x4.fromInt32x4Bits(m.testGlobal());
    var ret3 = SIMD.Uint32x4.fromInt32x4Bits(m.testGlobalImport());

    equalSimd([8488484, 4848848, 29975939, 9493872], ret1, SIMD.Uint32x4, "testLocal");
    equalSimd([1065353216, 1073741824, 1077936128, 1082130432], ret2, SIMD.Uint32x4, "testGlobal");
    equalSimd([100, 1073741824, 1028, 102], ret3, SIMD.Uint32x4, "testGlobalImport");

    print("PASS");
