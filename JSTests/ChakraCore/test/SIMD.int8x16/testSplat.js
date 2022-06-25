//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function equal(a, b) {
    if (a == b)
        print("Correct");
    else
        print(">> Fail!");
}

function testSplat() {
    var n = SIMD.Int8x16.splat(3);
    print("splat");

    equal(3, SIMD.Int8x16.extractLane(n, 0));
    equal(3, SIMD.Int8x16.extractLane(n, 1));
    equal(3, SIMD.Int8x16.extractLane(n, 2));
    equal(3, SIMD.Int8x16.extractLane(n, 3));
    equal(3, SIMD.Int8x16.extractLane(n, 4));
    equal(3, SIMD.Int8x16.extractLane(n, 5));
    equal(3, SIMD.Int8x16.extractLane(n, 6));
    equal(3, SIMD.Int8x16.extractLane(n, 7));
    equal(3, SIMD.Int8x16.extractLane(n, 8));
    equal(3, SIMD.Int8x16.extractLane(n, 9));
    equal(3, SIMD.Int8x16.extractLane(n, 10));
    equal(3, SIMD.Int8x16.extractLane(n, 11));
    equal(3, SIMD.Int8x16.extractLane(n, 12));
    equal(3, SIMD.Int8x16.extractLane(n, 13));
    equal(3, SIMD.Int8x16.extractLane(n, 14));
    equal(3, SIMD.Int8x16.extractLane(n, 15));
}

testSplat();
testSplat();
testSplat();
testSplat();
testSplat();
testSplat();
testSplat();
testSplat();