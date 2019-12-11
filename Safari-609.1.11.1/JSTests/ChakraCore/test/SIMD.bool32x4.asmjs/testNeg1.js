//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
function asmModule(stdlib, imports) {
    "use asm";

    var b8 = stdlib.SIMD.Bool16x8;
    var b8extractLane = b8.extractLane;
    function testNeg()
    {
        var a = b8(1, 0, 1, 0, 1, 0, 1, 0);
        var result = 0.0;
        result = +(b8extractLane(a,0));
        return;
    }
    return {testNeg:testNeg};
}

var m = asmModule(this, {g1:SIMD.Bool16x8(1, 1, 1, 1, 1, 1, 1, 1)});
