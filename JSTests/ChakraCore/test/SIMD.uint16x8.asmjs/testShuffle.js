//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var ui8 = stdlib.SIMD.Uint16x8;
    var ui8check = ui8.check;
    var ui8shuffle = ui8.shuffle;
    var ui8add = ui8.add;
    var ui8mul = ui8.mul;

    var globImportui8 = ui8check(imports.g1);

    var ui8g1 = ui8(10, 1073, 107, 1082, 10402, 12332, 311, 650);
    var ui8g2 = ui8(35316, 492529, 1128, 1085, 3692, 3937, 9755, 2638);

    var loopCOUNT = 3;

    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    var i8fu8 = i8.fromUint16x8Bits;

    function testShuffleLocal() {
        var a = ui8(5033, 3401, 665, 3234, 948, 2834, 7748, 25);
        var b = ui8(3483, 2144, 5697, 65, 1000000, 984, 3434, 9876);
        var result = ui8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = ui8shuffle(a, b, 0, 1, 4, 5, 8, 3, 2, 6);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fu8(result));
    }

    function testShuffleGlobal() {
        var result = ui8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = ui8shuffle(ui8g1, ui8g2, 0, 1, 4, 5, 8, 3, 2, 6);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fu8(result));
    }

    function testShuffleGlobalImport() {
        var a = ui8(5033, 3401, 665, 3234, 948, 2834, 7748, 25);
        var result = ui8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = ui8shuffle(a, globImportui8, 0, 1, 4, 5, 8, 3, 2, 6);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fu8(result));
    }
    
    function testShuffleFunc() {
        var a = ui8(5033, 3401, 665, 3234, 948, 2834, 7748, 25);
        var result = ui8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = ui8shuffle(ui8add(a, ui8g1), ui8mul(a, ui8g1), 0, 1, 4, 5, 8, 3, 2, 6);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fu8(result));
    }

    return { testShuffleLocal: testShuffleLocal, testShuffleGlobal: testShuffleGlobal, testShuffleGlobalImport: testShuffleGlobalImport, testShuffleFunc: testShuffleFunc };
}

var m = asmModule(this, { g1: SIMD.Uint16x8(50, 1000, 3092, 3393, Infinity, 39238, NaN, 838) });

var ret1 = SIMD.Uint16x8.fromInt16x8Bits(m.testShuffleLocal());
var ret2 = SIMD.Uint16x8.fromInt16x8Bits(m.testShuffleGlobal());
var ret3 = SIMD.Uint16x8.fromInt16x8Bits(m.testShuffleGlobalImport());
var ret4 = SIMD.Uint16x8.fromInt16x8Bits(m.testShuffleFunc());

equalSimd([5033, 3401, 948, 2834, 3483, 3234, 665, 7748], ret1, SIMD.Uint16x8, "");
equalSimd([10, 1073, 10402, 12332, 35316, 1082, 107, 311], ret2, SIMD.Uint16x8, "");
equalSimd([5033, 3401, 948, 2834, 50, 3234, 665, 7748], ret3, SIMD.Uint16x8, "");
equalSimd([5043, 4474, 11350, 15166, 50330, 4316, 772, 8059], ret4, SIMD.Uint16x8, "");

print("PASS");



