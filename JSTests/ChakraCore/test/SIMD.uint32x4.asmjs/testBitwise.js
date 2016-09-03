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
    var ui4fromFloat32x4 = ui4.fromFloat32x4;
    var ui4fromFloat32x4Bits = ui4.fromFloat32x4Bits;
    //var ui4abs = ui4.abs;
    var ui4add = ui4.add;
    var ui4sub = ui4.sub;
    var ui4mul = ui4.mul;

    var ui4and = ui4.and;
    var ui4or = ui4.or;
    var ui4xor = ui4.xor;
    var ui4not = ui4.not;
    var ui4neg = ui4.neg;

    var globImportui4 = ui4check(imports.g1);       // global var import

    var ui4g1 = ui4(1065353216, 1073741824, 1077936128, 1082130432);          // global var initialized
    var ui4g2 = ui4(6531634, 74182444, 779364128, 821730432);

    var gval = 1234;
    var gval2 = 1234.0;

    var loopCOUNT = 3;
    
    var i4 = stdlib.SIMD.Int32x4;
    var i4check = i4.check;
    var i4fu4 = i4.fromUint32x4Bits;

    function func1()
    {
        var b = ui4(8488484, 4848848, 29975939, 9493872);
        var c = ui4(99372621, 18848392, 888288822, 1000010012);
        var d = ui4(0, 0, 0, 0);


        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            d = ui4and(b, b);
            b = ui4or(c, c);
            d = ui4xor(b, d);
            d = ui4not(d);
            d = ui4neg(d);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i4check(i4fu4(d));
    }
    
    function func2()
    {
        var b = ui4(8488484, 4848848, 29975939, 9493872);
        var c = ui4(99372621, 18848392, 888288822, 1000010012);
        var d = ui4(0, 0, 0, 0);
        var loopIndex = 0;

        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            d = ui4and(ui4g1, ui4g2);
            d = ui4or(d, b);
            d = ui4xor(d, globImportui4);
            d = ui4not(d);
            d = ui4neg(d);
            
        }

        return i4check(i4fu4(d));
    }

    function func3()
    {
        var b = ui4(8488484, 4848848, 29975939, 9493872);
        var c = ui4(99372621, 18848392, 888288822, 1000010012);
        var d = ui4(0, 0, 0, 0);
        var loopIndex = 0;

        loopIndex = loopCOUNT | 0;
        do {

            ui4g1 = ui4and(ui4g1, ui4g2);
            ui4g1 = ui4or(ui4g1, b);
            ui4g1 = ui4xor(ui4g1, c);
            ui4g1 = ui4not(ui4g1);
            ui4g1 = ui4neg(ui4g1);

            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);

        return i4check(i4fu4(ui4g1));
    }
    
    
    return {func1:func1, func2:func2, func3:func3};
}

var m = asmModule(this, { g1: SIMD.Uint32x4(100, 1073741824, 1028, 102) });

var ret1 = SIMD.Uint32x4.fromInt32x4Bits(m.func1());
var ret2 = SIMD.Uint32x4.fromInt32x4Bits(m.func2());
var ret3 = SIMD.Uint32x4.fromInt32x4Bits(m.func3());


equalSimd([1, 1, 1, 1], ret1, SIMD.Uint32x4, "Func1");
equalSimd([8488513, 1078590673, 29974920, 9493783], ret2, SIMD.Uint32x4, "Func2");
equalSimd([91080810, 22439513, 893080502, 990522477], ret3, SIMD.Uint32x4, "Func3");
print("PASS");
