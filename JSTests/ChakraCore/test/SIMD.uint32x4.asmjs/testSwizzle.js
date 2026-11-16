//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var ui4 = stdlib.SIMD.Uint32x4;
    var ui4check = ui4.check;
    var ui4swizzle = ui4.swizzle;
    var ui4add = ui4.add;
    var ui4mul = ui4.mul;

    var globImporti4 = ui4check(imports.g1);

    var ui4g1 = ui4(1065353216, 1073741824, 1077936128, 1082130432);          // global var initialized
    var ui4g2 = ui4(6531634, 74182444, 779364128, 821730432);

    var loopCOUNT = 3;

    var i4 = stdlib.SIMD.Int32x4;
    var i4check = i4.check;
    var i4fu4 = i4.fromUint32x4Bits;
    var u4fi4 = ui4.fromInt32x4Bits;

    function testswizzleLocal() {
        var a = ui4(8488484, 4848848, 29975939, 9493872);
        var result = ui4(0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = ui4swizzle(a, 2, 0, 3, 1);
            loopIndex = (loopIndex + 1) | 0;
        }   
        return i4check(i4fu4(result));
    }

    function testswizzleGlobal() {
        var result = ui4(0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = ui4swizzle(ui4g1, 2, 0, 3, 1);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i4check(i4fu4(result));
    }

    function testswizzleGlobalImport() {
        var result = ui4(0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = ui4swizzle(globImporti4, 2, 0, 3, 1);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i4check(i4fu4(result));
    }

    function testswizzleFunc() {
        var a = ui4(8488484, 4848848, 29975939, 9493872);
        var result = ui4(0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = ui4swizzle(ui4add(a, ui4g1), 2, 0, 3, 1);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i4check(i4fu4(result));
    }

    return { testswizzleLocal: testswizzleLocal, testswizzleGlobal: testswizzleGlobal, testswizzleGlobalImport: testswizzleGlobalImport, testswizzleFunc: testswizzleFunc };
}

var m = asmModule(this, { g1: SIMD.Uint32x4(50, 1000, 3092, 3393, 8838, 63838, NaN, 838) });

var ret1 = SIMD.Uint32x4.fromInt32x4Bits(m.testswizzleLocal());
var ret2 = SIMD.Uint32x4.fromInt32x4Bits(m.testswizzleGlobal());
var ret3 = SIMD.Uint32x4.fromInt32x4Bits(m.testswizzleGlobalImport());
var ret4 = SIMD.Uint32x4.fromInt32x4Bits(m.testswizzleFunc());

equalSimd([29975939, 8488484, 9493872, 4848848],  ret1, SIMD.Uint32x4, "");
equalSimd([1077936128, 1065353216, 1082130432, 1073741824], ret2, SIMD.Uint32x4, "");
equalSimd([3092, 50, 3393, 1000], ret3, SIMD.Uint32x4, "");
equalSimd([1107912067, 1073841700, 1091624304, 1078590672], ret4, SIMD.Uint32x4, "");
print("PASS");



