//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var ui16 = stdlib.SIMD.Uint8x16;
    var ui16check = ui16.check;
    var ui16splat = ui16.splat;
  
    var globImportui16 = ui16check(imports.g1);       // global var import
   
    var ui16g1 = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

    var gval = 20;

    var loopCOUNT = 3;
    
    var i16 = stdlib.SIMD.Int8x16;
    var i16check = i16.check;
    var i16fu16 = i16.fromUint8x16Bits;

    function splat1()
    {
        var a = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            a = ui16splat(255);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i16check(i16fu16(a));
    }
    
    function splat2()
    {
        var a = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            a = ui16splat(value() | 0);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i16check(i16fu16(a));
    }

    function splat3()
    {
        var a = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            a = ui16splat(gval);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i16check(i16fu16(a));
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

var m = asmModule(this, {g1:SIMD.Uint8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)});

var ret1 = SIMD.Uint8x16.fromInt8x16Bits(m.func1(SIMD.Uint8x16(1, 2, 3, 4, 5, 6, 7, 8), SIMD.Float32x4(1, 2, 3, 4)/*, SIMD.Float64x2(1, 2, 3, 4)*/));
var ret2 = SIMD.Uint8x16.fromInt8x16Bits(m.func2(SIMD.Uint8x16(1, 2, 3, 4, 5, 6, 7, 8), SIMD.Float32x4(1, 2, 3, 4)/*, SIMD.Float64x2(1, 2, 3, 4)*/));
var ret3 = SIMD.Uint8x16.fromInt8x16Bits(m.func3(SIMD.Uint8x16(1, 2, 3, 4, 5, 6, 7, 8), SIMD.Float32x4(1, 2, 3, 4)/*, SIMD.Float64x2(1, 2, 3, 4)*/));

equalSimd([255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255], ret1, SIMD.Uint8x16, "");
equalSimd([87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87], ret2, SIMD.Uint8x16, "");
equalSimd([20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20], ret3, SIMD.Uint8x16, "");
print("PASS");
