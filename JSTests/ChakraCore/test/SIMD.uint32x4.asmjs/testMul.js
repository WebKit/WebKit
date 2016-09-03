//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var ui4 = stdlib.SIMD.Uint32x4;
    var ui4check = ui4.check;
    var ui4mul = ui4.mul;

    var globImportui4 = ui4check(imports.g1);

    var ui4g1 = ui4(1065353216, 1073741824, 1077936128, 1082130432);          // global var initialized
    var ui4g2 = ui4(6531634, 74182444, 779364128, 821730432);

    var loopCOUNT = 3;
     
    var i4 = stdlib.SIMD.Int32x4;
    var i4check = i4.check;
    var i4fu4 = i4.fromUint32x4Bits;

    function testMulLocal()
    {
        var a = ui4(8488484, 4848848, 29975939, 9493872);
        var b = ui4(99372621, 18848392, 888288822, 1000010012);
        var result = ui4(0, 0, 0, 0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = ui4mul(a, b);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i4check(i4fu4(result));
    }
    function testMulGlobal()
    {    
        var result = ui4(0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = ui4mul(ui4g1, ui4g2);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i4check(i4fu4(result));
    }

    function testMulGlobalImport()
    {
        var a = ui4(8488484, 4848848, 29975939, 9493872);
        var result = ui4(0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = ui4mul(globImportui4, a);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i4check(i4fu4(result));
    }
    
    return {testMulLocal:testMulLocal, testMulGlobal:testMulGlobal, testMulGlobalImport:testMulGlobalImport};
}

var m = asmModule(this, { g1: SIMD.Uint32x4(100, 1073741824, 1028, 102) });

var ret1 = SIMD.Uint32x4.fromInt32x4Bits(m.testMulLocal());
var ret2 = SIMD.Uint32x4.fromInt32x4Bits(m.testMulGlobal());
var ret3 = SIMD.Uint32x4.fromInt32x4Bits(m.testMulGlobalImport());

equalSimd([4211364052, 378760832, 1840974754, 1974380608], ret1, SIMD.Uint32x4, "Test Mul");
equalSimd([1728053248, 0, 3355443200, 1073741824], ret2, SIMD.Uint32x4, "Test Mul");
equalSimd([848848400, 0, 750494220, 968374944], ret3, SIMD.Uint32x4, "Test Mul");
print("PASS");