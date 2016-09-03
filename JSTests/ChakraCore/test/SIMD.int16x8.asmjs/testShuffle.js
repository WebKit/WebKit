//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    var i8shuffle = i8.shuffle;
    var i8add = i8.add;
    var i8mul = i8.mul;

    var globImporti8 = i8check(imports.g1);

    var i8g1 = i8(10, -1073, -107, 1082, 10402, 12332, 311, -650);
    var i8g2 = i8(353216, -492529, -1128, 1085, 3692, 3937, 9755, 2638);

    var loopCOUNT = 3;

    function testShuffleLocal() {
        var a = i8(5033, -3401, 665, -3234, -948, 2834, 7748, -25);
        var b = i8(-3483, 2144, -5697, 65, 1000000, -984, 3434, -9876);
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i8shuffle(a, b, 0, 1, 4, 5, 8, 3, 2, 6);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(result);
    }

    function testShuffleGlobal() {
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i8shuffle(i8g1, i8g2, 0, 1, 4, 5, 8, 3, 2, 6);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(result);
    }

    function testShuffleGlobalImport() {
        var a = i8(5033, -3401, 665, -3234, -948, 2834, 7748, -25);
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i8shuffle(a, globImporti8, 0, 1, 4, 5, 8, 3, 2, 6);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(result);
    }
    
    function testShuffleFunc() {
        var a = i8(5033, -3401, 665, -3234, -948, 2834, 7748, -25);
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i8shuffle(i8add(a, i8g1), i8mul(a, i8g1), 0, 1, 4, 5, 8, 3, 2, 6);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(result);
    }

    return { testShuffleLocal: testShuffleLocal, testShuffleGlobal: testShuffleGlobal, testShuffleGlobalImport: testShuffleGlobalImport, testShuffleFunc: testShuffleFunc };
}

var m = asmModule(this, { g1: SIMD.Int16x8(-50, 1000, 3092, -3393, Infinity, -39283838, NaN, -838) });

equalSimd([5033, -3401, -948, 2834, -3483, -3234, 665, 7748], m.testShuffleLocal(), SIMD.Int16x8, "");
equalSimd([10, -1073, 10402, 12332, 25536, 1082, -107, 311], m.testShuffleGlobal(), SIMD.Int16x8, "");
equalSimd([5033, -3401, -948, 2834, -50, -3234, 665, 7748], m.testShuffleGlobalImport(), SIMD.Int16x8, "");
equalSimd([5043, -4474, 9454, 15166, -15206, -2152, 558, 8059], m.testShuffleFunc(), SIMD.Int16x8, "");

print("PASS");



