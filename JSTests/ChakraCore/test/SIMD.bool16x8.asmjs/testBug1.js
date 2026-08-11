//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
function asmModule(stdlib, imports) {
    "use asm";

    var b8 = stdlib.SIMD.Bool16x8;
    var b8extractLane = b8.extractLane;
    var fround = stdlib.Math.fround;
    function testBug()
    {
        var a = b8(1, 0, 1, 0, 1, 0, 1, 0);
        var result = fround(0);
        result = fround(b8extractLane(a,0));
        return;
    }
    return {testBug:testBug};
}

var m = asmModule(this, {g1:SIMD.Bool16x8(1, 1, 1, 1, 1, 1, 1, 1)});
