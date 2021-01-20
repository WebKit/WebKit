//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function module0(stdlib) {
    "use asm"
    var u8 = stdlib.SIMD.Uint16x8;
    var u8check = u8.check;
    function foo() {
        var abc = u8(1,1,1,1,1,1,1,1);
        return u8check(abc);
    }
    return { foo:foo }
}
var c = module0(this);
