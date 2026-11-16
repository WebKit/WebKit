//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function module0(stdlib) {
    "use asm"
    var u16 = stdlib.SIMD.Uint8x16;
    var u16check = u16.check;

    function foo(abc) {
        abc = u16check(abc);
        return ;
    }
    return { foo:foo }
}
var c = module0(this);
