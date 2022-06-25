//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function equal(a, b) {
    if (a !== b)
    {
        print(">> Fail!");
    }
}

function testSplat() {
    var n = SIMD.Int32x4.splat(3);
    equal(3, SIMD.Int32x4.extractLane(n, 0));
    equal(3, SIMD.Int32x4.extractLane(n, 1));
    equal(3, SIMD.Int32x4.extractLane(n, 2));
    equal(3, SIMD.Int32x4.extractLane(n, 3));
}



testSplat();
testSplat();
testSplat();
print("PASS");
