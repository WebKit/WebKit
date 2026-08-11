//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
function asmModule(stdlib, imports) {
    "use asm";

    var b = stdlib.SIMD.Float32x4;
    var bextractLane = b.extractLane;
    var fround = stdlib.Math.fround;
    function testBug()
    {
        var a = b(1.0, 0.0, 1.0, 0.0);
        var result = 0;
        result = bextractLane(a,0)|0;
        return;
    }
    return {testBug:testBug};
}

var m = asmModule(this, {g1:SIMD.Bool16x8(1, 1, 1, 1, 1, 1, 1, 1)});
