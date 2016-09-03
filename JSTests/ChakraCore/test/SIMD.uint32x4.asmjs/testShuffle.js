//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var ui4 = stdlib.SIMD.Uint32x4;
    var ui4check = ui4.check;
    var ui4shuffle = ui4.shuffle;
    var ui4add = ui4.add;
    var ui4mul = ui4.mul;

    var globImportui4 = ui4check(imports.g1);

    var ui4g1 = ui4(1065353216, 1073741824, 1077936128, 1082130432);          // global var initialized
    var ui4g2 = ui4(6531634, 74182444, 779364128, 821730432);

    var loopCOUNT = 3;

    var i4 = stdlib.SIMD.Int32x4;
    var i4check = i4.check;
    var i4fu4 = i4.fromUint32x4Bits;

    function testShuffleLocal() {
        var a = ui4(8488484, 4848848, 29975939, 9493872);
        var b = ui4(99372621, 18848392, 888288822, 1000010012);
        var result = ui4(0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = ui4shuffle(a, b, 3, 0, 2, 1);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i4check(i4fu4(result));
    }

    function testShuffleGlobal() {
        var result = ui4(0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = ui4shuffle(ui4g1, ui4g2, 3, 0, 2, 1);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i4check(i4fu4(result));
    }

    function testShuffleGlobalImport() {
        var a = ui4(8488484, 4848848, 29975939, 9493872);
        var result = ui4(0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = ui4shuffle(a, globImportui4, 3, 0, 2, 1);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i4check(i4fu4(result));
    }
    
    function testShuffleFunc() {
        var a = ui4(8488484, 4848848, 29975939, 9493872);
        var result = ui4(0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = ui4shuffle(ui4add(a, ui4g1), ui4mul(a, ui4g1), 3, 0, 2, 1);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i4check(i4fu4(result));
    }

    return { testShuffleLocal: testShuffleLocal, testShuffleGlobal: testShuffleGlobal, testShuffleGlobalImport: testShuffleGlobalImport, testShuffleFunc: testShuffleFunc };
}

var m = asmModule(this, { g1: SIMD.Uint32x4(100, 1073741824, 1028, 102) });

var ret1 = SIMD.Uint32x4.fromInt32x4Bits(m.testShuffleLocal());
var ret2 = SIMD.Uint32x4.fromInt32x4Bits(m.testShuffleGlobal());
var ret3 = SIMD.Uint32x4.fromInt32x4Bits(m.testShuffleGlobalImport());
var ret4 = SIMD.Uint32x4.fromInt32x4Bits(m.testShuffleFunc());

equalSimd([9493872, 8488484, 29975939, 4848848], ret1, SIMD.Uint32x4, "");
equalSimd([1082130432, 1065353216, 1077936128, 1073741824], ret2, SIMD.Uint32x4, "");
equalSimd([9493872, 8488484, 29975939, 4848848], ret3, SIMD.Uint32x4, "");
equalSimd([1091624304, 1073841700, 1107912067, 1078590672], ret4, SIMD.Uint32x4, "");

print("PASS");



