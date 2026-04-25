//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function module0(stdlib) {
    "use asm"
    var u4 = stdlib.SIMD.Uint32x4;
    var u4check = u4.check;
    function foo() {
        var abc = u4(1,1,1,1);
        return u4check(abc);
    }
    return { foo:foo }
}
var c = module0(this);
