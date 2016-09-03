//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var i16 = stdlib.SIMD.Int8x16;
    var i16check = i16.check;
    var i16splat = i16.splat;
  
    var globImporti16 = i16check(imports.g1);       // global var import
   
    var i16g1 = i16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

    var gval = 20;

    var loopCOUNT = 5;

    function splat1()
    {
        var a = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            a = i16splat(100);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i16check(a);
    }
    
    function splat2()
    {
        var a = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            a = i16splat(value() | 0);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i16check(a);
    }

    function splat3()
    {
        var a = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            a = i16splat(gval);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i16check(a);
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

var m = asmModule(this, {g1:SIMD.Int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)});

var ret1 = m.func1(SIMD.Int8x16(1, 2, 3, 4, 5, 6, 7, 8), SIMD.Float32x4(1, 2, 3, 4)/*, SIMD.Float64x2(1, 2, 3, 4)*/);
var ret2 = m.func2(SIMD.Int8x16(1, 2, 3, 4, 5, 6, 7, 8), SIMD.Float32x4(1, 2, 3, 4)/*, SIMD.Float64x2(1, 2, 3, 4)*/);
var ret3 = m.func3(SIMD.Int8x16(1, 2, 3, 4, 5, 6, 7, 8), SIMD.Float32x4(1, 2, 3, 4)/*, SIMD.Float64x2(1, 2, 3, 4)*/);

equalSimd([100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100], ret1, SIMD.Int8x16, "");
equalSimd([87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87], ret2, SIMD.Int8x16, "");
equalSimd([20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20], ret3, SIMD.Int8x16, "");
print("PASS");
