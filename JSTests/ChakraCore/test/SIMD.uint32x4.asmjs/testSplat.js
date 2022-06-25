//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var ui4 = stdlib.SIMD.Uint32x4;
    var ui4check = ui4.check;
    var ui4splat = ui4.splat;

    var gval = 20;

    var loopCOUNT = 3;

    var i4 = stdlib.SIMD.Int32x4;
    var i4check = i4.check;
    var i4fu4 = i4.fromUint32x4Bits;

    function splat1()
    {
        var a = ui4(0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            a = ui4splat(4294967295);
            loopIndex = (loopIndex + 1) | 0;
        }   
        return i4check(i4fu4(a));
    }
    
    function splat2()
    {
        var a = ui4(0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            a = ui4splat(value() | 0);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i4check(i4fu4(a));
    }

    function splat3()
    {
        var a = ui4(0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            a = ui4splat(gval);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i4check(i4fu4(a));
    }
    
    function value()
    {
        var ret = 1;
        var i = 0;
      
        for (i = 0; (i | 0) < 100; i = (i + 1) | 0)
            ret = (ret + i) | 0;

        return ret | 0;
    }
    
    return {func1:splat1, func2:splat2, func3:splat3};
}

var m = asmModule(this, { g1: SIMD.Uint32x4(20, 20, 20, 20) });

var ret1 = SIMD.Uint32x4.fromInt32x4Bits(m.func1());
var ret2 = SIMD.Uint32x4.fromInt32x4Bits(m.func2());
var ret3 = SIMD.Uint32x4.fromInt32x4Bits(m.func3());

equalSimd([4294967295, 4294967295, 4294967295, 4294967295], ret1, SIMD.Uint32x4, "");
equalSimd([4951, 4951, 4951, 4951], ret2, SIMD.Uint32x4, "");
equalSimd([20, 20, 20, 20], ret3, SIMD.Uint32x4, "");
print("PASS");
