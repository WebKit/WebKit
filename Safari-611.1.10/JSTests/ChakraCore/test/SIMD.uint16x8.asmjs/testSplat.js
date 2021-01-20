//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var ui8 = stdlib.SIMD.Uint16x8;
    var ui8check = ui8.check;
    var ui8splat = ui8.splat;

    var gval = 20;

    var loopCOUNT = 3;

    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    var i8fu8 = i8.fromUint16x8Bits;

    function splat1()
    {
        var a = ui8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            a = ui8splat(65535);
            loopIndex = (loopIndex + 1) | 0;
        }   
        return i8check(i8fu8(a));
    }
    
    function splat2()
    {
        var a = ui8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            a = ui8splat(value() | 0);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fu8(a));
    }

    function splat3()
    {
        var a = ui8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            a = ui8splat(gval);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fu8(a));
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

var m = asmModule(this, {g1:SIMD.Uint16x8(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)});

var ret1 = SIMD.Uint16x8.fromInt16x8Bits(m.func1());
var ret2 = SIMD.Uint16x8.fromInt16x8Bits(m.func2());
var ret3 = SIMD.Uint16x8.fromInt16x8Bits(m.func3());

equalSimd([65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535], ret1, SIMD.Uint16x8, "");
equalSimd([4951, 4951, 4951, 4951, 4951, 4951, 4951, 4951], ret2, SIMD.Uint16x8, "");
equalSimd([20, 20, 20, 20, 20, 20, 20, 20], ret3, SIMD.Uint16x8, "");
print("PASS");
