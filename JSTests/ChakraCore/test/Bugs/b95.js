//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function asmModule(stdlib, imports) {
    "use asm";

    var i16 = stdlib.SIMD.Int8x16;
    var i16c = i16.check;
    var i4 = stdlib.SIMD.Int32x4;
    var i16shiftRightByScalar = i16.shiftRightByScalar;
    var i16add = i16.add;
    var i4extractLane = i4.extractLane;

    function f2(a)
    {
        a = i16c(a);
        return a;
    }

    function testShiftLeftScalarLocal()
    {
        var a = i16(255, 40, -40, 40, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var b = i4(255,0,0,0);
        var i = 0;
        i = i4extractLane (b, 0);
        a = i16shiftRightByScalar(a, i >>> 0);
        a = i16c(f2(a));
        return i|0;
    }

    return {
        testShiftLeftScalarLocal: testShiftLeftScalarLocal };
}

var m = asmModule(this, {});

print (m.testShiftLeftScalarLocal());
print (m.testShiftLeftScalarLocal());
