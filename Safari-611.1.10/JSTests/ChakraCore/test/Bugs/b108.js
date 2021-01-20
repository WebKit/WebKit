//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function asmModule(stdlib, imports,buffer) {
    "use asm";
    var i4 = stdlib.SIMD.Int32x4;
    var i4check = i4.check;
    var i4shiftLeftByScalar = i4.shiftLeftByScalar;
    var i4shiftRightByScalar = i4.shiftRightByScalar;
    var u4 = stdlib.SIMD.Uint32x4;
    var u4check = u4.check;
    var u4shiftLeftByScalar = u4.shiftLeftByScalar  ;
    var u4shiftRightByScalar= u4.shiftRightByScalar ;
    var loopCOUNT = 3;

    function func1(a)
    {
        a = i4check(a);
        var x = u4(0, 0, 0, 0);
        var y = u4(0, 0, 0, 0);
        var loopIndex = 0;
        var loopCOUNT = 3;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            x = u4shiftLeftByScalar(x, loopIndex);
            y = u4shiftRightByScalar(y, loopIndex);

            loopIndex = (loopIndex + 1) | 0;
        }
        return i4check(a);
    }
    return {func1:func1};
}

var buffer = new ArrayBuffer(0x10000);
var m = asmModule(this, {}, buffer);

var v = SIMD.Int32x4(1, 2, 3, 4 );

var ret1 = m.func1(v);
print(ret1);

